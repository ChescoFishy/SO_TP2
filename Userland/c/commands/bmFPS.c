#include <stdint.h>
#include "lib/userlib.h"
#include "shell/shell.h"

// Benchmark de FPS aproximado (limpia pantalla en loop)
void bmFPS(){
    /*
     * bmFPS - crude frame-rate benchmark
     * - Repeatedly clears the screen for ~3 seconds and counts iterations.
     * - Assumes sys_ticks() increments roughly 18 times per second (BIOS-like).
     * - Avoid printing inside the loop to not skew the results.
     */
    uint64_t ticks = sys_ticks();
    uint64_t count = 0;
    /* 18 ticks ~= 1 second on many systems that emulate BIOS ticks */
    uint64_t duration = 18 * 7; /* ~3 seconds */

    shellPrintString("Inicio de test.\n");
    /* busy loop that only clears the screen and increments counter */
    while((sys_ticks() - ticks) < duration){
        sys_clear();
        count++;
    }

    /* count iterations over ~7 seconds -> approximate frames per second */
    uint64_t fps = count / 7;
    shellPrintString("FPS: ");

    char fpsBuff[BM_BUFF];
    num_to_str(fps, fpsBuff, 10);
    shellPrintString(fpsBuff);
    shellPrintString("\n");
}
