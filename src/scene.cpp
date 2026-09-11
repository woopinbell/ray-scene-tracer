#include "ray/scene.hpp"

#include <limits>
#include <optional>
#include <utility>

namespace ray {

Material::Material()
    : albedo(1.0, 1.0, 1.0),
      type(MaterialType::Diffuse) {}

Material::Material(const Color& color, MaterialType type_value)
    : albedo(color), type(type_value) {}

Light::Light()
    : position(), brightness(1.0), color(1.0, 1.0, 1.0) {}

Light::Light(const Vec3& position_value,
             double brightness_value,
             const Color& color_value)
    : position(position_value),
      brightness(brightness_value),
      color(color_value) {}

Camera::Camera()
    : position(),
      direction(0.0, 0.0, 1.0),
      fovDegrees(60.0),
      up(0.0, 1.0, 0.0) {}

Camera::Camera(const Vec3& position_value,
               const Vec3& direction_value,
               double fov_degrees_value)
    : position(position_value),
      direction(direction_value),
      fovDegrees(fov_degrees_value),
      up(0.0, 1.0, 0.0) {}

Scene::Scene()
    : width(0),
      height(0),
      hasResolution(false),
      hasAmbient(false),
      hasCamera(false),
      ambientRatio(0.0),
      ambientColor(1.0, 1.0, 1.0),
      background(0.02, 0.03, 0.05),
      camera(),
      lights(),
      shapes_(),
      bvh_(),
      unboundedIndices_(),
      accelerationReady_(false) {}

// [INTV:EDGE] BVH는 "빌드 시점의 도형 목록" 스냅샷을 담고 있으므로, 도형이 하나라도 추가되면 그
// 스냅샷은 즉시 낡은 것이 된다 — 여기서 캐시를 지우고 accelerationReady_를 내려, 다음 intersect가
// 낡은 가속 구조로 잘못된 결과를 내는 대신 buildAcceleration()이 다시 호출되기 전까지 선형 탐색으로
// 폴백하게 한다.
// - [TRAP] 이 무효화를 빼먹고 재구현하면, "도형 추가 -> intersect 호출(재빌드 안 함)" 순서에서 새로
//   추가된 도형이 BVH 트리에는 없는 채로 조용히 광선 교차 판정에서 누락되는(투명해 보이는) 버그가 된다.
void Scene::addShape(std::unique_ptr<Shape> shape) {
    if (shape) {
        shapes_.push_back(std::move(shape));
        bvh_.clear();
        unboundedIndices_.clear();
        accelerationReady_ = false;
    }
}

std::size_t Scene::shapeCount() const {
    return shapes_.size();
}

const Shape& Scene::shapeAt(std::size_t index) const {
    return *shapes_.at(index);
}

void Scene::addLight(const Light& light) {
    lights.push_back(light);
}

// [INTV:ARCH] 도형을 "유한한 경계 상자가 있는 것(BVH에 넣음)"과 "없는 것(예: 무한 평면 — 매번 직접
// 검사)"으로 분류해 각자 다른 가속 전략을 적용한다.
void Scene::buildAcceleration() {
    std::vector<BvhPrimitive> bounded;
    unboundedIndices_.clear();
    bounded.reserve(shapes_.size());
    unboundedIndices_.reserve(shapes_.size());

    for (std::size_t index = 0; index < shapes_.size(); ++index) {
        if (!shapes_[index]) {
            continue;
        }
        const std::optional<Aabb> shape_bounds =
            shapes_[index]->bounds();
        if (shape_bounds && shape_bounds->isValid()) {
            bounded.push_back(BvhPrimitive{
                static_cast<std::uint32_t>(index),
                *shape_bounds
            });
        } else {
            unboundedIndices_.push_back(
                static_cast<std::uint32_t>(index));
        }
    }
    bvh_.build(std::move(bounded));
    accelerationReady_ = true;
}

bool Scene::accelerationReady() const {
    return accelerationReady_;
}

bool Scene::intersect(const Ray& ray,
                      double t_min,
                      double t_max,
                      HitRecord& hit,
                      AccelMode mode,
                      RenderStats* stats) const {
    bool found = false;
    double closest = t_max;
    std::uint32_t best_index = 0;
    HitRecord candidate;

    // [INTV:ARCH] 한 도형을 검사하고 지금까지의 최단 거리보다 가까우면 갱신하는 로직을 [&] 캡처 람다로
    // 뽑아, 아래 선형 탐색 경로와 BVH 순회 경로 양쪽에서 완전히 같은 코드를 재사용한다.
    // - [TRAP] candidate.t == closest(정확히 같은 거리) tie-break를 "index가 더 큰 쪽"으로 고정하지
    //   않으면, 같은 장면을 Linear 모드와 Bvh 모드로 각각 렌더링했을 때 도형 순회 순서가 달라
    //   서로 다른 승자를 택할 수 있다 — 두 가속 모드의 렌더링 결과가 완전히 일치해야 한다는 이
    //   프로젝트의 검증 목표(Linear를 기준선으로 Bvh를 비교)를 깨는 원인이 된다.
    const auto test_shape =
        [&](std::uint32_t index) {
            const std::unique_ptr<Shape>& shape = shapes_[index];
            if (!shape) {
                return;
            }
            if (stats) {
                ++stats->primitiveTests;
            }
            if (!shape->intersect(
                    ray, t_min, closest, candidate)) {
                return;
            }
            if (!found ||
                candidate.t < closest ||
                (candidate.t == closest && index > best_index)) {
                found = true;
                closest = candidate.t;
                best_index = index;
                hit = candidate;
            }
        };

    if (mode == AccelMode::Linear || !accelerationReady_) {
        for (std::size_t index = 0; index < shapes_.size(); ++index) {
            test_shape(static_cast<std::uint32_t>(index));
        }
        return found;
    }

    // [INTV:PERF] BVH 트리를 재귀 함수 호출 대신 명시적 스택(vector)으로 순회 — 트리가 깊어도
    // 함수 호출 스택이 쌓이지 않고(스택 오버플로 위험 없음), "가까운 자식을 나중에 push(=먼저 pop)"
    // 하는 방문 순서 제어도 재귀보다 다루기 쉬워진다.
    struct StackEntry {
        std::uint32_t node;
        // 이 노드의 경계 상자에 광선이 들어오는 지점의 t값(가까운 노드부터 방문하기 위한 정렬 키).
        double entry;
    };
    std::vector<StackEntry> stack;
    const std::vector<BvhNode>& nodes = bvh_.nodes();
    const std::vector<std::uint32_t>& indices =
        bvh_.primitiveIndices();
    if (!nodes.empty()) {
        double root_entry = t_min;
        if (stats) {
            ++stats->aabbTests;
        }
        if (nodes[0].bounds.intersect(
                ray, t_min, closest, &root_entry)) {
            stack.push_back(StackEntry{0, root_entry});
        }
    }

    while (!stack.empty()) {
        const StackEntry current = stack.back();
        stack.pop_back();
        // [INTV:PERF] 가지치기(pruning): 이 노드에 들어가는 지점(entry)이 지금까지 찾은 가장 가까운
        // 히트보다 더 멀면, 그 안의 도형들을 검사해봐야 절대 더 나은 결과가 나올 수 없으므로 서브트리
        // 통째로 건너뛴다 — 선형 탐색 대비 BVH가 빠른 핵심 이유(안 볼 도형들을 서브트리째 잘라냄).
        if (current.entry > closest) {
            continue;
        }

        const BvhNode& node = nodes[current.node];
        if (node.isLeaf()) {
            for (std::uint32_t offset = 0;
                 offset < node.count;
                 ++offset) {
                test_shape(indices[node.first + offset]);
            }
            continue;
        }

        double left_entry = t_min;
        double right_entry = t_min;
        if (stats) {
            stats->aabbTests += 2;
        }
        const bool hit_left = nodes[node.left].bounds.intersect(
            ray, t_min, closest, &left_entry);
        const bool hit_right = nodes[node.right].bounds.intersect(
            ray, t_min, closest, &right_entry);

        if (hit_left && hit_right) {
            // [INTV:PERF] 두 자식 다 상자에 맞았다면 더 가까운(entry가 작은) 쪽을 "나중에" push해서
            // 스택(LIFO) 특성상 먼저 pop되어 먼저 방문되게 한다 — 가까운 쪽을 먼저 봐야 closest가
            // 일찍 좁혀져서 위의 가지치기가 더 많은 먼 쪽 서브트리를 잘라낼 수 있다.
            // - [TRAP] near/far 순서를 반대로 push하면(먼 쪽을 먼저 방문) 가지치기 기회가 줄어
            //   BVH를 쓰고도 성능 이점을 온전히 못 얻는다(정답은 같아도 느려지는 성능 버그).
            const bool left_first =
                left_entry < right_entry ||
                (left_entry == right_entry &&
                 node.left < node.right);
            const StackEntry near_entry =
                left_first
                    ? StackEntry{node.left, left_entry}
                    : StackEntry{node.right, right_entry};
            const StackEntry far_entry =
                left_first
                    ? StackEntry{node.right, right_entry}
                    : StackEntry{node.left, left_entry};
            stack.push_back(far_entry);
            stack.push_back(near_entry);
        } else if (hit_left) {
            stack.push_back(StackEntry{node.left, left_entry});
        } else if (hit_right) {
            stack.push_back(StackEntry{node.right, right_entry});
        }
    }

    // BVH에 들어가지 못한(경계 상자가 없는) 도형들은 트리로 걸러낼 수 없으니 항상 직접 검사한다.
    for (std::uint32_t index : unboundedIndices_) {
        test_shape(index);
    }
    return found;
}

}  // namespace ray
