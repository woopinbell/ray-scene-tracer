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
                const double axial_distance =
                    dot(point - center_, axis_);
                if (axial_distance < -half_height - kEpsilon ||
                    axial_distance > half_height + kEpsilon) {
                    continue;
                }
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
