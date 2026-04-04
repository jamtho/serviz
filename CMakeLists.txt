cmake_minimum_required(VERSION 3.16)
project(serviz C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# ----- raylib via FetchContent -----
include(FetchContent)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.5
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(raylib)

# ----- cJSON (vendored) -----
add_library(cjson STATIC third_party/cjson/cJSON.c)
target_include_directories(cjson PUBLIC third_party/cjson)

# ----- DuckDB (vendored amalgamation, compiled as C++) -----
if(EXISTS "${CMAKE_SOURCE_DIR}/third_party/duckdb/duckdb.cpp")
    add_library(duckdb STATIC third_party/duckdb/duckdb.cpp)
    target_include_directories(duckdb PUBLIC third_party/duckdb)
    set_target_properties(duckdb PROPERTIES LINKER_LANGUAGE CXX)
    target_compile_definitions(duckdb PUBLIC DUCKDB_BUILD_LIBRARY)
    if(MSVC)
        target_compile_options(duckdb PRIVATE /bigobj)
    endif()
    if(WIN32)
        target_compile_definitions(duckdb PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
        target_link_libraries(duckdb PRIVATE ws2_32 rstrtmgr bcrypt)
    else()
        target_link_libraries(duckdb PRIVATE pthread dl)
    endif()
else()
    message(FATAL_ERROR
        "DuckDB amalgamation not found at third_party/duckdb/duckdb.cpp\n"
        "Copy from geoviz or run download script")
endif()

# ----- Main executable -----
add_executable(serviz
    src/main.c
    src/spec.c
    src/data.c
    src/axes.c
    src/render.c
    src/tooltip.c
    src/colormap.c
    src/palette.c
)

target_include_directories(serviz PRIVATE src)
target_link_libraries(serviz PRIVATE raylib cjson duckdb)

if(NOT WIN32)
    target_link_libraries(serviz PRIVATE pthread dl m)
endif()

# Compiler warnings
if(MSVC)
    target_compile_options(serviz PRIVATE /W3)
else()
    target_compile_options(serviz PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ----- Tests -----
enable_testing()

add_executable(test_colormap tests/test_colormap.c src/colormap.c)
target_include_directories(test_colormap PRIVATE src)
add_test(NAME test_colormap COMMAND test_colormap)

add_executable(test_transform tests/test_transform.c)
target_include_directories(test_transform PRIVATE src)
target_link_libraries(test_transform PRIVATE m)
add_test(NAME test_transform COMMAND test_transform)

add_executable(test_palette tests/test_palette.c src/palette.c)
target_include_directories(test_palette PRIVATE src)
target_link_libraries(test_palette PRIVATE raylib)
add_test(NAME test_palette COMMAND test_palette)

add_executable(test_spec tests/test_spec.c src/spec.c src/colormap.c)
target_include_directories(test_spec PRIVATE src)
target_link_libraries(test_spec PRIVATE cjson)
add_test(NAME test_spec COMMAND test_spec)

add_executable(test_data tests/test_data.c src/data.c)
target_include_directories(test_data PRIVATE src)
target_link_libraries(test_data PRIVATE duckdb)
if(WIN32)
    target_link_libraries(test_data PRIVATE ws2_32)
endif()
add_test(NAME test_data COMMAND test_data)

add_executable(test_axes tests/test_axes.c src/axes.c)
target_include_directories(test_axes PRIVATE src)
target_link_libraries(test_axes PRIVATE raylib m)
add_test(NAME test_axes COMMAND test_axes)
