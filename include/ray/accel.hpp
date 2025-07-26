#pragma once

#include "ray/math.hpp"

#include <cstdint>
#include <vector>

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

struct BvhPrimitive {
    std::uint32_t shapeIndex;
    Aabb bounds;
};

struct BvhNode {
    Aabb bounds;
    std::uint32_t left = 0;
    std::uint32_t right = 0;
    std::uint32_t first = 0;
    std::uint32_t count = 0;

    bool isLeaf() const;
};

class Bvh {
public:
    void clear();
    bool empty() const;

    const std::vector<BvhNode>& nodes() const;
    const std::vector<std::uint32_t>& primitiveIndices() const;

private:
    std::vector<BvhNode> nodes_;
    std::vector<std::uint32_t> primitiveIndices_;
};

}  // namespace ray
