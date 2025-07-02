#include <iostream>
#include <fstream>
#include <sstream>
#include "photon/memory/arena.hpp"
#include "photon/source/source_manager.hpp"
#include "photon/lexer/lexer.hpp"
#include "photon/parser/parser.hpp"
#include "photon/common/types.hpp"

using namespace photon;
using namespace photon::memory;
using namespace photon::source;
using namespace photon::lexer;
using namespace photon::parser;

class SimpleASTVisitor : public ASTVisitor {
public:
    explicit SimpleASTVisitor(int indent = 0) : indent_(indent) {}
    
    void visit_program(Program& node) override {
        print_indent();
        std::cout << "Program:\n";
        for (auto& decl : node.declarations_mut()) {
            SimpleASTVisitor visitor(indent_ + 2);
            decl->accept(visitor);
        }
    }
    
    void visit_program(const Program& node) override {
        print_indent();
        std::cout << "Program:\n";
        for (const auto& decl : node.declarations()) {
            SimpleASTVisitor visitor(indent_ + 2);
            decl->accept(visitor);
        }
    }
    
    void visit_function_decl(FunctionDecl& node) override {
        print_indent();
        std::cout << "Function: " << node.name() << "(";
        for (usize i = 0; i < node.parameters().size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << node.parameters()[i].name;
        }
        std::cout << ")\n";
        
        SimpleASTVisitor visitor(indent_ + 2);
        node.body().accept(visitor);
    }
    
    void visit_function_decl(const FunctionDecl& node) override {
        print_indent();
        std::cout << "Function: " << node.name() << "(";
        for (usize i = 0; i < node.parameters().size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << node.parameters()[i].name;
        }
        std::cout << ")\n";
        
        SimpleASTVisitor visitor(indent_ + 2);
        node.body().accept(visitor);
    }
    
    void visit_block(Block& node) override {
        print_indent();
        std::cout << "Block {\n";
        for (auto& stmt : node.statements_mut()) {
            SimpleASTVisitor visitor(indent_ + 2);
            stmt->accept(visitor);
        }
        print_indent();
        std::cout << "}\n";
    }
    
    void visit_block(const Block& node) override {
        print_indent();
        std::cout << "Block {\n";
        for (const auto& stmt : node.statements()) {
            SimpleASTVisitor visitor(indent_ + 2);
            stmt->accept(visitor);
        }
        print_indent();
        std::cout << "}\n";
    }
    
    void visit_var_decl(VarDecl& node) override {
        print_indent();
        std::cout << "VarDecl: " << (node.is_mutable() ? "mut " : "") << node.name();
        if (node.init()) {
            std::cout << " = ";
            SimpleASTVisitor visitor(0);
            node.init()->accept(visitor);
        }
        std::cout << "\n";
    }
    
    void visit_var_decl(const VarDecl& node) override {
        print_indent();
        std::cout << "VarDecl: " << (node.is_mutable() ? "mut " : "") << node.name();
        if (node.init()) {
            std::cout << " = ";
            SimpleASTVisitor visitor(0);
            node.init()->accept(visitor);
        }
        std::cout << "\n";
    }
    
    void visit_integer_literal(IntegerLiteral& node) override {
        std::cout << node.value();
    }
    
    void visit_integer_literal(const IntegerLiteral& node) override {
        std::cout << node.value();
    }
    
    void visit_binary_expr(BinaryExpr& node) override {
        std::cout << "(";
        SimpleASTVisitor visitor(0);
        node.left_mut().accept(visitor);
        
        switch (node.get_operator()) {
            case BinaryExpr::Operator::Add: std::cout << " + "; break;
            case BinaryExpr::Operator::Sub: std::cout << " - "; break;
            case BinaryExpr::Operator::Mul: std::cout << " * "; break;
            case BinaryExpr::Operator::Div: std::cout << " / "; break;
            default: std::cout << " OP "; break;
        }
        
        node.right_mut().accept(visitor);
        std::cout << ")";
    }
    
