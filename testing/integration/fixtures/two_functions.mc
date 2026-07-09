// Deliberately libc-free (no printf) -- see minic_clinker_smoke.py for
// why: c-link has no dynamic linking, so nothing calling into libc could
// be linked by it at all.
//
// minic's CLI rejects multiple input files and its parser rejects a
// prototype-only (no-body) function declaration, so this single file
// defines both functions together (letting minic's own parser/sema
// accept it), and minic_clinker_smoke.py splits the resulting --emit-ir
// LLVM IR text in two post-hoc -- there is no way to get minic to treat
// these as separate translation units today.

int helper(int a, int b) {
    return a + b;
}

int main() {
    return helper(2, 3);
}
