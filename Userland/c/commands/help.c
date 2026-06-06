// Builtin help: lista los comandos disponibles desde la tabla commands[].
#include <stdint.h>
#include "lib/userlib.h"
#include "lib/strings.h"
#include "shell/shell.h"

// La tabla commands[] vive en shell/parser.c; la exponemos via find/iteracion.
extern Command commands[];

// Lista de comandos disponibles, generada desde la tabla commands[].
void help(){
    shellPrintString("Comandos disponibles:\n");
    for(int i = 0; commands[i].name != 0; i++){
        shellPrintString("  ");
        shellPrintString(commands[i].name);
        /* padding hasta columna 12 */
        int pad = 12 - (int)strlen(commands[i].name);
        for(int j = 0; j < pad; j++) shellPrintString(" ");
        shellPrintString(commands[i].description ? (char *)commands[i].description : "");
        shellPrintString("\n");
    }
    shellPrintString("Operadores:\n");
    shellPrintString("  cmd &        ejecuta en background\n");
    shellPrintString("  cmd1 | cmd2  pipe (ambos comandos deben ser procesos)\n");
    shellPrintString("  Ctrl+C       mata el proceso foreground actual\n");
    shellPrintString("  Ctrl+D       envia EOF al stdin\n");
}
