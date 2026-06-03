# Testing Patterns

**Analysis Date:** 2026-06-03

## Test Framework

**Runner:**
- **None on the host.** There is no unit-test framework, no CI test runner, no `npm test` equivalent.
- Tests are **userland processes** compiled into the userland binary and launched from inside the booted shell.

**Assertion model:**
- Tests self-report PASS/FAIL by printing to the console and/or checking invariants (e.g. the MM stress test verifies no allocation overlap; the sync test expects a shared counter to return to 0).
- "Passing" is observed by the operator reading shell output — there is no exit-code aggregation.

**Run Commands (from the booted shell, not the host):**
```
test_mm <max_memory>      # MM stress: random alloc/free, checks no overlap
test_proc <max_procs>     # create/block/unblock/kill dummy processes
test_sync <n> <use_sem>   # N pairs inc/dec a shared var; expect 0 with semaphores
test_prio <target>        # 3 processes race to a target with different priorities
ps                        # list processes (built-in)
mem                       # memory status (built-in)
bmFPS / bmCPU / bmMEM / bmKEY   # benchmarks (built-ins)
```

## Test File Organization

**Location:**
- `Userland/c/tests/` — `testMM.c`, `test_proc.c`, `test_sync.c`, `test_prio.c`, `test_util.c`
- Headers in `Userland/include/tests/`
- Registered as **process-class** commands in the `commands[]` table (`Userland/c/lib/userlib.c`), so they accept `&` (background) and `|` (pipe).

**Naming:**
- snake_case `test_*.c` (with `testMM.c` as the camelCase exception)
- `test_util.c` holds shared helpers used across tests

## Test Structure

Each test is a normal freestanding process `main`-style entry that:
1. Parses its argv (e.g. `max_memory`, `max_procs`)
2. Drives the kernel via syscalls (alloc/free, create/block/kill, sem_wait/post)
3. Checks invariants and prints results
4. Returns / exits, freeing the PCB slot

There is **no** `describe`/`it`, no setup/teardown framework, no mocking — tests exercise the real kernel running on real (emulated) hardware.

## Mocking

- **No mocking.** Tests run against the live kernel in QEMU. There are no test doubles, fixtures, or fakes — the whole point is to stress the actual allocator/scheduler/IPC.

## Coverage

- **No coverage tooling.** Coverage is implicit: the catedra test suite exercises the memory manager, process lifecycle, synchronization, and priority scheduling.
- No enforced thresholds; nothing blocks on test results.

## Test Types

**MM stress (`test_mm`):**
- Scope: memory manager (FF or Buddy, whichever was compiled in)
- Method: random alloc/free patterns up to `<max_memory>`, asserts allocations never overlap
- To compare allocators: rebuild with each `MM` value and rerun — `./compile.sh` then `MM=BUDDY ./compile.sh`

**Process lifecycle (`test_proc`):**
- Scope: `process.c` + `scheduler.c`
- Method: creates, blocks, unblocks, and kills dummy processes up to `<max_procs>` (≤ `MAX_PROCESSES = 64`)

**Synchronization (`test_sync`):**
- Scope: `semaphore.c`
- Method: N pairs increment/decrement a shared variable; with semaphores enabled (`use_sem = 1`) the result must be 0, without them it races

**Priority scheduling (`test_prio`):**
- Scope: scheduler priority handling
- Method: 3 processes race to a target with different priorities; higher priority should finish first

**Benchmarks (built-ins):**
- `bmFPS`, `bmCPU`, `bmMEM`, `bmKEY` — informal performance probes run synchronously in the shell

## Common Patterns

**Comparing the two allocators:**
```bash
./compile.sh            # First-Fit image
./run.sh                # boot, run: test_mm <max_memory>
MM=BUDDY ./compile.sh   # Buddy image
./run.sh                # boot, run: test_mm <max_memory>
```
The MM choice is a compile-time switch — a single image embeds exactly one allocator, so you must rebuild to switch.

**Running in background / piped:**
Because tests are real processes, they support `test_proc 10 &` and pipelines like `loop | wc`.

---

*Testing analysis: 2026-06-03*
*Update when test patterns change*
