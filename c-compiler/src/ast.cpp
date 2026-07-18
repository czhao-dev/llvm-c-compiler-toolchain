#include "ast.h"

#include <utility>

namespace minic {
namespace {

void printIndent(std::ostream &out, int indent) {
    for (int i = 0; i < indent; ++i) {
        out << "  ";
    }
}

} // namespace

const Type Type::Int = Type(TypeKind::Int);
const Type Type::Float = Type(TypeKind::Float);
const Type Type::Char = Type(TypeKind::Char);
const Type Type::Void = Type(TypeKind::Void);
const Type Type::String = Type(TypeKind::String);

Type Type::namedStruct(std::string name, int pointerDepth, int arrayLength) {
    Type type(TypeKind::Struct, pointerDepth, arrayLength);
    type.aggregateName_ = std::move(name);
    return type;
}

Type Type::namedUnion(std::string name, int pointerDepth, int arrayLength) {
    Type type(TypeKind::Union, pointerDepth, arrayLength);
    type.aggregateName_ = std::move(name);
    return type;
}

std::string typeName(Type type) {
    std::string base;
    switch (type.kind()) {
    case TypeKind::Int: base = "int"; break;
    case TypeKind::Float: base = "float"; break;
    case TypeKind::Char: base = "char"; break;
    case TypeKind::Void: base = "void"; break;
    case TypeKind::String: base = "string"; break;
    case TypeKind::Struct: base = "struct " + type.aggregateName(); break;
    case TypeKind::Union: base = "union " + type.aggregateName(); break;
    }
    std::string result = base + std::string(type.pointerDepth(), '*');
    if (type.isArray()) {
        result += '[' + std::to_string(type.arrayLength()) + ']';
    }
    return result;
}

std::string binaryOpSymbol(BinaryOp op) {
    switch (op) {
    case BinaryOp::Add: return "+";
    case BinaryOp::Sub: return "-";
    case BinaryOp::Mul: return "*";
    case BinaryOp::Div: return "/";
    case BinaryOp::Eq: return "==";
    case BinaryOp::Neq: return "!=";
    case BinaryOp::Lt: return "<";
    case BinaryOp::Gt: return ">";
    case BinaryOp::Leq: return "<=";
    case BinaryOp::Geq: return ">=";
    case BinaryOp::And: return "&&";
    case BinaryOp::BitAnd: return "&";
    case BinaryOp::BitOr: return "|";
    case BinaryOp::BitXor: return "^";
    case BinaryOp::Shl: return "<<";
    case BinaryOp::Shr: return ">>";
    case BinaryOp::Comma: return ",";
    case BinaryOp::Or: return "||";
    }
    return "<unknown op>";
}

std::string unaryOpSymbol(UnaryOp op) {
    switch (op) {
    case UnaryOp::Negate: return "-";
    case UnaryOp::Not: return "!";
    case UnaryOp::AddressOf: return "&";
    case UnaryOp::Deref: return "*";
    case UnaryOp::BitNot: return "~";
    }
    return "<unknown op>";
}

// ---------------------------------------------------------------------------
// Expressions
// ---------------------------------------------------------------------------

IntLitExprNode::IntLitExprNode(SourceLocation location, long long value)
    : ExprNode(std::move(location)), value(value) {}

void IntLitExprNode::print(std::ostream &out, int indent) const {
    printIndent(out, indent);
    out << "IntLit " << value << '\n';
}

FloatLitExprNode::FloatLitExprNode(SourceLocation location, double value)
    : ExprNode(std::move(location)), value(value) {}

void FloatLitExprNode::print(std::ostream &out, int indent) const {
    printIndent(out, indent);
    out << "FloatLit " << value << '\n';
}

CharLitExprNode::CharLitExprNode(SourceLocation location, char value)
    : ExprNode(std::move(location)), value(value) {}

void CharLitExprNode::print(std::ostream &out, int indent) const {
    printIndent(out, indent);
    out << "CharLit '" << value << "'\n";
}

StringLitExprNode::StringLitExprNode(SourceLocation location, std::string value)
    : ExprNode(std::move(location)), value(std::move(value)) {}

void StringLitExprNode::print(std::ostream &out, int indent) const {
    printIndent(out, indent);
    out << "StringLit \"" << value << "\"\n";
}

IdentExprNode::IdentExprNode(SourceLocation location, std::string name)
    : ExprNode(std::move(location)), name(std::move(name)) {}

void IdentExprNode::print(std::ostream &out, int indent) const {
    printIndent(out, indent);
    out << "Ident " << name << '\n';
}

UnaryOpExprNode::UnaryOpExprNode(SourceLocation location, UnaryOp op, ExprPtr operand)
    : ExprNode(std::move(location)), op(op), operand(std::move(operand)) {}

void UnaryOpExprNode::print(std::ostream &out, int indent) const {
    printIndent(out, indent);
    out << "UnaryOp " << unaryOpSymbol(op) << '\n';
    operand->print(out, indent + 1);
}

BinOpExprNode::BinOpExprNode(SourceLocation location, BinaryOp op, ExprPtr lhs, ExprPtr rhs)
    : ExprNode(std::move(location)), op(op), lhs(std::move(lhs)), rhs(std::move(rhs)) {}

void BinOpExprNode::print(std::ostream &out, int indent) const {
    printIndent(out, indent);
    out << "BinOp " << binaryOpSymbol(op) << '\n';
    lhs->print(out, indent + 1);
    rhs->print(out, indent + 1);
}

IndexExprNode::IndexExprNode(SourceLocation location, ExprPtr base, ExprPtr index)
    : ExprNode(std::move(location)), base(std::move(base)), index(std::move(index)) {}

void IndexExprNode::print(std::ostream &out, int indent) const {
    printIndent(out, indent);
    out << "Index\n";
    base->print(out, indent + 1);
    index->print(out, indent + 1);
}

MemberExprNode::MemberExprNode(SourceLocation location, ExprPtr base, std::string field)
    : ExprNode(std::move(location)), base(std::move(base)), field(std::move(field)) {}

void MemberExprNode::print(std::ostream &out, int indent) const {
    printIndent(out, indent);
    out << "Member " << field << '\n';
    base->print(out, indent + 1);
}

CallExprNode::CallExprNode(SourceLocation location, std::string callee, std::vector<ExprPtr> args)
    : ExprNode(std::move(location)), callee(std::move(callee)), args(std::move(args)) {}

void CallExprNode::print(std::ostream &out, int indent) const {
    printIndent(out, indent);
    out << "Call " << callee << '\n';
    for (const auto &arg : args) {
        arg->print(out, indent + 1);
    }
}

TernaryExprNode::TernaryExprNode(SourceLocation location, ExprPtr condition, ExprPtr thenExpr, ExprPtr elseExpr)
    : ExprNode(std::move(location)), condition(std::move(condition)), thenExpr(std::move(thenExpr)),
      elseExpr(std::move(elseExpr)) {}

void TernaryExprNode::print(std::ostream &out, int indent) const {
    printIndent(out, indent);
    out << "Ternary\n";
    condition->print(out, indent + 1);
    thenExpr->print(out, indent + 1);
    elseExpr->print(out, indent + 1);
}

IncDecExprNode::IncDecExprNode(SourceLocation location, ExprPtr target, bool isIncrement, bool isPrefix)
    : ExprNode(std::move(location)), target(std::move(target)), isIncrement(isIncrement), isPrefix(isPrefix) {}

void IncDecExprNode::print(std::ostream &out, int indent) const {
    printIndent(out, indent);
    out << (isPrefix ? "Pre" : "Post") << (isIncrement ? "Inc" : "Dec") << '\n';
    target->print(out, indent + 1);
}

CastExprNode::CastExprNode(SourceLocation location, Type targetType, ExprPtr operand)
    : ExprNode(std::move(location)), targetType(targetType), operand(std::move(operand)) {}

void CastExprNode::print(std::ostream &out, int indent) const {
    printIndent(out, indent);
    out << "Cast <" << typeName(targetType) << ">\n";
    operand->print(out, indent + 1);
}

// ---------------------------------------------------------------------------
// Statements
// ---------------------------------------------------------------------------

BlockStmtNode::BlockStmtNode(SourceLocation location, std::vector<StmtPtr> statements)
    : StmtNode(std::move(location)), statements(std::move(statements)) {}

void BlockStmtNode::print(std::ostream &out, int indent) const {
    printIndent(out, indent);
    out << "Block\n";
    for (const auto &stmt : statements) {
        stmt->print(out, indent + 1);
    }
}

VarDeclStmtNode::VarDeclStmtNode(SourceLocation location, Type type, std::string name, ExprPtr init)
    : StmtNode(std::move(location)), type(type), name(std::move(name)), init(std::move(init)) {}

void VarDeclStmtNode::print(std::ostream &out, int indent) const {
    printIndent(out, indent);
    out << "VarDecl " << typeName(type) << ' ' << name << '\n';
    if (init) {
        init->print(out, indent + 1);
    }
}

AssignStmtNode::AssignStmtNode(SourceLocation location, ExprPtr target, ExprPtr value)
    : StmtNode(std::move(location)), target(std::move(target)), value(std::move(value)) {}

AssignStmtNode::AssignStmtNode(SourceLocation location, ExprPtr target, BinaryOp compoundOp, ExprPtr value)
    : StmtNode(std::move(location)), target(std::move(target)), value(std::move(value)), compoundOp(compoundOp) {}

void AssignStmtNode::print(std::ostream &out, int indent) const {
    printIndent(out, indent);
    out << "Assign";
    if (compoundOp) {
        out << " " << binaryOpSymbol(*compoundOp) << "=";
    }
    out << '\n';
    target->print(out, indent + 1);
    value->print(out, indent + 1);
}

ExprStmtNode::ExprStmtNode(SourceLocation location, ExprPtr expr)
    : StmtNode(std::move(location)), expr(std::move(expr)) {}

void ExprStmtNode::print(std::ostream &out, int indent) const {
    printIndent(out, indent);
    out << "ExprStmt\n";
    expr->print(out, indent + 1);
}

IfStmtNode::IfStmtNode(SourceLocation location, ExprPtr condition,
                       std::unique_ptr<BlockStmtNode> thenBlock,
                       std::unique_ptr<BlockStmtNode> elseBlock)
    : StmtNode(std::move(location)), condition(std::move(condition)),
      thenBlock(std::move(thenBlock)), elseBlock(std::move(elseBlock)) {}

void IfStmtNode::print(std::ostream &out, int indent) const {
    printIndent(out, indent);
    out << "If\n";
    printIndent(out, indent + 1);
    out << "Cond\n";
    condition->print(out, indent + 2);
    printIndent(out, indent + 1);
    out << "Then\n";
    thenBlock->print(out, indent + 2);
    if (elseBlock) {
        printIndent(out, indent + 1);
        out << "Else\n";
        elseBlock->print(out, indent + 2);
    }
}

WhileStmtNode::WhileStmtNode(SourceLocation location, ExprPtr condition, std::unique_ptr<BlockStmtNode> body)
    : StmtNode(std::move(location)), condition(std::move(condition)), body(std::move(body)) {}

void WhileStmtNode::print(std::ostream &out, int indent) const {
    printIndent(out, indent);
    out << "While\n";
    printIndent(out, indent + 1);
    out << "Cond\n";
    condition->print(out, indent + 2);
    printIndent(out, indent + 1);
    out << "Body\n";
    body->print(out, indent + 2);
}

ForStmtNode::ForStmtNode(SourceLocation location, StmtPtr init, ExprPtr condition, StmtPtr update,
                          std::unique_ptr<BlockStmtNode> body)
    : StmtNode(std::move(location)), init(std::move(init)), condition(std::move(condition)),
      update(std::move(update)), body(std::move(body)) {}

void ForStmtNode::print(std::ostream &out, int indent) const {
    printIndent(out, indent);
    out << "For\n";
    if (init) {
        printIndent(out, indent + 1);
        out << "Init\n";
        init->print(out, indent + 2);
    }
    if (condition) {
        printIndent(out, indent + 1);
        out << "Cond\n";
        condition->print(out, indent + 2);
    }
    if (update) {
        printIndent(out, indent + 1);
        out << "Update\n";
        update->print(out, indent + 2);
    }
    printIndent(out, indent + 1);
    out << "Body\n";
    body->print(out, indent + 2);
}

ReturnStmtNode::ReturnStmtNode(SourceLocation location, ExprPtr value)
    : StmtNode(std::move(location)), value(std::move(value)) {}

void ReturnStmtNode::print(std::ostream &out, int indent) const {
    printIndent(out, indent);
    out << "Return\n";
    if (value) {
        value->print(out, indent + 1);
    }
}

BreakStmtNode::BreakStmtNode(SourceLocation location) : StmtNode(std::move(location)) {}

void BreakStmtNode::print(std::ostream &out, int indent) const {
    printIndent(out, indent);
    out << "Break\n";
}

ContinueStmtNode::ContinueStmtNode(SourceLocation location) : StmtNode(std::move(location)) {}

void ContinueStmtNode::print(std::ostream &out, int indent) const {
    printIndent(out, indent);
    out << "Continue\n";
}

DoWhileStmtNode::DoWhileStmtNode(SourceLocation location, std::unique_ptr<BlockStmtNode> body, ExprPtr condition)
    : StmtNode(std::move(location)), body(std::move(body)), condition(std::move(condition)) {}

void DoWhileStmtNode::print(std::ostream &out, int indent) const {
    printIndent(out, indent);
    out << "DoWhile\n";
    printIndent(out, indent + 1);
    out << "Body\n";
    body->print(out, indent + 2);
    printIndent(out, indent + 1);
    out << "Cond\n";
    condition->print(out, indent + 2);
}

CaseLabelStmtNode::CaseLabelStmtNode(SourceLocation location, long long value)
    : StmtNode(std::move(location)), value(value) {}

void CaseLabelStmtNode::print(std::ostream &out, int indent) const {
    printIndent(out, indent);
    out << "Case " << value << '\n';
}

DefaultLabelStmtNode::DefaultLabelStmtNode(SourceLocation location) : StmtNode(std::move(location)) {}

void DefaultLabelStmtNode::print(std::ostream &out, int indent) const {
    printIndent(out, indent);
    out << "Default\n";
}

SwitchStmtNode::SwitchStmtNode(SourceLocation location, ExprPtr value, std::unique_ptr<BlockStmtNode> body)
    : StmtNode(std::move(location)), value(std::move(value)), body(std::move(body)) {}

void SwitchStmtNode::print(std::ostream &out, int indent) const {
    printIndent(out, indent);
    out << "Switch\n";
    value->print(out, indent + 1);
    body->print(out, indent + 1);
}

LabelStmtNode::LabelStmtNode(SourceLocation location, std::string name)
    : StmtNode(std::move(location)), name(std::move(name)) {}

void LabelStmtNode::print(std::ostream &out, int indent) const {
    printIndent(out, indent);
    out << "Label " << name << '\n';
}

GotoStmtNode::GotoStmtNode(SourceLocation location, std::string name)
    : StmtNode(std::move(location)), name(std::move(name)) {}

void GotoStmtNode::print(std::ostream &out, int indent) const {
    printIndent(out, indent);
    out << "Goto " << name << '\n';
}

// ---------------------------------------------------------------------------
// Top-level
// ---------------------------------------------------------------------------

void ParamNode::print(std::ostream &out, int indent) const {
    printIndent(out, indent);
    out << "Param " << typeName(type) << ' ' << name << '\n';
}

void FieldNode::print(std::ostream &out, int indent) const {
    printIndent(out, indent);
    out << "Field " << typeName(type) << ' ' << name << '\n';
}

AggregateDeclNode::AggregateDeclNode(SourceLocation location, std::string name, std::vector<FieldNode> fields,
                                      bool isUnion)
    : location(std::move(location)), name(std::move(name)), fields(std::move(fields)), isUnion(isUnion) {}

void AggregateDeclNode::print(std::ostream &out, int indent) const {
    printIndent(out, indent);
    out << (isUnion ? "Union " : "Struct ") << name << '\n';
    for (const auto &field : fields) {
        field.print(out, indent + 1);
    }
}

EnumDeclNode::EnumDeclNode(SourceLocation location, std::string name, std::vector<EnumeratorNode> enumerators)
    : location(std::move(location)), name(std::move(name)), enumerators(std::move(enumerators)) {}

void EnumDeclNode::print(std::ostream &out, int indent) const {
    printIndent(out, indent);
    out << "Enum " << name << '\n';
    for (const auto &enumerator : enumerators) {
        printIndent(out, indent + 1);
        out << enumerator.name << " = " << enumerator.value << '\n';
    }
}

FuncDefNode::FuncDefNode(SourceLocation location, Type returnType, std::string name,
                          std::vector<ParamNode> params, std::unique_ptr<BlockStmtNode> body)
    : location(std::move(location)), returnType(returnType), name(std::move(name)),
      params(std::move(params)), body(std::move(body)) {}

void FuncDefNode::print(std::ostream &out, int indent) const {
    printIndent(out, indent);
    out << "FuncDef " << name << " -> " << typeName(returnType) << '\n';
    for (const auto &param : params) {
        param.print(out, indent + 1);
    }
    body->print(out, indent + 1);
}

void ProgramNode::print(std::ostream &out, int indent) const {
    printIndent(out, indent);
    out << "Program\n";
    for (const auto &aggregate : aggregates) {
        aggregate->print(out, indent + 1);
    }
    for (const auto &enumDecl : enums) {
        enumDecl->print(out, indent + 1);
    }
    for (const auto &func : functions) {
        func->print(out, indent + 1);
    }
}

} // namespace minic
