#include "parser.h"
#include <stdexcept>
#include <iostream>

Parser::Parser(Lexer& lexer) : lexer(lexer) {
    currentToken = lexer.nextToken();
    peekToken = lexer.nextToken();
}
 
void Parser::advance() {
    currentToken = peekToken;
    peekToken = lexer.nextToken();
}

std::unique_ptr<ProgramNode> Parser::parseProgram() {
    auto program = std::make_unique<ProgramNode>();
    
    while (currentToken.type != Token::Eof) {
        auto stmt = parseStatement();
        if (!stmt) {
            throw std::runtime_error("Failed to parse statement");
        }
        program->statements.push_back(std::move(stmt));
    }
    
    return program;
}

std::unique_ptr<ASTNode> Parser::parseStatement() {
    if (currentToken.type == Token::Int || currentToken.type == Token::StringType
        || currentToken.type == Token::Bool || currentToken.type == Token::Float
        || currentToken.type == Token::Char || currentToken.type == Token::Array) {
            // add ; check for all parsestatement just like ident
        return parseVarDecl();
    }
    if (currentToken.type == Token::Ident) {
        auto tempreturn = parseAssignment();
        if (currentToken.type != Token::Semicolon) {
            throw std::runtime_error("Expected ';' after assignment");
        }
        advance();
        return tempreturn;
    }
    if (currentToken.type == Token::If) {
        return parseIfStatement();
    }
    if (currentToken.type == Token::Print) {  // New case
        advance(); // Consume 'print'
        if (currentToken.type != Token::LeftParen) {
            throw std::runtime_error("Expected '(' after 'print'");
        }
        advance(); // Consume '('
        auto expr = parseExpression(); // Parse the expression inside print()
        if (currentToken.type != Token::RightParen) {
            throw std::runtime_error("Expected ')' after print expression");
        }
        advance(); // Consume ')'
        if (currentToken.type != Token::Semicolon) {
            throw std::runtime_error("Expected ';' after print statement");
        }
        advance(); // Consume ';'
        return std::make_unique<PrintNode>(std::move(expr));
    }
    if (currentToken.type == Token::For || currentToken.type == Token::Foreach) {
        return parseLoop();
    }
    if (currentToken.type == Token::Try) {
        advance(); // Consume 'try'
        if (currentToken.type != Token::LeftBrace) {
            throw std::runtime_error("Expected '{' after 'try'");
        }
        auto tryBlock = parseBlock();
        if (currentToken.type != Token::Catch) {
            throw std::runtime_error("Expected 'catch' after try block");
        }
        advance(); // Consume 'catch'
        if (currentToken.type != Token::LeftParen) {
            throw std::runtime_error("Expected '(' after 'catch'");
        }
        advance(); // Consume '('
        if (currentToken.type != Token::Error) {
            throw std::runtime_error("Expected 'Error' in catch");
        }
        advance(); // Consume 'Error'
        if (currentToken.type != Token::Ident) {
            throw std::runtime_error("Expected identifier after 'Error'");
        }
        std::string errorVar = currentToken.lexeme;
        advance(); // Consume ident (e)
        if (currentToken.type != Token::RightParen) {
            throw std::runtime_error("Expected ')' after catch variable");
        }
        advance(); // Consume ')'
        if (currentToken.type != Token::LeftBrace) {
            throw std::runtime_error("Expected '{' after 'catch'");
        }
        auto catchBlock = parseBlock();
        return std::make_unique<TryCatchNode>(std::move(tryBlock), 
                                             std::move(catchBlock), 
                                             errorVar);
    }
    if (currentToken.type == Token::Match) {
        return parseMatch(); // No ';' required after match
    }
    

    //  std::cout << (currentToken.type == Token::Semicolon) << std::endl;
    throw std::runtime_error("Unexpected token in statement");
}

std::unique_ptr<ASTNode> Parser::parseVarDecl() {
    VarType type;
    if (currentToken.type == Token::Int) type = VarType::INT;
    else if (currentToken.type == Token::StringType) type = VarType::STRING;
    else if (currentToken.type == Token::Bool) type = VarType::BOOL;
    else if (currentToken.type == Token::Float) type = VarType::FLOAT;
    else if (currentToken.type == Token::Char) type = VarType::CHAR;
    else if (currentToken.type == Token::Array) type = VarType::ARRAY;
    else throw std::runtime_error("Unknown type in variable declaration");
    advance(); // Consume type
    
    if (currentToken.type != Token::Ident) {
        throw std::runtime_error("Expected identifier after type");
    }
    std::string name = currentToken.lexeme;
    advance(); // Consume ident

    if (currentToken.type == Token::Comma) {
        return parseVarDeclMultiVariable(type, name);
    }
    if (currentToken.type == Token::Semicolon) {
        advance();
        return std::make_unique<VarDeclNode>(type, name, nullptr);
    }
    if (currentToken.type != Token::Equal) {
        throw std::runtime_error("Expected '=' in variable declaration");
    }
    advance(); // Consume '='

    // Parse expression for value
    std::unique_ptr<ASTNode> value = parseExpression();

    if (currentToken.type != Token::Semicolon) {
        throw std::runtime_error("Expected ';' after variable declaration");
    }
    advance(); // Consume ';'
    
    return std::make_unique<VarDeclNode>(type, name, std::move(value));
}

