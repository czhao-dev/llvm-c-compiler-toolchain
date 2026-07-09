#pragma once

#include "ast.h"
#include "token.h"

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace minic {

enum class DiagnosticSeverity {
    Warning,
    Error,
};

// A single semantic error or warning, with the source location it applies to.
struct Diagnostic {
    DiagnosticSeverity severity;
    SourceLocation location;
    std::string message;

    // Formats as "file:line:col: error: message" (or "warning:").
    std::string toString() const;
};

// Scoped symbol table mapping variable names to their declared types.
// Scopes are pushed/popped as the analyzer enters and leaves blocks; lookup
// searches from the innermost scope outward.
class SymbolTable {
public:
    void enterScope();
    void exitScope();

    // Declares `name` in the innermost scope. Returns false (and does not
    // declare) if `name` is already declared in that scope.
    bool declare(const std::string &name, Type type);

    // Returns the declared type of `name`, searching from the innermost
    // scope outward, or nullptr if `name` is not in scope.
    const Type *lookup(const std::string &name) const;

private:
    std::vector<std::unordered_map<std::string, Type>> scopes_;
};

// Walks a parsed program and checks scoping and type rules. Diagnostics are
// collected rather than thrown so a single run can report every problem.
class SemanticAnalyzer {
public:
    std::vector<Diagnostic> analyze(const ProgramNode &program);

private:
    struct FunctionSignature {
        Type returnType;
        std::vector<Type> paramTypes;
        bool isVariadic = false;
    };

    // Ordered (name, type) fields of a struct or union, keyed by tag name.
    struct AggregateInfo {
        std::vector<std::pair<std::string, Type>> fields;
        bool isUnion = false;
    };

    void collectSignatures(const ProgramNode &program);
    // Two-phase, mirroring collectSignatures' role for functions: phase 1
    // registers every struct/union/enum tag name (so fields can reference
    // any other aggregate regardless of declaration order); phase 2
    // resolves and validates each aggregate's field types.
    void collectTypeDeclarations(const ProgramNode &program);
    void checkAggregateFields(const AggregateDeclNode &decl);
    // Errors if `type` (or, for an array/pointer, its element type) names a
    // struct/union tag that was never declared.
    void checkTypeIsValid(const SourceLocation &location, Type type);
    void checkFunction(const FuncDefNode &func);

    void checkBlock(const BlockStmtNode &block);
    void checkStmt(const StmtNode &stmt);
    void checkVarDecl(const VarDeclStmtNode &decl);
    void checkAssign(const AssignStmtNode &assign);
    void checkIf(const IfStmtNode &stmt);
    void checkWhile(const WhileStmtNode &stmt);
    void checkFor(const ForStmtNode &stmt);
    void checkDoWhile(const DoWhileStmtNode &stmt);
    void checkSwitch(const SwitchStmtNode &stmt);
    // Recursively gathers every LabelStmtNode name in `stmt` (and its
    // nested blocks/loops/switch) into `labels`, so a `goto` can jump
    // forward to a label declared later in the same function; any name
    // seen more than once is recorded in `duplicates`.
    void collectLabels(const StmtNode &stmt, std::unordered_set<std::string> &labels,
                        std::unordered_set<std::string> &duplicates);
    void checkReturn(const ReturnStmtNode &stmt);
    void checkCondition(const SourceLocation &location, Type type);

    Type checkExpr(const ExprNode &expr);
    Type checkIdent(const IdentExprNode &expr);
    Type checkUnaryOp(const UnaryOpExprNode &expr);
    Type checkBinOp(const BinOpExprNode &expr);
    // The type-combining half of checkBinOp; see its definition for why
    // it's factored out (compound assignment reuses it).
    Type checkBinOpTypes(const SourceLocation &location, BinaryOp op, Type lhsType, Type rhsType,
                          const ExprNode *lhsExpr, const ExprNode *rhsExpr);
    Type checkCall(const CallExprNode &expr);
    Type checkIndex(const IndexExprNode &expr);
    Type checkMember(const MemberExprNode &expr);
    Type checkTernary(const TernaryExprNode &expr);
    Type checkIncDec(const IncDecExprNode &expr);

    // Checks that a value of type `value` may be stored into (or returned
    // as, or passed as an argument of) type `target`. Reports an error for
    // incompatible types, or a warning for a narrowing float -> int/char
    // conversion. `context` is prepended to any diagnostic message.
    // `valueExpr`, if given, lets a literal `0` be accepted as a null
    // pointer constant when `target` is a pointer type.
    void checkAssignable(const SourceLocation &location, Type target, Type value,
                          const std::string &context, const ExprNode *valueExpr = nullptr);

    void error(SourceLocation location, std::string message);
    void warning(SourceLocation location, std::string message);

    std::vector<Diagnostic> diagnostics_;
    std::unordered_map<std::string, FunctionSignature> functions_;
    std::unordered_map<std::string, AggregateInfo> aggregates_;
    // Tag names (struct/union/enum) share one namespace, matching C, so
    // `struct Foo` and `union Foo` can't both exist; checked independently
    // of `aggregates_`, which only holds struct/union field layouts.
    std::unordered_map<std::string, SourceLocation> tagNames_;
    std::unordered_map<std::string, long long> enumConstants_;
    SymbolTable symbols_;
    const FuncDefNode *currentFunction_ = nullptr;
    int loopDepth_ = 0;
    int switchDepth_ = 0;
    std::unordered_set<std::string> declaredLabels_;
};

std::string semanticAnalyzerStatus();

} // namespace minic
