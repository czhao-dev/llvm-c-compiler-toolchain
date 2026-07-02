#include "token.h"

namespace cl {

std::string tokenTypeName(TokenType type) {
    switch (type) {
    case TokenType::Identifier: return "Identifier";
    case TokenType::IntLiteral: return "IntLiteral";
    case TokenType::FloatLiteral: return "FloatLiteral";
    case TokenType::StringLiteral: return "StringLiteral";
    case TokenType::CharLiteral: return "CharLiteral";
    case TokenType::If: return "If";
    case TokenType::While: return "While";
    case TokenType::EqualEqual: return "EqualEqual";
    case TokenType::NotEqual: return "NotEqual";
    case TokenType::Less: return "Less";
    case TokenType::Greater: return "Greater";
    case TokenType::LessEqual: return "LessEqual";
    case TokenType::GreaterEqual: return "GreaterEqual";
    case TokenType::LeftParen: return "LeftParen";
    case TokenType::RightParen: return "RightParen";
    case TokenType::LeftBrace: return "LeftBrace";
    case TokenType::RightBrace: return "RightBrace";
    case TokenType::Other: return "Other";
    case TokenType::EndOfFile: return "EndOfFile";
    }
    return "Unknown";
}

} // namespace cl
