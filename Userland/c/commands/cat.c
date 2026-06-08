#include <stdint.h>
#include "lib/userlib.h"
#include "commands/commands.h"

#define EOF 0

// cat: copia stdin a stdout tal cual hasta EOF. Pensado para pipes (ej. cat | filter).
void cat_main(int argc, char **argv){
    (void)argc; (void)argv;
    int c;
    char buf[128];
    uint16_t count = 0;
    while( (c = getchar()) != EOF) {
        print("%c", c);
        buf[count++] = c;
        if (c == '\n' || count - 1 >= 128) {
            buf[count] = 0;
            print("%s", buf);
            count = 0;
        }
    } 
    sys_exit(0); 
}