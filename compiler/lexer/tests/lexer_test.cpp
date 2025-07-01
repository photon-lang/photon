/**
 * @file lexer_test.cpp
 * @brief Comprehensive test suite for the Photon lexer
 * @author Photon Compiler Team
 * @version 1.0.0
 */

#include "photon/lexer/lexer.hpp"
#include "photon/lexer/token.hpp"
#include "photon/source/source_manager.hpp"
#include "photon/memory/arena.hpp"
#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>
#include <thread>
#include <atomic>

using namespace photon;
using namespace photon::lexer;
using namespace photon::source;
using namespace photon::memory;

namespace {

class LexerTest : public ::testing::Test {
protected:
    void SetUp() override {
        arena_ = std::make_unique<MemoryArena<>>();
        source_manager_ = std::make_unique<SourceManager>(*arena_);
        lexer_ = LexerFactory::create_test_lexer(*source_manager_, *arena_);
        
        // Create test directory
        test_dir_ = std::filesystem::temp_directory_path() / "photon_lexer_test";
        std::filesystem::create_directories(test_dir_);
    }
    
    void TearDown() override {
        lexer_.reset();
        source_manager_.reset();
        arena_.reset();
        
        // Clean up test files
        std::error_code ec;
        std::filesystem::remove_all(test_dir_, ec);
    }
    
    auto create_test_file(StringView filename, StringView content) -> String {
        auto path = test_dir_ / filename;
        std::ofstream file(path);
        file << content;
        file.close();
        return path.string();
    }
    
    auto tokenize_string(StringView content) -> Result<TokenStream, LexicalError> {
        return lexer_->tokenize(content, "<test>");
    }
    
    auto expect_token(const Token& token, TokenType expected_type) -> void {
        EXPECT_EQ(token.type, expected_type) 
            << "Expected " << token_type_name(expected_type) 
            << " but got " << token_type_name(token.type);
    }
    
    auto expect_token(const Token& token, TokenType expected_type, StringView expected_text) -> void {
        expect_token(token, expected_type);
        EXPECT_EQ(token.text(), expected_text);
    }
    
