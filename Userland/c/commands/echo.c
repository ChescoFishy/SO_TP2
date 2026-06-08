#include <stdint.h>
#include "lib/userlib.h"
#include "commands/commands.h"

// echo: imprime sus argumentos separados por espacios y un salto de linea final.
void echo_main(int argc, char **argv){
    for(int i = 0; i < argc; i++) {
        print("%s", argv[i]);
        if(i < argc - 1) {
            putchar(' ');
        }
    }
    putchar('\n');
    sys_exit(0);
}
