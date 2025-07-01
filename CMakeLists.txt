cmake_minimum_required(VERSION 3.20)

# Project definition
project(photon
    VERSION 0.1.0
    DESCRIPTION "The Photon Programming Language Compiler"
    LANGUAGES CXX
)

# C++20 is required
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Project options
option(PHOTON_ENABLE_TESTING "Enable testing" ON)
option(PHOTON_ENABLE_BENCHMARKS "Enable benchmarks" ON)
option(PHOTON_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(PHOTON_ENABLE_TSAN "Enable ThreadSanitizer" OFF)
option(PHOTON_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
option(PHOTON_ENABLE_COVERAGE "Enable code coverage" OFF)
option(PHOTON_ENABLE_LTO "Enable Link Time Optimization" OFF)
option(PHOTON_ENABLE_FUZZING "Enable fuzzing" OFF)

# Global compile options
add_compile_options(
    -Wall -Wextra -Wpedantic
    -Wconversion -Wsign-conversion
    -Wcast-qual -Wcast-align
    -Wpointer-arith -Winit-self
    -Wshadow -Wstrict-aliasing
)

# Configure based on build type
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions(PHOTON_DEBUG=1)
else()
    add_compile_definitions(NDEBUG=1)
endif()

# Sanitizers
if(PHOTON_ENABLE_ASAN)
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
endif()

if(PHOTON_ENABLE_TSAN)
    add_compile_options(-fsanitize=thread -fno-omit-frame-pointer)
    add_link_options(-fsanitize=thread)
endif()

if(PHOTON_ENABLE_UBSAN)
    add_compile_options(-fsanitize=undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=undefined)
endif()

# Code coverage
if(PHOTON_ENABLE_COVERAGE)
    add_compile_options(--coverage)
    add_link_options(--coverage)
endif()

# Testing framework
if(PHOTON_ENABLE_TESTING)
    find_package(GTest QUIET)
    if(GTest_FOUND)
        enable_testing()
        message(STATUS "GTest found - tests enabled")
    else()
        message(STATUS "GTest not found - tests disabled")
        set(PHOTON_ENABLE_TESTING OFF)
    endif()
endif()

# Benchmarking framework
if(PHOTON_ENABLE_BENCHMARKS)
    find_package(benchmark QUIET)
    if(NOT benchmark_FOUND)
        message(STATUS "benchmark library not found - benchmarks disabled")
        set(PHOTON_ENABLE_BENCHMARKS OFF)
    endif()
endif()

# Add compiler components
add_subdirectory(compiler)

# Temporary main executable for testing
add_executable(photonc main.cpp)
target_link_libraries(photonc PRIVATE photon::common photon::memory photon::diagnostics)
