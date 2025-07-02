/**
 * @file ast.hpp
 * @brief Abstract Syntax Tree node hierarchy for the Photon programming language
 * @author Photon Compiler Team
 * @version 1.0.0
 */

#pragma once

#include "photon/common/types.hpp"
#include "photon/lexer/token.hpp"
#include "photon/memory/arena.hpp"
#include <memory>
#include <vector>

namespace photon::parser {

// Forward declarations
class ASTVisitor;
class ASTNode;
class Expression;
class Statement;
class Declaration;

/**
 * @brief Unique pointer to AST nodes allocated in memory arena
 */
template<typename T>
using ASTPtr = std::unique_ptr<T>;

/**
 * @brief Vector of AST node pointers
 */
template<typename T>
using ASTList = Vec<ASTPtr<T>>;

/**
 * @brief Source location information for AST nodes
 */
using SourceRange = std::pair<diagnostics::SourceLocation, diagnostics::SourceLocation>;

/**
 * @brief Base class for all AST nodes
 * 
 * Provides common functionality for source location tracking,
 * visitor pattern support, and type-safe downcasting.
 */
class ASTNode {
public:
    /**
     * @brief AST node types for runtime type identification
     */
    enum class Kind : u16 {
        // Expressions
        IntegerLiteral = 100,
        FloatLiteral = 101,
        StringLiteral = 102,
        CharLiteral = 103,
        BoolLiteral = 104,
        Identifier = 110,
        BinaryExpr = 120,
        UnaryExpr = 121,
        CallExpr = 122,
        IndexExpr = 123,
        MemberExpr = 124,
        CastExpr = 125,
        
        // Statements
        ExprStmt = 200,
        VarDecl = 201,
        ConstDecl = 202,
        Block = 210,
        IfStmt = 211,
        WhileStmt = 212,
        ForStmt = 213,
        LoopStmt = 214,
        BreakStmt = 215,
        ContinueStmt = 216,
        ReturnStmt = 217,
        MatchStmt = 218,
        
        // Declarations
        FunctionDecl = 300,
        StructDecl = 301,
        EnumDecl = 302,
        TraitDecl = 303,
        ImplDecl = 304,
        TypeAliasDecl = 305,
        ModuleDecl = 306,
        UseDecl = 307,
        
        // Types
        PrimitiveType = 400,
        PointerType = 401,
        ReferenceType = 402,
        ArrayType = 403,
        FunctionType = 404,
        TupleType = 405,
        GenericType = 406,
        
        // Patterns
        WildcardPattern = 500,
        IdentifierPattern = 501,
        LiteralPattern = 502,
        TuplePattern = 503,
        StructPattern = 504,
        
        // Top-level
        Program = 600,
    };

protected:
    Kind kind_;
    SourceRange source_range_;
    
public:
    explicit ASTNode(Kind kind, SourceRange source_range = {})
        : kind_(kind), source_range_(source_range) {}
    
    virtual ~ASTNode() = default;
    
    // Non-copyable but movable
    ASTNode(const ASTNode&) = delete;
    auto operator=(const ASTNode&) -> ASTNode& = delete;
    ASTNode(ASTNode&&) noexcept = default;
    auto operator=(ASTNode&&) noexcept -> ASTNode& = default;
    
    [[nodiscard]] auto kind() const noexcept -> Kind { return kind_; }
    [[nodiscard]] auto source_range() const noexcept -> const SourceRange& { return source_range_; }
    
    /**
     * @brief Type-safe downcasting to derived node types
     */
    template<typename T>
    [[nodiscard]] auto as() -> T* {
        static_assert(std::is_base_of_v<ASTNode, T>, "T must derive from ASTNode");
        return T::class_of(this) ? static_cast<T*>(this) : nullptr;
    }
    
    template<typename T>
    [[nodiscard]] auto as() const -> const T* {
        static_assert(std::is_base_of_v<ASTNode, T>, "T must derive from ASTNode");
        return T::class_of(this) ? static_cast<const T*>(this) : nullptr;
    }
    
