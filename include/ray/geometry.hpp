#pragma once

#include "ray/material.hpp"

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
    virtual std::string typeName() const = 0;

protected:
    Material material_;
};

}  // namespace ray
