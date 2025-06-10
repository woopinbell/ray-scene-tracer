#pragma once

#include "ray/geometry.hpp"

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
    std::vector<std::shared_ptr<Shape>> shapes;

    Scene();

    void addShape(const std::shared_ptr<Shape>& shape);
    void addLight(const Light& light);
    bool intersect(const Ray& ray,
                   double t_min,
                   double t_max,
                   HitRecord& hit) const;
};

}  // namespace ray
