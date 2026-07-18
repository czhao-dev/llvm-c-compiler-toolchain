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
    int **pp = &p;
    **pp = **pp + 10;
    print_str("via double pointer: ");
    print_int(**pp);
    print_str("\n");

    if (p) {
        print_str("p is non-null\n");
    }

    int *null_ptr = 0;
    if (null_ptr == 0) {
        print_str("null_ptr is null\n");
    }

    return 0;
}
