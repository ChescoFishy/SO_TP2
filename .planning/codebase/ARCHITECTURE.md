# Architecture

**Analysis Date:** 2026-06-03

## Pattern Overview

**Overall:** Monolithic bare-metal x86-64 kernel with a scheduler-driven main loop and a cooperative + preemptive multitasking userland.

**Key Characteristics:**
- Single raw flat binary (not ELF), linked at physical `0x100000`, no OS underneath
- Everything (kernel and userland) runs in ring 0, `CS = 0x08` — no hardware privilege separation
- One flat physical address space, no paging, no virtual memory
- The scheduler *is* the main loop: once `scheduler_start` runs, control never returns to `main`
- Interrupt-driven: timer IRQ (irq00) preemption + `int 0x80` syscall gate / cooperative yield
- Compile-time memory-manager switch (First-Fit vs Buddy) — exactly one allocator per image

## Layers

**Boot / Initialization Layer:**
- Purpose: Bring the CPU into a known state and hand control to C
- Contains: `Kernel/loader.asm` (sets up GDT/segments), `Kernel/c/kernel/kernel.c` (`initializeKernelBinary`), `Kernel/c/kernel/moduleLoader.c` (`loadModules`)
- Depends on: Pure64 bootloader having already entered long mode and loaded `packedKernel.bin`
- Used by: Nothing after boot — runs once, then yields to the scheduler

**Interrupt / Trap Layer (asm ↔ C bridge):**
- Purpose: Save/restore full register state and bridge hardware events into C
- Contains: `Kernel/asm/interrupts.asm` (irq handlers, `_irq128Handler` syscall gate, context switch), `Kernel/c/interrupts/irqDispatcher.c`, `Kernel/c/interrupts/idtLoader.c`, `Kernel/c/exceptions/exceptions.c`
- Depends on: scheduler (to pick the next `rsp`), syscall dispatcher
- Used by: CPU (IDT entries) — entry point for every timer tick, exception, and syscall

**Core Kernel Services Layer:**
- Purpose: Memory, processes, scheduling, IPC, sync
- Contains: `Kernel/c/memoryManager/` (FF + Buddy), `Kernel/c/process/process.c`, `Kernel/c/process/scheduler.c`, `Kernel/c/ipc/semaphore.c`, `Kernel/c/ipc/pipe.c`
- Depends on: memory manager (everything allocates from the 8 MB heap), asm primitives (`xchg` lock)
- Used by: syscall dispatcher, boot layer

**Syscall Layer (kernel boundary):**
- Purpose: Single controlled entry from userland into kernel services
- Contains: `Kernel/c/syscalls/syscallDispatcher.c` (the `syscalls[]` table), `Kernel/include/syscalls/syscallDispatcher.h`
- Depends on: all core kernel services + drivers
- Used by: userland via `int 0x80` through `_irq128Handler`

**Driver Layer:**
- Purpose: Talk to hardware
- Contains: `Kernel/c/drivers/videoDriver.c`, `Kernel/c/drivers/keyboardDriver.c`, `Kernel/c/sound/sound.c`, `Kernel/c/time/time.c`, `Kernel/c/console/naiveConsole.c`
- Depends on: port I/O primitives in `Kernel/asm/libasm.asm`
- Used by: syscall layer

**Userland Layer:**
- Purpose: The shell and the spawnable processes
- Contains: `Userland/_loader.c`, `Userland/c/shell/shell.c`, `Userland/c/lib/userlib.c`, `Userland/c/syscall/syscall.c`, `Userland/asm/userlib.asm`, `Userland/c/tests/*`
- Depends on: only `userlib` (freestanding, no libc); calls kernel via syscall wrappers
- Used by: the user, through the booted shell

## Data Flow

**Timer-driven preemptive context switch:**
1. PIT fires IRQ0 → CPU jumps to the irq00 handler in `Kernel/asm/interrupts.asm`
2. Handler `pushState` — saves full register set onto the *current* process's stack
3. Passes the saved `rsp` to C: `scheduler_tick(rsp)` in `Kernel/c/process/scheduler.c`
4. Scheduler decrements quantum; if expired, selects next READY PCB (round-robin, priority-derived quanta)
5. Returns the next process's `rsp` to asm
6. Handler loads that `rsp`, `popState`, `iretq` — resumes the next process where it left off

