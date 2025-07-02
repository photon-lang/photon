/**
 * @file test_parser.cpp
 * @brief Basic parser functionality tests
 */

#include <gtest/gtest.h>
#include "photon/parser/parser.hpp"
#include "photon/lexer/lexer.hpp"
#include "photon/memory/arena.hpp"
#include "photon/source/source_manager.hpp"

using namespace photon::parser;
using namespace photon::lexer;
using namespace photon::memory;
using namespace photon::source;

class ParserTest : public ::testing::Test {
protected:
    void SetUp() override {
        arena = std::make_unique<MemoryArena<>>();
        source_mgr = std::make_unique<SourceManager>(*arena);
    }
    
    auto parse_from_string(const std::string& source) -> std::unique_ptr<Parser> {
        auto file_id_result = source_mgr->load_from_string("test.ph", source);
        EXPECT_TRUE(file_id_result.has_value());
        
        auto file_id = file_id_result.value();
        auto* file = source_mgr->get_file(file_id);
        EXPECT_NE(file, nullptr);
        
        auto lexer = Lexer(*source_mgr, *arena);
        
        auto tokens_result = lexer.tokenize(file_id);
        EXPECT_TRUE(tokens_result.has_value());
        
        auto token_stream = std::move(tokens_result.value());
        return std::make_unique<Parser>(std::move(token_stream), *arena);
    }

    std::unique_ptr<MemoryArena<>> arena;
    std::unique_ptr<SourceManager> source_mgr;
};

// === Basic Parser Creation ===

TEST_F(ParserTest, ParserCreation) {
    auto parser = parse_from_string("");
    EXPECT_FALSE(parser->has_errors());
}

// === Program Parsing ===

TEST_F(ParserTest, EmptyProgram) {
    auto parser = parse_from_string("");
    auto program_result = parser->parse_program();
    
    EXPECT_TRUE(program_result.has_value());
    EXPECT_FALSE(parser->has_errors());
    
    auto& program = program_result.value();
    EXPECT_EQ(program->kind(), ASTNode::Kind::Program);
    EXPECT_TRUE(program->declarations().empty());
}

TEST_F(ParserTest, SingleFunctionProgram) {
    auto source = R"(
        fn main() {
        }
    )";
    
    auto parser = parse_from_string(source);
    auto program_result = parser->parse_program();
    
    EXPECT_TRUE(program_result.has_value());
    EXPECT_FALSE(parser->has_errors());
    
    auto& program = program_result.value();
    EXPECT_EQ(program->declarations().size(), 1);
    
    auto& decl = program->declarations()[0];
    EXPECT_TRUE(decl->is<FunctionDecl>());
    
    auto func_decl = decl->as<FunctionDecl>();
    EXPECT_EQ(func_decl->name(), "main");
    EXPECT_TRUE(func_decl->parameters().empty());
    EXPECT_EQ(func_decl->return_type(), nullptr);
}

TEST_F(ParserTest, FunctionWithParameters) {
    auto source = R"(
        fn add(x: i32, y: i32) -> i32 {
        }
    )";
    
    auto parser = parse_from_string(source);
    auto program_result = parser->parse_program();
    
    EXPECT_TRUE(program_result.has_value());
    EXPECT_FALSE(parser->has_errors());
    
    auto& program = program_result.value();
    EXPECT_EQ(program->declarations().size(), 1);
    
    auto func_decl = program->declarations()[0]->as<FunctionDecl>();
    EXPECT_EQ(func_decl->name(), "add");
    
    auto& params = func_decl->parameters();
    EXPECT_EQ(params.size(), 2);
    EXPECT_EQ(params[0].name, "x");
    EXPECT_EQ(params[1].name, "y");
    
    EXPECT_NE(func_decl->return_type(), nullptr);
    EXPECT_TRUE(func_decl->return_type()->is<Identifier>());
}

