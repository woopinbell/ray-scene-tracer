#include "ray/geometry.hpp"

#include <cmath>
#include <utility>

namespace ray {

HitRecord::HitRecord()
    : t(0.0),
      point(),
      normal(),
      material(),
      shape(nullptr),
      frontFace(true) {}

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
    : Shape(material_value), center(center_value), radius(radius_value) {}

bool Sphere::intersect(const Ray& ray,
                       double t_min,
                       double t_max,
                       HitRecord& hit) const {
    if (radius <= kEpsilon) {
        return false;
    }

    const Vec3 oc = ray.origin - center;
    const double a = dot(ray.direction, ray.direction);
    if (a <= kEpsilon) {
        return false;
    }

    const double half_b = dot(oc, ray.direction);
    const double c = dot(oc, oc) - radius * radius;
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
    hit.setFaceNormal(ray, (hit.point - center) / radius);
    return true;
}

std::string Sphere::typeName() const {
    return "sphere";
}

}  // namespace ray
