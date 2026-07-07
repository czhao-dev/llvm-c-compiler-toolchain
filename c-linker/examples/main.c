// This is a freestanding program: no libc, no crt0. On x86-64 Linux, the
// kernel jumps straight to `_start` with no setup beyond argc/argv/envp on
// the stack, so `_start` must invoke exit itself via a raw syscall rather
// than returning. `add` is declared here but defined in math.c -- exactly
// the "main.c calls add(), add() lives in math.c" scenario c-link exists to
// resolve. Compile and link with:
//
//   clang --target=x86_64-unknown-linux-gnu -fno-pic -fno-function-sections -c main.c math.c
//   c-link main.c.o math.c.o -o prog
//   ./prog; echo $?   # -> 5, on x86-64 Linux
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
