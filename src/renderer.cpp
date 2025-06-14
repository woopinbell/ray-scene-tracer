#include "ray/renderer.hpp"

#include <cmath>

namespace ray {

RenderSettings::RenderSettings()
    : samplesPerPixel(1),
      maxDepth(1),
      tMin(kRayTMin),
      tMax(std::numeric_limits<double>::infinity()) {}

Image::Image() : width(0), height(0), pixels() {}

Image::Image(int width_value, int height_value)
    : width(width_value),
      height(height_value),
      pixels(static_cast<std::size_t>(width_value * height_value * 3), 0) {}

Image renderScene(const Scene& scene, const RenderSettings& settings) {
    Image image(scene.width, scene.height);
    std::size_t offset = 0;
    for (int y = 0; y < scene.height; ++y) {
        for (int x = 0; x < scene.width; ++x) {
            const Ray ray = makeCameraRay(scene.camera,
                                          scene.width,
                                          scene.height,
                                          x + 0.5,
                                          y + 0.5);
            const Color color = traceRay(scene, ray, settings.maxDepth);
            const Color clamped = clampColor(color);
            image.pixels[offset++] = static_cast<unsigned char>(
                std::lround(clamped.x * 255.0));
            image.pixels[offset++] = static_cast<unsigned char>(
                std::lround(clamped.y * 255.0));
            image.pixels[offset++] = static_cast<unsigned char>(
                std::lround(clamped.z * 255.0));
        }
    }
    return image;
}

}  // namespace ray
