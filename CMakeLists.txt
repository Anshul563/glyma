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
    src/engine/brush_engine.cpp
    src/engine/selection.cpp
    src/engine/fill.cpp
    src/engine/adjustments.cpp
    src/engine/pixel_edit.cpp
    src/engine/transform.cpp
    src/engine/text_layer.cpp
    src/engine/text_commands.cpp
    src/engine/shape_layer.cpp
    src/engine/shape_commands.cpp
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

    # Generate a Qt resource file from the semantic icon manifest so that only
    # the referenced assets are bundled (the downloaded collection has 6000+
    # files). The manifest is the single source of truth for the mapping and is
    # also embedded so IconManager can resolve semantic names at runtime.
    set(GLYMA_ICON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons")
    set(GLYMA_ICON_MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/resources/icon_manifest.txt")
    set(GLYMA_ICON_QRC "${CMAKE_CURRENT_BINARY_DIR}/glyma_icons.qrc")

    if(NOT EXISTS "${GLYMA_ICON_MANIFEST}")
        message(FATAL_ERROR "Missing icon manifest: ${GLYMA_ICON_MANIFEST}")
    endif()

    file(STRINGS "${GLYMA_ICON_MANIFEST}" GLYMA_ICON_MANIFEST_LINES)
    file(WRITE "${GLYMA_ICON_QRC}" "<!DOCTYPE RCC>\n<RCC version=\"1.0\">\n")
    file(APPEND "${GLYMA_ICON_QRC}" "  <qresource prefix=\"/glyma\">\n")
    file(APPEND "${GLYMA_ICON_QRC}"
        "    <file alias=\"icon_manifest.txt\">${GLYMA_ICON_MANIFEST}</file>\n"
    )

    set(GLYMA_ICON_ASSET_COUNT 0)
    set(GLYMA_ICON_SEEN "")
    foreach(GLYMA_ICON_LINE IN LISTS GLYMA_ICON_MANIFEST_LINES)
        string(STRIP "${GLYMA_ICON_LINE}" GLYMA_ICON_LINE)
        if(GLYMA_ICON_LINE STREQUAL "" OR GLYMA_ICON_LINE MATCHES "^#")
            continue()
        endif()
        string(REPLACE "=" ";" GLYMA_ICON_PARTS "${GLYMA_ICON_LINE}")
        list(LENGTH GLYMA_ICON_PARTS GLYMA_ICON_PARTS_LEN)
        if(GLYMA_ICON_PARTS_LEN LESS 2)
            message(FATAL_ERROR "Malformed icon manifest line: ${GLYMA_ICON_LINE}")
        endif()
        list(GET GLYMA_ICON_PARTS 0 GLYMA_ICON_NAME)
        list(GET GLYMA_ICON_PARTS 1 GLYMA_ICON_FILE)
        string(STRIP "${GLYMA_ICON_NAME}" GLYMA_ICON_NAME)
        string(STRIP "${GLYMA_ICON_FILE}" GLYMA_ICON_FILE)
        if(NOT EXISTS "${GLYMA_ICON_DIR}/${GLYMA_ICON_FILE}")
            message(FATAL_ERROR
                "Icon manifest references missing asset '${GLYMA_ICON_FILE}' for '${GLYMA_ICON_NAME}'"
            )
        endif()
        # Several semantic names may share one asset; register each file once.
        if(NOT GLYMA_ICON_FILE IN_LIST GLYMA_ICON_SEEN)
            list(APPEND GLYMA_ICON_SEEN "${GLYMA_ICON_FILE}")
            file(APPEND "${GLYMA_ICON_QRC}"
                "    <file alias=\"icons/${GLYMA_ICON_FILE}\">${GLYMA_ICON_DIR}/${GLYMA_ICON_FILE}</file>\n"
            )
            math(EXPR GLYMA_ICON_ASSET_COUNT "${GLYMA_ICON_ASSET_COUNT} + 1")
        endif()
    endforeach()
    file(APPEND "${GLYMA_ICON_QRC}" "  </qresource>\n</RCC>\n")
    message(STATUS "Glyma: bundling ${GLYMA_ICON_ASSET_COUNT} icons from icon_manifest.txt")

    add_library(glyma_app_ui STATIC
        include/glyma/app/adjustment_dialog.hpp
        include/glyma/app/app_controller.hpp
        include/glyma/app/canvas_view.hpp
        include/glyma/app/document_lifecycle.hpp
        include/glyma/app/icon_manager.hpp
        include/glyma/app/layer_dock.hpp
        include/glyma/app/main_window.hpp
        include/glyma/app/qt_image.hpp
        include/glyma/app/shape_properties_panel.hpp
        include/glyma/app/shape_renderer.hpp
        include/glyma/app/text_renderer.hpp
        include/glyma/app/typography_panel.hpp
        src/app/adjustment_dialog.cpp
        src/app/app_controller.cpp
        src/app/canvas_view.cpp
        src/app/document_lifecycle.cpp
        src/app/icon_manager.cpp
        src/app/layer_dock.cpp
        src/app/main_window.cpp
        src/app/qt_image.cpp
        src/app/shape_properties_panel.cpp
        src/app/shape_renderer.cpp
        src/app/text_renderer.cpp
        src/app/typography_panel.cpp
        ${GLYMA_ICON_QRC}
    )

    set_target_properties(glyma_app_ui PROPERTIES AUTOMOC ON AUTORCC ON)
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

    add_executable(glyma_brush_tests
        tests/brush_tests.cpp
    )

    add_executable(glyma_selection_tests
        tests/selection_tests.cpp
    )

    add_executable(glyma_fill_tests
        tests/fill_tests.cpp
    )

    add_executable(glyma_adjustments_tests
        tests/adjustments_tests.cpp
    )

    add_executable(glyma_pixel_edit_tests
        tests/pixel_edit_tests.cpp
    )

    add_executable(glyma_transform_tests
        tests/transform_tests.cpp
    )

    add_executable(glyma_text_tests
        tests/text_layer_tests.cpp
    )

    add_executable(glyma_shape_tests
        tests/shape_layer_tests.cpp
    )

    target_link_libraries(glyma_tests PRIVATE glyma_core)
    target_link_libraries(glyma_layer_tests PRIVATE glyma_core)
    target_link_libraries(glyma_document_tests PRIVATE glyma_core)
    target_link_libraries(glyma_history_tests PRIVATE glyma_core)
    target_link_libraries(glyma_png_tests PRIVATE glyma_core)
    target_link_libraries(glyma_project_tests PRIVATE glyma_core)
    target_link_libraries(glyma_brush_tests PRIVATE glyma_core)
    target_link_libraries(glyma_selection_tests PRIVATE glyma_core)
    target_link_libraries(glyma_fill_tests PRIVATE glyma_core)
    target_link_libraries(glyma_adjustments_tests PRIVATE glyma_core)
    target_link_libraries(glyma_pixel_edit_tests PRIVATE glyma_core)
    target_link_libraries(glyma_transform_tests PRIVATE glyma_core)
    target_link_libraries(glyma_text_tests PRIVATE glyma_core)
    target_link_libraries(glyma_shape_tests PRIVATE glyma_core)

    target_compile_options(glyma_tests PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(glyma_layer_tests PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(glyma_document_tests PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(glyma_history_tests PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(glyma_png_tests PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(glyma_project_tests PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(glyma_brush_tests PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(glyma_selection_tests PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(glyma_fill_tests PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(glyma_adjustments_tests PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(glyma_pixel_edit_tests PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(glyma_transform_tests PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(glyma_text_tests PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(glyma_shape_tests PRIVATE -Wall -Wextra -Wpedantic)

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

    add_test(
        NAME glyma_brush_tests
        COMMAND glyma_brush_tests
    )

    add_test(
        NAME glyma_selection_tests
        COMMAND glyma_selection_tests
    )

    add_test(
        NAME glyma_fill_tests
        COMMAND glyma_fill_tests
    )

    add_test(
        NAME glyma_adjustments_tests
        COMMAND glyma_adjustments_tests
    )

    add_test(
        NAME glyma_pixel_edit_tests
        COMMAND glyma_pixel_edit_tests
    )

    add_test(
        NAME glyma_transform_tests
        COMMAND glyma_transform_tests
    )

    add_test(
        NAME glyma_text_tests
        COMMAND glyma_text_tests
    )

    add_test(
        NAME glyma_shape_tests
        COMMAND glyma_shape_tests
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

        add_executable(glyma_icon_tests
            tests/icon_manager_tests.cpp
        )
        target_link_libraries(glyma_icon_tests PRIVATE glyma_app_ui)
        target_compile_options(glyma_icon_tests PRIVATE -Wall -Wextra -Wpedantic)

        add_test(
            NAME glyma_icon_tests
            COMMAND glyma_icon_tests
        )
        set_tests_properties(glyma_icon_tests PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        )
    endif()
endif()
