// A single main() calling several separately-defined functions.
//
// NOTE: this is scoped down from "main.c calls a function defined in
// math.c" -- minic's CLI only accepts one input file and rejects multiple
// inputs (see c-compiler-llvm/src/main.cpp's parseArgs, which throws
// "multiple input files were provided"). There is no multi-translation-
// -unit compilation support today, so this case instead exercises
// multi-function resolution and forward references within one file.
// Diffed against clang compiling this same source.

int square(int x) {
    return x * x;
}

int add(int a, int b) {
    return a + b;
}

int compute(int a, int b) {
    return square(add(a, b));
}

int main() {
    printf("%d\n", add(3, 4));
    printf("%d\n", square(5));
    printf("%d\n", compute(3, 4));
    return 0;
}
