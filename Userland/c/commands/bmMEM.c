#include <stdint.h>
#include "lib/userlib.h"
#include "shell/shell.h"

// Benchmark simple de memoria (llenado/copia/checksum)
void bmMEM(){
    /*
     * bmMEM - simple memory benchmark
     * - Fills a 4KB buffer many times, computes a checksum and does copies.
     * - Measures elapsed ticks and reports operations per tick.
     *
     * Notes:
     * - Make sure the operations count is calculated using 64-bit to avoid
     *   intermediate overflow on 32-bit platforms.
     */
    char buffer[4 * KB];
    uint64_t totalChecksum = 0;
    uint64_t ticks = sys_ticks();

    for(int iteration = 0; iteration < 10000; iteration++){
        for(int i = 0; i < 4 * KB; i++){
            buffer[i] = (i + iteration) % 256;
        }

        /* use 64-bit checksum to avoid truncation issues */
        uint64_t checksum = 0;
        for(int i = 0; i < 4 * KB; i++){
            checksum += (unsigned char)buffer[i];
            checksum = checksum % 1000000ULL;
        }

        for(int i = 0; i < 2 * KB; i++){
            buffer[i + 2 * KB] = buffer[i];
        }

        /* make checksum observable to prevent over-optimization */
        totalChecksum += checksum;
    }

    uint64_t finalTicks = sys_ticks();
    uint64_t delta = finalTicks - ticks;

    shellPrintString("Tiempo: ");

    char buff[BM_BUFF];
    num_to_str(delta, buff, 10);
    shellPrintString(buff);
    shellPrintString(" ticks\n");

    if(delta > 0){
        /* compute operations using 64-bit arithmetic to be safe */
        uint64_t operations = (uint64_t)10000 * (uint64_t)(4 * KB) * 3ULL;
        uint64_t operationsPerCycle = operations / delta;
        shellPrintString("Operaciones por tick: ");
        num_to_str(operationsPerCycle, buff, 10);
        shellPrintString(buff);
        shellPrintString("\n");
    }

    /* Print a checksum summary to ensure computations aren't optimized away */
    shellPrintString("Checksum: ");
    num_to_str(totalChecksum, buff, 10);
    shellPrintString(buff);
    shellPrintString("\n");
}
