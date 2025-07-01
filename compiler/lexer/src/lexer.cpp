/**
 * @file lexer.cpp
 * @brief High-performance DFA-based lexer implementation
 * @author Photon Compiler Team
 * @version 1.0.0
 */

#include "photon/lexer/lexer.hpp"
#include <algorithm>
#include <cctype>
#include <charconv>
#include <chrono>

namespace photon::lexer {

namespace {

/**
 * @brief DFA state for lexical analysis
 */
enum class LexerState : u8 {
    Start,
    Identifier,
    Number,
    HexNumber,
    BinNumber,
    OctNumber,
    Float,
    FloatExp,
    String,
    StringEscape,
    Char,
    CharEscape,
    LineComment,
    BlockComment,
    BlockCommentEnd,
    Operator,
    Done,
    Error
};

/**
 * @brief Character classification for fast lookup
 */
enum class CharClass : u8 {
    Whitespace,
    Letter,
    Digit,
    HexDigit,
    Quote,
    SingleQuote,
    Slash,
    Star,
    Plus,
    Minus,
    Equal,
    Less,
    Greater,
    Ampersand,
    Pipe,
    Exclamation,
    Dot,
    Colon,
    Semicolon,
    Comma,
    LeftParen,
    RightParen,
    LeftBrace,
    RightBrace,
    LeftBracket,
    RightBracket,
    Hash,
    At,
    Dollar,
    Percent,
    Caret,
    Tilde,
    Question,
    Newline,
    Eof,
    Invalid
};

/**
 * @brief Character classification lookup table (256 entries for all ASCII + extended)
 */
constexpr std::array<CharClass, 256> CHAR_CLASS_TABLE = []() {
    std::array<CharClass, 256> table{};
    
    // Initialize all as invalid
    for (auto& entry : table) {
        entry = CharClass::Invalid;
    }
    
    // Whitespace
    table[' '] = table['\t'] = table['\r'] = table['\v'] = table['\f'] = CharClass::Whitespace;
    table['\n'] = CharClass::Newline;
    table[0] = CharClass::Eof;
    
    // Letters and underscore
    for (unsigned char c = 'a'; c <= 'z'; ++c) table[c] = CharClass::Letter;
    for (unsigned char c = 'A'; c <= 'Z'; ++c) table[c] = CharClass::Letter;
    table[static_cast<unsigned char>('_')] = CharClass::Letter;
    
    // Digits
    for (unsigned char c = '0'; c <= '9'; ++c) table[c] = CharClass::Digit;
    
    // Hex digits (already covered by letters and digits, but marked specifically)
    for (unsigned char c = 'a'; c <= 'f'; ++c) table[c] = CharClass::HexDigit;
    for (unsigned char c = 'A'; c <= 'F'; ++c) table[c] = CharClass::HexDigit;
    for (unsigned char c = '0'; c <= '9'; ++c) table[c] = CharClass::HexDigit;
    
    // Special characters
    table['"'] = CharClass::Quote;
    table['\''] = CharClass::SingleQuote;
    table['/'] = CharClass::Slash;
    table['*'] = CharClass::Star;
    table['+'] = CharClass::Plus;
    table['-'] = CharClass::Minus;
    table['='] = CharClass::Equal;
    table['<'] = CharClass::Less;
    table['>'] = CharClass::Greater;
    table['&'] = CharClass::Ampersand;
    table['|'] = CharClass::Pipe;
    table['!'] = CharClass::Exclamation;
    table['.'] = CharClass::Dot;
    table[':'] = CharClass::Colon;
    table[';'] = CharClass::Semicolon;
    table[','] = CharClass::Comma;
    table['('] = CharClass::LeftParen;
    table[')'] = CharClass::RightParen;
    table['{'] = CharClass::LeftBrace;
    table['}'] = CharClass::RightBrace;
    table['['] = CharClass::LeftBracket;
    table[']'] = CharClass::RightBracket;
    table['#'] = CharClass::Hash;
    table['@'] = CharClass::At;
    table['$'] = CharClass::Dollar;
    table['%'] = CharClass::Percent;
    table['^'] = CharClass::Caret;
    table['~'] = CharClass::Tilde;
    table['?'] = CharClass::Question;
    
    return table;
}();

/**
 * @brief Get character class for fast classification
 */
constexpr auto get_char_class(char c) -> CharClass {
    return CHAR_CLASS_TABLE[static_cast<u8>(c)];
}

/**
 * @brief Check if character is valid for identifier continuation
 */
constexpr auto is_identifier_char(char c) -> bool {
    return std::isalnum(c) || c == '_';
}

/**
 * @brief Check if character is valid hex digit
 */
constexpr auto is_hex_digit(char c) -> bool {
    return std::isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

/**
 * @brief Parse escape sequence
 */
auto parse_escape_sequence(StringView& input, usize& pos) -> Result<char, LexicalError> {
    if (pos >= input.size()) {
        return Result<char, LexicalError>(LexicalError::UnexpectedEof);
    }
    
    char c = input[pos++];
    switch (c) {
        case 'n': return Result<char, LexicalError>('\n');
        case 't': return Result<char, LexicalError>('\t');
        case 'r': return Result<char, LexicalError>('\r');
        case '0': return Result<char, LexicalError>('\0');
        case '\\': return Result<char, LexicalError>('\\');
        case '\'': return Result<char, LexicalError>('\'');
        case '"': return Result<char, LexicalError>('"');
        default:
            return Result<char, LexicalError>(LexicalError::InvalidEscape);
    }
}

/**
 * @brief Convert string to integer with specified base
 */
auto parse_integer(StringView text, int base) -> Result<i64, LexicalError> {
    i64 result = 0;
    auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), result, base);
    
