/**
 * @file test_statements.cpp
 * @brief Statement parsing tests
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

class StatementTest : public ::testing::Test {
protected:
    void SetUp() override {
        arena = std::make_unique<MemoryArena<>>();
        source_mgr = std::make_unique<SourceManager>(*arena);
    }
    
    auto parse_statement(const std::string& source) -> photon::Result<ASTPtr<Statement>, ParseError> {
        auto file_id_result = source_mgr->load_from_string("test.ph", source);
        if (!file_id_result) {
            return photon::Result<ASTPtr<Statement>, ParseError>(ParseError::UnexpectedEof);
        }
        
        auto file_id = file_id_result.value();
        auto* file = source_mgr->get_file(file_id);
        if (!file) {
            return photon::Result<ASTPtr<Statement>, ParseError>(ParseError::UnexpectedEof);
        }
        
        auto lexer = Lexer(*source_mgr, *arena);
        
        auto tokens_result = lexer.tokenize(file_id);
        if (!tokens_result) {
            return photon::Result<ASTPtr<Statement>, ParseError>(ParseError::InvalidSyntax);
        }
        
        auto token_stream = std::move(tokens_result.value());
        auto parser = Parser(std::move(token_stream), *arena);
        
        return parser.parse_statement();
    }

    std::unique_ptr<MemoryArena<>> arena;
    std::unique_ptr<SourceManager> source_mgr;
};

// === Variable Declaration Tests ===

TEST_F(StatementTest, SimpleVariableDeclaration) {
    auto stmt_result = parse_statement("let x = 42");
    ASSERT_TRUE(stmt_result.has_value());
    
    auto& stmt = stmt_result.value();
    EXPECT_TRUE(stmt->is<VarDecl>());
    
    auto var_decl = stmt->as<VarDecl>();
    EXPECT_EQ(var_decl->name(), "x");
    EXPECT_FALSE(var_decl->is_mutable());
    EXPECT_EQ(var_decl->type(), nullptr);
    EXPECT_NE(var_decl->init(), nullptr);
    
    auto init = var_decl->init()->as<IntegerLiteral>();
    EXPECT_EQ(init->value(), 42);
}

TEST_F(StatementTest, MutableVariableDeclaration) {
    auto stmt_result = parse_statement("let mut counter = 0");
    ASSERT_TRUE(stmt_result.has_value());
    
    auto& stmt = stmt_result.value();
    EXPECT_TRUE(stmt->is<VarDecl>());
    
    auto var_decl = stmt->as<VarDecl>();
    EXPECT_EQ(var_decl->name(), "counter");
    EXPECT_TRUE(var_decl->is_mutable());
    EXPECT_EQ(var_decl->type(), nullptr);
    EXPECT_NE(var_decl->init(), nullptr);
}

TEST_F(StatementTest, TypedVariableDeclaration) {
    auto stmt_result = parse_statement("let x: i32 = 42");
    ASSERT_TRUE(stmt_result.has_value());
    
    auto& stmt = stmt_result.value();
    EXPECT_TRUE(stmt->is<VarDecl>());
    
    auto var_decl = stmt->as<VarDecl>();
    EXPECT_EQ(var_decl->name(), "x");
    EXPECT_FALSE(var_decl->is_mutable());
    EXPECT_NE(var_decl->type(), nullptr);
    EXPECT_NE(var_decl->init(), nullptr);
    
    auto type_name = var_decl->type()->as<Identifier>();
    EXPECT_EQ(type_name->name(), "i32");
}

TEST_F(StatementTest, MutableTypedVariableDeclaration) {
    auto stmt_result = parse_statement("let mut value: f64 = 3.14");
    ASSERT_TRUE(stmt_result.has_value());
    
    auto& stmt = stmt_result.value();
    EXPECT_TRUE(stmt->is<VarDecl>());
    
    auto var_decl = stmt->as<VarDecl>();
    EXPECT_EQ(var_decl->name(), "value");
    EXPECT_TRUE(var_decl->is_mutable());
    EXPECT_NE(var_decl->type(), nullptr);
    EXPECT_NE(var_decl->init(), nullptr);
    
    auto type_name = var_decl->type()->as<Identifier>();
    EXPECT_EQ(type_name->name(), "f64");
    
    auto init = var_decl->init()->as<FloatLiteral>();
    EXPECT_DOUBLE_EQ(init->value(), 3.14);
}

TEST_F(StatementTest, VariableDeclarationWithExpression) {
    auto stmt_result = parse_statement("let result = 1 + 2 * 3");
    ASSERT_TRUE(stmt_result.has_value());
    
    auto& stmt = stmt_result.value();
    EXPECT_TRUE(stmt->is<VarDecl>());
    
    auto var_decl = stmt->as<VarDecl>();
    EXPECT_EQ(var_decl->name(), "result");
    EXPECT_NE(var_decl->init(), nullptr);
    
    EXPECT_TRUE(var_decl->init()->is<BinaryExpr>());
    auto binary_expr = var_decl->init()->as<BinaryExpr>();
    EXPECT_EQ(binary_expr->get_operator(), BinaryExpr::Operator::Add);
}

TEST_F(StatementTest, VariableDeclarationWithFunctionCall) {
    auto stmt_result = parse_statement("let name = get_name()");
    ASSERT_TRUE(stmt_result.has_value());
    
    auto& stmt = stmt_result.value();
    EXPECT_TRUE(stmt->is<VarDecl>());
    
    auto var_decl = stmt->as<VarDecl>();
    EXPECT_EQ(var_decl->name(), "name");
    EXPECT_NE(var_decl->init(), nullptr);
    
    EXPECT_TRUE(var_decl->init()->is<CallExpr>());
    auto call_expr = var_decl->init()->as<CallExpr>();
    
    auto callee = call_expr->callee().as<Identifier>();
    EXPECT_EQ(callee->name(), "get_name");
}

// === Block Statement Tests ===

TEST_F(StatementTest, EmptyBlock) {
    auto stmt_result = parse_statement("{}");
    ASSERT_TRUE(stmt_result.has_value());
    
    auto& stmt = stmt_result.value();
    EXPECT_TRUE(stmt->is<Block>());
    
    auto block = stmt->as<Block>();
    EXPECT_TRUE(block->statements().empty());
}

TEST_F(StatementTest, BlockWithSingleStatement) {
    auto stmt_result = parse_statement("{ let x = 42 }");
    ASSERT_TRUE(stmt_result.has_value());
    
    auto& stmt = stmt_result.value();
    EXPECT_TRUE(stmt->is<Block>());
    
    auto block = stmt->as<Block>();
    EXPECT_EQ(block->statements().size(), 1);
    
    auto& inner_stmt = block->statements()[0];
    EXPECT_TRUE(inner_stmt->is<VarDecl>());
    
    auto var_decl = inner_stmt->as<VarDecl>();
    EXPECT_EQ(var_decl->name(), "x");
}

TEST_F(StatementTest, BlockWithMultipleStatements) {
    auto source = R"(
        {
            let x = 42
            let y = x + 1
            let z = y * 2
        }
    )";
    
    auto stmt_result = parse_statement(source);
    ASSERT_TRUE(stmt_result.has_value());
    
    auto& stmt = stmt_result.value();
    EXPECT_TRUE(stmt->is<Block>());
    
    auto block = stmt->as<Block>();
    EXPECT_EQ(block->statements().size(), 3);
    
    for (const auto& inner_stmt : block->statements()) {
        EXPECT_TRUE(inner_stmt->is<VarDecl>());
    }
    
    auto first_var = block->statements()[0]->as<VarDecl>();
    auto second_var = block->statements()[1]->as<VarDecl>();
    auto third_var = block->statements()[2]->as<VarDecl>();
    
    EXPECT_EQ(first_var->name(), "x");
    EXPECT_EQ(second_var->name(), "y");
    EXPECT_EQ(third_var->name(), "z");
}

TEST_F(StatementTest, NestedBlocks) {
    auto source = R"(
        {
            let outer = 1
            {
                let inner = 2
            }
        }
    )";
    
    auto stmt_result = parse_statement(source);
    ASSERT_TRUE(stmt_result.has_value());
    
    auto& stmt = stmt_result.value();
    EXPECT_TRUE(stmt->is<Block>());
    
    auto outer_block = stmt->as<Block>();
    EXPECT_EQ(outer_block->statements().size(), 2);
    
    EXPECT_TRUE(outer_block->statements()[0]->is<VarDecl>());
    EXPECT_TRUE(outer_block->statements()[1]->is<Block>());
    
    auto inner_block = outer_block->statements()[1]->as<Block>();
    EXPECT_EQ(inner_block->statements().size(), 1);
    
    EXPECT_TRUE(inner_block->statements()[0]->is<VarDecl>());
    auto inner_var = inner_block->statements()[0]->as<VarDecl>();
    EXPECT_EQ(inner_var->name(), "inner");
}

// === Expression Statement Tests ===

TEST_F(StatementTest, SimpleExpressionInBlock) {
    auto stmt_result = parse_statement("{ 42 }");
    ASSERT_TRUE(stmt_result.has_value());
    
    auto& stmt = stmt_result.value();
    EXPECT_TRUE(stmt->is<Block>());
    
    auto block = stmt->as<Block>();
    EXPECT_EQ(block->statements().size(), 1);
    
    // The expression should be stored directly in the statement list
    // Since we don't have ExprStmt, expressions are parsed as statements
}

TEST_F(StatementTest, FunctionCallInBlock) {
    auto stmt_result = parse_statement("{ print(\"hello\") }");
    ASSERT_TRUE(stmt_result.has_value());
    
    auto& stmt = stmt_result.value();
    EXPECT_TRUE(stmt->is<Block>());
    
    auto block = stmt->as<Block>();
    EXPECT_EQ(block->statements().size(), 1);
}

// === Complex Variable Initializers ===

TEST_F(StatementTest, VariableWithComplexExpression) {
    auto stmt_result = parse_statement("let result = (a + b) * (c - d)");
    ASSERT_TRUE(stmt_result.has_value());
    
    auto& stmt = stmt_result.value();
    EXPECT_TRUE(stmt->is<VarDecl>());
    
    auto var_decl = stmt->as<VarDecl>();
    EXPECT_EQ(var_decl->name(), "result");
    
    EXPECT_TRUE(var_decl->init()->is<BinaryExpr>());
    auto mult_expr = var_decl->init()->as<BinaryExpr>();
    EXPECT_EQ(mult_expr->get_operator(), BinaryExpr::Operator::Mul);
    
    EXPECT_TRUE(mult_expr->left().is<BinaryExpr>());
    EXPECT_TRUE(mult_expr->right().is<BinaryExpr>());
    
    auto left_add = mult_expr->left().as<BinaryExpr>();
    auto right_sub = mult_expr->right().as<BinaryExpr>();
    
    EXPECT_EQ(left_add->get_operator(), BinaryExpr::Operator::Add);
    EXPECT_EQ(right_sub->get_operator(), BinaryExpr::Operator::Sub);
}

TEST_F(StatementTest, VariableWithNestedCalls) {
    auto stmt_result = parse_statement("let value = max(min(a, b), min(c, d))");
    ASSERT_TRUE(stmt_result.has_value());
    
    auto& stmt = stmt_result.value();
    EXPECT_TRUE(stmt->is<VarDecl>());
    
    auto var_decl = stmt->as<VarDecl>();
    EXPECT_EQ(var_decl->name(), "value");
    
    EXPECT_TRUE(var_decl->init()->is<CallExpr>());
    auto outer_call = var_decl->init()->as<CallExpr>();
    
    auto callee = outer_call->callee().as<Identifier>();
    EXPECT_EQ(callee->name(), "max");
    
    EXPECT_EQ(outer_call->args().size(), 2);
    EXPECT_TRUE(outer_call->args()[0]->is<CallExpr>());
    EXPECT_TRUE(outer_call->args()[1]->is<CallExpr>());
}

// === Statement Sequences ===

TEST_F(StatementTest, BlockWithMixedStatements) {
    auto source = R"(
        {
            let x: i32 = 42
            let mut y = x * 2
            compute(x, y)
            let result = x + y
        }
    )";
    
    auto stmt_result = parse_statement(source);
    ASSERT_TRUE(stmt_result.has_value());
    
    auto& stmt = stmt_result.value();
    EXPECT_TRUE(stmt->is<Block>());
    
    auto block = stmt->as<Block>();
    EXPECT_EQ(block->statements().size(), 4);
    
    // Check the variable declarations
    auto first_var = block->statements()[0]->as<VarDecl>();
    EXPECT_EQ(first_var->name(), "x");
    EXPECT_FALSE(first_var->is_mutable());
    EXPECT_NE(first_var->type(), nullptr);
    
    auto second_var = block->statements()[1]->as<VarDecl>();
    EXPECT_EQ(second_var->name(), "y");
    EXPECT_TRUE(second_var->is_mutable());
    
    auto fourth_var = block->statements()[3]->as<VarDecl>();
    EXPECT_EQ(fourth_var->name(), "result");
}

// === Error Cases ===

TEST_F(StatementTest, IncompleteVariableDeclaration) {
    auto stmt_result = parse_statement("let");
    EXPECT_FALSE(stmt_result.has_value());
}

TEST_F(StatementTest, MissingVariableName) {
    auto stmt_result = parse_statement("let = 42");
    EXPECT_FALSE(stmt_result.has_value());
}

TEST_F(StatementTest, UnterminatedBlock) {
    auto stmt_result = parse_statement("{ let x = 42");
    EXPECT_FALSE(stmt_result.has_value());
}

TEST_F(StatementTest, MissingTypeAfterColon) {
    auto stmt_result = parse_statement("let x: = 42");
    EXPECT_FALSE(stmt_result.has_value());
}

TEST_F(StatementTest, MissingInitializerAfterAssign) {
    auto stmt_result = parse_statement("let x =");
    EXPECT_FALSE(stmt_result.has_value());
}