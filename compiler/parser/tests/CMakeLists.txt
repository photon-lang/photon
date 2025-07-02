# Parser Tests

set(PARSER_TEST_SOURCES
    test_ast.cpp
    test_parser.cpp
    test_expressions.cpp
    test_statements.cpp
    test_declarations.cpp
    test_error_recovery.cpp
)

# Create test executable
add_executable(parser_tests ${PARSER_TEST_SOURCES})

# Link dependencies
target_link_libraries(parser_tests
    PRIVATE
        photon::parser
        photon::lexer
        photon::diagnostics
        photon::memory
        photon::common
        GTest::gtest
        GTest::gtest_main
)

# Set target properties
set_target_properties(parser_tests PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# Include directories for tests
target_include_directories(parser_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Register tests with CTest
include(GoogleTest)
gtest_discover_tests(parser_tests)