TEST_F(ParserTest, MultipleFunctions) {
    auto source = R"(
        fn first() {
        }
        
        fn second(x: i32) -> i32 {
        }
    )";
    
    auto parser = parse_from_string(source);
    auto program_result = parser->parse_program();
    
    EXPECT_TRUE(program_result.has_value());
    EXPECT_FALSE(parser->has_errors());
    
    auto& program = program_result.value();
    EXPECT_EQ(program->declarations().size(), 2);
    
    auto first_func = program->declarations()[0]->as<FunctionDecl>();
    auto second_func = program->declarations()[1]->as<FunctionDecl>();
    
    EXPECT_EQ(first_func->name(), "first");
    EXPECT_EQ(second_func->name(), "second");
}

// === Expression Parsing ===

TEST_F(ParserTest, SimpleExpressions) {
    struct TestCase {
        std::string source;
        ASTNode::Kind expected_kind;
    };
    
    std::vector<TestCase> test_cases = {
        {"42", ASTNode::Kind::IntegerLiteral},
        {"3.14", ASTNode::Kind::FloatLiteral},
        {"\"hello\"", ASTNode::Kind::StringLiteral},
        {"true", ASTNode::Kind::BoolLiteral},
        {"false", ASTNode::Kind::BoolLiteral},
        {"variable", ASTNode::Kind::Identifier},
    };
    
    for (const auto& test : test_cases) {
        auto parser = parse_from_string(test.source);
        auto expr_result = parser->parse_expression();
        
        EXPECT_TRUE(expr_result.has_value()) << "Failed to parse: " << test.source;
        EXPECT_FALSE(parser->has_errors()) << "Errors parsing: " << test.source;
        
        auto& expr = expr_result.value();
        EXPECT_EQ(expr->kind(), test.expected_kind) << "Wrong kind for: " << test.source;
    }
}

TEST_F(ParserTest, BinaryExpressions) {
    struct TestCase {
        std::string source;
        BinaryExpr::Operator expected_op;
    };
    
    std::vector<TestCase> test_cases = {
        {"1 + 2", BinaryExpr::Operator::Add},
        {"5 - 3", BinaryExpr::Operator::Sub},
        {"4 * 7", BinaryExpr::Operator::Mul},
        {"8 / 2", BinaryExpr::Operator::Div},
        {"10 % 3", BinaryExpr::Operator::Mod},
        {"2 ** 3", BinaryExpr::Operator::Pow},
        {"x == y", BinaryExpr::Operator::Equal},
        {"a != b", BinaryExpr::Operator::NotEqual},
        {"x < y", BinaryExpr::Operator::Less},
        {"x > y", BinaryExpr::Operator::Greater},
        {"x <= y", BinaryExpr::Operator::LessEqual},
        {"x >= y", BinaryExpr::Operator::GreaterEqual},
    };
    
    for (const auto& test : test_cases) {
        auto parser = parse_from_string(test.source);
        auto expr_result = parser->parse_expression();
        
        EXPECT_TRUE(expr_result.has_value()) << "Failed to parse: " << test.source;
        EXPECT_FALSE(parser->has_errors()) << "Errors parsing: " << test.source;
        
        auto& expr = expr_result.value();
        EXPECT_TRUE(expr->is<BinaryExpr>()) << "Not a binary expr: " << test.source;
        
        auto binary_expr = expr->as<BinaryExpr>();
        EXPECT_EQ(binary_expr->get_operator(), test.expected_op) << "Wrong operator for: " << test.source;
    }
}

