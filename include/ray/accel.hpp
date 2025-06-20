#pragma once

#include "ray/math.hpp"

namespace ray {

struct Aabb {
    Vec3 minimum;
    Vec3 maximum;

    Aabb();
    Aabb(const Vec3& minimum_value, const Vec3& maximum_value);

    bool isValid() const;
    Vec3 centroid() const;
    bool intersect(const Ray& ray,
                   double t_min,
                   double t_max,
                   double* entry = nullptr) const;
};

Aabb surroundingBox(const Aabb& left, const Aabb& right);

}  // namespace ray
