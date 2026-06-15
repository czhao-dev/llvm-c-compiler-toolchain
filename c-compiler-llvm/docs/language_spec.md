# MiniC Language Spec

This file documents the grammar implemented by the lexer (`src/lexer.cpp`)
and the recursive-descent parser (`src/parser.cpp`), together with the type
rules enforced by the semantic analyzer (`src/sema.cpp`).

## Grammar

```bnf
program      ::= (aggregate_decl | enum_decl | function)*
function     ::= type identifier "(" params? ")" block
params       ::= param ("," param)*
param        ::= type identifier

aggregate_decl ::= ("struct" | "union") identifier "{" field_decl* "}" ";"
field_decl     ::= type identifier ("[" int_lit "]")? ";"

enum_decl    ::= "enum" identifier "{" enumerator ("," enumerator)* "}" ";"
enumerator   ::= identifier ("=" int_lit)?

block        ::= "{" statement* "}"

statement    ::= var_decl
               | assign_stmt
               | expr_stmt
               | if_stmt
               | while_stmt
               | for_stmt
               | do_while_stmt
               | switch_stmt
               | goto_stmt
               | label_stmt
               | return_stmt
               | break_stmt
               | continue_stmt

var_decl     ::= type identifier ("[" int_lit "]")? ("=" expression)? ";"
assign_stmt  ::= lvalue assign_op expression ";"
assign_op    ::= "=" | "+=" | "-=" | "*=" | "/=" | "&=" | "|=" | "^=" | "<<=" | ">>="
// A call or ++/-- used for its side effect; any other bare expression
// statement (e.g. a stray `1 + 2;`) is a parse error, not silently a no-op.
expr_stmt    ::= (identifier "(" args? ")" | unary) ";"
lvalue       ::= identifier | "*" unary | postfix "[" expression "]" | postfix "." identifier

if_stmt      ::= "if" "(" expression ")" block ("else" (if_stmt | block))?
while_stmt   ::= "while" "(" expression ")" block
for_stmt     ::= "for" "(" for_init? ";" expression? ";" simple_stmt_no_semi? ")" block
for_init     ::= var_decl_no_semi | simple_stmt_no_semi
// Used by for_init/the for-loop update clause: an assignment (any
// assign_op) or a bare expression — unlike a top-level expr_stmt, any
// expression is allowed here (e.g. `for (...; ...; i)` is legal, if
// pointless), matching C.
simple_stmt_no_semi ::= lvalue assign_op expression | expression

do_while_stmt ::= "do" block "while" "(" expression ")" ";"
switch_stmt  ::= "switch" "(" expression ")" "{" switch_item* "}"
// case/default labels are only recognized directly in a switch's body —
// not nested inside an if/while/etc. within it (no "Duff's device").
switch_item  ::= ("case" "-"? int_lit ":" | "default" ":" | statement)
goto_stmt    ::= "goto" identifier ";"
label_stmt   ::= identifier ":"

return_stmt  ::= "return" expression? ";"
break_stmt   ::= "break" ";"  // exits the innermost loop OR switch
continue_stmt ::= "continue" ";"  // re-enters the innermost loop only

type         ::= ("int" | "float" | "char" | "void"
                  | "struct" identifier | "union" identifier | "enum" identifier) "*"*
```

