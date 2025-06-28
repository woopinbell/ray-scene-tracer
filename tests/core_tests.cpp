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

void testBounds() {
    const ray::Aabb box(ray::Vec3(-1.0, -1.0, -1.0),
                        ray::Vec3(1.0, 1.0, 1.0));
    double entry = 0.0;
    require(box.intersect(ray::Ray(ray::Vec3(0.0, 0.0, -3.0),
                                   ray::Vec3(0.0, 0.0, 1.0)),
                          0.0,
                          100.0,
                          &entry) &&
                nearlyEqual(entry, 2.0),
            "AABB forward hit");
    require(box.intersect(ray::Ray(ray::Vec3(0.0, 0.0, 3.0),
                                   ray::Vec3(0.0, 0.0, -1.0)),
                          0.0,
                          100.0),
            "AABB negative direction");
    require(box.intersect(ray::Ray(ray::Vec3(1.0, 0.0, -3.0),
                                   ray::Vec3(0.0, 0.0, 1.0)),
                          0.0,
                          100.0),
            "AABB boundary hit");
    require(!box.intersect(ray::Ray(ray::Vec3(2.0, 0.0, -3.0),
                                    ray::Vec3(0.0, 0.0, 1.0)),
                           0.0,
                           100.0),
            "AABB parallel miss");

    const ray::Material white(ray::Color(1.0, 1.0, 1.0));
    const ray::Sphere sphere(ray::Vec3(1.0, 2.0, 3.0), 2.0, white);
    const std::optional<ray::Aabb> sphere_bounds = sphere.bounds();
    require(sphere_bounds &&
                sphere_bounds->minimum == ray::Vec3(-1.0, 0.0, 1.0) &&
                sphere_bounds->maximum == ray::Vec3(3.0, 4.0, 5.0),
            "sphere bounds");

    const ray::Plane plane(ray::Vec3(),
                           ray::Vec3(0.0, 1.0, 0.0),
                           white);
    require(!plane.bounds(), "plane remains unbounded");

    const ray::Cylinder cylinder(ray::Vec3(),
                                 ray::Vec3(1.0, 1.0, 0.0),
                                 1.0,
                                 4.0,
                                 white);
    const std::optional<ray::Aabb> cylinder_bounds = cylinder.bounds();
    const double exact_xy = 3.0 / std::sqrt(2.0);
    require(cylinder_bounds &&
                cylinder_bounds->maximum.x >= exact_xy &&
                cylinder_bounds->maximum.y >= exact_xy &&
                cylinder_bounds->maximum.x < exact_xy + 1.0e-3 &&
                cylinder_bounds->maximum.z >= 1.0 &&
                cylinder_bounds->maximum.z < 1.0 + 1.0e-3,
            "arbitrary-axis cylinder bounds");
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
    require(ray::checksumHex(image) == "0fde7b4d509f1daf", "checksum golden");
}

void testInvalidImageStorage() {
    ray::Image image(2, 1);
    image.pixels.pop_back();

    bool checksum_rejected = false;
    try {
        (void)ray::checksumHex(image);
    } catch (const std::invalid_argument&) {
        checksum_rejected = true;
    }
    require(checksum_rejected, "checksum rejects short pixel storage");

    const std::string path = "ray-core-test-preserved-output.ppm";
    {
        std::ofstream output(path);
        output << "preserve me\n";
    }
    bool output_rejected = false;
    try {
        ray::writePpm(image, path);
    } catch (const std::invalid_argument&) {
        output_rejected = true;
    }
    const std::string preserved = readFile(path);
    std::remove(path.c_str());
    require(output_rejected, "PPM writer rejects short pixel storage");
    require(preserved == "preserve me\n",
            "invalid image does not truncate existing output");

    image.pixels.push_back(0);
    image.pixels.push_back(0);
    bool oversized_rejected = false;
    try {
        image.validate();
    } catch (const std::invalid_argument&) {
        oversized_rejected = true;
    }
    require(oversized_rejected, "image rejects excess pixel storage");
}

void testCameraFrameReuse() {
    const ray::Camera camera(ray::Vec3(1.0, 2.0, -3.0),
                             ray::Vec3(-0.1, 0.25, 1.0),
                             57.0);
    const ray::CameraFrame frame = ray::buildCameraFrame(camera, 640, 360);
    const ray::Ray rebuilt =
        ray::makeCameraRay(camera, 640, 360, 217.5, 103.5);
    const ray::Ray reused =
        ray::makeCameraRay(camera, frame, 640, 360, 217.5, 103.5);
    require(rebuilt.origin == reused.origin &&
                rebuilt.direction == reused.direction,
            "cached camera frame");
}

void testImageDimensions() {
    const ray::Image image(2, 3);
    require(image.width == 2 &&
                image.height == 3 &&
                image.pixels.size() == 18,
            "image storage size");

    bool zero_rejected = false;
    try {
        (void)ray::Image(0, 1);
    } catch (const std::invalid_argument&) {
        zero_rejected = true;
    }
    require(zero_rejected,
            "zero image dimension rejection");

    bool negative_rejected = false;
    try {
        (void)ray::Image(-1, 1);
    } catch (const std::invalid_argument&) {
        negative_rejected = true;
    }
    require(negative_rejected,
            "negative image dimension rejection");
}

void testRenderGolden() {
    const ray::Scene scene = ray::loadScene(
        std::string(RAY_SOURCE_DIR) + "/scenes/basic.rt");
    const ray::Image image = ray::renderScene(scene);
    require(image.width == 640 && image.height == 360, "render dimensions");
    require(ray::checksumHex(image) == "456dc8d87ebf194f", "render checksum");
}

}  // namespace

int main() {
    try {
        testMath();
        testGeometry();
        testBounds();
        testInvalidFixture();
        testNearZeroDirections();
        testImageDimensions();
        testCameraFrameReuse();
        testOutput();
        testInvalidImageStorage();
        testRenderGolden();
    } catch (const std::exception& error) {
        std::cerr << "core regression failed: " << error.what() << '\n';
        return 1;
    }
    std::cout << "core regression checks passed\n";
    return 0;
}
