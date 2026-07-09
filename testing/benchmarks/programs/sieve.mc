// Sieve of Eratosthenes up to a fixed N. Array-access-heavy, no
// recursion. Prints the prime count, which also sanity-checks
// correctness as a side effect of running the benchmark.
//
// NOTE on why N is capped at 40000: this suite's exploration surfaced a
// real, pre-existing bug in minic -- a `while` loop nested inside an
// `if` that is itself inside another `while` loop crashes (SIGSEGV) once
// the outer loop enters that `if` roughly 5000+ times (confirmed via
// bisection: 4675 entries at N=45000 runs fine, ~5133 at N=50000
// segfaults, reproducing even with the inner loop's induction variable
// hoisted out of the `if` block and with the inner loop rewritten as a
// `for`). This looks like unbounded per-outer-iteration growth in
// something codegen allocates for the nested-loop control flow, not a
// stack-array-size limit (a 4M-int flat array with no nested loop, and a
// classic non-nested-in-if double loop as in bubble_sort.mc, both work
// fine at far larger sizes). N=40000 (4203 entries into the nested loop)
// keeps a safety margin below the observed failure threshold.
//
// Separately, `minic sieve.mc -O2` does not crash -- it hangs
// indefinitely at compile time (confirmed: no output after 15s, even at
// this same N=40000, which runs at -O0 in well under a second). This
// looks like a distinct, likely-related pathological case in the -O2
// LLVM optimizer pipeline for this nested-loop-over-an-array shape.
// run_benchmarks.py applies a compile timeout and reports this
// combination as a failure rather than hanging the whole suite; expect
// "sieve -O2 minic: compile timed out" in benchmark output. Both issues
// are tracked as known, pre-existing bugs rather than root-caused or
// fixed here, which is out of scope for this benchmark suite.

int main() {
    int n = 40000;
    int is_composite[40000];
    int i = 0;
    while (i < n) {
        is_composite[i] = 0;
        i = i + 1;
    }

    int count = 0;
    int p = 2;
    while (p < n) {
        if (is_composite[p] == 0) {
            count = count + 1;
            int multiple = p * p;
            while (multiple < n) {
                is_composite[multiple] = 1;
                multiple = multiple + p;
            }
        }
        p = p + 1;
    }

    printf("%d\n", count);
    return 0;
}
