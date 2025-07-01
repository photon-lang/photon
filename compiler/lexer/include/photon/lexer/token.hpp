/**
 * @file token.hpp
 * @brief Token types and representation for the Photon lexer
 * @author Photon Compiler Team
 * @version 1.0.0
 */

#pragma once

#include "photon/common/types.hpp"
#include "photon/diagnostics/source_location.hpp"
#include <string_view>
#include <variant>

namespace photon::lexer {

/**
 * @brief Token types for the Photon programming language
 * 
 * Organized by category for optimal performance in switch statements
 * and efficient perfect hash table construction.
 */
enum class TokenType : u16 {
    /* Special tokens */
    Invalid = 0,        ///< Invalid/error token
    Eof = 1,           ///< End of file
    Newline = 2,       ///< Significant newlines
    
    /* Literals */
    IntegerLiteral = 10,    ///< 42, 0xFF, 0b1010, 0o755
    FloatLiteral = 11,      ///< 3.14, 1.0e10, 0x1.Ap+4
    StringLiteral = 12,     ///< "hello", r"raw string"
    CharLiteral = 13,       ///< 'a', '\n', '\u{1F602}'
    BoolLiteral = 14,       ///< true, false
    
    /* Identifiers and keywords */
    Identifier = 20,        ///< variable_name, TypeName
    
    /* Keywords - Control Flow */
    KwIf = 30,             ///< if
    KwElse = 31,           ///< else
    KwElif = 32,           ///< elif
    KwMatch = 33,          ///< match
    KwWhile = 34,          ///< while
    KwFor = 35,            ///< for
    KwLoop = 36,           ///< loop
    KwBreak = 37,          ///< break
    KwContinue = 38,       ///< continue
    KwReturn = 39,         ///< return
    
    /* Keywords - Declaration */
    KwFn = 40,             ///< fn
    KwStruct = 41,         ///< struct
    KwEnum = 42,           ///< enum
    KwTrait = 43,          ///< trait
    KwImpl = 44,           ///< impl
    KwType = 45,           ///< type
    KwConst = 46,          ///< const
    KwStatic = 47,         ///< static
    KwLet = 48,            ///< let
    KwMut = 49,            ///< mut
    
    /* Keywords - Visibility and modifiers */
    KwPub = 50,            ///< pub
    KwPriv = 51,           ///< priv
    KwAsync = 52,          ///< async
    KwAwait = 53,          ///< await
    KwUnsafe = 54,         ///< unsafe
    KwExtern = 55,         ///< extern
    KwMove = 56,           ///< move
    
    /* Keywords - Types and values */
    KwSelf = 60,           ///< self
    KwSuper = 61,          ///< super
    KwCrate = 62,          ///< crate
    KwMod = 63,            ///< mod
    KwUse = 64,            ///< use
    KwAs = 65,             ///< as
    KwWhere = 66,          ///< where
    KwIn = 67,             ///< in
    
    /* Built-in types */
    KwI8 = 70,             ///< i8
    KwI16 = 71,            ///< i16
    KwI32 = 72,            ///< i32
    KwI64 = 73,            ///< i64
    KwU8 = 74,             ///< u8
    KwU16 = 75,            ///< u16
    KwU32 = 76,            ///< u32
    KwU64 = 77,            ///< u64
    KwF32 = 78,            ///< f32
    KwF64 = 79,            ///< f64
    KwBool = 80,           ///< bool
    KwChar = 81,           ///< char
    KwStr = 82,            ///< str
    
    /* Operators - Arithmetic */
    Plus = 100,            ///< +
    Minus = 101,           ///< -
    Star = 102,            ///< *
    Slash = 103,           ///< /
    Percent = 104,         ///< %
    StarStar = 105,        ///< **
    
    /* Operators - Comparison */
    Equal = 110,           ///< ==
    NotEqual = 111,        ///< !=
    Less = 112,            ///< <
    Greater = 113,         ///< >
    LessEqual = 114,       ///< <=
    GreaterEqual = 115,    ///< >=
    Spaceship = 116,       ///< <=>
    
    /* Operators - Logical */
    And = 120,             ///< &&
    Or = 121,              ///< ||
    Not = 122,             ///< !
    
    /* Operators - Bitwise */
    Ampersand = 130,       ///< &
    Pipe = 131,            ///< |
    Caret = 132,           ///< ^
    Tilde = 133,           ///< ~
    LeftShift = 134,       ///< <<
    RightShift = 135,      ///< >>
    
    /* Operators - Assignment */
    Assign = 140,          ///< =
    PlusAssign = 141,      ///< +=
    MinusAssign = 142,     ///< -=
    StarAssign = 143,      ///< *=
    SlashAssign = 144,     ///< /=
    PercentAssign = 145,   ///< %=
    AndAssign = 146,       ///< &=
    OrAssign = 147,        ///< |=
    XorAssign = 148,       ///< ^=
    LeftShiftAssign = 149, ///< <<=
    RightShiftAssign = 150,///< >>=
    
    /* Operators - Other */
    Arrow = 160,           ///< ->
    FatArrow = 161,        ///< =>
    Question = 162,        ///< ?
    Dot = 163,             ///< .
    DotDot = 164,          ///< ..
    DotDotDot = 165,       ///< ...
    DotDotEqual = 166,     ///< ..=
    ColonColon = 167,      ///< ::
    
    /* Delimiters */
    LeftParen = 200,       ///< (
    RightParen = 201,      ///< )
    LeftBrace = 202,       ///< {
    RightBrace = 203,      ///< }
    LeftBracket = 204,     ///< [
    RightBracket = 205,    ///< ]
    
