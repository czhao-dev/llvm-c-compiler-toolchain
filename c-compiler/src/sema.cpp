#include "sema.h"

#include <sstream>
#include <unordered_set>
#include <utility>

namespace minic {
namespace {

bool isNumericType(Type type) {
    return type == Type::Int || type == Type::Float || type == Type::Char;
}

// Bitwise operators and ++/-- only accept integral operands in C — not
// float, unlike the arithmetic operators.
bool isIntegralType(Type type) {
    return type == Type::Int || type == Type::Char;
}

bool isBitwiseOp(BinaryOp op) {
    return op == BinaryOp::BitAnd || op == BinaryOp::BitOr || op == BinaryOp::BitXor || op == BinaryOp::Shl ||
           op == BinaryOp::Shr;
}

bool isLValueExpr(const ExprNode &expr) {
    if (dynamic_cast<const IdentExprNode *>(&expr)) {
        return true;
    }
    if (const auto *unary = dynamic_cast<const UnaryOpExprNode *>(&expr)) {
        return unary->op == UnaryOp::Deref;
    }
    if (dynamic_cast<const IndexExprNode *>(&expr)) {
        return true;
    }
    if (const auto *member = dynamic_cast<const MemberExprNode *>(&expr)) {
        // `s.field` is an lvalue exactly when `s` is (you can't assign into
        // a field of a temporary, e.g. `getPoint().x = 1;`).
        return isLValueExpr(*member->base);
    }
    return false;
}

bool isNullPointerConstant(const ExprNode &expr) {
    const auto *lit = dynamic_cast<const IntLitExprNode *>(&expr);
    return lit != nullptr && lit->value == 0;
}

} // namespace

// ---------------------------------------------------------------------------
// Diagnostic
// ---------------------------------------------------------------------------

std::string Diagnostic::toString() const {
    std::ostringstream out;
    out << location.filename << ':' << location.line << ':' << location.column << ": "
        << (severity == DiagnosticSeverity::Error ? "error" : "warning") << ": " << message;
    return out.str();
}

// ---------------------------------------------------------------------------
// SymbolTable
// ---------------------------------------------------------------------------

void SymbolTable::enterScope() {
    scopes_.emplace_back();
}

void SymbolTable::exitScope() {
    scopes_.pop_back();
}

bool SymbolTable::declare(const std::string &name, Type type) {
    auto &scope = scopes_.back();
    if (scope.count(name) > 0) {
        return false;
    }
    scope.emplace(name, type);
    return true;
}

const Type *SymbolTable::lookup(const std::string &name) const {
    for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
        auto found = it->find(name);
        if (found != it->end()) {
            return &found->second;
        }
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// SemanticAnalyzer
// ---------------------------------------------------------------------------

void SemanticAnalyzer::error(SourceLocation location, std::string message) {
    diagnostics_.push_back(Diagnostic{DiagnosticSeverity::Error, std::move(location), std::move(message)});
}

void SemanticAnalyzer::warning(SourceLocation location, std::string message) {
    diagnostics_.push_back(Diagnostic{DiagnosticSeverity::Warning, std::move(location), std::move(message)});
}

std::vector<Diagnostic> SemanticAnalyzer::analyze(const ProgramNode &program) {
    diagnostics_.clear();
    functions_.clear();
    aggregates_.clear();
    tagNames_.clear();
    enumConstants_.clear();

    // printf is a built-in: it accepts any argument types and returns int,
    // matching the C standard library signature.
    functions_.emplace("printf", FunctionSignature{Type::Int, {}, /*isVariadic=*/true});

    collectTypeDeclarations(program);
    collectSignatures(program);

    for (const auto &func : program.functions) {
        checkFunction(*func);
    }

    return std::move(diagnostics_);
}

void SemanticAnalyzer::collectTypeDeclarations(const ProgramNode &program) {
    // Phase 1: register every tag name first, so a field can reference any
    // other struct/union/enum regardless of source order (mirrors how
    // collectSignatures lets functions call each other regardless of
    // order).
    for (const auto &aggregate : program.aggregates) {
        if (tagNames_.count(aggregate->name) > 0) {
            error(aggregate->location, "redefinition of '" + aggregate->name + "'");
            continue;
        }
        tagNames_.emplace(aggregate->name, aggregate->location);
        aggregates_.emplace(aggregate->name, AggregateInfo{{}, aggregate->isUnion});
    }
    for (const auto &enumDecl : program.enums) {
        if (tagNames_.count(enumDecl->name) > 0) {
            error(enumDecl->location, "redefinition of '" + enumDecl->name + "'");
            continue;
        }
        tagNames_.emplace(enumDecl->name, enumDecl->location);
    }

    // Phase 2: resolve each aggregate's field types and each enum's
    // constants, now that every tag name is visible. A duplicate tag name
    // was already reported in phase 1 and has no entry in aggregates_ to
    // append to, so skip every occurrence but the first.
    std::unordered_set<std::string> definedAggregates;
    for (const auto &aggregate : program.aggregates) {
        if (definedAggregates.insert(aggregate->name).second) {
            checkAggregateFields(*aggregate);
        }
    }
    for (const auto &enumDecl : program.enums) {
        for (const auto &enumerator : enumDecl->enumerators) {
            if (enumConstants_.count(enumerator.name) > 0) {
                error(enumerator.location, "redefinition of '" + enumerator.name + "'");
                continue;
            }
            enumConstants_.emplace(enumerator.name, enumerator.value);
        }
    }
}

void SemanticAnalyzer::checkAggregateFields(const AggregateDeclNode &decl) {
    AggregateInfo &info = aggregates_.at(decl.name);
    for (const auto &field : decl.fields) {
        checkTypeIsValid(field.location, field.type);

        if (field.type.elementType() == Type::Void) {
            error(field.location, "field '" + field.name + "' cannot have type 'void'");
        }

        // A struct/union can't contain itself by value (infinite size);
        // only a pointer (or, transitively, a pointer anywhere in a
        // multi-struct cycle) breaks the recursion. Only the direct case
        // is checked here — see AggregateDeclNode's doc comment in ast.h.
        if (field.type.isAggregate() && field.type.aggregateName() == decl.name && !field.type.isPointer()) {
            error(field.location, "field '" + field.name + "' directly contains '" + typeName(field.type) +
                                       "', which would have infinite size");
        }

        bool duplicate = false;
        for (const auto &existing : info.fields) {
            if (existing.first == field.name) {
                error(field.location, "duplicate field '" + field.name + "' in '" + decl.name + "'");
                duplicate = true;
                break;
            }
        }
        if (!duplicate) {
            info.fields.emplace_back(field.name, field.type);
        }
    }
}

void SemanticAnalyzer::checkTypeIsValid(const SourceLocation &location, Type type) {
    if (!type.isAggregate()) {
        return;
    }
    if (aggregates_.count(type.aggregateName()) == 0) {
        error(location, "use of undeclared " + std::string(type.isUnion() ? "union" : "struct") + " '" +
                             type.aggregateName() + "'");
    }
}

void SemanticAnalyzer::collectSignatures(const ProgramNode &program) {
    for (const auto &func : program.functions) {
        if (func->name == "printf") {
            error(func->location, "cannot redefine built-in function 'printf'");
            continue;
        }
        if (functions_.count(func->name) > 0) {
            error(func->location, "redefinition of function '" + func->name + "'");
            continue;
        }

        FunctionSignature sig;
        sig.returnType = func->returnType;
        for (const auto &param : func->params) {
            sig.paramTypes.push_back(param.type);
        }
        functions_.emplace(func->name, std::move(sig));
    }
}

void SemanticAnalyzer::checkFunction(const FuncDefNode &func) {
    currentFunction_ = &func;
    loopDepth_ = 0;
    switchDepth_ = 0;

    checkTypeIsValid(func.location, func.returnType);

    // Pre-pass so a `goto` can jump forward to a label declared later in
    // the function (mirrors collectSignatures letting functions call each
    // other regardless of order).
    declaredLabels_.clear();
    std::unordered_set<std::string> duplicateLabels;
    collectLabels(*func.body, declaredLabels_, duplicateLabels);
    for (const auto &name : duplicateLabels) {
        error(func.location, "duplicate label '" + name + "' in function '" + func.name + "'");
    }

    symbols_.enterScope();

    for (const auto &param : func.params) {
        checkTypeIsValid(param.location, param.type);
        if (param.type == Type::Void) {
            error(param.location, "parameter '" + param.name + "' cannot have type 'void'");
            continue;
        }
        if (!symbols_.declare(param.name, param.type)) {
            error(param.location, "redefinition of parameter '" + param.name + "'");
        }
    }

    checkBlock(*func.body);

    symbols_.exitScope();
    currentFunction_ = nullptr;
}

void SemanticAnalyzer::checkBlock(const BlockStmtNode &block) {
    for (const auto &stmt : block.statements) {
        checkStmt(*stmt);
    }
}

void SemanticAnalyzer::checkStmt(const StmtNode &stmt) {
    if (const auto *decl = dynamic_cast<const VarDeclStmtNode *>(&stmt)) {
        checkVarDecl(*decl);
    } else if (const auto *assign = dynamic_cast<const AssignStmtNode *>(&stmt)) {
        checkAssign(*assign);
    } else if (const auto *exprStmt = dynamic_cast<const ExprStmtNode *>(&stmt)) {
        checkExpr(*exprStmt->expr);
    } else if (const auto *ifStmt = dynamic_cast<const IfStmtNode *>(&stmt)) {
        checkIf(*ifStmt);
    } else if (const auto *whileStmt = dynamic_cast<const WhileStmtNode *>(&stmt)) {
        checkWhile(*whileStmt);
    } else if (const auto *forStmt = dynamic_cast<const ForStmtNode *>(&stmt)) {
        checkFor(*forStmt);
    } else if (const auto *ret = dynamic_cast<const ReturnStmtNode *>(&stmt)) {
        checkReturn(*ret);
    } else if (dynamic_cast<const BreakStmtNode *>(&stmt)) {
        if (loopDepth_ == 0 && switchDepth_ == 0) {
            error(stmt.location, "'break' statement not within a loop or switch");
        }
    } else if (dynamic_cast<const ContinueStmtNode *>(&stmt)) {
        if (loopDepth_ == 0) {
            error(stmt.location, "'continue' statement not within a loop");
        }
    } else if (const auto *doWhile = dynamic_cast<const DoWhileStmtNode *>(&stmt)) {
        checkDoWhile(*doWhile);
    } else if (const auto *switchStmt = dynamic_cast<const SwitchStmtNode *>(&stmt)) {
        checkSwitch(*switchStmt);
    } else if (dynamic_cast<const CaseLabelStmtNode *>(&stmt)) {
        error(stmt.location, "'case' label not within (or not directly inside) a switch statement");
    } else if (dynamic_cast<const DefaultLabelStmtNode *>(&stmt)) {
        error(stmt.location, "'default' label not within (or not directly inside) a switch statement");
    } else if (dynamic_cast<const LabelStmtNode *>(&stmt)) {
        // Validity (no duplicates) is checked once up front, in checkFunction.
    } else if (const auto *gotoStmt = dynamic_cast<const GotoStmtNode *>(&stmt)) {
        if (declaredLabels_.count(gotoStmt->name) == 0) {
            error(stmt.location, "use of undeclared label '" + gotoStmt->name + "'");
        }
    } else if (const auto *block = dynamic_cast<const BlockStmtNode *>(&stmt)) {
        symbols_.enterScope();
        checkBlock(*block);
        symbols_.exitScope();
    }
}

void SemanticAnalyzer::checkVarDecl(const VarDeclStmtNode &decl) {
    checkTypeIsValid(decl.location, decl.type);
    if (decl.type.elementType() == Type::Void) {
        error(decl.location, "variable '" + decl.name + "' cannot have type 'void'");
    }

    if (decl.init) {
        const Type valueType = checkExpr(*decl.init);
        checkAssignable(decl.location, decl.type, valueType,
                         "initializer for '" + decl.name + "'", decl.init.get());
    }

    if (!symbols_.declare(decl.name, decl.type)) {
        error(decl.location, "redefinition of '" + decl.name + "'");
    }
}

void SemanticAnalyzer::checkAssign(const AssignStmtNode &assign) {
    // Arrays are not assignable as a whole, only element-by-element via
    // `arr[i] = ...`; reject `arr = ...` before the generic decay-on-read
    // path in checkExpr/checkIdent hides the array-ness of the target.
    if (const auto *ident = dynamic_cast<const IdentExprNode *>(assign.target.get())) {
        if (const Type *declaredType = symbols_.lookup(ident->name);
            declaredType && declaredType->isArray()) {
            error(assign.location, "array '" + ident->name + "' is not assignable");
            checkExpr(*assign.value);
            return;
        }
    }

    const Type targetType = checkExpr(*assign.target);
    const Type valueType = checkExpr(*assign.value);

    if (!isLValueExpr(*assign.target)) {
        error(assign.location, "left-hand side of assignment is not assignable");
        return;
    }

    if (assign.compoundOp) {
        // `target op= value` is `target = target op value`, but evaluates
        // target's address only once — checkBinOpTypes here just combines
        // the two types already computed above, it doesn't re-checkExpr
        // the target.
        const Type resultType = checkBinOpTypes(assign.location, *assign.compoundOp, targetType, valueType,
                                                 nullptr, assign.value.get());
        checkAssignable(assign.location, targetType, resultType, "compound assignment");
        return;
    }

    checkAssignable(assign.location, targetType, valueType, "assignment", assign.value.get());
}

void SemanticAnalyzer::checkIf(const IfStmtNode &stmt) {
    checkCondition(stmt.condition->location, checkExpr(*stmt.condition));

    symbols_.enterScope();
    checkBlock(*stmt.thenBlock);
    symbols_.exitScope();

    if (stmt.elseBlock) {
        symbols_.enterScope();
        checkBlock(*stmt.elseBlock);
        symbols_.exitScope();
    }
}

void SemanticAnalyzer::checkWhile(const WhileStmtNode &stmt) {
    checkCondition(stmt.condition->location, checkExpr(*stmt.condition));

    ++loopDepth_;
    symbols_.enterScope();
    checkBlock(*stmt.body);
    symbols_.exitScope();
    --loopDepth_;
}

void SemanticAnalyzer::checkFor(const ForStmtNode &stmt) {
    symbols_.enterScope();

    if (stmt.init) {
        checkStmt(*stmt.init);
    }
    if (stmt.condition) {
        checkCondition(stmt.condition->location, checkExpr(*stmt.condition));
    }
    if (stmt.update) {
        checkStmt(*stmt.update);
    }

    ++loopDepth_;
    symbols_.enterScope();
    checkBlock(*stmt.body);
    symbols_.exitScope();
    --loopDepth_;

    symbols_.exitScope();
}

void SemanticAnalyzer::checkDoWhile(const DoWhileStmtNode &stmt) {
    ++loopDepth_;
    symbols_.enterScope();
    checkBlock(*stmt.body);
    symbols_.exitScope();
    --loopDepth_;

    // Checked in the outer scope, not the body's — a variable declared in
    // the do-block is out of scope by the time `while (...)` is reached,
    // matching C (the body's closing brace ends its scope before `while`).
    checkCondition(stmt.condition->location, checkExpr(*stmt.condition));
}

void SemanticAnalyzer::checkSwitch(const SwitchStmtNode &stmt) {
    const Type valueType = checkExpr(*stmt.value);
    if (!isIntegralType(valueType)) {
        error(stmt.value->location, "switch value must have an integer type, got '" + typeName(valueType) + "'");
    }

    ++switchDepth_;
    symbols_.enterScope();

    bool hasDefault = false;
    std::unordered_set<long long> seenValues;
    for (const auto &s : stmt.body->statements) {
        if (const auto *caseLabel = dynamic_cast<const CaseLabelStmtNode *>(s.get())) {
            if (!seenValues.insert(caseLabel->value).second) {
                error(caseLabel->location, "duplicate case value '" + std::to_string(caseLabel->value) + "'");
            }
        } else if (dynamic_cast<const DefaultLabelStmtNode *>(s.get())) {
            if (hasDefault) {
                error(s->location, "multiple 'default' labels in one switch statement");
            }
            hasDefault = true;
        } else {
            checkStmt(*s);
        }
    }

    symbols_.exitScope();
    --switchDepth_;
}

void SemanticAnalyzer::collectLabels(const StmtNode &stmt, std::unordered_set<std::string> &labels,
                                      std::unordered_set<std::string> &duplicates) {
    if (const auto *label = dynamic_cast<const LabelStmtNode *>(&stmt)) {
        if (!labels.insert(label->name).second) {
            duplicates.insert(label->name);
        }
    } else if (const auto *block = dynamic_cast<const BlockStmtNode *>(&stmt)) {
        for (const auto &s : block->statements) {
            collectLabels(*s, labels, duplicates);
        }
    } else if (const auto *ifStmt = dynamic_cast<const IfStmtNode *>(&stmt)) {
        collectLabels(*ifStmt->thenBlock, labels, duplicates);
        if (ifStmt->elseBlock) {
            collectLabels(*ifStmt->elseBlock, labels, duplicates);
        }
    } else if (const auto *whileStmt = dynamic_cast<const WhileStmtNode *>(&stmt)) {
        collectLabels(*whileStmt->body, labels, duplicates);
    } else if (const auto *doWhile = dynamic_cast<const DoWhileStmtNode *>(&stmt)) {
        collectLabels(*doWhile->body, labels, duplicates);
    } else if (const auto *forStmt = dynamic_cast<const ForStmtNode *>(&stmt)) {
        collectLabels(*forStmt->body, labels, duplicates);
    } else if (const auto *switchStmt = dynamic_cast<const SwitchStmtNode *>(&stmt)) {
        collectLabels(*switchStmt->body, labels, duplicates);
    }
}

void SemanticAnalyzer::checkReturn(const ReturnStmtNode &stmt) {
    const Type returnType = currentFunction_->returnType;
    const std::string &funcName = currentFunction_->name;

    if (stmt.value) {
        const Type valueType = checkExpr(*stmt.value);
        if (returnType == Type::Void) {
            error(stmt.location, "void function '" + funcName + "' should not return a value");
        } else {
            checkAssignable(stmt.location, returnType, valueType,
                             "return value of function '" + funcName + "'", stmt.value.get());
        }
    } else if (returnType != Type::Void) {
        error(stmt.location, "non-void function '" + funcName + "' must return a value");
    }
}

void SemanticAnalyzer::checkCondition(const SourceLocation &location, Type type) {
    if (!isNumericType(type) && !type.isPointer()) {
        error(location, "condition must have a numeric or pointer type, got '" + typeName(type) + "'");
    }
}

Type SemanticAnalyzer::checkExpr(const ExprNode &expr) {
    if (dynamic_cast<const IntLitExprNode *>(&expr)) {
        return Type::Int;
    }
    if (dynamic_cast<const FloatLitExprNode *>(&expr)) {
        return Type::Float;
    }
    if (dynamic_cast<const CharLitExprNode *>(&expr)) {
        return Type::Char;
    }
    if (dynamic_cast<const StringLitExprNode *>(&expr)) {
        return Type::String;
    }
    if (const auto *ident = dynamic_cast<const IdentExprNode *>(&expr)) {
        return checkIdent(*ident);
    }
    if (const auto *unary = dynamic_cast<const UnaryOpExprNode *>(&expr)) {
        return checkUnaryOp(*unary);
    }
    if (const auto *binOp = dynamic_cast<const BinOpExprNode *>(&expr)) {
        return checkBinOp(*binOp);
    }
    if (const auto *call = dynamic_cast<const CallExprNode *>(&expr)) {
        return checkCall(*call);
    }
    if (const auto *index = dynamic_cast<const IndexExprNode *>(&expr)) {
        return checkIndex(*index);
    }
    if (const auto *member = dynamic_cast<const MemberExprNode *>(&expr)) {
        return checkMember(*member);
    }
    if (const auto *ternary = dynamic_cast<const TernaryExprNode *>(&expr)) {
        return checkTernary(*ternary);
    }
    if (const auto *incDec = dynamic_cast<const IncDecExprNode *>(&expr)) {
        return checkIncDec(*incDec);
    }
    if (const auto *cast = dynamic_cast<const CastExprNode *>(&expr)) {
        return checkCast(*cast);
    }
    return Type::Int;
}

Type SemanticAnalyzer::checkIdent(const IdentExprNode &expr) {
    const Type *type = symbols_.lookup(expr.name);
    if (type) {
        // An array decays to a pointer to its first element whenever it's
        // read as a value (matching C); the array's own storage is only
        // exposed through emitLValue, e.g. for indexing or passing it to a
        // function.
        return type->isArray() ? type->decay() : *type;
    }
    // A variable can shadow an enum constant from an enclosing scope (they
    // share C's "ordinary identifier" namespace), so the scoped lookup
    // above must run first.
    if (enumConstants_.count(expr.name) > 0) {
        return Type::Int;
    }
    error(expr.location, "use of undeclared variable '" + expr.name + "'");
    return Type::Int;
}

Type SemanticAnalyzer::checkUnaryOp(const UnaryOpExprNode &expr) {
    if (expr.op == UnaryOp::AddressOf) {
        // MiniC has no pointer-to-array type, so &arr can't be represented
        // correctly; check this directly (before checkExpr decays the
        // array to a pointer and hides its array-ness).
        if (const auto *ident = dynamic_cast<const IdentExprNode *>(expr.operand.get())) {
            if (const Type *declaredType = symbols_.lookup(ident->name);
                declaredType && declaredType->isArray()) {
                error(expr.location, "cannot take the address of array '" + ident->name +
                                          "'; it already converts to a pointer when used as a value");
                return declaredType->elementType().pointerTo();
            }
        }

        const Type operandType = checkExpr(*expr.operand);
        if (!isLValueExpr(*expr.operand)) {
            error(expr.location, "cannot take the address of a non-lvalue expression");
            return operandType.pointerTo();
        }
        return operandType.pointerTo();
    }

    if (expr.op == UnaryOp::Deref) {
        const Type operandType = checkExpr(*expr.operand);
        if (!operandType.isPointer()) {
            error(expr.location, "cannot dereference non-pointer type '" + typeName(operandType) + "'");
            return Type::Int;
        }
        const Type pointee = operandType.pointee();
        if (pointee == Type::Void) {
            error(expr.location, "cannot dereference pointer to incomplete type 'void'");
            return Type::Int;
        }
        return pointee;
    }

    if (expr.op == UnaryOp::BitNot) {
        const Type operandType = checkExpr(*expr.operand);
        if (!isIntegralType(operandType)) {
            error(expr.location, "invalid operand to unary '~': '" + typeName(operandType) + "'");
            return Type::Int;
        }
        return Type::Int;
    }

    if (expr.op == UnaryOp::Not) {
        // `!p` is allowed for a pointer operand too — it's the negation of
        // the same truthiness rule used by `if`/`while` (checkCondition).
        const Type operandType = checkExpr(*expr.operand);
        if (!isNumericType(operandType) && !operandType.isPointer()) {
            error(expr.location, "invalid operand to unary '!': '" + typeName(operandType) + "'");
        }
        return Type::Int;
    }

    const Type operandType = checkExpr(*expr.operand);
    if (!isNumericType(operandType)) {
        error(expr.location, "invalid operand to unary '" + unaryOpSymbol(expr.op) + "': '" +
                                  typeName(operandType) + "'");
        return Type::Int;
    }

    switch (expr.op) {
    case UnaryOp::Negate: return operandType == Type::Float ? Type::Float : Type::Int;
    default: break;
    }
    return Type::Int;
}

Type SemanticAnalyzer::checkBinOp(const BinOpExprNode &expr) {
    if (expr.op == BinaryOp::Comma) {
        checkExpr(*expr.lhs);
        return checkExpr(*expr.rhs);
    }

    const Type lhsType = checkExpr(*expr.lhs);
    const Type rhsType = checkExpr(*expr.rhs);
    return checkBinOpTypes(expr.location, expr.op, lhsType, rhsType, expr.lhs.get(), expr.rhs.get());
}

// The type-combining half of checkBinOp, factored out so compound
// assignment (`target op= value`) can reuse it with the target's and
// value's *already-computed* types — it doesn't re-evaluate any
// expression. `lhsExpr`/`rhsExpr` are only consulted for null-pointer-
// constant detection and may be null when there's no corresponding
// expression (e.g. compound assignment's synthetic "lhs").
Type SemanticAnalyzer::checkBinOpTypes(const SourceLocation &location, BinaryOp op, Type lhsType, Type rhsType,
                                        const ExprNode *lhsExpr, const ExprNode *rhsExpr) {
    if (op == BinaryOp::And || op == BinaryOp::Or) {
        // Each operand is independently converted to bool (same rule as
        // checkCondition / unary '!'), so a pointer operand is fine even
        // though it isn't "numeric".
        const bool lhsOk = isNumericType(lhsType) || lhsType.isPointer();
        const bool rhsOk = isNumericType(rhsType) || rhsType.isPointer();
        if (!lhsOk || !rhsOk) {
            error(location, "invalid operands to binary '" + binaryOpSymbol(op) + "': '" + typeName(lhsType) +
                                 "' and '" + typeName(rhsType) + "'");
        }
        return Type::Int;
    }

    if (lhsType.isPointer() || rhsType.isPointer()) {
        const bool isEqOrNeq = op == BinaryOp::Eq || op == BinaryOp::Neq;
        const bool samePointerType = lhsType == rhsType;
        const bool lhsNullAgainstPointer =
            rhsType.isPointer() && lhsType == Type::Int && lhsExpr && isNullPointerConstant(*lhsExpr);
        const bool rhsNullAgainstPointer =
            lhsType.isPointer() && rhsType == Type::Int && rhsExpr && isNullPointerConstant(*rhsExpr);
        if (isEqOrNeq && (samePointerType || lhsNullAgainstPointer || rhsNullAgainstPointer)) {
            return Type::Int;
        }
        error(location, "invalid operands to binary '" + binaryOpSymbol(op) + "': '" + typeName(lhsType) +
                             "' and '" + typeName(rhsType) + "'");
        return Type::Int;
    }

    if (isBitwiseOp(op)) {
        if (!isIntegralType(lhsType) || !isIntegralType(rhsType)) {
            error(location, "invalid operands to binary '" + binaryOpSymbol(op) + "': '" + typeName(lhsType) +
                                 "' and '" + typeName(rhsType) + "'");
        }
        return Type::Int;
    }

    if (!isNumericType(lhsType) || !isNumericType(rhsType)) {
        error(location, "invalid operands to binary '" + binaryOpSymbol(op) + "': '" + typeName(lhsType) +
                             "' and '" + typeName(rhsType) + "'");
        return Type::Int;
    }

    switch (op) {
    case BinaryOp::Add:
    case BinaryOp::Sub:
    case BinaryOp::Mul:
    case BinaryOp::Div:
        return (lhsType == Type::Float || rhsType == Type::Float) ? Type::Float : Type::Int;
    default:
        // Comparisons (==, !=, <, >, <=, >=) and logical operators (&&, ||)
        // all produce an int (0 or 1), matching C.
        return Type::Int;
    }
}

Type SemanticAnalyzer::checkCall(const CallExprNode &expr) {
    auto it = functions_.find(expr.callee);
    if (it == functions_.end()) {
        error(expr.location, "call to undeclared function '" + expr.callee + "'");
        for (const auto &arg : expr.args) {
            checkExpr(*arg);
        }
        return Type::Int;
    }

    const FunctionSignature &sig = it->second;

    if (!sig.isVariadic && expr.args.size() != sig.paramTypes.size()) {
        error(expr.location, "wrong number of arguments to '" + expr.callee + "' — expected " +
                                  std::to_string(sig.paramTypes.size()) + ", got " +
                                  std::to_string(expr.args.size()));
    }

    for (std::size_t i = 0; i < expr.args.size(); ++i) {
        const Type argType = checkExpr(*expr.args[i]);
        if (!sig.isVariadic && i < sig.paramTypes.size()) {
            checkAssignable(expr.args[i]->location, sig.paramTypes[i], argType,
                             "argument " + std::to_string(i + 1) + " to '" + expr.callee + "'",
                             expr.args[i].get());
        }
    }

    return sig.returnType;
}

Type SemanticAnalyzer::checkIndex(const IndexExprNode &expr) {
    const Type baseType = checkExpr(*expr.base);
    const Type indexType = checkExpr(*expr.index);

    if (!isNumericType(indexType)) {
        error(expr.location, "array subscript is not an integer");
    }

    if (!baseType.isPointer()) {
        error(expr.location, "subscripted value is not an array or pointer ('" + typeName(baseType) + "')");
        return Type::Int;
    }

    const Type element = baseType.pointee();
    if (element == Type::Void) {
        error(expr.location, "cannot subscript pointer to incomplete type 'void'");
        return Type::Int;
    }
    return element;
}

Type SemanticAnalyzer::checkMember(const MemberExprNode &expr) {
    const Type baseType = checkExpr(*expr.base);
    if (!baseType.isAggregate()) {
        error(expr.location, "member reference base type '" + typeName(baseType) + "' is not a struct or union");
        return Type::Int;
    }

    // checkTypeIsValid (run when the base variable was declared) guarantees
    // this lookup succeeds for any program that reaches this point cleanly;
    // if it didn't, the missing-tag error has already been reported and
    // there's nothing more useful to say about the member access itself.
    auto it = aggregates_.find(baseType.aggregateName());
    if (it == aggregates_.end()) {
        return Type::Int;
    }

    for (const auto &field : it->second.fields) {
        if (field.first == expr.field) {
            return field.second;
        }
    }

    error(expr.location, "no member named '" + expr.field + "' in '" + typeName(baseType) + "'");
    return Type::Int;
}

Type SemanticAnalyzer::checkTernary(const TernaryExprNode &expr) {
    checkCondition(expr.condition->location, checkExpr(*expr.condition));

    const Type thenType = checkExpr(*expr.thenExpr);
    const Type elseType = checkExpr(*expr.elseExpr);

    if (thenType == elseType) {
        return thenType;
    }
    if (isNumericType(thenType) && isNumericType(elseType)) {
        return (thenType == Type::Float || elseType == Type::Float) ? Type::Float : Type::Int;
    }
    // Let one branch be a null-pointer constant against a pointer branch,
    // matching the rule for == / != and plain assignment.
    if (thenType.isPointer() && elseType == Type::Int && isNullPointerConstant(*expr.elseExpr)) {
        return thenType;
    }
    if (elseType.isPointer() && thenType == Type::Int && isNullPointerConstant(*expr.thenExpr)) {
        return elseType;
    }

    error(expr.location, "incompatible operand types in ternary expression: '" + typeName(thenType) + "' and '" +
                              typeName(elseType) + "'");
    return thenType;
}

Type SemanticAnalyzer::checkIncDec(const IncDecExprNode &expr) {
    const Type targetType = checkExpr(*expr.target);
    const char *symbol = expr.isIncrement ? "++" : "--";

    if (!isLValueExpr(*expr.target)) {
        error(expr.location, "operand of '" + std::string(symbol) + "' is not assignable");
        return targetType;
    }
    // Pointer ++/-- would be pointer arithmetic, which isn't supported yet
    // (see the arithmetic-operators section of docs/language_spec.md).
    if (!isNumericType(targetType)) {
        error(expr.location, "invalid operand to '" + std::string(symbol) + "': '" + typeName(targetType) + "'");
        return Type::Int;
    }
    return targetType;
}

Type SemanticAnalyzer::checkCast(const CastExprNode &expr) {
    const Type operandType = checkExpr(*expr.operand);

    // Numeric casts only for now (int/float/char in any direction, the
    // exact legality castNumeric's codegen already implements pairwise).
    // Pointer/aggregate casts are an explicit out-of-scope follow-up, not
    // a silent decision never to support them -- see docs/language_spec.md.
    if (!isNumericType(expr.targetType) || !isNumericType(operandType)) {
        error(expr.location, "cannot cast '" + typeName(operandType) + "' to '" + typeName(expr.targetType) + "'");
    }
    return expr.targetType;
}

void SemanticAnalyzer::checkAssignable(const SourceLocation &location, Type target, Type value,
                                        const std::string &context, const ExprNode *valueExpr) {
    if (target == value) {
        return;
    }

    if (target.isPointer() || value.isPointer()) {
        if (target.isPointer() && value == Type::Int && valueExpr && isNullPointerConstant(*valueExpr)) {
            return;
        }
        error(location, context + ": cannot convert '" + typeName(value) + "' to '" + typeName(target) + "'");
        return;
    }

    if (!isNumericType(target) || !isNumericType(value)) {
        error(location, context + ": cannot convert '" + typeName(value) + "' to '" + typeName(target) + "'");
        return;
    }

    if (value == Type::Float && target != Type::Float) {
        warning(location, context + ": implicit conversion from 'float' to '" + typeName(target) +
                               "' may lose precision");
    }
}

std::string semanticAnalyzerStatus() {
    return "semantic analyzer: scope and type checking implemented";
}

} // namespace minic
