#include "ray/geometry.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace ray {

HitRecord::HitRecord()
    : t(0.0),
      point(),
      normal(),
      material(),
      shape(nullptr),
      frontFace(true) {}

// [INTV:EDGE] 광선 방향과 표면 "바깥쪽" 법선의 내적이 음수면 광선이 표면 밖에서 안으로 들어온 것
// (앞면에 맞음). 항상 광선을 향하는 법선으로 정규화해 저장해두면, 이후 셰이딩 계산에서 표면의 앞/뒤를
// 매번 다시 판단할 필요가 없다.
void HitRecord::setFaceNormal(const Ray& ray, const Vec3& outward_normal) {
    frontFace = dot(ray.direction, outward_normal) < 0.0;
    normal = frontFace ? outward_normal : -outward_normal;
}

Shape::Shape(const Material& material_value) : material_(material_value) {}

const Material& Shape::material() const {
    return material_;
}

Sphere::Sphere(const Vec3& center_value,
               double radius_value,
               const Material& material_value)
    : Shape(material_value),
      center_(center_value),
      radius_(radius_value) {}

const Vec3& Sphere::center() const {
    return center_;
}

double Sphere::radius() const {
    return radius_;
}

// [INTV:ARCH] 광선-구 교차: P(t)=origin+t*direction을 |P(t)-center|^2=radius^2에 대입하면 t에 대한
// 이차방정식 a*t^2+2b*t+c=0이 나온다. 근의 공식의 "2b" 대신 half_b(=b, 2로 미리 나눈 값)를 쓰는 변형을
// 사용해 곱셈 횟수를 줄인다.
// - [FLOW] 1. 판별식이 음수면 실근 없음(교차 없음) -> 2. 더 가까운 근(-)을 먼저 시도 -> 3. 유효 범위
//   [t_min, closest] 밖이면 먼 근(+)으로 폴백 -> 4. 그것도 범위 밖이면 교차 없음
bool Sphere::intersect(const Ray& ray,
                       double t_min,
                       double t_max,
                       HitRecord& hit) const {
    if (radius_ <= kEpsilon) {
        return false;
    }

    const Vec3 oc = ray.origin - center_;
    const double a = dot(ray.direction, ray.direction);
    if (a <= kEpsilon) {
        return false;
    }

    const double half_b = dot(oc, ray.direction);
    const double c = dot(oc, oc) - radius_ * radius_;
    const double discriminant = half_b * half_b - a * c;
    if (discriminant < 0.0) {
        return false;
    }

    const double sqrt_discriminant = std::sqrt(discriminant);
    double root = (-half_b - sqrt_discriminant) / a;
    if (root < t_min || root > t_max) {
        root = (-half_b + sqrt_discriminant) / a;
        if (root < t_min || root > t_max) {
            return false;
        }
    }

    hit.t = root;
    hit.point = ray.at(root);
    hit.material = material_;
    hit.shape = this;
    hit.setFaceNormal(ray, (hit.point - center_) / radius_);
    return true;
}

std::string Sphere::typeName() const {
    return "sphere";
}

std::optional<Aabb> Sphere::bounds() const {
    const Vec3 extent(radius_, radius_, radius_);
    return Aabb(center_ - extent, center_ + extent);
}

Plane::Plane(const Vec3& point_value,
             const Vec3& normal_value,
             const Material& material_value)
    : Shape(material_value),
      point_(point_value),
      normal_(normalize(normal_value)) {}

const Vec3& Plane::point() const {
    return point_;
}

const Vec3& Plane::normal() const {
    return normal_;
}

// [INTV:ARCH] 평면 위 임의의 점 X는 dot(X - point_, normal_) == 0을 만족한다 — X = ray.at(t)를
// 대입해 t에 대해 풀면 아래 한 줄의 나눗셈이 나온다.
// - [TRAP] 분모(광선 방향과 법선의 내적)가 0에 가까운 경우를 걸러내지 않으면, 광선이 평면과 거의
//   평행할 때 나눗셈이 거대하거나 불안정한 t를 만들어낸다 — kEpsilon 문턱값으로 그 이전에 배제한다.
bool Plane::intersect(const Ray& ray,
                      double t_min,
                      double t_max,
                      HitRecord& hit) const {
    if (normal_.isNearZero()) {
        return false;
    }

    const double denominator = dot(normal_, ray.direction);
    if (std::fabs(denominator) <= kEpsilon) {
        return false;
    }

    const double t = dot(point_ - ray.origin, normal_) / denominator;
    if (t < t_min || t > t_max) {
        return false;
    }

    hit.t = t;
    hit.point = ray.at(t);
    hit.material = material_;
    hit.shape = this;
    hit.setFaceNormal(ray, normal_);
    return true;
}

