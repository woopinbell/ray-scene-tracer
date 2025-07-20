#pragma once

#include "ray/camera.hpp"

#include <limits>

namespace ray {

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

}  // namespace ray
