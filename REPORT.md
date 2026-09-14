# REPORT

## 간단한 소개

`miniRT`(ray-scene-tracer)는 `.rt` 장면 파일 하나를 입력받아 CPU 멀티스레드 광선 추적으로 PPM 이미지를 렌더링하는 오프라인 렌더러다. 구/평면/실린더 세 도형과 BVH 가속 구조, Lambertian 디퓨즈 + 반사하는 Metal 두 재질, 타일 단위 병렬 렌더링을 지원한다.

렌더링 중간에 실패해도 기존 출력 파일을 훼손하지 않는 원자적 파일 쓰기까지 갖춰, "렌더링 수학"과 "시스템 프로그래밍(스레드/파일 안전성)"이 절반씩 섞인 게 이 프로젝트의 구조적 특징이다.

## 요청/데이터 흐름

```
[CLI 인자] --parseUnsigned 등--> [CliOptions] --parser.cpp--> [Scene]
                                                                  |
                                                     scene.buildAcceleration()
                                                                  |
                                                        [BVH(accel.cpp)]
                                                                  |
                                                        renderScene(scene, settings)
                                                                  |
                                          타일 단위로 나눠 워커 스레드에 분배(renderer.cpp)
                                                                  |
                                              각 스레드가 담당 타일의 픽셀마다 반복:
                                                                  |
                                                  카메라 광선 생성(camera.cpp)
                                                                  |
                              광선-도형 교차(geometry.cpp, BVH로 후보 축소 -> 가장 가까운 HitRecord 선택)
                                                                  |
                              셰이딩(shading.cpp) — Diffuse/Metal, 그림자 검사, Metal은 재귀 반사
                                                                  |
                                    [Image 픽셀 버퍼] --output.cpp(원자적 쓰기)--> [output.ppm]
```

## 메인 소스

