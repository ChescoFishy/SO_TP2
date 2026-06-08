#include <stdint.h>
#include "lib/userlib.h"
#include "commands/commands.h"

// cat: copia stdin a stdout tal cual hasta EOF. Pensado para pipes (ej. cat | filter).
void cat_main(int argc, char **argv){
    (void)argc; (void)argv;
    char buf[128];
    uint64_t n;
    while((n = read_full(buf, sizeof(buf))) > 0){
        sys_write(STDOUT, buf, n);
    }
    sys_exit(0);
}
