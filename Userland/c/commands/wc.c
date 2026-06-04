#include <stdint.h>
#include "lib/userlib.h"
#include "commands/commands.h"

// wc: cuenta lineas en stdin hasta EOF y reporta el total.
void wc_main(int argc, char **argv){
    (void)argc; (void)argv;
    char buf[128];
    uint64_t n;
    uint64_t lines = 0;
    while((n = read_full(buf, sizeof(buf))) > 0){
        for(uint64_t i = 0; i < n; i++){
            if(buf[i] == '\n') lines++;
        }
    }
    char out[24];
    uint64_t len = num_to_str(lines, out, 10);
    out[len] = '\n';
    sys_write(STDOUT, out, len + 1);
    sys_exit(0);
}
