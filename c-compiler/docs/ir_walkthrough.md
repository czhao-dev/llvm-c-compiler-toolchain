# LLVM IR Walkthrough

This document shows the full pipeline for two example programs and explains
every LLVM IR instruction produced by the MiniC code generator. The final
section of each program shows what LLVM's `-O2` pass pipeline transforms
that IR into.

---

## fibonacci.mc

**Source**

```c
int fibonacci(int n) {
    if (n <= 1) { return n; }
    return fibonacci(n - 1) + fibonacci(n - 2);
}

int main() {
    int i = 0;
    while (i < 10) {
        printf("%d\n", fibonacci(i));
        i = i + 1;
    }
    return 0;
}
```

**Token stream** (`--emit-tokens`, fibonacci function only)

The lexer produces one token per terminal. Each line is
`line:col  TOKEN_TYPE  "lexeme"`:

```
1:1  TOK_INT       "int"
1:5  TOK_IDENT     "fibonacci"
1:14 TOK_LPAREN    "("
1:15 TOK_INT       "int"
1:19 TOK_IDENT     "n"
1:20 TOK_RPAREN    ")"
1:22 TOK_LBRACE    "{"
2:5  TOK_IF        "if"
2:8  TOK_LPAREN    "("
2:9  TOK_IDENT     "n"
2:11 TOK_LEQ       "<="
2:14 TOK_INT_LIT   "1"
2:15 TOK_RPAREN    ")"
2:17 TOK_LBRACE    "{"
3:9  TOK_RETURN    "return"
3:16 TOK_IDENT     "n"
3:17 TOK_SEMI      ";"
4:5  TOK_RBRACE    "}"
5:5  TOK_RETURN    "return"
5:12 TOK_IDENT     "fibonacci"
5:21 TOK_LPAREN    "("
5:22 TOK_IDENT     "n"
5:24 TOK_MINUS     "-"
5:26 TOK_INT_LIT   "1"
5:27 TOK_RPAREN    ")"
5:29 TOK_PLUS      "+"
5:31 TOK_IDENT     "fibonacci"
5:40 TOK_LPAREN    "("
5:41 TOK_IDENT     "n"
5:43 TOK_MINUS     "-"
5:45 TOK_INT_LIT   "2"
5:46 TOK_RPAREN    ")"
5:47 TOK_SEMI      ";"
6:1  TOK_RBRACE    "}"
```

Every character in the source appears as exactly one token. The lexer
distinguishes `<=` (TOK_LEQ) from `<` (TOK_LT) by looking one character
ahead. Keywords (`int`, `if`, `return`) are recognised by a hash-map lookup
after the identifier is consumed.

**AST** (`--emit-ast`, fibonacci function only)

The parser builds one AST node per grammar construct, connected by
`std::unique_ptr` ownership:

```
FuncDef fibonacci -> int
  Param int n
  Block
    If
      Cond
        BinOp <=
          Ident n
          IntLit 1
      Then
        Block
          Return
            Ident n
    Return
      BinOp +
        Call fibonacci
          BinOp -
            Ident n
            IntLit 1
        Call fibonacci
          BinOp -
            Ident n
            IntLit 2
```

Each `BinOp` node owns its two children; each `Call` node owns its argument
list. The `If` node has a `Cond` subtree and a `Then` block but no `Else`
block — the else branch is absent in the source. Nesting depth in the
printout reflects ownership depth in the tree.

**Semantic analysis**

The semantic analyzer walks the AST and confirms:
- `fibonacci` is declared before it is called (forward-reference handling
  collects all function signatures in a first pass)
- `n` is in scope for every `Ident n` node
- both operands of `<=` are `int`; result is `int`
- both operands of `+` are `int` (the return type of each `Call fibonacci`
  is `int`); result is `int`
- the return type of both `Return` statements is `int`, matching the
  function's declared return type

No warnings are emitted for this function.

**Generated IR (unoptimized, `-O0`)**

