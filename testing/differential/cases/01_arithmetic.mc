// Operator precedence, mixed int/float arithmetic, bitwise ops, shifts,
// and compound assignment. Diffed against clang compiling this same
// source (see testing/differential/run_differential_tests.py) -- MiniC
// has no modulo operator, so it's deliberately not exercised here.

int main() {
    int a = 2 + 3 * 4;   // precedence: 14, not 20
    int b = (2 + 3) * 4; // parens override precedence: 20
    int c = 10 - 2 - 3;  // left-associative subtraction: 5
    printf("%d %d %d\n", a, b, c);

    float f = 1.5 + 2 * 2.0; // mixed int/float promotion: 5.5
    printf("%f\n", f);

    int bits = (6 & 3) | (8 ^ 1) | (1 << 4);
    printf("%d\n", bits);

    int shifted = 256 >> 4;
    printf("%d\n", shifted);

    int x = 10;
    x += 5;
    x -= 2;
    x *= 3;
    x /= 2;
    printf("%d\n", x);

    int cond = (x > 10) ? 1 : 0;
    printf("%d\n", cond);

    return 0;
}
