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
    // [INTV:ARCH] 0이면 "하드웨어 코어 수만큼 자동 결정"이라는 의미(renderer.cpp의
    // hardware_concurrency 사용부 참고) — 0을 "미설정" 센티널 값으로 쓰는 관례.
    unsigned int threadCount;

    RenderSettings();
};

// [INTV:ARCH] width*height*3(RGB 각 1바이트)개의 unsigned char를 한 줄로 늘어놓은 원시 픽셀 버퍼 —
// 대부분의 이미지 파일 포맷/GPU 텍스처 업로드가 기대하는 표준적인 배치.
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