```llvm
; ModuleID = 'examples/fibonacci.mc'
source_filename = "examples/fibonacci.mc"
target triple = "arm64-apple-darwin25.5.0"

; Format string for printf — stored as a global constant.
@0 = private unnamed_addr constant [4 x i8] c"%d\0A\00", align 1

; printf is an external variadic function — declared but not defined.
declare i32 @printf(ptr, ...)

define i32 @fibonacci(i32 %n) {
entry:
  ; alloca reserves stack space for the parameter 'n'.
  ; MiniC always allocates a stack slot for each parameter so they can be
  ; reassigned (standard LLVM alloca+store+load idiom before mem2reg).
  %n1 = alloca i32, align 4
  store i32 %n, ptr %n1, align 4       ; spill the incoming argument to the slot

  ; Evaluate 'n <= 1': load n, compare, widen i1 → i32, re-narrow to i1.
  ; The zext+icmp pair is slightly redundant; mem2reg + instcombine will clean it up.
  %n2 = load i32, ptr %n1, align 4
  %letmp    = icmp sle i32 %n2, 1      ; signed <=
  %cmptoint = zext i1 %letmp to i32    ; MiniC represents booleans as i32 (C convention)
  %booltmp  = icmp ne i32 %cmptoint, 0 ; convert back to i1 for the branch
  br i1 %booltmp, label %if.then, label %if.end

if.then:
  ; Base case: return n.
  %n3 = load i32, ptr %n1, align 4
  ret i32 %n3

if.end:
  ; Recursive case: fibonacci(n-1) + fibonacci(n-2).
  %n4      = load i32, ptr %n1, align 4
  %subtmp  = sub i32 %n4, 1
  %calltmp = call i32 @fibonacci(i32 %subtmp)   ; fibonacci(n-1)
  %n5      = load i32, ptr %n1, align 4
  %subtmp6 = sub i32 %n5, 2
  %calltmp7 = call i32 @fibonacci(i32 %subtmp6) ; fibonacci(n-2)
  %addtmp  = add i32 %calltmp, %calltmp7
  ret i32 %addtmp
}

define i32 @main() {
entry:
  ; int i = 0;
  %i = alloca i32, align 4
  store i32 0, ptr %i, align 4
  br label %while.cond             ; jump to the loop condition

while.cond:
  ; while (i < 10)
  %i1      = load i32, ptr %i, align 4
  %lttmp   = icmp slt i32 %i1, 10
  %cmptoint = zext i1 %lttmp to i32
  %booltmp = icmp ne i32 %cmptoint, 0
  br i1 %booltmp, label %while.body, label %while.end

while.body:
  ; printf("%d\n", fibonacci(i));
  %i2      = load i32, ptr %i, align 4
  %calltmp = call i32 @fibonacci(i32 %i2)
  %calltmp3 = call i32 (ptr, ...) @printf(ptr @0, i32 %calltmp)

  ; i = i + 1;
  %i4      = load i32, ptr %i, align 4
  %addtmp  = add i32 %i4, 1
  store i32 %addtmp, ptr %i, align 4
  br label %while.cond             ; back edge

while.end:
  ret i32 0
}
```

Key things to notice in the unoptimized IR:

- Every local variable gets an `alloca` slot and is accessed via `store`/`load`.
  This is LLVM's canonical approach — the `mem2reg` pass promotes these to SSA
  registers during optimization.
- The `if` produces two blocks (`if.then`, `if.end`) with control flow via
  `br i1`. There is no phi node because each path returns immediately.
- The `while` loop produces three blocks: `while.cond` (condition + branch),
  `while.body` (body + back edge), `while.end` (exit).
- Each comparison goes through `zext i1 → i32` then `icmp ne i32, 0` because
  MiniC represents boolean results as `int` (matching C semantics). This double
  conversion is redundant and gets folded by `instcombine` during optimization.

**Generated IR (optimized, `-O2`)**