    if (ec == std::errc::invalid_argument) {
        return Result<i64, LexicalError>(LexicalError::InvalidNumber);
    } else if (ec == std::errc::result_out_of_range) {
        return Result<i64, LexicalError>(LexicalError::NumberTooLarge);
    }
    
    return Result<i64, LexicalError>(result);
}

/**
 * @brief Convert string to floating point
 */
auto parse_float(StringView text) -> Result<f64, LexicalError> {
    try {
        String str{text};
        f64 result = std::stod(str);
        return Result<f64, LexicalError>(result);
    } catch (const std::invalid_argument&) {
        return Result<f64, LexicalError>(LexicalError::InvalidFloat);
    } catch (const std::out_of_range&) {
        return Result<f64, LexicalError>(LexicalError::NumberTooLarge);
    }
}

} // anonymous namespace

/**
 * @brief Lexer implementation using PIMPL pattern
 */
class Lexer::Impl {
private:
    source::SourceManager& source_manager_;
    memory::MemoryArena<>& arena_;
    LexerOptions options_;
    
    // Current tokenization state
    StringView input_;
    usize position_;
    usize line_;
    usize column_;
    String filename_;
    
    // Statistics
    Statistics stats_;
    std::chrono::high_resolution_clock::time_point start_time_;
    
public:
    explicit Impl(source::SourceManager& source_manager,
                  memory::MemoryArena<>& arena,
                  LexerOptions options)
        : source_manager_(source_manager)
        , arena_(arena)
        , options_(options)
        , position_(0)
        , line_(1)
        , column_(1) {}
    
    auto tokenize(source::FileID source_id) -> Result<TokenStream, LexicalError> {
        const auto* source_file = source_manager_.get_file(source_id);
        if (!source_file) {
            return Result<TokenStream, LexicalError>(LexicalError::InvalidCharacter);
        }
        
        input_ = source_file->content();
        filename_ = String(source_file->filename());
        return tokenize_internal();
    }
    
    auto tokenize(StringView content, StringView filename) -> Result<TokenStream, LexicalError> {
        input_ = content;
        filename_ = String(filename);
        return tokenize_internal();
    }
    
    auto get_statistics() const noexcept -> Statistics {
        return stats_;
    }
    
    auto reset_statistics() noexcept -> void {
        stats_ = Statistics{};
    }
    
private:
    auto tokenize_internal() -> Result<TokenStream, LexicalError> {
        start_time_ = std::chrono::high_resolution_clock::now();
        
        position_ = 0;
        line_ = 1;
        column_ = 1;
        
        Vec<Token> tokens;
        tokens.reserve(input_.size() / 8); // Rough estimate: 1 token per 8 characters
        
        while (position_ < input_.size()) {
            auto token_result = next_token();
            if (!token_result.has_value()) {
                return Result<TokenStream, LexicalError>(token_result.error());
            }
            
            auto token = token_result.value();
            if (token.type == TokenType::Eof) {
                tokens.push_back(std::move(token));
                break;
            }
            
            // Skip whitespace tokens unless preserving them
            if (!options_.preserve_whitespace || token.type != TokenType::Invalid) {
                tokens.push_back(std::move(token));
            }
        }
        
        // Add EOF token if not already present
        if (tokens.empty() || tokens.back().type != TokenType::Eof) {
            tokens.emplace_back(TokenType::Eof, create_location());
        }
        
        update_statistics(tokens.size());
        return Result<TokenStream, LexicalError>(TokenStream{std::move(tokens)});
    }
    
