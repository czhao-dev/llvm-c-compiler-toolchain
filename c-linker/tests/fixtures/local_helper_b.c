// See local_helper_a.c.
static long helper(void) { return 2; }
long useHelperB(void) { return helper(); }