std::unique_ptr<ASTNode> Parser::parseExpression() {
    if (currentToken.type == Token::LeftParen || currentToken.type == Token::negLeftParen) {
        auto pervType = currentToken.type;
        advance(); // consume '('
        auto left = parseExpression();
        if (currentToken.type != Token::RightParen) {
            throw std::runtime_error("Expected ')' at line " + std::to_string(currentToken.line));
        }
        advance(); // consume ')'
        if(currentToken.type != Token::Semicolon){
            // std::unique_ptr<ASTNode> left;
            while (currentToken.type == Token::Plus || currentToken.type == Token::Minus ||
                currentToken.type == Token::Star || currentToken.type == Token::Slash || 
                currentToken.type == Token::EqualEqual || currentToken.type == Token::LessEqual ||
                currentToken.type == Token::NotEqual || currentToken.type == Token::Greater ||
                currentToken.type == Token::GreaterEqual || currentToken.type == Token::Less ||
                currentToken.type == Token::And || currentToken.type == Token::Or ||
                currentToken.type == Token::SignedIntLiteral || currentToken.type == Token::Modulo ||
                currentToken.type == Token::Xor  ) {
             BinaryOp op;
             if (currentToken.type == Token::Plus) {
                 advance(); // Consume '+'
                 auto right = parsePrimary();
                 // Handle string concatenation
                 if (op == BinaryOp::XOR) {
                     if (!dynamic_cast<BoolLiteral*>(left.get()) && !dynamic_cast<VarRefNode*>(left.get()) &&
                         !dynamic_cast<BoolLiteral*>(right.get()) && !dynamic_cast<VarRefNode*>(right.get())) {
                         throw std::runtime_error("XOR requires boolean operands at line " + std::to_string(currentToken.line));
                     }
                 }
                 if (dynamic_cast<StrLiteral*>(left.get()) || dynamic_cast<StrLiteral*>(right.get()) ||
                     dynamic_cast<VarRefNode*>(left.get()) || dynamic_cast<VarRefNode*>(right.get())) {    //   needs to be corrected
                     left = std::make_unique<ConcatNode>(std::move(left), std::move(right));
                 } else {
                     op = BinaryOp::ADD;
                     left = std::make_unique<BinaryOpNode>(op, std::move(left), std::move(right));
                 }
             } else
             if (currentToken.type != Token::SignedIntLiteral){
                 switch (currentToken.type) {
                     case Token::Plus: op = BinaryOp::ADD; break;
                     case Token::Minus: op = BinaryOp::SUBTRACT; break;
                     case Token::Star: op = BinaryOp::MULTIPLY; break;
                     case Token::Slash: op = BinaryOp::DIVIDE; break;
                     case Token::EqualEqual: op = BinaryOp::EQUAL; break;
                     case Token::LessEqual: op = BinaryOp::LESS_EQUAL; break;
                     case Token::NotEqual: op = BinaryOp::NOT_EQUAL; break;
                     case Token::Greater: op = BinaryOp::GREATER; break;
                     case Token::GreaterEqual: op = BinaryOp::GREATER_EQUAL; break;
                     case Token::Less: op = BinaryOp::LESS; break;
                     case Token::And: op = BinaryOp::AND; break;
                     case Token::Or: op = BinaryOp::OR; break;
                     case Token::Modulo: op = BinaryOp::MODULO; break;
                     case Token::Xor: op = BinaryOp::XOR; break; 
                     // case Token::IntLiteral: BinaryOp::ADD; break;
                     default: throw std::runtime_error("Unknown binary operator");
                 }
                 advance(); // consume operator
                 auto right = parsePrimary();
     
                 if(op == BinaryOp::ADD && currentToken.type == Token::StrLiteral){
                     if (dynamic_cast<StrLiteral*>(left.get()) || dynamic_cast<StrLiteral*>(right.get()) ||
                     dynamic_cast<VarRefNode*>(left.get()) || dynamic_cast<VarRefNode*>(right.get())) {    //   needs to be corrected
                     left = std::make_unique<ConcatNode>(std::move(left), std::move(right));
                     } else {
                         // error : NOT-string + string -- semantics
                     }
                 }else 
                 
                 left = std::make_unique<BinaryOpNode>(op, std::move(left), std::move(right));
             }else{
                 auto right = std::make_unique<IntLiteral>(std::stoi(currentToken.lexeme));
                 left = std::make_unique<BinaryOpNode>(BinaryOp::ADD, std::move(left), std::move(right));
                 advance(); // consume operator
     
                 // op = BinaryOp::ADD;
                 // auto right = std::make_unique<IntLiteral>(std::stoi(currentToken.lexeme));
                 // advance(); // consume the int literal
                 // left = std::make_unique<BinaryOpNode>(op, std::move(left), std::move(right));
             }
         }
         if (currentToken.type == Token::PlusPlus || currentToken.type == Token::MinusMinus) {
             UnaryOp op = (currentToken.type == Token::PlusPlus) ? UnaryOp::INCREMENT : UnaryOp::DECREMENT;
             advance(); // Consume '++' or '--'
             if (!dynamic_cast<VarRefNode*>(left.get()) && !dynamic_cast<BinaryOpNode*>(left.get())) {
                 throw std::runtime_error("Increment/decrement can only be applied to variables or array elements");
             }
             if (auto* binOp = dynamic_cast<BinaryOpNode*>(left.get())) {
                 if (binOp->op != BinaryOp::INDEX) {
                     throw std::runtime_error("Increment/decrement can only be applied to array elements with index");
                 }
             }
             left = std::make_unique<UnaryOpNode>(op, std::move(left));
         }
     
         return parseTernary(std::move(left));
        }
        if (pervType == Token::LeftParen) return left;
        else return std::make_unique<UnaryOpNode>(UnaryOp::NEGATE, std::move(left));
    }
    auto left = parsePrimary();
    while (currentToken.type == Token::Plus || currentToken.type == Token::Minus ||
           currentToken.type == Token::Star || currentToken.type == Token::Slash || 
           currentToken.type == Token::EqualEqual || currentToken.type == Token::LessEqual ||
           currentToken.type == Token::NotEqual || currentToken.type == Token::Greater ||
           currentToken.type == Token::GreaterEqual || currentToken.type == Token::Less ||
           currentToken.type == Token::And || currentToken.type == Token::Or ||
           currentToken.type == Token::SignedIntLiteral || currentToken.type == Token::Modulo ||
           currentToken.type == Token::Xor  ) {
        BinaryOp op;
        if (currentToken.type == Token::Plus) {
            advance(); // Consume '+'
            auto right = parsePrimary();
            // Handle string concatenation
            if (op == BinaryOp::XOR) {
                if (!dynamic_cast<BoolLiteral*>(left.get()) && !dynamic_cast<VarRefNode*>(left.get()) &&
                    !dynamic_cast<BoolLiteral*>(right.get()) && !dynamic_cast<VarRefNode*>(right.get())) {
                    throw std::runtime_error("XOR requires boolean operands at line " + std::to_string(currentToken.line));
                }
            }
            if (dynamic_cast<StrLiteral*>(left.get()) || dynamic_cast<StrLiteral*>(right.get()) ||
                dynamic_cast<VarRefNode*>(left.get()) || dynamic_cast<VarRefNode*>(right.get())) {    //   needs to be corrected
                left = std::make_unique<ConcatNode>(std::move(left), std::move(right));
            } else {
                op = BinaryOp::ADD;
                left = std::make_unique<BinaryOpNode>(op, std::move(left), std::move(right));
            }
        } else
        if (currentToken.type != Token::SignedIntLiteral){
            switch (currentToken.type) {
                case Token::Plus: op = BinaryOp::ADD; break;
                case Token::Minus: op = BinaryOp::SUBTRACT; break;
                case Token::Star: op = BinaryOp::MULTIPLY; break;
                case Token::Slash: op = BinaryOp::DIVIDE; break;
                case Token::EqualEqual: op = BinaryOp::EQUAL; break;
                case Token::LessEqual: op = BinaryOp::LESS_EQUAL; break;
                case Token::NotEqual: op = BinaryOp::NOT_EQUAL; break;
                case Token::Greater: op = BinaryOp::GREATER; break;
                case Token::GreaterEqual: op = BinaryOp::GREATER_EQUAL; break;
                case Token::Less: op = BinaryOp::LESS; break;
                case Token::And: op = BinaryOp::AND; break;
                case Token::Or: op = BinaryOp::OR; break;
                case Token::Modulo: op = BinaryOp::MODULO; break;
                case Token::Xor: op = BinaryOp::XOR; break; 
                // case Token::IntLiteral: BinaryOp::ADD; break;
                default: throw std::runtime_error("Unknown binary operator");
            }
            advance(); // consume operator
            auto right = parsePrimary();

            if(op == BinaryOp::ADD && currentToken.type == Token::StrLiteral){
                if (dynamic_cast<StrLiteral*>(left.get()) || dynamic_cast<StrLiteral*>(right.get()) ||
                dynamic_cast<VarRefNode*>(left.get()) || dynamic_cast<VarRefNode*>(right.get())) {    //   needs to be corrected
                left = std::make_unique<ConcatNode>(std::move(left), std::move(right));
                } else {
                    // error : NOT-string + string -- semantics
                }
            }else 
            
            left = std::make_unique<BinaryOpNode>(op, std::move(left), std::move(right));
        }else{
            auto right = std::make_unique<IntLiteral>(std::stoi(currentToken.lexeme));
            left = std::make_unique<BinaryOpNode>(BinaryOp::ADD, std::move(left), std::move(right));
            advance(); // consume operator

            // op = BinaryOp::ADD;
            // auto right = std::make_unique<IntLiteral>(std::stoi(currentToken.lexeme));
            // advance(); // consume the int literal
            // left = std::make_unique<BinaryOpNode>(op, std::move(left), std::move(right));
        }
    }
    if (currentToken.type == Token::PlusPlus || currentToken.type == Token::MinusMinus) {
        UnaryOp op = (currentToken.type == Token::PlusPlus) ? UnaryOp::INCREMENT : UnaryOp::DECREMENT;
        advance(); // Consume '++' or '--'
        if (!dynamic_cast<VarRefNode*>(left.get()) && !dynamic_cast<BinaryOpNode*>(left.get())) {
            throw std::runtime_error("Increment/decrement can only be applied to variables or array elements");
        }
        if (auto* binOp = dynamic_cast<BinaryOpNode*>(left.get())) {
            if (binOp->op != BinaryOp::INDEX) {
                throw std::runtime_error("Increment/decrement can only be applied to array elements with index");
            }
        }
        left = std::make_unique<UnaryOpNode>(op, std::move(left));
    }

    return parseTernary(std::move(left));
// }
}

