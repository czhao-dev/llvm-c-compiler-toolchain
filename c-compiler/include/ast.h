#pragma once

#include "token.h"

#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

namespace minic {

enum class TypeKind {
    Int,
    Float,
    Char,
    Void,
    // Pseudo-type assigned to string literals by the semantic analyzer.
    // MiniC has no first-class string type; string literals are only valid
    // as printf arguments.
    String,
    // A named struct or union (Type::aggregateName() holds the tag name).
    // There is no first-class "enum type": enum constants are resolved to
    // plain Type::Int values, so enum needs no TypeKind of its own.
    Struct,
    Union,
};

// A MiniC type: a base kind, a pointer-indirection depth (0 = not a
// pointer, 1 = T*, 2 = T**, ...), and an optional fixed array length (0 =
// not an array). Kept as a small value type rather than a flat enum so
// pointer and array types compose naturally without a separate "pointee
// type" side table.
//
// `arrayLength > 0` means "array of `arrayLength` elements of type {kind,
// pointerDepth}". Only one array dimension is supported (no arrays of
// arrays) and there is no pointer-to-array type — taking the address of an
// array variable is rejected by sema rather than modeled here.
//
// When `kind` is Struct or Union, `aggregateName` holds the struct/union's
// tag name (e.g. "Point" for `struct Point`); it's empty for every other
// kind. Two aggregate Types are equal only if their tag names match.
class Type {
public:
    Type() : kind_(TypeKind::Int), pointerDepth_(0), arrayLength_(0) {}
    explicit Type(TypeKind kind, int pointerDepth = 0, int arrayLength = 0)
        : kind_(kind), pointerDepth_(pointerDepth), arrayLength_(arrayLength) {}

    TypeKind kind() const { return kind_; }
    int pointerDepth() const { return pointerDepth_; }
    bool isPointer() const { return pointerDepth_ > 0; }
    Type pointerTo() const { return withDepthAndLength(pointerDepth_ + 1, 0); }
    // Caller must check isPointer() first.
    Type pointee() const { return withDepthAndLength(pointerDepth_ - 1, 0); }

    bool isArray() const { return arrayLength_ > 0; }
    int arrayLength() const { return arrayLength_; }
    // This type with the array dimension stripped (same kind/pointerDepth).
    Type elementType() const { return withDepthAndLength(pointerDepth_, 0); }
    // The pointer type an array decays to when used as a value.
    Type decay() const { return withDepthAndLength(pointerDepth_ + 1, 0); }
    // Builds "array of `length`" from this (non-array) type.
    Type arrayOf(int length) const { return withDepthAndLength(pointerDepth_, length); }

    bool isStruct() const { return kind_ == TypeKind::Struct; }
    bool isUnion() const { return kind_ == TypeKind::Union; }
    bool isAggregate() const { return isStruct() || isUnion(); }
    const std::string &aggregateName() const { return aggregateName_; }

    static Type namedStruct(std::string name, int pointerDepth = 0, int arrayLength = 0);
    static Type namedUnion(std::string name, int pointerDepth = 0, int arrayLength = 0);

    friend bool operator==(const Type &a, const Type &b) {
        return a.kind_ == b.kind_ && a.pointerDepth_ == b.pointerDepth_ && a.arrayLength_ == b.arrayLength_ &&
               a.aggregateName_ == b.aggregateName_;
    }
    friend bool operator!=(const Type &a, const Type &b) { return !(a == b); }

    static const Type Int;
    static const Type Float;
    static const Type Char;
    static const Type Void;
    static const Type String;

private:
    Type withDepthAndLength(int pointerDepth, int arrayLength) const {
        Type result(kind_, pointerDepth, arrayLength);
        result.aggregateName_ = aggregateName_;
        return result;
    }

