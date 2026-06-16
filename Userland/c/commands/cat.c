#include <stdint.h>
#include "lib/userlib.h"
#include "commands/commands.h"

/* cat: copia stdin a stdout tal como lo recibe hasta EOF (Ctrl+D o cierre del pipe). */
void cat_main(int argc, char **argv) {
    (void)argc; (void)argv;
    char buf[128];
    uint64_t read_bytes;

    while ((read_bytes = read_full(buf, sizeof(buf))) > 0)
        sys_write(STDOUT, buf, read_bytes);

    sys_exit(0);
}
