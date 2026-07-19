cmake_minimum_required(VERSION 3.20)
project(lucks VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

find_package(raylib 6.0 QUIET)
if (NOT raylib_FOUND)
    message(STATUS "raylib 6.0 not found locally; downloading it")
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        raylib
        GIT_REPOSITORY https://github.com/raysan5/raylib.git
        GIT_TAG 6.0
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(raylib)
endif()

FetchContent_Declare(
    raygui
    GIT_REPOSITORY https://github.com/raysan5/raygui.git
    GIT_TAG 4.0
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR _skip_cmake_project
)
FetchContent_MakeAvailable(raygui)

find_package(nlohmann_json 3.11 QUIET)
if (NOT nlohmann_json_FOUND)
    FetchContent_Declare(
        nlohmann_json
        URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

add_library(lucks_core
    src/core/simulation.cpp
    src/map/stack_generator.cpp
    src/persistence/result_writer.cpp
)
target_include_directories(lucks_core PUBLIC src)
target_link_libraries(lucks_core PUBLIC nlohmann_json::nlohmann_json)

add_executable(lucks
    src/app/main.cpp
    src/ui/visualizer.cpp
)
target_include_directories(lucks PRIVATE src "${raygui_SOURCE_DIR}/src")
target_link_libraries(lucks PRIVATE lucks_core raylib)

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(lucks_core PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(lucks PRIVATE -Wall -Wextra -Wpedantic)
endif()

set_target_properties(lucks PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

include(CTest)
if (BUILD_TESTING)
    add_executable(lucks_core_tests tests/core_tests.cpp)
    target_link_libraries(lucks_core_tests PRIVATE lucks_core)
    add_test(NAME lucks_core_tests COMMAND lucks_core_tests)
endif()
