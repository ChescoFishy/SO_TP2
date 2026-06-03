# Codebase Concerns

**Analysis Date:** 2026-06-03

> Note: This is a bare-metal teaching kernel. Many "concerns" below are inherent design choices for a TP (course assignment), not defects — they are documented so future work understands the risk surface, not as a backlog of bugs to fix.

## Security Considerations

**No privilege separation (ring 0 for everything):**
- Risk: All code — kernel and every userland process — runs in ring 0 with `CS = 0x08` (set in `build_initial_stack`, `Kernel/c/process/process.c:64`). Userland can execute privileged instructions and touch any memory. A buggy or malicious userland process can corrupt the kernel.
- Current mitigation: None — intentional for the assignment scope.
- Recommendations: True isolation would require a user-mode GDT segment + per-process paging — out of scope for the TP, but the assumption must be remembered before trusting userland input.

**No memory isolation / single address space:**
- Risk: There is one flat physical address space, no paging. All userland processes share it and share the shell's binary. One process can scribble over another's stack or the kernel heap.
- Files: heap at `0x600000` (`Kernel/c/kernel/kernel.c:13`), modules at fixed `0x400000`/`0x500000`.
- Current mitigation: 4 KB per-process stacks (`STACK_SIZE`, `process.h:8`) `mm_malloc`'d from the heap — but no guard pages, so stack overflow silently corrupts adjacent allocations.
- Recommendations: Guard pages would need paging. At minimum, be conservative with stack-heavy recursion in userland.

## Fragile Areas

**Syscall count must stay in sync in two places:**
- File: `Kernel/include/lib/defs.h` (`CANT_SYS 37`) and `Kernel/asm/interrupts.asm:296` (`cmp rax, 37`)
- Why fragile: The `int 0x80` gate bounds-checks `rax` against a **hardcoded literal `37`**, not the `CANT_SYS` macro. Adding a syscall requires editing both. Forgetting the asm literal means the new syscall is rejected at the gate even though it's in the table.
- Safe modification: Follow the full checklist in CONVENTIONS.md; grep for `37` in `interrupts.asm` whenever `CANT_SYS` changes.
- Test coverage: Not covered by any automated test — a mismatch only surfaces when the new syscall is invoked.

**Manual trap-frame construction (`build_initial_stack`):**
- File: `Kernel/c/process/process.c:58`
- Why fragile: New processes are bootstrapped by hand-forging a register/`iretq` frame (RIP, CS=`0x08`, RFLAGS, ...). The exact layout must match what `popState`+`iretq` in `interrupts.asm` expects. A mismatch between the asm push/pop order and this layout causes immediate faults or silent corruption on first schedule.
- Safe modification: Any change to the register save/restore order in `interrupts.asm` must be mirrored here, and vice versa.
- Test coverage: Indirectly exercised by `test_proc`, but no direct assertion on frame layout.

**Fixed-size global tables:**
- Files: `MAX_PROCESSES = 64` (`process.h:7`), `MAX_PIPES = 16` (`pipe.h:8`), `MAX_SEMS` (`semaphore.h`)
- Why fragile: Hard ceilings. Exhausting the PCB table (e.g. `test_proc` with too many procs, or leaked zombies not reaped) fails process creation. No dynamic growth.
- Safe modification: Bump the constant and rebuild; watch heap pressure (PCB stacks come from the 8 MB heap).

## Concurrency / Locking

**Single global semaphore-table lock:**
- File: `Kernel/asm/libasm.asm:20` (`xchg`-based test-and-set), guarding the semaphore table
- Risk: One coarse `sem_lock` serializes all semaphore-table operations. It's a spinlock acquired via atomic `xchg` — correct, but coarse-grained, and held across table scans. On a single-CPU PIT-preempted kernel this is acceptable, but it's a contention point and any path that blocks while holding it would deadlock.
- Recommendations: Keep critical sections short; never block while holding `sem_lock`. The pipe implementation correctly uses its own per-pipe data/space/mutex semaphores rather than the global lock.

**Race windows in scheduler/IPC:**
- Files: `Kernel/c/process/scheduler.c`, `Kernel/c/ipc/pipe.c`, `Kernel/c/ipc/semaphore.c`
- Risk: Context switches are driven from the timer IRQ. Any kernel data structure mutated outside a lock and also touched from `scheduler_tick` (or a syscall that yields via `force_switch`) has a potential race. Pipe reader/writer counts and wait-queue manipulation are the highest-risk spots (EOF when `writers == 0`, broken pipe when `readers == 0`).
- Recommendations: Audit that wait-queue enqueue/dequeue and count updates happen with interrupts disabled or under the appropriate semaphore; add stress coverage beyond `test_sync`.

## Tech Debt

**Compile-time MM switch (no runtime comparison):**
- Issue: First-Fit vs Buddy is selected with `-DMM_FF` / `-DMM_BUDDY` at build time; a single image embeds exactly one allocator.
- Files: `Kernel/c/memoryManager/memoryManagerFF.c`, `memoryManagerBuddy.c`
- Why: Simplicity — avoids an indirection layer / vtable in the hot allocation path.
- Impact: Comparing allocators requires a full rebuild + reboot (`MM=BUDDY ./compile.sh`), which slows iteration and makes side-by-side benchmarking manual.
- Fix approach: Acceptable for the TP. A runtime-selectable allocator would add a function-pointer table behind `memoryManager.h`.

**Build only works inside the Docker container:**
- Issue: Host builds fail; everything must run in `TP_SO_2` (`agodio/itba-so-multiarch:3.1`).
- Impact: Onboarding friction; image ownership can flip to root (`run.sh` may need `sudo chown $USER Image/...qcow2`).
- Fix approach: Documented in CLAUDE.md; the local `Dockerfile` is an alternative but not the default path.

## Test Coverage Gaps

**Syscall-gate bound mismatch:**
- What's not tested: That `CANT_SYS` and the asm literal `37` agree.
- Risk: Silent rejection of a newly added syscall.
- Priority: Medium — only bites when extending syscalls.

**Concurrency correctness beyond the happy path:**
- What's not tested: Pipe EOF/broken-pipe edge cases under heavy interleaving, semaphore wait-queue fairness, zombie reaping under `MAX_PROCESSES` pressure.
- Risk: Intermittent hangs or corruption that the deterministic `test_*` runs don't surface.
- Priority: Medium.
- Difficulty: High — no host test harness; would require scripted in-shell stress sequences.

---

*Concerns audit: 2026-06-03*
*Update as issues are fixed or new ones discovered*
