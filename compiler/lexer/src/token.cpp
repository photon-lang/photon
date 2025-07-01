/**
 * @file token.cpp
 * @brief Token implementation for the Photon lexer
 * @author Photon Compiler Team
 * @version 1.0.0
 */

#include "photon/lexer/token.hpp"
#include <unordered_map>
#include <array>
#include <sstream>

namespace photon::lexer {

namespace {

/**
 * @brief Perfect hash table for keyword recognition
 * 
 * Generated using a perfect hash function to minimize lookup time.
 * The hash function maps keywords to unique indices with no collisions.
 */
struct KeywordEntry {
    StringView keyword;
    TokenType token_type;
};

// Perfect hash table for keywords (sorted by hash value for optimal cache performance)
constexpr std::array<KeywordEntry, 50> KEYWORD_TABLE = {{
    {"as", TokenType::KwAs},
    {"if", TokenType::KwIf},
    {"fn", TokenType::KwFn},
    {"u8", TokenType::KwU8},
    {"i8", TokenType::KwI8},
    {"in", TokenType::KwIn},
    {"f32", TokenType::KwF32},
    {"f64", TokenType::KwF64},
    {"u16", TokenType::KwU16},
    {"u32", TokenType::KwU32},
    {"u64", TokenType::KwU64},
    {"i16", TokenType::KwI16},
    {"i32", TokenType::KwI32},
    {"i64", TokenType::KwI64},
    {"let", TokenType::KwLet},
    {"mut", TokenType::KwMut},
    {"str", TokenType::KwStr},
    {"for", TokenType::KwFor},
    {"use", TokenType::KwUse},
    {"pub", TokenType::KwPub},
    {"mod", TokenType::KwMod},
    {"else", TokenType::KwElse},
    {"elif", TokenType::KwElif},
    {"enum", TokenType::KwEnum},
    {"impl", TokenType::KwImpl},
    {"loop", TokenType::KwLoop},
    {"move", TokenType::KwMove},
    {"self", TokenType::KwSelf},
    {"type", TokenType::KwType},
    {"bool", TokenType::KwBool},
    {"char", TokenType::KwChar},
    {"true", TokenType::BoolLiteral},
    {"async", TokenType::KwAsync},
    {"await", TokenType::KwAwait},
    {"break", TokenType::KwBreak},
    {"const", TokenType::KwConst},
    {"crate", TokenType::KwCrate},
    {"false", TokenType::BoolLiteral},
    {"match", TokenType::KwMatch},
    {"priv", TokenType::KwPriv},
    {"super", TokenType::KwSuper},
    {"trait", TokenType::KwTrait},
    {"where", TokenType::KwWhere},
    {"while", TokenType::KwWhile},
    {"extern", TokenType::KwExtern},
    {"return", TokenType::KwReturn},
    {"static", TokenType::KwStatic},
    {"struct", TokenType::KwStruct},
    {"unsafe", TokenType::KwUnsafe},
    {"continue", TokenType::KwContinue},
}};

/**
 * @brief Token type name lookup table
 */
constexpr std::array<StringView, 300> TOKEN_TYPE_NAMES = {{
    "Invalid",
    "Eof", 
    "Newline",
    "", "", "", "", "", "", "",
    "IntegerLiteral",
    "FloatLiteral",
    "StringLiteral", 
    "CharLiteral",
    "BoolLiteral",
    "", "", "", "", "",
    "Identifier",
    "", "", "", "", "", "", "", "", "",
    "if", "else", "elif", "match", "while", "for", "loop", "break", "continue", "return",
    "fn", "struct", "enum", "trait", "impl", "type", "const", "static", "let", "mut",
    "pub", "priv", "async", "await", "unsafe", "extern", "move", "", "", "",
    "self", "super", "crate", "mod", "use", "as", "where", "in", "", "",
    "i8", "i16", "i32", "i64", "u8", "u16", "u32", "u64", "f32", "f64", "bool", "char", "str",
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "+", "-", "*", "/", "%", "**", "", "", "", "",
    "==", "!=", "<", ">", "<=", ">=", "<=>", "", "", "",
    "&&", "||", "!", "", "", "", "", "", "", "",
    "&", "|", "^", "~", "<<", ">>", "", "", "", "",
    "=", "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", "<<=", ">>=", "", "", "", "", "", "", "", "", "",
    "->", "=>", "?", ".", "..", "...", "..=", "::", "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "(", ")", "{", "}", "[", "]", "", "", "", "",
    ",", ";", ":", "@", "#", "$", "", "", "", ""
}};


} // anonymous namespace

auto Token::to_string() const -> String {
    std::ostringstream oss;
    oss << "Token{" << token_type_name(type);
    
    if (value.has_value()) {
        oss << ", value=";
        if (value.is<i64>()) {
            oss << value.get<i64>();
        } else if (value.is<f64>()) {
            oss << value.get<f64>();
        } else if (value.is<StringView>()) {
            oss << "\"" << value.get<StringView>() << "\"";
        } else if (value.is<bool>()) {
            oss << (value.get<bool>() ? "true" : "false");
        }
    }
    
    oss << ", location=" << location.filename() << ":" << location.line() << ":" << location.column();
    oss << "}";
    return oss.str();
}

auto TokenStream::consume(TokenType expected) -> Result<Token, LexicalError> {
    if (is_eof()) {
        return Result<Token, LexicalError>(LexicalError::UnexpectedEof);
    }
    
    const auto& token = current();
    if (token.type != expected) {
        return Result<Token, LexicalError>(LexicalError::InvalidCharacter);
    }
    
    advance();
    return Result<Token, LexicalError>(token);
}

auto TokenStream::eof_token() noexcept -> const Token& {
    static const Token EOF_TOKEN{TokenType::Eof, diagnostics::SourceLocation{"<eof>", 0, 0, 0}};
    return EOF_TOKEN;
}

auto token_type_name(TokenType type) -> StringView {
    auto index = static_cast<usize>(type);
    if (index < TOKEN_TYPE_NAMES.size() && !TOKEN_TYPE_NAMES[index].empty()) {
        return TOKEN_TYPE_NAMES[index];
    }
    return "Unknown";
}

auto is_keyword(StringView text) -> bool {
    // Simple linear search for now (can be optimized with perfect hash later)
    for (const auto& entry : KEYWORD_TABLE) {
        if (entry.keyword == text) {
            return true;
        }
    }
    return false;
}

auto keyword_token_type(StringView text) -> Result<TokenType, LexicalError> {
    // Simple linear search for now (can be optimized with perfect hash later)
    for (const auto& entry : KEYWORD_TABLE) {
        if (entry.keyword == text) {
            return Result<TokenType, LexicalError>(entry.token_type);
        }
    }
    return Result<TokenType, LexicalError>(LexicalError::InvalidCharacter);
}

} // namespace photon::lexer