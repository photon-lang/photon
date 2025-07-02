/**
 * @file test_ast.cpp
 * @brief AST node tests
 */

#include <gtest/gtest.h>
#include "photon/parser/ast.hpp"
#include "photon/memory/arena.hpp"
#include "photon/diagnostics/source_location.hpp"

using namespace photon::parser;
using namespace photon::diagnostics;

class ASTTest : public ::testing::Test {
protected:
    void SetUp() override {
        arena = std::make_unique<photon::memory::MemoryArena<>>();
        factory = std::make_unique<ASTFactory>(*arena);
    }

    std::unique_ptr<photon::memory::MemoryArena<>> arena;
    std::unique_ptr<ASTFactory> factory;
    
    photon::parser::SourceRange dummy_range = {SourceLocation{}, SourceLocation{}};
};

// === Literal Tests ===

TEST_F(ASTTest, IntegerLiteralCreation) {
    auto literal = factory->create<IntegerLiteral>(42, dummy_range);
    
    EXPECT_EQ(literal->kind(), ASTNode::Kind::IntegerLiteral);
    EXPECT_EQ(literal->value(), 42);
    EXPECT_TRUE(literal->is<IntegerLiteral>());
    EXPECT_FALSE(literal->is<FloatLiteral>());
    EXPECT_EQ(literal->to_string(), "42");
}

TEST_F(ASTTest, FloatLiteralCreation) {
    auto literal = factory->create<FloatLiteral>(3.14, dummy_range);
    
    EXPECT_EQ(literal->kind(), ASTNode::Kind::FloatLiteral);
    EXPECT_DOUBLE_EQ(literal->value(), 3.14);
    EXPECT_TRUE(literal->is<FloatLiteral>());
    EXPECT_FALSE(literal->is<IntegerLiteral>());
    EXPECT_EQ(literal->to_string(), "3.140000");
}

TEST_F(ASTTest, StringLiteralCreation) {
    auto literal = factory->create<StringLiteral>("hello", dummy_range);
    
    EXPECT_EQ(literal->kind(), ASTNode::Kind::StringLiteral);
    EXPECT_EQ(literal->value(), "hello");
    EXPECT_TRUE(literal->is<StringLiteral>());
    EXPECT_EQ(literal->to_string(), "\"hello\"");
}

TEST_F(ASTTest, BoolLiteralCreation) {
    auto true_literal = factory->create<BoolLiteral>(true, dummy_range);
    auto false_literal = factory->create<BoolLiteral>(false, dummy_range);
    
    EXPECT_EQ(true_literal->kind(), ASTNode::Kind::BoolLiteral);
    EXPECT_TRUE(true_literal->value());
    EXPECT_EQ(true_literal->to_string(), "true");
    
    EXPECT_EQ(false_literal->kind(), ASTNode::Kind::BoolLiteral);
    EXPECT_FALSE(false_literal->value());
    EXPECT_EQ(false_literal->to_string(), "false");
}

// === Identifier Tests ===

TEST_F(ASTTest, IdentifierCreation) {
    auto identifier = factory->create<Identifier>("variable", dummy_range);
    
    EXPECT_EQ(identifier->kind(), ASTNode::Kind::Identifier);
    EXPECT_EQ(identifier->name(), "variable");
    EXPECT_TRUE(identifier->is<Identifier>());
    EXPECT_EQ(identifier->to_string(), "variable");
}

// === Binary Expression Tests ===

TEST_F(ASTTest, BinaryExprCreation) {
    auto left = factory->create<IntegerLiteral>(5, dummy_range);
    auto right = factory->create<IntegerLiteral>(3, dummy_range);
    auto binary_expr = factory->create<BinaryExpr>(
        std::move(left), 
        BinaryExpr::Operator::Add, 
        std::move(right), 
        dummy_range
    );
    
    EXPECT_EQ(binary_expr->kind(), ASTNode::Kind::BinaryExpr);
    EXPECT_EQ(binary_expr->get_operator(), BinaryExpr::Operator::Add);
    EXPECT_TRUE(binary_expr->is<BinaryExpr>());
    
    auto& left_ref = binary_expr->left();
    auto& right_ref = binary_expr->right();
    EXPECT_TRUE(left_ref.is<IntegerLiteral>());
    EXPECT_TRUE(right_ref.is<IntegerLiteral>());
    
    auto left_literal = left_ref.as<IntegerLiteral>();
    auto right_literal = right_ref.as<IntegerLiteral>();
    EXPECT_EQ(left_literal->value(), 5);
    EXPECT_EQ(right_literal->value(), 3);
    
    EXPECT_EQ(binary_expr->to_string(), "(5 + 3)");
}