    TypeKind kind_;
    int pointerDepth_;
    int arrayLength_;
    std::string aggregateName_;
};

std::string typeName(Type type);

enum class BinaryOp {
    Add,
    Sub,
    Mul,
    Div,
    Eq,
    Neq,
    Lt,
    Gt,
    Leq,
    Geq,
    And,
    Or,
    BitAnd,
    BitOr,
    BitXor,
    Shl,
    Shr,
    // The comma operator: evaluates lhs for its side effects, discards the
    // result, then evaluates and yields rhs. Only reachable via an
    // explicitly parenthesized "(a, b)" — see Parser::parsePrimary.
    Comma,
};

enum class UnaryOp {
    Negate,
    Not,
    AddressOf,
    Deref,
    BitNot,
};

std::string binaryOpSymbol(BinaryOp op);
std::string unaryOpSymbol(UnaryOp op);

// Base class for every expression and statement node. Provides the source
// location used for diagnostics and a virtual print() used by --emit-ast.
class ASTNode {
public:
    explicit ASTNode(SourceLocation location) : location(std::move(location)) {}
    virtual ~ASTNode() = default;

    virtual void print(std::ostream &out, int indent) const = 0;

    SourceLocation location;
};

class ExprNode : public ASTNode {
public:
    using ASTNode::ASTNode;
};

class StmtNode : public ASTNode {
public:
    using ASTNode::ASTNode;
};

using ExprPtr = std::unique_ptr<ExprNode>;
using StmtPtr = std::unique_ptr<StmtNode>;

// ---------------------------------------------------------------------------
// Expressions
// ---------------------------------------------------------------------------

class IntLitExprNode : public ExprNode {
public:
    IntLitExprNode(SourceLocation location, long long value);
    void print(std::ostream &out, int indent) const override;

    long long value;
};

class FloatLitExprNode : public ExprNode {
public:
    FloatLitExprNode(SourceLocation location, double value);
    void print(std::ostream &out, int indent) const override;

    double value;
};

class CharLitExprNode : public ExprNode {
public:
    CharLitExprNode(SourceLocation location, char value);
    void print(std::ostream &out, int indent) const override;

    char value;
};

class StringLitExprNode : public ExprNode {
public:
    StringLitExprNode(SourceLocation location, std::string value);
    void print(std::ostream &out, int indent) const override;

    std::string value;
};

class IdentExprNode : public ExprNode {
public:
    IdentExprNode(SourceLocation location, std::string name);
    void print(std::ostream &out, int indent) const override;

    std::string name;
};

class UnaryOpExprNode : public ExprNode {
public:
    UnaryOpExprNode(SourceLocation location, UnaryOp op, ExprPtr operand);
    void print(std::ostream &out, int indent) const override;

    UnaryOp op;
    ExprPtr operand;
};

class BinOpExprNode : public ExprNode {
public:
    BinOpExprNode(SourceLocation location, BinaryOp op, ExprPtr lhs, ExprPtr rhs);
    void print(std::ostream &out, int indent) const override;

    BinaryOp op;
    ExprPtr lhs;
    ExprPtr rhs;
};

class IndexExprNode : public ExprNode {
public:
    IndexExprNode(SourceLocation location, ExprPtr base, ExprPtr index);
    void print(std::ostream &out, int indent) const override;

    ExprPtr base;
    ExprPtr index;
};

// `base.field`. `base->field` is desugared by the parser into
// MemberExprNode{UnaryOpExprNode{Deref, base}, field}, so this node only
// ever represents the dot form.
class MemberExprNode : public ExprNode {
public:
    MemberExprNode(SourceLocation location, ExprPtr base, std::string field);
    void print(std::ostream &out, int indent) const override;

    ExprPtr base;
    std::string field;
};

class CallExprNode : public ExprNode {
public:
    CallExprNode(SourceLocation location, std::string callee, std::vector<ExprPtr> args);
    void print(std::ostream &out, int indent) const override;

    std::string callee;
    std::vector<ExprPtr> args;
};

// `condition ? thenExpr : elseExpr`.
class TernaryExprNode : public ExprNode {
public:
    TernaryExprNode(SourceLocation location, ExprPtr condition, ExprPtr thenExpr, ExprPtr elseExpr);
    void print(std::ostream &out, int indent) const override;

    ExprPtr condition;
    ExprPtr thenExpr;
    ExprPtr elseExpr;
};

// `++target`/`--target` (isPrefix) or `target++`/`target--` (!isPrefix).
// A dedicated node rather than a UnaryOp variant because it both reads and
// writes `target` (which must be an lvalue) and the prefix/postfix forms
// differ in which value (new vs. old) the expression itself produces.
class IncDecExprNode : public ExprNode {
public:
    IncDecExprNode(SourceLocation location, ExprPtr target, bool isIncrement, bool isPrefix);
    void print(std::ostream &out, int indent) const override;