std::unique_ptr<ASTNode> Parser::parsePrimary() {
    if (currentToken.type == Token::LeftParen) {
        advance(); // consume '('
        auto expr = parseExpression();
        if (currentToken.type != Token::RightParen) {
            throw std::runtime_error("Expected ')' at line " + std::to_string(currentToken.line));
        }
        advance(); // consume ')'
        return expr;
    } else if (currentToken.type == Token::IntLiteral || currentToken.type == Token::SignedIntLiteral) {
        auto node = std::make_unique<IntLiteral>(std::stoi(currentToken.lexeme));
        advance();
        return node;
    } else if (currentToken.type == Token::StrLiteral) {
        auto node = std::make_unique<StrLiteral>(currentToken.lexeme);
        advance();
        return node;
    } else if (currentToken.type == Token::BoolLiteral) {
        auto node = std::make_unique<BoolLiteral>(currentToken.lexeme == "true");
        advance();
        return node;
    } else if (currentToken.type == Token::FloatLiteral) {
        auto node = std::make_unique<FloatLiteral>(std::stof(currentToken.lexeme));
        advance();
        return node;
    } else if (currentToken.type == Token::CharLiteral) {
        auto node = std::make_unique<CharLiteral>(currentToken.lexeme[0]);
        advance();
        return node;
    } else if (currentToken.type == Token::Ident) {
        std::string name = currentToken.lexeme;
        advance();
        auto varRef = std::make_unique<VarRefNode>(name);
        // Support arr[i] syntax
        if (currentToken.type == Token::LeftBracket) {
            advance(); // Consume '['
            auto index = parseExpression(); // Parse index expression (e.g., i or 5)
            if (!index) {
                throw std::runtime_error("Expected index expression in array access at line " + std::to_string(currentToken.line));
            }
            if (!dynamic_cast<IntLiteral*>(index.get()) && !dynamic_cast<VarRefNode*>(index.get())) {
                throw std::runtime_error("Array index must be an integer or identifier at line " + std::to_string(currentToken.line));
            }
            if (currentToken.type != Token::RightBracket) {
                throw std::runtime_error("Expected ']' after array index at line " + std::to_string(currentToken.line));
            }
            advance(); // Consume ']'
            return std::make_unique<BinaryOpNode>(BinaryOp::INDEX, std::move(varRef), std::move(index));
        }
        // NEW: Support for method calls (e.g., e.toString()) using BinaryOpNode
        if (currentToken.type == Token::Dot) {
            advance(); // Consume '.'
            if (currentToken.type != Token::Ident) {
                throw std::runtime_error("Expected method name after '.'");
            }
            std::string methodName = currentToken.lexeme; // e.g., "toString"
            advance();
            if (currentToken.type != Token::LeftParen) {
                throw std::runtime_error("Expected '(' after method name");
            }
            advance(); // Consume '('
            if (currentToken.type != Token::RightParen) {
                throw std::runtime_error("Expected ')' after method call");
            }
            advance(); // Consume ')'
            auto methodNode = std::make_unique<StrLiteral>(methodName);
            return std::make_unique<BinaryOpNode>(BinaryOp::METHOD_CALL, std::move(varRef), std::move(methodNode));
        }
        // END NEW
        return varRef;
    } else if (currentToken.type == Token::LeftBracket) { // NEW: Array literal
        advance(); // Consume '['
        std::vector<std::unique_ptr<ASTNode>> elements;
        if (currentToken.type != Token::RightBracket) {
            do {
                auto expr = parseExpression();
                if (!expr || (!dynamic_cast<IntLiteral*>(expr.get()) && !dynamic_cast<VarRefNode*>(expr.get()) &&
                !dynamic_cast<StrLiteral*>(expr.get()) && !dynamic_cast<BoolLiteral*>(expr.get()) &&
                !dynamic_cast<CharLiteral*>(expr.get()))) {
                    throw std::runtime_error("Array elements must be integers or identifiers");
                }
                elements.push_back(std::move(expr));
                if (currentToken.type == Token::Comma) {
                    advance(); // Consume ','
                } else {
                    break;
                }
            } while (currentToken.type != Token::RightBracket);
        }
        if (currentToken.type != Token::RightBracket) {
            throw std::runtime_error("Expected ']' after array literal");
        }
        advance(); // Consume ']'
        return std::make_unique<ArrayLiteralNode>(std::move(elements));
    } else if (currentToken.type == Token::Concat) {
        advance();
        if (currentToken.type != Token::LeftParen) {
            throw std::runtime_error("Expected '(' after 'concat'");
        }
        advance();
        auto left = parseExpression();
        if (!left) {
            throw std::runtime_error("Expected first argument in concat");
        }
        if (currentToken.type != Token::Comma) {
            throw std::runtime_error("Expected ',' after first argument in concat");
        }
        advance();
        auto right = parseExpression();
        if (!right) {
            throw std::runtime_error("Expected second argument in concat");
        }
        if (currentToken.type != Token::RightParen) {
            throw std::runtime_error("Expected ')' after concat arguments");
        }
        advance();
        return std::make_unique<ConcatNode>(std::move(left), std::move(right));
    } else if (currentToken.type == Token::Abs) {
            advance(); // Consume 'abs'
        if (currentToken.type != Token::LeftParen) {
            throw std::runtime_error("Expected '(' after 'abs'");
        }
        advance(); // Consume '('
        auto expr = parseExpression();
        if (!expr) {
            throw std::runtime_error("Expected expression in abs");
        }
        if (!dynamic_cast<IntLiteral*>(expr.get()) && !dynamic_cast<VarRefNode*>(expr.get())) {
            throw std::runtime_error("abs argument must be an integer literal or identifier");
        }
        if (currentToken.type != Token::RightParen) {
            throw std::runtime_error("Expected ')' after abs argument");
        }
        advance(); // Consume ')'
        return std::make_unique<BinaryOpNode>(BinaryOp::ABS, std::move(expr), nullptr);
    } else if (currentToken.type == Token::Pow) { // New
        advance();
        if (currentToken.type != Token::LeftParen) {
            throw std::runtime_error("Expected '(' after 'pow' at line " + std::to_string(currentToken.line));
        }
        advance();
        auto base = parseExpression();
        if (!base) {
            throw std::runtime_error("Expected base expression in pow at line " + std::to_string(currentToken.line));
        }
        if (!dynamic_cast<IntLiteral*>(base.get()) && !dynamic_cast<VarRefNode*>(base.get())) {
            throw std::runtime_error("pow base must be an integer literal or identifier at line " + std::to_string(currentToken.line));
        }
        if (currentToken.type != Token::Comma) {
            throw std::runtime_error("Expected ',' after pow base at line " + std::to_string(currentToken.line));
        }
        advance();
        auto exp = parseExpression();
        if (!exp) {
            throw std::runtime_error("Expected exponent expression in pow at line " + std::to_string(currentToken.line));
        }
        if (!dynamic_cast<IntLiteral*>(exp.get()) && !dynamic_cast<VarRefNode*>(exp.get())) {
            throw std::runtime_error("pow exponent must be an integer literal or identifier at line " + std::to_string(currentToken.line));
        }
        if (currentToken.type != Token::RightParen) {
            throw std::runtime_error("Expected ')' after pow arguments at line " + std::to_string(currentToken.line));
        }
        advance();
        return std::make_unique<BinaryOpNode>(BinaryOp::POW, std::move(base), std::move(exp));
    } else if (currentToken.type == Token::Length || currentToken.type == Token::Min || currentToken.type == Token::Max) { // NEW: length, min, max
        UnaryOp op;
        std::string opName;
        if (currentToken.type == Token::Length) {
            op = UnaryOp::LENGTH;
            opName = "length";
        } else if (currentToken.type == Token::Min) {
            op = UnaryOp::MIN;
            opName = "min";
        } else {
            op = UnaryOp::MAX;
            opName = "max";
        }
        advance(); // Consume 'length', 'min', or 'max'
        if (currentToken.type != Token::LeftParen) {
            throw std::runtime_error("Expected '(' after '" + opName + "' at line " + std::to_string(currentToken.line));
        }
        advance(); // Consume '('
        auto operand = parseExpression();
        if (!operand) {
            throw std::runtime_error("Expected array expression in " + opName + " at line " + std::to_string(currentToken.line));
        }
        if (!dynamic_cast<VarRefNode*>(operand.get()) && !dynamic_cast<ArrayLiteralNode*>(operand.get())) {
            throw std::runtime_error(opName + " argument must be an array or identifier at line " + std::to_string(currentToken.line));
        }
        if (currentToken.type != Token::RightParen) {
            throw std::runtime_error("Expected ')' after " + opName + " argument at line " + std::to_string(currentToken.line));
        }
        advance(); // Consume ')'
        return std::make_unique<UnaryOpNode>(op, std::move(operand));
    } else if (currentToken.type == Token::Index) { // NEW: index
        advance(); // Consume 'index'
        if (currentToken.type != Token::LeftParen) {
            throw std::runtime_error("Expected '(' after 'index' at line " + std::to_string(currentToken.line));
        }
        advance(); // Consume '('
        auto arr = parseExpression();
        if (!arr) {
            throw std::runtime_error("Expected array expression in index at line " + std::to_string(currentToken.line));
        }
        if (!dynamic_cast<VarRefNode*>(arr.get()) && !dynamic_cast<ArrayLiteralNode*>(arr.get())) {
            throw std::runtime_error("index first argument must be an array or identifier at line " + std::to_string(currentToken.line));
        }
        if (currentToken.type != Token::Comma) {
            throw std::runtime_error("Expected ',' after index array at line " + std::to_string(currentToken.line));
        }
        advance(); // Consume ','
        auto idx = parseExpression();
        if (!idx) {
            throw std::runtime_error("Expected index expression in index at line " + std::to_string(currentToken.line));
        }
        if (!dynamic_cast<IntLiteral*>(idx.get()) && !dynamic_cast<VarRefNode*>(idx.get())) {
            throw std::runtime_error("index second argument must be an integer or identifier at line " + std::to_string(currentToken.line));
        }
        if (currentToken.type != Token::RightParen) {
            throw std::runtime_error("Expected ')' after index arguments at line " + std::to_string(currentToken.line));
        }
        advance(); // Consume ')'
        return std::make_unique<BinaryOpNode>(BinaryOp::INDEX, std::move(arr), std::move(idx));
    } else if (currentToken.type == Token::Multiply || currentToken.type == Token::Add ||
               currentToken.type == Token::Subtract || currentToken.type == Token::Divide) { // NEW: array operations
        BinaryOp op;
        std::string opName;
        if (currentToken.type == Token::Multiply) {
            op = BinaryOp::MULTIPLY_ARRAY;
            opName = "multiply";
        } else if (currentToken.type == Token::Add) {
            op = BinaryOp::ADD_ARRAY;
            opName = "add";
        } else if (currentToken.type == Token::Subtract) {
            op = BinaryOp::SUBTRACT_ARRAY;
            opName = "subtract";
        } else {
            op = BinaryOp::DIVIDE_ARRAY;
            opName = "divide";
        }
        advance(); // Consume 'multiply', 'add', 'subtract', or 'divide'
        if (currentToken.type != Token::LeftParen) {
            throw std::runtime_error("Expected '(' after '" + opName + "' at line " + std::to_string(currentToken.line));
        }
        advance(); // Consume '('
        auto arr1 = parseExpression();
        if (!arr1) {
            throw std::runtime_error("Expected first array in " + opName + " at line " + std::to_string(currentToken.line));
        }
        if (!dynamic_cast<VarRefNode*>(arr1.get()) && !dynamic_cast<ArrayLiteralNode*>(arr1.get())) {
            throw std::runtime_error(opName + " first argument must be an array or identifier at line " + std::to_string(currentToken.line));
        }
        if (currentToken.type != Token::Comma) {
            throw std::runtime_error("Expected ',' after first array in " + opName + " at line " + std::to_string(currentToken.line));
        }
        advance(); // Consume ','
        auto arr2 = parseExpression();
        if (!arr2) {
            throw std::runtime_error("Expected second array in " + opName + " at line " + std::to_string(currentToken.line));
        }
        if (!dynamic_cast<VarRefNode*>(arr2.get()) && !dynamic_cast<ArrayLiteralNode*>(arr2.get())) {
            throw std::runtime_error(opName + " second argument must be an array or identifier at line " + std::to_string(currentToken.line));
        }
        if (currentToken.type != Token::RightParen) {
            throw std::runtime_error("Expected ')' after " + opName + " arguments at line " + std::to_string(currentToken.line));
        }
        advance(); // Consume ')'
        return std::make_unique<BinaryOpNode>(op, std::move(arr1), std::move(arr2));
    }else {
        throw std::runtime_error("Expected primary expression");
    }
}