TEST_F(ASTTest, BinaryExprOperators) {
    auto left = factory->create<IntegerLiteral>(10, dummy_range);
    auto right = factory->create<IntegerLiteral>(2, dummy_range);
    
    struct TestCase {
        BinaryExpr::Operator op;
        std::string expected;
    };
    
    std::vector<TestCase> test_cases = {
        {BinaryExpr::Operator::Sub, "(10 - 2)"},
        {BinaryExpr::Operator::Mul, "(10 * 2)"},
        {BinaryExpr::Operator::Div, "(10 / 2)"},
        {BinaryExpr::Operator::Mod, "(10 % 2)"},
        {BinaryExpr::Operator::Equal, "(10 == 2)"},
        {BinaryExpr::Operator::NotEqual, "(10 != 2)"},
        {BinaryExpr::Operator::Less, "(10 < 2)"},
        {BinaryExpr::Operator::Greater, "(10 > 2)"},
    };
    
    for (const auto& test : test_cases) {
        auto left_copy = factory->create<IntegerLiteral>(10, dummy_range);
        auto right_copy = factory->create<IntegerLiteral>(2, dummy_range);
        auto expr = factory->create<BinaryExpr>(
            std::move(left_copy), 
            test.op, 
            std::move(right_copy), 
            dummy_range
        );
        
        EXPECT_EQ(expr->to_string(), test.expected);
    }
}

// === Unary Expression Tests ===

TEST_F(ASTTest, UnaryExprCreation) {
    auto operand = factory->create<IntegerLiteral>(42, dummy_range);
    auto unary_expr = factory->create<UnaryExpr>(
        UnaryExpr::Operator::Minus, 
        std::move(operand), 
        dummy_range
    );
    
    EXPECT_EQ(unary_expr->kind(), ASTNode::Kind::UnaryExpr);
    EXPECT_EQ(unary_expr->get_operator(), UnaryExpr::Operator::Minus);
    EXPECT_TRUE(unary_expr->is<UnaryExpr>());
    
    auto& operand_ref = unary_expr->operand();
    EXPECT_TRUE(operand_ref.is<IntegerLiteral>());
    
    auto operand_literal = operand_ref.as<IntegerLiteral>();
    EXPECT_EQ(operand_literal->value(), 42);
    
    EXPECT_EQ(unary_expr->to_string(), "(-42)");
}

// === Call Expression Tests ===

TEST_F(ASTTest, CallExprCreation) {
    auto callee = factory->create<Identifier>("print", dummy_range);
    auto args = factory->create_list<Expression>();
    args.push_back(factory->create<StringLiteral>("hello", dummy_range));
    args.push_back(factory->create<IntegerLiteral>(123, dummy_range));
    
    auto call_expr = factory->create<CallExpr>(
        std::move(callee), 
        std::move(args), 
        dummy_range
    );
    
    EXPECT_EQ(call_expr->kind(), ASTNode::Kind::CallExpr);
    EXPECT_TRUE(call_expr->is<CallExpr>());
    
    auto& callee_ref = call_expr->callee();
    EXPECT_TRUE(callee_ref.is<Identifier>());
    
    auto& args_ref = call_expr->args();
    EXPECT_EQ(args_ref.size(), 2);
    
    EXPECT_TRUE(args_ref[0]->is<StringLiteral>());
    EXPECT_TRUE(args_ref[1]->is<IntegerLiteral>());
    
    EXPECT_EQ(call_expr->to_string(), "print(\"hello\", 123)");
}

// === Block Tests ===

TEST_F(ASTTest, BlockCreation) {
    auto statements = factory->create_list<Statement>();
    
    auto var_decl = factory->create<VarDecl>(
        "x", 
        nullptr, 
        factory->create<IntegerLiteral>(42, dummy_range), 
        false, 
        dummy_range
    );
    
    statements.push_back(std::move(var_decl));
    
    auto block = factory->create<Block>(std::move(statements), dummy_range);
    
    EXPECT_EQ(block->kind(), ASTNode::Kind::Block);
    EXPECT_TRUE(block->is<Block>());
    
    auto& statements_ref = block->statements();
    EXPECT_EQ(statements_ref.size(), 1);
    EXPECT_TRUE(statements_ref[0]->is<VarDecl>());
}

// === Variable Declaration Tests ===

