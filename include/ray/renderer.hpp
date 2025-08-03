#pragma once

#include "ray/camera.hpp"

#include <limits>
#include <vector>

namespace ray {

struct RenderSettings {
    int samplesPerPixel;
    int maxDepth;
    double tMin;
    double tMax;
    AccelMode accelMode;
    unsigned int threadCount;

    RenderSettings();
};

struct Image {
    int width;
    int height;
    std::vector<unsigned char> pixels;

    Image();
    Image(int width_value, int height_value);

    void validate() const;
};

bool findNearestHit(const Scene& scene,
                    const Ray& ray,
                    HitRecord& hit,
                    double t_min = kRayTMin,
                    double t_max = std::numeric_limits<double>::infinity(),
                    AccelMode mode = AccelMode::Bvh,
                    RenderStats* stats = nullptr);
bool isOccluded(const Scene& scene,
                const Ray& shadow_ray,
                double max_distance,
                AccelMode mode = AccelMode::Bvh,
                RenderStats* stats = nullptr);
Color shadeHit(const Scene& scene,
               const HitRecord& hit,
               const Ray& view_ray,
               AccelMode mode = AccelMode::Bvh,
               RenderStats* stats = nullptr);
Color traceRay(const Scene& scene,
               const Ray& ray,
               int max_depth = 1,
               AccelMode mode = AccelMode::Bvh,
               RenderStats* stats = nullptr);
Image renderScene(const Scene& scene,
                  const RenderSettings& settings = RenderSettings(),
                  RenderStats* stats = nullptr);

}  // namespace ray
