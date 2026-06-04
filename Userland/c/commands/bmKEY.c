#include <stdint.h>
#include "lib/userlib.h"
#include "shell/shell.h"

// Mide tiempo hasta presionar una tecla
void bmKEY(){
    shellPrintString("Presione cualquier tecla: \n");
    uint64_t ticks = sys_ticks();
    getchar();

    uint64_t finalTicks = sys_ticks();
    uint64_t delta = finalTicks - ticks;
    shellPrintString("Tiempo: ");
    char buff[BM_BUFF];

    num_to_str(delta, buff, 10);
    shellPrintString(buff);
    shellPrintString(" ticks\n");
}
