// Nested if/else, while, for, early return, and recursion. Diffed
// against clang compiling this same source.

int classify(int x) {
    if (x < 0) {
        if (x < -10) {
            return -2;
        } else {
            return -1;
        }
    } else if (x == 0) {
        return 0;
    } else {
        if (x > 10) {
            return 2;
        }
        return 1;
    }
}

int factorial(int n) {
    if (n <= 1) {
        return 1;
    }
    return n * factorial(n - 1);
}

int main() {
    int i = -15;
    while (i <= 15) {
        print_int(classify(i));
        print_str("\n");
        i += 5;
    }

    for (i = 1; i <= 6; i++) {
        print_int(i);
        print_str("! = ");
        print_int(factorial(i));
        print_str("\n");
    }

    int count = 0;
    int n = 0;
    do {
        count++;
        n += count;
    } while (count < 5);
    print_str("count=");
    print_int(count);
    print_str(" n=");
    print_int(n);
    print_str("\n");

    return 0;
}
