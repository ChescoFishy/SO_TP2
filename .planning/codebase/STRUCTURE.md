# Codebase Structure

**Analysis Date:** 2026-06-03

## Directory Layout

```
so-tp2/
├── Bootloader/           # Vendored Pure64 + BMFS (rarely touched)
│   ├── BMFS/            # BareMetal File System host utility (bmfs.c → bmfs.bin)
│   └── Pure64/          # Pure64 bootloader (asm source + prebuilt .sys)
├── Kernel/              # The kernel proper (linked at 0x100000, raw binary)
│   ├── asm/            # interrupts.asm (handlers/gate), libasm.asm (port I/O, xchg lock)
│   ├── c/              # Kernel C, grouped by logical module
│   ├── include/        # Kernel headers, mirrors c/ module grouping
│   ├── loader.asm      # First C-handoff: GDT/segments → kernelMain
│   ├── kernel.ld       # Linker script (load at 0x100000, output binary)
│   └── Makefile(.inc)  # Build kernel.bin + all GCCFLAGS
├── Userland/            # Shell + spawnable processes (freestanding, userlib only)
│   ├── asm/            # userlib.asm (syscall wrappers)
│   ├── c/              # Userland C, grouped by module
│   ├── include/        # Userland headers
│   ├── _loader.c       # Calls the shell's main
│   └── sampleCodeModule.ld
├── Toolchain/           # Host-side ModulePacker (concatenates kernel + modules)
│   └── ModulePacker/   # main.c → mp.bin (host gcc)
├── Image/               # Image/Makefile assembles the bootable disk image
├── CLAUDE.md            # Project guidance
├── create.sh            # One-time: pull image, create TP_SO_2 container
├── compile.sh           # Build (MM=FF|BUDDY, TARGET=qemu|vbox|usb)
└── run.sh               # Boot QEMU on the produced image
```

## Directory Purposes

