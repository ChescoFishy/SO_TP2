#include <stdint.h>
#include "lib/userlib.h"
#include "shell/shell.h"
#include "commands/commands.h"

// nice <pid> <prioridad>: cambia la prioridad de un proceso.
void nice_cmd(void){
    const char *a = cmd_args();
    int64_t pid  = (a != 0) ? next_uint(&a) : -1;
    int64_t prio = (a != 0) ? next_uint(&a) : -1;
    if(pid <= 0 || prio < 0){
        shellPrintString("uso: nice <pid> <prioridad>\n");
        return;
    }
    sys_nice((uint64_t)pid, (uint64_t)prio);
}