**Syscall (`int 0x80`):**
1. Userland wrapper (`Userland/asm/userlib.asm`) sets `rax` = syscall number, args in registers, executes `int 0x80`
2. `_irq128Handler` in `Kernel/asm/interrupts.asm` bounds-checks `rax` against literal `37` (`cmp rax, 37`)
3. Dispatches through `syscalls[]` in `syscallDispatcher.c`
4. The `sys_*` function runs (may block the caller via a semaphore/pipe wait queue)
5. On cooperative yield (`force_switch` set), the same path performs a context switch before returning

**State Management:**
- All kernel state is global/static: PCB table (`MAX_PROCESSES = 64`), semaphore table (`MAX_SEMS`), pipe table (`MAX_PIPES = 16`)
- No persistent storage at runtime — state lives in RAM only
- Per-process state stored in its PCB; register state lives on each process's own 4 KB stack

## Key Abstractions

**Process (PCB):**
- Purpose: Represents a schedulable thread of execution
- Examples: `process.c` — fixed-size table, fields: pid, state (READY/RUNNING/BLOCKED/ZOMBIE), priority (1–5), `rsp`, fd[2] (stdin/stdout), parent_pid
- Pattern: Fixed-size static table indexed by slot; PIDs map to slots

**Initial stack frame (`build_initial_stack`):**
- Purpose: Forge a register/iretq frame so a brand-new process can be `popState`+`iretq`'d into as if it had been interrupted
- Examples: `Kernel/c/process/process.c:58` — manually pushes RIP=entry, CS=`0x08`, RFLAGS, etc.
- Pattern: Hand-rolled trap frame construction

**Memory manager interface:**
- Purpose: Single stable allocator API with two interchangeable implementations
- Examples: `mm_init / mm_malloc / mm_malloc_kernel / mm_free / mm_status` (`Kernel/include/memoryManager/memoryManager.h`)
- Pattern: Compile-time strategy selection (`-DMM_FF` / `-DMM_BUDDY`)

**Named semaphore:**
- Purpose: Process synchronization by agreeing on a name a priori
- Examples: `semaphore.c` — `MAX_SEMS` table, per-sem PID wait-queue, no busy-waiting
- Pattern: Counting semaphore guarded by one global `sem_lock` (acquired via `xchg`)

**Pipe:**
- Purpose: Unidirectional blocking byte stream (IPC)
- Examples: `pipe.c` — circular buffer guarded by 3 semaphores (data/space/mutex), classic producer/consumer; fds encoded by base offset (`PIPE_FD_READ_BASE = 100`, `PIPE_FD_WRITE_BASE = 200`)
- Pattern: Bounded-buffer; EOF when `writers == 0`, broken pipe when `readers == 0`

## Entry Points

**Kernel entry:**
- Location: `Kernel/loader.asm` → `kernelMain` → `initializeKernelBinary` in `Kernel/c/kernel/kernel.c`
- Triggers: Pure64 jumps here after loading `packedKernel.bin`
- Responsibilities: Load modules, clear BSS, load IDT, `mm_init`, init process/scheduler/sem/pipe, create idle + shell, then `main()` → `scheduler_start()`

**Hardware interrupts:**
- Location: IDT entries → handlers in `Kernel/asm/interrupts.asm`
- Triggers: timer (IRQ0), keyboard (IRQ1), exceptions, `int 0x80`
- Responsibilities: Save/restore state, bridge to C dispatchers

**Userland entry:**
- Location: `Userland/_loader.c` → shell `main` (the binary loaded at `0x400000`)
- Triggers: kernel creates the shell as the first foreground process
- Responsibilities: Run the shell REPL; spawn process-class commands via `sys_create_process`

## Error Handling

**Strategy:** CPU exceptions are caught by handlers in `Kernel/c/exceptions/exceptions.c` (e.g. invalid opcode, division by zero) which print diagnostics. There is no exception-unwinding (`-fno-exceptions`); errors propagate via return codes.

**Patterns:**
- Syscalls return negative/sentinel values on failure; userland wrappers check them
- `int 0x80` gate silently ignores out-of-range syscall numbers (bounds check against `37`)
- Pipes signal EOF / broken-pipe via reader/writer counts rather than error codes

## Cross-Cutting Concerns

**Synchronization:**
- One global `sem_lock` (test-and-set via `xchg` in `libasm.asm`) guards the semaphore table
- Per-resource semaphores for pipes; no busy-waiting (blocked processes leave the run queue)

**Scheduling:**
- Round-robin with priority-derived quanta; idle process (`hlt`s) registered as fallback via `scheduler_set_idle`

**Console / Video:**
- Shared video driver; `naiveConsole.c` for early/raw output before the full driver

---

*Architecture analysis: 2026-06-03*
*Update when major patterns change*