TEST_F(ParserTest, UnaryExpressions) {
    struct TestCase {
        std::string source;
        UnaryExpr::Operator expected_op;
    };
    
    std::vector<TestCase> test_cases = {
        {"+x", UnaryExpr::Operator::Plus},
        {"-x", UnaryExpr::Operator::Minus},
        {"!x", UnaryExpr::Operator::Not},
        {"~x", UnaryExpr::Operator::BitwiseNot},
        {"*x", UnaryExpr::Operator::Dereference},
        {"&x", UnaryExpr::Operator::AddressOf},
    };
    
    for (const auto& test : test_cases) {
        auto parser = parse_from_string(test.source);
        auto expr_result = parser->parse_expression();
        
        EXPECT_TRUE(expr_result.has_value()) << "Failed to parse: " << test.source;
        EXPECT_FALSE(parser->has_errors()) << "Errors parsing: " << test.source;
        
        auto& expr = expr_result.value();
        EXPECT_TRUE(expr->is<UnaryExpr>()) << "Not a unary expr: " << test.source;
        
        auto unary_expr = expr->as<UnaryExpr>();
        EXPECT_EQ(unary_expr->get_operator(), test.expected_op) << "Wrong operator for: " << test.source;
    }
}

TEST_F(ParserTest, ParenthesizedExpressions) {
    auto parser = parse_from_string("(42)");
    auto expr_result = parser->parse_expression();
    
    EXPECT_TRUE(expr_result.has_value());
    EXPECT_FALSE(parser->has_errors());
    
    auto& expr = expr_result.value();
    EXPECT_TRUE(expr->is<IntegerLiteral>());
    
    auto literal = expr->as<IntegerLiteral>();
    EXPECT_EQ(literal->value(), 42);
}

TEST_F(ParserTest, CallExpressions) {
    auto parser = parse_from_string("print(\"hello\", 42)");
    auto expr_result = parser->parse_expression();
    
    EXPECT_TRUE(expr_result.has_value());
    EXPECT_FALSE(parser->has_errors());
    
    auto& expr = expr_result.value();
    EXPECT_TRUE(expr->is<CallExpr>());
    
    auto call_expr = expr->as<CallExpr>();
    EXPECT_TRUE(call_expr->callee().is<Identifier>());
    
    auto callee = call_expr->callee().as<Identifier>();
    EXPECT_EQ(callee->name(), "print");
    
    auto& args = call_expr->args();
    EXPECT_EQ(args.size(), 2);
    EXPECT_TRUE(args[0]->is<StringLiteral>());
    EXPECT_TRUE(args[1]->is<IntegerLiteral>());
}

// === Statement Parsing ===

TEST_F(ParserTest, VariableDeclarations) {
    struct TestCase {
        std::string source;
        bool expected_mutable;
        bool has_type;
        bool has_init;
    };
    
    std::vector<TestCase> test_cases = {
        {"let x = 42", false, false, true},
        {"let mut y = 10", true, false, true},
        {"let z: i32 = 5", false, true, true},
        {"let mut w: i32 = 7", true, true, true},
    };
    
    for (const auto& test : test_cases) {
        auto parser = parse_from_string(test.source);
        auto stmt_result = parser->parse_statement();
        
        EXPECT_TRUE(stmt_result.has_value()) << "Failed to parse: " << test.source;
        EXPECT_FALSE(parser->has_errors()) << "Errors parsing: " << test.source;
        
        auto& stmt = stmt_result.value();
        EXPECT_TRUE(stmt->is<VarDecl>()) << "Not a var decl: " << test.source;
        
        auto var_decl = stmt->as<VarDecl>();
        EXPECT_EQ(var_decl->is_mutable(), test.expected_mutable) << "Wrong mutability: " << test.source;
        EXPECT_EQ(var_decl->type() != nullptr, test.has_type) << "Wrong type presence: " << test.source;
        EXPECT_EQ(var_decl->init() != nullptr, test.has_init) << "Wrong init presence: " << test.source;
    }
}

TEST_F(ParserTest, BlockStatements) {
    auto source = R"(
        {
            let x = 42
            let y = x + 1
        }
    )";
    
    auto parser = parse_from_string(source);
    auto stmt_result = parser->parse_statement();
    
    EXPECT_TRUE(stmt_result.has_value());
    EXPECT_FALSE(parser->has_errors());
    
    auto& stmt = stmt_result.value();
    EXPECT_TRUE(stmt->is<Block>());
    
    auto block = stmt->as<Block>();
    EXPECT_EQ(block->statements().size(), 2);
    
    for (const auto& inner_stmt : block->statements()) {
        EXPECT_TRUE(inner_stmt->is<VarDecl>());
    }
}

