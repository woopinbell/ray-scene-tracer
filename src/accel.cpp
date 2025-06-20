#include "ray/accel.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace ray {

namespace {

double component(const Vec3& value, int axis) {
    if (axis == 0) {
        return value.x;
    }
    if (axis == 1) {
        return value.y;
    }
    return value.z;
}

}  // namespace

Aabb::Aabb()
    : minimum(std::numeric_limits<double>::infinity(),
              std::numeric_limits<double>::infinity(),
              std::numeric_limits<double>::infinity()),
      maximum(-std::numeric_limits<double>::infinity(),
              -std::numeric_limits<double>::infinity(),
              -std::numeric_limits<double>::infinity()) {}

Aabb::Aabb(const Vec3& minimum_value, const Vec3& maximum_value)
    : minimum(minimum_value), maximum(maximum_value) {}

bool Aabb::isValid() const {
    return minimum.x <= maximum.x &&
           minimum.y <= maximum.y &&
           minimum.z <= maximum.z;
}

Vec3 Aabb::centroid() const {
    return (minimum + maximum) * 0.5;
}

bool Aabb::intersect(const Ray& ray,
                     double t_min,
                     double t_max,
                     double* entry) const {
    if (!isValid()) {
        return false;
    }

    double near_value = t_min;
    double far_value = t_max;
    for (int axis = 0; axis < 3; ++axis) {
        const double origin = component(ray.origin, axis);
        const double direction = component(ray.direction, axis);
        const double slab_min = component(minimum, axis);
        const double slab_max = component(maximum, axis);

        if (direction == 0.0) {
            if (origin < slab_min || origin > slab_max) {
                return false;
            }
            continue;
        }

        double first = (slab_min - origin) / direction;
        double second = (slab_max - origin) / direction;
        if (first > second) {
            std::swap(first, second);
        }
        near_value = std::max(near_value, first);
        far_value = std::min(far_value, second);
        if (far_value < near_value) {
            return false;
        }
    }

    if (entry) {
        *entry = near_value;
    }
    return true;
}

Aabb surroundingBox(const Aabb& left, const Aabb& right) {
    return Aabb(
        Vec3(std::min(left.minimum.x, right.minimum.x),
             std::min(left.minimum.y, right.minimum.y),
             std::min(left.minimum.z, right.minimum.z)),
        Vec3(std::max(left.maximum.x, right.maximum.x),
             std::max(left.maximum.y, right.maximum.y),
             std::max(left.maximum.z, right.maximum.z)));
}

}  // namespace ray
