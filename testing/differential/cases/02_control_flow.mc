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
        printf("%d\n", classify(i));
        i += 5;
    }

    for (i = 1; i <= 6; i++) {
        printf("%d! = %d\n", i, factorial(i));
    }

    int count = 0;
    int n = 0;
    do {
        count++;
        n += count;
    } while (count < 5);
    printf("count=%d n=%d\n", count, n);

    return 0;
}
