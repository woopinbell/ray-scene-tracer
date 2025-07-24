#pragma once

#include "ray/scene.hpp"

namespace ray {

struct CameraFrame {
    Vec3 forward;
    Vec3 right;
    Vec3 up;
    double viewportWidth;
    double viewportHeight;
};

CameraFrame buildCameraFrame(const Camera& camera, int width, int height);
Ray makeCameraRay(const Camera& camera,
                  int width,
                  int height,
                  double pixel_x,
                  double pixel_y);
Ray makeCameraRay(const Camera& camera,
                  const CameraFrame& frame,
                  int width,
                  int height,
                  double pixel_x,
                  double pixel_y);

}  // namespace ray
