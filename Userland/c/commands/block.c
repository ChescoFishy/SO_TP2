#include <stdint.h>
#include "lib/userlib.h"
#include "shell/shell.h"
#include "commands/commands.h"

// block <pid>: alterna el estado del proceso entre BLOCKED y READY.
#define STATE_BLOCKED 3   /* coincide con PROCESS_BLOCKED del kernel */
void block_cmd(void){
    const char *a = cmd_args();
    int64_t pid = (a != 0) ? next_uint(&a) : -1;
    if(pid <= 0){
        shellPrintString("uso: block <pid>\n");
        return;
    }
    static ProcessInfo buf[MAX_PROCESSES];
    uint64_t cnt = sys_ps(buf, MAX_PROCESSES);
    for(uint64_t i = 0; i < cnt; i++){
        if(buf[i].pid == (uint64_t)pid){
            if(buf[i].state == STATE_BLOCKED)
                sys_unblock((uint64_t)pid);
            else
                sys_block((uint64_t)pid);
            return;
        }
    }
    shellPrintString("block: PID no encontrado\n");
}
