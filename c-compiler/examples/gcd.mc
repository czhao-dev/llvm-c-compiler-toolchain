int gcd(int a, int b) {
    while (b != 0) {
        int t = b;
        b = a - (a / b) * b;
        a = t;
    }
    return a;
}

int main() {
    print_int(gcd(252, 105));
    print_str("\n");
    return 0;
}
