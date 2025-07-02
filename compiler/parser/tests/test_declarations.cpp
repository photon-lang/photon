/**
 * @file test_declarations.cpp
 * @brief Declaration parsing tests
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

class DeclarationTest : public ::testing::Test {
protected:
    void SetUp() override {
        arena = std::make_unique<MemoryArena<>>();
        source_mgr = std::make_unique<SourceManager>(*arena);
    }
    
    auto parse_program(const std::string& source) -> photon::Result<ASTPtr<Program>, ParseError> {
        auto file_id_result = source_mgr->load_from_string("test.ph", source);
        if (!file_id_result) {
            return photon::Result<ASTPtr<Program>, ParseError>(ParseError::UnexpectedEof);
        }
        
        auto file_id = file_id_result.value();
        auto* file = source_mgr->get_file(file_id);
        if (!file) {
            return photon::Result<ASTPtr<Program>, ParseError>(ParseError::UnexpectedEof);
        }
        
        auto lexer = Lexer(*source_mgr, *arena);
        
        auto tokens_result = lexer.tokenize(file_id);
        if (!tokens_result) {
            return photon::Result<ASTPtr<Program>, ParseError>(ParseError::InvalidSyntax);
        }
        
        auto token_stream = std::move(tokens_result.value());
        auto parser = Parser(std::move(token_stream), *arena);
        
        return parser.parse_program();
    }

    std::unique_ptr<MemoryArena<>> arena;
    std::unique_ptr<SourceManager> source_mgr;
};

// === Function Declaration Tests ===

TEST_F(DeclarationTest, SimpleFunctionDeclaration) {
    auto source = "fn main() {}";
    
    auto program_result = parse_program(source);
    ASSERT_TRUE(program_result.has_value());
    
    auto& program = program_result.value();
    EXPECT_EQ(program->declarations().size(), 1);
    
    auto& decl = program->declarations()[0];
    EXPECT_TRUE(decl->is<FunctionDecl>());
    
    auto func_decl = decl->as<FunctionDecl>();
    EXPECT_EQ(func_decl->name(), "main");
    EXPECT_TRUE(func_decl->parameters().empty());
    EXPECT_EQ(func_decl->return_type(), nullptr);
    
    auto& body = func_decl->body();
    EXPECT_TRUE(body.statements().empty());
}

TEST_F(DeclarationTest, FunctionWithParameters) {
    auto source = "fn add(x: i32, y: i32) {}";
    
    auto program_result = parse_program(source);
    ASSERT_TRUE(program_result.has_value());
    
    auto& program = program_result.value();
    EXPECT_EQ(program->declarations().size(), 1);
    
    auto func_decl = program->declarations()[0]->as<FunctionDecl>();
    EXPECT_EQ(func_decl->name(), "add");
    
    auto& params = func_decl->parameters();
    EXPECT_EQ(params.size(), 2);
    
    EXPECT_EQ(params[0].name, "x");
    EXPECT_TRUE(params[0].type->is<Identifier>());
    auto x_type = params[0].type->as<Identifier>();
    EXPECT_EQ(x_type->name(), "i32");
    
    EXPECT_EQ(params[1].name, "y");
    EXPECT_TRUE(params[1].type->is<Identifier>());
    auto y_type = params[1].type->as<Identifier>();
    EXPECT_EQ(y_type->name(), "i32");
}

TEST_F(DeclarationTest, FunctionWithReturnType) {
    auto source = "fn get_value() -> i32 {}";
    
    auto program_result = parse_program(source);
    ASSERT_TRUE(program_result.has_value());
    
    auto& program = program_result.value();
    EXPECT_EQ(program->declarations().size(), 1);
    
    auto func_decl = program->declarations()[0]->as<FunctionDecl>();
    EXPECT_EQ(func_decl->name(), "get_value");
    EXPECT_TRUE(func_decl->parameters().empty());
    EXPECT_NE(func_decl->return_type(), nullptr);
    
    auto return_type = func_decl->return_type()->as<Identifier>();
    EXPECT_EQ(return_type->name(), "i32");
}

TEST_F(DeclarationTest, FunctionWithParametersAndReturnType) {
    auto source = "fn multiply(a: f64, b: f64) -> f64 {}";
    
    auto program_result = parse_program(source);
    ASSERT_TRUE(program_result.has_value());
    
    auto& program = program_result.value();
    auto func_decl = program->declarations()[0]->as<FunctionDecl>();
    
    EXPECT_EQ(func_decl->name(), "multiply");
    EXPECT_EQ(func_decl->parameters().size(), 2);
    EXPECT_NE(func_decl->return_type(), nullptr);
    
    auto& params = func_decl->parameters();
    EXPECT_EQ(params[0].name, "a");
    EXPECT_EQ(params[1].name, "b");
    
    auto return_type = func_decl->return_type()->as<Identifier>();
    EXPECT_EQ(return_type->name(), "f64");
}

TEST_F(DeclarationTest, FunctionWithBody) {
    auto source = R"(
        fn calculate(x: i32) -> i32 {
            let result = x * 2
            let final_result = result + 1
        }
    )";
    
    auto program_result = parse_program(source);
    ASSERT_TRUE(program_result.has_value());
    
    auto& program = program_result.value();
    auto func_decl = program->declarations()[0]->as<FunctionDecl>();
    
    EXPECT_EQ(func_decl->name(), "calculate");
    
    auto& body = func_decl->body();
    EXPECT_EQ(body.statements().size(), 2);
    
    for (const auto& stmt : body.statements()) {
        EXPECT_TRUE(stmt->is<VarDecl>());
    }
    
    auto first_var = body.statements()[0]->as<VarDecl>();
    auto second_var = body.statements()[1]->as<VarDecl>();
    
    EXPECT_EQ(first_var->name(), "result");
    EXPECT_EQ(second_var->name(), "final_result");
}

// === Multiple Function Declarations ===

TEST_F(DeclarationTest, MultipleFunctions) {
    auto source = R"(
        fn first() {}
        
        fn second(x: i32) -> i32 {
            let value = x * 2
        }
        
        fn third(a: f64, b: f64) -> f64 {}
    )";
    
    auto program_result = parse_program(source);
    ASSERT_TRUE(program_result.has_value());
    
    auto& program = program_result.value();
    EXPECT_EQ(program->declarations().size(), 3);
    
    auto first_func = program->declarations()[0]->as<FunctionDecl>();
    auto second_func = program->declarations()[1]->as<FunctionDecl>();
    auto third_func = program->declarations()[2]->as<FunctionDecl>();
    
    EXPECT_EQ(first_func->name(), "first");
    EXPECT_TRUE(first_func->parameters().empty());
    EXPECT_EQ(first_func->return_type(), nullptr);
    
    EXPECT_EQ(second_func->name(), "second");
    EXPECT_EQ(second_func->parameters().size(), 1);
    EXPECT_NE(second_func->return_type(), nullptr);
    
    EXPECT_EQ(third_func->name(), "third");
    EXPECT_EQ(third_func->parameters().size(), 2);
    EXPECT_NE(third_func->return_type(), nullptr);
}

// === Complex Parameter Lists ===

TEST_F(DeclarationTest, FunctionWithManyParameters) {
    auto source = "fn complex(a: i8, b: i16, c: i32, d: i64, e: f32, f: f64, g: bool, h: str) {}";
    
    auto program_result = parse_program(source);
    ASSERT_TRUE(program_result.has_value());
    
    auto& program = program_result.value();
    auto func_decl = program->declarations()[0]->as<FunctionDecl>();
    
    EXPECT_EQ(func_decl->name(), "complex");
    
    auto& params = func_decl->parameters();
    EXPECT_EQ(params.size(), 8);
    
    std::vector<std::string> expected_names = {"a", "b", "c", "d", "e", "f", "g", "h"};
    std::vector<std::string> expected_types = {"i8", "i16", "i32", "i64", "f32", "f64", "bool", "str"};
    
    for (photon::usize i = 0; i < params.size(); ++i) {
        EXPECT_EQ(params[i].name, expected_names[i]);
        
        auto type_id = params[i].type->as<Identifier>();
        EXPECT_EQ(type_id->name(), expected_types[i]);
    }
}

TEST_F(DeclarationTest, FunctionWithSingleParameter) {
    auto source = "fn process(data: str) {}";
    
    auto program_result = parse_program(source);
    ASSERT_TRUE(program_result.has_value());
    
    auto& program = program_result.value();
    auto func_decl = program->declarations()[0]->as<FunctionDecl>();
    
    EXPECT_EQ(func_decl->name(), "process");
    EXPECT_EQ(func_decl->parameters().size(), 1);
    
    auto& param = func_decl->parameters()[0];
    EXPECT_EQ(param.name, "data");
    
    auto type_id = param.type->as<Identifier>();
    EXPECT_EQ(type_id->name(), "str");
}

// === Nested Function Bodies ===

TEST_F(DeclarationTest, FunctionWithNestedBlocks) {
    auto source = R"(
        fn nested_example() {
            let outer = 1
            {
                let inner = 2
                {
                    let deeply_nested = 3
                }
            }
        }
    )";
    
    auto program_result = parse_program(source);
    ASSERT_TRUE(program_result.has_value());
    
    auto& program = program_result.value();
    auto func_decl = program->declarations()[0]->as<FunctionDecl>();
    
    EXPECT_EQ(func_decl->name(), "nested_example");
    
    auto& body = func_decl->body();
    EXPECT_EQ(body.statements().size(), 2);
    
    EXPECT_TRUE(body.statements()[0]->is<VarDecl>());
    EXPECT_TRUE(body.statements()[1]->is<Block>());
    
    auto nested_block = body.statements()[1]->as<Block>();
    EXPECT_EQ(nested_block->statements().size(), 2);
    
    EXPECT_TRUE(nested_block->statements()[0]->is<VarDecl>());
    EXPECT_TRUE(nested_block->statements()[1]->is<Block>());
}

TEST_F(DeclarationTest, FunctionWithComplexBody) {
    auto source = R"(
        fn complex_function(x: i32, y: i32) -> i32 {
            let sum = x + y
            let product = x * y
            let difference = x - y
            let result = sum + product - difference
        }
    )";
    
    auto program_result = parse_program(source);
    ASSERT_TRUE(program_result.has_value());
    
    auto& program = program_result.value();
    auto func_decl = program->declarations()[0]->as<FunctionDecl>();
    
    EXPECT_EQ(func_decl->name(), "complex_function");
    EXPECT_EQ(func_decl->parameters().size(), 2);
    
    auto& body = func_decl->body();
    EXPECT_EQ(body.statements().size(), 4);
    
    for (const auto& stmt : body.statements()) {
        EXPECT_TRUE(stmt->is<VarDecl>());
    }
    
    auto result_var = body.statements()[3]->as<VarDecl>();
    EXPECT_EQ(result_var->name(), "result");
    EXPECT_TRUE(result_var->init()->is<BinaryExpr>());
}

// === Function Name Variations ===

TEST_F(DeclarationTest, FunctionNamesWithUnderscores) {
    auto source = R"(
        fn snake_case_function() {}
        fn another_function_name() {}
        fn _private_function() {}
    )";
    
    auto program_result = parse_program(source);
    ASSERT_TRUE(program_result.has_value());
    
    auto& program = program_result.value();
    EXPECT_EQ(program->declarations().size(), 3);
    
    auto first_func = program->declarations()[0]->as<FunctionDecl>();
    auto second_func = program->declarations()[1]->as<FunctionDecl>();
    auto third_func = program->declarations()[2]->as<FunctionDecl>();
    
    EXPECT_EQ(first_func->name(), "snake_case_function");
    EXPECT_EQ(second_func->name(), "another_function_name");
    EXPECT_EQ(third_func->name(), "_private_function");
}

// === Error Cases ===

TEST_F(DeclarationTest, MissingFunctionName) {
    auto source = "fn () {}";
    
    auto program_result = parse_program(source);
    EXPECT_FALSE(program_result.has_value());
}

TEST_F(DeclarationTest, MissingParameterList) {
    auto source = "fn test {}";
    
    auto program_result = parse_program(source);
    EXPECT_FALSE(program_result.has_value());
}

TEST_F(DeclarationTest, MissingParameterType) {
    auto source = "fn test(x) {}";
    
    auto program_result = parse_program(source);
    EXPECT_FALSE(program_result.has_value());
}

TEST_F(DeclarationTest, MissingParameterName) {
    auto source = "fn test(: i32) {}";
    
    auto program_result = parse_program(source);
    EXPECT_FALSE(program_result.has_value());
}

TEST_F(DeclarationTest, MissingFunctionBody) {
    auto source = "fn test()";
    
    auto program_result = parse_program(source);
    EXPECT_FALSE(program_result.has_value());
}

TEST_F(DeclarationTest, MissingReturnTypeAfterArrow) {
    auto source = "fn test() -> {}";
    
    auto program_result = parse_program(source);
    EXPECT_FALSE(program_result.has_value());
}

TEST_F(DeclarationTest, UnterminatedParameterList) {
    auto source = "fn test(x: i32, y: i32 {}";
    
    auto program_result = parse_program(source);
    EXPECT_FALSE(program_result.has_value());
}

TEST_F(DeclarationTest, MissingCommaInParameterList) {
    auto source = "fn test(x: i32 y: i32) {}";
    
    auto program_result = parse_program(source);
    EXPECT_FALSE(program_result.has_value());
}

// === Edge Cases ===

TEST_F(DeclarationTest, EmptyParameterList) {
    auto source = "fn empty_params() {}";
    
    auto program_result = parse_program(source);
    ASSERT_TRUE(program_result.has_value());
    
    auto& program = program_result.value();
    auto func_decl = program->declarations()[0]->as<FunctionDecl>();
    
    EXPECT_EQ(func_decl->name(), "empty_params");
    EXPECT_TRUE(func_decl->parameters().empty());
}

TEST_F(DeclarationTest, FunctionWithTrailingCommaInParameters) {
    // This should fail as trailing commas are not supported
    auto source = "fn test(x: i32,) {}";
    
    auto program_result = parse_program(source);
    EXPECT_FALSE(program_result.has_value());
}