    auto next_token() -> Result<Token, LexicalError> {
        skip_whitespace();
        
        if (position_ >= input_.size()) {
            return Result<Token, LexicalError>(Token{TokenType::Eof, create_location()});
        }
        
        usize start_line = line_;
        usize start_column = column_;
        
        char c = peek();
        CharClass char_class = get_char_class(c);
        
        switch (char_class) {
            case CharClass::Letter:
                return tokenize_identifier_or_keyword();
                
            case CharClass::Digit:
                return tokenize_number();
                
            case CharClass::Quote:
                return tokenize_string();
                
            case CharClass::SingleQuote:
                return tokenize_char();
                
            case CharClass::Slash:
                return tokenize_slash_or_comment();
                
            case CharClass::Plus:
                return tokenize_plus();
                
            case CharClass::Minus:
                return tokenize_minus();
                
            case CharClass::Star:
                return tokenize_star();
                
            case CharClass::Equal:
                return tokenize_equal();
                
            case CharClass::Less:
                return tokenize_less();
                
            case CharClass::Greater:
                return tokenize_greater();
                
            case CharClass::Ampersand:
                return tokenize_ampersand();
                
            case CharClass::Pipe:
                return tokenize_pipe();
                
            case CharClass::Exclamation:
                return tokenize_exclamation();
                
            case CharClass::Dot:
                return tokenize_dot();
                
            case CharClass::Colon:
                return tokenize_colon();
                
            case CharClass::LeftParen:
                advance();
                return Result<Token, LexicalError>(Token{TokenType::LeftParen, create_location(start_line, start_column)});
                
            case CharClass::RightParen:
                advance();
                return Result<Token, LexicalError>(Token{TokenType::RightParen, create_location(start_line, start_column)});
                
            case CharClass::LeftBrace:
                advance();
                return Result<Token, LexicalError>(Token{TokenType::LeftBrace, create_location(start_line, start_column)});
                
            case CharClass::RightBrace:
                advance();
                return Result<Token, LexicalError>(Token{TokenType::RightBrace, create_location(start_line, start_column)});
                
            case CharClass::LeftBracket:
                advance();
                return Result<Token, LexicalError>(Token{TokenType::LeftBracket, create_location(start_line, start_column)});
                
            case CharClass::RightBracket:
                advance();
                return Result<Token, LexicalError>(Token{TokenType::RightBracket, create_location(start_line, start_column)});
                
            case CharClass::Comma:
                advance();
                return Result<Token, LexicalError>(Token{TokenType::Comma, create_location(start_line, start_column)});
                
            case CharClass::Semicolon:
                advance();
                return Result<Token, LexicalError>(Token{TokenType::Semicolon, create_location(start_line, start_column)});
                
            case CharClass::Hash:
                advance();
                return Result<Token, LexicalError>(Token{TokenType::Hash, create_location(start_line, start_column)});
                
            case CharClass::At:
                advance();
                return Result<Token, LexicalError>(Token{TokenType::At, create_location(start_line, start_column)});
                
            case CharClass::Dollar:
                advance();
                return Result<Token, LexicalError>(Token{TokenType::Dollar, create_location(start_line, start_column)});
                
            case CharClass::Percent:
                return tokenize_percent();
                
            case CharClass::Caret:
                return tokenize_caret();
                
            case CharClass::Tilde:
                advance();
                return Result<Token, LexicalError>(Token{TokenType::Tilde, create_location(start_line, start_column)});
                
            case CharClass::Question:
                advance();
                return Result<Token, LexicalError>(Token{TokenType::Question, create_location(start_line, start_column)});
                
            default:
                stats_.errors_recovered++;
                return Result<Token, LexicalError>(LexicalError::InvalidCharacter);
        }
    }
    
