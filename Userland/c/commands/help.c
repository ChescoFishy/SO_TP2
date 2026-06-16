// Builtin help: lista los comandos disponibles desde la tabla commands[].
#include <stdint.h>
#include "lib/userlib.h"
#include "lib/strings.h"
#include "shell/shell.h"

// La tabla commands[] vive en shell/parser.c; la exponemos via find/iteracion.
extern Command commands[];

// Tests provistos por la catedra. El resto de los test_* (p. ej. test_pipe) son
// propios del grupo y se listan junto al resto de los comandos.
static int is_test_catedra(const char *name){
    return strcmp(name, "test_mm")   == 0 || strcmp(name, "test_proc") == 0
        || strcmp(name, "test_prio") == 0 || strcmp(name, "test_sync") == 0;
}

// Imprime una fila "  <nombre><padding hasta columna 12><descripcion>".
static void print_command(const Command *cmd){
    shellPrintString("  ");
    shellPrintString(cmd->name);
    int pad = 12 - (int)strlen(cmd->name);
    for(int j = 0; j < pad; j++) shellPrintString(" ");
    shellPrintString(cmd->description ? (char *)cmd->description : "");
    shellPrintString("\n");
}

// Lista de comandos disponibles, generada desde la tabla commands[]. Los tests
// de la catedra se agrupan en un apartado propio al final.
void help(){
    shellPrintString("Comandos disponibles:\n");
    for(int i = 0; commands[i].name != 0; i++){
        if(is_test_catedra(commands[i].name)) continue;   // se listan aparte
        print_command(&commands[i]);
    }
    shellPrintString("Operadores:\n");
    shellPrintString("  cmd &        ejecuta en background\n");
    shellPrintString("  cmd1 | cmd2  pipe (ambos comandos deben ser procesos)\n");
    shellPrintString("  Ctrl+C       mata el proceso foreground actual\n");
    shellPrintString("  Ctrl+D       envia EOF al stdin\n");
    shellPrintString("Tests de la catedra:\n");
    for(int i = 0; commands[i].name != 0; i++){
        if(!es_test_catedra(commands[i].name)) continue;
        print_command(&commands[i]);
    }
}
