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

bool BvhNode::isLeaf() const {
    return count > 0;
}

void Bvh::build(std::vector<BvhPrimitive> primitives) {
    clear();
    if (primitives.empty()) {
        return;
    }
    nodes_.reserve(primitives.size() * 2);
    (void)buildNode(primitives,
                    0,
                    static_cast<std::uint32_t>(primitives.size()));
    primitiveIndices_.reserve(primitives.size());
    for (const BvhPrimitive& primitive : primitives) {
        primitiveIndices_.push_back(primitive.shapeIndex);
    }
}

void Bvh::clear() {
    nodes_.clear();
    primitiveIndices_.clear();
}

bool Bvh::empty() const {
    return nodes_.empty();
}

const std::vector<BvhNode>& Bvh::nodes() const {
    return nodes_;
}

const std::vector<std::uint32_t>& Bvh::primitiveIndices() const {
    return primitiveIndices_;
}

std::uint32_t Bvh::buildNode(std::vector<BvhPrimitive>& primitives,
                             std::uint32_t first,
                             std::uint32_t last) {
    Aabb node_bounds = primitives[first].bounds;
    Vec3 centroid_min = primitives[first].bounds.centroid();
    Vec3 centroid_max = centroid_min;
    for (std::uint32_t index = first + 1; index < last; ++index) {
        node_bounds = surroundingBox(node_bounds, primitives[index].bounds);
        const Vec3 centroid = primitives[index].bounds.centroid();
        centroid_min.x = std::min(centroid_min.x, centroid.x);
        centroid_min.y = std::min(centroid_min.y, centroid.y);
        centroid_min.z = std::min(centroid_min.z, centroid.z);
        centroid_max.x = std::max(centroid_max.x, centroid.x);
        centroid_max.y = std::max(centroid_max.y, centroid.y);
        centroid_max.z = std::max(centroid_max.z, centroid.z);
    }

    const std::uint32_t node_index =
        static_cast<std::uint32_t>(nodes_.size());
    nodes_.push_back(BvhNode());
    nodes_[node_index].bounds = node_bounds;

    const std::uint32_t count = last - first;
    if (count <= 4) {
        nodes_[node_index].first = first;
        nodes_[node_index].count = count;
        return node_index;
    }

    const Vec3 extent = centroid_max - centroid_min;
    int axis = 0;
    if (extent.y > extent.x) {
        axis = 1;
    }
    if (component(extent, 2) > component(extent, axis)) {
        axis = 2;
    }
    std::stable_sort(
        primitives.begin() + first,
        primitives.begin() + last,
        [axis](const BvhPrimitive& left, const BvhPrimitive& right) {
            const double left_value = component(left.bounds.centroid(), axis);
            const double right_value = component(right.bounds.centroid(), axis);
            if (left_value != right_value) {
                return left_value < right_value;
            }
            return left.shapeIndex < right.shapeIndex;
        });

    const std::uint32_t middle = first + count / 2;
    const std::uint32_t left = buildNode(primitives, first, middle);
    const std::uint32_t right = buildNode(primitives, middle, last);
    nodes_[node_index].left = left;
    nodes_[node_index].right = right;
    return node_index;
}

}  // namespace ray
