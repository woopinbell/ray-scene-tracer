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

Plane::Plane(const Vec3& point_value,
             const Vec3& normal_value,
             const Material& material_value)
    : Shape(material_value), point(point_value), normal(normalize(normal_value)) {}

bool Plane::intersect(const Ray& ray,
                      double t_min,
                      double t_max,
                      HitRecord& hit) const {
    if (normal.isNearZero()) {
        return false;
    }

    const double denominator = dot(normal, ray.direction);
    if (std::fabs(denominator) <= kEpsilon) {
        return false;
    }

    const double t = dot(point - ray.origin, normal) / denominator;
    if (t < t_min || t > t_max) {
        return false;
    }

    hit.t = t;
    hit.point = ray.at(t);
    hit.material = material_;
    hit.shape = this;
    hit.setFaceNormal(ray, normal);
    return true;
}

std::string Plane::typeName() const {
    return "plane";
}

Cylinder::Cylinder(const Vec3& center_value,
                   const Vec3& axis_value,
                   double radius_value,
                   double height_value,
                   const Material& material_value)
    : Shape(material_value),
      center(center_value),
      axis(normalize(axis_value)),
      radius(radius_value),
      height(height_value) {}

bool Cylinder::intersect(const Ray& ray,
                         double t_min,
                         double t_max,
                         HitRecord& hit) const {
    if (axis.isNearZero() || radius <= kEpsilon || height <= kEpsilon) {
        return false;
    }

    bool found = false;
    double closest = t_max;
    const double half_height = height * 0.5;
    const Vec3 oc = ray.origin - center;
    const double direction_axis = dot(ray.direction, axis);
    const double origin_axis = dot(oc, axis);
    const Vec3 direction_perp = ray.direction - axis * direction_axis;
    const Vec3 origin_perp = oc - axis * origin_axis;
    const double a = dot(direction_perp, direction_perp);

    if (a > kEpsilon) {
        const double half_b = dot(direction_perp, origin_perp);
        const double c = dot(origin_perp, origin_perp) - radius * radius;
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
                const double axial_distance = dot(point - center, axis);
                if (axial_distance < -half_height - kEpsilon ||
                    axial_distance > half_height + kEpsilon) {
                    continue;
                }
                const Vec3 outward_normal =
                    normalize((point - center) - axis * axial_distance);
                if (outward_normal.isNearZero()) {
                    continue;
                }
                hit.t = root;
                hit.point = point;
                hit.material = material_;
                hit.shape = this;
                hit.setFaceNormal(ray, outward_normal);
                closest = root;
                found = true;
            }
        }
    }
    return found;
}

std::string Cylinder::typeName() const {
    return "cylinder";
}

}  // namespace ray