    template<typename T>
    [[nodiscard]] auto is() const noexcept -> bool {
        static_assert(std::is_base_of_v<ASTNode, T>, "T must derive from ASTNode");
        return T::class_of(this);
    }
    
    /**
     * @brief Visitor pattern support
     */
    virtual auto accept(ASTVisitor& visitor) -> void = 0;
    virtual auto accept(ASTVisitor& visitor) const -> void = 0;
    
    /**
     * @brief Pretty-print the AST node for debugging
     */
    virtual auto to_string() const -> String = 0;
};

/**
 * @brief Base class for all expressions
 */
class Expression : public ASTNode {
public:
    explicit Expression(Kind kind, SourceRange source_range = {})
        : ASTNode(kind, source_range) {}
    
    static auto class_of(const ASTNode* node) -> bool {
        auto k = static_cast<u16>(node->kind());
        return k >= 100 && k < 200;
    }
};

/**
 * @brief Base class for all statements
 */
class Statement : public ASTNode {
public:
    explicit Statement(Kind kind, SourceRange source_range = {})
        : ASTNode(kind, source_range) {}
    
    static auto class_of(const ASTNode* node) -> bool {
        auto k = static_cast<u16>(node->kind());
        return k >= 200 && k < 300;
    }
};

/**
 * @brief Base class for all declarations
 */
class Declaration : public ASTNode {
public:
    explicit Declaration(Kind kind, SourceRange source_range = {})
        : ASTNode(kind, source_range) {}
    
    static auto class_of(const ASTNode* node) -> bool {
        auto k = static_cast<u16>(node->kind());
        return k >= 300 && k < 400;
    }
};

/**
 * @brief Integer literal expression
 */
class IntegerLiteral : public Expression {
private:
    i64 value_;
    
public:
    explicit IntegerLiteral(i64 value, SourceRange source_range = {})
        : Expression(Kind::IntegerLiteral, source_range), value_(value) {}
    
    [[nodiscard]] auto value() const noexcept -> i64 { return value_; }
    
    static auto class_of(const ASTNode* node) -> bool {
        return node->kind() == Kind::IntegerLiteral;
    }
    
    auto accept(ASTVisitor& visitor) -> void override;
    auto accept(ASTVisitor& visitor) const -> void override;
    auto to_string() const -> String override;
};

/**
 * @brief Float literal expression
 */
class FloatLiteral : public Expression {
private:
    f64 value_;
    
public:
    explicit FloatLiteral(f64 value, SourceRange source_range = {})
        : Expression(Kind::FloatLiteral, source_range), value_(value) {}
    
    [[nodiscard]] auto value() const noexcept -> f64 { return value_; }
    
    static auto class_of(const ASTNode* node) -> bool {
        return node->kind() == Kind::FloatLiteral;
    }
    
    auto accept(ASTVisitor& visitor) -> void override;
    auto accept(ASTVisitor& visitor) const -> void override;
    auto to_string() const -> String override;
};

/**
 * @brief String literal expression
 */
class StringLiteral : public Expression {
private:
    StringView value_;
    
public:
    explicit StringLiteral(StringView value, SourceRange source_range = {})
        : Expression(Kind::StringLiteral, source_range), value_(value) {}
    
    [[nodiscard]] auto value() const noexcept -> StringView { return value_; }
    
    static auto class_of(const ASTNode* node) -> bool {
        return node->kind() == Kind::StringLiteral;
    }
    
    auto accept(ASTVisitor& visitor) -> void override;
    auto accept(ASTVisitor& visitor) const -> void override;
    auto to_string() const -> String override;
};

/**
 * @brief Boolean literal expression
 */
class BoolLiteral : public Expression {
private:
    bool value_;
    
public:
    explicit BoolLiteral(bool value, SourceRange source_range = {})
        : Expression(Kind::BoolLiteral, source_range), value_(value) {}
    
    [[nodiscard]] auto value() const noexcept -> bool { return value_; }
    
