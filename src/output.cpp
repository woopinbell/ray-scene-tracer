#include "ray/output.hpp"

#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <utility>

#ifdef _WIN32
#include <windows.h>
#endif

namespace ray {

namespace {

class TemporaryOutput {
public:
    explicit TemporaryOutput(std::string path)
        : path_(std::move(path)), committed_(false) {}

    ~TemporaryOutput() {
        if (!committed_) {
            (void)std::remove(path_.c_str());
        }
    }

    const std::string& path() const {
        return path_;
    }

    void commit() {
        committed_ = true;
    }

private:
    std::string path_;
    bool committed_;
};

std::string temporaryPathFor(const std::string& path) {
    static std::atomic<unsigned long long> sequence{0};
    const unsigned long long stamp =
        static_cast<unsigned long long>(
            std::chrono::steady_clock::now()
                .time_since_epoch()
                .count());
    const unsigned long long number =
        sequence.fetch_add(1, std::memory_order_relaxed);
    return path + ".tmp." + std::to_string(stamp) + "." +
           std::to_string(number);
}

bool replaceFile(const std::string& source,
                 const std::string& destination,
                 std::string& reason) {
#ifdef _WIN32
    if (MoveFileExA(source.c_str(),
                    destination.c_str(),
                    MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        return true;
    }
    reason = "system error " + std::to_string(GetLastError());
    return false;
#else
    if (std::rename(source.c_str(), destination.c_str()) == 0) {
        return true;
    }
    reason = std::strerror(errno);
    return false;
#endif
}

}  // namespace

void writePpm(const Image& image, std::ostream& output) {
    image.validate();
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
    if (!output) {
        throw std::runtime_error("cannot write PPM output stream");
    }
}

void writePpm(const Image& image, const std::string& path) {
    image.validate();
    TemporaryOutput temporary(temporaryPathFor(path));
    {
        std::ofstream output(temporary.path(),
                             std::ios::out | std::ios::trunc);
        if (!output) {
            throw std::runtime_error(
                "cannot open temporary output file for: " + path);
        }
        output.exceptions(std::ios::badbit | std::ios::failbit);
        writePpm(image, output);
        output.flush();
        output.close();
    }

    std::string reason;
    if (!replaceFile(temporary.path(), path, reason)) {
        throw std::runtime_error(
            "cannot replace output file " + path + ": " + reason);
    }
    temporary.commit();
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
