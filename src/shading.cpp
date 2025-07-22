#include "ray/renderer.hpp"

#include <algorithm>
#include <limits>

namespace ray {

bool findNearestHit(const Scene& scene,
                    const Ray& ray,
                    HitRecord& hit,
                    double t_min,
                    double t_max,
                    RenderStats* stats) {
    return scene.intersect(ray, t_min, t_max, hit, stats);
}

bool isOccluded(const Scene& scene,
                const Ray& shadow_ray,
                double max_distance,
                RenderStats* stats) {
    HitRecord ignored;
    return scene.intersect(shadow_ray,
                           kRayTMin,
                           std::max(kRayTMin, max_distance - kRayTMin),
                           ignored,
                           stats);
}

Color shadeHit(const Scene& scene,
               const HitRecord& hit,
               const Ray& view_ray,
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
            continue;
        }

        if (stats) {
            ++stats->shadowRays;
        }
        if (isOccluded(scene,
                       Ray(shadow_origin, light_direction),
                       distance_to_light,
                       stats)) {
            continue;
        }

        result += hit.material.albedo * light.color *
                  (light.brightness * diffuse);
    }

    return clampColor(result);
}

Color traceRay(const Scene& scene,
               const Ray& ray,
               int max_depth,
               RenderStats* stats) {
    (void)max_depth;

    HitRecord hit;
    if (!scene.intersect(ray,
                         kRayTMin,
                         std::numeric_limits<double>::infinity(),
                         hit,
                         stats)) {
        return scene.background;
    }
    return shadeHit(scene, hit, ray, stats);
}

}  // namespace ray
