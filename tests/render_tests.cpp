#include "ray.hpp"

#include <iostream>
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
    } catch (const std::exception& error) {
        std::cerr << "render determinism failed: "
                  << error.what() << '\n';
        return 1;
    }
    std::cout << "render determinism checks passed\n";
    return 0;
}
