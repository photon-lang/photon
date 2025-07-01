/**
 * @file arena_test.cpp
 * @brief Comprehensive test suite for MemoryArena allocator
 * @author Photon Compiler Team
 * @version 1.0.0
 * 
 * This test suite validates all aspects of the MemoryArena implementation
 * including edge cases, error conditions, and performance characteristics.
 */

#include "photon/memory/arena.hpp"
#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <random>
#include <chrono>

using namespace photon::memory;

namespace {

struct TestObject {
    int value;
    double data;
    
    TestObject() : value(0), data(0.0) {}
    TestObject(int v, double d) : value(v), data(d) {}
    
    bool operator==(const TestObject& other) const {
        return value == other.value && data == other.data;
    }
};

struct AlignedObject {
    alignas(64) char data[64];
    
    AlignedObject() { 
        std::fill(std::begin(data), std::end(data), 'A'); 
    }
};

} // anonymous namespace

class ArenaTest : public ::testing::Test {
protected:
    void SetUp() override {
        arena = std::make_unique<MemoryArena<4096>>();
    }
    
    void TearDown() override {
        arena.reset();
    }
    
    std::unique_ptr<MemoryArena<4096>> arena;
};

// Basic functionality tests

TEST_F(ArenaTest, DefaultConstructorCreatesEmptyArena) {
    EXPECT_EQ(arena->bytes_used(), 0u);
    EXPECT_EQ(arena->block_count(), 0u);
    EXPECT_EQ(arena->total_allocated(), 0u);
}

TEST_F(ArenaTest, AllocateBasicTypes) {
    auto* int_ptr = arena->allocate<int>();
    ASSERT_NE(int_ptr, nullptr);
    EXPECT_TRUE(arena->owns(int_ptr));
    EXPECT_EQ(arena->block_count(), 1u);
    EXPECT_GE(arena->bytes_used(), sizeof(int));
    
    *int_ptr = 42;
    EXPECT_EQ(*int_ptr, 42);
    
    auto* double_ptr = arena->allocate<double>();
    ASSERT_NE(double_ptr, nullptr);
    EXPECT_TRUE(arena->owns(double_ptr));
    
    *double_ptr = 3.14;
    EXPECT_EQ(*double_ptr, 3.14);
}

TEST_F(ArenaTest, AllocateMultipleObjects) {
    constexpr size_t count = 10;
    auto* array = arena->allocate<int>(count);
    ASSERT_NE(array, nullptr);
    
    for (size_t i = 0; i < count; ++i) {
        array[i] = static_cast<int>(i);
    }
    
    for (size_t i = 0; i < count; ++i) {
        EXPECT_EQ(array[i], static_cast<int>(i));
    }
}

TEST_F(ArenaTest, EmplaceObject) {
    auto* obj = arena->emplace<TestObject>(42, 3.14);
    ASSERT_NE(obj, nullptr);
    EXPECT_EQ(obj->value, 42);
    EXPECT_EQ(obj->data, 3.14);
    EXPECT_TRUE(arena->owns(obj));
}

TEST_F(ArenaTest, AllocateRawMemory) {
    constexpr size_t size = 128;
    auto* ptr = arena->allocate(size);
    ASSERT_NE(ptr, nullptr);
    EXPECT_TRUE(arena->owns(ptr));
    
    // Verify we can write to the memory
    std::memset(ptr, 0xAB, size);
    auto* byte_ptr = static_cast<unsigned char*>(ptr);
    for (size_t i = 0; i < size; ++i) {
        EXPECT_EQ(byte_ptr[i], 0xABu);
    }
}

// Alignment tests

TEST_F(ArenaTest, DefaultAlignmentIsRespected) {
    auto* ptr = arena->allocate(1);
    auto addr = reinterpret_cast<uintptr_t>(ptr);
    EXPECT_EQ(addr % alignof(std::max_align_t), 0u);
}

TEST_F(ArenaTest, CustomAlignmentIsRespected) {
    constexpr size_t alignment = 64;
    auto* ptr = arena->allocate(1, alignment);
    auto addr = reinterpret_cast<uintptr_t>(ptr);
    EXPECT_EQ(addr % alignment, 0u);
}

TEST_F(ArenaTest, AlignedObjectAllocation) {
    auto* obj = arena->allocate<AlignedObject>();
    auto addr = reinterpret_cast<uintptr_t>(obj);
    EXPECT_EQ(addr % 64, 0u);
}

// Block management tests