```llvm
; ModuleID = 'examples/fibonacci.mc'
source_filename = "examples/fibonacci.mc"
target triple = "arm64-apple-darwin25.5.0"

@0 = private unnamed_addr constant [4 x i8] c"%d\0A\00", align 1

; nofree/nounwind/local_unnamed_addr — attributes added by the optimizer
; signaling this function has no side effects on memory (beyond the call itself).
declare noundef i32 @printf(ptr noundef readonly captures(none), ...) local_unnamed_addr #0

; memory(none): the optimizer proved fibonacci reads/writes no memory
; (all its "variables" are now SSA registers after mem2reg).
define i32 @fibonacci(i32 %n) local_unnamed_addr #1 {
entry:
  ; mem2reg eliminated all alloca/store/load pairs — %n is used directly.
  ; instcombine simplified 'n <= 1' (zext+icmp) into a single 'icmp slt n, 2'.
  %letmp11 = icmp slt i32 %n, 2      ; n < 2  ≡  n <= 1  for integers
  br i1 %letmp11, label %common.ret, label %if.end

common.ret:
  ; PHI node merges the base-case return (from entry) and the loop exit.
  ; This single return block replaces two separate 'ret' instructions.
  %accumulator.tr.lcssa = phi i32 [ 0, %entry ], [ %addtmp, %if.end ]
  %n.tr.lcssa           = phi i32 [ %n, %entry ], [ %subtmp6, %if.end ]
  ; On the base case path: n + 0 = n (the original 'return n').
  ; On the loop-exit path: last fibonacci(n-1) result + accumulated sum.
  %accumulator.ret.tr = add i32 %n.tr.lcssa, %accumulator.tr.lcssa
  ret i32 %accumulator.ret.tr

if.end:
  ; The optimizer converted the fibonacci(n-2) recursion into a loop.
  ; Each iteration: accumulate fibonacci(n-1), then decrement n by 2 and loop.
  ; This is "accumulator-style" tail-call optimization applied to one branch.
  %n.tr13          = phi i32 [ %subtmp6, %if.end ], [ %n, %entry ]
  %accumulator.tr12 = phi i32 [ %addtmp,  %if.end ], [ 0,  %entry ]
  %subtmp  = add nsw i32 %n.tr13, -1          ; n - 1
  %calltmp = tail call i32 @fibonacci(i32 %subtmp)  ; still recurses for (n-1)
  %subtmp6 = add nsw i32 %n.tr13, -2          ; n - 2  (becomes next loop's n)
  %addtmp  = add i32 %calltmp, %accumulator.tr12    ; accumulate
  ; Loop exit condition: once n < 4, the next fibonacci(n-2) would be a
  ; base case, so jump to common.ret to add the final base case value.
  %letmp   = icmp samesign ult i32 %n.tr13, 4
  br i1 %letmp, label %common.ret, label %if.end
}

; main is fully unrolled: the while (i < 10) loop over fibonacci(0)..fibonacci(9)
; is replaced with 10 straight-line tail calls. No loop overhead, no branch.
define noundef i32 @main() local_unnamed_addr #0 {
entry:
  %calltmp   = tail call i32 @fibonacci(i32 0)
  %calltmp3  = tail call i32 (ptr, ...) @printf(ptr nonnull dereferenceable(1) @0, i32 %calltmp)
  %calltmp.1 = tail call i32 @fibonacci(i32 1)
  %calltmp3.1 = tail call i32 (ptr, ...) @printf(ptr nonnull dereferenceable(1) @0, i32 %calltmp.1)
  ; ... (calls for i=2 through i=9 follow the same pattern)
  %calltmp.9 = tail call i32 @fibonacci(i32 9)
  %calltmp3.9 = tail call i32 (ptr, ...) @printf(ptr nonnull dereferenceable(1) @0, i32 %calltmp.9)
  ret i32 0
}

attributes #0 = { nofree nounwind }
attributes #1 = { nofree nosync nounwind memory(none) }
```

What the `-O2` pipeline changed and why:

| Transformation | Before | After | Pass responsible |
|---|---|---|---|
| Eliminate alloca/store/load | 5 loads of `%n` across the function | `%n` used directly as SSA value | `mem2reg` |
| Simplify boolean round-trip | `zext i1 → i32` then `icmp ne i32, 0` | single `icmp slt i32 %n, 2` | `instcombine` |
| Merge return blocks | Two `ret` instructions | One `ret` with PHI | `simplifycfg` |
| Convert `fibonacci(n-2)` recursion to loop | Two recursive calls per invocation | One recursive call + loop with accumulator | `tailcallelim` / `loop-rotate` |
| Unroll `main`'s while loop | Loop with back edge and condition block | 10 straight-line tail calls | `loop-unroll` |
| Mark `fibonacci` memory-free | No attributes | `memory(none)` | alias analysis |

---

## Benchmark: fibonacci(40)

Compiled from the same source, measured with `time ./binary`:

| Flag | Runtime | Relative |
|---|---|---|
| `-O0` | 0.39 s | baseline |
| `-O2` | 0.28 s | **1.4× faster** |