    void visit_binary_expr(const BinaryExpr& node) override {
        std::cout << "(";
        SimpleASTVisitor visitor(0);
        node.left().accept(visitor);
        
        switch (node.get_operator()) {
            case BinaryExpr::Operator::Add: std::cout << " + "; break;
            case BinaryExpr::Operator::Sub: std::cout << " - "; break;
            case BinaryExpr::Operator::Mul: std::cout << " * "; break;
            case BinaryExpr::Operator::Div: std::cout << " / "; break;
            default: std::cout << " OP "; break;
        }
        
        node.right().accept(visitor);
        std::cout << ")";
    }
    
    void visit_identifier(Identifier& node) override {
        std::cout << node.name();
    }
    
    void visit_identifier(const Identifier& node) override {
        std::cout << node.name();
    }
    
    // Required virtual methods (stub implementations)
    void visit_float_literal(FloatLiteral&) override {}
    void visit_float_literal(const FloatLiteral&) override {}
    void visit_string_literal(StringLiteral&) override {}
    void visit_string_literal(const StringLiteral&) override {}
    void visit_bool_literal(BoolLiteral&) override {}
    void visit_bool_literal(const BoolLiteral&) override {}
    void visit_unary_expr(UnaryExpr&) override {}
    void visit_unary_expr(const UnaryExpr&) override {}
    void visit_call_expr(CallExpr&) override {}
    void visit_call_expr(const CallExpr&) override {}

private:
    int indent_;
    
    void print_indent() {
        for (int i = 0; i < indent_; ++i) {
            std::cout << " ";
        }
    }
};

auto load_file(const std::string& filename) -> std::string {
    std::ifstream file(filename);
    if (!file) {
        throw std::runtime_error("Could not open file: " + filename);
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

auto test_parser(const std::string& source_code, const std::string& description) -> void {
    std::cout << "\n=== " << description << " ===\n";
    std::cout << "Source:\n" << source_code << "\n\n";
    
    try {
        // Setup
        MemoryArena<> arena;
        SourceManager source_mgr(arena);
        
        // Load source
        auto file_id_result = source_mgr.load_from_string("test.ph", source_code);
        if (!file_id_result) {
            std::cout << "❌ Failed to load source\n";
            return;
        }
        
        // Tokenize
        Lexer lexer(source_mgr, arena);
        auto tokens_result = lexer.tokenize(file_id_result.value());
        if (!tokens_result) {
            std::cout << "❌ Failed to tokenize\n";
            return;
        }
        
        std::cout << "✅ Tokenization successful\n";
        
        // Parse
        Parser parser(std::move(tokens_result.value()), arena);
        auto program_result = parser.parse_program();
        
        if (!program_result) {
            std::cout << "❌ Failed to parse\n";
            if (parser.has_errors()) {
                std::cout << "Errors: " << parser.errors().size() << "\n";
            }
            return;
        }
        
        std::cout << "✅ Parsing successful\n";
        
        // Print AST
        std::cout << "\nAST:\n";
        SimpleASTVisitor visitor;
        program_result.value()->accept(visitor);
        
    } catch (const std::exception& e) {
        std::cout << "❌ Exception: " << e.what() << "\n";
    }
}

int main(int argc, char* argv[]) {
    std::cout << "🚀 Photon Compiler - Parser Demo\n";
    std::cout << "================================\n";
    
    if (argc > 1) {
        // Parse file from command line
        try {
            auto source = load_file(argv[1]);
            test_parser(source, "File: " + std::string(argv[1]));
        } catch (const std::exception& e) {
            std::cerr << "Error loading file: " << e.what() << std::endl;
            return 1;
        }
    } else {
        // Run built-in tests
        
        // Test 1: Simple expression
        test_parser("42", "Integer Literal");
        
        // Test 2: Binary expression
        test_parser("1 + 2", "Binary Expression");
        
        // Test 3: Variable declaration
        test_parser("let x = 42", "Variable Declaration");
        
        // Test 4: Simple function
        test_parser(R"(
fn main() {
    let x = 42
})", "Simple Function");
        
        // Test 5: Function with parameters
        test_parser(R"(
fn add(a: i32, b: i32) -> i32 {
    let result = a + b
})", "Function with Parameters");
        
        // Test 6: Multiple functions
        test_parser(R"(
fn main() {
    let x = 10
    let y = 20
}

fn helper() {
    let z = 30
})", "Multiple Functions");
    }
    
    return 0;
}