TEST_F(ArenaTest, MultipleBlockAllocation) {
    constexpr size_t block_size = 4096;
    constexpr size_t allocation_size = 1024;
    constexpr size_t num_allocations = 6; // Should require 2 blocks
    
    std::vector<void*> ptrs;
    
    for (size_t i = 0; i < num_allocations; ++i) {
        auto* ptr = arena->allocate(allocation_size);
        ASSERT_NE(ptr, nullptr);
        ptrs.push_back(ptr);
        EXPECT_TRUE(arena->owns(ptr));
    }
    
    EXPECT_GE(arena->block_count(), 2u);
    
    // Verify all pointers are still valid and owned
    for (auto* ptr : ptrs) {
        EXPECT_TRUE(arena->owns(ptr));
    }
}

TEST_F(ArenaTest, ResetPreservesFirstBlock) {
    // Allocate enough to require multiple blocks
    for (int i = 0; i < 10; ++i) {
        arena->allocate(512);
    }
    
    auto initial_block_count = arena->block_count();
    auto initial_total = arena->total_allocated();
    
    arena->reset();
    
    EXPECT_EQ(arena->bytes_used(), 0u);
    EXPECT_EQ(arena->block_count(), std::min(initial_block_count, 1u));
    EXPECT_EQ(arena->total_allocated(), initial_total); // Total is cumulative
    
    // Should be able to allocate again
    auto* ptr = arena->allocate<int>();
    ASSERT_NE(ptr, nullptr);
    EXPECT_TRUE(arena->owns(ptr));
}

// Error condition tests

TEST_F(ArenaTest, ZeroSizeAllocationThrows) {
    EXPECT_THROW(arena->allocate(0), std::invalid_argument);
}

TEST_F(ArenaTest, OversizedAllocationThrows) {
    constexpr size_t block_size = 4096;
    EXPECT_THROW(arena->allocate(block_size + 1), std::invalid_argument);
}

TEST_F(ArenaTest, InvalidAlignmentThrows) {
    EXPECT_THROW(arena->allocate(1, 3), std::invalid_argument); // Not power of 2
    EXPECT_THROW(arena->allocate(1, 0), std::invalid_argument); // Zero alignment
}

TEST_F(ArenaTest, ZeroCountAllocationThrows) {
    EXPECT_THROW(arena->allocate<int>(0), std::invalid_argument);
}

TEST_F(ArenaTest, OversizedCountAllocationThrows) {
    constexpr size_t max_count = 4096 / sizeof(int);
    EXPECT_THROW(arena->allocate<int>(max_count + 1), std::invalid_argument);
}

// Ownership tests

TEST_F(ArenaTest, OwnershipDetection) {
    auto* ptr1 = arena->allocate<int>();
    auto* ptr2 = arena->allocate<double>();
    
    EXPECT_TRUE(arena->owns(ptr1));
    EXPECT_TRUE(arena->owns(ptr2));
    
    // Test with external pointer
    int external_int = 42;
    EXPECT_FALSE(arena->owns(&external_int));
    
    // Test with nullptr
    EXPECT_FALSE(arena->owns(nullptr));
}

TEST_F(ArenaTest, OwnershipAcrossBlocks) {
    std::vector<void*> ptrs;
    
    // Allocate across multiple blocks
    for (int i = 0; i < 20; ++i) {
        ptrs.push_back(arena->allocate(256));
    }
    
    // All should be owned
    for (auto* ptr : ptrs) {
        EXPECT_TRUE(arena->owns(ptr));
    }
}

// Move semantics tests

TEST_F(ArenaTest, MoveConstruction) {
    // Allocate some memory
    auto* ptr1 = arena->allocate<int>();
    auto* ptr2 = arena->allocate<double>();
    *ptr1 = 42;
    *ptr2 = 3.14;
    
    auto bytes_used = arena->bytes_used();
    auto block_count = arena->block_count();
    auto total_allocated = arena->total_allocated();
    
    // Move construct
    auto moved_arena = std::move(*arena);
    
    // Original arena should be empty
    EXPECT_EQ(arena->bytes_used(), 0u);
    EXPECT_EQ(arena->block_count(), 0u);
    EXPECT_EQ(arena->total_allocated(), 0u);
    EXPECT_FALSE(arena->owns(ptr1));
    
    // Moved arena should have the data
    EXPECT_EQ(moved_arena.bytes_used(), bytes_used);
    EXPECT_EQ(moved_arena.block_count(), block_count);
    EXPECT_EQ(moved_arena.total_allocated(), total_allocated);
    EXPECT_TRUE(moved_arena.owns(ptr1));
    EXPECT_TRUE(moved_arena.owns(ptr2));
    
    // Values should be preserved
    EXPECT_EQ(*ptr1, 42);
    EXPECT_EQ(*ptr2, 3.14);
}

