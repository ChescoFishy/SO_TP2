#include <stdint.h>
#include "lib/userlib.h"
#include "shell/shell.h"
#include "commands/commands.h"

// mem: imprime el estado de la memoria via sys_mem_status.
void mem_cmd(void){
    MemStatus st;
    sys_mem_status(&st);
    char b[24];
    shellPrintString("Estado de memoria (bytes):\n");
    shellPrintString("  Total: "); num_to_str(st.total, b, 10); shellPrintString(b); shellNewline();
    shellPrintString("  Usada: "); num_to_str(st.used,  b, 10); shellPrintString(b); shellNewline();
    shellPrintString("  Libre: "); num_to_str(st.free,  b, 10); shellPrintString(b); shellNewline();
    shellPrintString("  Bloques asignados: ");
    num_to_str(st.alloc_count, b, 10); shellPrintString(b); shellNewline();
}
