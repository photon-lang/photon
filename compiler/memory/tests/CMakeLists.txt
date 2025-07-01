# Memory component tests

# Create test executable
add_executable(memory_test arena_test.cpp)

target_link_libraries(memory_test
    PRIVATE
    photon::memory
    photon::common
    GTest::gtest
    GTest::gtest_main
)

target_compile_options(memory_test PRIVATE -Werror)

# Register test with CTest
add_test(NAME MemoryTests COMMAND memory_test)

# Set test properties
set_tests_properties(MemoryTests PROPERTIES
    TIMEOUT 30
    LABELS "unit;memory"
)