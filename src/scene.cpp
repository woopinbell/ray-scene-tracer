#include "ray/scene.hpp"

namespace ray {

Material::Material() : albedo(1.0, 1.0, 1.0) {}

Material::Material(const Color& color) : albedo(color) {}

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
      shapes() {}

void Scene::addLight(const Light& light) {
    lights.push_back(light);
}

}  // namespace ray
