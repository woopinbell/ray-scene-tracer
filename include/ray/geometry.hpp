#pragma once

#include "ray/accel.hpp"
#include "ray/material.hpp"

#include <optional>
#include <string>

namespace ray {

class Shape;

struct HitRecord {
    double t;
    Vec3 point;
    Vec3 normal;
    Material material;
    const Shape* shape;
    bool frontFace;

    HitRecord();
    void setFaceNormal(const Ray& ray, const Vec3& outward_normal);
};

class Shape {
public:
    explicit Shape(const Material& material_value = Material());
    virtual ~Shape() = default;

    const Material& material() const;
    virtual bool intersect(const Ray& ray,
                           double t_min,
                           double t_max,
                           HitRecord& hit) const = 0;
    virtual std::optional<Aabb> bounds() const = 0;
    virtual std::string typeName() const = 0;

protected:
    Material material_;
};

class Sphere : public Shape {
public:
    Vec3 center;
    double radius;

    Sphere(const Vec3& center_value,
           double radius_value,
           const Material& material_value);

    bool intersect(const Ray& ray,
                   double t_min,
                   double t_max,
                   HitRecord& hit) const override;
    std::optional<Aabb> bounds() const override;
    std::string typeName() const override;
};

class Plane : public Shape {
public:
    Vec3 point;
    Vec3 normal;

    Plane(const Vec3& point_value,
          const Vec3& normal_value,
          const Material& material_value);

    bool intersect(const Ray& ray,
                   double t_min,
                   double t_max,
                   HitRecord& hit) const override;
    std::optional<Aabb> bounds() const override;
    std::string typeName() const override;
};

class Cylinder : public Shape {
public:
    Vec3 center;
    Vec3 axis;
    double radius;
    double height;

    Cylinder(const Vec3& center_value,
             const Vec3& axis_value,
             double radius_value,
             double height_value,
             const Material& material_value);

    bool intersect(const Ray& ray,
                   double t_min,
                   double t_max,
                   HitRecord& hit) const override;
    std::optional<Aabb> bounds() const override;
    std::string typeName() const override;
};

}  // namespace ray
