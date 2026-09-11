#pragma once

#include "ray/math.hpp"

#include <cstdint>
#include <vector>

namespace ray {

// [INTV:ARCH] Linear는 도형을 하나씩 다 검사하는 기준선(정답 검증/디버깅용), Bvh는 가속 구조를 타고
// 내려가는 방식 — 둘 다 지원해 BVH의 결과를 Linear와 비교 검증할 수 있게 한 설계.
enum class AccelMode {
    Linear,
    Bvh
};

// [INTV:ARCH] AABB(Axis-Aligned Bounding Box): 도형을 감싸는 가장 작은 직육면체. "광선이 이 상자를
// 스치기라도 하는가"를 도형 자체의 정확한 교차 계산보다 훨씬 싸게 먼저 걸러내는 조기 컷오프 —
// BVH 트리의 각 노드가 들고 있는 정보가 바로 이것.
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

// [INTV:ARCH] [INTV:PERF] 포인터로 자식을 가리키는 대신 std::vector<BvhNode> 안의 인덱스(left/right)
// 로 자식을 가리키는 "인덱스 기반 트리" — 노드들이 메모리에 연속으로 붙어 있어 캐시 지역성이 좋고,
// new/delete 없이 통째로 직렬화·복사할 수 있다.
// - [TRAP] 포인터 기반 트리로 재구현하면 노드마다 별도 힙 할당이 생겨 트리 순회 시 캐시 미스가 늘고,
//   Bvh 객체 자체의 복사/이동도 각 노드를 깊은 복사해야 하는 번거로움이 생긴다.
struct BvhNode {
    Aabb bounds;
    std::uint32_t left = 0;
    std::uint32_t right = 0;
    std::uint32_t first = 0;
    // [INTV:ARCH] count > 0이면 리프 노드로, first부터 count개의 도형을 직접 담고 있다는 뜻 —
    // 내부 노드와 리프 노드를 별도 타입으로 나누지 않고 이 필드 하나로 구분하는 태그리스 유니온 설계.
    std::uint32_t count = 0;

    bool isLeaf() const;
};

class Bvh {
public:
    void build(std::vector<BvhPrimitive> primitives);
    void clear();
    bool empty() const;

    const std::vector<BvhNode>& nodes() const;
    const std::vector<std::uint32_t>& primitiveIndices() const;

private:
    std::uint32_t buildNode(std::vector<BvhPrimitive>& primitives,
                            std::uint32_t first,
                            std::uint32_t last);

    std::vector<BvhNode> nodes_;
    std::vector<std::uint32_t> primitiveIndices_;
};

}  // namespace ray
