#include <stdint.h>
#include "lib/userlib.h"
#include "commands/commands.h"

// cat: copia stdin a stdout hasta EOF. Pensado para pipes (ej. cat | filter).
void cat_main(int argc, char **argv){
    for(int i = 0; i < argc; i++) {
        sys_write(STDOUT, argv[i], sizeof(argv)+1);
    }
    sys_write(STDOUT, "\n", 1);
    sys_exit(0);
}