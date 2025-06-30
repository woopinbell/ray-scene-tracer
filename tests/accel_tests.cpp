#include "ray.hpp"

#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

bool nearlyEqual(double left, double right, double epsilon = 1.0e-9) {
    return std::fabs(left - right) <= epsilon;
}

template <typename T, typename = void>
struct ExposesShapeStorage : std::false_type {};

template <typename T>
struct ExposesShapeStorage<
    T,
    std::void_t<decltype(std::declval<T&>().shapes)>>
    : std::true_type {};

static_assert(!ExposesShapeStorage<ray::Scene>::value,
              "Scene must not expose mutable shape storage");
static_assert(
    std::is_same<
        decltype(std::declval<ray::Scene&>().shapeAt(0)),
        const ray::Shape&>::value,
    "Scene shape access must be read-only");
static_assert(
    !std::is_assignable<
        decltype(std::declval<ray::Sphere&>().center()),
        ray::Vec3>::value,
    "sphere geometry must be read-only");
static_assert(
    !std::is_assignable<
        decltype(std::declval<ray::Plane&>().normal()),
        ray::Vec3>::value,
    "plane geometry must be read-only");
static_assert(
    !std::is_assignable<
        decltype(std::declval<ray::Cylinder&>().axis()),
        ray::Vec3>::value,
    "cylinder geometry must be read-only");

void requireEquivalentHit(ray::Scene& scene,
                          const ray::Ray& ray_value,
                          const std::string& label) {
    ray::HitRecord linear_hit;
    ray::HitRecord bvh_hit;
    const bool linear_found =
        scene.intersect(ray_value,
                        ray::kRayTMin,
                        1000.0,
                        linear_hit,
                        ray::AccelMode::Linear);
    const bool bvh_found =
        scene.intersect(ray_value,
                        ray::kRayTMin,
                        1000.0,
                        bvh_hit,
                        ray::AccelMode::Bvh);
    require(linear_found == bvh_found, label + " hit state");
    if (!linear_found) {
        return;
    }
    require(nearlyEqual(linear_hit.t, bvh_hit.t),
            label + " distance");
    require(linear_hit.point == bvh_hit.point,
            label + " point");
    require(linear_hit.normal == bvh_hit.normal,
            label + " normal");
    require(linear_hit.material.albedo == bvh_hit.material.albedo,
            label + " material");
    require(linear_hit.shape == bvh_hit.shape,
            label + " primitive");
}

void testEmptyScene() {
    ray::Scene scene;
    scene.buildAcceleration();
    requireEquivalentHit(
        scene,
        ray::Ray(ray::Vec3(), ray::Vec3(0.0, 0.0, 1.0)),
        "empty scene");
}

void testSingleSphere() {
    ray::Scene scene;
    scene.addShape(std::make_unique<ray::Sphere>(
        ray::Vec3(0.0, 0.0, 5.0),
        1.0,
        ray::Material(ray::Color(0.8, 0.2, 0.1))));
    scene.buildAcceleration();
    requireEquivalentHit(
        scene,
        ray::Ray(ray::Vec3(), ray::Vec3(0.0, 0.0, 1.0)),
        "single sphere");
}

void testPlaneOnly() {
    ray::Scene scene;
    scene.addShape(std::make_unique<ray::Plane>(
        ray::Vec3(0.0, 0.0, 3.0),
        ray::Vec3(0.0, 0.0, -1.0),
        ray::Material(ray::Color(0.2, 0.8, 0.1))));
    scene.buildAcceleration();
    requireEquivalentHit(
        scene,
        ray::Ray(ray::Vec3(), ray::Vec3(0.0, 0.0, 1.0)),
        "plane-only scene");
}

void testCylinder() {
    ray::Scene scene;
    scene.addShape(std::make_unique<ray::Cylinder>(
        ray::Vec3(0.0, 0.0, 5.0),
        ray::Vec3(1.0, 1.0, 0.0),
        1.0,
        3.0,
        ray::Material(ray::Color(0.2, 0.4, 0.9))));
    scene.buildAcceleration();
    requireEquivalentHit(
        scene,
        ray::Ray(ray::Vec3(), ray::Vec3(0.0, 0.0, 1.0)),
        "arbitrary-axis cylinder");
}