    template<typename T>
    auto expect_token_value(const Token& token, TokenType expected_type, const T& expected_value) -> void {
        expect_token(token, expected_type);
        ASSERT_TRUE(token.value.is<T>());
        EXPECT_EQ(token.value.get<T>(), expected_value);
    }

protected:
    std::unique_ptr<MemoryArena<>> arena_;
    std::unique_ptr<SourceManager> source_manager_;
    std::unique_ptr<ILexer> lexer_;
    std::filesystem::path test_dir_;
};

/* Basic Token Tests */

TEST_F(LexerTest, HandlesEmptyInput) {
    auto result = tokenize_string("");
    ASSERT_TRUE(result.has_value());
    
    auto stream = result.value();
    EXPECT_EQ(stream.size(), 1u); // Just EOF
    expect_token(stream.current(), TokenType::Eof);
}

TEST_F(LexerTest, HandlesWhitespaceOnly) {
    auto result = tokenize_string("   \t  \n  \r\n  ");
    ASSERT_TRUE(result.has_value());
    
    auto stream = result.value();
    EXPECT_EQ(stream.size(), 1u); // Just EOF (whitespace ignored)
    expect_token(stream.current(), TokenType::Eof);
}

TEST_F(LexerTest, TokenizesSimpleIdentifier) {
    auto result = tokenize_string("hello");
    ASSERT_TRUE(result.has_value());
    
    auto stream = result.value();
    EXPECT_EQ(stream.size(), 2u); // identifier + EOF
    
    expect_token(stream.current(), TokenType::Identifier, "hello");
    stream.advance();
    expect_token(stream.current(), TokenType::Eof);
}

TEST_F(LexerTest, TokenizesMultipleIdentifiers) {
    auto result = tokenize_string("foo bar baz");
    ASSERT_TRUE(result.has_value());
    
    auto stream = result.value();
    EXPECT_EQ(stream.size(), 4u); // 3 identifiers + EOF
    
    expect_token(stream.current(), TokenType::Identifier, "foo");
    stream.advance();
    expect_token(stream.current(), TokenType::Identifier, "bar");
    stream.advance();
    expect_token(stream.current(), TokenType::Identifier, "baz");
    stream.advance();
    expect_token(stream.current(), TokenType::Eof);
}

/* Keyword Tests */

TEST_F(LexerTest, RecognizesControlFlowKeywords) {
    auto result = tokenize_string("if else elif match while for loop break continue return");
    ASSERT_TRUE(result.has_value());
    
    auto stream = result.value();
    expect_token(stream.current(), TokenType::KwIf);
    stream.advance();
    expect_token(stream.current(), TokenType::KwElse);
    stream.advance();
    expect_token(stream.current(), TokenType::KwElif);
    stream.advance();
    expect_token(stream.current(), TokenType::KwMatch);
    stream.advance();
    expect_token(stream.current(), TokenType::KwWhile);
    stream.advance();
    expect_token(stream.current(), TokenType::KwFor);
    stream.advance();
    expect_token(stream.current(), TokenType::KwLoop);
    stream.advance();
    expect_token(stream.current(), TokenType::KwBreak);
    stream.advance();
    expect_token(stream.current(), TokenType::KwContinue);
    stream.advance();
    expect_token(stream.current(), TokenType::KwReturn);
}

TEST_F(LexerTest, RecognizesDeclarationKeywords) {
    auto result = tokenize_string("fn struct enum trait impl type const static let mut");
    ASSERT_TRUE(result.has_value());
    
    auto stream = result.value();
    expect_token(stream.current(), TokenType::KwFn);
    stream.advance();
    expect_token(stream.current(), TokenType::KwStruct);
    stream.advance();
    expect_token(stream.current(), TokenType::KwEnum);
    stream.advance();
    expect_token(stream.current(), TokenType::KwTrait);
    stream.advance();
    expect_token(stream.current(), TokenType::KwImpl);
    stream.advance();
    expect_token(stream.current(), TokenType::KwType);
    stream.advance();
    expect_token(stream.current(), TokenType::KwConst);
    stream.advance();
    expect_token(stream.current(), TokenType::KwStatic);
    stream.advance();
    expect_token(stream.current(), TokenType::KwLet);
    stream.advance();
    expect_token(stream.current(), TokenType::KwMut);
}

TEST_F(LexerTest, RecognizesBuiltinTypes) {
    auto result = tokenize_string("i8 i16 i32 i64 u8 u16 u32 u64 f32 f64 bool char str");
    ASSERT_TRUE(result.has_value());
    
    auto stream = result.value();
    expect_token(stream.current(), TokenType::KwI8);
    stream.advance();
    expect_token(stream.current(), TokenType::KwI16);
    stream.advance();
    expect_token(stream.current(), TokenType::KwI32);
    stream.advance();
    expect_token(stream.current(), TokenType::KwI64);
    stream.advance();
    expect_token(stream.current(), TokenType::KwU8);
    stream.advance();
    expect_token(stream.current(), TokenType::KwU16);
    stream.advance();
    expect_token(stream.current(), TokenType::KwU32);
    stream.advance();
    expect_token(stream.current(), TokenType::KwU64);
    stream.advance();
    expect_token(stream.current(), TokenType::KwF32);
    stream.advance();
    expect_token(stream.current(), TokenType::KwF64);
    stream.advance();
    expect_token(stream.current(), TokenType::KwBool);
    stream.advance();
    expect_token(stream.current(), TokenType::KwChar);
    stream.advance();
    expect_token(stream.current(), TokenType::KwStr);
}

/* Literal Tests */

TEST_F(LexerTest, TokenizesIntegerLiterals) {
    auto result = tokenize_string("42 0 123456789");
    ASSERT_TRUE(result.has_value());
    
    auto stream = result.value();
    expect_token_value(stream.current(), TokenType::IntegerLiteral, static_cast<i64>(42));
    stream.advance();
    expect_token_value(stream.current(), TokenType::IntegerLiteral, static_cast<i64>(0));
    stream.advance();
    expect_token_value(stream.current(), TokenType::IntegerLiteral, static_cast<i64>(123456789));
}

TEST_F(LexerTest, TokenizesHexadecimalLiterals) {
    auto result = tokenize_string("0xFF 0x123ABC 0x0");
    ASSERT_TRUE(result.has_value());
    
    auto stream = result.value();
    expect_token_value(stream.current(), TokenType::IntegerLiteral, static_cast<i64>(255));
    stream.advance();
    expect_token_value(stream.current(), TokenType::IntegerLiteral, static_cast<i64>(0x123ABC));
    stream.advance();
    expect_token_value(stream.current(), TokenType::IntegerLiteral, static_cast<i64>(0));
}

TEST_F(LexerTest, TokenizesBinaryLiterals) {
    auto result = tokenize_string("0b1010 0b11111111 0b0");
    ASSERT_TRUE(result.has_value());
    
    auto stream = result.value();
    expect_token_value(stream.current(), TokenType::IntegerLiteral, static_cast<i64>(10));
    stream.advance();
    expect_token_value(stream.current(), TokenType::IntegerLiteral, static_cast<i64>(255));
    stream.advance();
    expect_token_value(stream.current(), TokenType::IntegerLiteral, static_cast<i64>(0));
}

TEST_F(LexerTest, TokenizesOctalLiterals) {
    auto result = tokenize_string("0o755 0o123 0o0");
    ASSERT_TRUE(result.has_value());
    
    auto stream = result.value();
    expect_token_value(stream.current(), TokenType::IntegerLiteral, static_cast<i64>(493)); // 755 octal = 493 decimal
    stream.advance();
    expect_token_value(stream.current(), TokenType::IntegerLiteral, static_cast<i64>(83));  // 123 octal = 83 decimal
    stream.advance();
    expect_token_value(stream.current(), TokenType::IntegerLiteral, static_cast<i64>(0));
}

TEST_F(LexerTest, TokenizesFloatLiterals) {
    auto result = tokenize_string("3.14 0.5 123.456");
    ASSERT_TRUE(result.has_value());
    
    auto stream = result.value();
    expect_token_value(stream.current(), TokenType::FloatLiteral, 3.14);
    stream.advance();
    expect_token_value(stream.current(), TokenType::FloatLiteral, 0.5);
    stream.advance();
    expect_token_value(stream.current(), TokenType::FloatLiteral, 123.456);
}

TEST_F(LexerTest, TokenizesScientificNotation) {
    auto result = tokenize_string("1.0e10 2.5E-3 1e5");
    ASSERT_TRUE(result.has_value());
    
    auto stream = result.value();
    expect_token_value(stream.current(), TokenType::FloatLiteral, 1.0e10);
    stream.advance();
    expect_token_value(stream.current(), TokenType::FloatLiteral, 2.5e-3);
    stream.advance();
    expect_token_value(stream.current(), TokenType::FloatLiteral, 1e5);
}

TEST_F(LexerTest, TokenizesStringLiterals) {
    auto result = tokenize_string(R"("hello" "world with spaces" "")");
    ASSERT_TRUE(result.has_value());
    
    auto stream = result.value();
    expect_token(stream.current(), TokenType::StringLiteral, "hello");
    stream.advance();
    expect_token(stream.current(), TokenType::StringLiteral, "world with spaces");
    stream.advance();
    expect_token(stream.current(), TokenType::StringLiteral, "");
}

TEST_F(LexerTest, TokenizesCharLiterals) {
    auto result = tokenize_string("'a' 'Z' '5'");
    ASSERT_TRUE(result.has_value());
    
    auto stream = result.value();
    expect_token(stream.current(), TokenType::CharLiteral, "a");
    stream.advance();
    expect_token(stream.current(), TokenType::CharLiteral, "Z");
    stream.advance();
    expect_token(stream.current(), TokenType::CharLiteral, "5");
}

TEST_F(LexerTest, TokenizesEscapeSequences) {
    auto result = tokenize_string(R"("hello\nworld" "tab\there" '\'' '\\')");
    ASSERT_TRUE(result.has_value());
    
    auto stream = result.value();
    expect_token(stream.current(), TokenType::StringLiteral, "hello\nworld");
    stream.advance();
    expect_token(stream.current(), TokenType::StringLiteral, "tab\there");
    stream.advance();
    expect_token(stream.current(), TokenType::CharLiteral, "'");
    stream.advance();
    expect_token(stream.current(), TokenType::CharLiteral, "\\");
}

TEST_F(LexerTest, TokenizesBooleanLiterals) {
    auto result = tokenize_string("true false");
    ASSERT_TRUE(result.has_value());
    
    auto stream = result.value();
    expect_token_value(stream.current(), TokenType::BoolLiteral, true);
    stream.advance();
    expect_token_value(stream.current(), TokenType::BoolLiteral, false);
}

/* Operator Tests */

TEST_F(LexerTest, TokenizesArithmeticOperators) {
    auto result = tokenize_string("+ - * / % **");
    ASSERT_TRUE(result.has_value());
    
    auto stream = result.value();
    expect_token(stream.current(), TokenType::Plus);
    stream.advance();
    expect_token(stream.current(), TokenType::Minus);
    stream.advance();
    expect_token(stream.current(), TokenType::Star);
    stream.advance();
    expect_token(stream.current(), TokenType::Slash);
    stream.advance();
    expect_token(stream.current(), TokenType::Percent);
    stream.advance();
    expect_token(stream.current(), TokenType::StarStar);
}

TEST_F(LexerTest, TokenizesComparisonOperators) {
    auto result = tokenize_string("== != < > <= >= <=>");
    ASSERT_TRUE(result.has_value());
    
    auto stream = result.value();
    expect_token(stream.current(), TokenType::Equal);
    stream.advance();
    expect_token(stream.current(), TokenType::NotEqual);
    stream.advance();
    expect_token(stream.current(), TokenType::Less);
    stream.advance();
    expect_token(stream.current(), TokenType::Greater);
    stream.advance();
    expect_token(stream.current(), TokenType::LessEqual);
    stream.advance();
    expect_token(stream.current(), TokenType::GreaterEqual);
    stream.advance();
    expect_token(stream.current(), TokenType::Spaceship);
}

TEST_F(LexerTest, TokenizesLogicalOperators) {
    auto result = tokenize_string("&& || !");
    ASSERT_TRUE(result.has_value());
    
    auto stream = result.value();
    expect_token(stream.current(), TokenType::And);
    stream.advance();
    expect_token(stream.current(), TokenType::Or);
    stream.advance();
    expect_token(stream.current(), TokenType::Not);
}

TEST_F(LexerTest, TokenizesAssignmentOperators) {
    auto result = tokenize_string("= += -= *= /= %= &= |= ^= <<= >>=");
    ASSERT_TRUE(result.has_value());
    
    auto stream = result.value();
    expect_token(stream.current(), TokenType::Assign);
    stream.advance();
    expect_token(stream.current(), TokenType::PlusAssign);
    stream.advance();
    expect_token(stream.current(), TokenType::MinusAssign);
    stream.advance();
    expect_token(stream.current(), TokenType::StarAssign);
    stream.advance();
    expect_token(stream.current(), TokenType::SlashAssign);
    stream.advance();
    expect_token(stream.current(), TokenType::PercentAssign);
    stream.advance();
    expect_token(stream.current(), TokenType::AndAssign);
    stream.advance();
    expect_token(stream.current(), TokenType::OrAssign);
    stream.advance();
    expect_token(stream.current(), TokenType::XorAssign);
    stream.advance();
    expect_token(stream.current(), TokenType::LeftShiftAssign);
    stream.advance();
    expect_token(stream.current(), TokenType::RightShiftAssign);
}

/* Delimiter and Punctuation Tests */

TEST_F(LexerTest, TokenizesDelimiters) {
    auto result = tokenize_string("( ) { } [ ]");
    ASSERT_TRUE(result.has_value());
    
    auto stream = result.value();
    expect_token(stream.current(), TokenType::LeftParen);
    stream.advance();
    expect_token(stream.current(), TokenType::RightParen);
    stream.advance();
    expect_token(stream.current(), TokenType::LeftBrace);
    stream.advance();
    expect_token(stream.current(), TokenType::RightBrace);
    stream.advance();
    expect_token(stream.current(), TokenType::LeftBracket);
    stream.advance();
    expect_token(stream.current(), TokenType::RightBracket);
}

TEST_F(LexerTest, TokenizesPunctuation) {
    auto result = tokenize_string(", ; : @ # $");
    ASSERT_TRUE(result.has_value());
    
    auto stream = result.value();
    expect_token(stream.current(), TokenType::Comma);
    stream.advance();
    expect_token(stream.current(), TokenType::Semicolon);
    stream.advance();
    expect_token(stream.current(), TokenType::Colon);
    stream.advance();
    expect_token(stream.current(), TokenType::At);
    stream.advance();
    expect_token(stream.current(), TokenType::Hash);
    stream.advance();
    expect_token(stream.current(), TokenType::Dollar);
}

/* Complex Expression Tests */

TEST_F(LexerTest, TokenizesSimpleFunction) {
    auto result = tokenize_string("fn add(a: i32, b: i32) -> i32 { a + b }");
    ASSERT_TRUE(result.has_value());
    
    auto stream = result.value();
    expect_token(stream.current(), TokenType::KwFn);
    stream.advance();
    expect_token(stream.current(), TokenType::Identifier, "add");
    stream.advance();
    expect_token(stream.current(), TokenType::LeftParen);
    stream.advance();
    expect_token(stream.current(), TokenType::Identifier, "a");
    stream.advance();
    expect_token(stream.current(), TokenType::Colon);
    stream.advance();
    expect_token(stream.current(), TokenType::KwI32);
    stream.advance();
    expect_token(stream.current(), TokenType::Comma);
    stream.advance();
    expect_token(stream.current(), TokenType::Identifier, "b");
    stream.advance();
    expect_token(stream.current(), TokenType::Colon);
    stream.advance();
    expect_token(stream.current(), TokenType::KwI32);
    stream.advance();
    expect_token(stream.current(), TokenType::RightParen);
    stream.advance();
    expect_token(stream.current(), TokenType::Arrow);
    stream.advance();
    expect_token(stream.current(), TokenType::KwI32);
    stream.advance();
    expect_token(stream.current(), TokenType::LeftBrace);
    stream.advance();
    expect_token(stream.current(), TokenType::Identifier, "a");
    stream.advance();
    expect_token(stream.current(), TokenType::Plus);
    stream.advance();
    expect_token(stream.current(), TokenType::Identifier, "b");
    stream.advance();
    expect_token(stream.current(), TokenType::RightBrace);
}

/* Error Handling Tests */

TEST_F(LexerTest, HandlesInvalidCharacter) {
    auto result = tokenize_string("hello @ world");
    ASSERT_TRUE(result.has_value());
    
    auto stream = result.value();
    expect_token(stream.current(), TokenType::Identifier, "hello");
    stream.advance();
    expect_token(stream.current(), TokenType::At);
    stream.advance();
    expect_token(stream.current(), TokenType::Identifier, "world");
}

TEST_F(LexerTest, HandlesUnterminatedString) {
    auto result = tokenize_string(R"("unterminated string)");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), LexicalError::UnterminatedString);
}

TEST_F(LexerTest, HandlesUnterminatedChar) {
    auto result = tokenize_string("'unterminated");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), LexicalError::UnterminatedChar);
}