The speedup comes from two sources: the inner loop in `fibonacci` eliminates
one level of the recursive tree (all the `fibonacci(n-2)` calls become
iterations instead of call frames), and `main`'s loop is unrolled so the
branch predictor never sees the loop-exit condition at all.

---

## sum_of_squares.mc

**Source**

```c
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
    printf("%f\n", sum_of_squares(100));
    return 0;
}
```

**AST** (`--emit-ast`, sum_of_squares function)

```
FuncDef sum_of_squares -> float
  Param int n
  Block
    VarDecl float total
      FloatLit 0
    VarDecl int i
      IntLit 1
    While
      Cond
        BinOp <=
          Ident i
          Ident n
      Body
        Block
          VarDecl float fi
            Ident i
          Assign total
            BinOp +
              Ident total
              BinOp *
                Ident fi
                Ident fi
          Assign i
            BinOp +
              Ident i
              IntLit 1
    Return
      Ident total
```

**Generated IR (unoptimized, `-O0`)**

```llvm
; ModuleID = 'examples/sum_of_squares.mc'
source_filename = "examples/sum_of_squares.mc"
target triple = "arm64-apple-darwin25.5.0"

; Format string for printf — "%f\n" stored as a 4-byte global.
@0 = private unnamed_addr constant [4 x i8] c"%f\0A\00", align 1

declare i32 @printf(ptr, ...)

define float @sum_of_squares(i32 %n) {
entry:
  ; Allocate stack slots for all locals and the parameter.
  %fi    = alloca float, align 4    ; loop-body local
  %i     = alloca i32,   align 4    ; loop counter
  %total = alloca float, align 4    ; accumulator
  %n1    = alloca i32,   align 4    ; copy of the parameter
  store i32 %n, ptr %n1, align 4

  ; float total = 0.0;  int i = 1;
  store float 0.000000e+00, ptr %total, align 4
  store i32 1, ptr %i, align 4
  br label %while.cond

while.cond:
  ; while (i <= n)
  %i2    = load i32, ptr %i,  align 4
  %n3    = load i32, ptr %n1, align 4
  %letmp = icmp sle i32 %i2, %n3   ; signed <=
  %cmptoint = zext i1 %letmp to i32
  %booltmp  = icmp ne i32 %cmptoint, 0
  br i1 %booltmp, label %while.body, label %while.end

while.body:
  ; float fi = i;  (int → float conversion)
  %i4        = load i32, ptr %i, align 4
  %sitofptmp = sitofp i32 %i4 to float   ; Sign-extend Int To Float
  store float %sitofptmp, ptr %fi, align 4

  ; total = total + fi * fi;
  %total5 = load float, ptr %total, align 4
  %fi6    = load float, ptr %fi, align 4
  %fi7    = load float, ptr %fi, align 4
  %multmp = fmul float %fi6, %fi7         ; floating-point multiply
  %addtmp = fadd float %total5, %multmp   ; floating-point add
  store float %addtmp, ptr %total, align 4

  ; i = i + 1;
  %i8      = load i32, ptr %i, align 4
  %addtmp9 = add i32 %i8, 1
  store i32 %addtmp9, ptr %i, align 4
  br label %while.cond                    ; back edge

while.end:
  %total10 = load float, ptr %total, align 4
  ret float %total10
}

define i32 @main() {
entry:
  ; sum_of_squares returns float; printf receives it as double (C ABI vararg rule).
  %calltmp      = call float @sum_of_squares(i32 100)
  %printfdouble = fpext float %calltmp to double   ; float → double for vararg
  %calltmp1     = call i32 (ptr, ...) @printf(ptr @0, double %printfdouble)
  ret i32 0
}
```

Key things to notice:

- `int i` uses `alloca i32` while `float total` uses `alloca float` — the
  LLVM type matches the MiniC declared type exactly.
- `float fi = i;` becomes `sitofp i32 → float` — "Sign-extend Int To Float."
  MiniC emits an explicit conversion instruction whenever an integer value is
  stored into a float variable.
- `total + fi * fi` becomes `fmul float` then `fadd float` — the floating-point
  variants of multiply and add. Integer arithmetic would use `mul`/`add` instead.
