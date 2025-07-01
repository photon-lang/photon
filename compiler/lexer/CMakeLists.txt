# Lexer component

# Create lexer library
add_library(photon_lexer STATIC
    src/token.cpp
    src/lexer.cpp
)

target_include_directories(photon_lexer PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(photon_lexer PUBLIC 
    photon::common
    photon::memory
    photon::diagnostics
    photon::source
)

target_compile_features(photon_lexer PUBLIC cxx_std_20)

# Alias for consistent naming
add_library(photon::lexer ALIAS photon_lexer)

# Tests
if(PHOTON_ENABLE_TESTING)
    add_subdirectory(tests)
endif()

# Benchmarks
if(PHOTON_ENABLE_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()