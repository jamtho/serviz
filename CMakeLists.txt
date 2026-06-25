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
        "Run: python scripts/download_deps.py")
endif()

# ----- Main executable -----
add_executable(serviz
    src/main.c
    src/cli.c
    src/spec.c
    src/data.c
    src/axes.c
    src/render.c
    src/tooltip.c
    src/colormap.c
    src/palette.c
)

target_include_directories(serviz PRIVATE src)
target_link_libraries(serviz PRIVATE raylib cjson duckdb serviz_warnings)

if(NOT WIN32)
    target_link_libraries(serviz PRIVATE pthread dl m)
endif()

# Compiler warnings
if(MSVC)
    add_library(serviz_warnings INTERFACE)
    target_compile_options(serviz_warnings INTERFACE /W3)
else()
    add_library(serviz_warnings INTERFACE)
    target_compile_options(serviz_warnings INTERFACE -Wall -Wextra -Wpedantic)
endif()

# Test flags: re-enable assert() in Release builds (NDEBUG strips it).
if(MSVC)
    add_library(serviz_test_flags INTERFACE)
else()
    add_library(serviz_test_flags INTERFACE)
    target_compile_options(serviz_test_flags INTERFACE -UNDEBUG)
endif()

# ----- Tests -----
enable_testing()

add_executable(test_colormap tests/test_colormap.c src/colormap.c)
target_include_directories(test_colormap PRIVATE src)
target_link_libraries(test_colormap PRIVATE serviz_warnings serviz_test_flags)
add_test(NAME test_colormap COMMAND test_colormap)

add_executable(test_transform tests/test_transform.c)
target_include_directories(test_transform PRIVATE src)
target_link_libraries(test_transform PRIVATE m serviz_warnings serviz_test_flags)
add_test(NAME test_transform COMMAND test_transform)

add_executable(test_palette tests/test_palette.c src/palette.c)
target_include_directories(test_palette PRIVATE src)
target_link_libraries(test_palette PRIVATE raylib serviz_warnings serviz_test_flags)
add_test(NAME test_palette COMMAND test_palette)

add_executable(test_spec tests/test_spec.c src/spec.c src/colormap.c)
target_include_directories(test_spec PRIVATE src)
target_link_libraries(test_spec PRIVATE cjson serviz_warnings serviz_test_flags)
add_test(NAME test_spec COMMAND test_spec)

add_executable(test_data tests/test_data.c src/data.c)
target_include_directories(test_data PRIVATE src)
target_link_libraries(test_data PRIVATE duckdb serviz_warnings serviz_test_flags)
if(WIN32)
    target_link_libraries(test_data PRIVATE ws2_32)
endif()
add_test(NAME test_data COMMAND test_data)

add_executable(test_axes tests/test_axes.c src/axes.c)
target_include_directories(test_axes PRIVATE src)
target_link_libraries(test_axes PRIVATE raylib m serviz_warnings serviz_test_flags)
add_test(NAME test_axes COMMAND test_axes)

add_executable(test_cli tests/test_cli.c src/cli.c)
target_include_directories(test_cli PRIVATE src)
target_link_libraries(test_cli PRIVATE serviz_warnings serviz_test_flags)
add_test(NAME test_cli COMMAND test_cli)

add_executable(test_tooltip tests/test_tooltip.c src/tooltip.c)
target_include_directories(test_tooltip PRIVATE src)
target_link_libraries(test_tooltip PRIVATE raylib serviz_warnings serviz_test_flags)
add_test(NAME test_tooltip COMMAND test_tooltip)

# ----- Install -----
include(GNUInstallDirs)
install(TARGETS serviz RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES README.md REVIEW.md LICENSE DESTINATION ${CMAKE_INSTALL_DOCDIR})
