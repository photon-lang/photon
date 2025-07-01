/**
 * @file arena_impl.hpp
 * @brief Implementation details for MemoryArena template class
 * @author Photon Compiler Team
 * @version 1.0.0
 * 
 * This file contains the template implementation for the MemoryArena class.
 * Separated from the main header to improve compilation times.
 */

#pragma once

#include <stdexcept>
#include <new>
#include <bit>

namespace photon::memory {

template<usize BlockSize, usize Alignment>
MemoryArena<BlockSize, Alignment>::MemoryArena() noexcept
    : first_block_(nullptr)
    , current_block_(nullptr)
    , current_pos_(nullptr)
    , block_end_(nullptr)
    , total_allocated_(0)
    , bytes_used_(0)
    , block_count_(0) {
}

template<usize BlockSize, usize Alignment>
MemoryArena<BlockSize, Alignment>::~MemoryArena() noexcept {
    // Unique_ptr handles cleanup automatically through the chain
}

template<usize BlockSize, usize Alignment>
MemoryArena<BlockSize, Alignment>::MemoryArena(MemoryArena&& other) noexcept
    : first_block_(std::move(other.first_block_))
    , current_block_(other.current_block_)
    , current_pos_(other.current_pos_)
    , block_end_(other.block_end_)
    , total_allocated_(other.total_allocated_)
    , bytes_used_(other.bytes_used_)
    , block_count_(other.block_count_) {
    
    other.current_block_ = nullptr;
    other.current_pos_ = nullptr;
    other.block_end_ = nullptr;
    other.total_allocated_ = 0;
    other.bytes_used_ = 0;
    other.block_count_ = 0;
}

template<usize BlockSize, usize Alignment>
auto MemoryArena<BlockSize, Alignment>::operator=(MemoryArena&& other) noexcept -> MemoryArena& {
    if (this != &other) {
        // Release current resources
        first_block_.reset();
        
        // Move from other
        first_block_ = std::move(other.first_block_);
        current_block_ = other.current_block_;
        current_pos_ = other.current_pos_;
        block_end_ = other.block_end_;
        total_allocated_ = other.total_allocated_;
        bytes_used_ = other.bytes_used_;
        block_count_ = other.block_count_;
        
        // Reset other
        other.current_block_ = nullptr;
        other.current_pos_ = nullptr;
        other.block_end_ = nullptr;
        other.total_allocated_ = 0;
        other.bytes_used_ = 0;
        other.block_count_ = 0;
    }
    return *this;
}

template<usize BlockSize, usize Alignment>
auto MemoryArena<BlockSize, Alignment>::allocate(usize size, usize alignment) -> void* {
    if (size == 0) {
        throw std::invalid_argument("Cannot allocate zero bytes");
    }
    
    if (size > BlockSize) {
        throw std::invalid_argument("Allocation size exceeds block size");
    }
    
    if ((alignment & (alignment - 1)) != 0) {
        throw std::invalid_argument("Alignment must be power of 2");
    }
    
    // Ensure we have a current block
    if (!current_block_) {
        allocate_new_block();
    }
    
    // Align current position
    auto* aligned_pos = static_cast<u8*>(align_pointer(current_pos_, alignment));
    auto* allocation_end = aligned_pos + size;
    
    // Check if allocation fits in current block
    if (allocation_end > block_end_) {
        allocate_new_block();
        aligned_pos = static_cast<u8*>(align_pointer(current_pos_, alignment));
        allocation_end = aligned_pos + size;
        
        // Verify allocation fits in new block
        if (allocation_end > block_end_) {
            throw std::bad_alloc{};
        }
    }
    
    // Update allocation tracking
    current_pos_ = allocation_end;
    auto allocated_bytes = static_cast<usize>(allocation_end - aligned_pos);
    bytes_used_ += allocated_bytes;
    total_allocated_ += allocated_bytes;
    
    return aligned_pos;
}

template<usize BlockSize, usize Alignment>
template<typename T>
auto MemoryArena<BlockSize, Alignment>::allocate(usize count) -> T* {
    static_assert(ArenaAllocatable<T>, "Type T must be arena allocatable");
    
    if (count == 0) {
        throw std::invalid_argument("Cannot allocate zero objects");
    }
    
    // Check for overflow
    constexpr auto max_count = BlockSize / sizeof(T);
    if (count > max_count) {
        throw std::invalid_argument("Allocation count exceeds block capacity");
    }
    
    auto size = count * sizeof(T);
    auto alignment = alignof(T);
    
    return static_cast<T*>(allocate(size, alignment));
}

template<usize BlockSize, usize Alignment>
template<typename T, typename... Args>
auto MemoryArena<BlockSize, Alignment>::emplace(Args&&... args) -> T* {
    static_assert(ArenaAllocatable<T>, "Type T must be arena allocatable");
    
    auto* ptr = allocate<T>(1);
    
    try {
        return new (ptr) T(std::forward<Args>(args)...);
    } catch (...) {
        // Note: We can't easily deallocate in an arena, but the memory
        // will be reclaimed on reset() or destruction
        throw;
    }
}

template<usize BlockSize, usize Alignment>
auto MemoryArena<BlockSize, Alignment>::reset() noexcept -> void {
    if (!first_block_) {
        return;
    }
    
    // Keep the first block, deallocate the rest
    first_block_->next.reset();
    
    // Reset to beginning of first block
    current_block_ = first_block_.get();
    current_pos_ = current_block_->data;
    block_end_ = current_block_->data + BlockSize;
    
    // Reset counters
    bytes_used_ = 0;
    block_count_ = 1;
    // Note: total_allocated_ is cumulative and not reset
}

template<usize BlockSize, usize Alignment>
auto MemoryArena<BlockSize, Alignment>::total_allocated() const noexcept -> usize {
    return total_allocated_;
}

template<usize BlockSize, usize Alignment>
auto MemoryArena<BlockSize, Alignment>::bytes_used() const noexcept -> usize {
    return bytes_used_;
}

template<usize BlockSize, usize Alignment>
auto MemoryArena<BlockSize, Alignment>::block_count() const noexcept -> usize {
    return block_count_;
}

template<usize BlockSize, usize Alignment>
auto MemoryArena<BlockSize, Alignment>::owns(const void* ptr) const noexcept -> bool {
    if (!ptr || !first_block_) {
        return false;
    }
    
    auto* byte_ptr = static_cast<const u8*>(ptr);
    
    // Check each block
    for (auto* block = first_block_.get(); block; block = block->next.get()) {
        auto* block_start = block->data;
        auto* block_end = block->data + BlockSize;
        
        if (byte_ptr >= block_start && byte_ptr < block_end) {
            return true;
        }
    }
    
    return false;
}

template<usize BlockSize, usize Alignment>
auto MemoryArena<BlockSize, Alignment>::allocate_new_block() -> void {
    auto new_block = std::make_unique<Block>();
    auto* new_block_ptr = new_block.get();
    
    if (!first_block_) {
        // First block
        first_block_ = std::move(new_block);
    } else {
        // Link to current block
        current_block_->next = std::move(new_block);
    }
    
    // Update current block pointers
    current_block_ = new_block_ptr;
    current_pos_ = current_block_->data;
    block_end_ = current_block_->data + BlockSize;
    
    ++block_count_;
}

template<usize BlockSize, usize Alignment>
auto MemoryArena<BlockSize, Alignment>::align_pointer(void* ptr, usize alignment) noexcept -> void* {
    auto addr = reinterpret_cast<uintptr_t>(ptr);
    auto aligned_addr = (addr + alignment - 1) & ~(alignment - 1);
    return reinterpret_cast<void*>(aligned_addr);
}

template<usize BlockSize, usize Alignment>
auto MemoryArena<BlockSize, Alignment>::align_size(usize size, usize alignment) noexcept -> usize {
    return (size + alignment - 1) & ~(alignment - 1);
}

} // namespace photon::memory