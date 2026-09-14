cmake_minimum_required(VERSION 3.16)

project(ray_scene_tracer LANGUAGES CXX)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

find_package(Threads REQUIRED)
option(RAY_ENABLE_SANITIZERS "Enable AddressSanitizer and UBSan" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(raycore
    src/accel.cpp
    src/camera.cpp
    src/geometry.cpp
    src/math.cpp
    src/output.cpp
    src/parser.cpp
    src/renderer.cpp
    src/scene.cpp
    src/shading.cpp
)
target_include_directories(raycore PUBLIC include)
target_link_libraries(raycore PUBLIC Threads::Threads)

if(MSVC)
    target_compile_options(raycore PRIVATE /W4)
else()
    target_compile_options(raycore PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(RAY_ENABLE_SANITIZERS)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(
            raycore
            PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer
        )
        target_link_options(
            raycore
            PUBLIC -fsanitize=address,undefined
        )
    else()
        message(FATAL_ERROR "Sanitizers require Clang or GCC")
    endif()
endif()

add_executable(ray-scene-tracer src/main.cpp)
target_link_libraries(ray-scene-tracer PRIVATE raycore)

add_executable(ray-benchmark benchmarks/render_benchmark.cpp)
target_link_libraries(ray-benchmark PRIVATE raycore)

# [INTV:ARCH] CI(.github/workflows/ci.yml)의 "회귀 검사" 스텝이 실행하는 `ctest`는 바로 이
# add_test() 목록을 읽어 돈다 — 즉 이 블록이 "CI가 실제로 무엇을 검증하는가"의 목록 그 자체다.
# 크게 두 계층으로 나뉜다: (1) C++ 실행 파일로 컴파일되는 단위/통합 테스트(core/accel/material/
# render/output — 각 tests/*.cpp가 raycore 라이브러리를 직접 링크해 함수 단위로 검증), (2) 완성된
# ray-scene-tracer 바이너리를 블랙박스로 실행해보는 셸 스크립트 테스트(cli_contract, 결정론적
# 렌더링 재현, 스모크). include(CTest) + BUILD_TESTING은 CMake 표준 관용구 — 테스트 타겟을
# 프로덕션 빌드에는 안 끼워도 되게 옵션으로 분리해둔 것(CI는 -DBUILD_TESTING=ON으로 켜서 돈다).
include(CTest)
if(BUILD_TESTING)
    add_executable(ray-core-tests tests/core_tests.cpp)
    target_link_libraries(ray-core-tests PRIVATE raycore)
    target_compile_definitions(
        ray-core-tests
        PRIVATE RAY_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
    )

    add_test(NAME core_regression COMMAND ray-core-tests)

    add_executable(ray-accel-tests tests/accel_tests.cpp)
    target_link_libraries(ray-accel-tests PRIVATE raycore)
    add_test(NAME accel_regression COMMAND ray-accel-tests)

    add_executable(ray-material-tests tests/material_tests.cpp)
    target_link_libraries(ray-material-tests PRIVATE raycore)
    target_compile_definitions(
        ray-material-tests
        PRIVATE RAY_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
    )
    add_test(NAME material_regression COMMAND ray-material-tests)

    add_executable(ray-render-tests tests/render_tests.cpp)
    target_link_libraries(ray-render-tests PRIVATE raycore)
    add_test(NAME render_determinism COMMAND ray-render-tests)

    add_executable(ray-output-tests tests/output_tests.cpp)
    target_link_libraries(ray-output-tests PRIVATE raycore)
    add_test(NAME output_failure_regression COMMAND ray-output-tests)

    add_test(
        NAME cli_contract
        COMMAND bash
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_contract.sh
                $<TARGET_FILE:ray-scene-tracer>
                ${CMAKE_CURRENT_SOURCE_DIR}
    )

    add_test(
        NAME render_output_determinism
        COMMAND bash
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/render_determinism.sh
                $<TARGET_FILE:ray-scene-tracer>
    )

    add_test(
        NAME render_smoke
        COMMAND bash
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/render_smoke.sh
                $<TARGET_FILE:ray-scene-tracer>
    )

    # [INTV:EDGE] 셸 스크립트 테스트 3개(cli_contract, render_output_determinism, render_smoke)에
    # $<TARGET_FILE:ray-scene-tracer>로 방금 빌드된 실행 파일의 실제 경로를 넘긴다 — 이 경로는
    # 빌드 디렉터리 이름/OS(빌드 산출물 확장자 등)에 따라 달라질 수 있어, 스크립트 안에 상대
    # 경로를 하드코딩하면 로컬과 CI에서 다르게 깨질 수 있다. CMake의 제너레이터 표현식이 빌드
    # 시점에 실제 경로로 치환해줘서 이 문제를 피한다.
    set_tests_properties(
        core_regression
        accel_regression
        material_regression
        render_determinism
        output_failure_regression
        cli_contract
        PROPERTIES TIMEOUT 60
    )
    # [INTV:EDGE] render_output_determinism/render_smoke만 TIMEOUT을 120초로 더 길게 둔 이유 —
    # 이 둘은 실제로 씬 파일을 렌더링해서 나온 이미지를 확인하는 테스트라, 함수 몇 개만 호출하고
    # 끝나는 단위 테스트들(60초)보다 구조적으로 더 오래 걸린다. 타임아웃 자체는 "무한 루프/교착
    # 상태에 빠진 테스트가 CI를 영원히 붙잡지 않게" 하는 안전장치이므로, 정상적으로 걸리는
    # 시간보다 너무 타이트하게 잡으면 느린 CI 러너에서 오탐(정상인데 타임아웃)이 날 수 있다.
    set_tests_properties(
        render_output_determinism
        render_smoke
        PROPERTIES TIMEOUT 120
    )
endif()
