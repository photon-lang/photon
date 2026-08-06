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
#include <cstring>
#include <mutex>

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
 * @brief Check if character is an ASCII decimal digit
 * @param c Character to classify
 * @return True for '0' through '9'
 * @complexity O(1)
 */
constexpr auto is_ascii_digit(char c) -> bool {
    return c >= '0' && c <= '9';
}

/**
 * @brief Check if character is valid for identifier continuation
 * @param c Character to classify
 * @return True for ASCII alphanumerics and underscore
 * @complexity O(1)
 */
constexpr auto is_identifier_char(char c) -> bool {
    return is_ascii_digit(c) ||
           (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') ||
           c == '_';
}

/**
 * @brief Check if character is valid hex digit
 * @param c Character to classify
 * @return True for '0'-'9', 'a'-'f' and 'A'-'F'
 * @complexity O(1)
 */
constexpr auto is_hex_digit(char c) -> bool {
    return is_ascii_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

/**
 * @brief Check if character is a valid digit for the given radix
 * @param c Character to classify
 * @param base Numeric radix (2, 8, 10 or 16)
 * @return True if the character is a digit in that radix
 * @complexity O(1)
 */
constexpr auto is_digit_for_base(char c, int base) -> bool {
    switch (base) {
        case 2: return c == '0' || c == '1';
        case 8: return c >= '0' && c <= '7';
        case 16: return is_hex_digit(c);
        default: return is_ascii_digit(c);
    }
}

/**
 * @brief Translate an escape sequence body character to the value it denotes
 * @param c Character following the backslash
 * @return Escaped character, or InvalidEscape if the sequence is not recognised
 * @complexity O(1)
 */
auto translate_escape(char c) -> Result<char, LexicalError> {
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
    
    /**
     * @brief Serializes tokenization so concurrent callers cannot corrupt the shared scan state
     */
    mutable std::mutex mutex_;

    StringView input_;
    usize position_;
    usize line_;
    usize column_;
    usize token_start_;
    StringView filename_;

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
        , column_(1)
        , token_start_(0) {}
    
    auto tokenize(source::FileID source_id) -> Result<TokenStream, LexicalError> {
        std::lock_guard<std::mutex> guard(mutex_);

        const auto* source_file = source_manager_.get_file(source_id);
        if (!source_file) {
            return Result<TokenStream, LexicalError>(LexicalError::InvalidSourceFile);
        }

        input_ = source_file->content();
        filename_ = intern_filename(source_file->filename());
        return tokenize_internal();
    }

    auto tokenize(StringView content, StringView filename) -> Result<TokenStream, LexicalError> {
        std::lock_guard<std::mutex> guard(mutex_);

        input_ = content;
        filename_ = intern_filename(filename);
        return tokenize_internal();
    }

    auto get_statistics() const noexcept -> Statistics {
        std::lock_guard<std::mutex> guard(mutex_);
        return stats_;
    }

    auto reset_statistics() noexcept -> void {
        std::lock_guard<std::mutex> guard(mutex_);
        stats_ = Statistics{};
    }
    
private:
    /**
     * @brief Copies a filename into the arena so tokens can hold a stable view of it
     * @param filename Filename supplied by the caller or source manager
     * @return View of the arena-owned copy, valid until the arena is reset
     *
     * @post Returned view is independent of the caller's buffer lifetime
     * @complexity O(n) in the length of the filename
     */
    auto intern_filename(StringView filename) -> StringView {
        if (filename.empty()) {
            return {};
        }

        char* buffer = arena_.allocate<char>(filename.size());
        std::memcpy(buffer, filename.data(), filename.size());
        return StringView{buffer, filename.size()};
    }

    auto tokenize_internal() -> Result<TokenStream, LexicalError> {
        start_time_ = std::chrono::high_resolution_clock::now();
        
        position_ = 0;
        line_ = 1;
        column_ = 1;
        token_start_ = 0;

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
            
            tokens.push_back(std::move(token));
        }
        
        // Add EOF token if not already present
        if (tokens.empty() || tokens.back().type != TokenType::Eof) {
            tokens.emplace_back(TokenType::Eof, create_location());
        }
        
        update_statistics(tokens.size());
        return Result<TokenStream, LexicalError>(TokenStream{std::move(tokens)});
    }
    
    auto next_token() -> Result<Token, LexicalError> {
        while (true) {
        skip_whitespace();

        if (position_ >= input_.size()) {
            return Result<Token, LexicalError>(Token{TokenType::Eof, create_location()});
        }

        token_start_ = position_;

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

            case CharClass::Newline:
                advance();
                return Result<Token, LexicalError>(Token{TokenType::Newline, create_location(start_line, start_column)});

            case CharClass::Slash: {
                if (position_ + 1 < input_.size() && input_[position_ + 1] == '/') {
                    skip_line_comment();
                    continue;
                }

                if (position_ + 1 < input_.size() && input_[position_ + 1] == '*') {
                    auto comment_result = skip_block_comment();
                    if (!comment_result.has_value()) {
                        return Result<Token, LexicalError>(comment_result.error());
                    }
                    continue;
                }

                return tokenize_slash();
            }

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
    
    /**
     * @brief Consumes a run of digits in the given radix, dropping '_' separators
     * @param base Numeric radix (2, 8, 10 or 16)
     * @param digits Receives the separator-free digit text
     * @return Number of significant digits consumed
     * @complexity O(n) in the length of the digit run
     */
    auto scan_digits(int base, String& digits) -> usize {
        usize count = 0;
        while (position_ < input_.size() && (is_digit_for_base(peek(), base) || peek() == '_')) {
            if (peek() != '_') {
                digits += peek();
                ++count;
            }
            advance();
        }
        return count;
    }

    /**
     * @brief Checks that a numeric literal is not immediately followed by identifier characters
     * @return True if the literal is correctly terminated
     * @complexity O(1)
     */
    [[nodiscard]] auto number_is_terminated() const -> bool {
        return position_ >= input_.size() || !is_identifier_char(peek());
    }

    auto tokenize_number() -> Result<Token, LexicalError> {
        usize start_line = line_;
        usize start_column = column_;

        if (peek() == '0' && position_ + 1 < input_.size()) {
            char second = input_[position_ + 1];
            if (second == 'x' || second == 'X') {
                return tokenize_prefixed_number(16);
            }
            if (second == 'b' || second == 'B') {
                return tokenize_prefixed_number(2);
            }
            if (second == 'o' || second == 'O') {
                return tokenize_prefixed_number(8);
            }
        }

        String digits;
        bool is_float = false;

        scan_digits(10, digits);

        if (position_ + 1 < input_.size() && peek() == '.' && is_ascii_digit(input_[position_ + 1])) {
            is_float = true;
            digits += '.';
            advance();
            scan_digits(10, digits);
        }

        if (position_ < input_.size() && (peek() == 'e' || peek() == 'E')) {
            usize probe = position_ + 1;
            if (probe < input_.size() && (input_[probe] == '+' || input_[probe] == '-')) {
                ++probe;
            }

            if (probe < input_.size() && is_ascii_digit(input_[probe])) {
                is_float = true;
                digits += peek();
                advance();
                if (peek() == '+' || peek() == '-') {
                    digits += peek();
                    advance();
                }
                scan_digits(10, digits);
            }
        }

        if (!number_is_terminated()) {
            return Result<Token, LexicalError>(LexicalError::InvalidNumber);
        }

        auto location = create_location(start_line, start_column);

        if (is_float) {
            auto value_result = parse_float(digits);
            if (!value_result.has_value()) {
                return Result<Token, LexicalError>(value_result.error());
            }
            return Result<Token, LexicalError>(Token{
                TokenType::FloatLiteral,
                TokenValue{value_result.value()},
                location
            });
        }

        auto value_result = parse_integer(digits, 10);
        if (!value_result.has_value()) {
            return Result<Token, LexicalError>(value_result.error());
        }

        return Result<Token, LexicalError>(Token{
            TokenType::IntegerLiteral,
            TokenValue{value_result.value()},
            location
        });
    }

    /**
     * @brief Scans an integer literal introduced by a radix prefix (0x, 0b or 0o)
     * @param base Numeric radix implied by the prefix
     * @return Integer token, or InvalidNumber if no digits follow the prefix
     * @complexity O(n) in the length of the literal
     */
    auto tokenize_prefixed_number(int base) -> Result<Token, LexicalError> {
        usize start_line = line_;
        usize start_column = column_;

        advance();
        advance();

        String digits;
        if (scan_digits(base, digits) == 0) {
            return Result<Token, LexicalError>(LexicalError::InvalidNumber);
        }

        if (!number_is_terminated()) {
            return Result<Token, LexicalError>(LexicalError::InvalidNumber);
        }

        auto value_result = parse_integer(digits, base);
        if (!value_result.has_value()) {
            return Result<Token, LexicalError>(value_result.error());
        }

        return Result<Token, LexicalError>(Token{
            TokenType::IntegerLiteral,
            TokenValue{value_result.value()},
            create_location(start_line, start_column)
        });
    }


    /**
     * @brief Consumes the body of an escape sequence, keeping line and column tracking correct
     * @return Escaped character, or the reason the sequence is invalid
     * @pre The introducing backslash has already been consumed
     * @complexity O(1)
     */
    auto read_escape_sequence() -> Result<char, LexicalError> {
        if (position_ >= input_.size()) {
            return Result<char, LexicalError>(LexicalError::UnexpectedEof);
        }

        char c = peek();
        advance();
        return translate_escape(c);
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
                auto escape_result = read_escape_sequence();
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
            auto escape_result = read_escape_sequence();
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
    
    /**
     * @brief Consumes a line comment up to but not including the terminating newline
     * @complexity O(n) in the length of the comment
     */
    auto skip_line_comment() -> void {
        advance();
        advance();
        while (position_ < input_.size() && peek() != '\n') {
            advance();
        }
    }

    /**
     * @brief Consumes a block comment including its closing delimiter
     * @return Success, or UnexpectedEof if the comment is never closed
     * @complexity O(n) in the length of the comment
     */
    auto skip_block_comment() -> Result<std::monostate, LexicalError> {
        advance();
        advance();

        while (position_ + 1 < input_.size()) {
            if (peek() == '*' && input_[position_ + 1] == '/') {
                advance();
                advance();
                return Result<std::monostate, LexicalError>(std::monostate{});
            }
            advance();
        }

        position_ = input_.size();
        return Result<std::monostate, LexicalError>(LexicalError::UnexpectedEof);
    }

    auto tokenize_slash() -> Result<Token, LexicalError> {
        usize start_line = line_;
        usize start_column = column_;

        advance(); // consume '/'

        if (position_ < input_.size() && peek() == '=') {
            advance(); // consume '='
            return Result<Token, LexicalError>(Token{TokenType::SlashAssign, create_location(start_line, start_column)});
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
    
    /**
     * @brief Builds a location for a token that started at the current token boundary
     * @param line Line on which the token starts (1-based)
     * @param column Column on which the token starts (1-based)
     * @return Location whose byte offset is the token's first byte
     * @complexity O(1)
     */
    auto create_location(usize line, usize column) const -> diagnostics::SourceLocation {
        return diagnostics::SourceLocation{filename_, static_cast<u32>(line), static_cast<u32>(column), static_cast<u32>(token_start_)};
    }
    
    auto update_statistics(usize token_count) -> void {
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time_);
        auto elapsed_ns = duration.count();

        stats_.tokens_produced = token_count;
        stats_.bytes_processed = input_.size();
        stats_.lines_processed = line_;
        stats_.tokens_per_second = elapsed_ns > 0
            ? static_cast<f64>(token_count) * 1000000000.0 / static_cast<f64>(elapsed_ns)
            : 0.0;
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