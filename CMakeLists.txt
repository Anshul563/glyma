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

option(GLYMA_BUILD_APP "Build the Qt 6 desktop application" ON)

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

add_executable(glyma_demo
    src/main.cpp
)

target_link_libraries(glyma_demo PRIVATE glyma_core)
target_compile_options(glyma_demo PRIVATE -Wall -Wextra -Wpedantic)

if(GLYMA_BUILD_APP)
    find_package(Qt6 REQUIRED COMPONENTS Widgets
        HINTS "$ENV{HOME}/.local/qt6/usr"
    )

    add_library(glyma_app_ui STATIC
        include/glyma/app/app_controller.hpp
        include/glyma/app/canvas_view.hpp
        include/glyma/app/document_lifecycle.hpp
        include/glyma/app/layer_dock.hpp
        include/glyma/app/main_window.hpp
        include/glyma/app/qt_image.hpp
        src/app/app_controller.cpp
        src/app/canvas_view.cpp
        src/app/document_lifecycle.cpp
        src/app/layer_dock.cpp
        src/app/main_window.cpp
        src/app/qt_image.cpp
    )

    set_target_properties(glyma_app_ui PROPERTIES AUTOMOC ON)
    target_link_libraries(glyma_app_ui PUBLIC glyma_core Qt6::Widgets)
    target_compile_options(glyma_app_ui PRIVATE -Wall -Wextra -Wpedantic)

    add_executable(glyma
        src/app/main.cpp
    )
    target_link_libraries(glyma PRIVATE glyma_app_ui)
    target_compile_options(glyma PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(BUILD_TESTING)
    add_executable(glyma_tests
        tests/image_buffer_tests.cpp
    )

    add_executable(glyma_lifecycle_tests
        tests/lifecycle_tests.cpp
        src/app/document_lifecycle.cpp
    )
    target_link_libraries(glyma_lifecycle_tests PRIVATE glyma_core)
    target_compile_options(glyma_lifecycle_tests PRIVATE -Wall -Wextra -Wpedantic)

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
        NAME glyma_lifecycle_tests
        COMMAND glyma_lifecycle_tests
    )

    add_test(
        NAME glyma_png_tests
        COMMAND glyma_png_tests
    )

    add_test(
        NAME glyma_project_tests
        COMMAND glyma_project_tests
    )

    if(GLYMA_BUILD_APP)
        add_executable(glyma_ui_tests
            tests/ui_tests.cpp
        )
        target_link_libraries(glyma_ui_tests PRIVATE glyma_app_ui)
        target_compile_options(glyma_ui_tests PRIVATE -Wall -Wextra -Wpedantic)

        add_test(
            NAME glyma_ui_tests
            COMMAND glyma_ui_tests
        )
        set_tests_properties(glyma_ui_tests PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        )
    endif()
endif()
