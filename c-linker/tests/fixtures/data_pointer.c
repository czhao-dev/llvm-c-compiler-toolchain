// A .data global whose initializer is the address of another global --
// this forces the compiler to emit an R_X86_64_64 relocation inside
// .data itself (a pointer is 8 bytes, so it can never be a 32-bit
// relocation), exercising relocation fixups outside of .text.
long staged_value = 100;
long *ptr_to_staged = &staged_value;