std::unique_ptr<ASTNode> Parser::parseVarDeclMultiVariable(VarType type, std::string name) {
    int counter = 1; // might not be neccesary
    std::vector<std::string> IdentNames;
    IdentNames.push_back(name);
    while(true){
        advance();
        if (currentToken.type != Token::Ident) {
            throw std::runtime_error("Expected identifier after Comma");
        }
        name = currentToken.lexeme;
        IdentNames.push_back(name);
        advance();
        if(currentToken.type == Token::Equal) break;
        else if(currentToken.type != Token::Comma) {
            throw std::runtime_error("Expected Token::Equal or Token::Comma after Identifier");
        }
    }
    // now the current token is = 
    advance();
    std::unique_ptr<ASTNode> value;
    if (type == VarType::INT && currentToken.type == Token::IntLiteral) {
        value = std::make_unique<IntLiteral>(std::stoi(currentToken.lexeme));
        advance();
        if(currentToken.type == Token::Comma) return parseVarDeclMultiBoth(type, std::move(value), IdentNames);
    } 
    else if (type == VarType::STRING && currentToken.type == Token::StrLiteral) {
        value = std::make_unique<StrLiteral>(currentToken.lexeme);
        advance();
        if(currentToken.type == Token::Comma) return parseVarDeclMultiBoth(type, std::move(value), IdentNames);
    } 
    else if (type == VarType::ARRAY && currentToken.type == Token::LeftBracket) { // NEW: Array multi-variable
        value = parsePrimary(); // Parse array literal
        if (!dynamic_cast<ArrayLiteralNode*>(value.get())) {
            throw std::runtime_error("Array initializer must be an array literal");
        }
        advance();
        if (currentToken.type == Token::Comma) return parseVarDeclMultiBoth(type, std::move(value), IdentNames);
    }
    else if (type == VarType::FLOAT && (currentToken.type == Token::FloatLiteral || currentToken.type == Token::IntLiteral)) {
        value = std::make_unique<FloatLiteral>(std::stof(currentToken.lexeme));
        advance();
        if(currentToken.type == Token::Comma) return parseVarDeclMultiBoth(type, std::move(value), IdentNames);
    }
    else if (type == VarType::BOOL && currentToken.type == Token::BoolLiteral) {
        value = std::make_unique<BoolLiteral>(currentToken.lexeme == "true");
        advance();
        if(currentToken.type == Token::Comma) return parseVarDeclMultiBoth(type, std::move(value), IdentNames);
    }
    else if (type == VarType::CHAR && currentToken.type == Token::CharLiteral) {
        value = std::make_unique<CharLiteral>(currentToken.lexeme[0]);
        advance();
        if(currentToken.type == Token::Comma) return parseVarDeclMultiBoth(type, std::move(value), IdentNames);
    }
    else {
        throw std::runtime_error("Type mismatch in variable declaration(2)");
    }
    // if (currentToken.type != Token::Semicolon) {
    //     throw std::runtime_error("Expected ';' after variable declaration");
    // }
    // advance(); // Consume ;
    std::vector<std::unique_ptr<VarDeclNode>> declarations;
    size_t i = 0;

    while (i < IdentNames.size()) {
        std::unique_ptr<ASTNode> valueCopy; // New value for each VarDeclNode
        if (auto* intLit = dynamic_cast<IntLiteral*>(value.get())) {
            valueCopy = std::make_unique<IntLiteral>(intLit->value); // Deep copy for int
        } else if (auto* strLit = dynamic_cast<StrLiteral*>(value.get())) {
            valueCopy = std::make_unique<StrLiteral>(strLit->value); // Deep copy for string
        } else if (auto* arrLit = dynamic_cast<ArrayLiteralNode*>(value.get())) { // NEW: Array copy
            std::vector<std::unique_ptr<ASTNode>> elementsCopy;
            for (const auto& elem : arrLit->elements) {
                if (auto* intLit = dynamic_cast<IntLiteral*>(elem.get())) {
                    elementsCopy.push_back(std::make_unique<IntLiteral>(intLit->value));
                } else {
                    throw std::runtime_error("Array elements must be integers");
                }
            }
            valueCopy = std::make_unique<ArrayLiteralNode>(std::move(elementsCopy));
        } else if (auto* floatLit = dynamic_cast<FloatLiteral*>(value.get())) {
            valueCopy = std::make_unique<FloatLiteral>(floatLit->value);
        } else if (auto* boolLit = dynamic_cast<BoolLiteral*>(value.get())) {
            valueCopy = std::make_unique<BoolLiteral>(boolLit->value);
        } else if (auto* charLit = dynamic_cast<CharLiteral*>(value.get())) {
            valueCopy = std::make_unique<CharLiteral>(charLit->value);
        } else {
            throw std::runtime_error("Unsupported value type in declaration");
        }
        declarations.emplace_back(std::make_unique<VarDeclNode>(type, IdentNames[i], std::move(valueCopy)));
        i++;
    }
    
    if (currentToken.type != Token::Semicolon) {
        throw std::runtime_error("Expected ';' after variable declaration");
    }
    advance(); // Consume ;
return std::make_unique<MultiVarDeclNode>(std::move(declarations));
}

