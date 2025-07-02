/**
 * @file parser.hpp
 * @brief Recursive descent parser with Pratt parsing for expressions
 * @author Photon Compiler Team
 * @version 1.0.0
 */

#pragma once

#include "photon/common/types.hpp"
#include "photon/lexer/token.hpp"
#include "photon/memory/arena.hpp"
#include "photon/parser/ast.hpp"
#include <memory>

namespace photon::parser {

/**
 * @brief Parser error types
 */
enum class ParseError : u32 {
    UnexpectedToken = 3000,
    UnexpectedEof = 3001,
    ExpectedExpression = 3002,
    ExpectedStatement = 3003,
    ExpectedDeclaration = 3004,
    ExpectedIdentifier = 3005,
    ExpectedType = 3006,
    ExpectedOperator = 3007,
    InvalidSyntax = 3008,
    MissingDelimiter = 3009,
    InvalidLiteral = 3010,
    NestedTooDeep = 3011,
    InvalidAssignment = 3012,
    DuplicateParameter = 3013,
    InvalidReturnType = 3014,
    MissingFunctionBody = 3015,
};

/**
 * @brief Error recovery strategies
 */
enum class RecoveryStrategy : u8 {
    Skip,           ///< Skip the current token
    Synchronize,    ///< Skip until synchronization point
    Insert,         ///< Insert expected token
    Abort,          ///< Stop parsing (fatal error)
};

/**
 * @brief Parser configuration options
 */
struct ParserOptions {
    usize max_recursion_depth = 1000;
    bool enable_error_recovery = true;
    bool track_comments = false;
    bool strict_mode = false;
};

/**
 * @brief High-performance recursive descent parser
 * 
 * Features:
 * - Pratt parsing for expressions with proper precedence
 * - Error recovery and synchronization
 * - Memory-efficient AST construction
 * - Left-recursive grammar handling
 * - Comprehensive error reporting
 */
class Parser {
private:
    lexer::TokenStream tokens_;
    ASTFactory factory_;
    ParserOptions options_;
    usize recursion_depth_;
    Vec<ParseError> errors_;
    
public:
    explicit Parser(lexer::TokenStream tokens, memory::MemoryArena<>& arena, 
                   ParserOptions options = {});
    
    // Non-copyable and non-movable due to reference member in ASTFactory
    Parser(const Parser&) = delete;
    auto operator=(const Parser&) -> Parser& = delete;
    Parser(Parser&&) = delete;
    auto operator=(Parser&&) -> Parser& = delete;
    
    /**
     * @brief Parse complete program (top-level entry point)
     */
    [[nodiscard]] auto parse_program() -> Result<ASTPtr<Program>, ParseError>;
    
    /**
     * @brief Parse single expression (for REPL/testing)
     */
    [[nodiscard]] auto parse_expression() -> Result<ASTPtr<Expression>, ParseError>;
    
    /**
     * @brief Parse single statement (for REPL/testing)
     */
    [[nodiscard]] auto parse_statement() -> Result<ASTPtr<Statement>, ParseError>;
    
    /**
     * @brief Get all parsing errors encountered
     */
    [[nodiscard]] auto errors() const noexcept -> const Vec<ParseError>& { return errors_; }
    
    /**
     * @brief Check if parser encountered any errors
     */
    [[nodiscard]] auto has_errors() const noexcept -> bool { return !errors_.empty(); }
    
    /**
     * @brief Clear all errors (for reuse)
     */
    auto clear_errors() noexcept -> void { errors_.clear(); }

private:
    // === Core parsing methods ===
    
    /**
     * @brief Parse top-level declarations
     */
    [[nodiscard]] auto parse_declarations() -> Result<ASTList<Declaration>, ParseError>;
    [[nodiscard]] auto parse_declaration() -> Result<ASTPtr<Declaration>, ParseError>;
    
    /**
     * @brief Parse function declaration
     */
    [[nodiscard]] auto parse_function_decl() -> Result<ASTPtr<FunctionDecl>, ParseError>;
    [[nodiscard]] auto parse_function_parameters() -> Result<Vec<FunctionDecl::Parameter>, ParseError>;
    [[nodiscard]] auto parse_parameter() -> Result<FunctionDecl::Parameter, ParseError>;
    
    /**
     * @brief Parse statements
     */
    [[nodiscard]] auto parse_statement_list() -> Result<ASTList<Statement>, ParseError>;
    [[nodiscard]] auto parse_block() -> Result<ASTPtr<Block>, ParseError>;
    [[nodiscard]] auto parse_var_decl() -> Result<ASTPtr<VarDecl>, ParseError>;
    