A `struct`/`union`/`enum` tag reference (e.g. `struct Point` in `struct Point p;`
or `struct Point makeOrigin()`) must name an already-declared aggregate —
see [Structs, Unions, and Enums](#structs-unions-and-enums) below. Note
that struct/union/enum *declarations* are top-level only, like functions;
they can't be declared inside a function body.

### Concrete Examples

**program** — a complete MiniC source file is a sequence of function
definitions. There is no top-level statement syntax; all code lives inside
functions.

```c
int square(int n) { return n * n; }

int main() {
    printf("%d\n", square(7));
    return 0;
}
```

**function** — a return type, a name, a parenthesised parameter list, and
a braced body block.

```c
int add(int a, int b) { return a + b; }
void greet(char c) { printf("%c\n", c); }
```

**aggregate_decl / field_decl** — a `struct` or `union` tag name and a
braced list of `type name;` fields. Top-level only, like a function; a
field's type may reference any other struct/union regardless of
declaration order (only a direct-self-reference by value is rejected — see
[Structs, Unions, and Enums](#structs-unions-and-enums)).

```c
struct Point {
    int x;
    int y;
};

union Number {
    int i;
    float f;
};
```

**enum_decl / enumerator** — a tag name and a comma-separated list of
names, each optionally assigned an explicit `int` value; an enumerator
without `= value` is one more than the previous (or `0` for the first).

```c
enum Color {
    RED,        // 0
    GREEN,      // 1
    BLUE = 10,  // 10
    INDIGO      // 11
};
```

**params / param** — a comma-separated list of `type name` pairs. The
parameter list may be empty.

```c
// two params
int clamp(int val, int lo, int hi) { ... }

// no params
int zero() { return 0; }
```

**block** — a brace-delimited sequence of zero or more statements. Every
`if`/`else`/`while`/`for` body is a block, even when it contains a single
statement.

```c
{
    int x = 5;
    x = x + 1;
    return x;
}
```

**var_decl** — declares a local variable in the current scope. The
initializer is required for `float` when a value is needed immediately; it
is optional for all types. A declaration without an initializer leaves the
variable uninitialized; reading it before assignment is undefined behavior,
as in C.

```c
int count = 0;
float ratio = 3.14;
char c;            // uninitialized; safe only if assigned before first read
int values[5];     // a fixed-size array; no initializer syntax for arrays
```

A declarator may also carry a single `[size]` suffix, making it a
fixed-size array instead of a scalar. `size` must be a positive integer
literal (not a general constant expression). Arrays have no initializer
syntax (no `{1, 2, 3}` literal); fill one with a loop after declaring it.

**assign_stmt** — assigns a new value to an lvalue: either an already-declared
variable, or a dereferenced pointer (`*p`). The left-hand side is checked
first, then the right-hand side is evaluated.

```c
x = x + 1;
total = total + fi * fi;
*p = 5;       // assigns through a pointer
arr[2] = 5;   // assigns through an array/pointer index
p.x = 5;      // assigns through a struct/union field
pp->x = 5;    // assigns through a field of a pointed-to struct
```

An array variable itself is **not** assignable as a whole (`arr = other;`
is an error, matching C) — only individual elements via `arr[i] = ...`. A
struct or union variable, in contrast, **is** assignable as a whole
(`p2 = p1;` copies every field) — see
[Structs, Unions, and Enums](#structs-unions-and-enums).

`assign_op` also includes the compound forms `+= -= *= /= &= |= ^= <<= >>=`.
`target op= value` means `target = target op value`, except `target`'s
*address* is only computed once — this matters when it has a side effect,
e.g. `arr[f()] += 1` calls `f` exactly once, not twice.

```c
total += fi * fi;
x &= mask;
arr[i] <<= 1;
```

**expr_stmt** — a function call or `++`/`--` used as a statement; a call's
return value is discarded. Any *other* bare expression statement (e.g. a
stray `1 + 2;`) is a parse error rather than a silent no-op — MiniC assumes
you meant an assignment.

```c
printf("%d\n", fibonacci(i));
i++;
--count;
```

**if_stmt** — the condition is parenthesised; each branch is a block. The
`else` clause is optional and may be followed by another `if` for
`else if` chains.

```c
if (n <= 1) {
    return n;
}

if (x > 0) {
    printf("positive\n");
} else if (x < 0) {
    printf("negative\n");
} else {
    printf("zero\n");
}
```

**while_stmt** — evaluates the condition before each iteration; exits when
the condition is zero.

```c
while (i < 10) {
    printf("%d\n", i);
    i = i + 1;
}
```

**for_stmt** — the initializer runs once before the first iteration; the
update runs at the end of each iteration. All three clauses are optional
(an omitted condition is treated as always-true).

```c
for (int i = 0; i < n; i = i + 1) {
    total = total + i;
}
```

**do_while_stmt** — like `while`, but the condition is checked *after* the
body, so the body always runs at least once. The condition is checked in
the scope *outside* the body block (a variable declared in the body is
already out of scope by the time the condition runs).

```c
int i = 0;
do {
    i = i + 1;
} while (i < 3);
```

**switch_stmt** — `case`/`default` labels split the body into a flat,
fallthrough sequence, exactly like C: control runs from one labeled segment
into the next unless something (usually `break`) exits the switch first.
Each `case` value must be a (optionally negated) integer literal, and all
case values in one switch must be distinct; at most one `default` is
allowed. The switch's value must have an integer type (`int` or `char`).
Labels are only recognized directly in the switch's body — not nested
inside an `if`/`while`/etc. within it (the rare "Duff's device" idiom is
not supported).

```c
switch (n) {
case 0:
    printf("zero\n");
    break;
case 1:
case 2:
    printf("one or two\n");
    break;
default:
    printf("other\n");
}
```

**goto_stmt / label_stmt** — `goto` transfers control directly to a label
in the same function, forward or backward, crossing in or out of nested
blocks/loops/switches freely (sema resolves every label in the function
before checking any `goto`, so forward jumps work). A label's name shares
the same scope as the rest of the function; duplicate label names are an
error, as is a `goto` to a name that's never declared as a label anywhere
in the function. There's no restriction against jumping into a block past
a variable's declaration (a real C compiler would flag some such jumps;
MiniC doesn't).

```c
int n = 0;
retry:
n = n + 1;
if (n < 3) {
    goto retry;
}
```

**return_stmt** — exits the current function, optionally with a value. A
`void` function uses `return;`; a non-`void` function must supply an
expression matching the declared return type.

```c
return n;
return fibonacci(n - 1) + fibonacci(n - 2);
return;    // valid only in a void function
```

**break_stmt / continue_stmt** — `break` exits the innermost enclosing loop
*or* `switch`; `continue` jumps to the loop condition (for `while`/`do`-`while`)
or the update clause (for `for`) of the innermost enclosing **loop**
specifically — a `switch` doesn't count as a loop for `continue`, so
`continue` inside a `switch` with no enclosing loop is still an error.

```c
while (1) {
    if (done) { break; }
    if (skip) { continue; }
    process();
}
```

---

## Expressions

Operator precedence, lowest to highest:

```bnf
expression   ::= ternary
ternary      ::= logical_or ("?" expression ":" ternary)?
logical_or   ::= logical_and ("||" logical_and)*
logical_and  ::= bitwise_or ("&&" bitwise_or)*
bitwise_or   ::= bitwise_xor ("|" bitwise_xor)*
bitwise_xor  ::= bitwise_and ("^" bitwise_and)*
bitwise_and  ::= equality ("&" equality)*
equality     ::= comparison (("==" | "!=") comparison)*
comparison   ::= shift (("<" | ">" | "<=" | ">=") shift)*
shift        ::= additive (("<<" | ">>") additive)*
additive     ::= multiplicative (("+" | "-") multiplicative)*
multiplicative ::= unary (("*" | "/") unary)*
unary        ::= ("!" | "-" | "&" | "*" | "~") unary
               | ("++" | "--") unary
               | postfix
postfix      ::= primary ( "[" expression "]" | "." identifier | "->" identifier
                          | "++" | "--" )*
primary      ::= int_lit | float_lit | char_lit | string_lit
               | identifier
               | identifier "(" args? ")"
               | "(" expression ("," expression)* ")"
args         ::= expression ("," expression)*
```

`int <= 1`, `n - 1`, `a && b`, `!done`, and `fibonacci(n - 1)` are all
examples of expressions covered by this grammar. String literals only
appear as call arguments (e.g., the format string passed to `printf`).

A leading `&` or `*` in an operand position is the unary address-of or
dereference operator rather than the binary `&&`/`*` operators — the parser
disambiguates by position, not by lookahead, exactly as in real C grammars.
`&x`, `*p`, and `**pp` are all valid unary expressions.

`[`, `.`, and `->` all bind tighter than the prefix unary operators, so
`*arr[i]` parses as `*(arr[i])`, `&arr[i]` as `&(arr[i])`, and `&p->x` as
`&(p->x)`, matching C. `p->field` is desugared by the parser into
`(*p).field` — there's no separate "arrow" AST node, so sema and codegen
only ever need to handle the dot form. Indexing is its own grammar
production rather than sugar for pointer arithmetic — there is no general
`p + 1` pointer arithmetic via `+`/`-` (those operators still require
numeric, non-pointer operands; this also means `++`/`--` reject pointer
operands).

The **comma operator** (`a, b`: evaluate `a` for its side effects, discard
the result, then evaluate and yield `b`) is only reachable inside explicit
parentheses — `primary`'s `"(" expression ("," expression)* ")"` production
— never as a bare top-level production. This is deliberate: it means the
comma operator can never be confused with the unrelated comma that
separates call arguments or parameters, which are still plain
comma-separated lists, not applications of the comma operator.

`++`/`--` have a prefix form (parsed in `unary`, producing the
already-updated value) and a postfix form (parsed in `postfix`, producing
the value *before* the update); both require an lvalue operand and mutate
it as a side effect, same as in C.

---

## Type System

### Base Types

| Type    | Width  | Notes |
|---------|--------|-------|
| `int`   | 32-bit | signed integer; LLVM `i32` |
| `float` | 32-bit | IEEE 754 single; LLVM `float` |
| `char`  | 8-bit  | signed; LLVM `i8` |
| `void`  | —      | valid only as a function return type, or as the pointee of `void*` |

### Pointers

Any base type may be suffixed with one or more `*` to form a pointer type
(`int*`, `char**`, `void*`, ...). A `Type` is represented internally as a
base kind plus an indirection depth, so arbitrarily deep pointers compose
without a separate "pointee type" concept.

- `&expr` — address-of. `expr` must be an lvalue (a variable or a `*ptr`
  dereference); the result type is `expr`'s type with one more `*`.
- `*expr` — dereference. `expr` must have pointer type; the result type is
  `expr`'s type with one fewer `*`. Dereferencing `void*` is an error
  (`void` is an incomplete type, as in C).
- A pointer may be compared with `==`/`!=` against another pointer of the
  *exact same* type, or against the integer literal `0` (a null-pointer
  constant — `int *p = 0;` and `if (p == 0)` both work this way; there is no
  `NULL` macro since MiniC has no preprocessor).
- A pointer may be used directly as an `if`/`while` condition: true when
  non-null.
- Pointer arithmetic (`p + 1`) and array indexing are not yet supported —
  see [docs/ROADMAP.md](ROADMAP.md) for the arrays tier that adds them.

```c
int x = 5;
int *p = &x;
*p = 6;        // x is now 6
int **pp = &p;
if (p) { ... }
```

### Arrays

`Type` carries a single array-length dimension alongside the pointer
depth (`include/ast.h`'s `Type::arrayLength()`), so `int arr[5]` and
`int *arr[5]` (an array of 5 pointers) are both representable, but there is
no multi-dimensional array (`int[5][5]`) and no pointer-to-array type.

- A local variable may be declared with a single `[size]` suffix
  (`int arr[10];`); `size` must be a positive integer literal.
- `arr[i]` indexes either an array or a pointer — both lower to the same
  GEP-based address computation, since an array decays to a pointer to its
  first element whenever it's used as a value (exactly like C). The index
  must be numeric; subscripting a `void*` is an error, same as
  dereferencing one.
- Because of that decay, passing an array where a pointer parameter is
  expected (`void f(int *p); ... f(arr);`) just works — `arr` and `&arr[0]`
  produce the same pointer value.
- `&arr` (address of the whole array) is rejected, since MiniC has no
  pointer-to-array type to represent the result; write `&arr[0]` (or just
  `arr`) for "a pointer to this array's data" instead.
- An array is not assignable as a whole (`arr = other;` is an error);
  assign element-by-element with `arr[i] = ...`.
- There is no array-literal initializer syntax (`int arr[3] = {1, 2, 3};`);
  fill an array with a loop after declaring it.

```c
int values[5];
values[0] = 1;
int *p = values;        // decays to int*
int x = p[0];            // same element as values[0]
int *q = &values[2];     // pointer to the third element
```

### Structs, Unions, and Enums

A `Type` that names a struct or union carries that tag name alongside its
kind (`include/ast.h`'s `Type::aggregateName()`), so two aggregate types
are equal only when their tag names match. There are no anonymous
structs/unions, no nested struct *definitions* (a field's type can
reference any other already-declared-or-not-yet-declared struct/union by
name, but you can't write a new `struct { ... }` body inline as a field's
type), and no struct-literal initializer syntax (`struct Point p = {1, 2};`).

- Struct and union fields are plain `type name;` declarations, optionally
  array-sized, in a top-level `struct Tag { ... };` / `union Tag { ... };`.
- **Structs are sequential**: each field gets its own storage, in
  declaration order (an LLVM named struct type under the hood).
- **Unions overlap**: every field lives at the same address, sized to fit
  the largest one (there's no native LLVM union — codegen picks whichever
  field's LLVM type has the largest `getTypeAllocSize` as the union's
  storage type, queried from `llvm::DataLayout`, and every other field is
  read/written at that same address through its own type).
- `s.field` accesses a member of a struct/union value `s`; `p->field` is
  the equivalent for a pointer `p` (sugar for `(*p).field`).
- Struct/union **values are first-class**: a local variable, function
  parameter, or return value of struct/union type holds (or copies) the
  whole aggregate, the same way an `int` does — passing one to a function
  or returning one gives the callee/caller an independent copy, not a
  reference to the original. This falls out of LLVM's native
  load/store/pass-by-value support for aggregate types; MiniC doesn't
  special-case it with manual `memcpy`s.
- A struct/union variable **is** assignable as a whole (`p2 = p1;` copies
  every field) — unlike an array, a struct/union value really does have a
  single well-defined width.
- A field's type may reference another struct/union **regardless of
  declaration order** (sema resolves every tag name before checking any
  field, in two passes) — but a struct/union can't directly contain
  itself by value (`struct Node { struct Node n; };` is infinite size);
  only a pointer field can form a self-reference, the standard linked-data
  idiom. Only the *direct* case is checked; a cycle spread across two or
  more structs (`A` contains `B` by value, `B` contains `A` by value) is
  only caught later, by codegen, and only if that cyclic type is actually
  instantiated somewhere.
- `enum Tag { A, B = 2, ... };` declares **plain `int` constants** (`A` is
  `0`, an unadorned name after an explicit value is one more than the
  previous), not a distinct enum type — `enum Tag` as a type reference is
  just `int`. A variable can shadow an enum constant's name in a nested
  scope, like any other identifier.
- struct, union, and enum tags share one namespace (matching C): you can't
  declare both `struct Foo` and `union Foo`.

```c
struct Point { int x; int y; };
union Number { int i; float f; };
enum Color { RED, GREEN, BLUE };

struct Point makeOrigin() {
    struct Point p;
    p.x = 0;
    p.y = 0;
    return p;          // returns a copy
}

void move(struct Point *p, int dx, int dy) {
    p->x = p->x + dx;  // (*p).x = (*p).x + dx
    p->y = p->y + dy;
}

int main() {
    struct Point a = makeOrigin();
    struct Point b = a;   // whole-struct copy
    move(&a, 1, 1);       // b is unaffected
    int c = GREEN;        // c == 1
    return 0;
}
```

### Arithmetic Operators (+, -, *, /)

Both operands must be numeric (`int`, `float`, or `char`). The result type
follows C's usual arithmetic conversions:

- If either operand is `float`, the other is implicitly widened to `float`
  and the result is `float`.
- Otherwise the result is `int` (a `char` operand is sign-extended to `int`
  before the operation).

Integer division truncates toward zero (C semantics); there is no implicit
integer-to-float promotion for `/`.

```c
int a = 7 / 2;      // 3 — integer division
float b = 7.0 / 2;  // 3.5 — float division (2 is widened)
```

### Comparison Operators (==, !=, <, >, <=, >=)

Both operands must be numeric. The result is always `int`: `1` if the
comparison holds, `0` otherwise. This matches C semantics and means
comparisons are composable with arithmetic.

### Logical Operators (&&, ||, !)

Each operand may be numeric *or* a pointer (matching `if`/`while`'s
condition rule); any non-zero/non-null value is true. The result is `int`
(1 or 0). `&&`/`||` **do** short-circuit: the right operand's code isn't
even reached unless the left operand's value makes it necessary, so it's
safe to write `p != 0 && p->field == 1` without `p` being dereferenced when
null.

### Bitwise Operators (&, |, ^, ~, <<, >>)

Operands must be **integral** (`int` or `char` — not `float`, unlike the
arithmetic operators). The result is always `int`. `<<`/`>>` shift the
left operand by the right operand's value; `>>` is an arithmetic
(sign-extending) shift, matching signed `int` on essentially all real
platforms. `~` is the unary one's-complement operator.

```c
int flags = a & b | c ^ d;
int shifted = (x << 2) >> 1;
int inverted = ~mask;
```

Note that `&` here is the same token as address-of and `&&` — like
`*`/dereference, the parser distinguishes the binary bitwise-AND form from
the unary address-of form by position, not by a different token.

### Ternary Conditional (?:)

`condition ? thenExpr : elseExpr`. The condition follows the same
numeric-or-pointer rule as `if`. The two branches must be the same type,
or both numeric (in which case the usual `int`/`float` widening applies),
or one a pointer and the other the null-pointer-constant `0` (matching
`==`'s pointer rules). `?:` is right-associative and binds looser than
every binary operator but tighter than nothing — it's the entry point of
`expression` itself, so `a = b ? c : d` would need parentheses around the
assignment if MiniC had assignment-as-an-expression (it doesn't; see
[Variable Declaration and Assignment](#variable-declaration-and-assignment)).

```c
int max = a > b ? a : b;
float f = cond ? someInt : someFloat;  // widens to float
int *p = cond ? &x : 0;
```

### Increment/Decrement (++, --)

`++x`/`--x` (prefix) update `x` and the expression's value *is* the new
value; `x++`/`x--` (postfix) update `x` but the expression's value is the
value *before* the update. The operand must be an lvalue and numeric (not
a pointer — pointer `++` would be pointer arithmetic, which isn't
supported). Both forms work as statements (`i++;`) or nested in a larger
expression (`arr[i++]`, though MiniC has no array-literal/pointer
arithmetic context where that idiom is as central as in real C).

```c
int i = 0;
printf("%d\n", i++);  // prints 0, i is now 1
printf("%d\n", ++i);  // prints 2, i is now 2
```

### Compound Assignment (+=, -=, *=, /=, &=, |=, ^=, <<=, >>=)

`target op= value` means `target = target op value`, except `target`'s
address is computed only once — relevant when it has a side effect (e.g.
`arr[f()] += 1` calls `f` exactly once). The combined type must convert
back to `target`'s type under the same rules as plain assignment
(including the narrowing-conversion warning). There's no `%=` since MiniC
has no `%` (modulo) operator.

### Comma Operator (,)

`(a, b)`: evaluate `a` for its side effects, discard the result, evaluate
`b`, and yield it. Only reachable inside explicit parentheses, so it can
never be confused with the argument-list/parameter-list comma. Most useful
in a `for` loop's update clause: `for (...; ...; i++, j--)`.

### Implicit Conversions

| Context | Allowed | Diagnostic |
|---------|---------|-----------|
| `int` ← `float` | yes, truncation | warning: narrowing conversion |
| `char` ← `int` | yes, truncation | warning: narrowing conversion |
| `char` ← `float` | yes, double truncation | warning: narrowing conversion |
| `int` ← `char` | yes, sign-extends | no warning |
| `float` ← `int` | yes, widens | no warning |
| any ← `void` | no | error |

### Variable Declaration and Assignment

The declared type is the *target* type. The initializer's type must be
assignable to the target (see the table above). The same rule applies to
`assign_stmt`.

```c
int x = 3.7;    // warning: narrowing float → int; x = 3
float y = 5;    // ok: int 5 widens to float 5.0
```

### Function Calls

Argument count must exactly match the parameter count (except for the
built-in `printf`, which is variadic). Each argument's type must be
assignable to the corresponding parameter type under the same rules as
variable assignment.

### Return Statements

The expression type must be assignable to the enclosing function's declared
return type. A `void` function may use a bare `return;` or fall off the
end of its body. A non-`void` function that reaches the end of its body
without a `return` is undefined behavior, as in C.

### Condition Expressions

The condition of `if`, `while`, and `for` must be a numeric type (`int`,
`float`, or `char`). A zero value is false; any other value is true. There
is no boolean type.

```c
if (3.14) { ... }   // ok: non-zero float is true
while (n) { ... }   // ok: exits when n == 0
```

---

## AST

`include/ast.h` defines one node type per grammar construct:

- `ProgramNode` — holds three top-level lists: `aggregates`
  (`AggregateDeclNode`, for both struct and union — `isUnion` tells them
  apart), `enums` (`EnumDeclNode`), and `functions` (`FuncDefNode`)
- `FuncDefNode`, `ParamNode`, `AggregateDeclNode`, `FieldNode`,
  `EnumDeclNode`, `EnumeratorNode` — top-level structure
- `BlockStmtNode`, `VarDeclStmtNode`, `AssignStmtNode`, `ExprStmtNode`,
  `IfStmtNode`, `WhileStmtNode`, `ForStmtNode`, `DoWhileStmtNode`,
  `SwitchStmtNode`, `CaseLabelStmtNode`, `DefaultLabelStmtNode`,
  `LabelStmtNode`, `GotoStmtNode`, `ReturnStmtNode`, `BreakStmtNode`,
  `ContinueStmtNode` — statements. `SwitchStmtNode::body` is a flat
  `BlockStmtNode` whose direct statements mix regular statements with
  `CaseLabelStmtNode`/`DefaultLabelStmtNode` markers — the same shape as
  the C grammar's fallthrough semantics, so codegen can emit the body as
  one sequential pass that just switches basic blocks at each marker.
- `BinOpExprNode`, `UnaryOpExprNode`, `CallExprNode`, `IdentExprNode`,
  `IndexExprNode`, `MemberExprNode`, `TernaryExprNode`, `IncDecExprNode`,
  `IntLitExprNode`, `FloatLitExprNode`, `CharLitExprNode`,
  `StringLitExprNode` — expressions.
  - `UnaryOpExprNode` covers `&`/`*`/`~` (`UnaryOp::AddressOf`/`Deref`/`BitNot`)
    as well as `-`/`!`.
  - `BinOpExprNode` covers `&`/`|`/`^`/`<<`/`>>` (`BinaryOp::BitAnd`/`BitOr`/
    `BitXor`/`Shl`/`Shr`) and the comma operator (`BinaryOp::Comma`)
    alongside the arithmetic/comparison/logical operators.
  - `IndexExprNode` holds `base`/`index` sub-expressions for `arr[i]`.
  - `MemberExprNode` holds a `base` expression and a `field` name for
    `base.field` — `base->field` is desugared at parse time into
    `MemberExprNode{UnaryOpExprNode{Deref, base}, field}`, so this node
    only ever represents the dot form.
  - `TernaryExprNode` holds `condition`/`thenExpr`/`elseExpr`.
  - `IncDecExprNode` holds a `target` lvalue plus `isIncrement`/`isPrefix`
    flags, covering all four of `++x`, `--x`, `x++`, `x--` with one node.

`AssignStmtNode::target` is a general lvalue expression (an `IdentExprNode`,
a `UnaryOpExprNode{Deref, ...}`, an `IndexExprNode`, or a `MemberExprNode`),
not just a variable name, so that `*p = 5;`, `arr[i] = 5;`, `s.field = 5;`,
and `x = 5;` all share the same AST shape. `AssignStmtNode::compoundOp` is
an `std::optional<BinaryOp>`: empty for a plain `=`, or set to e.g.
`BinaryOp::Add` for `+=` — there's no separate "compound assignment" node.

Every node implements `print(std::ostream&, int indent)`, used by the
`--emit-ast` CLI flag to dump an indented tree, e.g.:

```
$ ./build/minic examples/fibonacci.mc --emit-ast
Program
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
  ...
```

---

## Error Reporting

Syntax errors are thrown immediately with a `file:line:col: error: <message>
(got <token>)` format:

```
$ echo 'int main() { return 0 }' > bad.mc && ./build/minic bad.mc --emit-ast
bad.mc:1:23: error: expected ';' after return statement (got TOK_RBRACE '}')
```

Semantic errors and warnings are collected in a single pass and printed
together after parsing completes, so a single run surfaces all problems:

```
$ ./build/minic bad.mc
bad.mc:3:12: error: use of undeclared variable 'nn'
bad.mc:8:12: warning: narrowing conversion from 'float' to 'int'
bad.mc:15:5: error: return type mismatch — expected 'int', got 'void'
```
