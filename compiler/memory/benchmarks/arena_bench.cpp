/**
 * @file arena_bench.cpp
 * @brief Performance benchmarks for MemoryArena allocator
 * @author Photon Compiler Team
 * @version 1.0.0
 * 
 * This benchmark suite measures the performance characteristics of the
 * MemoryArena allocator compared to standard allocation methods.
 */

#include "photon/memory/arena.hpp"
#include <benchmark/benchmark.h>
#include <memory>
#include <vector>
#include <random>

using namespace photon::memory;

namespace {

struct TestNode {
    int value;
    double data;
    TestNode* next;
    
    TestNode(int v = 0, double d = 0.0, TestNode* n = nullptr) 
        : value(v), data(d), next(n) {}
};

// Simulate compiler AST allocation patterns
template<typename Allocator>
void simulate_ast_allocation(benchmark::State& state, Allocator& alloc) {
    const auto num_nodes = state.range(0);
    
    for (auto _ : state) {
        std::vector<TestNode*> nodes;
        nodes.reserve(num_nodes);
        
        // Allocate nodes
        for (int i = 0; i < num_nodes; ++i) {
            auto* node = alloc.template allocate<TestNode>();
            new (node) TestNode(i, i * 2.0, nullptr);
            nodes.push_back(node);
        }
        
        // Link nodes (simulate AST construction)
        for (int i = 1; i < num_nodes; ++i) {
            nodes[i-1]->next = nodes[i];
        }
        
        // Traverse (simulate analysis pass)
        int sum = 0;
        for (auto* node = nodes[0]; node; node = node->next) {
            sum += node->value;
        }
        benchmark::DoNotOptimize(sum);
        
        // Reset for next iteration (arena) or individual deallocation (std)
        if constexpr (requires { alloc.reset(); }) {
            alloc.reset();
        }
    }
}

} // anonymous namespace

// Arena allocation benchmarks

static void BM_ArenaAllocation_Small(benchmark::State& state) {
    MemoryArena<4096> arena;
    const auto size = state.range(0);
    
    for (auto _ : state) {
        auto* ptr = arena.allocate(size);
        benchmark::DoNotOptimize(ptr);
        
        if (state.range(0) == 1) { // Reset only for single byte to avoid overhead
            arena.reset();
        }
    }
    
    state.SetBytesProcessed(int64_t(state.iterations()) * int64_t(state.range(0)));
}
BENCHMARK(BM_ArenaAllocation_Small)->Range(1, 1024);

