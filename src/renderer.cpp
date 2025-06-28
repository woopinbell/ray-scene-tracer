#include "ray/renderer.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <thread>
#include <vector>

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
      maxDepth(4),
      tMin(kRayTMin),
      tMax(std::numeric_limits<double>::infinity()),
      accelMode(AccelMode::Bvh),
      threadCount(0) {}

Image::Image() : width(0), height(0), pixels() {}

Image::Image(int width_value, int height_value)
    : width(width_value),
      height(height_value),
      pixels(pixelStorageSize(width_value, height_value), 0) {}

void Image::validate() const {
    const std::size_t expected = pixelStorageSize(width, height);
    if (pixels.size() != expected) {
        throw std::invalid_argument(
            "image pixel storage does not match its dimensions");
    }
}

Image renderScene(const Scene& scene,
                  const RenderSettings& settings,
                  RenderStats* stats) {
    constexpr int kTileSize = 16;
    const auto started = std::chrono::steady_clock::now();
    Image image(scene.width, scene.height);
    const CameraFrame camera_frame =
        buildCameraFrame(scene.camera, scene.width, scene.height);

    const std::size_t tiles_x =
        (static_cast<std::size_t>(scene.width) + kTileSize - 1) /
        kTileSize;
    const std::size_t tiles_y =
        (static_cast<std::size_t>(scene.height) + kTileSize - 1) /
        kTileSize;
    const std::size_t tile_count = tiles_x * tiles_y;
    unsigned int worker_count = settings.threadCount;
    if (worker_count == 0) {
        worker_count = std::thread::hardware_concurrency();
        if (worker_count == 0) {
            worker_count = 1;
        }
    }
    worker_count = static_cast<unsigned int>(
        std::min<std::size_t>(worker_count, tile_count));

    struct alignas(64) WorkerStats {
        RenderStats values;
    };
    std::vector<WorkerStats> worker_stats(worker_count);
    std::atomic<std::size_t> next_tile{0};
    std::vector<std::thread> workers;
    workers.reserve(worker_count);

    struct ThreadJoiner {
        std::vector<std::thread>& workers;
        std::atomic<std::size_t>& nextTile;
        std::size_t tileCount;

        ~ThreadJoiner() {
            nextTile.store(tileCount, std::memory_order_relaxed);
            for (std::thread& worker : workers) {
                if (worker.joinable()) {
                    worker.join();
                }
            }
        }
    };
    const ThreadJoiner thread_joiner{
        workers,
        next_tile,
        tile_count
    };

    for (unsigned int worker = 0; worker < worker_count; ++worker) {
        workers.emplace_back([&, worker]() {
            RenderStats& local = worker_stats[worker].values;
            for (;;) {
                const std::size_t tile =
                    next_tile.fetch_add(1, std::memory_order_relaxed);
                if (tile >= tile_count) {
                    break;
                }
                const int start_x =
                    static_cast<int>((tile % tiles_x) * kTileSize);
                const int start_y =
                    static_cast<int>((tile / tiles_x) * kTileSize);
                const int end_x =
                    std::min(start_x + kTileSize, scene.width);
                const int end_y =
                    std::min(start_y + kTileSize, scene.height);

                for (int y = start_y; y < end_y; ++y) {
                    for (int x = start_x; x < end_x; ++x) {
                        const Ray ray =
                            makeCameraRay(scene.camera,
                                          camera_frame,
                                          scene.width,
                                          scene.height,
                                          x + 0.5,
                                          y + 0.5);
                        ++local.primaryRays;
                        const Color color =
                            traceRay(scene,
                                     ray,
                                     settings.maxDepth,
                                     settings.accelMode,
                                     &local);
                        const Color clamped = clampColor(color);
                        const std::size_t offset =
                            (static_cast<std::size_t>(y) *
                                 static_cast<std::size_t>(scene.width) +
                             static_cast<std::size_t>(x)) *
                            3;
                        image.pixels[offset] =
                            static_cast<unsigned char>(
                                std::lround(clamped.x * 255.0));
                        image.pixels[offset + 1] =
                            static_cast<unsigned char>(
                                std::lround(clamped.y * 255.0));
                        image.pixels[offset + 2] =
                            static_cast<unsigned char>(
                                std::lround(clamped.z * 255.0));
                    }
                }
            }
        });
    }
    for (std::thread& worker : workers) {
        worker.join();
    }

    if (stats) {
        *stats = RenderStats();
        for (const WorkerStats& worker : worker_stats) {
            stats->primaryRays += worker.values.primaryRays;
            stats->secondaryRays += worker.values.secondaryRays;
            stats->shadowRays += worker.values.shadowRays;
            stats->primitiveTests += worker.values.primitiveTests;
            stats->aabbTests += worker.values.aabbTests;
        }
        const auto finished = std::chrono::steady_clock::now();
        stats->renderMilliseconds =
            std::chrono::duration<double, std::milli>(finished - started).count();
    }
    return image;
}

}  // namespace ray
