#include <stdint.h>
#include "lib/userlib.h"
#include "shell/shell.h"

// Benchmark de CPU (operaciones int/float)
void bmCPU(){
    /*
     * bmCPU - simple CPU benchmark
     * - Runs a mix of integer and floating-point operations N times
     * - Measures elapsed ticks using sys_ticks() and prints total time
     *   and a rough operations-per-tick metric.
     *
     * Notes:
     * - N is kept reasonably large to get measurable tick counts.
     * - This is a coarse benchmark (no warming, no cycle-accurate timing).
     */
    const uint32_t N = 1000000u;
    uint64_t ticks = sys_ticks();

    uint64_t result = 0;
    double float_result = 0.0;

    for(uint32_t i = 0; i < N; ++i){
        result += (uint64_t)i * 7ull;
        result = result % 2147483647ull;

        float_result += (double)i * 3.14159;
        if ((i % 100000u) == 0 && i != 0) {
            float_result *= 0.5;
        }
    }

    uint64_t end_ticks = sys_ticks();
    uint64_t delta = end_ticks - ticks;

    shellPrintString("Tiempo: ");

    char timeBuff[32];
    num_to_str(delta, timeBuff, 10);
    shellPrintString(timeBuff);
    shellPrintString(" ticks\n");

    if(delta > 0){
        /* promote N to 64-bit before division to avoid surprises */
        uint64_t ops_per_tick = ((uint64_t)N) / delta;
        shellPrintString("Operaciones por tick: ");
        num_to_str(ops_per_tick, timeBuff, 10);
        shellPrintString(timeBuff);
        shellPrintString("\n");
    } else {
        shellPrintString("Elapsed ticks = 0, no se puede calcular ops/tick.\n");
    }

    return;
}