void testEqualDistanceTie() {
    ray::Scene scene;
    scene.addShape(std::make_unique<ray::Sphere>(
        ray::Vec3(0.0, 0.0, 5.0),
        1.0,
        ray::Material(ray::Color(1.0, 0.0, 0.0))));
    scene.addShape(std::make_unique<ray::Sphere>(
        ray::Vec3(0.0, 0.0, 5.0),
        1.0,
        ray::Material(ray::Color(0.0, 1.0, 0.0))));
    const ray::Shape* expected =
        &scene.shapeAt(scene.shapeCount() - 1);
    scene.buildAcceleration();

    ray::HitRecord linear_hit;
    ray::HitRecord bvh_hit;
    const ray::Ray ray_value(ray::Vec3(), ray::Vec3(0.0, 0.0, 1.0));
    require(scene.intersect(ray_value,
                            ray::kRayTMin,
                            100.0,
                            linear_hit,
                            ray::AccelMode::Linear),
            "linear equal-distance hit");
    require(scene.intersect(ray_value,
                            ray::kRayTMin,
                            100.0,
                            bvh_hit,
                            ray::AccelMode::Bvh),
            "BVH equal-distance hit");
    require(linear_hit.shape == expected &&
                bvh_hit.shape == expected,
            "later primitive wins equal-distance tie");
}

void testShapeMutationBoundary() {
    ray::Scene scene;
    scene.addShape(std::make_unique<ray::Sphere>(
        ray::Vec3(10.0, 0.0, 5.0),
        1.0,
        ray::Material(ray::Color(1.0, 0.0, 0.0))));
    scene.buildAcceleration();
    require(scene.accelerationReady(),
            "initial acceleration state");

    scene.addShape(std::make_unique<ray::Sphere>(
        ray::Vec3(0.0, 0.0, 5.0),
        1.0,
        ray::Material(ray::Color(0.0, 1.0, 0.0))));
    require(scene.shapeCount() == 2,
            "shape mutation uses the Scene boundary");
    require(!scene.accelerationReady(),
            "shape addition invalidates acceleration");

    const ray::Shape* expected = &scene.shapeAt(1);
    const ray::Ray forward(
        ray::Vec3(), ray::Vec3(0.0, 0.0, 1.0));
    ray::HitRecord fallback_hit;
    require(scene.intersect(forward,
                            ray::kRayTMin,
                            100.0,
                            fallback_hit,
                            ray::AccelMode::Bvh) &&
                fallback_hit.shape == expected,
            "invalidated BVH falls back to current geometry");

    scene.buildAcceleration();
    ray::HitRecord rebuilt_hit;
    require(scene.intersect(forward,
                            ray::kRayTMin,
                            100.0,
                            rebuilt_hit,
                            ray::AccelMode::Bvh) &&
                rebuilt_hit.shape == expected,
            "rebuilt BVH indexes current geometry");
}

ray::Scene makeDenseScene() {
    ray::Scene scene;
    scene.width = 160;
    scene.height = 90;
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
    scene.addShape(std::make_unique<ray::Plane>(
        ray::Vec3(0.0, -1.0, 0.0),
        ray::Vec3(0.0, 1.0, 0.0),
        ray::Material(ray::Color(0.35, 0.38, 0.42))));

    for (int row = 0; row < 20; ++row) {
        for (int column = 0; column < 20; ++column) {
            const double x = (column - 9.5) * 1.05;
            const double z = 2.0 + row * 1.05;
            const double y = -0.45 + 0.18 * ((row + column) % 4);
            const ray::Color color(
                0.2 + 0.6 * static_cast<double>(column % 5) / 4.0,
                0.2 + 0.6 * static_cast<double>(row % 5) / 4.0,
                0.25 +
                    0.5 *
                        static_cast<double>((row + column) % 5) /
                        4.0);
            scene.addShape(std::make_unique<ray::Sphere>(
                ray::Vec3(x, y, z),
                0.42,
                ray::Material(color)));
        }
    }
    scene.buildAcceleration();
    return scene;
}

void testDenseRender() {
    const ray::Scene scene = makeDenseScene();
    ray::RenderSettings linear_settings;
    linear_settings.accelMode = ray::AccelMode::Linear;
    ray::RenderSettings bvh_settings;
    bvh_settings.accelMode = ray::AccelMode::Bvh;
    ray::RenderStats linear_stats;
    ray::RenderStats bvh_stats;

    const ray::Image linear =
        ray::renderScene(scene, linear_settings, &linear_stats);
    const ray::Image bvh =
        ray::renderScene(scene, bvh_settings, &bvh_stats);

    require(linear.pixels == bvh.pixels,
            "linear and BVH pixels");
    require(ray::checksumHex(linear) == ray::checksumHex(bvh),
            "linear and BVH checksum");
    require(bvh_stats.primitiveTests * 4 <
                linear_stats.primitiveTests,
            "BVH primitive test reduction");
}

}  // namespace

int main() {
    try {
        testEmptyScene();
        testSingleSphere();
        testPlaneOnly();
        testCylinder();
        testEqualDistanceTie();
        testShapeMutationBoundary();
        testDenseRender();
    } catch (const std::exception& error) {
        std::cerr << "acceleration regression failed: "
                  << error.what() << '\n';
        return 1;
    }
    std::cout << "acceleration regression checks passed\n";
    return 0;
}