- `printf` receives `sum_of_squares`'s `float` result as a `double` because
  the C ABI requires float varargs to be promoted to double. MiniC emits an
  `fpext float → double` instruction before the call.

**Generated IR (optimized, `-O2`)**

```llvm
; ModuleID = 'examples/sum_of_squares.mc'
source_filename = "examples/sum_of_squares.mc"
target triple = "arm64-apple-darwin25.5.0"

@0 = private unnamed_addr constant [4 x i8] c"%f\0A\00", align 1

; nofree nounwind — no memory side effects on the printf declaration.
declare noundef i32 @printf(ptr noundef readonly captures(none), ...) local_unnamed_addr #0

; memory(none): optimizer proved sum_of_squares touches no memory
; (all locals promoted to SSA registers by mem2reg).
define float @sum_of_squares(i32 %n) local_unnamed_addr #1 {
entry:
  ; If n < 1, the while condition is immediately false — skip straight to exit.
  %letmp.not13 = icmp slt i32 %n, 1
  br i1 %letmp.not13, label %while.end, label %while.body

while.body:
  ; PHI nodes replace alloca+store+load for every loop variable.
  %i.015     = phi i32   [ %addtmp9, %while.body ], [ 1,            %entry ]
  %total.014 = phi float [ %addtmp,  %while.body ], [ 0.000000e+00, %entry ]

  ; float fi = i;  then  total = total + fi * fi;
  %sitofptmp = sitofp i32 %i.015 to float   ; i → fi
  %multmp    = fmul float %sitofptmp, %sitofptmp  ; fi * fi (load eliminated)
  %addtmp    = fadd float %total.014, %multmp     ; total + fi*fi

  ; i = i + 1;
  %addtmp9 = add i32 %i.015, 1

  ; Loop exit check: was this the last iteration?
  %letmp.not = icmp sgt i32 %addtmp9, %n
  br i1 %letmp.not, label %while.end, label %while.body

while.end:
  ; PHI selects 0.0 (empty-loop path) or the accumulated total.
  %total.0.lcssa = phi float [ 0.000000e+00, %entry ], [ %addtmp, %while.body ]
  ret float %total.0.lcssa
}

; main is fully inlined at -O2: sum_of_squares(100) is unrolled into
; a single loop that runs directly inside main, with no call overhead.
define noundef i32 @main() local_unnamed_addr #0 {
entry:
  br label %while.body.i

while.body.i:
  %i.015.i     = phi i32   [ %addtmp9.i, %while.body.i ], [ 1,            %entry ]
  %total.014.i = phi float [ %addtmp.i,  %while.body.i ], [ 0.000000e+00, %entry ]
  %sitofptmp.i = uitofp nneg i32 %i.015.i to float
  %multmp.i    = fmul float %sitofptmp.i, %sitofptmp.i
  %addtmp.i    = fadd float %total.014.i, %multmp.i
  %addtmp9.i   = add nuw nsw i32 %i.015.i, 1
  %letmp.not.i = icmp samesign ugt i32 %i.015.i, 99   ; loop 100 times
  br i1 %letmp.not.i, label %sum_of_squares.exit, label %while.body.i

sum_of_squares.exit:
  %printfdouble = fpext float %addtmp.i to double
  %calltmp1 = tail call i32 (ptr, ...) @printf(ptr nonnull dereferenceable(1) @0, double %printfdouble)
  ret i32 0
}

attributes #0 = { nofree nounwind }
attributes #1 = { nofree norecurse nosync nounwind memory(none) }
```

What the `-O2` pipeline changed and why:

| Transformation | Before | After | Pass responsible |
|---|---|---|---|
| Eliminate alloca/store/load | `%total`, `%i`, `%fi`, `%n1` all on the stack | All promoted to SSA PHI nodes | `mem2reg` |
| Eliminate redundant `fi` loads | `%fi` loaded twice for `fi * fi` | Single `%sitofptmp` used twice | `instcombine` |
| Simplify boolean round-trip | `zext i1 → i32; icmp ne i32, 0` | Single `icmp sgt`/`slt` | `instcombine` |
| Inline `sum_of_squares` into `main` | `call float @sum_of_squares(i32 100)` | Loop body copied directly into `main` | `inline` |
| Mark function memory-free | No attributes | `memory(none)`, `norecurse` | alias analysis |
