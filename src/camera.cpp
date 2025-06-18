#include "ray/camera.hpp"

#include <algorithm>
#include <cmath>

namespace ray {

CameraFrame buildCameraFrame(const Camera& camera, int width, int height) {
    Vec3 forward = normalize(camera.direction);
    if (forward.isNearZero()) {
        forward = Vec3(0.0, 0.0, 1.0);
    }

    Vec3 up_seed = normalize(camera.up);
    if (up_seed.isNearZero() || std::fabs(dot(up_seed, forward)) > 0.999) {
        up_seed = std::fabs(forward.y) < 0.999
                      ? Vec3(0.0, 1.0, 0.0)
                      : Vec3(1.0, 0.0, 0.0);
    }

    const Vec3 right = normalize(cross(up_seed, forward));
    const Vec3 true_up = normalize(cross(forward, right));
    const double safe_width = std::max(1, width);
    const double safe_height = std::max(1, height);
    const double aspect = safe_width / safe_height;
    const double fov_radians =
        camera.fovDegrees * 3.14159265358979323846 / 180.0;
    const double viewport_height = 2.0 * std::tan(fov_radians * 0.5);

    CameraFrame frame;
    frame.forward = forward;
    frame.right = right;
    frame.up = true_up;
    frame.viewportHeight = viewport_height;
    frame.viewportWidth = viewport_height * aspect;
    return frame;
}

Ray makeCameraRay(const Camera& camera,
                  int width,
                  int height,
                  double pixel_x,
                  double pixel_y) {
    const CameraFrame frame = buildCameraFrame(camera, width, height);
    return makeCameraRay(camera,
                         frame,
                         width,
                         height,
                         pixel_x,
                         pixel_y);
}

Ray makeCameraRay(const Camera& camera,
                  const CameraFrame& frame,
                  int width,
                  int height,
                  double pixel_x,
                  double pixel_y) {
    const double safe_width = static_cast<double>(std::max(1, width));
    const double safe_height = static_cast<double>(std::max(1, height));
    const double u =
        (pixel_x / safe_width - 0.5) * frame.viewportWidth;
    const double v =
        (0.5 - pixel_y / safe_height) * frame.viewportHeight;
    const Vec3 direction =
        normalize(frame.forward + frame.right * u + frame.up * v);
    return Ray(camera.position, direction);
}

}  // namespace ray