    auto tokenize_identifier_or_keyword() -> Result<Token, LexicalError> {
        usize start_pos = position_;
        usize start_line = line_;
        usize start_column = column_;
        
        // Consume identifier characters
        while (position_ < input_.size() && is_identifier_char(peek())) {
            advance();
        }
        
        StringView text = input_.substr(start_pos, position_ - start_pos);
        auto location = create_location(start_line, start_column);
        
        // Check if it's a keyword
        auto keyword_result = keyword_token_type(text);
        if (keyword_result.has_value()) {
            TokenType keyword_type = keyword_result.value();
            if (keyword_type == TokenType::BoolLiteral) {
                bool value = (text == "true");
                return Result<Token, LexicalError>(Token{keyword_type, TokenValue{value}, location});
            }
            return Result<Token, LexicalError>(Token{keyword_type, location});
        }
        
        // It's an identifier
        return Result<Token, LexicalError>(Token{TokenType::Identifier, TokenValue{text}, location});
    }
    
    auto tokenize_number() -> Result<Token, LexicalError> {
        usize start_pos = position_;
        usize start_line = line_;
        usize start_column = column_;
        
        // Check for special number prefixes
        if (peek() == '0' && position_ + 1 < input_.size()) {
            char second = input_[position_ + 1];
            if (second == 'x' || second == 'X') {
                return tokenize_hex_number();
            } else if (second == 'b' || second == 'B') {
                return tokenize_binary_number();
            } else if (second == 'o' || second == 'O') {
                return tokenize_octal_number();
            }
        }
        
        // Regular decimal number
        while (position_ < input_.size() && std::isdigit(peek())) {
            advance();
        }
        
        // Check for float
        if (position_ < input_.size() && peek() == '.' && 
            position_ + 1 < input_.size() && std::isdigit(input_[position_ + 1])) {
            advance(); // consume '.'
            while (position_ < input_.size() && std::isdigit(peek())) {
                advance();
            }
            
            // Check for scientific notation
            if (position_ < input_.size() && (peek() == 'e' || peek() == 'E')) {
                advance();
                if (position_ < input_.size() && (peek() == '+' || peek() == '-')) {
                    advance();
                }
                while (position_ < input_.size() && std::isdigit(peek())) {
                    advance();
                }
            }
            
            StringView text = input_.substr(start_pos, position_ - start_pos);
            auto value_result = parse_float(text);
            if (!value_result.has_value()) {
                return Result<Token, LexicalError>(value_result.error());
            }
            
            return Result<Token, LexicalError>(Token{
                TokenType::FloatLiteral, 
                TokenValue{value_result.value()}, 
                create_location(start_line, start_column)
            });
        }
        
        StringView text = input_.substr(start_pos, position_ - start_pos);
        auto value_result = parse_integer(text, 10);
        if (!value_result.has_value()) {
            return Result<Token, LexicalError>(value_result.error());
        }
        
        return Result<Token, LexicalError>(Token{
            TokenType::IntegerLiteral, 
            TokenValue{value_result.value()}, 
            create_location(start_line, start_column)
        });
    }
    
    auto tokenize_hex_number() -> Result<Token, LexicalError> {
        usize start_pos = position_;
        usize start_line = line_;
        usize start_column = column_;
        
        advance(); // consume '0'
        advance(); // consume 'x' or 'X'
        
        while (position_ < input_.size() && is_hex_digit(peek())) {
            advance();
        }
        
        StringView text = input_.substr(start_pos + 2, position_ - start_pos - 2); // Skip "0x"
        if (text.empty()) {
            return Result<Token, LexicalError>(LexicalError::InvalidNumber);
        }
        
        auto value_result = parse_integer(text, 16);
        if (!value_result.has_value()) {
            return Result<Token, LexicalError>(value_result.error());
        }
        
        return Result<Token, LexicalError>(Token{
            TokenType::IntegerLiteral, 
            TokenValue{value_result.value()}, 
            create_location(start_line, start_column)
        });
    }
    
    auto tokenize_binary_number() -> Result<Token, LexicalError> {
        usize start_pos = position_;
        usize start_line = line_;
        usize start_column = column_;
        
        advance(); // consume '0'
        advance(); // consume 'b' or 'B'
        
        while (position_ < input_.size() && (peek() == '0' || peek() == '1')) {
            advance();
        }
        
        StringView text = input_.substr(start_pos + 2, position_ - start_pos - 2); // Skip "0b"
        if (text.empty()) {
            return Result<Token, LexicalError>(LexicalError::InvalidNumber);
        }
        
        auto value_result = parse_integer(text, 2);
        if (!value_result.has_value()) {
            return Result<Token, LexicalError>(value_result.error());
        }
        
        return Result<Token, LexicalError>(Token{
            TokenType::IntegerLiteral, 
            TokenValue{value_result.value()}, 
            create_location(start_line, start_column)
        });
    }
    
