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

// [INTV:EDGE] 기본 생성자가 minimum=+inf, maximum=-inf로 시작 — "아직 아무것도 안 담은 빈 상자"가
// isValid()==false가 되도록(minimum > maximum) 자연스럽게 만들어, surroundingBox로 다른 상자와
// 합칠 때 첫 원소가 무조건 이기도록 하는 항등원(identity) 역할을 한다.
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

// [INTV:ARCH] 슬랩 방법(slab method): 상자를 x/y/z축 각각에 수직인 두 평면 쌍("슬랩")의 교집합으로
// 보고, 광선이 각 슬랩 안에 머무는 t 구간을 축마다 구해 전부 겹치는 구간이 남는지 확인한다. 그 구간이
// 하나라도 비면(far < near) 광선이 상자를 비껴간 것 — AABB 교차 판정의 표준 알고리즘.
// - [TRAP] direction == 0.0(광선이 그 축과 평행)인 경우를 나눗셈에 그대로 넣으면 0으로 나누기가 되어
//   NaN/무한대가 나온다. 그 축에서는 나눗셈 대신 "시작점이 애초에 슬랩 범위 안에 있는가"만 확인하는
//   별도 분기가 필요하다.
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

    // [INTV:ARCH] entry는 nullptr일 수 있는 선택적 출력 파라미터 — BVH 순회에서 "이 노드에 얼마나
    // 가까이서 들어가는지"로 자식 방문 순서를 정할 때만 채워 쓴다(가까운 쪽 먼저 방문해 조기 종료
    // 기회를 늘리는 최적화의 기반 정보).
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
    // [INTV:PERF] 재할당으로 인한 복사를 피하려고 최종적으로 필요할 노드 수(균형 이진 트리 기준 대략
    // 원소 수의 2배)를 미리 확보.
    nodes_.reserve(primitives.size() * 2);
    // [INTV:TRAP] buildNode의 반환값(루트 노드 인덱스, 항상 0)을 여기서 쓰지 않는다고 그냥 버리면
    // 정적 분석 도구가 "반환값 무시" 경고를 낼 수 있다 — (void) 캐스팅으로 "실수로 빠뜨린 게 아니라
    // 의도적으로 무시한다"는 뜻을 명시한다.
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

// [INTV:ARCH] 중앙값 분할(median split) 기반의 재귀적 BVH 구성.
// - [FLOW] 1. [first,last) 구간 전체를 감싸는 경계 상자와 중심점 범위를 구함 -> 2. 4개 이하로 줄면
//   리프로 확정 -> 3. 중심점이 가장 넓게 퍼진 축을 고름 -> 4. 그 축 기준으로 정렬 후 절반으로 잘라
//   좌/우 자식을 재귀 생성
// - [TRAP] 정렬을 전체 구간이 아니라 [first,last) 부분 구간에만 적용해야 한다(std::sort(v.begin(),
//   v.end())로 잘못 재구현하면 이미 확정된 다른 노드들의 순서까지 흐트러진다).
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

    // [INTV:PERF] 도형들의 중심점이 x/y/z 중 어느 축으로 가장 넓게 퍼져 있는지 골라 그 축으로 나누는
    // 휴리스틱 — 상자를 가장 고르게(덜 겹치게) 둘로 쪼개 트리 순회 시 조기 컷오프 효율을 높인다.
    const Vec3 extent = centroid_max - centroid_min;
    int axis = 0;
    if (extent.y > extent.x) {
        axis = 1;
    }
    if (component(extent, 2) > component(extent, axis)) {
        axis = 2;
    }
    // [INTV:EDGE] std::stable_sort + tie-break(중심점이 같으면 shapeIndex로 비교) — 부동소수점 중심점이
    // 정확히 같은 값일 때도 정렬 결과가 매번 재현되게 해, 같은 장면을 다시 빌드해도 트리 구조와 순회
    // 순서가 흔들리지 않는 결정론적(deterministic) 렌더링을 보장한다.
    // - [TRAP] std::sort(불안정 정렬)나 tie-break 없는 비교자로 재구현하면, 같은 입력에서도 실행마다
    //   미세하게 다른 트리가 만들어질 수 있어 렌더링 결과 재현성(비교 테스트, 회귀 테스트)이 깨진다.
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