TEST_F(LexerTest, HandlesInvalidEscape) {
    auto result = tokenize_string(R"("invalid \q escape")");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), LexicalError::InvalidEscape);
}

TEST_F(LexerTest, HandlesInvalidNumber) {
    auto result = tokenize_string("0xFF_INVALID");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), LexicalError::InvalidNumber);
}

/* Source Location Tests */

TEST_F(LexerTest, TracksSourceLocations) {
    auto result = tokenize_string("hello\nworld");
    ASSERT_TRUE(result.has_value());
    
    auto stream = result.value();
    auto& hello_token = stream.current();
    expect_token(hello_token, TokenType::Identifier, "hello");
    EXPECT_EQ(hello_token.location.line(), 1u);
    EXPECT_EQ(hello_token.location.column(), 1u);
    
    stream.advance();
    auto& world_token = stream.current();
    expect_token(world_token, TokenType::Identifier, "world");
    EXPECT_EQ(world_token.location.line(), 2u);
    EXPECT_EQ(world_token.location.column(), 1u);
}

/* Performance Tests */

TEST_F(LexerTest, TokenizesLargeFile) {
    // Create a large source file
    String large_content;
    large_content.reserve(100000);
    for (int i = 0; i < 10000; ++i) {
        large_content += "fn function_" + std::to_string(i) + "() -> i32 { " + std::to_string(i) + " }\n";
    }
    
    auto start = std::chrono::high_resolution_clock::now();
    auto result = tokenize_string(large_content);
    auto end = std::chrono::high_resolution_clock::now();
    
    ASSERT_TRUE(result.has_value());
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    EXPECT_LT(duration.count(), 1000); // Should complete in under 1 second
    
    auto stats = lexer_->get_statistics();
    EXPECT_GT(stats.tokens_per_second, 10000.0); // At least 10K tokens/sec
}

