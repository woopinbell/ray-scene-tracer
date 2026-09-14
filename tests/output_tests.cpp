// [INTV:ARCH] CTest의 output_failure_regression 항목 — PPM 이미지 출력 경로 중에서도 "정상
// 저장"이 아니라 실패/경계 상황에 집중한다: 스트림 쓰기 자체가 실패하는 경우(testStreamFailure),
// 기존 파일을 안전하게 덮어쓰는지(testAtomicReplacement), 그리고 덮어쓰기 도중 실패했을 때
// 원래 있던 목적지를 훼손하지 않는지(testFailedReplacementPreservesDestination).
// [INTV:TRAP] ray::writePpm은 목적지에 바로 쓰지 않고 "임시 파일에 쓴 뒤 원자적으로 rename"하는
// 패턴을 쓰는 것으로 보인다 — testAtomicReplacement/testFailedReplacementPreservesDestination이
// 둘 다 requireNoTemporaryFiles로 "테스트가 끝난 뒤 디렉터리에 임시 파일이 남아있지 않은지"를
// 확인하는 게 그 증거다. 재구현 시 이 패턴을 빠뜨리고 목적지 파일에 직접(중간에 truncate하며)
// 쓰면, 쓰기 도중 실패했을 때 원래 있던 파일이 반쯤 깨진 상태로 남는다 — 이 테스트 파일이 막으려는
// 바로 그 실패 모드다.
#include "ray.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <streambuf>
#include <string>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

class FailingBuffer : public std::streambuf {
protected:
    std::streamsize xsputn(const char*, std::streamsize) override {
        return 0;
    }

    int_type overflow(int_type) override {
        return traits_type::eof();
    }
};

class TestDirectory {
public:
    TestDirectory() {
        const unsigned long long token =
            static_cast<unsigned long long>(
                std::chrono::steady_clock::now()
                    .time_since_epoch()
                    .count());
        path_ = std::filesystem::temp_directory_path() /
                ("ray-output-regression-" + std::to_string(token));
        if (!std::filesystem::create_directory(path_)) {
            throw std::runtime_error("cannot create output test directory");
        }
    }

    ~TestDirectory() {
        std::error_code ignored;
        std::filesystem::remove_all(path_, ignored);
    }

    const std::filesystem::path& path() const {
        return path_;
    }

private:
    std::filesystem::path path_;
};

std::string readFile(const std::filesystem::path& path) {
    std::ifstream input(path);
    std::ostringstream contents;
    contents << input.rdbuf();
    return contents.str();
}

void writeFile(const std::filesystem::path& path,
               const std::string& contents) {
    std::ofstream output(path);
    output << contents;
    if (!output) {
        throw std::runtime_error("cannot prepare output test file");
    }
}

void requireNoTemporaryFiles(const std::filesystem::path& directory,
                             const std::string& prefix) {
    for (const std::filesystem::directory_entry& entry :
         std::filesystem::directory_iterator(directory)) {
        const std::string name = entry.path().filename().string();
        require(name.rfind(prefix + ".tmp.", 0) != 0,
                "temporary output file was not cleaned up");
    }
}

ray::Image sampleImage() {
    ray::Image image(2, 1);
    image.pixels = {255, 0, 16, 0, 127, 255};
    return image;
}

void testStreamFailure() {
    FailingBuffer buffer;
    std::ostream output(&buffer);
    bool rejected = false;
    try {
        ray::writePpm(sampleImage(), output);
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    require(rejected, "PPM serializer reports stream failure");
}

void testAtomicReplacement() {
    TestDirectory directory;
    const std::filesystem::path target = directory.path() / "image.ppm";
    writeFile(target, "old image\n");

    ray::writePpm(sampleImage(), target.string());

    require(readFile(target) ==
                "P3\n2 1\n255\n255 0 16\n0 127 255\n",
            "PPM writer replaces an existing file");
    requireNoTemporaryFiles(directory.path(), "image.ppm");
}

void testFailedReplacementPreservesDestination() {
    TestDirectory directory;
    const std::filesystem::path target =
        directory.path() / "existing-destination";
    std::filesystem::create_directory(target);
    const std::filesystem::path sentinel = target / "keep.txt";
    writeFile(sentinel, "preserve me\n");

    bool rejected = false;
    try {
        ray::writePpm(sampleImage(), target.string());
    } catch (const std::runtime_error&) {
        rejected = true;
    }

    require(rejected, "PPM writer reports replacement failure");
    require(std::filesystem::is_directory(target),
            "failed replacement preserves destination type");
    require(readFile(sentinel) == "preserve me\n",
            "failed replacement preserves destination contents");
    requireNoTemporaryFiles(directory.path(),
                            "existing-destination");
}

}  // namespace

int main() {
    try {
        testStreamFailure();
        testAtomicReplacement();
        testFailedReplacementPreservesDestination();
    } catch (const std::exception& error) {
        std::cerr << "output failure regression failed: "
                  << error.what() << '\n';
        return 1;
    }
    std::cout << "output failure regression checks passed\n";
    return 0;
}
