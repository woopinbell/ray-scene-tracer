#include "ray.hpp"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

bool nearlyEqual(double left, double right, double epsilon = 1.0e-9) {
    return std::fabs(left - right) <= epsilon;
}

std::string readFile(const std::string& path) {
    std::ifstream input(path);
    std::ostringstream contents;
    contents << input.rdbuf();
    return contents.str();
}

void testMath() {
    const ray::Vec3 a(1.0, 2.0, 3.0);
    const ray::Vec3 b(-2.0, 0.5, 4.0);
    require(a + b == ray::Vec3(-1.0, 2.5, 7.0), "vector addition");
    require(nearlyEqual(ray::dot(a, b), 11.0), "dot product");
    require(ray::cross(ray::Vec3(1.0, 0.0, 0.0),
                       ray::Vec3(0.0, 1.0, 0.0)) ==
                ray::Vec3(0.0, 0.0, 1.0),
            "cross product");
    require(nearlyEqual(ray::normalize(ray::Vec3(0.0, 3.0, 4.0)).length(), 1.0),
            "normalization");
    require(ray::normalize(ray::Vec3(1.0e308, 0.0, 0.0)) ==
                ray::Vec3(1.0, 0.0, 0.0),
            "large finite vector normalization");
}

void testGeometry() {
    const ray::Material white(ray::Color(1.0, 1.0, 1.0));
    const ray::Ray forward(ray::Vec3(), ray::Vec3(0.0, 0.0, 1.0));
    ray::HitRecord hit;

    const ray::Sphere sphere(ray::Vec3(0.0, 0.0, 5.0), 1.0, white);
    require(sphere.intersect(forward, ray::kRayTMin, 100.0, hit), "sphere hit");
    require(nearlyEqual(hit.t, 4.0), "sphere distance");

    const ray::Plane plane(ray::Vec3(0.0, 0.0, 2.0),
                           ray::Vec3(0.0, 0.0, -1.0),
                           white);
    require(plane.intersect(forward, ray::kRayTMin, 100.0, hit), "plane hit");
    require(nearlyEqual(hit.t, 2.0), "plane distance");

    const ray::Cylinder cylinder(ray::Vec3(0.0, 0.0, 5.0),
                                 ray::Vec3(0.0, 1.0, 0.0),
                                 1.0,
                                 2.0,
                                 white);
    require(cylinder.intersect(forward, ray::kRayTMin, 100.0, hit),
            "cylinder hit");
    require(nearlyEqual(hit.t, 4.0), "cylinder distance");
}

void testInvalidFixture() {
    bool rejected = false;
    try {
        (void)ray::parser::parseSceneFile(
            std::string(RAY_SOURCE_DIR) + "/scenes/invalid.rt");
    } catch (const ray::ParseError& error) {
        rejected = error.line() == 3;
    }
    require(rejected, "invalid scene fixture");
}

void testNearZeroDirections() {
    const std::string prefix =
        "R 8 8\n"
        "A 0.1 255,255,255\n";
    bool camera_rejected = false;
    try {
        (void)ray::parser::parseSceneText(
            prefix +
                "C 0,0,0 0.000001,0,0 60\n",
            "small-camera-direction.rt");
    } catch (const ray::ParseError&) {
        camera_rejected = true;
    }
    require(camera_rejected,
            "near-zero camera direction rejection");

    bool cylinder_rejected = false;
    try {
        (void)ray::parser::parseSceneText(
            prefix +
                "C 0,0,0 0,0,1 60\n"
                "cy 0,0,5 0.000001,0,0 1 2 255,0,0\n",
            "small-cylinder-axis.rt");
    } catch (const ray::ParseError&) {
        cylinder_rejected = true;
    }
    require(cylinder_rejected,
            "near-zero cylinder axis rejection");
}

void testOutput() {
    ray::Image image(2, 1);
    image.pixels = {255, 0, 16, 0, 127, 255};

    const std::string path = "ray-core-test-output.ppm";
    ray::writePpm(image, path);
    const std::string ppm = readFile(path);
    std::remove(path.c_str());

    require(ppm == "P3\n2 1\n255\n255 0 16\n0 127 255\n", "PPM encoding");
}

void testRenderGolden() {
    const ray::Scene scene = ray::loadScene(
        std::string(RAY_SOURCE_DIR) + "/scenes/basic.rt");
    const ray::Image image = ray::renderScene(scene);
    require(image.width == 640 && image.height == 360, "render dimensions");
}

}  // namespace

int main() {
    try {
        testMath();
        testGeometry();
        testInvalidFixture();
        testNearZeroDirections();
        testOutput();
        testRenderGolden();
    } catch (const std::exception& error) {
        std::cerr << "core regression failed: " << error.what() << '\n';
        return 1;
    }
    std::cout << "core regression checks passed\n";
    return 0;
}