    static auto class_of(const ASTNode* node) -> bool {
        return node->kind() == Kind::BoolLiteral;
    }
    
    auto accept(ASTVisitor& visitor) -> void override;
    auto accept(ASTVisitor& visitor) const -> void override;
    auto to_string() const -> String override;
};

/**
 * @brief Identifier expression
 */
class Identifier : public Expression {
private:
    StringView name_;
    
public:
    explicit Identifier(StringView name, SourceRange source_range = {})
        : Expression(Kind::Identifier, source_range), name_(name) {}
    
    [[nodiscard]] auto name() const noexcept -> StringView { return name_; }
    
    static auto class_of(const ASTNode* node) -> bool {
        return node->kind() == Kind::Identifier;
    }
    
    auto accept(ASTVisitor& visitor) -> void override;
    auto accept(ASTVisitor& visitor) const -> void override;
    auto to_string() const -> String override;
};

/**
 * @brief Binary operation expression
 */
class BinaryExpr : public Expression {
public:
    enum class Operator : u8 {
        Add, Sub, Mul, Div, Mod, Pow,           // Arithmetic
        Equal, NotEqual, Less, Greater,          // Comparison
        LessEqual, GreaterEqual, Spaceship,     // Comparison
        LogicalAnd, LogicalOr,                  // Logical
        BitwiseAnd, BitwiseOr, BitwiseXor,      // Bitwise
        LeftShift, RightShift,                  // Bitwise
        Assign,                                 // Assignment
        Range, RangeInclusive,                  // Range
    };
    
private:
    ASTPtr<Expression> left_;
    Operator operator_;
    ASTPtr<Expression> right_;
    
public:
    BinaryExpr(ASTPtr<Expression> left, Operator op, ASTPtr<Expression> right, SourceRange source_range = {})
        : Expression(Kind::BinaryExpr, source_range)
        , left_(std::move(left))
        , operator_(op)
        , right_(std::move(right)) {}
    
    [[nodiscard]] auto left() const noexcept -> const Expression& { return *left_; }
    [[nodiscard]] auto right() const noexcept -> const Expression& { return *right_; }
    [[nodiscard]] auto get_operator() const noexcept -> Operator { return operator_; }
    
    [[nodiscard]] auto left_mut() noexcept -> Expression& { return *left_; }
    [[nodiscard]] auto right_mut() noexcept -> Expression& { return *right_; }
    
    static auto class_of(const ASTNode* node) -> bool {
        return node->kind() == Kind::BinaryExpr;
    }
    
    auto accept(ASTVisitor& visitor) -> void override;
    auto accept(ASTVisitor& visitor) const -> void override;
    auto to_string() const -> String override;
};

/**
 * @brief Unary operation expression
 */
class UnaryExpr : public Expression {
public:
    enum class Operator : u8 {
        Plus, Minus, Not, BitwiseNot,
        Dereference, AddressOf, 
    };
    
private:
    Operator operator_;
    ASTPtr<Expression> operand_;
    
public:
    UnaryExpr(Operator op, ASTPtr<Expression> operand, SourceRange source_range = {})
        : Expression(Kind::UnaryExpr, source_range)
        , operator_(op)
        , operand_(std::move(operand)) {}
    
    [[nodiscard]] auto get_operator() const noexcept -> Operator { return operator_; }
    [[nodiscard]] auto operand() const noexcept -> const Expression& { return *operand_; }
    [[nodiscard]] auto operand_mut() noexcept -> Expression& { return *operand_; }
    
    static auto class_of(const ASTNode* node) -> bool {
        return node->kind() == Kind::UnaryExpr;
    }
    
    auto accept(ASTVisitor& visitor) -> void override;
    auto accept(ASTVisitor& visitor) const -> void override;
    auto to_string() const -> String override;
};

/**
 * @brief Function call expression
 */
class CallExpr : public Expression {
private:
    ASTPtr<Expression> callee_;
    ASTList<Expression> args_;
    
public:
    CallExpr(ASTPtr<Expression> callee, ASTList<Expression> args, SourceRange source_range = {})
        : Expression(Kind::CallExpr, source_range)
        , callee_(std::move(callee))
        , args_(std::move(args)) {}
    
