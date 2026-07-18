// Operator precedence, mixed int/float arithmetic, bitwise ops, shifts,
// and compound assignment. Diffed against clang compiling this same
// source (see testing/differential/run_differential_tests.py) -- MiniC
// has no modulo operator, so it's deliberately not exercised here.

int main() {
    int a = 2 + 3 * 4;   // precedence: 14, not 20
    int b = (2 + 3) * 4; // parens override precedence: 20
    int c = 10 - 2 - 3;  // left-associative subtraction: 5
    print_int(a);
    print_str(" ");
    print_int(b);
    print_str(" ");
    print_int(c);
    print_str("\n");

    float f = 1.5 + 2 * 2.0; // mixed int/float promotion: 5.5
    print_float(f);
    print_str("\n");

    int bits = (6 & 3) | (8 ^ 1) | (1 << 4);
    print_int(bits);
    print_str("\n");

    int shifted = 256 >> 4;
    print_int(shifted);
    print_str("\n");

    int x = 10;
    x += 5;
    x -= 2;
    x *= 3;
    x /= 2;
    print_int(x);
    print_str("\n");

    int cond = (x > 10) ? 1 : 0;
    print_int(cond);
    print_str("\n");

    return 0;
}
