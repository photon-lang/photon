/**
 * @file arena.hpp
 * @brief High-performance arena allocator for AST nodes and compiler data structures
 * @author Photon Compiler Team
 * @version 1.0.0
 * 
 * This module provides a fast bump allocator for AST nodes with optimal cache
 * locality and minimal allocation overhead. Designed for compiler workloads
 * where allocation patterns are predominantly append-only.
 */

#pragma once

#include "photon/common/types.hpp"
#include <memory>
#include <new>
#include <concepts>

namespace photon::memory {

/**
 * @class MemoryArena
 * @brief Fast bump allocator with configurable block size and alignment
 * 
 * The MemoryArena class provides O(1) allocation with perfect cache locality
 * for compiler data structures. It maintains a linked list of memory blocks
 * and bumps the allocation pointer within the current block.
 * 
 * @tparam BlockSize Size of individual memory blocks (default 64KB)
 * @tparam Alignment Memory alignment requirement (default std::max_align_t)
 * 
 * @invariant All allocated memory is properly aligned
 * @invariant No individual allocation exceeds BlockSize
 * @invariant Arena state is consistent after any operation
 * 
 * @performance O(1) allocation time, O(1) reset time
 * @thread_safety Not thread-safe, use separate instances per thread
 * @memory_safety All allocations are tracked and freed on destruction
 */
template<usize BlockSize = 65536, usize Alignment = alignof(std::max_align_t)>
class MemoryArena {
public:
    static_assert(BlockSize >= 1024, "Block size must be at least 1KB");
    static_assert((Alignment & (Alignment - 1)) == 0, "Alignment must be power of 2");
    static_assert(Alignment <= BlockSize, "Alignment cannot exceed block size");

    /**
     * @brief Constructs an empty arena
     * @post Arena is ready for allocations
     */
    MemoryArena() noexcept;

    /**
     * @brief Destructor - frees all allocated blocks
     * @post All memory is released back to system
     */
    ~MemoryArena() noexcept;

    // Non-copyable but movable
    MemoryArena(const MemoryArena&) = delete;
    auto operator=(const MemoryArena&) -> MemoryArena& = delete;

    MemoryArena(MemoryArena&& other) noexcept;
    auto operator=(MemoryArena&& other) noexcept -> MemoryArena&;

    /**
     * @brief Allocates aligned memory of specified size
     * @param size Number of bytes to allocate
     * @param alignment Memory alignment requirement (must be power of 2)
     * @return Pointer to allocated memory, never null
     * 
     * @throws std::bad_alloc If system memory exhausted
     * @throws std::invalid_argument If size > BlockSize or alignment invalid
     * 
     * @pre size > 0 && size <= BlockSize
     * @pre alignment > 0 && (alignment & (alignment - 1)) == 0
     * @post Returned pointer is aligned to specified boundary
     * 
     * @complexity O(1) amortized, O(BlockSize) worst case for new block
     * @memory Allocates new block if current block insufficient
     */
    [[nodiscard]] auto allocate(usize size, usize alignment = Alignment) -> void*;

    /**
     * @brief Allocates memory for object of type T
     * @tparam T Type to allocate
     * @param count Number of objects to allocate
     * @return Typed pointer to allocated memory
     * 
     * @throws std::bad_alloc If allocation fails
     * @throws std::invalid_argument If allocation too large
     * 
     * @pre count > 0
     * @post Returned pointer is aligned for type T
     * 
     * @complexity O(1) amortized
     */
    template<typename T>
    [[nodiscard]] auto allocate(usize count = 1) -> T*;

    /**
     * @brief Constructs object in arena memory
     * @tparam T Type to construct
     * @tparam Args Constructor argument types
     * @param args Constructor arguments
     * @return Pointer to constructed object
     * 
     * @throws std::bad_alloc If allocation fails
     * @throws Any exception from T's constructor
     * 
     * @post Object is constructed and ready for use
     * 
     * @complexity O(1) + T's constructor complexity
     */
    template<typename T, typename... Args>
    [[nodiscard]] auto emplace(Args&&... args) -> T*;

    /**
     * @brief Resets arena to initial state, preserving first block
     * @post All previous allocations are invalidated
     * @post Arena is ready for new allocations
     * 
     * @complexity O(1) for single block, O(n) for n blocks
     * @memory Deallocates all blocks except the first
     */
    auto reset() noexcept -> void;

    /**
     * @brief Gets total number of bytes allocated by arena
     * @return Total allocation in bytes
     * @complexity O(1)
     */
    [[nodiscard]] auto total_allocated() const noexcept -> usize;

    /**
     * @brief Gets number of bytes used in current allocation cycle
     * @return Used bytes since construction or last reset
     * @complexity O(1)
     */
    [[nodiscard]] auto bytes_used() const noexcept -> usize;

    /**
     * @brief Gets number of allocated blocks
     * @return Block count
     * @complexity O(1)
     */
    [[nodiscard]] auto block_count() const noexcept -> usize;

    /**
     * @brief Checks if arena owns the given pointer
     * @param ptr Pointer to check
     * @return True if pointer was allocated by this arena
     * @complexity O(n) where n is number of blocks
     */
    [[nodiscard]] auto owns(const void* ptr) const noexcept -> bool;

private:
    struct Block {
        alignas(Alignment) u8 data[BlockSize];
        Ptr<Block> next;
        
        Block() noexcept : next(nullptr) {}
    };

    Ptr<Block> first_block_;
    Block* current_block_;
    u8* current_pos_;
    u8* block_end_;
    usize total_allocated_;
    usize bytes_used_;
    usize block_count_;

    auto allocate_new_block() -> void;
    auto align_pointer(void* ptr, usize alignment) noexcept -> void*;
    static auto align_size(usize size, usize alignment) noexcept -> usize;
};

/**
 * @concept ArenaAllocatable
 * @brief Concept for types that can be allocated in an arena
 */
template<typename T>
concept ArenaAllocatable = std::is_destructible_v<T> && 
                          (sizeof(T) <= 65536) &&
                          (alignof(T) <= alignof(std::max_align_t));

} // namespace photon::memory

#include "arena_impl.hpp"