cmake_minimum_required(VERSION 3.20)

project(Glyma
    VERSION 0.1.0
    DESCRIPTION "Open-source image editing engine"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)

find_package(PkgConfig REQUIRED)
pkg_check_modules(PNG REQUIRED libpng)

# libzip: prefer its CMake package, fall back to pkg-config.
find_package(libzip QUIET CONFIG HINTS "$ENV{HOME}/.local")
if(NOT libzip_FOUND)
    pkg_check_modules(LIBZIP REQUIRED libzip)
endif()

# nlohmann/json: header-only; prefer installed package, fall back to find_path.
find_package(nlohmann_json QUIET CONFIG HINTS "$ENV{HOME}/.local")
if(NOT nlohmann_json_FOUND)
    find_path(NLOHMANN_JSON_INCLUDE_DIR nlohmann/json.hpp
        HINTS "$ENV{HOME}/.local/include"
        REQUIRED
    )
endif()

add_library(glyma_core STATIC
    src/engine/image_buffer.cpp
    src/engine/compositor.cpp
    src/engine/layer.cpp
    src/engine/layer_stack.cpp
    src/engine/document.cpp
    src/engine/command_history.cpp
    src/engine/commands.cpp
    src/io/png_reader.cpp
    src/io/png_writer.cpp
    src/io/document_io.cpp
    src/io/project_writer.cpp
    src/io/project_reader.cpp
)

target_include_directories(glyma_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${PNG_INCLUDE_DIRS}
)

target_link_directories(glyma_core PUBLIC ${PNG_LIBRARY_DIRS})
target_link_libraries(glyma_core PUBLIC ${PNG_LIBRARIES})
target_compile_options(glyma_core PUBLIC ${PNG_CFLAGS_OTHER})

if(libzip_FOUND)
    target_link_libraries(glyma_core PUBLIC libzip::zip)
else()
    target_include_directories(glyma_core PUBLIC ${LIBZIP_INCLUDE_DIRS})
    target_link_directories(glyma_core PUBLIC ${LIBZIP_LIBRARY_DIRS})
    target_link_libraries(glyma_core PUBLIC ${LIBZIP_LIBRARIES})
endif()

if(nlohmann_json_FOUND)
    target_link_libraries(glyma_core PUBLIC nlohmann_json::nlohmann_json)
else()
    target_include_directories(glyma_core PUBLIC ${NLOHMANN_JSON_INCLUDE_DIR})
endif()

target_compile_options(glyma_core PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

add_executable(glyma
    src/main.cpp
)

target_link_libraries(glyma PRIVATE glyma_core)

if(BUILD_TESTING)
    add_executable(glyma_tests
        tests/image_buffer_tests.cpp
    )

    add_executable(glyma_layer_tests
        tests/layer_tests.cpp
    )

    add_executable(glyma_document_tests
        tests/document_tests.cpp
    )

    add_executable(glyma_history_tests
        tests/command_history_tests.cpp
    )

    add_executable(glyma_png_tests
        tests/png_io_tests.cpp
    )

    add_executable(glyma_project_tests
        tests/project_format_tests.cpp
    )

    target_link_libraries(glyma_tests PRIVATE glyma_core)
    target_link_libraries(glyma_layer_tests PRIVATE glyma_core)
    target_link_libraries(glyma_document_tests PRIVATE glyma_core)
    target_link_libraries(glyma_history_tests PRIVATE glyma_core)
    target_link_libraries(glyma_png_tests PRIVATE glyma_core)
    target_link_libraries(glyma_project_tests PRIVATE glyma_core)

    target_compile_options(glyma_tests PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(glyma_layer_tests PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(glyma_document_tests PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(glyma_history_tests PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(glyma_png_tests PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(glyma_project_tests PRIVATE -Wall -Wextra -Wpedantic)

    add_test(
        NAME glyma_image_buffer_tests
        COMMAND glyma_tests
    )

    add_test(
        NAME glyma_layer_tests
        COMMAND glyma_layer_tests
    )

    add_test(
        NAME glyma_document_tests
        COMMAND glyma_document_tests
    )

    add_test(
        NAME glyma_history_tests
        COMMAND glyma_history_tests
    )

    add_test(
        NAME glyma_png_tests
        COMMAND glyma_png_tests
    )

    add_test(
        NAME glyma_project_tests
        COMMAND glyma_project_tests
    )
endif()