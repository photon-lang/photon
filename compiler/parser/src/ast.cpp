/**
 * @file ast.cpp
 * @brief AST node implementations and visitor pattern methods
 * @author Photon Compiler Team
 * @version 1.0.0
 */

#include "photon/parser/ast.hpp"
#include <sstream>

namespace photon::parser {

// === IntegerLiteral ===

auto IntegerLiteral::accept(ASTVisitor& visitor) -> void {
    visitor.visit_integer_literal(*this);
}

auto IntegerLiteral::accept(ASTVisitor& visitor) const -> void {
    visitor.visit_integer_literal(*this);
}

auto IntegerLiteral::to_string() const -> String {
    return std::to_string(value_);
}

// === FloatLiteral ===

auto FloatLiteral::accept(ASTVisitor& visitor) -> void {
    visitor.visit_float_literal(*this);
}

auto FloatLiteral::accept(ASTVisitor& visitor) const -> void {
    visitor.visit_float_literal(*this);
}

auto FloatLiteral::to_string() const -> String {
    return std::to_string(value_);
}

// === StringLiteral ===

auto StringLiteral::accept(ASTVisitor& visitor) -> void {
    visitor.visit_string_literal(*this);
}

auto StringLiteral::accept(ASTVisitor& visitor) const -> void {
    visitor.visit_string_literal(*this);
}

auto StringLiteral::to_string() const -> String {
    return String("\"") + String(value_) + String("\"");
}

// === BoolLiteral ===

auto BoolLiteral::accept(ASTVisitor& visitor) -> void {
    visitor.visit_bool_literal(*this);
}

auto BoolLiteral::accept(ASTVisitor& visitor) const -> void {
    visitor.visit_bool_literal(*this);
}

auto BoolLiteral::to_string() const -> String {
    return value_ ? "true" : "false";
}

// === Identifier ===

auto Identifier::accept(ASTVisitor& visitor) -> void {
    visitor.visit_identifier(*this);
}

auto Identifier::accept(ASTVisitor& visitor) const -> void {
    visitor.visit_identifier(*this);
}

auto Identifier::to_string() const -> String {
    return String(name_);
}

// === BinaryExpr ===

auto BinaryExpr::accept(ASTVisitor& visitor) -> void {
    visitor.visit_binary_expr(*this);
}

auto BinaryExpr::accept(ASTVisitor& visitor) const -> void {
    visitor.visit_binary_expr(*this);
}

auto BinaryExpr::to_string() const -> String {
    std::ostringstream oss;
    oss << "(" << left_->to_string() << " ";
    
    switch (get_operator()) {
        case Operator::Add: oss << "+"; break;
        case Operator::Sub: oss << "-"; break;
        case Operator::Mul: oss << "*"; break;
        case Operator::Div: oss << "/"; break;
        case Operator::Mod: oss << "%"; break;
        case Operator::Pow: oss << "**"; break;
        case Operator::Equal: oss << "=="; break;
        case Operator::NotEqual: oss << "!="; break;
        case Operator::Less: oss << "<"; break;
        case Operator::Greater: oss << ">"; break;
        case Operator::LessEqual: oss << "<="; break;
        case Operator::GreaterEqual: oss << ">="; break;
        case Operator::Spaceship: oss << "<=>"; break;
        case Operator::LogicalAnd: oss << "&&"; break;
        case Operator::LogicalOr: oss << "||"; break;
        case Operator::BitwiseAnd: oss << "&"; break;
        case Operator::BitwiseOr: oss << "|"; break;
        case Operator::BitwiseXor: oss << "^"; break;
        case Operator::LeftShift: oss << "<<"; break;
        case Operator::RightShift: oss << ">>"; break;
        case Operator::Assign: oss << "="; break;
        case Operator::Range: oss << ".."; break;
        case Operator::RangeInclusive: oss << "..="; break;
    }
    
    oss << " " << right_->to_string() << ")";
    return oss.str();
}

// === UnaryExpr ===

auto UnaryExpr::accept(ASTVisitor& visitor) -> void {
    visitor.visit_unary_expr(*this);
}

auto UnaryExpr::accept(ASTVisitor& visitor) const -> void {
    visitor.visit_unary_expr(*this);
}

auto UnaryExpr::to_string() const -> String {
    std::ostringstream oss;
    oss << "(";
    
    switch (get_operator()) {
        case Operator::Plus: oss << "+"; break;
        case Operator::Minus: oss << "-"; break;
        case Operator::Not: oss << "!"; break;
        case Operator::BitwiseNot: oss << "~"; break;
        case Operator::Dereference: oss << "*"; break;
        case Operator::AddressOf: oss << "&"; break;
    }
    
    oss << operand_->to_string() << ")";
    return oss.str();
}

// === CallExpr ===

auto CallExpr::accept(ASTVisitor& visitor) -> void {
    visitor.visit_call_expr(*this);
}

auto CallExpr::accept(ASTVisitor& visitor) const -> void {
    visitor.visit_call_expr(*this);
}

auto CallExpr::to_string() const -> String {
    std::ostringstream oss;
    oss << callee_->to_string() << "(";
    
    for (usize i = 0; i < args_.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << args_[i]->to_string();
    }
    
    oss << ")";
    return oss.str();
}

// === Block ===

auto Block::accept(ASTVisitor& visitor) -> void {
    visitor.visit_block(*this);
}

auto Block::accept(ASTVisitor& visitor) const -> void {
    visitor.visit_block(*this);
}

auto Block::to_string() const -> String {
    std::ostringstream oss;
    oss << "{\n";
    
    for (const auto& stmt : statements_) {
        oss << "  " << stmt->to_string() << ";\n";
    }
    
    oss << "}";
    return oss.str();
}

// === VarDecl ===

auto VarDecl::accept(ASTVisitor& visitor) -> void {
    visitor.visit_var_decl(*this);
}

auto VarDecl::accept(ASTVisitor& visitor) const -> void {
    visitor.visit_var_decl(*this);
}

auto VarDecl::to_string() const -> String {
    std::ostringstream oss;
    oss << "let ";
    
    if (is_mutable_) {
        oss << "mut ";
    }
    
    oss << name_;
    
    if (type_) {
        oss << ": " << type_->to_string();
    }
    
    if (init_) {
        oss << " = " << init_->to_string();
    }
    
    return oss.str();
}

// === FunctionDecl ===

auto FunctionDecl::accept(ASTVisitor& visitor) -> void {
    visitor.visit_function_decl(*this);
}

auto FunctionDecl::accept(ASTVisitor& visitor) const -> void {
    visitor.visit_function_decl(*this);
}

auto FunctionDecl::to_string() const -> String {
    std::ostringstream oss;
    oss << "fn " << name_ << "(";
    
    for (usize i = 0; i < parameters_.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << parameters_[i].name << ": " << parameters_[i].type->to_string();
    }
    
    oss << ")";
    
    if (return_type_) {
        oss << " -> " << return_type_->to_string();
    }
    
    oss << " " << body_->to_string();
    return oss.str();
}

// === Program ===

auto Program::accept(ASTVisitor& visitor) -> void {
    visitor.visit_program(*this);
}

auto Program::accept(ASTVisitor& visitor) const -> void {
    visitor.visit_program(*this);
}

auto Program::to_string() const -> String {
    std::ostringstream oss;
    
    for (usize i = 0; i < declarations_.size(); ++i) {
        if (i > 0) oss << "\n\n";
        oss << declarations_[i]->to_string();
    }
    
    return oss.str();
}

} // namespace photon::parser