# Lexer component tests

# Create test executable
add_executable(lexer_test 
    lexer_test.cpp
)

target_link_libraries(lexer_test
    PRIVATE
    photon::lexer
    photon::source
    photon::memory
    photon::common
    photon::diagnostics
    GTest::gtest
    GTest::gtest_main
)

target_compile_options(lexer_test PRIVATE -Werror)

# Register test with CTest
add_test(NAME LexerTests COMMAND lexer_test)

# Set test properties
set_tests_properties(LexerTests PROPERTIES
    TIMEOUT 120
    LABELS "unit;lexer"
)