    ExprPtr target;
    bool isIncrement;
    bool isPrefix;
};

// ---------------------------------------------------------------------------
// Statements
// ---------------------------------------------------------------------------

class BlockStmtNode : public StmtNode {
public:
    BlockStmtNode(SourceLocation location, std::vector<StmtPtr> statements);
    void print(std::ostream &out, int indent) const override;

    std::vector<StmtPtr> statements;
};

class VarDeclStmtNode : public StmtNode {
public:
    VarDeclStmtNode(SourceLocation location, Type type, std::string name, ExprPtr init);
    void print(std::ostream &out, int indent) const override;

    Type type;
    std::string name;
    ExprPtr init; // may be null
};

class AssignStmtNode : public StmtNode {
public:
    AssignStmtNode(SourceLocation location, ExprPtr target, ExprPtr value);
    AssignStmtNode(SourceLocation location, ExprPtr target, BinaryOp compoundOp, ExprPtr value);
    void print(std::ostream &out, int indent) const override;

    // An lvalue expression: an IdentExprNode, a UnaryOpExprNode{Deref, ...},
    // an IndexExprNode, or a MemberExprNode.
    ExprPtr target;
    ExprPtr value;
    // For `target op= value` (e.g. `+=`): the read-modify-write is
    // `target = target <compoundOp> value`, but `target`'s *address* is
    // only evaluated once (important when it has side effects, e.g.
    // `arr[f()] += 1`) — see emitAssign. Empty for a plain `=`.
    std::optional<BinaryOp> compoundOp;
};

class ExprStmtNode : public StmtNode {
public:
    ExprStmtNode(SourceLocation location, ExprPtr expr);
    void print(std::ostream &out, int indent) const override;

    ExprPtr expr;
};

class IfStmtNode : public StmtNode {
public:
    IfStmtNode(SourceLocation location, ExprPtr condition,
               std::unique_ptr<BlockStmtNode> thenBlock,
               std::unique_ptr<BlockStmtNode> elseBlock);
    void print(std::ostream &out, int indent) const override;

    ExprPtr condition;
    std::unique_ptr<BlockStmtNode> thenBlock;
    std::unique_ptr<BlockStmtNode> elseBlock; // may be null
};

class WhileStmtNode : public StmtNode {
public:
    WhileStmtNode(SourceLocation location, ExprPtr condition, std::unique_ptr<BlockStmtNode> body);
    void print(std::ostream &out, int indent) const override;

    ExprPtr condition;
    std::unique_ptr<BlockStmtNode> body;
};

class ForStmtNode : public StmtNode {
public:
    ForStmtNode(SourceLocation location, StmtPtr init, ExprPtr condition, StmtPtr update,
                 std::unique_ptr<BlockStmtNode> body);
    void print(std::ostream &out, int indent) const override;

    StmtPtr init;         // VarDeclStmtNode or AssignStmtNode, may be null
    ExprPtr condition;    // may be null
    StmtPtr update;       // AssignStmtNode, may be null
    std::unique_ptr<BlockStmtNode> body;
};

class ReturnStmtNode : public StmtNode {
public:
    ReturnStmtNode(SourceLocation location, ExprPtr value);
    void print(std::ostream &out, int indent) const override;

    ExprPtr value; // may be null
};

class BreakStmtNode : public StmtNode {
public:
    explicit BreakStmtNode(SourceLocation location);
    void print(std::ostream &out, int indent) const override;
};

class ContinueStmtNode : public StmtNode {
public:
    explicit ContinueStmtNode(SourceLocation location);
    void print(std::ostream &out, int indent) const override;
};

// `do { body } while (condition);` — condition is checked after the body,
// so the body always runs at least once.
class DoWhileStmtNode : public StmtNode {
public:
    DoWhileStmtNode(SourceLocation location, std::unique_ptr<BlockStmtNode> body, ExprPtr condition);
    void print(std::ostream &out, int indent) const override;

