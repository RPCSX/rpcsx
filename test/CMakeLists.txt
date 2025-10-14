# Tests and Benchmarks Configuration
cmake_minimum_required(VERSION 3.16)
project(MemoryTableTests)

# Add Google Benchmark as an external project if not found
find_package(benchmark QUIET)

if(NOT benchmark_FOUND)
    include(FetchContent)
    
    FetchContent_Declare(
        googlebenchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG v1.9.4
    )
    
    # Disable benchmark tests and examples to speed up build
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "Disable benchmark tests" FORCE)
    set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "Disable Google Test for benchmark" FORCE)
    set(BENCHMARK_ENABLE_WERROR OFF CACHE BOOL "Disable warnings as errors" FORCE)
    
    FetchContent_MakeAvailable(googlebenchmark)
endif()

# Add Google Test for unit testing
find_package(GTest QUIET)

if(NOT GTest_FOUND)
    include(FetchContent)
    
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.15.2
    )
    
    # Disable gtest installation
    set(INSTALL_GTEST OFF CACHE BOOL "Disable gtest installation" FORCE)
    set(gtest_force_shared_crt ON CACHE BOOL "Force shared CRT for gtest" FORCE)
    
    FetchContent_MakeAvailable(googletest)
endif()

add_executable(gcn_shader_tests
    gcn_shader_tests.cpp
)

target_link_libraries(gcn_shader_tests
    gtest
    gtest_main
    gcn-shader
)