    [[nodiscard]] auto callee() const noexcept -> const Expression& { return *callee_; }
    [[nodiscard]] auto args() const noexcept -> const ASTList<Expression>& { return args_; }
    
    [[nodiscard]] auto callee_mut() noexcept -> Expression& { return *callee_; }
    [[nodiscard]] auto args_mut() noexcept -> ASTList<Expression>& { return args_; }
    
    static auto class_of(const ASTNode* node) -> bool {
        return node->kind() == Kind::CallExpr;
    }
    
    auto accept(ASTVisitor& visitor) -> void override;
    auto accept(ASTVisitor& visitor) const -> void override;
    auto to_string() const -> String override;
};

/**
 * @brief Block statement containing multiple statements
 */
class Block : public Statement {
private:
    ASTList<Statement> statements_;
    
public:
    explicit Block(ASTList<Statement> statements, SourceRange source_range = {})
        : Statement(Kind::Block, source_range), statements_(std::move(statements)) {}
    
    [[nodiscard]] auto statements() const noexcept -> const ASTList<Statement>& { return statements_; }
    [[nodiscard]] auto statements_mut() noexcept -> ASTList<Statement>& { return statements_; }
    
    static auto class_of(const ASTNode* node) -> bool {
        return node->kind() == Kind::Block;
    }
    
    auto accept(ASTVisitor& visitor) -> void override;
    auto accept(ASTVisitor& visitor) const -> void override;
    auto to_string() const -> String override;
};

/**
 * @brief Variable declaration statement
 */
class VarDecl : public Statement {
private:
    StringView name_;
    ASTPtr<Expression> type_;  // Optional type annotation
    ASTPtr<Expression> init_;  // Optional initializer
    bool is_mutable_;
    
public:
    VarDecl(StringView name, ASTPtr<Expression> type, ASTPtr<Expression> init, 
            bool is_mutable, SourceRange source_range = {})
        : Statement(Kind::VarDecl, source_range)
        , name_(name)
        , type_(std::move(type))
        , init_(std::move(init))
        , is_mutable_(is_mutable) {}
    
    [[nodiscard]] auto name() const noexcept -> StringView { return name_; }
    [[nodiscard]] auto type() const noexcept -> const Expression* { return type_.get(); }
    [[nodiscard]] auto init() const noexcept -> const Expression* { return init_.get(); }
    [[nodiscard]] auto is_mutable() const noexcept -> bool { return is_mutable_; }
    
    static auto class_of(const ASTNode* node) -> bool {
        return node->kind() == Kind::VarDecl;
    }
    
    auto accept(ASTVisitor& visitor) -> void override;
    auto accept(ASTVisitor& visitor) const -> void override;
    auto to_string() const -> String override;
};

/**
 * @brief Function declaration
 */
class FunctionDecl : public Declaration {
public:
    struct Parameter {
        StringView name;
        ASTPtr<Expression> type;
        SourceRange source_range;
    };
    
private:
    StringView name_;
    Vec<Parameter> parameters_;
    ASTPtr<Expression> return_type_;
    ASTPtr<Block> body_;
    
public:
    FunctionDecl(StringView name, Vec<Parameter> parameters, 
                 ASTPtr<Expression> return_type, ASTPtr<Block> body,
                 SourceRange source_range = {})
        : Declaration(Kind::FunctionDecl, source_range)
        , name_(name)
        , parameters_(std::move(parameters))
        , return_type_(std::move(return_type))
        , body_(std::move(body)) {}
    
    [[nodiscard]] auto name() const noexcept -> StringView { return name_; }
    [[nodiscard]] auto parameters() const noexcept -> const Vec<Parameter>& { return parameters_; }
    [[nodiscard]] auto return_type() const noexcept -> const Expression* { return return_type_.get(); }
    [[nodiscard]] auto body() const noexcept -> const Block& { return *body_; }
    
