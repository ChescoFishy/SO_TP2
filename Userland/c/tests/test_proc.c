#include "tests/test_proc.h"
#include "syscall/syscall.h"
#include "tests/test_util.h"
#include "lib/userlib.h"

enum State { RUNNING, BLOCKED, KILLED };

typedef struct P_rq {
    int32_t    pid;
    enum State state;
} p_rq;

static int64_t test_processes(uint64_t argc, char *argv[]) {
    uint8_t  rq;
    uint8_t  alive = 0;
    uint8_t  action;
    uint64_t max_processes;
    char    *argvAux[] = {0};

    if (argc != 1)
        return -1;

    if ((max_processes = (uint64_t)satoi(argv[0])) == 0)
        return -1;

    p_rq p_rqs[max_processes];

    while (1) {

        /* Crear max_processes procesos */
        for (rq = 0; rq < max_processes; rq++) {
            p_rqs[rq].pid = (int32_t)my_create_process("endless_loop", 0, argvAux);

            if (p_rqs[rq].pid == -1) {
                printf("test_processes: ERROR creando proceso\n");
                return -1;
            } else {
                p_rqs[rq].state = RUNNING;
                alive++;
            }
        }

        /* Matar/bloquear/desbloquear al azar hasta que todos estén muertos */
        while (alive > 0) {

            for (rq = 0; rq < max_processes; rq++) {
                action = (uint8_t)(GetUniform(100) % 2);

                switch (action) {
                    case 0:
                        if (p_rqs[rq].state == RUNNING || p_rqs[rq].state == BLOCKED) {
                            if (my_kill((uint64_t)p_rqs[rq].pid) == -1) {
                                printf("test_processes: ERROR matando proceso\n");
                                return -1;
                            }
                            p_rqs[rq].state = KILLED;
                            my_wait(p_rqs[rq].pid);
                            alive--;
                        }
                        break;

                    case 1:
                        if (p_rqs[rq].state == RUNNING) {
                            if (my_block((uint64_t)p_rqs[rq].pid) == -1) {
                                printf("test_processes: ERROR bloqueando proceso\n");
                                return -1;
                            }
                            p_rqs[rq].state = BLOCKED;
                        }
                        break;
                }
            }

            /* Desbloquear procesos al azar */
            for (rq = 0; rq < max_processes; rq++) {
                if (p_rqs[rq].state == BLOCKED && GetUniform(100) % 2) {
                    if (my_unblock((uint64_t)p_rqs[rq].pid) == -1) {
                        printf("test_processes: ERROR desbloqueando proceso\n");
                        return -1;
                    }
                    p_rqs[rq].state = RUNNING;
                }
            }
        }
    }
}

/* Entry point como proceso: la shell pasa argv via sys_create_process y se
** encarga de su lifetime (free post-waitpid). El child no debe liberarlo. */
void test_proc_main(int argc, char **argv) {
    char num_buf[24];
    int  ok = 0;
    if (argc >= 1 && argv != 0 && argv[0] != 0) {
        uint64_t i = 0;
        while (argv[0][i] && i < sizeof(num_buf) - 1) { num_buf[i] = argv[0][i]; i++; }
        num_buf[i] = '\0';
        ok = 1;
    }

    if (!ok) {
        printf("uso: test_proc <max_processes>\n");
        sys_exit(-1);
    }
    char *args[1] = {num_buf};
    test_processes(1, args);
    sys_exit(0);
}

