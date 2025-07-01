# Memory management component

# Create memory library
add_library(photon_memory INTERFACE)
target_include_directories(photon_memory INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(photon_memory INTERFACE photon_common)

# Alias for consistent naming
add_library(photon::memory ALIAS photon_memory)

# Tests
if(PHOTON_ENABLE_TESTING)
    add_subdirectory(tests)
endif()

# Benchmarks
if(PHOTON_ENABLE_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()