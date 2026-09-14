// [INTV:ARCH] CTest에는 render_determinism이라는 이름으로 등록되지만(CMakeLists.txt), 이 파일이
// 실제로 검증하는 건 "여러 번 렌더링해도 결과가 같은가"가 아니라 renderScene의 멀티스레드
// 경로에서 워커 스레드 하나가 던진 예외가 호출자(메인 스레드)까지 정상적으로 전파되는가 —
// 진짜 픽셀 단위 결정론(재현성) 검증은 tests/render_determinism.sh(같은 씬을 두 번 렌더링해서
// 바이트 단위로 비교)가 별도로 맡는다.
// [INTV:TRAP] 스레드 풀에서 워커가 예외를 던지면 그 예외는 스레드 경계를 자동으로 못 넘는다 —
// std::thread에서 그냥 예외가 발생하면 프로그램이 std::terminate로 죽어버릴 뿐, 메인 스레드의
// try/catch로는 절대 안 잡힌다. renderScene은 각 워커를 std::exception_ptr로 감싸 예외를
// 캡처해뒀다가(renderer.cpp), 모든 워커가 끝난 뒤 메인 스레드에서 std::rethrow_exception으로
// 다시 던지는 방식으로 이 문제를 해결한다 — 재구현 시 이 캡처+재던지기 장치를 빠뜨리면 워커
// 예외가 조용히 삼켜지거나 프로그램이 통째로 죽는다. 이 테스트가 바로 그 장치가 실제로 동작하는지
// (예외 메시지가 손실 없이 그대로 전파되는지)를 검증한다.
#include "ray.hpp"

#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

ray::Scene makeScene() {
    return ray::parser::parseSceneText(
        "R 96 54\n"
        "A 0.12 255,255,255\n"
        "C 0,1,-4 0,-0.08,1 55\n"
        "L -3,6,-1 0.9 255,244,220\n"
        "L 4,3,5 0.4 180,210,255\n"
        "sp -0.9,0,3 1.5 220,70,45 diffuse\n"
        "sp 0.9,0,3.5 1.4 210,220,235 metal\n"
        "pl 0,-0.8,0 0,1,0 150,165,180 diffuse\n"
        "cy 2,-0.1,6 0.2,1,0.1 0.8 2 70,190,120 diffuse\n",
        "determinism.rt");
}

struct Result {
    ray::Image image;
    ray::RenderStats stats;
    std::string checksum;
};

Result render(const ray::Scene& scene,
              ray::AccelMode mode,
              unsigned int threads) {
    ray::RenderSettings settings;
    settings.accelMode = mode;
    settings.threadCount = threads;
    settings.maxDepth = 4;
    Result result;
    result.image = ray::renderScene(scene, settings, &result.stats);
    result.checksum = ray::checksumHex(result.image);
    return result;
}

void requireSameWork(const Result& left,
                     const Result& right,
                     const std::string& label) {
    require(left.stats.primaryRays == right.stats.primaryRays,
            label + " primary rays");
    require(left.stats.secondaryRays == right.stats.secondaryRays,
            label + " secondary rays");
    require(left.stats.shadowRays == right.stats.shadowRays,
            label + " shadow rays");
    require(left.stats.primitiveTests == right.stats.primitiveTests,
            label + " primitive tests");
    require(left.stats.aabbTests == right.stats.aabbTests,
            label + " AABB tests");
}

class ThrowingShape : public ray::Shape {
public:
    bool intersect(const ray::Ray&,
                   double,
                   double,
                   ray::HitRecord&) const override {
        throw std::runtime_error("worker exception sentinel");
    }

    std::optional<ray::Aabb> bounds() const override {
        return std::nullopt;
    }

    std::string typeName() const override {
        return "throwing";
    }
};

void testWorkerExceptionPropagation() {
    ray::Scene scene;
    scene.width = 32;
    scene.height = 16;
    scene.camera = ray::Camera(
        ray::Vec3(), ray::Vec3(0.0, 0.0, 1.0), 60.0);
    scene.addShape(std::make_unique<ThrowingShape>());
    scene.buildAcceleration();

    ray::RenderSettings settings;
    settings.threadCount = 4;
    bool propagated = false;
    try {
        (void)ray::renderScene(scene, settings);
    } catch (const std::runtime_error& error) {
        propagated =
            std::string(error.what()) == "worker exception sentinel";
    }
    require(propagated,
            "renderScene propagates an exception from a worker");
}

}  // namespace

int main() {
    try {
        const ray::Scene scene = makeScene();
        const Result linear_one =
            render(scene, ray::AccelMode::Linear, 1);
        const Result linear_four =
            render(scene, ray::AccelMode::Linear, 4);
        const Result bvh_one =
            render(scene, ray::AccelMode::Bvh, 1);
        const Result bvh_four =
            render(scene, ray::AccelMode::Bvh, 4);

        require(linear_one.image.pixels == linear_four.image.pixels &&
                    linear_one.image.pixels == bvh_one.image.pixels &&
                    linear_one.image.pixels == bvh_four.image.pixels,
                "all render modes produce identical pixels");
        require(linear_one.checksum == linear_four.checksum &&
                    linear_one.checksum == bvh_one.checksum &&
                    linear_one.checksum == bvh_four.checksum,
                "all render modes produce identical checksums");
        requireSameWork(linear_one,
                        linear_four,
                        "linear thread count");
        requireSameWork(bvh_one,
                        bvh_four,
                        "BVH thread count");
        require(linear_one.stats.primaryRays == 96u * 54u,
                "primary ray count");
        testWorkerExceptionPropagation();
    } catch (const std::exception& error) {
        std::cerr << "render determinism failed: "
                  << error.what() << '\n';
        return 1;
    }
    std::cout << "render determinism checks passed\n";
    return 0;
}
