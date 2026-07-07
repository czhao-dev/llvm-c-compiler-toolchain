// Freestanding entry point: no libc, no crt0. Declares `add` but does not
// define it -- the definition lives in math.c, so linking this file alone
// produces an undefined-symbol error, and linking it with math.o resolves
// the call and its relocation.
long add(long a, long b);

static void exitWith(int code) {
    __asm__ volatile("mov %0, %%edi\n\t"
                      "mov $60, %%eax\n\t" // sys_exit on x86-64 Linux
                      "syscall\n\t"
                      :
                      : "r"(code)
                      : "edi", "eax");
}

void _start(void) {
    long result = add(2, 3);
    exitWith((int)result);
}