    // === Expression parsing with Pratt parser ===
    
    /**
     * @brief Pratt parser for expressions with precedence handling
     */
    [[nodiscard]] auto parse_expr(i32 min_precedence = 0) -> Result<ASTPtr<Expression>, ParseError>;
    [[nodiscard]] auto parse_primary() -> Result<ASTPtr<Expression>, ParseError>;
    [[nodiscard]] auto parse_postfix(ASTPtr<Expression> left) -> Result<ASTPtr<Expression>, ParseError>;
    
    /**
     * @brief Parse specific expression types
     */
    [[nodiscard]] auto parse_literal() -> Result<ASTPtr<Expression>, ParseError>;
    [[nodiscard]] auto parse_identifier() -> Result<ASTPtr<Identifier>, ParseError>;
    [[nodiscard]] auto parse_call_expr(ASTPtr<Expression> callee) -> Result<ASTPtr<CallExpr>, ParseError>;
    [[nodiscard]] auto parse_call_arguments() -> Result<ASTList<Expression>, ParseError>;
    
    /**
     * @brief Parse type expressions
     */
    [[nodiscard]] auto parse_type() -> Result<ASTPtr<Expression>, ParseError>;
    
    // === Operator precedence and associativity ===
    
    /**
     * @brief Get operator precedence (higher = tighter binding)
     */
    [[nodiscard]] auto get_precedence(lexer::TokenType type) const noexcept -> i32;
    
    /**
     * @brief Check if operator is right-associative
     */
    [[nodiscard]] auto is_right_associative(lexer::TokenType type) const noexcept -> bool;
    
    /**
     * @brief Convert token to binary operator
     */
    [[nodiscard]] auto token_to_binary_op(lexer::TokenType type) const noexcept 
        -> Result<BinaryExpr::Operator, ParseError>;
    
    /**
     * @brief Convert token to unary operator
     */
    [[nodiscard]] auto token_to_unary_op(lexer::TokenType type) const noexcept 
        -> Result<UnaryExpr::Operator, ParseError>;
    
    // === Utility methods ===
    
    /**
     * @brief Current token access
     */
    [[nodiscard]] auto current() const noexcept -> const lexer::Token& { return tokens_.current(); }
    [[nodiscard]] auto peek(usize offset = 1) const noexcept -> const lexer::Token& { return tokens_.peek(offset); }
    [[nodiscard]] auto is_eof() const noexcept -> bool { return tokens_.is_eof(); }
    
    /**
     * @brief Token consumption
     */
    auto advance() noexcept -> void { tokens_.advance(); }
    [[nodiscard]] auto consume(lexer::TokenType expected) -> Result<lexer::Token, ParseError>;
    [[nodiscard]] auto match_token(lexer::TokenType type) const noexcept -> bool;
    [[nodiscard]] auto match_any(std::initializer_list<lexer::TokenType> types) const noexcept -> bool;
    
    /**
     * @brief Error handling and recovery
     */
    auto report_error(ParseError error) -> void;
    auto recover(RecoveryStrategy strategy) -> void;
    auto synchronize() -> void;
    [[nodiscard]] auto is_synchronization_point() const noexcept -> bool;
    
    /**
     * @brief Recursion depth management
     */
    auto enter_recursion() -> Result<void, ParseError>;
    auto exit_recursion() noexcept -> void;
    
    /**
     * @brief Source location tracking
     */
    [[nodiscard]] auto current_location() const noexcept -> diagnostics::SourceLocation;
    [[nodiscard]] auto make_range(const diagnostics::SourceLocation& start) const noexcept -> SourceRange;
};

/**
 * @brief Operator precedence table for Pratt parsing
 */
enum class Precedence : i32 {
    None = 0,
    Assignment = 10,    // = += -= *= /= %= &= |= ^= <<= >>=
    Range = 20,         // .. ..= 
    LogicalOr = 30,     // ||
    LogicalAnd = 40,    // &&
    Equality = 50,      // == != <=>
    Comparison = 60,    // < > <= >=
    BitwiseOr = 70,     // |
    BitwiseXor = 80,    // ^
    BitwiseAnd = 90,    // &
    Shift = 100,        // << >>
    Addition = 110,     // + -
    Multiplication = 120, // * / %
    Power = 130,        // **
    Unary = 140,        // ! - + ~ & *
    Postfix = 150,      // . () [] 
    Primary = 160,      // literals, identifiers
};

/**
 * @brief Convert precedence enum to integer
 */
constexpr auto to_int(Precedence prec) noexcept -> i32 {
    return static_cast<i32>(prec);
}

} // namespace photon::parser