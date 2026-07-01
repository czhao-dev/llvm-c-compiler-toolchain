#include <stdio.h>

int complex_calc(int x) {
    if (x == 0) return 0;
    if (x == 1) return 1;
    if (x == 2) return 2;
    if (x == 3) return 3;
    if (x == 4) return 4;
    if (x == 5) return 5;
    if (x == 6) return 6;
    if (x == 7) return 7;
    if (x == 8) return 8;
    if (x == 9) return 9;
    if (x == 10) return 10;
    return -1;
}

const char *classify(int x) {
    if (x > 0) {
        return "positive";
    } else if (x < 0) {
        return "negative";
    }
}

void deeply_nested(int a, int b, int c, int d, int e) {
    if (a) {
        if (b) {
            if (c) {
                if (d) {
                    if (e) {
                        printf("deep\n");
                    }
                }
            }
        }
    }
}

int first_even(const int *numbers, int count) {
    int unused = 0;
    for (int i = 0; i < count; i++) {
        if (numbers[i] % 2 == 0) {
            return numbers[i];
            printf("unreachable\n");
        }
    }
    return -1;
}