/* Token Stream Tests */

TEST_F(LexerTest, TokenStreamNavigation) {
    auto result = tokenize_string("a b c");
    ASSERT_TRUE(result.has_value());
    
    auto stream = result.value();
    EXPECT_EQ(stream.position(), 0u);
    EXPECT_FALSE(stream.is_eof());
    
    expect_token(stream.current(), TokenType::Identifier, "a");
    expect_token(stream.peek(), TokenType::Identifier, "b");
    expect_token(stream.peek(2), TokenType::Identifier, "c");
    
    stream.advance();
    EXPECT_EQ(stream.position(), 1u);
    expect_token(stream.current(), TokenType::Identifier, "b");
    
    stream.seek(2);
    expect_token(stream.current(), TokenType::Identifier, "c");
    
    stream.reset();
    EXPECT_EQ(stream.position(), 0u);
    expect_token(stream.current(), TokenType::Identifier, "a");
}

TEST_F(LexerTest, TokenStreamConsume) {
    auto result = tokenize_string("hello world");
    ASSERT_TRUE(result.has_value());
    
    auto stream = result.value();
    
    auto token_result = stream.consume(TokenType::Identifier);
    ASSERT_TRUE(token_result.has_value());
    expect_token(token_result.value(), TokenType::Identifier, "hello");
    
    token_result = stream.consume(TokenType::Identifier);
    ASSERT_TRUE(token_result.has_value());
    expect_token(token_result.value(), TokenType::Identifier, "world");
    
    // Should fail for wrong type
    auto fail_result = stream.consume(TokenType::Identifier);
    ASSERT_FALSE(fail_result.has_value());
}

