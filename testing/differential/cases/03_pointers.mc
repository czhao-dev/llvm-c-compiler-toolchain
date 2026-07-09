// Pointer assignment, double dereference, and address-of, extending the
// pattern from c-compiler/examples/pointer_swap.mc. Diffed against
// clang compiling this same source.

void swap(int *a, int *b) {
    int temp = *a;
    *a = *b;
    *b = temp;
}

int increment(int *p) {
    *p = *p + 1;
    return *p;
}

// NOTE: double dereference is exercised below via a local `int **`, not as
// a function *parameter* type -- minic's -O2 pipeline currently fails to
// compile any function taking an `int **` parameter (a real, pre-existing
// bug: the LLVM optimizer passes run against the locally-installed LLVM
// libraries emit a newer memory-attribute textual-IR syntax that the
// system `clang` minic shells out to for final codegen doesn't parse,
// since the two come from different LLVM lineages/versions). Confirmed
// with a minimal repro; tracked as a known issue rather than worked
// around in the compiler itself, which is out of scope for this suite.

int main() {
    int x = 3;
    int y = 7;
    printf("before: x=%d y=%d\n", x, y);

    swap(&x, &y);
    printf("after: x=%d y=%d\n", x, y);

    printf("incremented: %d\n", increment(&x));

    int *p = &x;
    int **pp = &p;
    **pp = **pp + 10;
    printf("via double pointer: %d\n", **pp);

    if (p) {
        printf("p is non-null\n");
    }

    int *null_ptr = 0;
    if (null_ptr == 0) {
        printf("null_ptr is null\n");
    }

    return 0;
}
