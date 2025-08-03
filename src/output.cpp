#include "ray/output.hpp"

#include <cstdint>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace ray {

void writePpm(const Image& image, const std::string& path) {
    image.validate();
    std::ofstream output(path);
    if (!output) {
        throw std::runtime_error("cannot open output file: " + path);
    }
    output << "P3\n" << image.width << ' ' << image.height << "\n255\n";
    for (int y = 0; y < image.height; ++y) {
        for (int x = 0; x < image.width; ++x) {
            const std::size_t base =
                (static_cast<std::size_t>(y) *
                     static_cast<std::size_t>(image.width) +
                 static_cast<std::size_t>(x)) *
                3;
            output << static_cast<int>(image.pixels[base]) << ' '
                   << static_cast<int>(image.pixels[base + 1]) << ' '
                   << static_cast<int>(image.pixels[base + 2]) << '\n';
        }
    }
}

std::string checksumHex(const Image& image) {
    image.validate();
    std::uint64_t hash = 14695981039346656037ULL;
    const auto mix = [&hash](unsigned char value) {
        hash ^= static_cast<std::uint64_t>(value);
        hash *= 1099511628211ULL;
    };
    mix(static_cast<unsigned char>(image.width & 0xff));
    mix(static_cast<unsigned char>((image.width >> 8) & 0xff));
    mix(static_cast<unsigned char>(image.height & 0xff));
    mix(static_cast<unsigned char>((image.height >> 8) & 0xff));
    for (unsigned char value : image.pixels) {
        mix(value);
    }
    std::ostringstream out;
    out << std::hex << std::setfill('0') << std::setw(16) << hash;
    return out.str();
}

}  // namespace ray
