#pragma once

#include "ray/geometry.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace ray {

struct Light {
    Vec3 position;
    double brightness;
    Color color;

    Light();
    Light(const Vec3& position_value,
          double brightness_value,
          const Color& color_value);
};

struct Camera {
    Vec3 position;
    Vec3 direction;
    double fovDegrees;
    Vec3 up;

    Camera();
    Camera(const Vec3& position_value,
           const Vec3& direction_value,
           double fov_degrees_value);
};

struct RenderStats {
    std::uint64_t primaryRays = 0;
    std::uint64_t secondaryRays = 0;
    std::uint64_t shadowRays = 0;
    std::uint64_t primitiveTests = 0;
    std::uint64_t aabbTests = 0;
    double renderMilliseconds = 0.0;
};

class Scene {
public:
    int width;
    int height;
    bool hasResolution;
    bool hasAmbient;
    bool hasCamera;
    double ambientRatio;
    Color ambientColor;
    Color background;
    Camera camera;
    std::vector<Light> lights;

    Scene();
    // [INTV:ARCH] Scene은 std::unique_ptr<Shape>들(shapes_)을 소유하는데, unique_ptr 자체가 복사를
    // 지원하지 않아 컴파일러 기본 복사 생성도 애초에 만들어지지 않는다 — "= delete"는 그 사실을 코드로도
    // 명시한 것. 이동(move)은 소유권을 옮기기만 하면 되므로 기본 구현으로 충분하다.
    // - [TRAP] noexcept 없이 이동 생성자/대입을 선언하면, std::vector<Scene>이 재배치될 때 표준
    //   라이브러리가 "이동이 예외를 던질 수도 있다"고 보수적으로 판단해 이동 대신 복사를 강제로
    //   시도하는데, Scene은 애초에 복사가 삭제되어 있어 컴파일 자체가 실패한다.
    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;
    Scene(Scene&&) noexcept = default;
    Scene& operator=(Scene&&) noexcept = default;

    void addShape(std::unique_ptr<Shape> shape);
    std::size_t shapeCount() const;
    const Shape& shapeAt(std::size_t index) const;
    void addLight(const Light& light);
    void buildAcceleration();
    bool accelerationReady() const;
    bool intersect(const Ray& ray,
                   double t_min,
                   double t_max,
                   HitRecord& hit,
                   AccelMode mode = AccelMode::Bvh,
                   RenderStats* stats = nullptr) const;

private:
    std::vector<std::unique_ptr<Shape>> shapes_;
    Bvh bvh_;
    // 유한한 경계 상자가 없는 도형(예: 무한 평면)의 색인 — BVH 트리로는 걸러낼 수 없어 매 교차
    // 판정마다 직접 전수 검사해야 하는 목록.
    std::vector<std::uint32_t> unboundedIndices_;
    bool accelerationReady_;
};

}  // namespace ray
