# Diagnostics component benchmarks

if(NOT PHOTON_ENABLE_BENCHMARKS)
    return()
endif()

# Create benchmark executable
add_executable(diagnostics_benchmark
    diagnostic_benchmark.cpp
)

target_link_libraries(diagnostics_benchmark
    PRIVATE
    photon::diagnostics
    photon::memory
    photon::common
    benchmark::benchmark
    benchmark::benchmark_main
)

target_compile_options(diagnostics_benchmark PRIVATE -O3 -DNDEBUG)

# Set benchmark properties
set_target_properties(diagnostics_benchmark PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/benchmarks"
)