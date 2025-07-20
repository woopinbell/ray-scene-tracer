#include "ray.hpp"

#include <exception>
#include <iostream>
#include <string>

namespace {

void print_usage() {
    std::cerr << "usage: ./ray-scene-tracer <scene.rt> <output.ppm> [--checksum]\n";
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 3 && argc != 4) {
        print_usage();
        return 2;
    }
    const std::string scene_path = argv[1];
    const std::string output_path = argv[2];
    const bool print_checksum = argc == 4 && std::string(argv[3]) == "--checksum";
    if (argc == 4 && !print_checksum) {
        print_usage();
        return 2;
    }
    try {
        const ray::Scene scene = ray::loadScene(scene_path);
        const ray::Image image = ray::renderScene(scene);
        ray::writePpm(image, output_path);
        if (print_checksum) {
            std::cout << ray::checksumHex(image) << '\n';
        }
    } catch (const std::exception& error) {
        std::cerr << "ray-scene-tracer: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