    static auto class_of(const ASTNode* node) -> bool {
        return node->kind() == Kind::FunctionDecl;
    }
    
    auto accept(ASTVisitor& visitor) -> void override;
    auto accept(ASTVisitor& visitor) const -> void override;
    auto to_string() const -> String override;
};

/**
 * @brief Top-level program containing all declarations
 */
class Program : public ASTNode {
private:
    ASTList<Declaration> declarations_;
    
public:
    explicit Program(ASTList<Declaration> declarations, SourceRange source_range = {})
        : ASTNode(Kind::Program, source_range), declarations_(std::move(declarations)) {}
    
    [[nodiscard]] auto declarations() const noexcept -> const ASTList<Declaration>& { return declarations_; }
    [[nodiscard]] auto declarations_mut() noexcept -> ASTList<Declaration>& { return declarations_; }
    
    static auto class_of(const ASTNode* node) -> bool {
        return node->kind() == Kind::Program;
    }
    
    auto accept(ASTVisitor& visitor) -> void override;
    auto accept(ASTVisitor& visitor) const -> void override;
    auto to_string() const -> String override;
};

/**
 * @brief Visitor interface for AST traversal
 */
class ASTVisitor {
public:
    virtual ~ASTVisitor() = default;
    
    // Expressions
    virtual auto visit_integer_literal(IntegerLiteral& node) -> void = 0;
    virtual auto visit_integer_literal(const IntegerLiteral& node) -> void = 0;
    virtual auto visit_float_literal(FloatLiteral& node) -> void = 0;
    virtual auto visit_float_literal(const FloatLiteral& node) -> void = 0;
    virtual auto visit_string_literal(StringLiteral& node) -> void = 0;
    virtual auto visit_string_literal(const StringLiteral& node) -> void = 0;
    virtual auto visit_bool_literal(BoolLiteral& node) -> void = 0;
    virtual auto visit_bool_literal(const BoolLiteral& node) -> void = 0;
    virtual auto visit_identifier(Identifier& node) -> void = 0;
    virtual auto visit_identifier(const Identifier& node) -> void = 0;
    virtual auto visit_binary_expr(BinaryExpr& node) -> void = 0;
    virtual auto visit_binary_expr(const BinaryExpr& node) -> void = 0;
    virtual auto visit_unary_expr(UnaryExpr& node) -> void = 0;
    virtual auto visit_unary_expr(const UnaryExpr& node) -> void = 0;
    virtual auto visit_call_expr(CallExpr& node) -> void = 0;
    virtual auto visit_call_expr(const CallExpr& node) -> void = 0;
    
    // Statements
    virtual auto visit_block(Block& node) -> void = 0;
    virtual auto visit_block(const Block& node) -> void = 0;
    virtual auto visit_var_decl(VarDecl& node) -> void = 0;
    virtual auto visit_var_decl(const VarDecl& node) -> void = 0;
    
    // Declarations
    virtual auto visit_function_decl(FunctionDecl& node) -> void = 0;
    virtual auto visit_function_decl(const FunctionDecl& node) -> void = 0;
    
    // Top-level
    virtual auto visit_program(Program& node) -> void = 0;
    virtual auto visit_program(const Program& node) -> void = 0;
};

/**
 * @brief Factory for creating AST nodes with proper memory management
 */
class ASTFactory {
private:
    memory::MemoryArena<>& arena_;
    
public:
    explicit ASTFactory(memory::MemoryArena<>& arena) : arena_(arena) {}
    
    template<typename T, typename... Args>
    [[nodiscard]] auto create(Args&&... args) -> ASTPtr<T> {
        static_assert(std::is_base_of_v<ASTNode, T>, "T must derive from ASTNode");
        // For now, we use std::make_unique, but we have arena_ available for future memory optimization
        (void)arena_; // Suppress unused warning
        return std::make_unique<T>(std::forward<Args>(args)...);
    }
    
    template<typename T>
    [[nodiscard]] auto create_list() -> ASTList<T> {
        return ASTList<T>{};
    }
};

} // namespace photon::parser