std::string Plane::typeName() const {
    return "plane";
}

std::optional<Aabb> Plane::bounds() const {
    return std::nullopt;
}

Cylinder::Cylinder(const Vec3& center_value,
                   const Vec3& axis_value,
                   double radius_value,
                   double height_value,
                   const Material& material_value)
    : Shape(material_value),
      center_(center_value),
      axis_(normalize(axis_value)),
      radius_(radius_value),
      height_(height_value) {}

const Vec3& Cylinder::center() const {
    return center_;
}

const Vec3& Cylinder::axis() const {
    return axis_;
}

double Cylinder::radius() const {
    return radius_;
}

double Cylinder::height() const {
    return height_;
}

// [INTV:ARCH] 실린더는 옆면(곡면)과 위/아래 뚜껑(원판) 세 표면의 교차 결과 중 "가장 가까운 것"만
// 남겨야 하는데, 그 "더 가까우면 갱신" 로직을 이 익명 네임스페이스의 헬퍼로 뽑아 세 표면 각각에서
// 중복 없이 재사용한다.
namespace {

bool update_hit_if_closer(const Ray& ray,
                          const Material& material,
                          const Shape* shape,
                          double t,
                          const Vec3& outward_normal,
                          double t_min,
                          double& closest,
                          HitRecord& hit) {
    if (t < t_min || t > closest) {
        return false;
    }
    hit.t = t;
    hit.point = ray.at(t);
    hit.material = material;
    hit.shape = shape;
    hit.setFaceNormal(ray, outward_normal);
    closest = t;
    return true;
}

// [INTV:ARCH] Plane::intersect와 같은 평면 교차 공식(뚜껑은 axis_에 수직인 평면 조각)을 재사용하되,
// 교점을 구한 뒤 뚜껑 중심으로부터의 거리가 반지름 이내(원판 안)인지까지 추가로 확인한다.
bool test_cylinder_cap(const Ray& ray,
                       const Vec3& cap_center,
                       const Vec3& outward_normal,
                       double radius,
                       const Material& material,
                       const Shape* shape,
                       double t_min,
                       double& closest,
                       HitRecord& hit) {
    const double denominator = dot(outward_normal, ray.direction);
    if (std::fabs(denominator) <= kEpsilon) {
        return false;
    }

    const double t = dot(cap_center - ray.origin, outward_normal) / denominator;
    if (t < t_min || t > closest) {
        return false;
    }

    const Vec3 point = ray.at(t);
    if ((point - cap_center).lengthSquared() > radius * radius + kEpsilon) {
        return false;
    }

    return update_hit_if_closer(ray,
                                material,
                                shape,
                                t,
                                outward_normal,
                                t_min,
                                closest,
                                hit);
}

}  // namespace

