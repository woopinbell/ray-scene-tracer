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
    Sphere(const Vec3& center_value,
           double radius_value,
           const Material& material_value);

    const Vec3& center() const;
    double radius() const;

    bool intersect(const Ray& ray,
                   double t_min,
                   double t_max,
                   HitRecord& hit) const override;
    std::optional<Aabb> bounds() const override;
    std::string typeName() const override;

private:
    Vec3 center_;
    double radius_;
};

class Plane : public Shape {
public:
    Plane(const Vec3& point_value,
          const Vec3& normal_value,
          const Material& material_value);

    const Vec3& point() const;
    const Vec3& normal() const;

    bool intersect(const Ray& ray,
                   double t_min,
                   double t_max,
                   HitRecord& hit) const override;
    std::optional<Aabb> bounds() const override;
    std::string typeName() const override;

private:
    Vec3 point_;
    Vec3 normal_;
};

class Cylinder : public Shape {
public:
    Cylinder(const Vec3& center_value,
             const Vec3& axis_value,
             double radius_value,
             double height_value,
             const Material& material_value);

    const Vec3& center() const;
    const Vec3& axis() const;
    double radius() const;
    double height() const;

    bool intersect(const Ray& ray,
                   double t_min,
                   double t_max,
                   HitRecord& hit) const override;
    std::optional<Aabb> bounds() const override;
    std::string typeName() const override;

private:
    Vec3 center_;
    Vec3 axis_;
    double radius_;
    double height_;
};

}  // namespace ray
