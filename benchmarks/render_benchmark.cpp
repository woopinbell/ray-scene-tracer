#include "ray.hpp"

#include <chrono>
#include <iostream>
#include <memory>
#include <string>

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
    scene.addShape(std::make_shared<ray::Plane>(
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
            scene.addShape(std::make_shared<ray::Sphere>(
                ray::Vec3(x, y, z),
                0.42,
                ray::Material(color)));
        }
    }
    return scene;
}

struct Sample {
    double milliseconds;
    ray::RenderStats stats;
    std::string checksum;
};

Sample render(const ray::Scene& scene) {
    ray::RenderStats stats;
    const ray::Image image =
        ray::renderScene(scene, ray::RenderSettings(), &stats);
    return Sample{stats.renderMilliseconds, stats, ray::checksumHex(image)};
}

}  // namespace

int main() {
    const Sample sample = render(makeDenseScene());
    std::cout << sample.checksum << '\n';
    return 0;
}
