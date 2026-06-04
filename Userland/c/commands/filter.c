#include <stdint.h>
#include "lib/userlib.h"
#include "commands/commands.h"

static int is_vowel(char c){
    switch(c){
        case 'a': case 'e': case 'i': case 'o': case 'u':
        case 'A': case 'E': case 'I': case 'O': case 'U':
            return 1;
        default:
            return 0;
    }
}

/* filter: lee de stdin y reimprime todo salvo las vocales. Pensado para pipes
** (ej. cat | filter) pero tambien funciona con datos de un pipe nombrado. */
void filter_main(int argc, char **argv){
    (void)argc; (void)argv;
    char buf[128], out[128];
    uint64_t n;
    while((n = read_full(buf, sizeof(buf))) > 0){
        uint64_t j = 0;
        for(uint64_t i = 0; i < n; i++){
            if(!is_vowel(buf[i])) out[j++] = buf[i];
        }
        if(j > 0) sys_write(STDOUT, out, j);
    }
    sys_exit(0);
}
