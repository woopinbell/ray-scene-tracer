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

void Scene::addShape(std::unique_ptr<Shape> shape) {
    if (shape) {
        shapes.push_back(std::move(shape));
    }
}

void Scene::addLight(const Light& light) {
    lights.push_back(light);
}

bool Scene::intersect(const Ray& ray,
                      double t_min,
                      double t_max,
                      HitRecord& hit,
                      AccelMode mode,
                      RenderStats* stats) const {
    (void)mode;
    bool found = false;
    double closest = t_max;
    std::uint32_t best_index = 0;
    HitRecord candidate;

    const auto test_shape =
        [&](std::uint32_t index) {
            const std::unique_ptr<Shape>& shape = shapes[index];
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

    for (std::size_t index = 0; index < shapes.size(); ++index) {
        test_shape(static_cast<std::uint32_t>(index));
    }
    return found;
}

}  // namespace ray
