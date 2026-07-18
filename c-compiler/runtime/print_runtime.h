// Prototypes for runtime/print_runtime.c's four print builtins. When a
// .mc source is compiled directly by clang (as real C, for cross-
// validation against minic) rather than by minic itself, clang compiles
// the source and print_runtime.c as separate translation units and needs
// these declarations visible via -include; minic itself doesn't use this
// header at all, since its own codegen declares the same signatures
// directly (see CodeGenerator::declarePrintBuiltins in src/codegen.cpp).
#ifndef MINIC_PRINT_RUNTIME_H
#define MINIC_PRINT_RUNTIME_H

void print_int(int x);
void print_float(float x);
void print_char(char x);
void print_str(char *s);

#endif
