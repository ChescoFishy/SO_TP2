# Coding Conventions

**Analysis Date:** 2026-06-03

## Language Convention

**Spanish throughout.** This is an ITBA course assignment — code comments, identifiers in comments, and **commit messages** are written in Spanish. Match this style when editing. Commits follow a loose conventional-commit prefix in English (`refactor:`, `fix:`, `add:`, `chore:`) with a Spanish/English body.

## Naming Patterns

**Files:**
- camelCase `.c`/`.h` for kernel modules: `memoryManagerBuddy.c`, `syscallDispatcher.c`, `naiveConsole.c`, `keyboardDriver.c`
- Mixed in userland: snake_case test files (`test_proc.c`, `test_sync.c`, `test_prio.c`) alongside camelCase (`testMM.c`, `userlib.c`)
- `.asm` for NASM, `.ld` for linker scripts

**Functions:**
- snake_case with a subsystem prefix: `mm_malloc`, `mm_free`, `mm_status`, `sem_wait`, `sem_post`, `scheduler_tick`, `scheduler_start`, `pipe_create`, `process_init`
- Kernel syscall implementations prefixed `sys_*` (`sys_malloc`, `sys_create_process`)
- The `_kernel` variant convention: `mm_malloc_kernel` excludes the allocation from `alloc_count` so `mm_status` reflects only userland-visible allocations

**Variables:**
- snake_case for locals and fields
- UPPER_SNAKE_CASE for `#define` constants: `MAX_PROCESSES`, `STACK_SIZE`, `HEAP_START`, `HEAP_SIZE`, `CANT_SYS`, `PIPE_FD_READ_BASE`

**Types:**
- PascalCase for typedef'd structs (`Command`, `ProcessEntry`)
- enum-style states in UPPER_CASE (`READY`, `RUNNING`, `BLOCKED`, `ZOMBIE`)

## Code Style

**Compiler (kernel — `Kernel/Makefile.inc`):**
```
-m64 -ffreestanding -nostdlib -fno-pie -std=c99
-mno-red-zone
-mno-mmx -mno-sse -mno-sse2          # NO SIMD / floating-point
-fno-builtin-malloc -fno-builtin-free -fno-builtin-realloc
-fno-exceptions -fno-asynchronous-unwind-tables -fno-common
-Wall -Wextra -Wmissing-prototypes -Wmissing-declarations
-Wredundant-decls -Wformat -Wstrict-prototypes -Wno-unused-parameter
```
Userland uses identical flags. NASM: `-felf64`. Linker: `--warn-common -z max-page-size=0x1000`.

**Hard constraints (do not violate):**
- **No libc** — freestanding, `-nostdlib`. Don't `#include <stdio.h>` etc. Use `Kernel/c/lib/lib.c` / `Userland/c/lib/userlib.c` helpers instead.
- **No floating-point / SSE/MMX** — the build disables them; using a float will break codegen or fault.
- **`-mno-red-zone`** — required because interrupt handlers run on the same stack.
- Strict prototype warnings are on — declare every function in its header.

## Import Organization

- Sources include headers as `#include "foo.h"` (bare filename). Makefiles supply `-I./include` (kernel) / `-Ic/include` (userland), so headers resolve without path prefixes despite living in module subdirectories.
- Headers always live in `Kernel/include/<module>/` or `Userland/include/<module>/`, never beside the `.c`.

## Error Handling

**Patterns:**
- No exceptions (`-fno-exceptions`). Errors propagate via return codes — negative or sentinel values.
- Syscalls return status; userland wrappers check the return.
- CPU faults handled in `Kernel/c/exceptions/exceptions.c` (print diagnostics; no unwinding).
- The `int 0x80` gate silently ignores out-of-range syscall numbers (bounds-checked against literal `37`).

## Logging

- No logging framework. Output goes through the video driver / `naiveConsole.c` and userland print helpers.
- Diagnostics during boot/exceptions printed directly to screen.

## Comments

**When to Comment:**
- Comments are in Spanish, often explaining *why* (e.g. the manual stack-frame construction in `process.c` is heavily commented to explain the `iretq` layout).
- Density is moderate-to-high in tricky asm-adjacent code (interrupts, stack setup, locks) and lighter elsewhere.
- Match surrounding density: explain non-obvious bare-metal invariants, skip narrating obvious statements.

**TODO Comments:**
- No active `TODO`/`FIXME` markers in the source as of this analysis. (Note: the word "TODOS" appears in a Spanish comment in `memoryManagerBuddy.c` — it means "all", not a TODO marker.)

## Function Design

- Subsystem functions kept focused; asm bridges (`pushState`/`popState`) isolate register handling from C.
- Hand-rolled trap-frame construction lives in one place (`build_initial_stack`, `process.c:58`) — don't duplicate it.

## Module Design

**Two-class command model (userland):** the `commands[]` table in `Userland/c/lib/userlib.c` distinguishes:
- **Built-ins** (`help`, `clear`, `ps`, `mem`, `kill`, `nice`, `block`, benchmarks) — run synchronously in the shell process; do **not** accept `&` or `|`.
- **Processes** (`test_mm`, `test_proc`, `test_sync`, `test_prio`, `cat`, `wc`, `filter`, `loop`, `mvar`) — spawned via `sys_create_process`, run in fg/bg, pipeable. They share the shell's address space (one userland binary) but are independently scheduled.

**Adding a syscall** (full checklist):
1. Bump `CANT_SYS` in `Kernel/include/lib/defs.h`
2. Update the literal `37` in `_irq128Handler` (`Kernel/asm/interrupts.asm`) to match
3. Write the `sys_*` function in `syscallDispatcher.c`
4. Add it to the `syscalls[]` table
5. Expose a wrapper in `Userland/asm/userlib.asm` and a C wrapper in `Userland/c/lib/userlib.c`

**Memory manager** is a compile-time strategy: one image embeds exactly one allocator (`-DMM_FF` or `-DMM_BUDDY`). Both implementations must keep the `Kernel/include/memoryManager/memoryManager.h` interface stable.

---

*Convention analysis: 2026-06-03*
*Update when patterns change*
