# Source component tests

# Create test executable
add_executable(source_test 
    source_manager_test.cpp
)

target_link_libraries(source_test
    PRIVATE
    photon::source
    photon::memory
    photon::common
    photon::diagnostics
    GTest::gtest
    GTest::gtest_main
)

target_compile_options(source_test PRIVATE -Werror)

# Register test with CTest
add_test(NAME SourceTests COMMAND source_test)

# Set test properties
set_tests_properties(SourceTests PROPERTIES
    TIMEOUT 60
    LABELS "unit;source"
)