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

    RenderSettings();
};

struct Image {
    int width;
    int height;
    std::vector<unsigned char> pixels;

    Image();
    Image(int width_value, int height_value);
};

bool findNearestHit(const Scene& scene,
                    const Ray& ray,
                    HitRecord& hit,
                    double t_min = kRayTMin,
                    double t_max = std::numeric_limits<double>::infinity());
bool isOccluded(const Scene& scene,
                const Ray& shadow_ray,
                double max_distance);
Color shadeHit(const Scene& scene,
               const HitRecord& hit,
               const Ray& view_ray);
Color traceRay(const Scene& scene,
               const Ray& ray,
               int max_depth = 1);
Image renderScene(const Scene& scene,
                  const RenderSettings& settings = RenderSettings());

}  // namespace ray