static void BM_ArenaAllocation_TypedSingle(benchmark::State& state) {
    MemoryArena<65536> arena;
    
    for (auto _ : state) {
        auto* ptr = arena.allocate<TestNode>();
        benchmark::DoNotOptimize(ptr);
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_ArenaAllocation_TypedSingle);

static void BM_ArenaAllocation_TypedBatch(benchmark::State& state) {
    MemoryArena<65536> arena;
    const auto count = state.range(0);
    
    for (auto _ : state) {
        auto* ptr = arena.allocate<TestNode>(count);
        benchmark::DoNotOptimize(ptr);
        arena.reset();
    }
    
    state.SetItemsProcessed(int64_t(state.iterations()) * int64_t(count));
}
BENCHMARK(BM_ArenaAllocation_TypedBatch)->Range(1, 1000);

static void BM_ArenaEmplace(benchmark::State& state) {
    MemoryArena<65536> arena;
    
    for (auto _ : state) {
        auto* node = arena.emplace<TestNode>(42, 3.14, nullptr);
        benchmark::DoNotOptimize(node);
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_ArenaEmplace);

static void BM_ArenaReset(benchmark::State& state) {
    MemoryArena<65536> arena;
    const auto num_allocs = state.range(0);
    
    // Pre-allocate to measure reset time
    for (int i = 0; i < num_allocs; ++i) {
        arena.allocate<TestNode>();
    }
    
    for (auto _ : state) {
        // Make some allocations
        for (int i = 0; i < 100; ++i) {
            arena.allocate<TestNode>();
        }
        
        auto start = std::chrono::high_resolution_clock::now();
        arena.reset();
        auto end = std::chrono::high_resolution_clock::now();
        
        auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        state.SetIterationTime(elapsed.count() / 1e9);
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_ArenaReset)->Range(100, 10000)->UseManualTime();

// Comparison with standard allocation

static void BM_StdAllocation_Single(benchmark::State& state) {
    for (auto _ : state) {
        auto ptr = std::make_unique<TestNode>();
        benchmark::DoNotOptimize(ptr);
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_StdAllocation_Single);

static void BM_StdAllocation_Vector(benchmark::State& state) {
    const auto count = state.range(0);
    
    for (auto _ : state) {
        std::vector<std::unique_ptr<TestNode>> nodes;
        nodes.reserve(count);
        
        for (int i = 0; i < count; ++i) {
            nodes.push_back(std::make_unique<TestNode>(i, i * 2.0, nullptr));
        }
        
        benchmark::DoNotOptimize(nodes);
    }
    
    state.SetItemsProcessed(int64_t(state.iterations()) * int64_t(count));
}
BENCHMARK(BM_StdAllocation_Vector)->Range(10, 1000);

static void BM_MallocFree(benchmark::State& state) {
    const auto size = state.range(0);
    
    for (auto _ : state) {
        auto* ptr = std::malloc(size);
        benchmark::DoNotOptimize(ptr);
        std::free(ptr);
    }
    
    state.SetBytesProcessed(int64_t(state.iterations()) * int64_t(size));
}
BENCHMARK(BM_MallocFree)->Range(8, 1024);

// AST simulation benchmarks

static void BM_ArenaASTSimulation(benchmark::State& state) {
    MemoryArena<1048576> arena; // 1MB arena for large ASTs
    simulate_ast_allocation(state, arena);
    
    state.SetItemsProcessed(int64_t(state.iterations()) * int64_t(state.range(0)));
}
BENCHMARK(BM_ArenaASTSimulation)->Range(100, 10000);

static void BM_StdASTSimulation(benchmark::State& state) {
    struct StdAllocator {
        template<typename T>
        T* allocate() {
            return new T();
        }
    };
    
    StdAllocator alloc;
    const auto num_nodes = state.range(0);
    
    for (auto _ : state) {
        std::vector<std::unique_ptr<TestNode>> nodes;
        nodes.reserve(num_nodes);
        
        // Allocate nodes
        for (int i = 0; i < num_nodes; ++i) {
            auto node = std::make_unique<TestNode>(i, i * 2.0, nullptr);
            nodes.push_back(std::move(node));
        }
        
        // Link nodes
        for (int i = 1; i < num_nodes; ++i) {
            nodes[i-1]->next = nodes[i].get();
        }
        
        // Traverse
        int sum = 0;
        for (auto* node = nodes[0].get(); node; node = node->next) {
            sum += node->value;
        }
        benchmark::DoNotOptimize(sum);
        
        // Cleanup handled by unique_ptr destructors
    }
    
    state.SetItemsProcessed(int64_t(state.iterations()) * int64_t(state.range(0)));
}
BENCHMARK(BM_StdASTSimulation)->Range(100, 10000);

// Memory usage benchmarks

static void BM_ArenaMemoryEfficiency(benchmark::State& state) {
    MemoryArena<65536> arena;
    const auto num_allocs = state.range(0);
    
    for (auto _ : state) {
        auto initial_used = arena.bytes_used();
        
        for (int i = 0; i < num_allocs; ++i) {
            arena.allocate<TestNode>();
        }
        
        auto final_used = arena.bytes_used();
        auto bytes_per_alloc = (final_used - initial_used) / num_allocs;
        
        state.counters["BytesPerAlloc"] = bytes_per_alloc;
        state.counters["Efficiency"] = double(sizeof(TestNode)) / double(bytes_per_alloc);
        
        arena.reset();
    }
    
    state.SetItemsProcessed(int64_t(state.iterations()) * int64_t(num_allocs));
}
BENCHMARK(BM_ArenaMemoryEfficiency)->Range(10, 1000);

// Alignment benchmarks

static void BM_ArenaAlignment_Default(benchmark::State& state) {
    MemoryArena<65536> arena;
    
    for (auto _ : state) {
        auto* ptr = arena.allocate(state.range(0));
        benchmark::DoNotOptimize(ptr);
        
        // Verify alignment
        auto addr = reinterpret_cast<uintptr_t>(ptr);
        if (addr % alignof(std::max_align_t) != 0) {
            state.SkipWithError("Alignment violation");
        }
    }
    
    state.SetBytesProcessed(int64_t(state.iterations()) * int64_t(state.range(0)));
}
BENCHMARK(BM_ArenaAlignment_Default)->Range(1, 1024);

static void BM_ArenaAlignment_Custom(benchmark::State& state) {
    MemoryArena<65536> arena;
    const auto alignment = state.range(0);
    
    for (auto _ : state) {
        auto* ptr = arena.allocate(32, alignment);
        benchmark::DoNotOptimize(ptr);
        
        // Verify alignment
        auto addr = reinterpret_cast<uintptr_t>(ptr);
        if (addr % alignment != 0) {
            state.SkipWithError("Alignment violation");
        }
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_ArenaAlignment_Custom)->RangeMultiplier(2)->Range(1, 64);

// Multi-block benchmarks

static void BM_ArenaMultiBlock(benchmark::State& state) {
    MemoryArena<4096> arena; // Small blocks to force multiple blocks
    const auto total_size = state.range(0);
    const auto alloc_size = 512;
    const auto num_allocs = total_size / alloc_size;
    
    for (auto _ : state) {
        for (int i = 0; i < num_allocs; ++i) {
            auto* ptr = arena.allocate(alloc_size);
            benchmark::DoNotOptimize(ptr);
        }
        
        state.counters["BlockCount"] = arena.block_count();
        arena.reset();
    }
    
    state.SetBytesProcessed(int64_t(state.iterations()) * int64_t(total_size));
}
BENCHMARK(BM_ArenaMultiBlock)->Range(8192, 1048576);

BENCHMARK_MAIN();