    auto tokenize_octal_number() -> Result<Token, LexicalError> {
        usize start_pos = position_;
        usize start_line = line_;
        usize start_column = column_;
        
        advance(); // consume '0'
        advance(); // consume 'o' or 'O'
        
        while (position_ < input_.size() && peek() >= '0' && peek() <= '7') {
            advance();
        }
        
        StringView text = input_.substr(start_pos + 2, position_ - start_pos - 2); // Skip "0o"
        if (text.empty()) {
            return Result<Token, LexicalError>(LexicalError::InvalidNumber);
        }
        
        auto value_result = parse_integer(text, 8);
        if (!value_result.has_value()) {
            return Result<Token, LexicalError>(value_result.error());
        }
        
        return Result<Token, LexicalError>(Token{
            TokenType::IntegerLiteral, 
            TokenValue{value_result.value()}, 
            create_location(start_line, start_column)
        });
    }
    
    auto tokenize_string() -> Result<Token, LexicalError> {
        usize start_line = line_;
        usize start_column = column_;
        
        advance(); // consume opening quote
        
        String content;
        content.reserve(64); // Reserve space for typical strings
        
        while (position_ < input_.size() && peek() != '"') {
            if (peek() == '\\') {
                advance(); // consume backslash
                auto escape_result = parse_escape_sequence(input_, position_);
                if (!escape_result.has_value()) {
                    return Result<Token, LexicalError>(escape_result.error());
                }
                content += escape_result.value();
            } else {
                content += peek();
                advance();
            }
        }
        
        if (position_ >= input_.size()) {
            return Result<Token, LexicalError>(LexicalError::UnterminatedString);
        }
        
        advance(); // consume closing quote
        
        // Store string in arena for lifetime management
        char* arena_str = arena_.allocate<char>(content.size() + 1);
        std::memcpy(arena_str, content.data(), content.size());
        arena_str[content.size()] = '\0';
        
        return Result<Token, LexicalError>(Token{
            TokenType::StringLiteral, 
            TokenValue{StringView{arena_str, content.size()}}, 
            create_location(start_line, start_column)
        });
    }
    
    auto tokenize_char() -> Result<Token, LexicalError> {
        usize start_line = line_;
        usize start_column = column_;
        
        advance(); // consume opening quote
        
        if (position_ >= input_.size()) {
            return Result<Token, LexicalError>(LexicalError::UnterminatedChar);
        }
        
        char c;
        if (peek() == '\\') {
            advance(); // consume backslash
            auto escape_result = parse_escape_sequence(input_, position_);
            if (!escape_result.has_value()) {
                return Result<Token, LexicalError>(escape_result.error());
            }
            c = escape_result.value();
        } else {
            c = peek();
            advance();
        }
        
        if (position_ >= input_.size() || peek() != '\'') {
            return Result<Token, LexicalError>(LexicalError::UnterminatedChar);
        }
        
        advance(); // consume closing quote
        
        // Store single character in arena
        char* arena_char = arena_.allocate<char>(2);
        arena_char[0] = c;
        arena_char[1] = '\0';
        
        return Result<Token, LexicalError>(Token{
            TokenType::CharLiteral, 
            TokenValue{StringView{arena_char, 1}}, 
            create_location(start_line, start_column)
        });
    }
    
    auto tokenize_slash_or_comment() -> Result<Token, LexicalError> {
        usize start_line = line_;
        usize start_column = column_;
        
        advance(); // consume '/'
        
        if (position_ < input_.size()) {
            if (peek() == '/') {
                // Line comment
                advance(); // consume second '/'
                while (position_ < input_.size() && peek() != '\n') {
                    advance();
                }
                // Skip the comment and continue tokenizing
                return next_token();
            } else if (peek() == '*') {
                // Block comment
                advance(); // consume '*'
                bool found_end = false;
                while (position_ + 1 < input_.size()) {
                    if (peek() == '*' && input_[position_ + 1] == '/') {
                        advance(); // consume '*'
                        advance(); // consume '/'
                        found_end = true;
                        break;
                    }
                    advance();
                }
                if (!found_end) {
                    return Result<Token, LexicalError>(LexicalError::UnterminatedString);
                }
                // Skip the comment and continue tokenizing
                return next_token();
            } else if (peek() == '=') {
                advance(); // consume '='
                return Result<Token, LexicalError>(Token{TokenType::SlashAssign, create_location(start_line, start_column)});
            }
        }
        
        return Result<Token, LexicalError>(Token{TokenType::Slash, create_location(start_line, start_column)});
    }
    
