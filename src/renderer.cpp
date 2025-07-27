#include "ray/renderer.hpp"

#include <chrono>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace ray {

namespace {

std::size_t pixelStorageSize(int width, int height) {
    if (width <= 0 || height <= 0) {
        throw std::invalid_argument("image dimensions must be positive");
    }
    const std::size_t safe_width = static_cast<std::size_t>(width);
    const std::size_t safe_height = static_cast<std::size_t>(height);
    const std::size_t limit = std::numeric_limits<std::size_t>::max();
    if (safe_width > limit / safe_height ||
        safe_width * safe_height > limit / 3) {
        throw std::overflow_error("image dimensions are too large");
    }
    return safe_width * safe_height * 3;
}

}  // namespace

RenderSettings::RenderSettings()
    : samplesPerPixel(1),
      maxDepth(1),
      tMin(kRayTMin),
      tMax(std::numeric_limits<double>::infinity()),
      accelMode(AccelMode::Bvh) {}

Image::Image() : width(0), height(0), pixels() {}

Image::Image(int width_value, int height_value)
    : width(width_value),
      height(height_value),
      pixels(pixelStorageSize(width_value, height_value), 0) {}

Image renderScene(const Scene& scene,
                  const RenderSettings& settings,
                  RenderStats* stats) {
    const auto started = std::chrono::steady_clock::now();
    Image image(scene.width, scene.height);
    const CameraFrame camera_frame =
        buildCameraFrame(scene.camera, scene.width, scene.height);
    std::size_t offset = 0;
    for (int y = 0; y < scene.height; ++y) {
        for (int x = 0; x < scene.width; ++x) {
            const Ray ray = makeCameraRay(scene.camera,
                                          camera_frame,
                                          scene.width,
                                          scene.height,
                                          x + 0.5,
                                          y + 0.5);
            if (stats) {
                ++stats->primaryRays;
            }
            const Color color = traceRay(scene,
                                         ray,
                                         settings.maxDepth,
                                         settings.accelMode,
                                         stats);
            const Color clamped = clampColor(color);
            image.pixels[offset++] = static_cast<unsigned char>(
                std::lround(clamped.x * 255.0));
            image.pixels[offset++] = static_cast<unsigned char>(
                std::lround(clamped.y * 255.0));
            image.pixels[offset++] = static_cast<unsigned char>(
                std::lround(clamped.z * 255.0));
        }
    }
    if (stats) {
        const auto finished = std::chrono::steady_clock::now();
        stats->renderMilliseconds =
            std::chrono::duration<double, std::milli>(finished - started).count();
    }
    return image;
}

}  // namespace ray
