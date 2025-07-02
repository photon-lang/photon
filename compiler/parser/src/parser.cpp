/**
 * @file parser.cpp
 * @brief Recursive descent parser implementation
 * @author Photon Compiler Team
 * @version 1.0.0
 */

#include "photon/parser/parser.hpp"
#include <algorithm>
#include <limits>

namespace photon::parser {

Parser::Parser(lexer::TokenStream tokens, memory::MemoryArena<>& arena, ParserOptions options)
    : tokens_(std::move(tokens))
    , factory_(arena)
    , options_(std::move(options))
    , recursion_depth_(0) {
}

auto Parser::parse_program() -> Result<ASTPtr<Program>, ParseError> {
    auto start_location = current_location();
    
    auto declarations_result = parse_declarations();
    if (!declarations_result) {
        return Result<ASTPtr<Program>, ParseError>(declarations_result.error());
    }
    
    auto source_range = make_range(start_location);
    auto program = factory_.create<Program>(std::move(declarations_result.value()), source_range);
    
    return Result<ASTPtr<Program>, ParseError>(std::move(program));
}

auto Parser::parse_expression() -> Result<ASTPtr<Expression>, ParseError> {
    return parse_expr();
}

auto Parser::parse_statement() -> Result<ASTPtr<Statement>, ParseError> {
    if (current().type == lexer::TokenType::LeftBrace) {
        auto block_result = parse_block();
        if (!block_result) {
            return Result<ASTPtr<Statement>, ParseError>(block_result.error());
        }
        return Result<ASTPtr<Statement>, ParseError>(std::move(block_result.value()));
    }
    
    if (current().type == lexer::TokenType::KwLet) {
        auto var_result = parse_var_decl();
        if (!var_result) {
            return Result<ASTPtr<Statement>, ParseError>(var_result.error());
        }
        return Result<ASTPtr<Statement>, ParseError>(std::move(var_result.value()));
    }
    
    report_error(ParseError::ExpectedStatement);
    return Result<ASTPtr<Statement>, ParseError>(ParseError::ExpectedStatement);
}

auto Parser::parse_declarations() -> Result<ASTList<Declaration>, ParseError> {
    auto declarations = factory_.create_list<Declaration>();
    
    while (!is_eof() && current().type != lexer::TokenType::RightBrace) {
        auto decl_result = parse_declaration();
        if (!decl_result) {
            if (options_.enable_error_recovery) {
                recover(RecoveryStrategy::Synchronize);
                continue;
            }
            return Result<ASTList<Declaration>, ParseError>(decl_result.error());
        }
        
        declarations.push_back(std::move(decl_result.value()));
    }
    
    return Result<ASTList<Declaration>, ParseError>(std::move(declarations));
}

auto Parser::parse_declaration() -> Result<ASTPtr<Declaration>, ParseError> {
    switch (current().type) {
        case lexer::TokenType::KwFn: {
            auto func_result = parse_function_decl();
            if (!func_result) {
                return Result<ASTPtr<Declaration>, ParseError>(func_result.error());
            }
            // Convert FunctionDecl to Declaration using move
            ASTPtr<Declaration> decl = std::move(func_result.value());
            return Result<ASTPtr<Declaration>, ParseError>(std::move(decl));
        }
            
        default:
            report_error(ParseError::ExpectedDeclaration);
            return Result<ASTPtr<Declaration>, ParseError>(ParseError::ExpectedDeclaration);
    }
}

auto Parser::parse_function_decl() -> Result<ASTPtr<FunctionDecl>, ParseError> {
    auto start_location = current_location();
    
    auto fn_token_result = consume(lexer::TokenType::KwFn);
    if (!fn_token_result) {
        return Result<ASTPtr<FunctionDecl>, ParseError>(fn_token_result.error());
    }
    
    auto name_result = parse_identifier();
    if (!name_result) {
        return Result<ASTPtr<FunctionDecl>, ParseError>(name_result.error());
    }
    
    auto params_result = parse_function_parameters();
    if (!params_result) {
        return Result<ASTPtr<FunctionDecl>, ParseError>(params_result.error());
    }
    
    ASTPtr<Expression> return_type = nullptr;
    if (current().type == lexer::TokenType::Arrow) {
        advance(); // consume '->'
        auto type_result = parse_type();
        if (!type_result) {
            return Result<ASTPtr<FunctionDecl>, ParseError>(type_result.error());
        }
        return_type = std::move(type_result.value());
    }
    
    auto body_result = parse_block();
    if (!body_result) {
        return Result<ASTPtr<FunctionDecl>, ParseError>(body_result.error());
    }
    
    auto source_range = make_range(start_location);
    auto function = factory_.create<FunctionDecl>(
        name_result.value()->name(),
        std::move(params_result.value()),
        std::move(return_type),
        std::move(body_result.value()),
        source_range
    );
    
    return Result<ASTPtr<FunctionDecl>, ParseError>(std::move(function));
}

auto Parser::parse_function_parameters() -> Result<Vec<FunctionDecl::Parameter>, ParseError> {
    auto left_paren_result = consume(lexer::TokenType::LeftParen);
    if (!left_paren_result) {
        return Result<Vec<FunctionDecl::Parameter>, ParseError>(left_paren_result.error());
    }
    
    Vec<FunctionDecl::Parameter> parameters;
    
    while (current().type != lexer::TokenType::RightParen && !is_eof()) {
        auto param_result = parse_parameter();
        if (!param_result) {
            return Result<Vec<FunctionDecl::Parameter>, ParseError>(param_result.error());
        }
        
        parameters.push_back(std::move(param_result.value()));
        
        if (current().type == lexer::TokenType::Comma) {
            advance();
        } else if (current().type != lexer::TokenType::RightParen) {
            report_error(ParseError::MissingDelimiter);
            return Result<Vec<FunctionDecl::Parameter>, ParseError>(ParseError::MissingDelimiter);
        }
    }
    
    auto right_paren_result = consume(lexer::TokenType::RightParen);
    if (!right_paren_result) {
        return Result<Vec<FunctionDecl::Parameter>, ParseError>(right_paren_result.error());
    }
    
    return Result<Vec<FunctionDecl::Parameter>, ParseError>(std::move(parameters));
}

auto Parser::parse_parameter() -> Result<FunctionDecl::Parameter, ParseError> {
    auto start_location = current_location();
    
    auto name_result = parse_identifier();
    if (!name_result) {
        return Result<FunctionDecl::Parameter, ParseError>(name_result.error());
    }
    
    auto colon_result = consume(lexer::TokenType::Colon);
    if (!colon_result) {
        return Result<FunctionDecl::Parameter, ParseError>(colon_result.error());
    }
    
    auto type_result = parse_type();
    if (!type_result) {
        return Result<FunctionDecl::Parameter, ParseError>(type_result.error());
    }
    
    auto source_range = make_range(start_location);
    
    FunctionDecl::Parameter param{
        name_result.value()->name(),
        std::move(type_result.value()),
        source_range
    };
    
    return Result<FunctionDecl::Parameter, ParseError>(std::move(param));
}

auto Parser::parse_block() -> Result<ASTPtr<Block>, ParseError> {
    auto start_location = current_location();
    
    auto left_brace_result = consume(lexer::TokenType::LeftBrace);
    if (!left_brace_result) {
        return Result<ASTPtr<Block>, ParseError>(left_brace_result.error());
    }
    
    auto statements_result = parse_statement_list();
    if (!statements_result) {
        return Result<ASTPtr<Block>, ParseError>(statements_result.error());
    }
    
    auto right_brace_result = consume(lexer::TokenType::RightBrace);
    if (!right_brace_result) {
        return Result<ASTPtr<Block>, ParseError>(right_brace_result.error());
    }
    
    auto source_range = make_range(start_location);
    auto block = factory_.create<Block>(std::move(statements_result.value()), source_range);
    
    return Result<ASTPtr<Block>, ParseError>(std::move(block));
}

auto Parser::parse_statement_list() -> Result<ASTList<Statement>, ParseError> {
    auto statements = factory_.create_list<Statement>();
    
    while (current().type != lexer::TokenType::RightBrace && !is_eof()) {
        if (current().type == lexer::TokenType::KwLet) {
            auto var_result = parse_var_decl();
            if (!var_result) {
                if (options_.enable_error_recovery) {
                    recover(RecoveryStrategy::Synchronize);
                    continue;
                }
                return Result<ASTList<Statement>, ParseError>(var_result.error());
            }
            statements.push_back(std::move(var_result.value()));
        } else {
            if (current().type == lexer::TokenType::Newline || current().type == lexer::TokenType::Semicolon) {
                advance();
                continue;
            }
            
            auto expr_result = parse_expr();
            if (!expr_result) {
                if (options_.enable_error_recovery) {
                    recover(RecoveryStrategy::Synchronize);
                    continue;
                }
                return Result<ASTList<Statement>, ParseError>(expr_result.error());
            }
            
            if (current().type == lexer::TokenType::Semicolon) {
                advance();
            }
        }
    }
    
    return Result<ASTList<Statement>, ParseError>(std::move(statements));
}

auto Parser::parse_var_decl() -> Result<ASTPtr<VarDecl>, ParseError> {
    auto start_location = current_location();
    
    auto let_result = consume(lexer::TokenType::KwLet);
    if (!let_result) {
        return Result<ASTPtr<VarDecl>, ParseError>(let_result.error());
    }
    
    bool is_mutable = false;
    if (current().type == lexer::TokenType::KwMut) {
        is_mutable = true;
        advance();
    }
    
    auto name_result = parse_identifier();
    if (!name_result) {
        return Result<ASTPtr<VarDecl>, ParseError>(name_result.error());
    }
    
    ASTPtr<Expression> type_annotation = nullptr;
    if (current().type == lexer::TokenType::Colon) {
        advance();
        auto type_result = parse_type();
        if (!type_result) {
            return Result<ASTPtr<VarDecl>, ParseError>(type_result.error());
        }
        type_annotation = std::move(type_result.value());
    }
    
    ASTPtr<Expression> initializer = nullptr;
    if (current().type == lexer::TokenType::Assign) {
        advance();
        auto init_result = parse_expr();
        if (!init_result) {
            return Result<ASTPtr<VarDecl>, ParseError>(init_result.error());
        }
        initializer = std::move(init_result.value());
    }
    
    auto source_range = make_range(start_location);
    auto var_decl = factory_.create<VarDecl>(
        name_result.value()->name(),
        std::move(type_annotation),
        std::move(initializer),
        is_mutable,
        source_range
    );
    
    return Result<ASTPtr<VarDecl>, ParseError>(std::move(var_decl));
}

auto Parser::parse_expr(i32 min_precedence) -> Result<ASTPtr<Expression>, ParseError> {
    auto recursion_guard = enter_recursion();
    if (!recursion_guard) {
        return Result<ASTPtr<Expression>, ParseError>(recursion_guard.error());
    }
    
    auto left_result = parse_primary();
    if (!left_result) {
        exit_recursion();
        return left_result;
    }
    
    auto left = std::move(left_result.value());
    
    while (!is_eof()) {
        auto current_precedence = get_precedence(current().type);
        if (current_precedence < min_precedence) {
            break;
        }
        
        auto op_token = current();
        advance();
        
        auto next_min_precedence = current_precedence;
        if (!is_right_associative(op_token.type)) {
            next_min_precedence++;
        }
        
        auto right_result = parse_expr(next_min_precedence);
        if (!right_result) {
            exit_recursion();
            return right_result;
        }
        
        auto binary_op_result = token_to_binary_op(op_token.type);
        if (!binary_op_result) {
            exit_recursion();
            return Result<ASTPtr<Expression>, ParseError>(binary_op_result.error());
        }
        
        auto source_range = std::make_pair(left->source_range().first, right_result.value()->source_range().second);
        left = factory_.create<BinaryExpr>(
            std::move(left),
            binary_op_result.value(),
            std::move(right_result.value()),
            source_range
        );
    }
    
    auto postfix_result = parse_postfix(std::move(left));
    exit_recursion();
    return postfix_result;
}

auto Parser::parse_primary() -> Result<ASTPtr<Expression>, ParseError> {
    switch (current().type) {
        case lexer::TokenType::IntegerLiteral:
        case lexer::TokenType::FloatLiteral:
        case lexer::TokenType::StringLiteral:
        case lexer::TokenType::BoolLiteral:
            return parse_literal();
            
        case lexer::TokenType::Identifier: {
            auto id_result = parse_identifier();
            if (!id_result) {
                return Result<ASTPtr<Expression>, ParseError>(id_result.error());
            }
            // Convert Identifier to Expression using move
            ASTPtr<Expression> expr = std::move(id_result.value());
            return Result<ASTPtr<Expression>, ParseError>(std::move(expr));
        }
            
        case lexer::TokenType::LeftParen: {
            advance(); // consume '('
            auto expr_result = parse_expr();
            if (!expr_result) {
                return expr_result;
            }
            
            auto right_paren_result = consume(lexer::TokenType::RightParen);
            if (!right_paren_result) {
                return Result<ASTPtr<Expression>, ParseError>(right_paren_result.error());
            }
            
            return expr_result;
        }
        
        case lexer::TokenType::Plus:
        case lexer::TokenType::Minus:
        case lexer::TokenType::Not:
        case lexer::TokenType::Tilde:
        case lexer::TokenType::Ampersand:
        case lexer::TokenType::Star: {
            auto op_token = current();
            advance();
            
            auto unary_op_result = token_to_unary_op(op_token.type);
            if (!unary_op_result) {
                return Result<ASTPtr<Expression>, ParseError>(unary_op_result.error());
            }
            
            auto operand_result = parse_expr(to_int(Precedence::Unary));
            if (!operand_result) {
                return operand_result;
            }
            
            auto source_range = std::make_pair(op_token.location, operand_result.value()->source_range().second);
            auto unary_expr = factory_.create<UnaryExpr>(
                unary_op_result.value(),
                std::move(operand_result.value()),
                source_range
            );
            
            return Result<ASTPtr<Expression>, ParseError>(std::move(unary_expr));
        }
        
        default:
            report_error(ParseError::ExpectedExpression);
            return Result<ASTPtr<Expression>, ParseError>(ParseError::ExpectedExpression);
    }
}

auto Parser::parse_postfix(ASTPtr<Expression> left) -> Result<ASTPtr<Expression>, ParseError> {
    while (!is_eof()) {
        switch (current().type) {
            case lexer::TokenType::LeftParen: {
                auto call_result = parse_call_expr(std::move(left));
                if (!call_result) {
                    return Result<ASTPtr<Expression>, ParseError>(call_result.error());
                }
                // Convert CallExpr to Expression using move
                left = std::move(call_result.value());
                break;
            }
            
            default:
                return Result<ASTPtr<Expression>, ParseError>(std::move(left));
        }
    }
    
    return Result<ASTPtr<Expression>, ParseError>(std::move(left));
}

auto Parser::parse_literal() -> Result<ASTPtr<Expression>, ParseError> {
    auto token = current();
    auto location = current_location();
    advance();
    
    switch (token.type) {
        case lexer::TokenType::IntegerLiteral: {
            if (!token.value.is<i64>()) {
                report_error(ParseError::InvalidLiteral);
                return Result<ASTPtr<Expression>, ParseError>(ParseError::InvalidLiteral);
            }
            auto literal = factory_.create<IntegerLiteral>(
                token.value.get<i64>(),
                std::make_pair(location, location)
            );
            return Result<ASTPtr<Expression>, ParseError>(std::move(literal));
        }
        
        case lexer::TokenType::FloatLiteral: {
            if (!token.value.is<f64>()) {
                report_error(ParseError::InvalidLiteral);
                return Result<ASTPtr<Expression>, ParseError>(ParseError::InvalidLiteral);
            }
            auto literal = factory_.create<FloatLiteral>(
                token.value.get<f64>(),
                std::make_pair(location, location)
            );
            return Result<ASTPtr<Expression>, ParseError>(std::move(literal));
        }
        
        case lexer::TokenType::StringLiteral: {
            if (!token.value.is<StringView>()) {
                report_error(ParseError::InvalidLiteral);
                return Result<ASTPtr<Expression>, ParseError>(ParseError::InvalidLiteral);
            }
            auto literal = factory_.create<StringLiteral>(
                token.value.get<StringView>(),
                std::make_pair(location, location)
            );
            return Result<ASTPtr<Expression>, ParseError>(std::move(literal));
        }
        
        case lexer::TokenType::BoolLiteral: {
            if (!token.value.is<bool>()) {
                report_error(ParseError::InvalidLiteral);
                return Result<ASTPtr<Expression>, ParseError>(ParseError::InvalidLiteral);
            }
            auto literal = factory_.create<BoolLiteral>(
                token.value.get<bool>(),
                std::make_pair(location, location)
            );
            return Result<ASTPtr<Expression>, ParseError>(std::move(literal));
        }
        
        default:
            report_error(ParseError::InvalidLiteral);
            return Result<ASTPtr<Expression>, ParseError>(ParseError::InvalidLiteral);
    }
}

auto Parser::parse_identifier() -> Result<ASTPtr<Identifier>, ParseError> {
    if (current().type != lexer::TokenType::Identifier) {
        report_error(ParseError::ExpectedIdentifier);
        return Result<ASTPtr<Identifier>, ParseError>(ParseError::ExpectedIdentifier);
    }
    
    auto token = current();
    auto location = current_location();
    advance();
    
    auto identifier = factory_.create<Identifier>(
        token.text(),
        std::make_pair(location, location)
    );
    
    return Result<ASTPtr<Identifier>, ParseError>(std::move(identifier));
}

auto Parser::parse_call_expr(ASTPtr<Expression> callee) -> Result<ASTPtr<CallExpr>, ParseError> {
    auto args_result = parse_call_arguments();
    if (!args_result) {
        return Result<ASTPtr<CallExpr>, ParseError>(args_result.error());
    }
    
    auto source_range = callee->source_range();
    auto call_expr = factory_.create<CallExpr>(
        std::move(callee),
        std::move(args_result.value()),
        source_range
    );
    
    return Result<ASTPtr<CallExpr>, ParseError>(std::move(call_expr));
}

auto Parser::parse_call_arguments() -> Result<ASTList<Expression>, ParseError> {
    auto left_paren_result = consume(lexer::TokenType::LeftParen);
    if (!left_paren_result) {
        return Result<ASTList<Expression>, ParseError>(left_paren_result.error());
    }
    
    auto arguments = factory_.create_list<Expression>();
    
    while (current().type != lexer::TokenType::RightParen && !is_eof()) {
        auto arg_result = parse_expr();
        if (!arg_result) {
            return Result<ASTList<Expression>, ParseError>(arg_result.error());
        }
        
        arguments.push_back(std::move(arg_result.value()));
        
        if (current().type == lexer::TokenType::Comma) {
            advance();
        } else if (current().type != lexer::TokenType::RightParen) {
            report_error(ParseError::MissingDelimiter);
            return Result<ASTList<Expression>, ParseError>(ParseError::MissingDelimiter);
        }
    }
    
    auto right_paren_result = consume(lexer::TokenType::RightParen);
    if (!right_paren_result) {
        return Result<ASTList<Expression>, ParseError>(right_paren_result.error());
    }
    
    return Result<ASTList<Expression>, ParseError>(std::move(arguments));
}

auto Parser::parse_type() -> Result<ASTPtr<Expression>, ParseError> {
    auto id_result = parse_identifier();
    if (!id_result) {
        return Result<ASTPtr<Expression>, ParseError>(id_result.error());
    }
    // Convert Identifier to Expression using move
    ASTPtr<Expression> expr = std::move(id_result.value());
    return Result<ASTPtr<Expression>, ParseError>(std::move(expr));
}

auto Parser::get_precedence(lexer::TokenType type) const noexcept -> i32 {
    switch (type) {
        case lexer::TokenType::Assign:
        case lexer::TokenType::PlusAssign:
        case lexer::TokenType::MinusAssign:
        case lexer::TokenType::StarAssign:
        case lexer::TokenType::SlashAssign:
        case lexer::TokenType::PercentAssign:
        case lexer::TokenType::AndAssign:
        case lexer::TokenType::OrAssign:
        case lexer::TokenType::XorAssign:
        case lexer::TokenType::LeftShiftAssign:
        case lexer::TokenType::RightShiftAssign:
            return to_int(Precedence::Assignment);
            
        case lexer::TokenType::DotDot:
        case lexer::TokenType::DotDotEqual:
            return to_int(Precedence::Range);
            
        case lexer::TokenType::Or:
            return to_int(Precedence::LogicalOr);
            
        case lexer::TokenType::And:
            return to_int(Precedence::LogicalAnd);
            
        case lexer::TokenType::Equal:
        case lexer::TokenType::NotEqual:
        case lexer::TokenType::Spaceship:
            return to_int(Precedence::Equality);
            
        case lexer::TokenType::Less:
        case lexer::TokenType::Greater:
        case lexer::TokenType::LessEqual:
        case lexer::TokenType::GreaterEqual:
            return to_int(Precedence::Comparison);
            
        case lexer::TokenType::Pipe:
            return to_int(Precedence::BitwiseOr);
            
        case lexer::TokenType::Caret:
            return to_int(Precedence::BitwiseXor);
            
        case lexer::TokenType::Ampersand:
            return to_int(Precedence::BitwiseAnd);
            
        case lexer::TokenType::LeftShift:
        case lexer::TokenType::RightShift:
            return to_int(Precedence::Shift);
            
        case lexer::TokenType::Plus:
        case lexer::TokenType::Minus:
            return to_int(Precedence::Addition);
            
        case lexer::TokenType::Star:
        case lexer::TokenType::Slash:
        case lexer::TokenType::Percent:
            return to_int(Precedence::Multiplication);
            
        case lexer::TokenType::StarStar:
            return to_int(Precedence::Power);
            
        default:
            return to_int(Precedence::None);
    }
}

auto Parser::is_right_associative(lexer::TokenType type) const noexcept -> bool {
    switch (type) {
        case lexer::TokenType::Assign:
        case lexer::TokenType::PlusAssign:
        case lexer::TokenType::MinusAssign:
        case lexer::TokenType::StarAssign:
        case lexer::TokenType::SlashAssign:
        case lexer::TokenType::PercentAssign:
        case lexer::TokenType::AndAssign:
        case lexer::TokenType::OrAssign:
        case lexer::TokenType::XorAssign:
        case lexer::TokenType::LeftShiftAssign:
        case lexer::TokenType::RightShiftAssign:
        case lexer::TokenType::StarStar:
            return true;
            
        default:
            return false;
    }
}

auto Parser::token_to_binary_op(lexer::TokenType type) const noexcept -> Result<BinaryExpr::Operator, ParseError> {
    switch (type) {
        case lexer::TokenType::Plus:
            return Result<BinaryExpr::Operator, ParseError>(BinaryExpr::Operator::Add);
        case lexer::TokenType::Minus:
            return Result<BinaryExpr::Operator, ParseError>(BinaryExpr::Operator::Sub);
        case lexer::TokenType::Star:
            return Result<BinaryExpr::Operator, ParseError>(BinaryExpr::Operator::Mul);
        case lexer::TokenType::Slash:
            return Result<BinaryExpr::Operator, ParseError>(BinaryExpr::Operator::Div);
        case lexer::TokenType::Percent:
            return Result<BinaryExpr::Operator, ParseError>(BinaryExpr::Operator::Mod);
        case lexer::TokenType::StarStar:
            return Result<BinaryExpr::Operator, ParseError>(BinaryExpr::Operator::Pow);
        case lexer::TokenType::Equal:
            return Result<BinaryExpr::Operator, ParseError>(BinaryExpr::Operator::Equal);
        case lexer::TokenType::NotEqual:
            return Result<BinaryExpr::Operator, ParseError>(BinaryExpr::Operator::NotEqual);
        case lexer::TokenType::Less:
            return Result<BinaryExpr::Operator, ParseError>(BinaryExpr::Operator::Less);
        case lexer::TokenType::Greater:
            return Result<BinaryExpr::Operator, ParseError>(BinaryExpr::Operator::Greater);
        case lexer::TokenType::LessEqual:
            return Result<BinaryExpr::Operator, ParseError>(BinaryExpr::Operator::LessEqual);
        case lexer::TokenType::GreaterEqual:
            return Result<BinaryExpr::Operator, ParseError>(BinaryExpr::Operator::GreaterEqual);
        case lexer::TokenType::Spaceship:
            return Result<BinaryExpr::Operator, ParseError>(BinaryExpr::Operator::Spaceship);
        case lexer::TokenType::And:
            return Result<BinaryExpr::Operator, ParseError>(BinaryExpr::Operator::LogicalAnd);
        case lexer::TokenType::Or:
            return Result<BinaryExpr::Operator, ParseError>(BinaryExpr::Operator::LogicalOr);
        case lexer::TokenType::Ampersand:
            return Result<BinaryExpr::Operator, ParseError>(BinaryExpr::Operator::BitwiseAnd);
        case lexer::TokenType::Pipe:
            return Result<BinaryExpr::Operator, ParseError>(BinaryExpr::Operator::BitwiseOr);
        case lexer::TokenType::Caret:
            return Result<BinaryExpr::Operator, ParseError>(BinaryExpr::Operator::BitwiseXor);
        case lexer::TokenType::LeftShift:
            return Result<BinaryExpr::Operator, ParseError>(BinaryExpr::Operator::LeftShift);
        case lexer::TokenType::RightShift:
            return Result<BinaryExpr::Operator, ParseError>(BinaryExpr::Operator::RightShift);
        case lexer::TokenType::Assign:
            return Result<BinaryExpr::Operator, ParseError>(BinaryExpr::Operator::Assign);
        case lexer::TokenType::DotDot:
            return Result<BinaryExpr::Operator, ParseError>(BinaryExpr::Operator::Range);
        case lexer::TokenType::DotDotEqual:
            return Result<BinaryExpr::Operator, ParseError>(BinaryExpr::Operator::RangeInclusive);
        default:
            return Result<BinaryExpr::Operator, ParseError>(ParseError::ExpectedOperator);
    }
}

auto Parser::token_to_unary_op(lexer::TokenType type) const noexcept -> Result<UnaryExpr::Operator, ParseError> {
    switch (type) {
        case lexer::TokenType::Plus:
            return Result<UnaryExpr::Operator, ParseError>(UnaryExpr::Operator::Plus);
        case lexer::TokenType::Minus:
            return Result<UnaryExpr::Operator, ParseError>(UnaryExpr::Operator::Minus);
        case lexer::TokenType::Not:
            return Result<UnaryExpr::Operator, ParseError>(UnaryExpr::Operator::Not);
        case lexer::TokenType::Tilde:
            return Result<UnaryExpr::Operator, ParseError>(UnaryExpr::Operator::BitwiseNot);
        case lexer::TokenType::Star:
            return Result<UnaryExpr::Operator, ParseError>(UnaryExpr::Operator::Dereference);
        case lexer::TokenType::Ampersand:
            return Result<UnaryExpr::Operator, ParseError>(UnaryExpr::Operator::AddressOf);
        default:
            return Result<UnaryExpr::Operator, ParseError>(ParseError::ExpectedOperator);
    }
}

auto Parser::consume(lexer::TokenType expected) -> Result<lexer::Token, ParseError> {
    if (current().type != expected) {
        report_error(ParseError::UnexpectedToken);
        return Result<lexer::Token, ParseError>(ParseError::UnexpectedToken);
    }
    
    auto token = current();
    advance();
    return Result<lexer::Token, ParseError>(std::move(token));
}

auto Parser::match_token(lexer::TokenType type) const noexcept -> bool {
    return current().type == type;
}

auto Parser::match_any(std::initializer_list<lexer::TokenType> types) const noexcept -> bool {
    return std::any_of(types.begin(), types.end(), [this](lexer::TokenType type) {
        return match_token(type);
    });
}

auto Parser::report_error(ParseError error) -> void {
    errors_.push_back(error);
}

auto Parser::recover(RecoveryStrategy strategy) -> void {
    switch (strategy) {
        case RecoveryStrategy::Skip:
            if (!is_eof()) {
                advance();
            }
            break;
            
        case RecoveryStrategy::Synchronize:
            synchronize();
            break;
            
        case RecoveryStrategy::Insert:
            break;
            
        case RecoveryStrategy::Abort:
            break;
    }
}

auto Parser::synchronize() -> void {
    while (!is_eof() && !is_synchronization_point()) {
        advance();
    }
}

auto Parser::is_synchronization_point() const noexcept -> bool {
    switch (current().type) {
        case lexer::TokenType::KwFn:
        case lexer::TokenType::KwStruct:
        case lexer::TokenType::KwEnum:
        case lexer::TokenType::KwTrait:
        case lexer::TokenType::KwImpl:
        case lexer::TokenType::KwLet:
        case lexer::TokenType::KwConst:
        case lexer::TokenType::LeftBrace:
        case lexer::TokenType::RightBrace:
        case lexer::TokenType::Semicolon:
            return true;
        default:
            return false;
    }
}

auto Parser::enter_recursion() -> Result<void, ParseError> {
    if (recursion_depth_ >= options_.max_recursion_depth) {
        return Result<void, ParseError>(ParseError::NestedTooDeep);
    }
    recursion_depth_++;
    return Result<void, ParseError>();
}

auto Parser::exit_recursion() noexcept -> void {
    if (recursion_depth_ > 0) {
        recursion_depth_--;
    }
}

auto Parser::current_location() const noexcept -> diagnostics::SourceLocation {
    return current().location;
}

auto Parser::make_range(const diagnostics::SourceLocation& start) const noexcept -> SourceRange {
    return std::make_pair(start, current_location());
}

} // namespace photon::parser