    /* Punctuation */
    Comma = 210,           ///< ,
    Semicolon = 211,       ///< ;
    Colon = 212,           ///< :
    At = 213,              ///< @
    Hash = 214,            ///< #
    Dollar = 215,          ///< $
};

/**
 * @brief Token value variants for different literal types
 */
struct TokenValue {
    std::variant<
        std::monostate,    ///< No value (for keywords, operators, etc.)
        i64,               ///< Integer literals
        f64,               ///< Float literals  
        StringView,        ///< String/char literals and identifiers
        bool               ///< Boolean literals
    > data;
    
    TokenValue() = default;
    TokenValue(i64 value) : data(value) {}
    TokenValue(f64 value) : data(value) {}
    TokenValue(StringView value) : data(value) {}
    TokenValue(bool value) : data(value) {}
    
    [[nodiscard]] auto has_value() const noexcept -> bool {
        return !std::holds_alternative<std::monostate>(data);
    }
    
    template<typename T>
    [[nodiscard]] auto get() const -> const T& {
        return std::get<T>(data);
    }
    
    template<typename T>
    [[nodiscard]] auto is() const noexcept -> bool {
        return std::holds_alternative<T>(data);
    }
};

/**
 * @brief Token representation with type, value, and source location
 */
struct Token {
    TokenType type;                           ///< Token classification
    TokenValue value;                         ///< Token value (if applicable)
    diagnostics::SourceLocation location;    ///< Source position information
    
    Token() = default;
    Token(TokenType type, diagnostics::SourceLocation location)
        : type(type), location(std::move(location)) {}
    
    Token(TokenType type, TokenValue value, diagnostics::SourceLocation location)
        : type(type), value(std::move(value)), location(std::move(location)) {}
    
    [[nodiscard]] auto is_keyword() const noexcept -> bool {
        return static_cast<u16>(type) >= 30 && static_cast<u16>(type) < 100;
    }
    
    [[nodiscard]] auto is_operator() const noexcept -> bool {
        return static_cast<u16>(type) >= 100 && static_cast<u16>(type) < 200;
    }
    
    [[nodiscard]] auto is_literal() const noexcept -> bool {
        return static_cast<u16>(type) >= 10 && static_cast<u16>(type) < 20;
    }
    
    [[nodiscard]] auto is_delimiter() const noexcept -> bool {
        return static_cast<u16>(type) >= 200 && static_cast<u16>(type) < 220;
    }
    
    [[nodiscard]] auto text() const -> StringView {
        if (value.is<StringView>()) {
            return value.get<StringView>();
        }
        return {};
    }
    
    [[nodiscard]] auto to_string() const -> String;
};

/**
 * @brief Lexical error types
 */
enum class LexicalError : u32 {
    InvalidCharacter = 2000,      ///< Unexpected character
    UnterminatedString = 2001,    ///< String literal not closed
    UnterminatedChar = 2002,      ///< Character literal not closed
    InvalidEscape = 2003,         ///< Invalid escape sequence
    InvalidNumber = 2004,         ///< Malformed numeric literal
    InvalidUnicode = 2005,        ///< Invalid Unicode escape
    NumberTooLarge = 2006,        ///< Numeric literal out of range
    UnexpectedEof = 2007,         ///< Unexpected end of file
    InvalidFloat = 2008,          ///< Malformed floating-point literal
    InvalidRadix = 2009,          ///< Invalid numeric base
};

/**
 * @brief Stream of tokens with efficient access patterns
 */
class TokenStream {
private:
    Vec<Token> tokens_;
    usize position_;
    
public:
    explicit TokenStream(Vec<Token> tokens) 
        : tokens_(std::move(tokens)), position_(0) {}
    
    [[nodiscard]] auto current() const noexcept -> const Token& {
        return position_ < tokens_.size() ? tokens_[position_] : eof_token();
    }
    
    [[nodiscard]] auto peek(usize offset = 1) const noexcept -> const Token& {
        usize pos = position_ + offset;
        return pos < tokens_.size() ? tokens_[pos] : eof_token();
    }
    
    auto advance() noexcept -> void {
        if (position_ < tokens_.size()) {
            position_++;
        }
    }
    
    [[nodiscard]] auto consume(TokenType expected) -> Result<Token, LexicalError>;
    
    [[nodiscard]] auto position() const noexcept -> usize { return position_; }
    [[nodiscard]] auto size() const noexcept -> usize { return tokens_.size(); }
    [[nodiscard]] auto is_eof() const noexcept -> bool { 
        return position_ >= tokens_.size() || current().type == TokenType::Eof; 
    }
    
    auto reset() noexcept -> void { position_ = 0; }
    auto seek(usize pos) noexcept -> void { position_ = std::min(pos, tokens_.size()); }
    
    [[nodiscard]] auto begin() const noexcept { return tokens_.begin(); }
    [[nodiscard]] auto end() const noexcept { return tokens_.end(); }
    
private:
    [[nodiscard]] static auto eof_token() noexcept -> const Token&;
};

/**
 * @brief Get string representation of token type
 */
[[nodiscard]] auto token_type_name(TokenType type) -> StringView;

/**
 * @brief Check if string is a keyword
 */
[[nodiscard]] auto is_keyword(StringView text) -> bool;

/**
 * @brief Get keyword token type from string (if it's a keyword)
 */
[[nodiscard]] auto keyword_token_type(StringView text) -> Result<TokenType, LexicalError>;

} // namespace photon::lexer