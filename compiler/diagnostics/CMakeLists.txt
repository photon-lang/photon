# Diagnostics component

# Create diagnostics library
add_library(photon_diagnostics STATIC
    src/source_location.cpp
    src/diagnostic_engine.cpp
    src/formatter.cpp
)

target_include_directories(photon_diagnostics PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(photon_diagnostics PUBLIC 
    photon::common
    photon::memory
)

target_compile_features(photon_diagnostics PUBLIC cxx_std_20)

# Alias for consistent naming
add_library(photon::diagnostics ALIAS photon_diagnostics)

# Tests
if(PHOTON_ENABLE_TESTING)
    add_subdirectory(tests)
endif()

# Benchmarks
if(PHOTON_ENABLE_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()