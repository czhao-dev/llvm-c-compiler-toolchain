# c-linker: Specification

This document is the normative reference for the object file subset this
linker reads, the executable it writes, the linking algorithm, and the
diagnostics it can produce. The [README](../README.md) is the quick-start
version of this document.

## 1. Overview & Scope

`c-linker` is a **static-only** linker for **ELF64, little-endian, x86-64,
relocatable object files** (`ET_REL`, `EM_X86_64`) — the same files
`clang --target=x86_64-unknown-linux-gnu -c` produces. It links a flat list
of such files into a single, real, minimal, statically-loadable ELF64
executable (`ET_EXEC`).

Non-goals, stated once here rather than repeated at every relevant section:

- **Dynamic linking.** No PLT, no GOT, no `.so`/`.dylib` input or output.
  Every symbol referenced by a relocation must be resolvable from the
  input object files alone.
- **Archive parsing.** No `.a`/`.lib` support — callers pass individual
  `.o` files.
- **Link-time optimization.** Sections and bytes are merged and patched
  verbatim; nothing is inlined, reordered, or dead-stripped.
- **Other architectures/formats.** No ELF32, no big-endian, no Mach-O, no
  ARM/RISC-V/etc. relocation types.
- **`.bss` / uninitialized globals.** All data must be explicitly
  initialized; a symbol whose definition lives in a dropped/unsupported
  section (including `.bss`) is only an error if something actually
  relocates against it (see §7, `UnsupportedInput`).
- **`STB_WEAK` symbols.** Only `STB_LOCAL` and `STB_GLOBAL` bindings are
  supported.
- **More than one `.text` or `.data` section per input file.** Fixtures
  and expected inputs are compiled with `-fno-function-sections` to
  guarantee this.

## 2. Byte Order & Conventions

