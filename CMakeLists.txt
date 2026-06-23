cmake_minimum_required(VERSION 3.21)
project(verosim LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---- vendored Verovio (Layer 1 parser) ----
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/verovio/cmake/CMakeLists.txt")
    message(FATAL_ERROR
        "Verovio submodule is missing. Run: git submodule update --init --recursive")
endif()
option(VEROSIM_STATIC_VEROVIO "Build vendored Verovio as a static library" ON)
set(BUILD_AS_LIBRARY ON CACHE BOOL "" FORCE)
if(VEROSIM_STATIC_VEROVIO)
    set(VEROVIO_BUILD_SHARED OFF CACHE BOOL "" FORCE)
else()
    set(VEROVIO_BUILD_SHARED ON CACHE BOOL "" FORCE)
endif()
add_subdirectory(verovio/cmake verovio-build)
if(TARGET verovio)
    target_compile_options(verovio PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-w>)
endif()

# Verovio's include_directories() are directory-scoped and do not propagate;
# any target including toolkit.h needs the full list itself.
set(VEROVIO_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/verovio)
set(VEROVIO_INCLUDE_DIRS
    ${VEROVIO_ROOT}/include
    ${VEROVIO_ROOT}/include/crc
    ${VEROVIO_ROOT}/include/midi
    ${VEROVIO_ROOT}/include/hum
    ${VEROVIO_ROOT}/include/json
    ${VEROVIO_ROOT}/include/pugi
    ${VEROVIO_ROOT}/include/tuning-library
    ${VEROVIO_ROOT}/include/zip
    ${VEROVIO_ROOT}/include/vrv
    ${VEROVIO_ROOT}/libmei/dist
    ${VEROVIO_ROOT}/libmei/addons)

add_library(verosim_core STATIC
    src/extraction/vrv_bridge.cpp
    src/extraction/extract.cpp
    src/extraction/event_extraction.cpp
    src/extraction/barline_extraction.cpp
    src/extraction/pitch_context.cpp
    src/extraction/meter_context.cpp
    src/extraction/control_extraction.cpp
    src/extraction/signature_extraction.cpp
    src/extraction/score_normalizer.cpp
    src/extraction/typed_space_policy.cpp
    src/extraction/effective_pitch.cpp
    src/extraction/notation_rules.cpp
    src/model/metric_mode.cpp
    src/model/sym_score.cpp
    src/engine/interner.cpp
    src/engine/myers.cpp
    src/engine/note_diff.cpp
    src/engine/set_distance.cpp
    src/engine/block_diff.cpp
    src/engine/compare.cpp
    src/cli/args.cpp
    src/cli/list_io.cpp
    src/cli/batch.cpp
    src/cli/check.cpp
    src/cli/compare_cli.cpp
    src/cli/count_symbols.cpp
    src/cli/dump_tree.cpp
    src/visual/html_report.cpp
    src/visual/score_renderer.cpp
    src/visual/svg_annotator.cpp
    src/visual/svg_bundle.cpp
    src/visual/svg_overlap.cpp
    src/visual/svg_symbol_index.cpp
    src/visual/visual_mark.cpp
    src/visual/visual_plan.cpp
    src/visual/visual_resolver.cpp
    src/visual/visualize.cpp)
target_include_directories(verosim_core PUBLIC include ${VEROVIO_INCLUDE_DIRS})
target_link_libraries(verosim_core PUBLIC verovio)
# Toolkit needs the font resources from the vendored data dir even though we
# never render (LoadData refuses to run without them, toolkit.cpp:516-522).
target_compile_definitions(verosim_core PUBLIC
    VEROSIM_VRV_DATA_DIR="${VEROVIO_ROOT}/data")

add_executable(verosim src/cli/main.cpp)
target_link_libraries(verosim PRIVATE verosim_core)

include(CTest)
if(BUILD_TESTING)
    include(FetchContent)
    FetchContent_Declare(Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.8.1)
    FetchContent_MakeAvailable(Catch2)
    list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
    add_subdirectory(tests)
endif()
