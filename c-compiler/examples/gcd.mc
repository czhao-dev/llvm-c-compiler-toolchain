int gcd(int a, int b) {
    while (b != 0) {
        int t = b;
        b = a - (a / b) * b;
        a = t;
    }
    return a;
}

int main() {
    printf("%d\n", gcd(252, 105));
    return 0;
}
