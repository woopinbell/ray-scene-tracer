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

// [INTV:ARCH] [INTV:EDGE] RAII 스코프 가드: 임시 파일을 "커밋됐다"고 명시적으로 표시하지 않는 한,
// 이 객체가 소멸될 때(정상 반환이든 예외든) 자동으로 지운다 — writePpm이 중간에 예외를 던져도 완성
// 못 한 임시 파일이 디스크에 고아로 남지 않는다.
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

// [INTV:EDGE] 타임스탬프 + 원자적 카운터(sequence)를 함께 섞어 경로를 만든다 — 여러 렌더 프로세스가
// 동시에 같은 output 경로에 쓰더라도(같은 벤치마크를 병렬로 여러 번 돌리는 경우 등) 임시 파일 이름이
// 서로 충돌하지 않게 한다.
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

// [INTV:TRADE_OFF] POSIX의 rename()은 목적지가 이미 존재해도 원자적으로 덮어쓰지만, Windows의
// MoveFileEx는 MOVEFILE_REPLACE_EXISTING 플래그 없이는 기존 파일이 있으면 실패한다 — 같은 "원자적
// 파일 교체"를 표현하는 두 플랫폼의 API가 근본적으로 다른 기본 동작을 가진 이식성 이슈.
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

// [INTV:ARCH] [INTV:EDGE] 원자적 파일 발행(atomic publish) 패턴: 최종 경로(path)에 직접 쓰지 않고
// 임시 경로에 전부 쓴 뒤 마지막에 한 번에 rename한다 — 렌더링 도중 프로세스가 죽거나 디스크가 꽉 차도,
// path에는 "이전 완성본이 그대로 있거나, 이번 렌더링이 완전히 끝난 새 파일"만 나타나고 "절반만 써진
// 손상된 이미지"가 나타나는 순간이 없다.
// - [TRAP] output.exceptions(badbit|failbit) 설정 없이 재구현하면, 디스크 풀 등으로 쓰기가 실패해도
//   ofstream은 기본적으로 예외를 던지지 않고 조용히 실패 상태 플래그만 세운다 — 그 상태에서 계속
//   진행하면 손상된 파일을 마치 성공한 것처럼 rename까지 해버릴 수 있다.
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
    // [INTV:TRAP] commit()을 rename 성공 "이후"에 호출해야 한다 — rename 실패로 예외가 던져지는
    // 경로에서는 commit이 실행되지 않아 TemporaryOutput 소멸자가 여전히 임시 파일을 정리해준다.
    // 순서를 바꿔 rename 전에 commit하면, rename이 실패했을 때도 임시 파일이 삭제되지 않고 남는다.
    temporary.commit();
}

// [INTV:ARCH] FNV-1a 해시(64비트): 매직 넘버 14695981039346656037/1099511628211은 FNV-1a 표준이
// 정의한 오프셋 베이시스/소수(prime) 상수 — 암호학적 해시가 아니라 렌더링 결과 회귀 테스트에서
// "이전 렌더와 픽셀이 완전히 같은가"를 빠르게 비교하기 위한 용도로 적합한 가벼운 체크섬.
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