    auto tokenize_plus() -> Result<Token, LexicalError> {
        usize start_line = line_;
        usize start_column = column_;
        
        advance(); // consume '+'
        
        if (position_ < input_.size() && peek() == '=') {
            advance(); // consume '='
            return Result<Token, LexicalError>(Token{TokenType::PlusAssign, create_location(start_line, start_column)});
        }
        
        return Result<Token, LexicalError>(Token{TokenType::Plus, create_location(start_line, start_column)});
    }
    
    auto tokenize_minus() -> Result<Token, LexicalError> {
        usize start_line = line_;
        usize start_column = column_;
        
        advance(); // consume '-'
        
        if (position_ < input_.size()) {
            if (peek() == '=') {
                advance(); // consume '='
                return Result<Token, LexicalError>(Token{TokenType::MinusAssign, create_location(start_line, start_column)});
            } else if (peek() == '>') {
                advance(); // consume '>'
                return Result<Token, LexicalError>(Token{TokenType::Arrow, create_location(start_line, start_column)});
            }
        }
        
        return Result<Token, LexicalError>(Token{TokenType::Minus, create_location(start_line, start_column)});
    }
    
    auto tokenize_star() -> Result<Token, LexicalError> {
        usize start_line = line_;
        usize start_column = column_;
        
        advance(); // consume '*'
        
        if (position_ < input_.size()) {
            if (peek() == '=') {
                advance(); // consume '='
                return Result<Token, LexicalError>(Token{TokenType::StarAssign, create_location(start_line, start_column)});
            } else if (peek() == '*') {
                advance(); // consume second '*'
                return Result<Token, LexicalError>(Token{TokenType::StarStar, create_location(start_line, start_column)});
            }
        }
        
        return Result<Token, LexicalError>(Token{TokenType::Star, create_location(start_line, start_column)});
    }
    
    auto tokenize_equal() -> Result<Token, LexicalError> {
        usize start_line = line_;
        usize start_column = column_;
        
        advance(); // consume '='
        
        if (position_ < input_.size()) {
            if (peek() == '=') {
                advance(); // consume second '='
                return Result<Token, LexicalError>(Token{TokenType::Equal, create_location(start_line, start_column)});
            } else if (peek() == '>') {
                advance(); // consume '>'
                return Result<Token, LexicalError>(Token{TokenType::FatArrow, create_location(start_line, start_column)});
            }
        }
        
        return Result<Token, LexicalError>(Token{TokenType::Assign, create_location(start_line, start_column)});
    }
    
    auto tokenize_less() -> Result<Token, LexicalError> {
        usize start_line = line_;
        usize start_column = column_;
        
        advance(); // consume '<'
        
        if (position_ < input_.size()) {
            if (peek() == '=') {
                advance(); // consume '='
                if (position_ < input_.size() && peek() == '>') {
                    advance(); // consume '>'
                    return Result<Token, LexicalError>(Token{TokenType::Spaceship, create_location(start_line, start_column)});
                }
                return Result<Token, LexicalError>(Token{TokenType::LessEqual, create_location(start_line, start_column)});
            } else if (peek() == '<') {
                advance(); // consume second '<'
                if (position_ < input_.size() && peek() == '=') {
                    advance(); // consume '='
                    return Result<Token, LexicalError>(Token{TokenType::LeftShiftAssign, create_location(start_line, start_column)});
                }
                return Result<Token, LexicalError>(Token{TokenType::LeftShift, create_location(start_line, start_column)});
            }
        }
        
        return Result<Token, LexicalError>(Token{TokenType::Less, create_location(start_line, start_column)});
    }
    
    auto tokenize_greater() -> Result<Token, LexicalError> {
        usize start_line = line_;
        usize start_column = column_;
        
        advance(); // consume '>'
        
        if (position_ < input_.size()) {
            if (peek() == '=') {
                advance(); // consume '='
                return Result<Token, LexicalError>(Token{TokenType::GreaterEqual, create_location(start_line, start_column)});
            } else if (peek() == '>') {
                advance(); // consume second '>'
                if (position_ < input_.size() && peek() == '=') {
                    advance(); // consume '='
                    return Result<Token, LexicalError>(Token{TokenType::RightShiftAssign, create_location(start_line, start_column)});
                }
                return Result<Token, LexicalError>(Token{TokenType::RightShift, create_location(start_line, start_column)});
            }
        }
        
        return Result<Token, LexicalError>(Token{TokenType::Greater, create_location(start_line, start_column)});
    }
    
