# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

MASS OS — a bare-metal x86-64 kernel (TP2 Sistemas Operativos, ITBA) that boots on QEMU/VirtualBox/real hardware via Pure64 + BMFS. Implements memory management, processes, scheduling, and syscalls from scratch.

## Build & run

All compilation happens **inside the Docker container** `TP_SO_2` (image `agodio/itba-so-multiarch:3.1`), which provides the `x86_64-linux-gnu-gcc/ld` cross-toolchain and `nasm`. Building on the host directly will fail.

| Command | Purpose |
|---------|---------|
| `./create.sh` | One-time: pull image and create `TP_SO_2` container with cwd mounted at `/root` |
| `./compile.sh` | Build with First-Fit memory manager (default), QEMU image |
| `MM=BUDDY ./compile.sh` | Build with Buddy System memory manager |
| `./compile.sh vbox` / `./compile.sh usb` | Build VMDK / raw IMG instead of qcow2 |
| `./run.sh` | Boot QEMU on `Image/x64BareBonesImage.qcow2` (fallback to `.img`), 512MB RAM, IDE disk |
| `make clean` | Clean all subprojects |

`compile.sh` does `make clean` + `make all` inside the container, propagating `TARGET` and `MM`. The MM choice is a **compile-time switch** (`-DMM_FF` or `-DMM_BUDDY`); a single image embeds exactly one allocator. To compare allocators, rebuild and rerun.

If `./run.sh` fails with "permission denied" on the image, the container wrote it as root: `sudo chown $USER Image/x64BareBonesImage.qcow2`.

### Tests

There is no host-side test runner. The catedra test suite ships as **userland processes** launched from inside the booted shell (not built-ins), so they accept `&` (background) and `|` (pipe):

```
test_mm <max_memory>   # MM stress test: random alloc/free, checks no overlap
test_proc <max_procs>  # creates/blocks/unblocks/kills dummy processes
test_sync <n> <use_sem># N pairs inc/dec a shared var; expect 0 with semaphores
test_prio <target>     # 3 processes race to a target with different priorities
ps                     # list processes (built-in)
bmFPS / bmCPU / bmMEM / bmKEY  # benchmarks (built-ins)
```

To test both allocators, rebuild with each `MM` value and rerun `test_mm`.

## Architecture

Four top-level components, each with its own Makefile, assembled into a single bootable disk image by `Image/Makefile`:

