#include "ray/renderer.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <exception>
#include <limits>
#include <stdexcept>
#include <thread>
#include <vector>

namespace ray {

namespace {

// [INTV:EDGE] width*height*3을 그냥 계산하면 아주 큰 해상도에서 size_t 범위를 넘어 오버플로가 나며
// 조용히 틀린(너무 작은) 값이 나올 수 있다. 곱하기 전에 나눗셈으로 미리 상한을 검사해, 실제 곱셈이
// 일어나기 전에 걸러낸다.
std::size_t pixelStorageSize(int width, int height) {
    if (width <= 0 || height <= 0) {
        throw std::invalid_argument("image dimensions must be positive");
    }
    const std::size_t safe_width = static_cast<std::size_t>(width);
    const std::size_t safe_height = static_cast<std::size_t>(height);
    const std::size_t limit = std::numeric_limits<std::size_t>::max();
    if (safe_width > limit / safe_height ||
        safe_width * safe_height > limit / 3) {
        throw std::overflow_error("image dimensions are too large");
    }
    return safe_width * safe_height * 3;
}

}  // namespace

RenderSettings::RenderSettings()
    : samplesPerPixel(1),
      maxDepth(4),
      tMin(kRayTMin),
      tMax(std::numeric_limits<double>::infinity()),
      accelMode(AccelMode::Bvh),
      threadCount(0) {}

Image::Image() : width(0), height(0), pixels() {}

Image::Image(int width_value, int height_value)
    : width(width_value),
      height(height_value),
      pixels(pixelStorageSize(width_value, height_value), 0) {}

void Image::validate() const {
    const std::size_t expected = pixelStorageSize(width, height);
    if (pixels.size() != expected) {
        throw std::invalid_argument(
            "image pixel storage does not match its dimensions");
    }
}

// [INTV:ARCH] 타일 기반 병렬 렌더링: 이미지를 kTileSize x kTileSize 픽셀 단위로 나눠 워커 스레드들이
// 원자적 카운터(next_tile)로 하나씩 가져가는 락 프리 작업 큐 모델.
// - [FLOW] 1. 타일 개수/워커 수 계산 -> 2. 워커별 통계 슬롯(캐시라인 정렬) 준비 -> 3. 각 워커가
//   fetch_add로 타일을 하나씩 뽑아 픽셀을 렌더링 -> 4. 워커 예외는 저장해뒀다가 join 후 메인 스레드에서
//   재던짐 -> 5. 워커별 통계를 합산
// - [TRAP] 픽셀 한 줄씩 정적으로 나눠 스레드에 고정 배분하면, 도형이 몰린 영역(더 많은 교차 검사)을
//   맡은 스레드만 오래 걸리고 나머지는 놀게 되는 부하 불균형이 생긴다. 타일 + 공유 카운터 방식이
//   자연스러운 동적 부하 분산(work stealing에 가까운 효과)을 준다.
Image renderScene(const Scene& scene,
                  const RenderSettings& settings,
                  RenderStats* stats) {
    constexpr int kTileSize = 16;
    const auto started = std::chrono::steady_clock::now();
    Image image(scene.width, scene.height);
    const CameraFrame camera_frame =
        buildCameraFrame(scene.camera, scene.width, scene.height);

    const std::size_t tiles_x =
        (static_cast<std::size_t>(scene.width) + kTileSize - 1) /
        kTileSize;
    const std::size_t tiles_y =
        (static_cast<std::size_t>(scene.height) + kTileSize - 1) /
        kTileSize;
    const std::size_t tile_count = tiles_x * tiles_y;
    unsigned int worker_count = settings.threadCount;
    if (worker_count == 0) {
        // [INTV:EDGE] hardware_concurrency()는 0을 반환할 수도 있는(정보를 못 얻는 환경) 함수라,
        // 그 경우 최소 1 스레드는 보장해야 한다.
        worker_count = std::thread::hardware_concurrency();
        if (worker_count == 0) {
            worker_count = 1;
        }
    }
    worker_count = static_cast<unsigned int>(
        std::min<std::size_t>(worker_count, tile_count));

    // [INTV:PERF] alignas(64): 이 구조체를 캐시 라인 크기(보통 64바이트)에 맞춰 정렬 — 여러 스레드가
    // worker_stats의 서로 다른 원소를 동시에 계속 갱신하는데, 정렬 없이 원소들이 같은 캐시 라인에
    // 걸쳐 있으면 서로 무관한 스레드끼리도 캐시 라인을 주고받으며 성능이 떨어지는 "false sharing"이
    // 생긴다.
    // - [TRAP] alignas 없이 std::vector<RenderStats>를 그대로 쓰면, 논리적으로는 각 스레드가 자기
    //   원소만 쓰는데도 실제 하드웨어 캐시 라인 단위에서는 남의 원소와 같은 라인을 공유해 캐시가
    //   계속 무효화되는 성능 버그가 생긴다(정확성 버그가 아니라 눈에 안 보이는 성능 버그라 발견이 어렵다).
    struct alignas(64) WorkerStats {
        RenderStats values;
    };
    std::vector<WorkerStats> worker_stats(worker_count);
    // [INTV:ARCH] std::atomic<std::size_t>: 여러 스레드가 뮤텍스 없이 fetch_add로 겹치지 않는 타일
    // 번호를 하나씩 뽑아가는 락 프리 작업 큐.
    std::atomic<std::size_t> next_tile{0};
    std::vector<std::thread> workers;
    // [INTV:EDGE] 스레드 안에서 던진 예외는 그 스레드를 만든 쪽(메인 스레드)으로 자동 전파되지 않는다.
    // std::exception_ptr로 워커별 예외를 캡처해뒀다가, 모든 스레드가 끝난 뒤 메인 스레드에서
    // rethrow_exception으로 다시 던져 정상적인 예외 처리 흐름에 합류시킨다.
    std::vector<std::exception_ptr> worker_errors(worker_count);
    workers.reserve(worker_count);

    // [INTV:ARCH] [INTV:EDGE] RAII 가드: 소멸자에서만 "남은 작업을 취소하고 모든 스레드를 join한다"는
    // 정리 동작을 한다. 스코프를 벗어날 때(정상 반환이든 예외든) 자동으로 실행되므로, join을 깜빡하거나
    // 예외로 건너뛰어 스레드가 detach된 채 남는 상황(소멸 시 std::terminate 유발)을 막는다.
    // - [TRAP] try/finally가 없는 C++에서 join()을 함수 끝에 직접 써두면, 중간에 예외가 던져질 때
    //   그 join 호출을 건너뛰어 std::thread 소멸자가 "아직 joinable한 스레드"를 만나 즉시
    //   std::terminate를 호출한다. RAII 가드가 이 경로까지 포함해 보장하는 것이 핵심.
    struct ThreadJoiner {
        std::vector<std::thread>& workers;
        std::atomic<std::size_t>& nextTile;
        std::size_t tileCount;

        ~ThreadJoiner() {
            nextTile.store(tileCount, std::memory_order_relaxed);
            for (std::thread& worker : workers) {
                if (worker.joinable()) {
                    worker.join();
                }
            }
        }
    };
    const ThreadJoiner thread_joiner{
        workers,
        next_tile,
        tile_count
    };

    for (unsigned int worker = 0; worker < worker_count; ++worker) {
        // [INTV:EDGE] [&, worker]: worker 이외의 바깥 변수는 참조로, worker만 값으로 캡처한다.
        // - [TRAP] worker까지 참조로 캡처하면([&]만 쓰면) 모든 스레드가 같은 루프 변수 worker를
        //   공유하게 되어(반복마다 값이 바뀌고 루프가 끝나면 범위를 벗어남) 각 스레드가 자기 몫의
        //   worker_stats/worker_errors 인덱스를 잘못 참조하는 데이터 경쟁이 생긴다. 루프 변수를
        //   스레드에 넘길 땐 항상 값으로 스냅샷을 떠야 한다.
        workers.emplace_back([&, worker]() {
            try {
                RenderStats& local = worker_stats[worker].values;
                for (;;) {
                    // [INTV:ARCH] fetch_add(1, relaxed): "읽고 증가시킨 뒤 증가 전 값을 반환"을
                    // 원자적으로 수행 — 겹치지 않는 타일 번호를 스레드들이 나눠 갖는 락 프리 분배.
                    // memory_order_relaxed로 충분한 이유: 이 카운터는 순서 보장이 필요한 다른 메모리
                    // 접근과 동기화될 필요 없이, 카운터 자체의 원자적 증가만 보장되면 되는 단순 카운터다.
                    const std::size_t tile =
                        next_tile.fetch_add(1, std::memory_order_relaxed);
                    if (tile >= tile_count) {
                        break;
                    }
                    const int start_x =
                        static_cast<int>((tile % tiles_x) * kTileSize);
                    const int start_y =
                        static_cast<int>((tile / tiles_x) * kTileSize);
                    const int end_x =
                        std::min(start_x + kTileSize, scene.width);
                    const int end_y =
                        std::min(start_y + kTileSize, scene.height);

                    for (int y = start_y; y < end_y; ++y) {
                        for (int x = start_x; x < end_x; ++x) {
                            const Ray ray =
                                makeCameraRay(scene.camera,
                                              camera_frame,
                                              scene.width,
                                              scene.height,
                                              x + 0.5,
                                              y + 0.5);
                            ++local.primaryRays;
                            const Color color =
                                traceRay(scene,
                                         ray,
                                         settings.maxDepth,
                                         settings.accelMode,
                                         &local);
                            const Color clamped = clampColor(color);
                            // [INTV:EDGE] 여러 스레드가 잠금 없이 같은 image.pixels 벡터에 동시에
                            // 쓰지만, 타일 분배 덕분에 서로 다른 스레드는 항상 서로 다른 (x, y) 픽셀만
                            // 건드려 실제로 겹치는 메모리 접근이 없다 — 락 없이도 데이터 경쟁이 안
                            // 되는 이유는 "쓰기 대상이 스레드마다 서로소(disjoint)"이기 때문이다.
                            const std::size_t offset =
                                (static_cast<std::size_t>(y) *
                                     static_cast<std::size_t>(scene.width) +
                                 static_cast<std::size_t>(x)) *
                                3;
                            image.pixels[offset] =
                                static_cast<unsigned char>(
                                    std::lround(clamped.x * 255.0));
                            image.pixels[offset + 1] =
                                static_cast<unsigned char>(
                                    std::lround(clamped.y * 255.0));
                            image.pixels[offset + 2] =
                                static_cast<unsigned char>(
                                    std::lround(clamped.z * 255.0));
                        }
                    }
                }
            } catch (...) {
                // [INTV:EDGE] 지금 당장 처리하지 않고 std::current_exception()으로 캡처해 저장한 뒤,
                // next_tile을 타일 총수까지 밀어 다른 워커들도 남은 타일 없이 곧 루프를 빠져나가게
                // 한다 — 한 워커의 예외가 전체 렌더링을 신속히 정지시키는 조기 종료(fail-fast) 신호.
                worker_errors[worker] = std::current_exception();
                next_tile.store(tile_count, std::memory_order_relaxed);
            }
        });
    }
    for (std::thread& worker : workers) {
        worker.join();
    }
    // [INTV:ARCH] 모든 스레드가 끝난 뒤(join 완료 후)에야 저장해둔 예외를 메인 스레드에서 다시 던진다
    // — 호출부는 멀티스레드라는 구현 세부사항을 몰라도 보통의 try/catch로 렌더링 실패를 잡을 수 있다.
    for (const std::exception_ptr& error : worker_errors) {
        if (error) {
            std::rethrow_exception(error);
        }
    }

    if (stats) {
        *stats = RenderStats();
        for (const WorkerStats& worker : worker_stats) {
            stats->primaryRays += worker.values.primaryRays;
            stats->secondaryRays += worker.values.secondaryRays;
            stats->shadowRays += worker.values.shadowRays;
            stats->primitiveTests += worker.values.primitiveTests;
            stats->aabbTests += worker.values.aabbTests;
        }
        const auto finished = std::chrono::steady_clock::now();
        stats->renderMilliseconds =
            std::chrono::duration<double, std::milli>(finished - started).count();
    }
    return image;
}

}  // namespace ray