    auto tokenize_ampersand() -> Result<Token, LexicalError> {
        usize start_line = line_;
        usize start_column = column_;
        
        advance(); // consume '&'
        
        if (position_ < input_.size()) {
            if (peek() == '&') {
                advance(); // consume second '&'
                return Result<Token, LexicalError>(Token{TokenType::And, create_location(start_line, start_column)});
            } else if (peek() == '=') {
                advance(); // consume '='
                return Result<Token, LexicalError>(Token{TokenType::AndAssign, create_location(start_line, start_column)});
            }
        }
        
        return Result<Token, LexicalError>(Token{TokenType::Ampersand, create_location(start_line, start_column)});
    }
    
    auto tokenize_pipe() -> Result<Token, LexicalError> {
        usize start_line = line_;
        usize start_column = column_;
        
        advance(); // consume '|'
        
        if (position_ < input_.size()) {
            if (peek() == '|') {
                advance(); // consume second '|'
                return Result<Token, LexicalError>(Token{TokenType::Or, create_location(start_line, start_column)});
            } else if (peek() == '=') {
                advance(); // consume '='
                return Result<Token, LexicalError>(Token{TokenType::OrAssign, create_location(start_line, start_column)});
            }
        }
        
        return Result<Token, LexicalError>(Token{TokenType::Pipe, create_location(start_line, start_column)});
    }
    
    auto tokenize_exclamation() -> Result<Token, LexicalError> {
        usize start_line = line_;
        usize start_column = column_;
        
        advance(); // consume '!'
        
        if (position_ < input_.size() && peek() == '=') {
            advance(); // consume '='
            return Result<Token, LexicalError>(Token{TokenType::NotEqual, create_location(start_line, start_column)});
        }
        
        return Result<Token, LexicalError>(Token{TokenType::Not, create_location(start_line, start_column)});
    }
    
    auto tokenize_dot() -> Result<Token, LexicalError> {
        usize start_line = line_;
        usize start_column = column_;
        
        advance(); // consume '.'
        
        if (position_ < input_.size() && peek() == '.') {
            advance(); // consume second '.'
            if (position_ < input_.size()) {
                if (peek() == '.') {
                    advance(); // consume third '.'
                    return Result<Token, LexicalError>(Token{TokenType::DotDotDot, create_location(start_line, start_column)});
                } else if (peek() == '=') {
                    advance(); // consume '='
                    return Result<Token, LexicalError>(Token{TokenType::DotDotEqual, create_location(start_line, start_column)});
                }
            }
            return Result<Token, LexicalError>(Token{TokenType::DotDot, create_location(start_line, start_column)});
        }
        
        return Result<Token, LexicalError>(Token{TokenType::Dot, create_location(start_line, start_column)});
    }
    
    auto tokenize_colon() -> Result<Token, LexicalError> {
        usize start_line = line_;
        usize start_column = column_;
        
        advance(); // consume ':'
        
        if (position_ < input_.size() && peek() == ':') {
            advance(); // consume second ':'
            return Result<Token, LexicalError>(Token{TokenType::ColonColon, create_location(start_line, start_column)});
        }
        
        return Result<Token, LexicalError>(Token{TokenType::Colon, create_location(start_line, start_column)});
    }
    
    auto tokenize_percent() -> Result<Token, LexicalError> {
        usize start_line = line_;
        usize start_column = column_;
        
        advance(); // consume '%'
        
        if (position_ < input_.size() && peek() == '=') {
            advance(); // consume '='
            return Result<Token, LexicalError>(Token{TokenType::PercentAssign, create_location(start_line, start_column)});
        }
        
        return Result<Token, LexicalError>(Token{TokenType::Percent, create_location(start_line, start_column)});
    }
    
    auto tokenize_caret() -> Result<Token, LexicalError> {
        usize start_line = line_;
        usize start_column = column_;
        
        advance(); // consume '^'
        
        if (position_ < input_.size() && peek() == '=') {
            advance(); // consume '='
            return Result<Token, LexicalError>(Token{TokenType::XorAssign, create_location(start_line, start_column)});
        }
        
        return Result<Token, LexicalError>(Token{TokenType::Caret, create_location(start_line, start_column)});
    }
    
    auto skip_whitespace() -> void {
        while (position_ < input_.size()) {
            char c = peek();
            if (c == ' ' || c == '\t' || c == '\r' || c == '\v' || c == '\f') {
                advance();
            } else if (c == '\n') {
                if (options_.preserve_whitespace) {
                    break; // Let newline be tokenized
                }
                advance();
            } else {
                break;
            }
        }
    }
    