std::unique_ptr<ASTNode> Parser::parseVarDeclMultiBoth(VarType type, std::unique_ptr<ASTNode> value, std::vector<std::string> IdentNames) {
    std::vector<std::unique_ptr<VarDeclNode>> declarations;
    declarations.emplace_back(std::make_unique<VarDeclNode>(type, IdentNames[0], std::move(value)));
    int i = 1;
    while (i < IdentNames.size()) {
        if (currentToken.type != Token::Comma) {
            throw std::runtime_error("Type mismatch in variable declaration(3)");
        }
        advance();
        if (type == VarType::INT && currentToken.type == Token::IntLiteral) {
            value = std::make_unique<IntLiteral>(std::stoi(currentToken.lexeme));
        } else if (type == VarType::STRING && currentToken.type == Token::StrLiteral) {
            value = std::make_unique<StrLiteral>(currentToken.lexeme);
            
        } else if (type == VarType::ARRAY && currentToken.type == Token::LeftBracket) { // NEW: Array literal
            value = parsePrimary();
            if (!dynamic_cast<ArrayLiteralNode*>(value.get())) {
                throw std::runtime_error("Array initializer must be an array literal");
            }
        } else if (type == VarType::FLOAT && (currentToken.type == Token::FloatLiteral || currentToken.type == Token::IntLiteral)) {
            value = std::make_unique<FloatLiteral>(std::stof(currentToken.lexeme));
        } else if (type == VarType::BOOL && currentToken.type == Token::BoolLiteral) {
            value = std::make_unique<BoolLiteral>(currentToken.lexeme == "true");
        } else if (type == VarType::CHAR && currentToken.type == Token::CharLiteral) {
            value = std::make_unique<CharLiteral>(currentToken.lexeme[0]);
        } else {
            throw std::runtime_error("Type mismatch in variable declaration(4)");
        }
        declarations.emplace_back(std::make_unique<VarDeclNode>(type, IdentNames[i], std::move(value)));
        advance();
        i++;
    }
    if (currentToken.type != Token::Semicolon) {
        throw std::runtime_error("Expected ';' after variable declaration");
    }
    advance(); // Consume ;
    return std::make_unique<MultiVarDeclNode>(std::move(declarations));
}

