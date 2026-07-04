# c-linker

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![CMake](https://img.shields.io/badge/CMake-3.20%2B-064F8C.svg)](https://cmake.org)
[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

> A static linker for ELF64 x86-64 object files: merges `.text`/`.data`
> sections from multiple `.o` files, resolves symbol references across
> them, applies relocation fixups, and writes a real, runnable static ELF
> executable. No dynamic linking (no PLT/GOT), no archive (`.a`) parsing,
> no LTO — a small, fast, byte-shuffling static linker.

---

## Table of Contents

- [Overview](#overview)
- [Example](#example)
- [Architecture](#architecture)
- [Testing](#testing)
- [Repo Structure](#repo-structure)
- [Build & Run](#build--run)
- [License](#license)
- [References](#references)

---

## Overview

`c-link` takes a flat list of ELF64 x86-64 relocatable object files (`.o`,
`ET_REL` — exactly what `clang -c` produces) and links them into a single
static executable, in three steps:

1. **Merging sections.** Every input file's `.text` is concatenated into
   one continuous code block, and every `.data` into one continuous
   initialized-data block, each respecting per-file section alignment.
2. **Symbol resolution.** Every input file's symbol table is parsed; an
   undefined reference in one file (e.g. `main.o` calling `add`) is matched
   against the file that actually defines it (e.g. `math.o`). Two files
   defining the same global symbol is a `multiple definition` error; a
   reference with no definition anywhere is an `undefined symbol` error.
3. **Relocation fixups.** Once every section has a final load address, each
   input file's relocation entries are walked and the placeholder bytes
   they point at are overwritten with the real, resolved address —
   absolute (`R_X86_64_64`) or PC-relative (`R_X86_64_PC32`/`PLT32`).

It intentionally does **not**:

- Perform dynamic linking — no PLT, no GOT, no `.so`/`.dylib`. Every
  symbol must be resolvable from the input object files alone.
- Parse archive files (`.a`/`.lib`) — pass individual `.o` files, not
  libraries.
- Do any link-time optimization — it never inlines or reorders code across
  files, it only merges, resolves, and patches bytes.

**Why real ELF, not a toy format?** This linker parses the exact same
`ET_REL` object files a real compiler produces (via pointer-casting onto a
small vendored subset of the ELF64 structures — macOS doesn't ship
`<elf.h>`, so [include/elf64.h](include/elf64.h) defines the handful of
fields this linker actually reads/writes), rather than inventing a
simplified custom container. Test and example fixtures are real, small,
freestanding C sources compiled with real `clang`
(`--target=x86_64-unknown-linux-gnu`, which works from any host — including
this repo's macOS/arm64 dev environment — without a Linux sysroot, since
the fixtures never `#include` a system header). The linker's own runtime
has zero external dependencies; `clang` is only needed to build this
subproject's tests and examples.

---

## Example

```c
// math.c
long add(long a, long b) { return a + b; }
```

```c
// main.c — freestanding: no libc, no crt0, so _start must exit via a raw
// syscall itself rather than returning.
long add(long a, long b);

static void exitWith(int code) {
    __asm__ volatile("mov %0, %%edi\n\tmov $60, %%eax\n\tsyscall\n\t"
                      : : "r"(code) : "edi", "eax");
}

void _start(void) {
    exitWith((int)add(2, 3));
}
```

```
$ clang --target=x86_64-unknown-linux-gnu -fno-pic -fno-function-sections -c main.c math.c
$ c-link main.c.o math.c.o -o prog
$ ./prog; echo $?
5
```

(`./prog` only runs on x86-64 Linux — see [Build & Run](#build--run).)
The full sources are [examples/main.c](examples/main.c) and
[examples/math.c](examples/math.c).

---

## Architecture

```
input .o files (real ELF64 x86-64, ET_REL)
        │
        ▼
  readElfObject()        pointer-casts the ELF64 structures this linker
                          needs (elf64.h); extracts .text/.data/.symtab/
                          .strtab/.rela.text/.rela.data, drops everything
                          else (.rodata, .bss, .eh_frame, .debug_*, ...)
        │
        ▼
  buildSymbolTable()      records every Global defined symbol;
                          flags MultipleDefinition
        │
        ▼
  checkUndefinedSymbols() every relocation's undefined reference must
                          have a match; flags UndefinedSymbol
        │
        ▼
  mergeSections()         concatenates .text (resp. .data) across all
                          files, in order, with per-file alignment padding
        │
        ▼
  applyRelocations()      patches Abs64 (write64(S+A)) and Pc32/Plt32
                          (write32(S+A-P)) sites in place
        │
        ▼
  writeElfExecutable()    emits a real, minimal, loadable static ET_EXEC
                          ELF64 executable (two page-aligned PT_LOAD
                          segments, no section headers needed to run)
```

`linkObjects()` is the disk-free core pipeline (takes/returns in-memory
`ObjectFile`/`LinkedImage` values), so every stage is directly testable
without touching the filesystem; `link()` adds the disk I/O, and `cli.cpp`
adds argument parsing and exit-code mapping. See
[docs/SPEC.md](docs/SPEC.md) for the exact byte layouts and patch formulas.

---

## Testing

Six test executables, built with nothing more than manual `expect()`
assertions — no external test framework, matching every sibling
subproject. **All 6 suites pass**, and every one of them links or reads
*real* ELF object files compiled on the fly by the real `clang` found at
configure time — there are no checked-in binary fixtures (every
subproject's `.gitignore` already excludes `*.o`).

```
$ ctest --test-dir build --output-on-failure
Test project .../c-linker/build
    Start 1: elf_reader_test
1/6 Test #1: elf_reader_test ..................   Passed
    Start 2: symbol_resolver_test
2/6 Test #2: symbol_resolver_test .............   Passed
    Start 3: section_merger_test
3/6 Test #3: section_merger_test ..............   Passed
    Start 4: relocation_test
4/6 Test #4: relocation_test ..................   Passed
    Start 5: linker_test
5/6 Test #5: linker_test ......................   Passed
    Start 6: cli_test
6/6 Test #6: cli_test .........................   Passed

100% tests passed, 0 tests failed out of 6
```

| Suite | What it checks |
|---|---|
| `elf_reader_test` | Real `.o` parsing (`.text`/symbols), plus malformed input (bad magic, truncated file, corrupted section offset) failing gracefully instead of crashing |
| `symbol_resolver_test` | Clean single definition, multiple-definition conflict, undefined-reference detection, `static` (local) symbols never conflicting across files |
| `section_merger_test` | Multi-file concatenation offsets, alignment padding (and that padding is zero), files contributing to only one of `.text`/`.data` |
| `relocation_test` | Hand-verified `Abs64` and `Pc32`/`Plt32` patch math against real compiled relocations, plus a fabricated out-of-range case producing `RelocationOverflow` and leaving bytes untouched |
| `linker_test` | End-to-end `linkObjects()`: entry point, call-site patching, an auto-computed data base, undefined-symbol and multiple-definition failure paths |
| `cli_test` | Subprocess exercise of the built `c-link` binary: a real runnable ELF header comes out on success, exit codes 0/1/2, `--help` |

---

## Repo Structure

```
c-linker/
├── README.md
├── LICENSE
├── CMakeLists.txt
├── scripts/
│   └── configure.sh          ← cmake -S . -B build -G Ninja
├── include/
│   ├── elf64.h                 ← vendored ELF64 structs/constants (no logic)
│   ├── object_file.h           ← ObjectFile/Section/Symbol/Relocation model
│   ├── elf_reader.h            ← readElfObject()
│   ├── elf_writer.h            ← writeElfExecutable()
│   ├── diagnostic.h            ← Severity/DiagnosticCode/Diagnostic/toString()
│   ├── symbol_resolver.h       ← SymbolTable, buildSymbolTable(), checkUndefinedSymbols()
│   ├── section_merger.h        ← MergedLayout, mergeSections()
│   ├── relocation_applier.h    ← resolvedAddress(), applyRelocations()
│   ├── linker.h                ← LinkOptions/LinkedImage/LinkResult, link(), linkObjects()
│   └── cli.h                   ← LinkArgs/CliError, parseArgs(), run()
├── src/                        ← one .cpp per header above, plus main.cpp
├── tests/
│   ├── support/compile_fixture.h  ← shells out to clang to build real .o fixtures
│   ├── fixtures/                  ← real, small, freestanding C sources
│   ├── elf_reader_test.cpp, symbol_resolver_test.cpp, section_merger_test.cpp,
│   │   relocation_test.cpp, linker_test.cpp  ← in-memory / library-level
│   └── cli_test.cpp               ← subprocess exercise of c-link
├── examples/
│   └── main.c, math.c          ← the worked example above
└── docs/
    └── SPEC.md                 ← object file format, patch formulas, non-goals
```

---

## Build & Run

**Dependencies:** CMake 3.20+, a C++20 compiler, Ninja (recommended), and
`clang` on `PATH` (used only to compile this subproject's own test/example
fixtures into real ELF object files — `c-link` itself has no runtime
dependencies).

```bash
./scripts/configure.sh        # configures with Ninja
cmake --build build           # compiles libclnk_core.a, c-link, and all tests
ctest --test-dir build --output-on-failure
```

### CLI usage

```bash
# Link one or more ELF64 x86-64 relocatable object files into an executable
./build/c-link main.o math.o -o prog

# Pick a different entry symbol (default: _start)
./build/c-link start.o -o prog --entry _entry

# Override the default load addresses (must be page-aligned, i.e. multiples of 0x1000)
./build/c-link main.o math.o -o prog --base-text 0x500000 --base-data 0x700000

# Usage
./build/c-link --help
```

Exit codes: `0` linked successfully; `1` the link failed (undefined
symbol, multiple definition, or malformed/unsupported input — diagnostics
printed to stderr); `2` a usage error (no input files, missing `-o`, an
input path that doesn't exist, or an unrecognized flag).

The output is a real, minimal static ELF64 executable — `./prog` runs
directly on x86-64 Linux. This repo's dev/CI machines may not all be able
to execute it (e.g. macOS/arm64 can't run a Linux ELF binary natively),
but every byte the linker computes is verified structurally by the test
suite regardless of host.

---

## License

This project is licensed under the MIT License — see [LICENSE](LICENSE) for the full text.

---

## References

- Levine, John R. *Linkers and Loaders*. Morgan Kaufmann, 2000. — the
  primary reference for this subproject's whole design: symbol
  resolution, section merging, and relocation processing.
- Tool Interface Standard (TIS) Committee. *Executable and Linkable Format
  (ELF) Specification*, and the *System V Application Binary Interface,
  AMD64 Architecture Processor Supplement* — the normative definitions of
  the object/executable file layout and the `R_X86_64_*` relocation
  semantics this linker implements a subset of.
- [c-compiler-llvm](../c-compiler-llvm) — the sibling subproject that
  currently shells out to `clang` for final binary generation; `c-linker`
  is the piece of this toolchain's own pipeline that step would otherwise
  depend on.
