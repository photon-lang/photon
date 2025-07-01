#include <iostream>
#include "photon/memory/arena.hpp"
#include "photon/common/types.hpp"

using namespace photon;
using namespace photon::memory;

struct TestAST {
    int type;
    String name;
    Vec<TestAST*> children;
    
    TestAST(int t, String n) : type(t), name(std::move(n)) {}
};

int main() {
    std::cout << "Photon Compiler - Memory Arena Test\n";
    
    try {
        // Test the memory arena
        MemoryArena<8192> arena;
        
        std::cout << "Creating AST nodes with arena allocator...\n";
        
        // Create some AST nodes
        auto* root = arena.emplace<TestAST>(1, "root");
        auto* child1 = arena.emplace<TestAST>(2, "function");
        auto* child2 = arena.emplace<TestAST>(3, "variable");
        
        root->children.push_back(child1);
        root->children.push_back(child2);
        
        std::cout << "Arena stats:\n";
        std::cout << "  Bytes used: " << arena.bytes_used() << "\n";
        std::cout << "  Block count: " << arena.block_count() << "\n";
        std::cout << "  Total allocated: " << arena.total_allocated() << "\n";
        
        // Test ownership
        std::cout << "Ownership test:\n";
        std::cout << "  Owns root: " << (arena.owns(root) ? "yes" : "no") << "\n";
        std::cout << "  Owns child1: " << (arena.owns(child1) ? "yes" : "no") << "\n";
        
        // Test reset
        arena.reset();
        std::cout << "After reset - bytes used: " << arena.bytes_used() << "\n";
        
        std::cout << "Memory arena test completed successfully!\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}