TEST_F(ASTTest, VarDeclCreation) {
    auto type_annotation = factory->create<Identifier>("i32", dummy_range);
    auto initializer = factory->create<IntegerLiteral>(42, dummy_range);
    
    auto var_decl = factory->create<VarDecl>(
        "x", 
        std::move(type_annotation), 
        std::move(initializer), 
        true, // mutable
        dummy_range
    );
    
    EXPECT_EQ(var_decl->kind(), ASTNode::Kind::VarDecl);
    EXPECT_EQ(var_decl->name(), "x");
    EXPECT_TRUE(var_decl->is_mutable());
    EXPECT_TRUE(var_decl->is<VarDecl>());
    
    EXPECT_NE(var_decl->type(), nullptr);
    EXPECT_NE(var_decl->init(), nullptr);
    
    EXPECT_TRUE(var_decl->type()->is<Identifier>());
    EXPECT_TRUE(var_decl->init()->is<IntegerLiteral>());
    
    EXPECT_EQ(var_decl->to_string(), "let mut x: i32 = 42");
}

TEST_F(ASTTest, VarDeclMinimal) {
    auto var_decl = factory->create<VarDecl>(
        "y", 
        nullptr, // no type annotation
        nullptr, // no initializer
        false,   // immutable
        dummy_range
    );
    
    EXPECT_EQ(var_decl->name(), "y");
    EXPECT_FALSE(var_decl->is_mutable());
    EXPECT_EQ(var_decl->type(), nullptr);
    EXPECT_EQ(var_decl->init(), nullptr);
    
    EXPECT_EQ(var_decl->to_string(), "let y");
}

// === Function Declaration Tests ===

TEST_F(ASTTest, FunctionDeclCreation) {
    photon::Vec<FunctionDecl::Parameter> parameters;
    parameters.push_back({
        "x",
        factory->create<Identifier>("i32", dummy_range),
        dummy_range
    });
    parameters.push_back({
        "y", 
        factory->create<Identifier>("i32", dummy_range),
        dummy_range
    });
    
    auto return_type = factory->create<Identifier>("i32", dummy_range);
    auto body = factory->create<Block>(factory->create_list<Statement>(), dummy_range);
    
    auto func_decl = factory->create<FunctionDecl>(
        "add",
        std::move(parameters),
        std::move(return_type),
        std::move(body),
        dummy_range
    );
    
    EXPECT_EQ(func_decl->kind(), ASTNode::Kind::FunctionDecl);
    EXPECT_EQ(func_decl->name(), "add");
    EXPECT_TRUE(func_decl->is<FunctionDecl>());
    
    auto& params_ref = func_decl->parameters();
    EXPECT_EQ(params_ref.size(), 2);
    EXPECT_EQ(params_ref[0].name, "x");
    EXPECT_EQ(params_ref[1].name, "y");
    
    EXPECT_NE(func_decl->return_type(), nullptr);
    EXPECT_TRUE(func_decl->return_type()->is<Identifier>());
    
    auto expected = "fn add(x: i32, y: i32) -> i32 {\n}";
    EXPECT_EQ(func_decl->to_string(), expected);
}

// === Type-safe casting tests ===

TEST_F(ASTTest, TypeSafeCasting) {
    auto literal = factory->create<IntegerLiteral>(42, dummy_range);
    ASTNode* node = literal.get();
    
    // Test successful downcasting
    auto int_literal = node->as<IntegerLiteral>();
    EXPECT_NE(int_literal, nullptr);
    EXPECT_EQ(int_literal->value(), 42);
    
    // Test failed downcasting
    auto float_literal = node->as<FloatLiteral>();
    EXPECT_EQ(float_literal, nullptr);
    
    // Test is<> method
    EXPECT_TRUE(node->is<IntegerLiteral>());
    EXPECT_FALSE(node->is<FloatLiteral>());
    EXPECT_TRUE(node->is<Expression>());
}

// === Base class hierarchy tests ===

TEST_F(ASTTest, BaseClassHierarchy) {
    auto literal = factory->create<IntegerLiteral>(42, dummy_range);
    
    EXPECT_TRUE(Expression::class_of(literal.get()));
    EXPECT_FALSE(Statement::class_of(literal.get()));
    EXPECT_FALSE(Declaration::class_of(literal.get()));
    
    auto var_decl = factory->create<VarDecl>("x", nullptr, nullptr, false, dummy_range);
    
    EXPECT_FALSE(Expression::class_of(var_decl.get()));
    EXPECT_TRUE(Statement::class_of(var_decl.get()));
    EXPECT_FALSE(Declaration::class_of(var_decl.get()));
    
    auto func_decl = factory->create<FunctionDecl>(
        "test", 
        photon::Vec<FunctionDecl::Parameter>{}, 
        nullptr, 
        factory->create<Block>(factory->create_list<Statement>(), dummy_range),
        dummy_range
    );
    
    EXPECT_FALSE(Expression::class_of(func_decl.get()));
    EXPECT_FALSE(Statement::class_of(func_decl.get()));
    EXPECT_TRUE(Declaration::class_of(func_decl.get()));
}