std::unique_ptr<ASTNode> Parser::parseAssignment() {
    std::string name = currentToken.lexeme;
    auto tempType = currentToken.type;
    advance(); // Consume ident
    
    // Handle array indexing: arr[i]
    std::unique_ptr<ASTNode> left = std::make_unique<VarRefNode>(name);
    if (currentToken.type == Token::LeftBracket) {
        advance(); // Consume '['
        auto index = parseExpression();
        if (!index) {
            throw std::runtime_error("Expected index expression in array access at line " + std::to_string(currentToken.line));
        }
        if (!dynamic_cast<IntLiteral*>(index.get()) && !dynamic_cast<VarRefNode*>(index.get()) &&
        !dynamic_cast<StrLiteral*>(index.get()) && !dynamic_cast<BoolLiteral*>(index.get()) &&
        !dynamic_cast<CharLiteral*>(index.get())) {
            throw std::runtime_error("Array index must be an integer or identifier at line " + std::to_string(currentToken.line));
        }
        if (currentToken.type != Token::RightBracket) {
            throw std::runtime_error("Expected ']' after array index at line " + std::to_string(currentToken.line));
        }
        advance(); // Consume ']'
        left = std::make_unique<BinaryOpNode>(BinaryOp::INDEX, std::move(left), std::move(index));
    }

    // Handle ++ or --
    if (currentToken.type == Token::PlusPlus || currentToken.type == Token::MinusMinus) {
        UnaryOp op = (currentToken.type == Token::PlusPlus) ? UnaryOp::INCREMENT : UnaryOp::DECREMENT;
        advance(); // Consume '++' or '--'
        return std::make_unique<UnaryOpNode>(op, std::move(left));
    }

    BinaryOp compoundOp;
    bool isCompound = false;
    
    // Handle =, +=, -=, *=, /=
    if (currentToken.type == Token::Equal) {
        advance();
    } else if (currentToken.type == Token::PlusEqual) {
        compoundOp = BinaryOp::ADD;
        isCompound = true;
        advance();
    } else if (currentToken.type == Token::MinusEqual) {
        compoundOp = BinaryOp::SUBTRACT;
        isCompound = true;
        advance();
    } else if (currentToken.type == Token::StarEqual) {
        compoundOp = BinaryOp::MULTIPLY;
        isCompound = true;
        advance();
    } else if (currentToken.type == Token::SlashEqual) {
        compoundOp = BinaryOp::DIVIDE;
        isCompound = true;
        advance();
    } else if (currentToken.type == Token::ModuloEqual) { // NEW: Added %= support
        compoundOp = BinaryOp::MODULO;
        isCompound = true;
        advance();
    } else {
        throw std::runtime_error("Expected '=' or compound assignment operator");
    }
   
    auto value = parseExpression(); // new

    // if (currentToken.type != Token::Semicolon) {
    //     throw std::runtime_error("Expected ';' after assignment");
    // }
    // advance(); // Consume ;

    if (isCompound) {
        if (!dynamic_cast<IntLiteral*>(value.get()) && 
            !dynamic_cast<FloatLiteral*>(value.get()) && 
            !dynamic_cast<VarRefNode*>(value.get())) {
            throw std::runtime_error("Divide-equal (/=) expression must be an int or float literal or variable at line " + 
                                    std::to_string(currentToken.line));
        }
        return std::make_unique<CompoundAssignNode>(name, compoundOp, std::move(value));
    } else {
        return std::make_unique<AssignNode>(name, std::move(value));
    }
}

