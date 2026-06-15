int fibonacci(int n) {
    if (n <= 1) {
        return n;
    }
    return fibonacci(n - 1) + fibonacci(n - 2);
}

int main() {
    int i = 0;
    while (i < 10) {
        printf("%d\n", fibonacci(i));
        i = i + 1;
    }
    return 0;
}
