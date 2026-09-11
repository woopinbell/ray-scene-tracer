#include "ray/renderer.hpp"

#include <algorithm>
#include <limits>

namespace ray {

bool findNearestHit(const Scene& scene,
                    const Ray& ray,
                    HitRecord& hit,
                    double t_min,
                    double t_max,
                    AccelMode mode,
                    RenderStats* stats) {
    return scene.intersect(ray, t_min, t_max, hit, mode, stats);
}

// [INTV:ARCH] 그림자 검사는 "충돌 지점"이 아니라 "충돌 여부"만 필요하므로, HitRecord를 무시(ignored)
// 하고 가장 가까운 히트를 찾자마자 조기 반환할 수 있는 별도 함수로 분리했다 — findNearestHit과 같은
// scene.intersect를 재사용하지만 호출 의도가 다르다는 것을 이름으로 드러낸다.
bool isOccluded(const Scene& scene,
                const Ray& shadow_ray,
                double max_distance,
                AccelMode mode,
                RenderStats* stats) {
    HitRecord ignored;
    return scene.intersect(shadow_ray,
                           kRayTMin,
                           std::max(kRayTMin, max_distance - kRayTMin),
                           ignored,
                           mode,
                           stats);
}

// [INTV:ARCH] Diffuse 셰이딩 모델(Lambertian + 환경광): 환경광(ambient)을 기본값으로 깔고, 각 광원에
// 대해 그림자 광선(shadow ray)으로 가려졌는지 확인한 뒤 가려지지 않은 광원만 diffuse 항을 누적한다.
// - [EDGE] shadow_origin을 hit.point가 아니라 "hit.point + normal*kRayTMin"으로 살짝 띄우는 이유:
//   그대로 표면 위에서 그림자 광선을 쏘면 부동소수점 오차로 광선 시작점이 표면 바로 아래에서
//   시작해 자기 자신과 즉시 재교차(self-shadowing, "shadow acne")하는 문제가 생긴다.
// - [TRAP] max_distance(광원까지 거리)를 그림자 광선의 t_max로 제한하지 않으면, 광원보다 "뒤에" 있는
//   도형과의 교차까지 그림자로 오판해 실제로는 빛을 받아야 할 표면이 어둡게 나온다.
Color shadeHit(const Scene& scene,
               const HitRecord& hit,
               const Ray& view_ray,
               AccelMode mode,
               RenderStats* stats) {
    (void)view_ray;

    Color result =
        hit.material.albedo * scene.ambientColor * scene.ambientRatio;
    const Vec3 shadow_origin = hit.point + hit.normal * kRayTMin;

    for (const Light& light : scene.lights) {
        const Vec3 to_light = light.position - hit.point;
        const double distance_to_light = to_light.length();
        if (distance_to_light <= kEpsilon) {
            continue;
        }

        const Vec3 light_direction = to_light / distance_to_light;
        const double diffuse =
            std::max(0.0, dot(hit.normal, light_direction));
        if (diffuse <= 0.0) {
            // [INTV:PERF] 법선과 광원 방향의 내적이 0 이하면 광원이 표면 "뒤쪽"에 있다는 뜻 —
            // 어차피 기여가 0이므로, 비용이 큰 그림자 광선(isOccluded)을 아예 쏘지 않고 건너뛰는
            // 조기 컷오프.
            continue;
        }

        if (stats) {
            ++stats->shadowRays;
        }
        if (isOccluded(scene,
                       Ray(shadow_origin, light_direction),
                       distance_to_light,
                       mode,
                       stats)) {
            continue;
        }

        result += hit.material.albedo * light.color *
                  (light.brightness * diffuse);
    }

    return clampColor(result);
}

// [INTV:ARCH] 재귀적 광선 추적: Metal 표면을 만나면 반사 방향으로 새 광선을 만들어 자기 자신을
// max_depth-1로 재귀 호출하고, Diffuse 표면을 만나면 재귀를 멈추고 shadeHit으로 조명을 계산한다.
// - [FLOW] 1. 가장 가까운 교차를 찾음(없으면 배경색) -> 2. Metal이면: depth 소진 시 검정 반환, 아니면
//   반사 방향 계산 후 재귀 -> 3. Diffuse면 shadeHit으로 종료
// - [TRAP] max_depth <= 0 조기 반환이 없으면, 서로를 비추는 거울들 사이에서 재귀가 끝없이 깊어지다
//   스택 오버플로로 크래시한다 — 이 한 줄이 재귀 깊이 상한을 보장하는 유일한 장치다.
Color traceRay(const Scene& scene,
               const Ray& ray,
               int max_depth,
               AccelMode mode,
               RenderStats* stats) {
    (void)max_depth;

    HitRecord hit;
    if (!scene.intersect(ray,
                         kRayTMin,
                         std::numeric_limits<double>::infinity(),
                         hit,
                         mode,
                         stats)) {
        return scene.background;
    }
    if (hit.material.type == MaterialType::Metal) {
        if (max_depth <= 0) {
            return Color();
        }
        // [INTV:EDGE] 반사 공식 d' = d - 2*(d·n)*n (d: 입사 방향, n: 법선) — 입사 벡터에서 법선 성분만
        // 두 배로 빼서 반사 벡터를 만드는 표준 벡터 반사 공식. reflected_ray의 원점도 shadeHit과
        // 같은 이유(shadow acne 방지)로 법선 방향으로 kRayTMin만큼 띄운다.
        const Vec3 reflected_direction =
            ray.direction -
            hit.normal * (2.0 * dot(ray.direction, hit.normal));
        const Ray reflected_ray(
            hit.point + hit.normal * kRayTMin,
            reflected_direction);
        if (stats) {
            ++stats->secondaryRays;
        }
        return hit.material.albedo *
               traceRay(scene,
                        reflected_ray,
                        max_depth - 1,
                        mode,
                        stats);
    }
    return shadeHit(scene, hit, ray, mode, stats);
}

}  // namespace ray
