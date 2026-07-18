// examples/pointer_swap.mc

void swap(int *a, int *b) {
    int temp = *a;
    *a = *b;
    *b = temp;
}

int increment(int *p) {
    *p = *p + 1;
    return *p;
}

int main() {
    int x = 3;
    int y = 7;
    print_str("before: x=");
    print_int(x);
    print_str(" y=");
    print_int(y);
    print_str("\n");

    swap(&x, &y);
    print_str("after: x=");
    print_int(x);
    print_str(" y=");
    print_int(y);
    print_str("\n");

    print_str("incremented: ");
    print_int(increment(&x));
    print_str("\n");

    int *p = &x;
    if (p) {
        print_str("p is non-null\n");
    }
    return 0;
}
