# Technology Stack

**Analysis Date:** 2026-06-03

## Languages

**Primary:**
- C99 (freestanding, no libc) — kernel subsystems (`Kernel/c/**/*.c`) and all userland code (`Userland/c/**/*.c`, `Userland/_loader.c`)
- x86-64 NASM assembly — interrupt handlers, context switch, syscall gate, boot loader (`Kernel/asm/*.asm`, `Kernel/loader.asm`, `Userland/asm/userlib.asm`, `Bootloader/Pure64/src/**/*.asm`)

**Secondary:**
- C (ANSI/C99, host libc) — toolchain utilities only: `Toolchain/ModulePacker/main.c`, `Bootloader/BMFS/bmfs.c`
- GNU Make — all build orchestration (`Makefile`, `Kernel/Makefile`, `Userland/Makefile`, `Image/Makefile`, `Bootloader/Makefile`, `Toolchain/Makefile`)
- Bash — build and launch scripts (`compile.sh`, `create.sh`, `run.sh`, `compile-and-run.sh`)
- GNU Linker scripts — binary layout (`Kernel/kernel.ld`, `Userland/sampleCodeModule.ld`)

## Runtime

**Environment:**
- Bare-metal x86-64, no operating system
- All code (kernel and userland) runs in ring 0, segment `CS = 0x08` (no hardware privilege separation)
- No virtual memory / no paging configured; flat physical address space

**Execution model:**
- Kernel linked at physical `0x100000`, output format `binary` (raw flat binary, not ELF)
- Userland code module loaded at physical `0x400000`; data module at `0x500000`
- Kernel heap at `0x600000`, size 8 MB (defined in `Kernel/c/kernel/kernel.c`)

## Build Container

**Image:** `agodio/itba-so-multiarch:3.1`

**Container name:** `TP_SO_2`

**Purpose:** Provides the cross-compilation toolchain. Building on the host directly will fail.

**Setup:** `create.sh` pulls the image and creates the container with `--security-opt seccomp:unconfined`, mounting the repo root at `/root` inside the container.

**Alternative (local Dockerfile):** `Dockerfile` at repo root builds an equivalent environment from `debian:bookworm-slim` with `gcc`, `nasm`, `make`, `gcc-x86-64-linux-gnu`, `binutils-x86-64-linux-gnu`, `qemu-utils`. Not the default workflow — `create.sh` uses the upstream image.

## Cross-Toolchain

**Compiler:** `x86_64-linux-gnu-gcc` (cross-GCC targeting x86-64 Linux ABI, used for bare-metal output)

**Linker:** `x86_64-linux-gnu-ld`

**Archiver:** `x86_64-linux-gnu-ar`

**Assembler:** `nasm`

**Image converter:** `qemu-img` (host tool, used in `Image/Makefile` to convert raw `.img` → `.vmdk` or `.qcow2`)

**Host compiler:** `gcc` (native, used only for `Toolchain/ModulePacker/mp.bin` and `Bootloader/BMFS/bmfs.bin`)

## Compiler Flags

**Kernel C flags** (defined in `Kernel/Makefile.inc`):
```
-m64
-fno-exceptions
-fno-asynchronous-unwind-tables
-mno-mmx -mno-sse -mno-sse2       # no SIMD/floating-point
-fno-builtin-malloc -fno-builtin-free -fno-builtin-realloc
-mno-red-zone                      # safe for interrupt handlers
-Wall -Wextra -Wmissing-prototypes -Wmissing-declarations
-Wredundant-decls -Wformat -Wstrict-prototypes -Wno-unused-parameter
-ffreestanding -nostdlib -fno-common
-std=c99
-fno-pie
```

**Userland C flags** (defined in `Userland/Makefile.inc`):
Identical to kernel flags plus `-fno-exceptions -std=c99`.

**NASM flags** (both kernel and userland): `-felf64`

**Linker flags:** `--warn-common -z max-page-size=0x1000`

**Memory manager compile-time switch:**
- `-DMM_FF` — First-Fit allocator (`Kernel/c/memoryManager/memoryManagerFF.c`)
- `-DMM_BUDDY` — Buddy System allocator (`Kernel/c/memoryManager/memoryManagerBuddy.c`)
- Controlled via `MM` make variable (`MM=FF` default, `MM=BUDDY` alternative)

## Frameworks

**Core:** None — fully bare-metal, no OS, no runtime library

**Testing:** No host-side test runner; tests are userland processes (`test_mm`, `test_proc`, `test_sync`, `test_prio`) launched from the booted shell

**Build:** GNU Make

## Key Dependencies

**No external package manager** (no `package.json`, `Cargo.toml`, `requirements.txt`, etc.)

**Runtime dependencies:** None — no libc, no POSIX, no external libraries at runtime

**Build-time toolchain (inside container):**
- `x86_64-linux-gnu-gcc` / `x86_64-linux-gnu-ld` — cross C compiler/linker
- `nasm` — x86 assembler
- `qemu-utils` — for `qemu-img` image conversion
- host `gcc` — for `ModulePacker` and `BMFS` (host-native tools)

**Vendored components:**
- `Bootloader/Pure64/` — Pure64 bootloader (Ian Seyler / Return Infinity), pre-built `.sys` binaries, source in `Bootloader/Pure64/src/`
- `Bootloader/BMFS/bmfs.c` — BareMetal File System utility (Ian Seyler / Return Infinity)

## Makefile Structure

```
Makefile                  # Top-level: sequences bootloader → kernel → userland → toolchain → image
├── Kernel/Makefile       # Builds kernel.bin (links loader.o + c/**/*.o + asm/*.o via kernel.ld)
│   └── Kernel/Makefile.inc  # Toolchain vars and all GCCFLAGS
├── Userland/Makefile     # Builds 0000-sampleCodeModule.bin and 0001-sampleDataModule.bin
│   └── Userland/Makefile.inc
├── Bootloader/Makefile   # Delegates to BMFS/Makefile and Pure64/Makefile
│   ├── Bootloader/BMFS/Makefile     # gcc -ansi -std=c99 → bmfs.bin
│   └── Bootloader/Pure64/Makefile  # nasm → bmfs_mbr.sys, pxestart.sys, pure64.sys
├── Toolchain/Makefile    # Delegates to ModulePacker/Makefile
│   └── Toolchain/ModulePacker/Makefile  # host gcc → mp.bin
└── Image/Makefile        # Runs mp.bin to pack kernel, then bmfs.bin to write disk image
                          # qemu-img to convert raw → qcow2 or vmdk
```

## Configuration

**Compile-time toggles:**
- `TARGET` make variable: `qemu` (default), `vbox`, `usb` — selects output image format
- `MM` make variable: `FF` (default), `BUDDY` — selects memory manager implementation

**No environment variables are read at runtime** (bare-metal; no env var mechanism exists at runtime)

**No config files:** All configuration is in Makefiles and scripts

## Platform Requirements

**Development (host):**
- Docker (required — all cross-compilation runs in the container)
- `qemu-system-x86_64` (for running via `./run.sh`)
- macOS or Linux host (scripts use `bash`)

**Production / emulation:**
- QEMU with IDE disk emulation (`-drive if=ide`) and 512 MB RAM
- Optional: VirtualBox (via VMDK) or bare hardware (via raw IMG + `dd`)

---

*Stack analysis: 2026-06-03*
