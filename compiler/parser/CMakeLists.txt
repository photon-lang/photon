# Parser Component

# Source files
set(PARSER_SOURCES
    src/parser.cpp
    src/ast.cpp
)

# Header files
set(PARSER_HEADERS
    include/photon/parser/parser.hpp
    include/photon/parser/ast.hpp
)

# Create parser library
add_library(photon_parser ${PARSER_SOURCES} ${PARSER_HEADERS})
add_library(photon::parser ALIAS photon_parser)

# Set target properties
set_target_properties(photon_parser PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    OUTPUT_NAME "photon_parser"
    EXPORT_NAME "parser"
)

# Include directories
target_include_directories(photon_parser
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Link dependencies
target_link_libraries(photon_parser
    PUBLIC
        photon::common
        photon::memory
        photon::diagnostics
        photon::lexer
)

# Compiler-specific options
target_compile_options(photon_parser PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

# Enable tests if available
if(PHOTON_ENABLE_TESTING AND GTest_FOUND)
    add_subdirectory(tests)
endif()

# Install targets (disabled for now to avoid export conflicts)
# install(TARGETS photon_parser
#     EXPORT PhotonParserTargets
#     LIBRARY DESTINATION lib
#     ARCHIVE DESTINATION lib
#     RUNTIME DESTINATION bin
#     INCLUDES DESTINATION include
# )
# 
# install(DIRECTORY include/
#     DESTINATION include
#     FILES_MATCHING PATTERN "*.hpp"
# )
# 
# install(EXPORT PhotonParserTargets
#     FILE PhotonParserTargets.cmake
#     NAMESPACE photon::
#     DESTINATION lib/cmake/photon
# )