std::unique_ptr<ASTNode> Parser::parseIfStatement() {
    advance(); // Consume 'if'

    if (currentToken.type != Token::LeftParen) {
        throw std::runtime_error("Expected '(' after 'if'");
    }
    advance(); // Consume '('

    auto condition = parseExpression(); // Parse the condition (e.g., "true", "a > b")

    if (currentToken.type != Token::RightParen) {
        throw std::runtime_error("Expected ')' after condition");
    }
    advance(); // Consume ')'

    if (currentToken.type != Token::LeftBrace) {
        throw std::runtime_error("Expected '{' after if condition");
    }
    auto thenBlock = parseBlock(); // Parse the "then" block

    std::unique_ptr<ASTNode> elseBlock = nullptr;
    if (currentToken.type == Token::Else) {
        advance(); // Consume 'else'
        if (currentToken.type == Token::If) {
            elseBlock = parseIfStatement(); // Recurse for "else if"
        } else if (currentToken.type == Token::LeftBrace) {
            elseBlock = parseBlock(); // Plain "else" block
        } else {
            throw std::runtime_error("Expected 'if' or '{' after 'else'");
        }
    }

    return std::make_unique<IfElseNode>(std::move(condition), std::move(thenBlock), std::move(elseBlock));
}

std::unique_ptr<BlockNode> Parser::parseBlock() {
    if (currentToken.type != Token::LeftBrace) {
        throw std::runtime_error("Expected '{' to start block");
    }
    advance(); // consume '{'

    auto block = std::make_unique<BlockNode>();

    while (currentToken.type != Token::RightBrace) {
        block->statements.push_back(parseStatement());
    }

    advance(); // consume '}'
    return block;
}

