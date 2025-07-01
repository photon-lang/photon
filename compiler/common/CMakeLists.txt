# Common utilities and type definitions

# Create common library
add_library(photon_common INTERFACE)
target_include_directories(photon_common INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(photon_common INTERFACE cxx_std_20)

# Alias for consistent naming
add_library(photon::common ALIAS photon_common)