    auto peek() const -> char {
        return position_ < input_.size() ? input_[position_] : '\0';
    }
    
    auto advance() -> void {
        if (position_ < input_.size()) {
            if (input_[position_] == '\n') {
                line_++;
                column_ = 1;
            } else {
                column_++;
            }
            position_++;
        }
    }
    
    auto create_location() const -> diagnostics::SourceLocation {
        return diagnostics::SourceLocation{filename_, static_cast<u32>(line_), static_cast<u32>(column_), static_cast<u32>(position_)};
    }
    
    auto create_location(usize line, usize column) const -> diagnostics::SourceLocation {
        return diagnostics::SourceLocation{filename_, static_cast<u32>(line), static_cast<u32>(column), static_cast<u32>(position_)};
    }
    
    auto update_statistics(usize token_count) -> void {
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time_);
        
        stats_.tokens_produced = token_count;
        stats_.bytes_processed = input_.size();
        stats_.lines_processed = line_;
        stats_.tokens_per_second = static_cast<f64>(token_count) / (static_cast<f64>(duration.count()) / 1000000.0);
        stats_.memory_used = arena_.total_allocated();
    }
};

/* Lexer Implementation */

Lexer::Lexer(source::SourceManager& source_manager,
             memory::MemoryArena<>& arena,
             LexerOptions options)
    : impl_(std::make_unique<Impl>(source_manager, arena, options)) {}

Lexer::~Lexer() = default;

Lexer::Lexer(Lexer&&) noexcept = default;
auto Lexer::operator=(Lexer&&) noexcept -> Lexer& = default;

auto Lexer::tokenize(source::FileID source_id) -> Result<TokenStream, LexicalError> {
    return impl_->tokenize(source_id);
}

auto Lexer::tokenize(StringView content, StringView filename) -> Result<TokenStream, LexicalError> {
    return impl_->tokenize(content, filename);
}

auto Lexer::get_statistics() const noexcept -> Statistics {
    return impl_->get_statistics();
}

auto Lexer::reset_statistics() noexcept -> void {
    impl_->reset_statistics();
}

auto Lexer::tokenize_with_recovery(
    source::FileID source_id,
    std::function<bool(LexicalError, diagnostics::SourceLocation)> /*error_handler*/)
    -> Result<TokenStream, LexicalError> {
    
    // For now, just delegate to regular tokenization
    // TODO: Implement error recovery mechanism
    return tokenize(source_id);
}

auto Lexer::tokenize_streaming(
    source::FileID source_id,
    std::function<bool(Token)> callback)
    -> Result<std::monostate, LexicalError> {
    
    auto result = tokenize(source_id);
    if (!result.has_value()) {
        return Result<std::monostate, LexicalError>(result.error());
    }
    
    auto stream = result.value();
    for (const auto& token : stream) {
        if (!callback(token)) {
            break;
        }
    }
    
    return Result<std::monostate, LexicalError>(std::monostate{});
}

/* Factory Implementation */

auto LexerFactory::create_standard_lexer(
    source::SourceManager& source_manager,
    memory::MemoryArena<>& arena) -> std::unique_ptr<ILexer> {
    
    LexerOptions options;
    options.preserve_whitespace = false;
    options.preserve_comments = false;
    options.strict_mode = true;
    options.optimize_identifiers = true;
    
    return std::make_unique<Lexer>(source_manager, arena, options);
}

auto LexerFactory::create_ide_lexer(
    source::SourceManager& source_manager,
    memory::MemoryArena<>& arena) -> std::unique_ptr<ILexer> {
    
    LexerOptions options;
    options.preserve_whitespace = true;
    options.preserve_comments = true;
    options.strict_mode = false;
    options.enable_streaming = true;
    options.optimize_identifiers = true;
    
    return std::make_unique<Lexer>(source_manager, arena, options);
}

auto LexerFactory::create_test_lexer(
    source::SourceManager& source_manager,
    memory::MemoryArena<>& arena) -> std::unique_ptr<ILexer> {
    
    LexerOptions options;
    options.preserve_whitespace = false;
    options.preserve_comments = false;
    options.strict_mode = false; // Allow recovery in tests
    options.optimize_identifiers = false; // Simpler for testing
    
    return std::make_unique<Lexer>(source_manager, arena, options);
}

} // namespace photon::lexer