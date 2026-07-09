// Naive recursive Fibonacci. Heavy on function calls and stack frame
// allocation, deliberately not the iterative/memoized version. N=39 is
// tuned to take on the order of a few hundred ms at -O0.

int fib(int n) {
    if (n <= 1) {
        return n;
    }
    return fib(n - 1) + fib(n - 2);
}

int main() {
    printf("%d\n", fib(39));
    return 0;
}
