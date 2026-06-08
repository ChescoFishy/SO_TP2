#include <stdint.h>
#include "lib/userlib.h"
#include "commands/commands.h"

// aStdin: emite la letra 'a' por stdout. Pensado para pipes
// (ej. aStdin | cat), donde alimenta el stdin del proceso siguiente. Si el
// lector cierra el pipe, sys_write devuelve <=0 (broken pipe) y termina.
void astdin_main(int argc, char **argv){
    (void)argc; (void)argv;
    char c = 'a';
    sys_write(STDOUT, &c, 1);
    putchar('\n');
    sys_exit(0);
}