1. **`src/main.cpp`**, **`src/parser.cpp`** — `std::isdigit`으로 먼저 문자 집합을 검증한 뒤에야 `stoull`을 호출한다 — 안 그러면 `"-5"` 같은 입력이 매우 큰 부호 없는 값으로 둔갑한다(`[INTV:EDGE]`). 각 옵션마다 `seen_*` 플래그로 중복 지정을 거부하고(`[INTV:EDGE]`), `.rt` 파일은 재귀 하향이 아닌 단순 라인 기반으로 파싱한다(`[INTV:ARCH]`). `std::stod`는 `"12abc"` 같은 입력도 앞부분만 파싱해 성공 처리하므로 별도 검증이 필요하고(`[INTV:EDGE]`), 마지막 인자(재질)가 선택적인지는 토큰 개수로 판별한다(`[INTV:ARCH]`). 방향 벡터가 `(0,0,0)`으로 들어오면 별도로 거부해야 한다(`[INTV:EDGE]`).
2. **`include/ray/scene.hpp`**, **`src/scene.cpp`** — `Scene`이 `unique_ptr<Shape>`들을 소유해 복사 금지가 자연스럽게 따라온다(`[INTV:ARCH]`). 도형을 "유한한 경계 상자가 있는 것(BVH에 편입)"과 "없는 것(무한 평면 등, 매번 직접 검사)"으로 나누고(`[INTV:ARCH]`), BVH는 빌드 시점 스냅샷이라 도형이 추가되면 재빌드해야 한다(`[INTV:EDGE]`). BVH 순회는 재귀 함수 호출 대신 명시적 스택(vector)으로 하고(`[INTV:PERF]`), 가지치기(entry가 지금까지 찾은 최단 거리보다 멀면 스킵)와 더 가까운 자식을 나중에 push하는 순서 최적화가 있다 (`[INTV:PERF]`, 2곳).
3. **`include/ray/accel.hpp`**, **`src/accel.cpp`** — Linear(기준선, 정답 검증/디버깅용)와 Bvh 두 가속 구조가 있고(`[INTV:ARCH]`), AABB는 도형을 감싸는 가장 작은 직육면체다(`[INTV:ARCH]`). 포인터 대신 `vector<BvhNode>` 안의 인덱스(left/right)로 자식을 가리켜 캐시 지역성을 높이고(`[INTV:ARCH][INTV:PERF]`), 슬랩 방법(slab method)으로 광선-AABB 교차를 판정한다(`[INTV:ARCH]`, 이 프로젝트 에서 수학적으로 가장 밀도 높은 지점). 중앙값 분할(median split) 기반 재귀적 구성이고(`[INTV:ARCH]`), 도형 중심점이 가장 넓게 퍼진 축으로 나누며 (`[INTV:PERF]`), `stable_sort` + tie-break로 부동소수점 중심점이 같을 때도 결정적 순서를 보장한다(`[INTV:EDGE]`). `buildNode`의 반환값(루트 인덱스)을 그냥 버리면 안 된다(`[TRAP]`).
4. **`include/ray/geometry.hpp`**, **`src/geometry.cpp`** — 모든 도형이 상속하는 추상 기반 클래스로 소멸자를 반드시 virtual로 선언해야 하고(`[INTV:EDGE]`), `std::optional<Aabb>`로 무한 평면처럼 유한 경계 상자가 없는 도형을 표현한다 (`[INTV:ARCH]`). 구(이차방정식 근의 공식), 평면(내적 하나로 t를 구함), 실린더(옆면+위/아래 뚜껑 3개 표면 중 최단 거리) 순으로 난이도가 올라간다 (`[INTV:ARCH]`, 각 공식의 유도 과정이 함수 바로 위에 있다). 회전된 실린더의 AABB는 각 축의 "옆면이 만드는 폭"으로 계산하고(`[INTV:EDGE]`), `std::nextafter`로 경계를 1ULP만큼 바깥으로 밀어낸다(`[INTV:EDGE]`).
5. **`src/camera.cpp`**, **`include/ray/camera.hpp`** — 외적으로 서로 수직인 right/up을 만들어 정규직교 기저를 구성하고(`[INTV:ARCH]`), 핀홀 카메라 모델은 FOV 절반의 tan 값으로 단위 거리 앞 뷰포트 크기를 정한다(`[INTV:ARCH]`). 픽셀 좌표(왼쪽 위 원점)를 `[-0.5, 0.5]` 뷰포트 좌표로 바꾸고(`[INTV:EDGE]`), 미리 계산된 `CameraFrame`을 받는 오버로드와 매번 내부에서 계산하는 오버로드 쌍이 있다(`[INTV:PERF]`).
6. **`src/shading.cpp`** — 그림자 검사는 "충돌 여부"만 필요해 HitRecord 세부 정보를 무시하는 더 값싼 경로를 쓰고(`[INTV:ARCH]`), Diffuse는 Lambertian + 환경광 모델이다(`[INTV:ARCH]`, 법선·광원 내적이 0 이하면 광원이 뒤쪽에 있다는 `[INTV:PERF]` 최적화 포함). Metal은 반사 방향(`d' = d - 2*(d·n)*n`)으로 새 광선을 만들어 재귀 호출한다(`[INTV:ARCH][INTV:EDGE]`, 재귀 깊이 제한과 함께 읽을 것).
7. **`include/ray/renderer.hpp`**, **`src/renderer.cpp`** — 이미지를 `kTileSize x kTileSize` 단위로 나눠 워커 스레드들이 나눠 처리하고 (`[INTV:ARCH]`), `width*height*3`을 그냥 계산하면 큰 해상도에서 오버플로가 날 수 있다(`[INTV:EDGE]`). `hardware_concurrency()`가 0을 반환할 수도 있어 폴백이 필요하고(`[INTV:EDGE]`), `alignas(64)`로 구조체를 캐시 라인에 맞춰 false sharing을 줄인다(`[INTV:PERF]`). `std::atomic<std::size_t>`의 `fetch_add(1, relaxed)`로 뮤텍스 없이 다음 타일을 나눠 갖고(`[INTV:ARCH]`), 여러 스레드가 잠금 없이 같은 `image.pixels`에 쓰지만 타일이 겹치지 않아 안전하다 (`[INTV:EDGE]`). 스레드 안에서 던진 예외는 만든 쪽으로 자동 전파되지 않으므로 (`[INTV:EDGE]`) `std::current_exception()`으로 캡처해뒀다가(`[INTV:EDGE]`) 모든 스레드가 join된 뒤 메인 스레드에서 다시 던진다(`[INTV:ARCH]`) — 이 프로젝트가 시스템 프로그래밍 관점에서 가장 밀도 높은 파일이다(`tests/render_tests.cpp`가 이 지점을 검증한다).
8. **`src/output.cpp`** — RAII 스코프 가드로 "명시적으로 커밋 표시하지 않으면 임시 파일이 자동 정리"되고(`[INTV:ARCH][INTV:EDGE]`), 원자적 파일 발행 패턴으로 최종 경로에 직접 쓰지 않고 임시 파일에 쓴 뒤 `rename`으로 교체한다 (`[INTV:ARCH][INTV:EDGE]`). `commit()`은 반드시 rename 성공 "이후"에 호출해야 하고(`[TRAP]`), POSIX의 `rename()`은 목적지가 있어도 원자적으로 덮어쓰지만 Windows는 그렇지 않은 플랫폼 차이가 있다(`[INTV:TRADE_OFF]`). 이 패턴을 빠뜨리면 렌더링 도중 실패했을 때 기존 출력 파일이 반쯤 깨진 채 남는다 (`tests/output_tests.cpp`가 이걸 검증).

## 부가 소스

- **`include/ray/math.hpp`**, **`src/math.cpp`** — `Color`를 `Vec3`의 별칭으로 재사용하고(`[INTV:ARCH]`), 교차 t값의 최소 허용치로 반사/그림자 레이의 자기교차(self-intersection)를 막는다(`[INTV:EDGE]`). `std::hypot`은 중간 제곱 항의 오버플로를 피하고(`[INTV:EDGE]`), 길이가 극히 작은 벡터를 정규화하면 NaN이 나오는 경계를 막는다(`[INTV:EDGE]`). 렌더링 수학의 기반 도구 파일이라 별도 알고리즘적 설계 판단보다는 부동소수점 안전 처리가 대부분이다.
