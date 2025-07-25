#include "ray/accel.hpp"

#include <algorithm>
#include <limits>

namespace ray {

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