- **Bootloader/** — Pure64 + BMFS (vendored). Loads the packed kernel from disk into memory and jumps to it. Rarely touched.
- **Toolchain/ModulePacker** — Host-side tool that concatenates `kernel.bin` + userland modules into `packedKernel.bin` for the bootloader.
- **Kernel/** — The kernel proper. Linked at `0x100000` via `kernel.ld`, output as raw `binary` (not ELF). Entry: `loader.asm` → `kernelMain` → `initializeKernelBinary` → `main` (calls `scheduler_start`, never returns).
- **Userland/** — Compiled into two modules loaded by the kernel at fixed addresses:
  - `0000-sampleCodeModule.bin` → loaded at `0x400000`, this is the **shell** (the first foreground process spawned)
  - `0001-sampleDataModule.bin` → loaded at `0x500000`

The kernel heap lives at `0x600000` and is 8 MB (see `HEAP_START`/`HEAP_SIZE` in `Kernel/c/kernel.c`).

### Boot flow

`loader.asm` → `kernelMain` (sets up GDT/segments) → `initializeKernelBinary` in `kernel.c`:
1. `loadModules` copies userland modules from end-of-kernel to their fixed load addresses.
2. `clearBSS`, `load_idt`.
3. `mm_init(HEAP_START, HEAP_SIZE)`.
4. `process_init`, `scheduler_init`, `sem_init`, `pipe_init`.
5. Creates `idle` process (just `hlt`s, lowest priority, registered as the scheduler's fallback via `scheduler_set_idle`) and the `shell` process (foreground, entry = `0x400000`).
6. Returns; assembly then calls `main()` → `scheduler_start()` → `scheduler_start_asm` (loads first PCB's `rsp`, `popState`, `iretq` into userland).

### Memory manager

`Kernel/include/memoryManager.h` defines a stable interface; `Kernel/c/memoryManager/` has two implementations selected by the `MM` make variable:

- `memoryManagerFF.c` — First-Fit free list
- `memoryManagerBuddy.c` — Buddy system (power-of-two splits/coalesces)

Both expose `mm_init / mm_malloc / mm_malloc_kernel / mm_free / mm_status`. The `_kernel` variant excludes the allocation from `alloc_count` so `mm_status` reflects only userland-visible allocations made via `sys_malloc`.

### Processes & scheduler

- `process.c` — fixed-size PCB table (`MAX_PROCESSES = 64`), 4 KB stacks (`mm_malloc`'d), priorities 1–5, states `READY/RUNNING/BLOCKED/ZOMBIE`. PIDs, fd[2] (stdin/stdout), parent_pid, waitpid support.
- `scheduler.c` — round-robin with priority-derived quanta. `scheduler_tick` is called from the timer IRQ (irq00) in `interrupts.asm`; cooperative yield via `int 0x80` with `force_switch`. Both handlers in `interrupts.asm` save/restore full register state on the process's stack and pass `rsp` to/from C.
- The scheduler is the kernel's main loop — `scheduler_start_asm` is the only path that ever leaves the kernel into userland.

### Sync & IPC

- `semaphore.c` — named semaphores (`MAX_SEMS = 32`), shared by agreeing on a name a priori. One global `sem_lock` (taken via `xchg` in `libasm.asm`) guards the table; `sem_wait`/`sem_post` block/wake processes via a per-sem PID wait-queue. No busy-waiting.
- `pipe.c` — unidirectional blocking pipes (anonymous via `pipe_create`, or named via `pipe_open`). Each pipe is a circular buffer guarded by 3 semaphores (`data`/`space`/`mutex`) — classic producer/consumer. fds are encoded by base offset (`PIPE_FD_READ_BASE`/`PIPE_FD_WRITE_BASE`); `pipe_is_fd` distinguishes them from stdin/stdout (0/1). EOF when `writers == 0`, broken pipe when `readers == 0`.

### Syscalls

`Kernel/include/syscallDispatcher.h` lists all syscalls (`CANT_SYS = 39` in `defs.h`): 0–18 video/audio/memory, 19–28 processes, 29–32 semaphores, 33–36 pipes, 37 write-with-color, 38 console cursor on/off. Dispatched via `int 0x80` through the `syscalls[]` table in `syscallDispatcher.c` (the asm gate in `interrupts.asm` bounds-checks `rax` against the literal `39`). Userland calls them via `Userland/asm/userlib.asm` wrappers and `Userland/c/userlib.c` C wrappers. Adding a syscall requires: bumping `CANT_SYS` (and the literal in `_irq128Handler`), writing the `sys_*` function, adding it to the dispatcher table, and exposing a wrapper in `userlib`.

### Userland linkage

Userland code is freestanding and links against only `userlib` — no libc. Userland ships as a single flat binary loaded at `0x400000`; `_loader.c` calls the shell's `main`. There is **one address space** for all userland — there is no per-process binary and no hardware privilege separation (everything, kernel and userland, runs in ring 0 with `CS = 0x08`; see `build_initial_stack` in `process.c`).

Shell commands fall into two classes (see the `commands[]` table in `userlib.c`):
- **Built-ins** (`help`, `clear`, `ps`, `mem`, `kill`, `nice`, `block`, benchmarks, …) run synchronously in the shell process; they do **not** accept `&` or `|`.
- **Processes** (`test_mm`, `test_proc`, `test_sync`, `test_prio`, `cat`, `wc`, `filter`, `loop`, `mvar`) are spawned via `sys_create_process` as real PCB-backed processes, so they run in fg/bg and can be connected with a pipe. They share the shell's address space (no separate binary) but are independently scheduled.

## Conventions

- Code, comments, and commit messages are in **Spanish** (the repo is a course assignment at ITBA). Match this style when editing.
- Kernel C is compiled `-ffreestanding -nostdlib -mno-red-zone -fno-pie -std=c99` with strict warnings (`-Wall -Wextra -Wmissing-prototypes -Wstrict-prototypes`). No SSE/MMX/floating-point. Don't pull in libc.
- Headers are always in `Kernel/include/` or `Userland/c/include/`; sources include them as `#include "foo.h"` (the Makefiles add `-I./include` / `-Ic/include`).
- `make clean` is invoked before every `compile.sh` run, so incremental builds across MM changes are safe.