**Kernel/c/**
- Purpose: All kernel C sources, grouped by logical module (recently reorganized — see branch `reorganization`)
- Contains: one subdirectory per subsystem
- Key files & subdirectories:
  - `kernel/kernel.c` (`initializeKernelBinary`, heap constants), `kernel/moduleLoader.c`
  - `memoryManager/memoryManagerFF.c`, `memoryManager/memoryManagerBuddy.c`
  - `process/process.c`, `process/scheduler.c`
  - `ipc/semaphore.c`, `ipc/pipe.c`
  - `syscalls/syscallDispatcher.c`
  - `interrupts/irqDispatcher.c`, `interrupts/idtLoader.c`, `exceptions/exceptions.c`
  - `drivers/videoDriver.c`, `drivers/keyboardDriver.c`, `sound/sound.c`, `time/time.c`
  - `console/naiveConsole.c`, `lib/lib.c`

**Kernel/include/**
- Purpose: Kernel headers, mirroring the `c/` module grouping
- Contains: `*.h` in subdirectories (`process/`, `ipc/`, `memoryManager/`, `syscalls/`, `interrupts/`, `drivers/`, `lib/`, `console/`, `sound/`, `time/`, `exceptions/`, `kernel/`)
- Key files: `lib/defs.h` (`CANT_SYS 37`), `process/process.h` (`MAX_PROCESSES`, `STACK_SIZE`), `ipc/pipe.h` (`MAX_PIPES`, fd bases), `memoryManager/memoryManager.h` (allocator interface)
- Included as `#include "foo.h"` — Makefiles add `-I./include`

**Userland/c/**
- Purpose: Shell and spawnable processes, grouped by module
- Key subdirectories: `shell/shell.c`, `lib/userlib.c` (commands[] table), `syscall/syscall.c`, `tests/` (testMM, test_proc, test_sync, test_prio, test_util)

**Userland/include/**
- Purpose: Userland headers (`lib/`, `shell/`, `syscall/`, `memoryManager/`, `tests/`)
- Included via `-Ic/include`

**Bootloader/, Toolchain/, Image/**
- Purpose: Vendored boot stack, host packer, image assembly — touched rarely

## Key File Locations

**Entry Points:**
- `Kernel/loader.asm` — kernel entry, sets up GDT/segments
- `Kernel/c/kernel/kernel.c` (`initializeKernelBinary`) — C initialization
- `Userland/_loader.c` — userland entry, calls shell main
- `Kernel/asm/interrupts.asm` — IRQ/exception/syscall entry points

**Configuration:**
- `Kernel/Makefile.inc`, `Userland/Makefile.inc` — toolchain vars + all GCCFLAGS
- `Kernel/kernel.ld`, `Userland/sampleCodeModule.ld` — link/layout
- `compile.sh` / `run.sh` / `create.sh` — build/run orchestration (MM, TARGET vars)

**Core Logic:**
- `Kernel/c/process/scheduler.c` — the kernel main loop
- `Kernel/c/process/process.c` — PCB table, stack construction
- `Kernel/c/syscalls/syscallDispatcher.c` — syscall table
- `Kernel/c/memoryManager/` — two allocator implementations

**Testing:**
- `Userland/c/tests/` — test processes launched from the shell (no host runner)

**Documentation:**
- `CLAUDE.md` — project + workflow guidance

## Naming Conventions

**Files:**
- camelCase `.c`/`.h` for kernel modules (`memoryManagerBuddy.c`, `syscallDispatcher.c`, `naiveConsole.c`)
- snake_case `.c` for some userland tests (`test_proc.c`, `test_sync.c`, `test_prio.c`) alongside camelCase (`testMM.c`, `userlib.c`)
- `.asm` for NASM sources, `.ld` for linker scripts
- UPPERCASE for repo-level docs (`CLAUDE.md`)

**Directories:**
- PascalCase for top-level components (`Kernel/`, `Userland/`, `Bootloader/`, `Toolchain/`, `Image/`)
- lowercase for module subdirectories (`process/`, `ipc/`, `memoryManager/`, `drivers/`)
- Parallel `c/` and `include/` trees mirror each other

**Functions/Identifiers:**
- snake_case with subsystem prefix (`mm_malloc`, `sem_wait`, `sem_post`, `scheduler_tick`, `pipe_create`, `sys_*`)
- UPPER_SNAKE_CASE for `#define` constants (`MAX_PROCESSES`, `HEAP_START`, `CANT_SYS`, `PIPE_FD_READ_BASE`)

## Where to Add New Code

**New kernel subsystem:**
- Source: `Kernel/c/<module>/<name>.c`
- Header: `Kernel/include/<module>/<name>.h` (include as `"<name>.h"`)
- Wire into build: covered automatically by `Kernel/Makefile` glob, but verify

**New syscall:**
- Bump `CANT_SYS` in `Kernel/include/lib/defs.h` AND the literal `37` in `_irq128Handler` (`Kernel/asm/interrupts.asm:296`)
- Write `sys_*` in `Kernel/c/syscalls/syscallDispatcher.c`, add to `syscalls[]`
- Expose wrapper in `Userland/asm/userlib.asm` + `Userland/c/lib/userlib.c`

**New shell command:**
- Built-in (synchronous, no `&`/`|`): add to `commands[]` in `Userland/c/lib/userlib.c`, implement inline
- Process (fg/bg, pipeable): add to `commands[]`, spawn via `sys_create_process`

**New test:**
- `Userland/c/tests/<name>.c` + header in `Userland/include/tests/`, register as a process command

## Special Directories

**Image/**
- Purpose: Build output — bootable disk image (`x64BareBonesImage.qcow2` / `.img` / `.vmdk`)
- Source: Assembled by `Image/Makefile` (ModulePacker + BMFS + qemu-img)
- Committed: No — build artifacts are gitignored

**Bootloader/Pure64/, Bootloader/BMFS/**
- Purpose: Vendored third-party boot stack (Return Infinity / Ian Seyler)
- Source: External; prebuilt `.sys` binaries committed
- Committed: Yes (vendored source of truth)

---

*Structure analysis: 2026-06-03*
*Update when directory structure changes*
