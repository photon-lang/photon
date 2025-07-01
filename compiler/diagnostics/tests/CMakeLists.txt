# Diagnostics component tests

# Create test executable
add_executable(diagnostics_test 
    source_location_test.cpp
    diagnostic_test.cpp
    formatter_test.cpp
)

target_link_libraries(diagnostics_test
    PRIVATE
    photon::diagnostics
    photon::memory
    photon::common
    GTest::gtest
    GTest::gtest_main
)

target_compile_options(diagnostics_test PRIVATE -Werror)

# Register test with CTest
add_test(NAME DiagnosticsTests COMMAND diagnostics_test)

# Set test properties
set_tests_properties(DiagnosticsTests PROPERTIES
    TIMEOUT 60
    LABELS "unit;diagnostics"
)