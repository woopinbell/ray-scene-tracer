#include "ray/camera.hpp"

#include <algorithm>
#include <cmath>

namespace ray {

// [INTV:EDGE] 정규직교 기저(orthonormal basis) 구성: up_seed는 "대략 위쪽이 어디인지"를 알려주는
// 힌트일 뿐, forward와 정확히 수직일 필요는 없다(아래 외적이 정확히 수직인 축을 뽑아낸다). 다만
// forward와 거의 평행하면(외적 결과가 거의 0) 좌표계가 무너지므로 다른 기본 축으로 대체한다.
// - [TRAP] up_seed와 forward가 거의 평행한 경우를 처리하지 않고 그대로 cross(up_seed, forward)를
//   쓰면, 결과 벡터의 길이가 0에 가까워져 그다음 normalize()가 (math.cpp의 가드에 의해) 영벡터를
//   반환하고 카메라 좌표계 전체가 무너져 렌더링 결과가 새까맣게 나오는 원인 추적하기 어려운 버그가 된다.
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

    // [INTV:ARCH] forward를 기준으로 서로 수직인 right/up을 외적으로 만들어 정규직교 기저를 구성 —
    // 그람-슈미트 직교화와 같은 원리. 이 세 축이 픽셀 위치를 3D 광선 방향으로 바꾸는 카메라 좌표계가 된다.
    const Vec3 right = normalize(cross(up_seed, forward));
    const Vec3 true_up = normalize(cross(forward, right));
    const double safe_width = std::max(1, width);
    const double safe_height = std::max(1, height);
    const double aspect = safe_width / safe_height;
    const double fov_radians =
        camera.fovDegrees * 3.14159265358979323846 / 180.0;
    // [INTV:ARCH] 핀홀 카메라 모델: FOV 절반에 대한 tan 값이 카메라로부터 단위 거리 앞에 놓인
    // 뷰포트(가상 스크린)의 절반 높이가 된다 — FOV가 클수록 뷰포트가 넓어져 더 넓은 화각을 담는다.
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
    // [INTV:EDGE] 픽셀 좌표(왼쪽 위 원점, 아래로 갈수록 증가)를 [-0.5, 0.5] 뷰포트 좌표(u, v)로
    // 변환. v만 "0.5 - ..."로 부호를 뒤집는 이유: 이미지 좌표계는 아래 방향이 +y인 반면 카메라의
    // up 벡터는 위쪽이 +이므로, 이 반전이 없으면 렌더링 결과가 상하로 뒤집혀 나온다.
    const double u =
        (pixel_x / safe_width - 0.5) * frame.viewportWidth;
    const double v =
        (0.5 - pixel_y / safe_height) * frame.viewportHeight;
    const Vec3 direction =
        normalize(frame.forward + frame.right * u + frame.up * v);
    return Ray(camera.position, direction);
}

}  // namespace ray