All multi-byte integers in both the input object files and the output
executable are little-endian, matching x86-64. `include/elf64.h` vendors
the subset of the ELF64 on-disk structures this linker needs (not a system
`<elf.h>`, since macOS doesn't ship one) — field names and values match the
System V ABI exactly.

## 3. Input Object File Sections

Of every section in an input `ET_REL` file, only the following are read;
everything else (`.rodata`, `.bss`, `.comment`, `.note.*`, `.eh_frame`,
`.debug_*`, `.llvm_addrsig`, `.group`, ...) is looked up by name, not
found among the recognized names, and therefore silently dropped:

| Section | Required? | Purpose |
|---|---|---|
| `.text` | no (at most one) | machine code, merged verbatim |
| `.data` | no (at most one) | initialized globals, merged verbatim |
| `.symtab` | only if `.rela.*` present | symbol table |
| `.strtab` (via `.symtab`'s `sh_link`) | with `.symtab` | symbol names |
| `.rela.text` | no | relocations targeting `.text` |
| `.rela.data` | no | relocations targeting `.data` |
| section header string table (via `e_shstrndx`) | yes | section names |

A file with more than one section of a given name (e.g. two `.text`
sections from `-ffunction-sections`) is rejected with `UnsupportedInput`.

## 4. Symbol Model

Every `.symtab` entry is kept in its original index order (relocations
reference symbols by that index), translated to one of four locations:

- **Undefined** (`st_shndx == SHN_UNDEF`) — needs cross-file resolution.
- **Text** / **Data** — `st_shndx` matches this file's own `.text`/`.data`
  section index; `value` is `st_value`, the byte offset within that
  section. This covers ordinary function/object symbols *and* the
  `STT_SECTION` symbols compilers sometimes emit for local references
  (e.g. a relocation against `.data + N` instead of a named local symbol)
  — both are just "defined, at this offset, in this section" to this
  linker.
- **Other** — any other real section (`.rodata`, `.bss`, ...), `SHN_ABS`
  (e.g. `STT_FILE` symbols), or `SHN_COMMON`. Harmless unless a relocation
  actually targets one, in which case it's `UnsupportedInput`.

Binding is `STB_LOCAL` or `STB_GLOBAL`; `STB_WEAK` (or anything else) is
rejected outright at read time.

## 5. Relocation Types & Patch Semantics

Only two relocation types are supported, read from `.rela.text`/`.rela.data`:

| ELF type | Internal type | Patch |
|---|---|---|
| `R_X86_64_64` | `Abs64` | `write64(S + A)` |
| `R_X86_64_PC32`, `R_X86_64_PLT32` | `Pc32` | `write32(S + A - P)` |

Where, after every section has a final merged load address:

- **S** — the resolved symbol's final absolute address.
- **A** — the relocation's addend, taken verbatim from the file.
- **P** — the relocation site's own final absolute address, i.e. the
  address of `r_offset` itself *after merging* — **not** offset-plus-width.
  This matches the real ELF x86-64 psABI: the compiler already bakes any
  "distance to the next instruction" adjustment into `A` (a `call rel32`
  to an external symbol is emitted with `A = -4`, accounting for the
  4-byte operand it displaces past).

`R_X86_64_PLT32` is treated identically to `R_X86_64_PC32` — this linker
never builds a PLT, so there is no distinction between "call through the
PLT" and "call directly" once every symbol is statically resolved. Any
other relocation type (e.g. `R_X86_64_GOTPCREL`, which would imply PIC —
this is why fixtures are compiled `-fno-pic`) is `UnsupportedRelocationType`.

If a `Pc32` result doesn't fit in a signed 32-bit displacement, that's
`RelocationOverflow` and the site is left unpatched (the link still fails
overall).

## 6. Linked Output: a Real ELF64 Executable

The output is a minimal, real, loadable static `ET_EXEC` ELF64 x86-64
executable — runnable directly on x86-64 Linux, not a bespoke format. Its
layout, all page-aligned (`kPageSize = 0x1000`) per the ELF/Linux loader
requirement that `p_vaddr ≡ p_offset (mod p_align)` for every `PT_LOAD`
segment:

```
file offset 0            one page reserved for: ELF header, two
                          program headers, zero padding
file offset kPageSize     merged .text bytes (loaded at textBase)
                          zero padding to the next page boundary
file offset (page-aligned) merged .data bytes (loaded at dataBase)
```

Two `PT_LOAD` segments are always emitted: text (`R+X`, `p_vaddr =
textBase - kPageSize`, `p_offset = 0`, covering the header page and
`.text`) and data (`R+W`, `p_vaddr = dataBase`, page-aligned file offset).
No section headers are emitted — a stripped static executable doesn't
need them to load and run, only to be inspected by tools like `objdump`.

`e_entry` is the resolved absolute address of the entry symbol (default
`_start`, since there is no libc/crt0 to call a `main` that returns
normally — a freestanding entry point must exit via a raw syscall itself).

Default addresses: `textBase = 0x401000` (leaving exactly one page below
it, at `0x400000`, for the header), `dataBase` auto-computed as the next
page boundary after the end of `.text` unless overridden. Both must be
page-aligned; `--base-text`/`--base-data` reject anything else.

## 7. Linking Algorithm

`linkObjects()` runs these steps in order, collecting diagnostics rather
than throwing:

1. `buildSymbolTable()` — record every `Global` defined symbol from every
   file; `MultipleDefinition` for any name defined more than once.
2. `checkUndefinedSymbols()` — every relocation targeting an `Undefined`
   symbol must have a matching table entry; `UndefinedSymbol` otherwise.
   If either step produced any diagnostic, the link stops here (merging
   and relocating a symbol table that's already known to be wrong isn't
   useful, and the entry symbol isn't even looked up yet).
3. `mergeSections()` — concatenate `.text` and `.data` across all files,
   in input order, with per-file alignment padding.
4. Resolve the entry symbol (default `_start`) against the symbol table;
   `UndefinedSymbol` if missing.
5. `applyRelocations()` — patch every relocation site; `UnsupportedInput`
   for a relocation targeting a symbol outside `.text`/`.data`,
   `RelocationOverflow` for an out-of-range `Pc32` result.
6. Assemble the `LinkedImage` (entry point, merged `.text`/`.data` and
   their base addresses).

## 8. Diagnostics

All diagnostics are `Severity::Error` — a static link either fully
succeeds or fully fails, there is no warning tier.

| Code | Meaning | Exit code |
|---|---|---|
| `MalformedObjectFile` | not a well-formed ELF64 x86-64 `ET_REL` file | 1 |
| `UnsupportedInput` | a real ELF feature outside this linker's scope | 1 |
| `UnsupportedRelocationType` | a relocation type other than `Abs64`/`Pc32` | 1 |
| `UndefinedSymbol` | a reference with no definition anywhere | 1 |
| `MultipleDefinition` | a global symbol defined in more than one place | 1 |
| `RelocationOverflow` | a `Pc32` result doesn't fit in 32 bits | 1 |

CLI usage errors (missing `-o`, no inputs, unknown flag, a nonexistent
input path) are a separate `CliError` path, always exit code 2, and never
reach the diagnostics above.