std::unique_ptr<ASTNode> Parser::parseLoop() {
    bool isForeach = (currentToken.type == Token::Foreach);
    advance(); // Consume 'for' or 'foreach'
    if (currentToken.type != Token::LeftParen) throw std::runtime_error("Expected '(' after loop keyword");
    advance(); // Consume '('

    if (isForeach) {
        if (currentToken.type != Token::Ident) throw std::runtime_error("Expected identifier in foreach");
        std::string varName = currentToken.lexeme;
        advance(); // Consume varName
        if (currentToken.type != Token::In) throw std::runtime_error("Expected 'in' in foreach");
        advance(); // Consume 'in'
        auto collection = parseExpression(); // Parse array expression
        if (!collection) throw std::runtime_error("Expected array expression after 'in'");
        if (currentToken.type != Token::RightParen) throw std::runtime_error("Expected ')' after foreach");
        advance(); // Consume ')'
        auto body = parseBlock();
        return std::make_unique<LoopNode>(varName, std::move(collection), std::move(body));
    } else {
        std::unique_ptr<ASTNode> init = nullptr;
        if (currentToken.type == Token::Int) {
            init = parseVarDecl(); // e.g., int i = 0
        } else if (currentToken.type == Token::Ident) {
            init = parseAssignment(); // e.g., i = 0
        } 
        // else if (currentToken.type != Token::Semicolon) {
        //     throw std::runtime_error("Expected declaration, assignment, or ';' in for init");
        // }
        // if (currentToken.type != Token::Semicolon) throw std::runtime_error("Expected ';' after init");
        // advance(); // Consume ';'
        auto condition = currentToken.type == Token::Semicolon ? nullptr : parseExpression();
        if (currentToken.type != Token::Semicolon) throw std::runtime_error("Expected ';' after condition");
        advance(); // Consume ';'
        auto update = currentToken.type == Token::RightParen ? nullptr : parseAssignment(); // expression changed to assignment
        if (currentToken.type != Token::RightParen) throw std::runtime_error("Expected ')' after update");
        advance(); // Consume ')'
        auto body = parseBlock();
        return std::make_unique<LoopNode>(std::move(init), std::move(condition), std::move(update), std::move(body));
    }
}

std::unique_ptr<ASTNode> Parser::parseTryCatch() {
    advance(); // Consume 'try'
    if (currentToken.type != Token::LeftBrace) {
        throw std::runtime_error("Expected '{' after 'try'");
    }
    auto tryBlock = parseBlock();
    if (currentToken.type != Token::Catch) {
        throw std::runtime_error("Expected 'catch' after try block");
    }
    advance(); // Consume 'catch'
    if (currentToken.type != Token::LeftParen) {
        throw std::runtime_error("Expected '(' after 'catch'");
    }
    advance(); // Consume '('
    if (currentToken.type != Token::Error) {
        throw std::runtime_error("Expected 'Error' in catch");
    }
    advance(); // Consume 'Error'
    if (currentToken.type != Token::Ident) {
        throw std::runtime_error("Expected identifier after 'Error'");
    }
    std::string errorVar = currentToken.lexeme; // e.g., "e"
    advance(); // Consume identifier 'e'
    if (currentToken.type != Token::RightParen) {
        throw std::runtime_error("Expected ')' after catch variable");
    }
    advance(); // Consume ')'
    if (currentToken.type != Token::LeftBrace) {
        throw std::runtime_error("Expected '{' after catch variable");
    }
    auto catchBlock = parseBlock();
    // Insert VarDeclNode for e
    catchBlock->statements.insert(
        catchBlock->statements.begin(),
        std::make_unique<VarDeclNode>(VarType::ERROR, errorVar, nullptr)
    );
    return std::make_unique<TryCatchNode>(std::move(tryBlock), std::move(catchBlock), errorVar);
}

std::unique_ptr<ASTNode> Parser::parseTernary(std::unique_ptr<ASTNode> condition) {
    if (currentToken.type == Token::Question) {
        advance(); // Consume '?'
        auto trueBranch = parseExpression(); // Parse true branch (e.g., y)
        if (currentToken.type != Token::Colon) {
            throw std::runtime_error("Expected ':' in ternary expression at line " + std::to_string(currentToken.line));
        }
        advance(); // Consume ':'
        auto falseBranch = parseExpression(); // Parse false branch (e.g., w)
        // Inline boolean operator check
        auto* binOp = dynamic_cast<BinaryOpNode*>(condition.get());
        if (!binOp || !(
            binOp->op == BinaryOp::EQUAL ||
            binOp->op == BinaryOp::NOT_EQUAL ||
            binOp->op == BinaryOp::LESS ||
            binOp->op == BinaryOp::LESS_EQUAL ||
            binOp->op == BinaryOp::GREATER ||
            binOp->op == BinaryOp::GREATER_EQUAL ||
            binOp->op == BinaryOp::AND ||
            binOp->op == BinaryOp::OR
        )) {
            throw std::runtime_error("Ternary condition must be a boolean expression at line " + std::to_string(currentToken.line));
        }
        return std::make_unique<TernaryExprNode>(std::move(condition), std::move(trueBranch), std::move(falseBranch));
    }
    return condition;
}

std::unique_ptr<ASTNode> Parser::parseMatch() {
    advance(); // Consume 'match'
    auto expr = parseExpression(); // Parse match expression (e.g., x)
    if (currentToken.type != Token::LeftBrace) {
        throw std::runtime_error("Expected '{' after match expression at line " + std::to_string(currentToken.line));
    }
    advance(); // Consume '{'

    std::vector<std::unique_ptr<MatchCaseNode>> cases;
    bool hasDefault = false;

    while (currentToken.type != Token::RightBrace && currentToken.type != Token::Eof) {
        std::unique_ptr<ASTNode> value;
        if (currentToken.type == Token::Underscore) {
            if (hasDefault) {
                throw std::runtime_error("Multiple default cases in match at line " + std::to_string(currentToken.line));
            }
            hasDefault = true;
            value = nullptr; // Default case has no value
            advance(); // Consume '_'
        } else {
            value = parseExpression(); // Parse case value (e.g., 0, 1)
        }

        if (currentToken.type != Token::Arrow) {
            throw std::runtime_error("Expected '->' in match case at line " + std::to_string(currentToken.line));
        }
        advance(); // Consume '->'

        std::unique_ptr<ASTNode> body;
        if (currentToken.type == Token::LeftBrace) {
            // Parse block for multiple statements
            body = parseBlock();
        } else {
            // Parse single statement (e.g., print("zero"))
            body = parseStatement();
        }

        cases.push_back(std::make_unique<MatchCaseNode>(std::move(value), std::move(body)));

        // Handle optional comma, but only if not at the end of the match
        if (currentToken.type == Token::Comma && peekToken.type != Token::RightBrace) {
            advance(); // Consume ','
        }
    }

    if (currentToken.type != Token::RightBrace) {
        throw std::runtime_error("Expected '}' to close match at line " + std::to_string(currentToken.line));
    }
    advance(); // Consume '}'

    return std::make_unique<MatchNode>(std::move(expr), std::move(cases));
}