/* Concurrent Access Tests */

TEST_F(LexerTest, ConcurrentTokenization) {
    const int num_threads = 4;
    const int iterations = 100;
    std::atomic<int> success_count{0};
    std::atomic<int> error_count{0};
    
    auto worker = [this, &success_count, &error_count]() {
        for (int i = 0; i < iterations; ++i) {
            String content = "fn test_" + std::to_string(i) + "() { return " + std::to_string(i) + "; }";
            auto result = tokenize_string(content);
            if (result.has_value()) {
                success_count++;
            } else {
                error_count++;
            }
        }
    };
    
    Vec<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back(worker);
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    EXPECT_EQ(success_count.load(), num_threads * iterations);
    EXPECT_EQ(error_count.load(), 0);
}

/* Factory Tests */

TEST_F(LexerTest, FactoryCreateStandardLexer) {
    auto lexer = LexerFactory::create_standard_lexer(*source_manager_, *arena_);
    ASSERT_NE(lexer, nullptr);
    
    auto result = lexer->tokenize("hello world");
    ASSERT_TRUE(result.has_value());
    
    auto stream = result.value();
    expect_token(stream.current(), TokenType::Identifier, "hello");
}

TEST_F(LexerTest, FactoryCreateIdeLexer) {
    auto lexer = LexerFactory::create_ide_lexer(*source_manager_, *arena_);
    ASSERT_NE(lexer, nullptr);
    
    auto result = lexer->tokenize("hello world");
    ASSERT_TRUE(result.has_value());
}

TEST_F(LexerTest, FactoryCreateTestLexer) {
    auto lexer = LexerFactory::create_test_lexer(*source_manager_, *arena_);
    ASSERT_NE(lexer, nullptr);
    
    auto result = lexer->tokenize("hello world");
    ASSERT_TRUE(result.has_value());
}

} // anonymous namespace