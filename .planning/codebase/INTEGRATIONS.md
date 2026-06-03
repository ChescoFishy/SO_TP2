# External Integrations

**Analysis Date:** 2026-06-03

## Bootloader

**Pure64:**
- Role: First-stage boot firmware. Loaded by the MBR, initializes the CPU to 64-bit long mode, sets up GDT, initializes SMP, and loads the BMFS-resident `packedKernel.bin` into memory, then jumps to it.
- Binaries: `Bootloader/Pure64/pure64.sys` (main loader), `Bootloader/Pure64/bmfs_mbr.sys` (MBR sector), `Bootloader/Pure64/pxestart.sys` (PXE variant)
- Source: `Bootloader/Pure64/src/pure64.asm` and subdirectory `.asm` files
- Build: `nasm` via `Bootloader/Pure64/Makefile`
- Origin: Return Infinity (Ian Seyler), vendored

**BMFS (BareMetal File System):**
- Role: Minimal flat filesystem used as the disk image container. The BMFS host utility (`Bootloader/BMFS/bmfs.bin`) creates and writes the disk image by embedding the MBR, Pure64, and the packed kernel as a BMFS-formatted disk.
- Source: `Bootloader/BMFS/bmfs.c`
- Binary: `Bootloader/BMFS/bmfs.bin` (host tool, built with native `gcc`)
- Build: `gcc -ansi -std=c99` via `Bootloader/BMFS/Makefile`
- Origin: Return Infinity (Ian Seyler), vendored
- Disk layout: MBR (512 bytes) + Pure64 stage + BMFS directory (4096 bytes at offset 4096) + 2 MiB blocks
- Minimum disk size: 6 MiB (`6 * 1024 * 1024` bytes, defined in `bmfs.c`)

## ModulePacker (Toolchain)

**Role:** Host-side binary packing tool. Concatenates `Kernel/kernel.bin` + `Userland/0000-sampleCodeModule.bin` + `Userland/0001-sampleDataModule.bin` into a single `Image/packedKernel.bin` that the bootloader loads and the kernel unpacks at runtime.

**Binary:** `Toolchain/ModulePacker/mp.bin`

**Source:** `Toolchain/ModulePacker/main.c`, `Toolchain/ModulePacker/modulePacker.h`

**Build:** native `gcc` (host compiler, not cross), via `Toolchain/ModulePacker/Makefile`

**Format:** The packed binary consists of:
1. Raw `kernel.bin` bytes
2. A 4-byte `int` recording how many extra modules follow
3. For each module: a 4-byte `uint32_t` size, then the raw module bytes

**Invoked by:** `Image/Makefile` as `$(MP) $(KERNEL) $(USERLAND) -o $(PACKEDKERNEL)`

**Kernel-side unpacker:** `Kernel/c/kernel/moduleLoader.c` — `loadModules()` reads the packed format and copies each module to its fixed address (`0x400000`, `0x500000`)

## Disk Image Formats

**Raw IMG (`x64BareBonesImage.img`):**
- Format: Raw binary sector dump
- Size: `IMGSIZE = 6291456` bytes (6 MiB), set in `Image/Makefile`
- Created by: `Bootloader/BMFS/bmfs.bin initialize <size> <mbr> <pure64> <packedKernel>`
- Used for: USB physical boot (`dd if=...img of=/dev/sdX`)

**qcow2 (`x64BareBonesImage.qcow2`):**
- Format: QEMU Copy-On-Write v2
- Created by: `qemu-img convert -f raw -O qcow2 $(IMG) $(QCOW2)` in `Image/Makefile`
- Default QEMU target; lighter than raw for QEMU usage
- Path: `Image/x64BareBonesImage.qcow2`

**VMDK (`x64BareBonesImage.vmdk`):**
- Format: VMware Virtual Machine Disk
- Created by: `qemu-img convert -f raw -O vmdk $(IMG) $(VMDK)` in `Image/Makefile`
- Used for: VirtualBox (`compile.sh vbox`)
- Path: `Image/x64BareBonesImage.vmdk`

**Image build pipeline:**
```
kernel.bin + userland .bin files
        → mp.bin (ModulePacker)
        → packedKernel.bin
        → bmfs.bin initialize (writes MBR + Pure64 + packedKernel into raw .img)
        → qemu-img convert → .qcow2 or .vmdk
```

## Emulation / Virtualization Targets

**QEMU (`qemu-system-x86_64`):**
- Version: any recent build with IDE disk support
- Command: `qemu-system-x86_64 -drive file=<img>,format=<fmt>,if=ide -m 512`
- RAM: 512 MB
- Disk interface: IDE (`if=ide`)
- Audio: optional PC speaker via `-audiodev pa,id=snd0 -machine pcspk-audiodev=snd0` (PulseAudio); falls back silently if unavailable
- Logs: `Image/qemu.log`, `Image/serial.log` (present in repo)
- Run via: `./run.sh` or `./compile-and-run.sh`

