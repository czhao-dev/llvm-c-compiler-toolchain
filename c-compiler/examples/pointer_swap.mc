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
    printf("before: x=%d y=%d\n", x, y);

    swap(&x, &y);
    printf("after: x=%d y=%d\n", x, y);

    printf("incremented: %d\n", increment(&x));

    int *p = &x;
    if (p) {
        printf("p is non-null\n");
    }
    return 0;
}