// [INTV:ARCH] 광선-무한원기둥 교차: 광선/오프셋 벡터를 축(axis_) 성분과 축에 수직인 성분으로 분해하면,
// 축에 수직인 평면에 투영했을 때의 "광선-원 교차" 문제로 바뀐다 — Sphere::intersect와 같은 형태의
// 이차방정식(a, half_b, c)이 다시 등장하는 이유다.
// - [FLOW] 1. 무한 원기둥 옆면과의 이차방정식을 풀어 두 근을 구함 -> 2. 각 근의 축 방향 성분이 실제
//   높이 범위(±half_height) 안인지 걸러 유한한 옆면으로 한정 -> 3. 위/아래 뚜껑 원판도 각각 검사 ->
//   4. 세 표면 중 가장 가까운(closest) 결과만 채택
// - [TRAP] 옆면 이차방정식만 풀고 뚜껑 검사를 빼먹으면, 실린더 끝을 정면으로 바라보는 광선이 옆면을
//   벗어나는데도(축 범위 밖) 교차가 없다고 잘못 판정되어 뚜껑이 뚫려 보이는 렌더링 버그가 된다.
bool Cylinder::intersect(const Ray& ray,
                         double t_min,
                         double t_max,
                         HitRecord& hit) const {
    if (axis_.isNearZero() ||
        radius_ <= kEpsilon ||
        height_ <= kEpsilon) {
        return false;
    }

    bool found = false;
    double closest = t_max;
    const double half_height = height_ * 0.5;
    const Vec3 oc = ray.origin - center_;
    const double direction_axis = dot(ray.direction, axis_);
    const double origin_axis = dot(oc, axis_);
    const Vec3 direction_perp = ray.direction - axis_ * direction_axis;
    const Vec3 origin_perp = oc - axis_ * origin_axis;
    const double a = dot(direction_perp, direction_perp);

    if (a > kEpsilon) {
        const double half_b = dot(direction_perp, origin_perp);
        const double c =
            dot(origin_perp, origin_perp) - radius_ * radius_;
        const double discriminant = half_b * half_b - a * c;
        if (discriminant >= 0.0) {
            const double sqrt_discriminant = std::sqrt(discriminant);
            const double roots[2] = {
                (-half_b - sqrt_discriminant) / a,
                (-half_b + sqrt_discriminant) / a
            };

            for (double root : roots) {
                if (root < t_min || root > closest) {
                    continue;
                }
                const Vec3 point = ray.at(root);
                // [INTV:EDGE] 위에서 푼 것은 "무한히 긴 원기둥"과의 교차이므로, 축 방향 성분
                // (axial_distance)이 실제 높이 범위(±half_height) 안에 들어오는지 걸러내야 유한한
                // 실린더 옆면이 된다.
                const double axial_distance =
                    dot(point - center_, axis_);
                if (axial_distance < -half_height - kEpsilon ||
                    axial_distance > half_height + kEpsilon) {
                    continue;
                }
                // 곡면의 법선은 교점에서 축 성분을 뺀, 축에 수직인 방향(반지름 방향).
                const Vec3 outward_normal =
                    normalize((point - center_) -
                              axis_ * axial_distance);
                if (outward_normal.isNearZero()) {
                    continue;
                }
                found = update_hit_if_closer(ray,
                                             material_,
                                             this,
                                             root,
                                             outward_normal,
                                             t_min,
                                             closest,
                                             hit) || found;
            }
        }
    }

    const Vec3 top_center = center_ + axis_ * half_height;
    const Vec3 bottom_center = center_ - axis_ * half_height;
    found = test_cylinder_cap(ray,
                              top_center,
                              axis_,
                              radius_,
                              material_,
                              this,
                              t_min,
                              closest,
                              hit) || found;
    found = test_cylinder_cap(ray,
                              bottom_center,
                              -axis_,
                              radius_,
                              material_,
                              this,
                              t_min,
                              closest,
                              hit) || found;

    return found;
}

std::string Cylinder::typeName() const {
    return "cylinder";
}

// [INTV:EDGE] 회전된 실린더(축이 기울어짐)의 AABB를 각 축(x/y/z)에 대해 "옆면이 만드는 폭"과
// "뚜껑 원판이 만드는 폭" 중 더 큰 쪽으로 계산한다.
std::optional<Aabb> Cylinder::bounds() const {
    const double half_height = height_ * 0.5;
    const auto extent_for = [this, half_height](double axis_component) {
        const double absolute_axis = std::fabs(axis_component);
        const double radial =
            std::sqrt(std::max(0.0, 1.0 - axis_component * axis_component));
        const double side_extent =
            absolute_axis * (half_height + kEpsilon) +
            radius_ * radial;
        const double cap_extent =
            absolute_axis * half_height +
            std::sqrt(radius_ * radius_ + kEpsilon) * radial;
        return std::max(side_extent, cap_extent);
    };

    const Vec3 extent(extent_for(axis_.x),
                      extent_for(axis_.y),
                      extent_for(axis_.z));
    Vec3 minimum = center_ - extent;
    Vec3 maximum = center_ + extent;
    // [INTV:EDGE] std::nextafter로 경계를 부동소수점 최소 단위(1ULP)만큼 바깥쪽으로 밀어낸다 — 위에서
    // 계산한 경계 상자가 반올림 오차로 실제 실린더보다 아주 살짝 작아져 접선 방향으로 스치는 광선을
    // BVH 단계에서 놓치는 것을 막는 보수적(conservative) 여유.
    // - [TRAP] 이 보정 없이 계산값을 그대로 쓰면, "도형과는 실제로 교차하지만 BVH가 상자를 안 스쳤다고
    //   오판해 통째로 건너뛰는" 극히 드물고 재현하기 어려운 렌더링 결손이 생길 수 있다.
    minimum.x = std::nextafter(
        minimum.x, -std::numeric_limits<double>::infinity());
    minimum.y = std::nextafter(
        minimum.y, -std::numeric_limits<double>::infinity());
    minimum.z = std::nextafter(
        minimum.z, -std::numeric_limits<double>::infinity());
    maximum.x = std::nextafter(
        maximum.x, std::numeric_limits<double>::infinity());
    maximum.y = std::nextafter(
        maximum.y, std::numeric_limits<double>::infinity());
    maximum.z = std::nextafter(
        maximum.z, std::numeric_limits<double>::infinity());
    return Aabb(minimum, maximum);
}

}  // namespace ray
