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
    std::vector<std::uint32_t> unboundedIndices_;
    bool accelerationReady_;
};

}  // namespace ray
