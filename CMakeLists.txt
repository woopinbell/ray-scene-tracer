cmake_minimum_required(VERSION 3.16)

project(ray_scene_tracer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(raycore
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

if(MSVC)
    target_compile_options(raycore PRIVATE /W4)
else()
    target_compile_options(raycore PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(ray-scene-tracer src/main.cpp)
target_link_libraries(ray-scene-tracer PRIVATE raycore)

include(CTest)
if(BUILD_TESTING)
    add_executable(ray-core-tests tests/core_tests.cpp)
    target_link_libraries(ray-core-tests PRIVATE raycore)
    target_compile_definitions(
        ray-core-tests
        PRIVATE RAY_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
    )

    add_test(NAME core_regression COMMAND ray-core-tests)
    add_test(
        NAME render_smoke
        COMMAND bash
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/render_smoke.sh
                $<TARGET_FILE:ray-scene-tracer>
    )
endif()
