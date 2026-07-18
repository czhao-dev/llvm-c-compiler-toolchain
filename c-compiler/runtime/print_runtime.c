// Runtime support for MiniC's print_int/print_float/print_char/print_str
// builtins. This is real, standard C -- compiled and linked into every
// minic-produced binary by codegen.cpp's final `clang ... -o <output>`
// step, and into the differential-testing/benchmark harnesses' clang
// builds the same way, so a .mc source calling these builtins is genuine
// C that both toolchains can produce a comparable binary from.
//
// No trailing newline is added by any of these -- callers control their
// own formatting explicitly (e.g. `print_str("\n")`), matching how the
// examples previously spelled out "\n" in printf format strings.

#include <stdio.h>

void print_int(int x) { printf("%d", x); }

void print_float(float x) { printf("%f", x); }

void print_char(char x) { printf("%c", x); }

void print_str(char *s) { fputs(s, stdout); }
