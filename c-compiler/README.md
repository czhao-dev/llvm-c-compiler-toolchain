# MiniC Compiler

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![LLVM](https://img.shields.io/badge/LLVM-17%2B-orange.svg)](https://llvm.org)
[![CMake](https://img.shields.io/badge/CMake-3.20%2B-064F8C.svg)](https://cmake.org)
[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

> A compiler for a statically-typed subset of C — from hand-written source
> code through a lexer, parser, semantic analyzer, and LLVM IR generator,
> producing native binaries that run without any runtime dependency.

---

## Table of Contents

- [Overview](#overview)
- [Repo Structure](#repo-structure)
- [Supported Language Features](#supported-language-features)
- [Example MiniC Programs](#example-minic-programs)
- [Pipeline Architecture](#pipeline-architecture)
- [Pipeline Walkthrough — fibonacci(5)](#pipeline-walkthrough--fibonacci5)
- [Testing & Validation](#testing--validation)
- [Optimization](#optimization)
- [Error Messages](#error-messages)
- [Build & Run](#build--run)
- [License](#license)
- [References](#references)

---

## Overview

MiniC takes C source files written in a well-defined subset of the language
and compiles them to native machine code through a complete pipeline: a
hand-written lexer, a recursive-descent parser, a semantic analyzer that
catches type errors and undeclared variables, and an LLVM IR code generator
that produces binaries via the LLVM backend.

Because the source language is a real subset of C — not an invented DSL —
the project needs no explanation of what the language does or why it
exists. Anyone who has written C can read a MiniC program and understand it
immediately.

The project demonstrates how a compiler works end to end: how source text
becomes tokens, how tokens become a structured tree, how that tree is
type-checked, and how it becomes LLVM IR that the backend turns into a
runnable binary.

**All phases are implemented, tested, and cross-validated against clang.**
`minic <file.mc>` compiles straight to a native binary; `minic <file.mc> -O2`
runs LLVM's full optimization pipeline first. See
[Testing & Validation](#testing--validation) below for what's actually been verified.

**CLI flags:** `--emit-tokens`, `--emit-ast`, `--emit-ir`,
`-O0`/`-O1`/`-O2`/`-O3`, `-o`.

The compiler is structured as a static library (`libminic_core.a`) with a
thin `main.cpp` CLI on top, making it easy to embed in test harnesses or
tooling without going through the command-line interface.

---

## Repo Structure

```
c-compiler/
├── README.md
├── LICENSE
├── CMakeLists.txt
├── scripts/
│   └── configure.sh         ← sets LLVM_DIR and invokes cmake
├── include/
│   ├── token.h              ← token kinds and Token struct
│   ├── lexer.h
│   ├── ast.h                ← AST node hierarchy
│   ├── parser.h
│   ├── sema.h               ← semantic analyzer
│   └── codegen.h
├── src/
│   ├── lexer.cpp
│   ├── ast.cpp
│   ├── parser.cpp
│   ├── sema.cpp
│   ├── codegen.cpp
│   └── main.cpp             ← CLI: invoke stages, flags for IR dump
├── tests/
│   ├── lexer_test.cpp
│   ├── smoke_test.cpp
│   ├── parser_test.cpp
│   ├── sema_test.cpp        ← error-case tests
│   └── codegen_test.cpp     ← runs examples, diffs against clang baseline
├── examples/
│   ├── fibonacci.mc
│   ├── sum_of_squares.mc
│   ├── fizzbuzz.mc
│   ├── gcd.mc
│   ├── pointer_swap.mc
│   ├── array_sum.mc
│   ├── struct_point.mc
│   ├── bit_ops.mc
│   └── control_flow.mc
└── docs/
    ├── language_spec.md     ← BNF grammar + type rules
    └── ir_walkthrough.md    ← annotated IR for each example program
```

---

## Supported Language Features

MiniC supports a deliberately constrained subset of C. Every feature in
scope is fully supported; anything outside scope is a clear compile error.

**Types**
`int`, `float`, `char`, `void` (for function return types only), pointers to
any of those (`int *`, `float **`, ...), fixed-size single-dimension arrays
(`int arr[10]`), and named structs/unions/enums (`struct Point`, `union
Number`, `enum Color`), and numeric casts (`(int)`/`(float)`/`(char)`,
right-associative, no pointer/aggregate casts). Not yet supported:
`sizeof`, storage classes, function prototypes. `volatile` is an explicit
non-goal, not a staged feature — it's rejected with a targeted parser
diagnostic.

**Variables and pointers**
Local variable declarations with initializers (`int x = 5;`),
assignment statements (`x = x + 1;`), and use of variables in expressions.
Address-of (`&x`) and dereference (`*p`) work as prefix unary operators,
including assignment through a dereferenced pointer (`*p = 5;`). A pointer
may be compared with `==`/`!=` against another pointer of the same type or
against the literal `0` (a null-pointer constant), and used directly as an
`if`/`while` condition (true when non-null).

**Arrays**
A local variable may be declared with a fixed size (`int arr[10];`) and
indexed for reading or writing (`arr[i] = arr[i] + 1;`). An array decays to
a pointer to its first element wherever it's used as a value — the same
`arr[i]` syntax works whether `arr` is a real array or a pointer parameter
that received a decayed array, and an array can be passed directly where a
pointer parameter is expected. Arrays aren't assignable as a whole and have
no literal-initializer syntax; there's no multi-dimensional array or
pointer-to-array type yet.

**Structs, unions, and enums**
`struct`/`union` declarations with named fields, accessed with `.` (and
`->` for pointers, which desugars to `(*p).field`). Struct/union values are
first-class — local variables, function parameters/returns, and whole-value
assignment (`p2 = p1;`, a real field-by-field copy) all just work, the same
as for `int`. A field's type may reference any other struct/union
regardless of declaration order; only direct by-value self-containment is
rejected (`struct Node { struct Node n; };` — use a pointer field instead).
`enum` declares named `int` constants (`enum Color { RED, GREEN, BLUE };`)
rather than a distinct type. No anonymous structs/unions, nested struct
*definitions*, or struct-literal initializers yet.

**Arithmetic operators**
`+`, `-`, `*`, `/` with standard precedence and associativity.
Integer division truncates toward zero, matching C semantics.

**Comparison and logical operators**
`==`, `!=`, `<`, `>`, `<=`, `>=`, `&&`, `||`, `!`.
All comparisons produce an `int` result (0 or 1), matching C. `&&`/`||`
short-circuit (the right operand's code only runs when needed), and either
operand may be a pointer (`p && p->x`) — not just numeric.

**Bitwise, ternary, increment/decrement, compound assignment, comma**
`&`, `|`, `^`, `~`, `<<`, `>>` (integral operands only, not `float`);
`cond ? a : b`; prefix/postfix `++`/`--`; compound assignment (`+=`, `-=`,
`*=`, `/=`, `&=`, `|=`, `^=`, `<<=`, `>>=` — the target's address is
computed only once, so `arr[f()] += 1` calls `f` exactly once); and the
comma operator, reachable only inside explicit parentheses (`(a, b)`) so it
can never collide with the unrelated comma in call-argument or parameter
lists. No `%`/`%=` (no modulo operator).

**Control flow**
`if`/`else`, `while`, `for`, and `do`-`while` loops, nested freely.
`switch`/`case`/`default` with real fallthrough (compiles to LLVM's native
`switch` instruction — one basic block per case, `break` to exit, no
implicit break between cases, just like C). `goto`/labels, forward or
backward, anywhere in the same function (sema resolves every label before
checking any `goto`, so forward jumps work). `break` exits the innermost
loop *or* switch; `continue` re-enters the innermost loop specifically (a
switch doesn't count, matching C).

**Functions**
Function declarations with typed parameters and return types, function
calls, and `return` statements. Recursive functions are supported because
the IR generator handles forward references correctly.

**Built-in I/O**
`print_int`, `print_float`, `print_char`, and `print_str` are built-in,
fixed-arity print functions — `printf` is deliberately not a builtin,
since as a variadic function it would bypass the strict argument type-
checking every other function call goes through (see
[Implicit Conversions](docs/language_spec.md#implicit-conversions)).
Each maps to a real C definition in
[runtime/print_runtime.c](runtime/print_runtime.c), linked into every
minic-produced binary automatically. None of them append a trailing
newline — call `print_str("\n")` explicitly, same as C's `printf` would
need `\n` spelled out in the format string.

---

## Example MiniC Programs

```c
// examples/fibonacci.mc

int fibonacci(int n) {
    if (n <= 1) {
        return n;
    }
    return fibonacci(n - 1) + fibonacci(n - 2);
}

int main() {
    int i = 0;
    while (i < 10) {
        print_int(fibonacci(i));
        print_str("\n");
        i = i + 1;
    }
    return 0;
}
```

```c
// examples/sum_of_squares.mc

float sum_of_squares(int n) {
    float total = 0.0;
    int i = 1;
    while (i <= n) {
        float fi = i;
        total = total + fi * fi;
        i = i + 1;
    }
    return total;
}

int main() {
    print_float(sum_of_squares(100));
    print_str("\n");
    return 0;
}
```

---

## Pipeline Architecture

```
Source file (.mc)
        │
        ▼
    Lexer                  reads characters, emits a flat stream of tokens
                           (keywords, identifiers, literals, operators)
        │  token stream
        ▼
    Parser                 recursive descent, consumes token stream,
                           builds an Abstract Syntax Tree (AST)
        │  AST
        ▼
    Semantic Analyzer      walks the AST, checks:
                           - all variables declared before use
                           - types are compatible across assignments
                           - function call argument counts match
                           - return type matches function declaration
        │  typed AST
        ▼
    LLVM IR Generator      walks the typed AST, emits LLVM IR using
                           IRBuilder — one IR instruction per AST node
        │  LLVM IR (.ll)
        ▼
    LLVM Backend           invoke llc or clang to compile IR to a
                           native binary or object file
        │
        ▼
    Native Binary
```

Each stage is cleanly separated: the lexer exposes a `TokenStream`, the
parser consumes it and returns an `ASTNode*` tree, the semantic analyzer
decorates that tree with resolved types in a single pass, and the IR
generator walks the decorated tree to emit LLVM IR via `IRBuilder<>`. The
LLVM backend (invoked via `clang`) handles register allocation, instruction
selection, and object linking. The core pipeline is compiled into
`libminic_core.a`, which the test suites link against directly without
going through the CLI.

---

## Pipeline Walkthrough — fibonacci(5)

This is an end-to-end trace showing what each stage of the pipeline does.

**Source**
```c
int fibonacci(int n) {
    if (n <= 1) { return n; }
    return fibonacci(n - 1) + fibonacci(n - 2);
}
```

**After lexing** — a flat list of tokens:
```
TOK_INT  TOK_IDENT("fibonacci")  TOK_LPAREN  TOK_INT  TOK_IDENT("n")
TOK_RPAREN  TOK_LBRACE  TOK_IF  TOK_LPAREN  TOK_IDENT("n")
TOK_LEQ  TOK_NUMBER(1)  ...
```

**After parsing** — an AST:
```
FuncDef(fibonacci, [Param(int, n)], int)
  IfStmt
    BinOp(<=, Ident(n), Number(1))
    Return(Ident(n))
  Return
    BinOp(+,
      Call(fibonacci, [BinOp(-, Ident(n), Number(1))]),
      Call(fibonacci, [BinOp(-, Ident(n), Number(2))]))
```

**After semantic analysis** — the AST is unchanged but every node
carries a resolved type. The analyzer confirms `n` is declared as `int`,
the `<=` comparison operands are both `int`, and the function's return type
matches its declaration.

**After IR generation** — LLVM IR (`-O0`):
```llvm
define i32 @fibonacci(i32 %n) {
entry:
  ; alloca+store+load is LLVM's canonical pattern for mutable locals before mem2reg.
  %n1 = alloca i32, align 4
  store i32 %n, ptr %n1, align 4
  %n2 = load i32, ptr %n1, align 4
  %letmp    = icmp sle i32 %n2, 1
  %cmptoint = zext i1 %letmp to i32
  %booltmp  = icmp ne i32 %cmptoint, 0
  br i1 %booltmp, label %if.then, label %if.end

if.then:
  %n3 = load i32, ptr %n1, align 4
  ret i32 %n3

if.end:
  %n4      = load i32, ptr %n1, align 4
  %subtmp  = sub i32 %n4, 1
  %calltmp = call i32 @fibonacci(i32 %subtmp)
  %n5      = load i32, ptr %n1, align 4
  %subtmp6 = sub i32 %n5, 2
  %calltmp7 = call i32 @fibonacci(i32 %subtmp6)
  %addtmp  = add i32 %calltmp, %calltmp7
  ret i32 %addtmp
}
```

The `if.then` / `if.end` block names come from the code generator; every
`if` statement produces exactly two target blocks. At `-O2`, `mem2reg`
eliminates all the `alloca`/`store`/`load` chains, `instcombine` folds the
double-comparison into a single `icmp slt`, and the optimizer converts the
`fibonacci(n-2)` recursion into a loop (see [docs/ir_walkthrough.md](docs/ir_walkthrough.md)).

**After LLVM backend** — a native binary that runs directly on the CPU.

---

## Testing & Validation

Every pipeline stage has its own test executable, built with nothing more
than `<cassert>` — no external test framework, so the build stays hermetic
and `ctest` is the only runner a contributor needs. **All 5 suites pass.**

### Test suite results

```
$ ctest --test-dir build --output-on-failure
Test project /path/to/c-compiler/build

      Start 1: lexer_test
  1/5 Test  #1: lexer_test    ........  Passed    0.06 sec

      Start 2: smoke_test
  2/5 Test  #2: smoke_test    ........  Passed    0.32 sec

      Start 3: parser_test
  3/5 Test  #3: parser_test   ........  Passed    0.04 sec

      Start 4: sema_test
  4/5 Test  #4: sema_test     ........  Passed    0.05 sec

      Start 5: codegen_test
  5/5 Test  #5: codegen_test  ........  Passed    0.98 sec

100% tests passed, 0 tests failed out of 5
Total Test time (real) =   1.47 sec
```

### What each suite covers

| Suite | Stage tested | What it checks |
|---|---|---|
| `lexer_test` | Lexer | Token stream shape for keywords, operators, numeric/string/char literals, escape sequences, and block/line comments |
| `parser_test` | Parser | AST node shape and nesting for every grammar construct; error message text for malformed input |
| `sema_test` | Semantic analyzer | Every diagnostic the type checker can produce — undeclared identifiers, type mismatches, wrong argument counts, return-type mismatches |
| `codegen_test` | Full pipeline | Compiles and **runs** all nine example programs end-to-end, asserting on exact stdout against a known-good golden output |
| `smoke_test` | CLI | End-to-end sanity check: `--emit-tokens`, `--emit-ast`, `--emit-ir`, and binary compilation all exit cleanly |

### Output correctness — cross-validation against clang

Every example program is compiled twice — once through `minic`, once through
`clang -x c` on the same `.mc` source — and the two binaries' output is
diffed. All nine programs produce byte-for-byte identical output:

```
Program           minic vs clang
──────────────────────────────────
fibonacci         IDENTICAL ✓
fizzbuzz          IDENTICAL ✓
gcd               IDENTICAL ✓
sum_of_squares    IDENTICAL ✓
pointer_swap      IDENTICAL ✓
array_sum         IDENTICAL ✓
struct_point      IDENTICAL ✓
bit_ops           IDENTICAL ✓
control_flow      IDENTICAL ✓
```

### Bug hunt and hardening

A targeted correctness review across all four pipeline stages turned up —
and fixed — four real defects:

| # | Bug | Root cause | Fix |
|---|---|---|---|
| 1 | Double, misordered diagnostics on bad assignments | `checkAssign` evaluated the RHS before checking whether the LHS was even declared, so `x = y;` with both undeclared printed the `y` error before the (more important) `x` error | Check the assignment target first; only evaluate the RHS afterward |
| 2 | Temp `.ll` file leaked on write failure | `compileToNative` only deleted its temp file after a successful `clang` invocation — an `ofstream` open/write failure threw before cleanup ran | Delete the temp file on every throwing path, not just the success path |
| 3 | `1.` and `1e5` failed to lex as floats | The lexer only recognized a `.` followed by a digit, with no exponent handling at all | Accept a bare trailing `.` and `[eE][+-]?[0-9]+` exponents in `lexNumber` |
| 4 | sema/codegen type mismatch on `-charVar` | Sema typed unary negation on `char` as `char`; codegen sign-extended to `int` before negating and returned `int` — the two stages disagreed about the expression's type | Sema now returns `int` for negation on any non-float operand, matching codegen and C's integer-promotion rule |

Each fix was verified with a hand-constructed regression case before being
folded back into the full test run above — all 5 suites and all 9
clang-diff comparisons still pass after each fix.

---

## Optimization

Passing `-O1`, `-O2`, or `-O3` runs LLVM's new-pass-manager pipeline
(`PassBuilder::buildPerModuleDefaultPipeline`) over the generated IR before
handing it to the backend. The same optimized IR is shown by `--emit-ir`.

Key transformations applied at `-O2` to `fibonacci.mc`:

| Pass | Effect on `fibonacci.mc` |
|---|---|
| `mem2reg` | Eliminates all `alloca`/`store`/`load` pairs; locals become SSA registers |
| `instcombine` | Folds `zext i1 → i32; icmp ne, 0` into a single comparison |
| `simplifycfg` | Merges the two `return` blocks into one with a PHI node |
| `tailcallelim` | Converts the `fibonacci(n-2)` branch into a loop with an accumulator |
| `loop-unroll` | Unrolls `main`'s fixed-count while loop into 10 straight-line calls |

**Benchmark — `fibonacci(40)` on Apple M-series:**

| Flag | Runtime | Speedup |
|---|---|---|
| `-O0` | 0.39 s | baseline |
| `-O2` | 0.28 s | **1.4×** |

See [docs/ir_walkthrough.md](docs/ir_walkthrough.md) for annotated before/after
IR listings explaining each transformation.

---

## Error Messages

A compiler is only as good as its error messages. MiniC reports errors as a
single `file:line:col: error: message` line — no source-line snippet or
caret, just a precise location and a clear description:

```
fibonacci.mc:2:9: error: use of undeclared variable 'nn'
fibonacci.mc:8:5: error: return type mismatch — expected 'int', got 'float'
fibonacci.mc:12:20: error: wrong number of arguments to 'fibonacci' — expected 1, got 2
```

---

## Build & Run

**Dependencies:** LLVM 17+, CMake 3.20+, a C++20 compiler, Ninja (recommended).

```bash
# macOS
brew install llvm cmake ninja

# Ubuntu
sudo apt install llvm cmake ninja-build
```

### Configure & build

Homebrew LLVM is installed locally at `/opt/homebrew/opt/llvm`. If
`llvm-config` is not on PATH, `scripts/configure.sh` still uses the
Homebrew path automatically.

```bash
./scripts/configure.sh        # configures with Ninja + correct LLVM_DIR
cmake --build build           # compiles libminic_core.a, minic, and all tests
ctest --test-dir build --output-on-failure
```

Manual configure command:

```bash
cmake -S . -B build -G Ninja \
  -DLLVM_DIR="$(/opt/homebrew/opt/llvm/bin/llvm-config --cmakedir)"
```

### CLI usage

```bash
# Dump the token stream (lexer output)
./build/minic examples/fibonacci.mc --emit-tokens

# Dump the AST (parser output)
./build/minic examples/fibonacci.mc --emit-ast

# Dump LLVM IR (no optimization)
./build/minic examples/fibonacci.mc --emit-ir

# Dump LLVM IR after the -O2 pass pipeline
./build/minic examples/fibonacci.mc -O2 --emit-ir

# Compile to a native binary (default: -O0)
./build/minic examples/gcd.mc -o gcd

# Compile with LLVM -O2 optimization pipeline
./build/minic examples/fibonacci.mc -O2 -o fibonacci_o2

# Compare output against clang for correctness
clang -x c examples/fibonacci.mc -o fibonacci_clang
diff <(./fibonacci_o2) <(./fibonacci_clang)
```

---

## License

This project is licensed under the MIT License — see [LICENSE](LICENSE) for the full text.

---

## References

**Compiler theory**
- Aho, Lam, Sethi, Ullman. *Compilers: Principles, Techniques, and Tools* (2nd ed., "Dragon Book"). Addison-Wesley, 2006. — The standard reference for lexer/parser/semantic-analysis theory; Chapter 6 covers intermediate code generation.
- Cooper, Torczon. *Engineering a Compiler* (3rd ed.). Morgan Kaufmann, 2022. — A modern alternative with clearer SSA and optimization coverage; Chapter 9 covers register allocation.
- Appel. *Modern Compiler Implementation in C*. Cambridge University Press, 1998. — Compact treatment of tree-walking IR generation close to what MiniC does.

**LLVM**
- LLVM Project. [LLVM Language Reference Manual](https://llvm.org/docs/LangRef.html). — Definitive reference for LLVM IR types, instructions, and calling conventions.
- LLVM Project. [LLVM Programmer's Manual](https://llvm.org/docs/ProgrammersManual.html). — Guide to the C++ API used in the IR generator (`IRBuilder<>`, `Module`, `Function`, `BasicBlock`).
- LLVM Project. [Writing an LLVM Pass](https://llvm.org/docs/WritingAnLLVMPass.html). — Background on the new pass manager used for `-O1`/`-O2`/`-O3`.
- Lattner, Adve. "LLVM: A Compilation Framework for Lifelong Program Analysis & Transformation." *CGO 2004*. — The original paper introducing LLVM's design philosophy.

**Recursive-descent parsing**
- Grune, Jacobs. *Parsing Techniques: A Practical Guide* (2nd ed.). Springer, 2008. — Chapter 6 covers recursive-descent and LL parsing in depth.
- Nystrom. [*Crafting Interpreters*](https://craftinginterpreters.com). Free online. — Excellent walkthrough of hand-writing a recursive-descent parser and tree-walking evaluator; closely mirrors MiniC's parser structure.

**C language semantics**
- ISO/IEC 9899:2018. *Programming Languages — C* (C17 standard). — The normative reference for type promotion rules (§6.3), integer arithmetic (§6.5), and pointer semantics (§6.3.2) that MiniC's semantic analyzer enforces.
