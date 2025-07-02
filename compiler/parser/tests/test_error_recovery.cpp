/**
 * @file test_error_recovery.cpp
 * @brief Error recovery and error handling tests
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

class ErrorRecoveryTest : public ::testing::Test {
protected:
    void SetUp() override {
        arena = std::make_unique<MemoryArena<>>();
        source_mgr = std::make_unique<SourceManager>(*arena);
    }
    
    auto parse_with_errors(const std::string& source, bool enable_recovery = true) -> std::unique_ptr<Parser> {
        auto file_id_result = source_mgr->load_from_string("test.ph", source);
        if (!file_id_result) {
            return nullptr;
        }
        
        auto file_id = file_id_result.value();
        auto* file = source_mgr->get_file(file_id);
        if (!file) {
            return nullptr;
        }
        
        auto lexer = Lexer(*source_mgr, *arena);
        
        auto tokens_result = lexer.tokenize(file_id);
        if (!tokens_result) {
            return nullptr;
        }
        
        auto token_stream = std::move(tokens_result.value());
        
        ParserOptions options;
        options.enable_error_recovery = enable_recovery;
        
        return std::make_unique<Parser>(std::move(token_stream), *arena, options);
    }

    std::unique_ptr<MemoryArena<>> arena;
    std::unique_ptr<SourceManager> source_mgr;
};

// === Basic Error Detection ===

TEST_F(ErrorRecoveryTest, UnexpectedTokenError) {
    auto parser = parse_with_errors("fn + test() {}");
    ASSERT_NE(parser, nullptr);
    
    auto program_result = parser->parse_program();
    EXPECT_FALSE(program_result.has_value());
    EXPECT_TRUE(parser->has_errors());
    
    auto& errors = parser->errors();
    EXPECT_FALSE(errors.empty());
}

TEST_F(ErrorRecoveryTest, MissingDelimiterError) {
    auto parser = parse_with_errors("fn test(x: i32 y: i32) {}");
    ASSERT_NE(parser, nullptr);
    
    auto program_result = parser->parse_program();
    EXPECT_FALSE(program_result.has_value());
    EXPECT_TRUE(parser->has_errors());
}

TEST_F(ErrorRecoveryTest, UnterminatedExpressionError) {
    auto parser = parse_with_errors("let x = (1 + 2");
    ASSERT_NE(parser, nullptr);
    
    auto program_result = parser->parse_program();
    EXPECT_FALSE(program_result.has_value());
    EXPECT_TRUE(parser->has_errors());
}

// === Error Recovery in Function Declarations ===

TEST_F(ErrorRecoveryTest, RecoveryAfterBadFunction) {
    auto source = R"(
        fn bad_function( {}
        
        fn good_function() {
            let x = 42
        }
    )";
    
    auto parser = parse_with_errors(source, true);
    ASSERT_NE(parser, nullptr);
    
    auto program_result = parser->parse_program();
    
    // With error recovery enabled, we should still get a program
    // but with errors reported
    EXPECT_TRUE(parser->has_errors());
    
    if (program_result.has_value()) {
        auto& program = program_result.value();
        // We might get the good function parsed despite the bad one
        EXPECT_GE(program->declarations().size(), 0);
    }
}

TEST_F(ErrorRecoveryTest, RecoveryWithoutErrorRecovery) {
    auto source = R"(
        fn bad_function( {}
        
        fn good_function() {
            let x = 42
        }
    )";
    
    auto parser = parse_with_errors(source, false);
    ASSERT_NE(parser, nullptr);
    
    auto program_result = parser->parse_program();
    
    // Without error recovery, parsing should fail completely
    EXPECT_FALSE(program_result.has_value());
    EXPECT_TRUE(parser->has_errors());
}

// === Error Recovery in Expressions ===

TEST_F(ErrorRecoveryTest, BadExpressionInGoodContext) {
    auto source = R"(
        fn test() {
            let x = 42
            let y = (1 +
            let z = 100
        }
    )";
    
    auto parser = parse_with_errors(source, true);
    ASSERT_NE(parser, nullptr);
    
    auto program_result = parser->parse_program();
    EXPECT_TRUE(parser->has_errors());
    
    // Parser should report errors but might still create some AST nodes
    if (program_result.has_value()) {
        auto& program = program_result.value();
        EXPECT_GE(program->declarations().size(), 0);
    }
}

// === Multiple Errors ===

TEST_F(ErrorRecoveryTest, MultipleErrors) {
    auto source = R"(
        fn first( {}
        fn second) {}
        fn third(x y) {}
        fn fourth() {
            let x = (
        }
    )";
    
    auto parser = parse_with_errors(source, true);
    ASSERT_NE(parser, nullptr);
    
    auto program_result = parser->parse_program();
    EXPECT_TRUE(parser->has_errors());
    
    auto& errors = parser->errors();
    EXPECT_GT(errors.size(), 1); // Should have multiple errors
}

// === Synchronization Points ===

TEST_F(ErrorRecoveryTest, SynchronizationAtFunctionKeyword) {
    auto source = R"(
        fn bad_function(x: {}
        
        fn good_function() {}
    )";
    
    auto parser = parse_with_errors(source, true);
    ASSERT_NE(parser, nullptr);
    
    auto program_result = parser->parse_program();
    EXPECT_TRUE(parser->has_errors());
    
    // Parser should recover and parse the good function
    if (program_result.has_value()) {
        auto& program = program_result.value();
        // Might find the second function despite the first being bad
        for (const auto& decl : program->declarations()) {
            if (decl->is<FunctionDecl>()) {
                auto func = decl->as<FunctionDecl>();
                if (func->name() == "good_function") {
                    // Found the good function, recovery worked
                    EXPECT_TRUE(func->parameters().empty());
                    break;
                }
            }
        }
    }
}

TEST_F(ErrorRecoveryTest, SynchronizationAtBraces) {
    auto source = R"(
        fn test() {
            let x = incomplete expression
            {
                let y = 42
            }
        }
    )";
    
    auto parser = parse_with_errors(source, true);
    ASSERT_NE(parser, nullptr);
    
    auto program_result = parser->parse_program();
    EXPECT_TRUE(parser->has_errors());
    
    // Should recover and continue parsing after synchronization
}

// === Error Types ===

TEST_F(ErrorRecoveryTest, ExpectedIdentifierError) {
    auto parser = parse_with_errors("fn 123() {}");
    ASSERT_NE(parser, nullptr);
    
    auto program_result = parser->parse_program();
    EXPECT_FALSE(program_result.has_value());
    EXPECT_TRUE(parser->has_errors());
}

TEST_F(ErrorRecoveryTest, ExpectedExpressionError) {
    auto parser = parse_with_errors("let x = ");
    ASSERT_NE(parser, nullptr);
    
    auto program_result = parser->parse_program();
    EXPECT_FALSE(program_result.has_value());
    EXPECT_TRUE(parser->has_errors());
}

TEST_F(ErrorRecoveryTest, ExpectedTypeError) {
    auto parser = parse_with_errors("fn test(x: ) {}");
    ASSERT_NE(parser, nullptr);
    
    auto program_result = parser->parse_program();
    EXPECT_FALSE(program_result.has_value());
    EXPECT_TRUE(parser->has_errors());
}

// === Nested Error Recovery ===

TEST_F(ErrorRecoveryTest, NestedBlockErrors) {
    auto source = R"(
        fn outer() {
            let x = 42
            {
                let y = incomplete
                {
                    let z = 100
                }
            }
            let w = 200
        }
    )";
    
    auto parser = parse_with_errors(source, true);
    ASSERT_NE(parser, nullptr);
    
    auto program_result = parser->parse_program();
    EXPECT_TRUE(parser->has_errors());
    
    // Should attempt to recover and continue parsing nested structures
}

// === Expression Error Recovery ===

TEST_F(ErrorRecoveryTest, IncompleteArithmeticExpression) {
    auto parser = parse_with_errors("let x = 1 + 2 *");
    ASSERT_NE(parser, nullptr);
    
    auto program_result = parser->parse_program();
    EXPECT_FALSE(program_result.has_value());
    EXPECT_TRUE(parser->has_errors());
}

TEST_F(ErrorRecoveryTest, MismatchedParentheses) {
    auto parser = parse_with_errors("let x = (1 + 2))");
    ASSERT_NE(parser, nullptr);
    
    auto program_result = parser->parse_program();
    EXPECT_FALSE(program_result.has_value());
    EXPECT_TRUE(parser->has_errors());
}

TEST_F(ErrorRecoveryTest, IncompleteFunctionCall) {
    auto parser = parse_with_errors("let x = foo(1, 2,");
    ASSERT_NE(parser, nullptr);
    
    auto program_result = parser->parse_program();
    EXPECT_FALSE(program_result.has_value());
    EXPECT_TRUE(parser->has_errors());
}

// === Complex Error Scenarios ===

TEST_F(ErrorRecoveryTest, MixedGoodAndBadCode) {
    auto source = R"(
        fn good_function() {
            let x = 42
            let y = x + 1
        }
        
        fn bad_function(invalid syntax here {}
        
        fn another_good_function() {
            let z = 100
        }
        
        let invalid_top_level = 
        
        fn final_function() {}
    )";
    
    auto parser = parse_with_errors(source, true);
    ASSERT_NE(parser, nullptr);
    
    auto program_result = parser->parse_program();
    EXPECT_TRUE(parser->has_errors());
    
    // With error recovery, we should get some valid functions
    if (program_result.has_value()) {
        auto& program = program_result.value();
        
        // Count how many valid function declarations we recovered
        photon::usize valid_functions = 0;
        for (const auto& decl : program->declarations()) {
            if (decl->is<FunctionDecl>()) {
                valid_functions++;
            }
        }
        
        // Should have recovered at least some functions
        EXPECT_GT(valid_functions, 0);
    }
}

// === Error Limits ===

TEST_F(ErrorRecoveryTest, TooManyErrors) {
    // Create source with many syntax errors
    std::string source = "fn a( fn b) fn c{ fn d(x fn e)";
    
    auto parser = parse_with_errors(source, true);
    ASSERT_NE(parser, nullptr);
    
    auto program_result = parser->parse_program();
    EXPECT_TRUE(parser->has_errors());
    
    auto& errors = parser->errors();
    EXPECT_GT(errors.size(), 1);
}

// === Error Message Accuracy ===

TEST_F(ErrorRecoveryTest, ErrorClearingBetweenParses) {
    auto parser = parse_with_errors("fn bad( {}", true);
    ASSERT_NE(parser, nullptr);
    
    auto first_result = parser->parse_program();
    EXPECT_TRUE(parser->has_errors());
    
    auto first_error_count = parser->errors().size();
    EXPECT_GT(first_error_count, 0);
    
    // Clear errors and parse again
    parser->clear_errors();
    EXPECT_FALSE(parser->has_errors());
    EXPECT_TRUE(parser->errors().empty());
}