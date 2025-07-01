# Memory component benchmarks

# Create benchmark executable
add_executable(memory_bench arena_bench.cpp)

target_link_libraries(memory_bench
    PRIVATE
    photon::memory
    photon::common
    benchmark::benchmark
)

target_compile_options(memory_bench PRIVATE -O3 -DNDEBUG)

# Set benchmark properties
set_target_properties(memory_bench PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/benchmarks
)