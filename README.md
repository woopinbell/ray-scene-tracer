# Ray scene tracer

![Language](https://img.shields.io/badge/language-C%2B%2B17-blue?logo=cplusplus&logoColor=white)
![Build](https://img.shields.io/badge/build-CMake%20%7C%20Make-lightgrey)

`ray-scene-tracer`는 42 `miniRT` 과제를 변형한 C++17 CPU ray tracer입니다. 원래 C 과제의 장면 입력과 ray tracing 학습 목표를 유지하면서, C++ 표준 라이브러리와 객체 지향 모듈로 구현합니다.

## 지원 범위

- `.rt` 장면 파일 파싱
- 카메라, 환경광, 점광원
- 구, 평면, 유한 원기둥 교차 계산
- 직접광과 그림자
- P3 PPM 이미지 출력
- linear/BVH 가속 모드
- 고정 thread 수 또는 자동 thread 수
- checksum 기반 렌더 결과 결정성 검증

GUI, GPU 렌더링, mesh와 texture는 범위에 포함하지 않습니다.

## 빌드

Make와 CMake를 모두 지원하지만, 둘은 같은 CMake build tree와 산출물 계약을 사용합니다.

```sh
make
```

직접 CMake를 사용할 때도 동일하게 `build/`를 지정합니다.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --parallel
```

산출물은 다음 위치에 모입니다.

```text
build/bin/ray-scene-tracer
build/bin/ray-benchmark
build/bin/ray-core-tests
build/lib/libraycore.a
```

루트에는 실행 파일 symlink를 만들지 않습니다. 따라서 Make와 CMake를 번갈아 사용해도 생성물이 루트에 섞이지 않습니다.

## 사용 예시

```sh
./build/bin/ray-scene-tracer scenes/basic.rt output.ppm
./build/bin/ray-scene-tracer scenes/basic.rt output.ppm \
    --accel bvh --threads auto --max-depth 4 --checksum
```

첫 두 인자는 입력 장면과 출력 PPM 경로입니다. 추가 옵션은 `--checksum`, `--accel linear|bvh`, `--threads N|auto`, `--max-depth 0..32`입니다.

## 테스트

Make 또는 CMake 어느 쪽으로도 전체 테스트를 실행할 수 있습니다.

```sh
make test
ctest --test-dir build --output-on-failure --no-tests=error
```

테스트는 parser, core, material, acceleration, output failure, CLI 계약, 렌더 smoke와 linear/BVH 및 thread 수 간 출력 결정성을 검사합니다.

Sanitizer 검증은 별도 build tree를 사용합니다.

```sh
make sanitize
```

벤치마크는 다음과 같이 실행합니다.

```sh
./build/bin/ray-benchmark
```

## 정리

```sh
make clean  # 지정된 CMake build tree 삭제
make fclean # clean과 동일
make re     # fclean 후 전체 재빌드
```

`BUILD_DIR`을 바꿔 별도 build tree를 사용할 수 있습니다.

```sh
make BUILD_DIR=build/debug BUILD_TYPE=Debug test
```

빌드 산출물과 생성 PPM은 저장소에 포함하지 않습니다.
