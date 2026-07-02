# c-linter: Rule Specification

This document is the normative reference for what each rule flags, what it
deliberately doesn't, and why. The [README](../README.md) is the quick-start
version of this document.

## 1. Lexer tolerance policy

`c-linter`'s lexer (`include/lexer.h`) is not a compiler-grade lexer and is
deliberately built to a different contract than one:

- **It never throws.** An unterminated `/* */`, `"..."`, or `'...'` just
  consumes to end-of-input rather than raising an error. A linter has to
  process real-world C it doesn't fully model — rejecting input is not an
  option the way it is for a compiler.
- **Keyword recognition is minimal by design.** Only `if` and `while` are
  distinct keyword tokens (`TokenType::If`/`TokenType::While`). Every other
  keyword-shaped word — `int`, `return`, `struct`, `static`, `sizeof`, ... —
  lexes as a plain `Identifier`. This is safe, not sloppy:
  1. No rule other than brace-style needs to distinguish a keyword from an
     identifier.
  2. Every real C keyword is lowercase with no embedded uppercase, so the
     naming rule (CL001) can never misfire on one.
  3. It keeps the lexer small and avoids drifting into semantic/type
     modeling, which is `c-static-analyzer`'s territory, not this tool's.
- **Unmodeled punctuation becomes `TokenType::Other`.** Anything the lexer
  doesn't have a specific token for — `[`, `]`, `,`, `;`, `->`, `&&`, `%=`,
  `#`, `~`, unary `-`, and so on — becomes a single-character `Other`
  token. This is what lets the lexer swallow an entire real `.c` file
  (pointer/array syntax, bitwise operators, preprocessor directive lines)
  without erroring.
- **`FloatLiteral` is a distinct token type from `IntLiteral`**, specifically
  so the magic-number rule's "IntLiteral token" test can be applied
  literally — `x == 3.14` is never flagged.

## 2. CL001 — Naming convention (snake_case)

**Flags:** every `Identifier` token whose lexeme contains an uppercase
letter *and* contains no underscore.

```
totalCount   -> flagged   (uppercase 'C', no underscore)
TotalCount   -> flagged   (uppercase 'T', no underscore)
PI           -> flagged   (uppercase, no underscore)
total_count  -> not flagged (has underscore)
MAX_SIZE     -> not flagged (has underscore)
count        -> not flagged (no uppercase)
```

**Deliberate simplification: no symbol table.** This check runs directly
over the token stream with no notion of "this token and that token refer to
the same variable." That means a badly-named identifier is flagged once
*per occurrence* — its declaration, and every subsequent read or write —
not once per symbol. Building a symbol table to deduplicate would cross
into semantic analysis, which is explicitly out of scope for this tool
(see `c-static-analyzer`). In practice this means fixing one bad name in a
file can silence many warnings at once, which is a reasonable trade for
staying purely syntactic.

## 3. CL002 / CL003 — Line length and trailing whitespace

**Flags:** run as a single raw-text pass (`checkLineRules`) over the
source, before tokenization. For each line (split on `\n`, with a trailing
`\r` stripped first so CRLF files aren't penalized):

- CL002 fires if the line's length exceeds the configured maximum
  (default 80, `--max-line-length=N`).
- CL003 fires if the line ends in a space or a tab.

The two checks are independent — a single line can trigger both. The final
line of a file is checked even if it has no trailing newline.

## 4. CL004 — Magic number in a comparison

**Flags:** a comparison operator (`==`, `!=`, `<`, `>`, `<=`, `>=`)
immediately followed by an `IntLiteral` token — optionally through a single
unary-minus `Other` token, so `!= -5` is detected the same way `!= 5` is.

**Exemptions: `0`, `1`, and `-1`.** These are common sentinel values
(`!= 0`, `>= 1`, `!= -1` for "not found") that aren't meaningfully "magic"
in the way a value like `31` or `4096` is. The value is parsed from the
literal's text (suffix letters `u`/`U`/`l`/`L` stripped, then interpreted
with the same base-0 rules C itself uses — `0x..` hex, leading-`0` octal,
otherwise decimal) so the exemption is by *value*, not by literal spelling:
`0x0`, `00`, and `0` are all recognized as the exempt value zero.

**Scope note:** the rule only looks *forward* one (or two, through a unary
minus) token from the comparison operator. `5 == x` (magic number on the
left) is not flagged — the requirement as specified is "comparison operator
followed by an integer literal," and this implementation applies that
literally rather than generalizing to both operand positions.

## 5. CL005 — Brace-style consistency (K&R vs. Allman)

**Flags:** for each `if`/`while` token, the algorithm:

1. Requires the very next token to be a `LeftParen` — otherwise this
   occurrence is skipped (tolerates macro-obscured or malformed code rather
   than guessing).
2. Walks forward tracking paren depth to find the `RightParen` that truly
   closes the condition, so nested parens (`if ((a + b) > (c * d))`) don't
   confuse the match. If the input is truncated before a matching
   `RightParen` is found, the occurrence is skipped — no crash, no
   diagnostic.
3. If the token immediately after that closing paren is not a `LeftBrace`,
   the occurrence is skipped — **braceless bodies are out of scope** for
   this rule (`if (x) foo();` is never flagged; "always use braces" would
   be a different, unrequested rule).
4. Otherwise, compares the source line of the closing paren against the
   source line of the opening brace: K&R (the default) requires them on
   the same line; Allman (`--brace-style=allman`) requires the brace on a
   later line.

`do { ... } while (x);` is unaffected: the trailing `while (x);` ends in a
semicolon, not a brace, so step 3 skips it — this rule only ever looks
forward from `if`/`while` for a following brace, it does not special-case
`do`-`while`.

## 6. Diagnostic format and exit codes

`file.c:12: warning: message [CL001]` — one line per diagnostic, printed to
stdout in `(line, column)` order per file, files processed in the order
given on the command line.

- Exit `0`: every input file opened successfully and produced zero
  diagnostics.
- Exit `1`: any input file failed to open, or any diagnostic was produced —
  a CI-friendly nonzero-on-findings convention shared with
  `c-static-analyzer`.
- Exit `2`: usage error (no input files given).
