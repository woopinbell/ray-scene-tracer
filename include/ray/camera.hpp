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
// [INTV:PERF] 오버로드 쌍: 아래쪽은 CameraFrame을 미리 계산해 넘겨받고, 위쪽은 그 계산을 매번 내부에서
// 새로 한다 — renderer.cpp처럼 같은 프레임으로 수백만 픽셀을 그릴 때는 프레임을 한 번만 만들어 재사용
// 하는 아래쪽 버전을 써야 픽셀마다 같은 삼각함수/정규화 계산을 반복하지 않는다.
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