// === Operator Precedence Tests ===

TEST_F(ParserTest, ArithmeticPrecedence) {
    auto parser = parse_from_string("1 + 2 * 3");
    auto expr_result = parser->parse_expression();
    
    EXPECT_TRUE(expr_result.has_value());
    EXPECT_FALSE(parser->has_errors());
    
    auto& expr = expr_result.value();
    EXPECT_TRUE(expr->is<BinaryExpr>());
    
    auto binary_expr = expr->as<BinaryExpr>();
    EXPECT_EQ(binary_expr->get_operator(), BinaryExpr::Operator::Add);
    
    // Left side should be 1
    EXPECT_TRUE(binary_expr->left().is<IntegerLiteral>());
    auto left = binary_expr->left().as<IntegerLiteral>();
    EXPECT_EQ(left->value(), 1);
    
    // Right side should be (2 * 3)
    EXPECT_TRUE(binary_expr->right().is<BinaryExpr>());
    auto right = binary_expr->right().as<BinaryExpr>();
    EXPECT_EQ(right->get_operator(), BinaryExpr::Operator::Mul);
}

TEST_F(ParserTest, AssociativityRightToLeft) {
    auto parser = parse_from_string("2 ** 3 ** 2");
    auto expr_result = parser->parse_expression();
    
    EXPECT_TRUE(expr_result.has_value());
    EXPECT_FALSE(parser->has_errors());
    
    auto& expr = expr_result.value();
    EXPECT_TRUE(expr->is<BinaryExpr>());
    
    auto binary_expr = expr->as<BinaryExpr>();
    EXPECT_EQ(binary_expr->get_operator(), BinaryExpr::Operator::Pow);
    
    // Should parse as 2 ** (3 ** 2) due to right associativity
    EXPECT_TRUE(binary_expr->left().is<IntegerLiteral>());
    EXPECT_TRUE(binary_expr->right().is<BinaryExpr>());
}

TEST_F(ParserTest, ParenthesesOverridePrecedence) {
    auto parser = parse_from_string("(1 + 2) * 3");
    auto expr_result = parser->parse_expression();
    
    EXPECT_TRUE(expr_result.has_value());
    EXPECT_FALSE(parser->has_errors());
    
    auto& expr = expr_result.value();
    EXPECT_TRUE(expr->is<BinaryExpr>());
    
    auto binary_expr = expr->as<BinaryExpr>();
    EXPECT_EQ(binary_expr->get_operator(), BinaryExpr::Operator::Mul);
    
    // Left side should be (1 + 2)
    EXPECT_TRUE(binary_expr->left().is<BinaryExpr>());
    auto left = binary_expr->left().as<BinaryExpr>();
    EXPECT_EQ(left->get_operator(), BinaryExpr::Operator::Add);
    
    // Right side should be 3
    EXPECT_TRUE(binary_expr->right().is<IntegerLiteral>());
}

// === Error Cases ===

TEST_F(ParserTest, UnexpectedTokenError) {
    auto parser = parse_from_string("fn");
    auto program_result = parser->parse_program();
    
    EXPECT_FALSE(program_result.has_value());
    EXPECT_TRUE(parser->has_errors());
}

TEST_F(ParserTest, MissingDelimiterError) {
    auto parser = parse_from_string("fn test(x: i32 y: i32) {}");
    auto program_result = parser->parse_program();
    
    EXPECT_FALSE(program_result.has_value());
    EXPECT_TRUE(parser->has_errors());
}

TEST_F(ParserTest, UnterminatedParentheses) {
    auto parser = parse_from_string("(1 + 2");
    auto expr_result = parser->parse_expression();
    
    EXPECT_FALSE(expr_result.has_value());
    EXPECT_TRUE(parser->has_errors());
}