**VirtualBox:**
- Interface: VMDK file attached as an IDE disk
- No automated launch; `run.sh vbox` prints manual attachment instructions
- Requires: manually create a VM in VirtualBox and attach `Image/x64BareBonesImage.vmdk`

**Real hardware / USB:**
- Interface: raw `Image/x64BareBonesImage.img` written to USB with `dd`
- `run.sh usb` prints the `dd` command; no automated flashing

## Hardware Interfaces (Kernel Drivers)

**Video — VBE Framebuffer:**
- Driver: `Kernel/c/drivers/videoDriver.c`
- Interface: VESA BIOS Extensions (VBE) mode info structure read from memory set up by Pure64; direct write to linear framebuffer address
- Capabilities: pixel drawing, font rendering (bitmap font in `Kernel/include/console/font.h`), scrolling

**Keyboard — PS/2 via I/O ports:**
- Driver: `Kernel/c/drivers/keyboardDriver.c`
- Interface: x86 I/O port reads (scancode table `kbd_min[]`, `kbd_mayus[]`); IRQ 1 triggers `_irq01Handler` in `Kernel/asm/interrupts.asm`
- Blocking read: kernel blocks the waiting process and unblocks on IRQ

**PC Speaker / PIT Channel 2:**
- Driver: `Kernel/c/sound/sound.c`
- Interface: PIT control port (`0x43`), PIT channel 2 data port, PC speaker gate port
- Capability: square wave tone generation at arbitrary frequency; used by `startSpeaker(freq)` / `turnOff()`

**Timer — PIT Channel 0 (IRQ 0):**
- Handler: `_irq00Handler` in `Kernel/asm/interrupts.asm`
- Calls `scheduler_tick` (C) on every tick for round-robin preemption
- Used by `Kernel/c/time/time.c` for `sys_sleep` / elapsed-time tracking

**PIC (Programmable Interrupt Controller):**
- Configured in `Kernel/asm/interrupts.asm` via `picMasterMask` / `picSlaveMask`
- IRQs 0–7 mapped to IDT entries 32–39 (master PIC), IRQs 8–15 to 40–47 (slave PIC)
- Syscall gate at IDT entry 128 (`int 0x80`)

**IDT (Interrupt Descriptor Table):**
- Loaded by `Kernel/c/interrupts/idtLoader.c` (`load_idt()`)
- Dispatchers: `Kernel/c/interrupts/irqDispatcher.c`, `Kernel/c/exceptions/exceptions.c`, `Kernel/c/syscalls/syscallDispatcher.c`
- Gate: `_irq128Handler` in `Kernel/asm/interrupts.asm` — bounds-checks `rax` against literal `37` (= `CANT_SYS`) then calls `syscalls[rax]()`

## Syscall Interface

**Mechanism:** `int 0x80` from userland (ring 0 gate); syscall number in `rax`, args in `rdi`, `rsi`, `rdx`, `r10`, `r8`, `r9`

**Userland wrappers:**
- Assembly stubs: `Userland/asm/userlib.asm` — raw `int 0x80` wrappers
- C wrappers: `Userland/c/lib/userlib.c` and `Userland/c/syscall/syscall.c`
- Header: `Userland/include/lib/userlib.h`, `Userland/include/syscall/syscall.h`

**Dispatcher table:** `Kernel/c/syscalls/syscallDispatcher.c` — `syscalls[]` array of 37 function pointers

**Count:** `CANT_SYS = 37` (defined in `Kernel/include/lib/defs.h`)

**Ranges:**
- 0–18: video, audio, memory (`sys_draw_pixel`, `sys_print`, `sys_malloc`, `sys_free`, …)
- 19–28: process management (`sys_create_process`, `sys_exit`, `sys_waitpid`, `sys_getpid`, …)
- 29–32: semaphores (`sys_sem_open`, `sys_sem_wait`, `sys_sem_post`, `sys_sem_close`)
- 33–36: pipes (`sys_pipe_create`, `sys_pipe_open`, `sys_pipe_read`, `sys_pipe_write`)

## Docker Integration

**Image:** `agodio/itba-so-multiarch:3.1` (multi-arch, pulled from Docker Hub)

**Container lifecycle:**
- Created once by `create.sh` with `-v "${PWD}":/root` (bind-mount repo root) and `tail -f /dev/null` keepalive
- Started as needed by `compile.sh` before `docker exec`
- `compile.sh` runs `docker exec -u root TP_SO_2 bash -lc "cd /root && make clean && ... && make all"`

**Security:** `--security-opt seccomp:unconfined` required (container writes disk images)

**File ownership:** Container runs as root; disk images written inside container are owned by root on host. Fix: `sudo chown $USER Image/x64BareBonesImage.qcow2`

## CI/CD & Deployment

**CI Pipeline:** None — no automated CI configured

**Hosting:** Not applicable (bare-metal kernel; "deployment" = booting a disk image)

---

*Integration audit: 2026-06-03*
