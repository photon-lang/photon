/**
 * @file test_expressions.cpp
 * @brief Comprehensive expression parsing tests
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

class ExpressionTest : public ::testing::Test {
protected:
    void SetUp() override {
        arena = std::make_unique<MemoryArena<>>();
        source_mgr = std::make_unique<SourceManager>(*arena);
    }
    
    auto parse_expression(const std::string& source) -> photon::Result<ASTPtr<Expression>, ParseError> {
        auto file_id_result = source_mgr->load_from_string("test.ph", source);
        if (!file_id_result) {
            return photon::Result<ASTPtr<Expression>, ParseError>(ParseError::UnexpectedEof);
        }
        
        auto file_id = file_id_result.value();
        auto* file = source_mgr->get_file(file_id);
        if (!file) {
            return photon::Result<ASTPtr<Expression>, ParseError>(ParseError::UnexpectedEof);
        }
        
        auto lexer = Lexer(*source_mgr, *arena);
        
        auto tokens_result = lexer.tokenize(file_id);
        if (!tokens_result) {
            return photon::Result<ASTPtr<Expression>, ParseError>(ParseError::InvalidSyntax);
        }
        
        auto token_stream = std::move(tokens_result.value());
        auto parser = Parser(std::move(token_stream), *arena);
        
        return parser.parse_expression();
    }

    std::unique_ptr<MemoryArena<>> arena;
    std::unique_ptr<SourceManager> source_mgr;
};

// === Complex Expression Trees ===

TEST_F(ExpressionTest, NestedBinaryExpressions) {
    auto expr_result = parse_expression("1 + 2 * 3 + 4");
    ASSERT_TRUE(expr_result.has_value());
    
    auto& expr = expr_result.value();
    EXPECT_TRUE(expr->is<BinaryExpr>());
    
    // Should parse as: (1 + (2 * 3)) + 4
    auto root = expr->as<BinaryExpr>();
    EXPECT_EQ(root->get_operator(), BinaryExpr::Operator::Add);
    
    auto left_add = root->left().as<BinaryExpr>();
    EXPECT_EQ(left_add->get_operator(), BinaryExpr::Operator::Add);
    
    auto left_left = left_add->left().as<IntegerLiteral>();
    EXPECT_EQ(left_left->value(), 1);
    
    auto left_right = left_add->right().as<BinaryExpr>();
    EXPECT_EQ(left_right->get_operator(), BinaryExpr::Operator::Mul);
    
    auto right = root->right().as<IntegerLiteral>();
    EXPECT_EQ(right->value(), 4);
}

TEST_F(ExpressionTest, MixedUnaryAndBinary) {
    auto expr_result = parse_expression("-5 + !true");
    ASSERT_TRUE(expr_result.has_value());
    
    auto& expr = expr_result.value();
    EXPECT_TRUE(expr->is<BinaryExpr>());
    
    auto binary = expr->as<BinaryExpr>();
    EXPECT_EQ(binary->get_operator(), BinaryExpr::Operator::Add);
    
    // Left side: -5
    EXPECT_TRUE(binary->left().is<UnaryExpr>());
    auto left_unary = binary->left().as<UnaryExpr>();
    EXPECT_EQ(left_unary->get_operator(), UnaryExpr::Operator::Minus);
    EXPECT_TRUE(left_unary->operand().is<IntegerLiteral>());
    
    // Right side: !true
    EXPECT_TRUE(binary->right().is<UnaryExpr>());
    auto right_unary = binary->right().as<UnaryExpr>();
    EXPECT_EQ(right_unary->get_operator(), UnaryExpr::Operator::Not);
    EXPECT_TRUE(right_unary->operand().is<BoolLiteral>());
}

TEST_F(ExpressionTest, ChainedComparisons) {
    auto expr_result = parse_expression("a < b && b <= c");
    ASSERT_TRUE(expr_result.has_value());
    
    auto& expr = expr_result.value();
    EXPECT_TRUE(expr->is<BinaryExpr>());
    
    auto logical_and = expr->as<BinaryExpr>();
    EXPECT_EQ(logical_and->get_operator(), BinaryExpr::Operator::LogicalAnd);
    
    auto left_cmp = logical_and->left().as<BinaryExpr>();
    EXPECT_EQ(left_cmp->get_operator(), BinaryExpr::Operator::Less);
    
    auto right_cmp = logical_and->right().as<BinaryExpr>();
    EXPECT_EQ(right_cmp->get_operator(), BinaryExpr::Operator::LessEqual);
}

// === Function Call Expressions ===

TEST_F(ExpressionTest, SimpleFunctionCall) {
    auto expr_result = parse_expression("foo()");
    ASSERT_TRUE(expr_result.has_value());
    
    auto& expr = expr_result.value();
    EXPECT_TRUE(expr->is<CallExpr>());
    
    auto call = expr->as<CallExpr>();
    EXPECT_TRUE(call->callee().is<Identifier>());
    
    auto callee = call->callee().as<Identifier>();
    EXPECT_EQ(callee->name(), "foo");
    
    EXPECT_TRUE(call->args().empty());
}

TEST_F(ExpressionTest, FunctionCallWithArgs) {
    auto expr_result = parse_expression("add(1, 2, 3)");
    ASSERT_TRUE(expr_result.has_value());
    
    auto& expr = expr_result.value();
    EXPECT_TRUE(expr->is<CallExpr>());
    
    auto call = expr->as<CallExpr>();
    EXPECT_EQ(call->args().size(), 3);
    
    for (photon::usize i = 0; i < 3; ++i) {
        EXPECT_TRUE(call->args()[i]->is<IntegerLiteral>());
        auto lit = call->args()[i]->as<IntegerLiteral>();
        EXPECT_EQ(lit->value(), static_cast<photon::i64>(i + 1));
    }
}

TEST_F(ExpressionTest, NestedFunctionCalls) {
    auto expr_result = parse_expression("outer(inner(42))");
    ASSERT_TRUE(expr_result.has_value());
    
    auto& expr = expr_result.value();
    EXPECT_TRUE(expr->is<CallExpr>());
    
    auto outer_call = expr->as<CallExpr>();
    auto outer_callee = outer_call->callee().as<Identifier>();
    EXPECT_EQ(outer_callee->name(), "outer");
    
    EXPECT_EQ(outer_call->args().size(), 1);
    EXPECT_TRUE(outer_call->args()[0]->is<CallExpr>());
    
    auto inner_call = outer_call->args()[0]->as<CallExpr>();
    auto inner_callee = inner_call->callee().as<Identifier>();
    EXPECT_EQ(inner_callee->name(), "inner");
    
    EXPECT_EQ(inner_call->args().size(), 1);
    EXPECT_TRUE(inner_call->args()[0]->is<IntegerLiteral>());
}

// === Complex Precedence Cases ===

TEST_F(ExpressionTest, PowerAssociativity) {
    auto expr_result = parse_expression("2 ** 3 ** 4");
    ASSERT_TRUE(expr_result.has_value());
    
    auto& expr = expr_result.value();
    EXPECT_TRUE(expr->is<BinaryExpr>());
    
    // Should parse as 2 ** (3 ** 4) due to right associativity
    auto root = expr->as<BinaryExpr>();
    EXPECT_EQ(root->get_operator(), BinaryExpr::Operator::Pow);
    
    auto left = root->left().as<IntegerLiteral>();
    EXPECT_EQ(left->value(), 2);
    
    EXPECT_TRUE(root->right().is<BinaryExpr>());
    auto right_pow = root->right().as<BinaryExpr>();
    EXPECT_EQ(right_pow->get_operator(), BinaryExpr::Operator::Pow);
}

TEST_F(ExpressionTest, ComparisonChaining) {
    auto expr_result = parse_expression("1 < 2 == true");
    ASSERT_TRUE(expr_result.has_value());
    
    auto& expr = expr_result.value();
    EXPECT_TRUE(expr->is<BinaryExpr>());
    
    // Should parse as (1 < 2) == true
    auto equality = expr->as<BinaryExpr>();
    EXPECT_EQ(equality->get_operator(), BinaryExpr::Operator::Equal);
    
    EXPECT_TRUE(equality->left().is<BinaryExpr>());
    auto comparison = equality->left().as<BinaryExpr>();
    EXPECT_EQ(comparison->get_operator(), BinaryExpr::Operator::Less);
    
    EXPECT_TRUE(equality->right().is<BoolLiteral>());
}

TEST_F(ExpressionTest, BitwiseOperatorPrecedence) {
    auto expr_result = parse_expression("a & b | c ^ d");
    ASSERT_TRUE(expr_result.has_value());
    
    auto& expr = expr_result.value();
    EXPECT_TRUE(expr->is<BinaryExpr>());
    
    // Should parse as ((a & b) | c) ^ d due to precedence
    auto xor_expr = expr->as<BinaryExpr>();
    EXPECT_EQ(xor_expr->get_operator(), BinaryExpr::Operator::BitwiseXor);
    
    auto or_expr = xor_expr->left().as<BinaryExpr>();
    EXPECT_EQ(or_expr->get_operator(), BinaryExpr::Operator::BitwiseOr);
    
    auto and_expr = or_expr->left().as<BinaryExpr>();
    EXPECT_EQ(and_expr->get_operator(), BinaryExpr::Operator::BitwiseAnd);
}

// === Complex Parenthesized Expressions ===

TEST_F(ExpressionTest, NestedParentheses) {
    auto expr_result = parse_expression("((1 + 2) * (3 + 4))");
    ASSERT_TRUE(expr_result.has_value());
    
    auto& expr = expr_result.value();
    EXPECT_TRUE(expr->is<BinaryExpr>());
    
    auto mult = expr->as<BinaryExpr>();
    EXPECT_EQ(mult->get_operator(), BinaryExpr::Operator::Mul);
    
    auto left_add = mult->left().as<BinaryExpr>();
    EXPECT_EQ(left_add->get_operator(), BinaryExpr::Operator::Add);
    
    auto right_add = mult->right().as<BinaryExpr>();
    EXPECT_EQ(right_add->get_operator(), BinaryExpr::Operator::Add);
}

TEST_F(ExpressionTest, FunctionCallInExpression) {
    auto expr_result = parse_expression("max(a, b) + min(c, d)");
    ASSERT_TRUE(expr_result.has_value());
    
    auto& expr = expr_result.value();
    EXPECT_TRUE(expr->is<BinaryExpr>());
    
    auto add = expr->as<BinaryExpr>();
    EXPECT_EQ(add->get_operator(), BinaryExpr::Operator::Add);
    
    EXPECT_TRUE(add->left().is<CallExpr>());
    EXPECT_TRUE(add->right().is<CallExpr>());
    
    auto left_call = add->left().as<CallExpr>();
    auto right_call = add->right().as<CallExpr>();
    
    auto left_callee = left_call->callee().as<Identifier>();
    auto right_callee = right_call->callee().as<Identifier>();
    
    EXPECT_EQ(left_callee->name(), "max");
    EXPECT_EQ(right_callee->name(), "min");
}

// === Unary Expression Edge Cases ===

TEST_F(ExpressionTest, MultipleUnaryOperators) {
    auto expr_result = parse_expression("!!true");
    ASSERT_TRUE(expr_result.has_value());
    
    auto& expr = expr_result.value();
    EXPECT_TRUE(expr->is<UnaryExpr>());
    
    auto outer_not = expr->as<UnaryExpr>();
    EXPECT_EQ(outer_not->get_operator(), UnaryExpr::Operator::Not);
    
    EXPECT_TRUE(outer_not->operand().is<UnaryExpr>());
    auto inner_not = outer_not->operand().as<UnaryExpr>();
    EXPECT_EQ(inner_not->get_operator(), UnaryExpr::Operator::Not);
    
    EXPECT_TRUE(inner_not->operand().is<BoolLiteral>());
}

TEST_F(ExpressionTest, UnaryWithFunctionCall) {
    auto expr_result = parse_expression("-f(x)");
    ASSERT_TRUE(expr_result.has_value());
    
    auto& expr = expr_result.value();
    EXPECT_TRUE(expr->is<UnaryExpr>());
    
    auto unary = expr->as<UnaryExpr>();
    EXPECT_EQ(unary->get_operator(), UnaryExpr::Operator::Minus);
    
    EXPECT_TRUE(unary->operand().is<CallExpr>());
    auto call = unary->operand().as<CallExpr>();
    
    auto callee = call->callee().as<Identifier>();
    EXPECT_EQ(callee->name(), "f");
}

// === Literal Value Tests ===

TEST_F(ExpressionTest, IntegerLiteralValues) {
    struct TestCase {
        std::string source;
        photon::i64 expected;
    };
    
    std::vector<TestCase> test_cases = {
        {"0", 0},
        {"42", 42},
        {"123456789", 123456789},
        {"-42", -42}, // This will be a unary expression
    };
    
    for (const auto& test : test_cases) {
        auto expr_result = parse_expression(test.source);
        ASSERT_TRUE(expr_result.has_value()) << "Failed to parse: " << test.source;
        
        if (test.source[0] == '-') {
            // Negative number is parsed as unary expression
            auto& expr = expr_result.value();
            EXPECT_TRUE(expr->is<UnaryExpr>());
            auto unary = expr->as<UnaryExpr>();
            EXPECT_TRUE(unary->operand().is<IntegerLiteral>());
            auto literal = unary->operand().as<IntegerLiteral>();
            EXPECT_EQ(literal->value(), -test.expected);
        } else {
            auto& expr = expr_result.value();
            EXPECT_TRUE(expr->is<IntegerLiteral>());
            auto literal = expr->as<IntegerLiteral>();
            EXPECT_EQ(literal->value(), test.expected);
        }
    }
}

TEST_F(ExpressionTest, FloatLiteralValues) {
    struct TestCase {
        std::string source;
        photon::f64 expected;
    };
    
    std::vector<TestCase> test_cases = {
        {"0.0", 0.0},
        {"3.14", 3.14},
        {"1.5e10", 1.5e10},
        {"2.71828", 2.71828},
    };
    
    for (const auto& test : test_cases) {
        auto expr_result = parse_expression(test.source);
        ASSERT_TRUE(expr_result.has_value()) << "Failed to parse: " << test.source;
        
        auto& expr = expr_result.value();
        EXPECT_TRUE(expr->is<FloatLiteral>());
        auto literal = expr->as<FloatLiteral>();
        EXPECT_DOUBLE_EQ(literal->value(), test.expected);
    }
}

TEST_F(ExpressionTest, StringLiteralValues) {
    struct TestCase {
        std::string source;
        std::string expected;
    };
    
    std::vector<TestCase> test_cases = {
        {"\"hello\"", "hello"},
        {"\"world\"", "world"},
        {"\"\"", ""},
        {"\"hello world\"", "hello world"},
    };
    
    for (const auto& test : test_cases) {
        auto expr_result = parse_expression(test.source);
        ASSERT_TRUE(expr_result.has_value()) << "Failed to parse: " << test.source;
        
        auto& expr = expr_result.value();
        EXPECT_TRUE(expr->is<StringLiteral>());
        auto literal = expr->as<StringLiteral>();
        EXPECT_EQ(std::string(literal->value()), test.expected);
    }
}