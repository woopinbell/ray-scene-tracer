cmake_minimum_required(VERSION 3.16)

project(ray_scene_tracer LANGUAGES CXX)

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
endif()