TEST_F(ArenaTest, MoveAssignment) {
    // Set up source arena
    auto* ptr = arena->allocate<int>();
    *ptr = 42;
    
    // Set up destination arena
    MemoryArena<4096> dest_arena;
    auto* dest_ptr = dest_arena.allocate<double>();
    *dest_ptr = 3.14;
    
    auto bytes_used = arena->bytes_used();
    
    // Move assign
    dest_arena = std::move(*arena);
    
    // Source should be empty
    EXPECT_EQ(arena->bytes_used(), 0u);
    EXPECT_FALSE(arena->owns(ptr));
    
    // Destination should have source data
    EXPECT_EQ(dest_arena.bytes_used(), bytes_used);
    EXPECT_TRUE(dest_arena.owns(ptr));
    EXPECT_FALSE(dest_arena.owns(dest_ptr)); // Old data should be gone
    EXPECT_EQ(*ptr, 42);
}

// Performance and memory usage tests

TEST_F(ArenaTest, AllocationPerformance) {
    constexpr int num_allocations = 10000;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < num_allocations; ++i) {
        arena->allocate<TestObject>();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    // Should be very fast - less than 1ms for 10k allocations
    EXPECT_LT(duration.count(), 1000);
    
    EXPECT_EQ(arena->bytes_used(), num_allocations * sizeof(TestObject));
}

TEST_F(ArenaTest, ResetPerformance) {
    // Allocate across multiple blocks
    for (int i = 0; i < 1000; ++i) {
        arena->allocate(100);
    }
    
    auto start = std::chrono::high_resolution_clock::now();
    arena->reset();
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    // Reset should be very fast
    EXPECT_LT(duration.count(), 100);
    EXPECT_EQ(arena->bytes_used(), 0u);
}

// Edge case tests

TEST_F(ArenaTest, SingleByteAllocations) {
    constexpr int count = 100;
    std::vector<void*> ptrs;
    
    for (int i = 0; i < count; ++i) {
        auto* ptr = arena->allocate(1);
        ASSERT_NE(ptr, nullptr);
        ptrs.push_back(ptr);
        
        // Write to verify it's writable
        *static_cast<char*>(ptr) = static_cast<char>(i % 256);
    }
    
    // Verify all allocations
    for (size_t i = 0; i < ptrs.size(); ++i) {
        EXPECT_TRUE(arena->owns(ptrs[i]));
        EXPECT_EQ(*static_cast<char*>(ptrs[i]), static_cast<char>(i % 256));
    }
}

TEST_F(ArenaTest, MaxSizeAllocation) {
    constexpr size_t max_size = 4096 - sizeof(std::max_align_t); // Leave room for alignment
    auto* ptr = arena->allocate(max_size);
    ASSERT_NE(ptr, nullptr);
    EXPECT_TRUE(arena->owns(ptr));
    
    // Should be able to write to all of it
    std::memset(ptr, 0xFF, max_size);
}

// Stress tests

TEST_F(ArenaTest, MixedAllocationSizes) {
    std::mt19937 rng(42);
    std::uniform_int_distribution<size_t> size_dist(1, 512);
    std::vector<std::pair<void*, size_t>> allocations;
    
    for (int i = 0; i < 1000; ++i) {
        auto size = size_dist(rng);
        auto* ptr = arena->allocate(size);
        ASSERT_NE(ptr, nullptr);
        
        allocations.emplace_back(ptr, size);
        
        // Fill with pattern
        std::memset(ptr, i % 256, size);
    }
    
    // Verify all allocations
    for (size_t i = 0; i < allocations.size(); ++i) {
        auto [ptr, size] = allocations[i];
        EXPECT_TRUE(arena->owns(ptr));
        
        // Verify pattern
        auto* byte_ptr = static_cast<unsigned char*>(ptr);
        for (size_t j = 0; j < size; ++j) {
            EXPECT_EQ(byte_ptr[j], static_cast<unsigned char>(i % 256));
        }
    }
}

// Custom block size tests

TEST(ArenaCustomSizeTest, SmallBlockSize) {
    MemoryArena<1024> small_arena;
    
    // Should be able to allocate within block
    auto* ptr1 = small_arena.allocate(512);
    ASSERT_NE(ptr1, nullptr);
    EXPECT_EQ(small_arena.block_count(), 1u);
    
    // Should trigger new block
    auto* ptr2 = small_arena.allocate(512);
    ASSERT_NE(ptr2, nullptr);
    EXPECT_GE(small_arena.block_count(), 2u);
    
    EXPECT_TRUE(small_arena.owns(ptr1));
    EXPECT_TRUE(small_arena.owns(ptr2));
}

TEST(ArenaCustomSizeTest, LargeBlockSize) {
    MemoryArena<1048576> large_arena; // 1MB blocks
    
    // Should fit many allocations in single block
    std::vector<void*> ptrs;
    for (int i = 0; i < 1000; ++i) {
        ptrs.push_back(large_arena.allocate(1024));
    }
    
    EXPECT_EQ(large_arena.block_count(), 1u);
    
    for (auto* ptr : ptrs) {
        EXPECT_TRUE(large_arena.owns(ptr));
    }
}