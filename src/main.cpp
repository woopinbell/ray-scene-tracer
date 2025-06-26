#include "ray.hpp"

#include <algorithm>
#include <cctype>
#include <exception>
#include <iostream>
#include <limits>
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
        << " [--checksum]"
        << " [--accel linear|bvh]"
        << " [--threads N|auto]"
        << " [--max-depth 0..32]\n";
}

bool parseUnsigned(const std::string& token,
                   unsigned long long maximum,
                   unsigned long long& value) {
    if (token.empty() ||
        !std::all_of(
            token.begin(),
            token.end(),
            [](unsigned char character) {
                return std::isdigit(character) != 0;
            })) {
        return false;
    }
    try {
        std::size_t parsed = 0;
        const unsigned long long candidate =
            std::stoull(token, &parsed);
        if (parsed != token.size() || candidate > maximum) {
            return false;
        }
        value = candidate;
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool parseCli(int argc, char** argv, CliOptions& options) {
    if (argc < 3) {
        return false;
    }
    options.scenePath = argv[1];
    options.outputPath = argv[2];

    bool seen_checksum = false;
    bool seen_accel = false;
    bool seen_threads = false;
    bool seen_max_depth = false;

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

        if (option == "--accel") {
            if (seen_accel || index + 1 >= argc) {
                return false;
            }
            seen_accel = true;
            const std::string value = argv[++index];
            if (value == "linear") {
                options.renderSettings.accelMode =
                    ray::AccelMode::Linear;
            } else if (value == "bvh") {
                options.renderSettings.accelMode =
                    ray::AccelMode::Bvh;
            } else {
                return false;
            }
            continue;
        }

        if (option == "--threads") {
            if (seen_threads || index + 1 >= argc) {
                return false;
            }
            seen_threads = true;
            const std::string value = argv[++index];
            if (value == "auto") {
                options.renderSettings.threadCount = 0;
                continue;
            }
            unsigned long long parsed = 0;
            if (!parseUnsigned(
                    value,
                    std::numeric_limits<unsigned int>::max(),
                    parsed) ||
                parsed == 0) {
                return false;
            }
            options.renderSettings.threadCount =
                static_cast<unsigned int>(parsed);
            continue;
        }

        if (option == "--max-depth") {
            if (seen_max_depth || index + 1 >= argc) {
                return false;
            }
            seen_max_depth = true;
            unsigned long long parsed = 0;
            if (!parseUnsigned(argv[++index], 32, parsed)) {
                return false;
            }
            options.renderSettings.maxDepth =
                static_cast<int>(parsed);
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
