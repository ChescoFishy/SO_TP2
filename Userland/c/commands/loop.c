#include <stdint.h>
#include "lib/userlib.h"
#include "tests/test_util.h"
#include "commands/commands.h"

/* loop: imprime su PID con un saludo cada cierto tiempo usando espera ACTIVA
** (no se bloquea), como pide el enunciado. argv[0] opcional = periodo en ticks. */
void loop_main(int argc, char **argv){
    uint64_t pid = sys_getpid();
    uint64_t period = (argc >= 1 && argv != 0 && argv[0] != 0)
                    ? (uint64_t)satoi(argv[0]) : 18;
    if(period == 0) period = 18;

    while(1){
        printf("Hola! soy el proceso %d\n", (int)pid);
        uint64_t start = sys_ticks();
        while(sys_ticks() - start < period)
            ;  /* espera activa: no cede CPU voluntariamente */
    }
}
