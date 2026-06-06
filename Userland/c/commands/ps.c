// Builtin ps: lista los procesos activos con PID, prioridad, fg, estado y nombre.
#include <stdint.h>
#include "lib/userlib.h"
#include "shell/shell.h"

static const char *state_names[] = {"FREE", "READY", "RUNNING", "BLOCKED", "ZOMBIE"};

// Muestra la lista de procesos activos con PID, nombre, prioridad y estado
void ps(void) {
    static ProcessInfo buf[MAX_PROCESSES];
    uint64_t count = sys_ps(buf, MAX_PROCESSES);
    char tmp[24];

    shellPrintString("PID  PRI  FG  STATE    NAME\n");
    shellPrintString("---  ---  --  -------  --------\n");

    for (uint64_t i = 0; i < count; i++) {
        // PID
        num_to_str(buf[i].pid, tmp, 10);
        shellPrintString(tmp);
        shellPrintString("    ");

        // Prioridad
        num_to_str(buf[i].priority, tmp, 10);
        shellPrintString(tmp);
        shellPrintString("    ");

        // Foreground
        shellPrintString(buf[i].foreground ? "Y " : "N ");
        shellPrintString("  ");

        // Estado
        uint8_t st = buf[i].state;
        if (st <= 4)
            shellPrintString((char *)state_names[st]);
        else
            shellPrintString("?");
        shellPrintString("  ");

        // Nombre
        shellPrintString(buf[i].name);
        shellPrintString("\n");
    }
}
