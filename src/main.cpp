#include "ray.hpp"

#include <exception>
#include <iostream>
#include <string>

namespace {

struct CliOptions {
    std::string scenePath;
    std::string outputPath;
    bool printChecksum = false;
    ray::RenderSettings renderSettings;
};

void printUsage() {
    std::cerr
        << "usage: ray-scene-tracer <scene.rt> <output.ppm>"
        << " [--checksum]\n";
}

bool parseCli(int argc, char** argv, CliOptions& options) {
    if (argc < 3) {
        return false;
    }
    options.scenePath = argv[1];
    options.outputPath = argv[2];

    bool seen_checksum = false;

    for (int index = 3; index < argc; ++index) {
        const std::string option = argv[index];
        if (option == "--checksum") {
            if (seen_checksum) {
                return false;
            }
            seen_checksum = true;
            options.printChecksum = true;
            continue;
        }
        return false;
    }
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    CliOptions options;
    if (!parseCli(argc, argv, options)) {
        printUsage();
        return 2;
    }

    try {
        const ray::Scene scene =
            ray::loadScene(options.scenePath);
        const ray::Image image =
            ray::renderScene(scene, options.renderSettings);
        ray::writePpm(image, options.outputPath);
        if (options.printChecksum) {
            std::cout << ray::checksumHex(image) << '\n';
        }
    } catch (const std::exception& error) {
        std::cerr << "ray-scene-tracer: "
                  << error.what() << '\n';
        return 1;
    }
    return 0;
}
