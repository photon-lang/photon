# Source component

# Create source library
add_library(photon_source STATIC
    src/source_manager.cpp
)

target_include_directories(photon_source PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(photon_source PUBLIC 
    photon::common
    photon::memory
    photon::diagnostics
)

target_compile_features(photon_source PUBLIC cxx_std_20)

# Alias for consistent naming
add_library(photon::source ALIAS photon_source)

# Tests
if(PHOTON_ENABLE_TESTING)
    add_subdirectory(tests)
endif()

# Benchmarks
if(PHOTON_ENABLE_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()