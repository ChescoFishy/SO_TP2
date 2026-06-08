#include <stdint.h>
#include "lib/userlib.h"
#include "lib/io.h"
#include "commands/commands.h"

#define RED 0xFF0000

/* red: copia stdin a stdout tal cual hasta EOF, pero pintando el texto de rojo
** en consola. Pensado para pipes (ej. cat | red); si la salida es un pipe el
** color se ignora y los bytes viajan crudos. */
void red_main(int argc, char **argv){
    (void)argc; (void)argv;
    char buf[128];
    uint64_t n;
    while((n = read_full(buf, sizeof(buf))) > 0){
        sys_write_color(STDOUT, buf, n, RED);
    }
    sys_exit(0);
}