    std::unique_ptr<BlockStmtNode> body;
    ExprPtr condition;
};

// `case value:` inside a switch body. Only recognized at the top level of
// a SwitchStmtNode's body (not nested inside an if/while within the
// switch) — see SwitchStmtNode's doc comment.
class CaseLabelStmtNode : public StmtNode {
public:
    CaseLabelStmtNode(SourceLocation location, long long value);
    void print(std::ostream &out, int indent) const override;

    long long value;
};

// `default:` inside a switch body. Same top-level-only restriction as
// CaseLabelStmtNode.
class DefaultLabelStmtNode : public StmtNode {
public:
    explicit DefaultLabelStmtNode(SourceLocation location);
    void print(std::ostream &out, int indent) const override;
};

// `switch (value) { ... }`. `body`'s direct statements are a flat,
// fallthrough sequence exactly like C: CaseLabelStmtNode/DefaultLabelStmtNode
// markers split it into segments, but control flows from one segment into
// the next unless a `break` (or `return`) ends it. Case labels nested
// inside an if/while/etc. within the switch (the rare "Duff's device"
// idiom) are not recognized — only the ones directly in `body`.
class SwitchStmtNode : public StmtNode {
public:
    SwitchStmtNode(SourceLocation location, ExprPtr value, std::unique_ptr<BlockStmtNode> body);
    void print(std::ostream &out, int indent) const override;

    ExprPtr value;
    std::unique_ptr<BlockStmtNode> body;
};

// `name:` — a goto target. Like case/default labels, recognized as a
// statement so it can appear anywhere a statement can.
class LabelStmtNode : public StmtNode {
public:
    LabelStmtNode(SourceLocation location, std::string name);
    void print(std::ostream &out, int indent) const override;

    std::string name;
};

// `goto name;`. Forward jumps work because codegen pre-scans a function
// body for every label before emitting any code — see
// CodeGenerator::collectLabels.
class GotoStmtNode : public StmtNode {
public:
    GotoStmtNode(SourceLocation location, std::string name);
    void print(std::ostream &out, int indent) const override;

    std::string name;
};

// ---------------------------------------------------------------------------
// Top-level
// ---------------------------------------------------------------------------

struct ParamNode {
    Type type;
    std::string name;
    SourceLocation location;

    void print(std::ostream &out, int indent) const;
};

struct FieldNode {
    Type type;
    std::string name;
    SourceLocation location;

    void print(std::ostream &out, int indent) const;
};

// A `struct Name { ... };` or `union Name { ... };` declaration. Both kinds
// share this shape; `isUnion` distinguishes how codegen lays out storage
// (sequential fields for a struct, all fields overlapping at offset 0 for
// a union).
class AggregateDeclNode {
public:
    AggregateDeclNode(SourceLocation location, std::string name, std::vector<FieldNode> fields, bool isUnion);

    void print(std::ostream &out, int indent) const;

    SourceLocation location;
    std::string name;
    std::vector<FieldNode> fields;
    bool isUnion;
};

struct EnumeratorNode {
    std::string name;
    long long value;
    SourceLocation location;
};

// An `enum Name { A, B = 2, ... };` declaration. Enumerators resolve to
// plain `int` constants (see Type's doc comment) — this node exists only
// so sema can register the constants and reject a duplicate tag name.
class EnumDeclNode {
public:
    EnumDeclNode(SourceLocation location, std::string name, std::vector<EnumeratorNode> enumerators);

    void print(std::ostream &out, int indent) const;

    SourceLocation location;
    std::string name;
    std::vector<EnumeratorNode> enumerators;
};

class FuncDefNode {
public:
    FuncDefNode(SourceLocation location, Type returnType, std::string name,
                std::vector<ParamNode> params, std::unique_ptr<BlockStmtNode> body);

    void print(std::ostream &out, int indent) const;

    SourceLocation location;
    Type returnType;
    std::string name;
    std::vector<ParamNode> params;
    std::unique_ptr<BlockStmtNode> body;
};

class ProgramNode {
public:
    void print(std::ostream &out, int indent = 0) const;

    std::vector<std::unique_ptr<AggregateDeclNode>> aggregates;
    std::vector<std::unique_ptr<EnumDeclNode>> enums;
    std::vector<std::unique_ptr<FuncDefNode>> functions;
};

} // namespace minic
