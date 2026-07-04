// A `static` (file-local) symbol named `helper`, deliberately duplicated in
// local_helper_b.c with a different body -- local symbols never enter the
// cross-file symbol table, so linking both files together must not report
// a multiple-definition conflict, and each file's own call must resolve to
// its own definition.
static long helper(void) { return 1; }
long useHelperA(void) { return helper(); }
