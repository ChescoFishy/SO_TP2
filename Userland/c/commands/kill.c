#include <stdint.h>
#include "lib/userlib.h"
#include "shell/shell.h"
#include "commands/commands.h"

// kill <pid>: mata el proceso indicado.
void kill_cmd(void){
    const char *a = cmd_args();
    int64_t pid = (a != 0) ? next_uint(&a) : -1;
    if(pid <= 0){
        shellPrintString("uso: kill <pid>\n");
        return;
    }
    sys_kill((uint64_t)pid);
}
