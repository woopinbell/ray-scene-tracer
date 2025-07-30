#include "ray.hpp"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

ray::Scene makeDenseScene() {
    ray::Scene scene;
    scene.width = 640;
    scene.height = 360;
    scene.hasResolution = true;
    scene.hasAmbient = true;
    scene.hasCamera = true;
    scene.ambientRatio = 0.08;
    scene.ambientColor = ray::Color(1.0, 1.0, 1.0);
    scene.background = ray::Color(0.02, 0.03, 0.05);
    scene.camera = ray::Camera(ray::Vec3(0.0, 5.0, -18.0),
                               ray::Vec3(0.0, -0.12, 1.0),
                               48.0);
    scene.addLight(ray::Light(ray::Vec3(-12.0, 16.0, -8.0),
                              0.85,
                              ray::Color(1.0, 0.96, 0.88)));
    scene.addLight(ray::Light(ray::Vec3(14.0, 10.0, 12.0),
                              0.55,
                              ray::Color(0.75, 0.85, 1.0)));

    const ray::Material ground(ray::Color(0.35, 0.38, 0.42));
    scene.addShape(std::make_unique<ray::Plane>(
        ray::Vec3(0.0, -1.0, 0.0),
        ray::Vec3(0.0, 1.0, 0.0),
        ground));

    for (int row = 0; row < 20; ++row) {
        for (int column = 0; column < 20; ++column) {
            const double x = (column - 9.5) * 1.05;
            const double z = 2.0 + row * 1.05;
            const double y = -0.45 + 0.18 * ((row + column) % 4);
            const ray::Color color(
                0.2 + 0.6 * static_cast<double>(column % 5) / 4.0,
                0.2 + 0.6 * static_cast<double>(row % 5) / 4.0,
                0.25 + 0.5 * static_cast<double>((row + column) % 5) / 4.0);
            scene.addShape(std::make_unique<ray::Sphere>(
                ray::Vec3(x, y, z),
                0.42,
                ray::Material(color)));
        }
    }
    scene.buildAcceleration();
    return scene;
}

struct Sample {
    double milliseconds;
    ray::RenderStats stats;
    std::string checksum;
};

Sample render(const ray::Scene& scene, ray::AccelMode mode) {
    ray::RenderStats stats;
    ray::RenderSettings settings;
    settings.accelMode = mode;
    settings.threadCount = 1;
    const ray::Image image = ray::renderScene(scene, settings, &stats);
    return Sample{stats.renderMilliseconds, stats, ray::checksumHex(image)};
}

Sample measure(const ray::Scene& scene, ray::AccelMode mode) {
    (void)render(scene, mode);

    std::vector<Sample> samples;
    for (int iteration = 0; iteration < 5; ++iteration) {
        samples.push_back(render(scene, mode));
    }
    std::sort(samples.begin(),
              samples.end(),
              [](const Sample& left, const Sample& right) {
                  return left.milliseconds < right.milliseconds;
              });
    const Sample median = samples[samples.size() / 2];
    for (const Sample& sample : samples) {
        if (sample.checksum != median.checksum ||
            sample.stats.primitiveTests != median.stats.primitiveTests ||
            sample.stats.aabbTests != median.stats.aabbTests) {
            throw std::runtime_error(
                "benchmark runs produced different results");
        }
    }
    return median;
}

void printResult(const std::string& name,
                 const Sample& sample,
                 bool trailing_comma) {
    std::cout << "    \"" << name << "\": {\n"
              << "      \"medianMilliseconds\": "
              << sample.milliseconds << ",\n"
              << "      \"primaryRays\": "
              << sample.stats.primaryRays << ",\n"
              << "      \"shadowRays\": "
              << sample.stats.shadowRays << ",\n"
              << "      \"aabbTests\": "
              << sample.stats.aabbTests << ",\n"
              << "      \"primitiveTests\": "
              << sample.stats.primitiveTests << ",\n"
              << "      \"checksum\": \""
              << sample.checksum << "\"\n"
              << "    }" << (trailing_comma ? "," : "") << "\n";
}

}  // namespace

int main() {
    const ray::Scene scene = makeDenseScene();
    const Sample linear = measure(scene, ray::AccelMode::Linear);
    const Sample bvh = measure(scene, ray::AccelMode::Bvh);
    if (linear.checksum != bvh.checksum) {
        throw std::runtime_error(
            "linear and BVH renders produced different images");
    }

    std::cout << std::fixed << std::setprecision(3)
              << "{\n"
              << "  \"scene\": \"dense-20x20\",\n"
              << "  \"width\": 640,\n"
              << "  \"height\": 360,\n"
              << "  \"warmupRuns\": 1,\n"
              << "  \"measuredRuns\": 5,\n"
              << "  \"results\": {\n";
    printResult("linear", linear, true);
    printResult("bvh", bvh, false);
    std::cout << "  }\n"
              << "}\n";
    return 0;
}
