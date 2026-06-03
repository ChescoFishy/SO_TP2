#include "tests/test_sync.h"
#include "syscall/syscall.h"
#include "tests/test_util.h"
#include "lib/userlib.h"

#define SEM_ID              "sem"
#define TOTAL_PAIR_PROCESSES 2

static int64_t global; /* memoria compartida (mismo espacio de direcciones) */

static void slowInc(int64_t *p, int64_t inc) {
    int64_t aux = *p;
    if (GetUniform(100) < 30)
        my_yield(); /* maximiza la probabilidad de condición de carrera */
    aux += inc;
    *p = aux;
}

/* Entry point del proceso worker.
   argv[0] = n (iteraciones), argv[1] = inc (+1 o -1), argv[2] = use_sem */
void my_process_inc(int argc, char *argv[]) {
    uint64_t n;
    int8_t   inc;
    int8_t   use_sem;

    if (argc != 3)
        return;

    if ((n = (uint64_t)satoi(argv[0])) == 0)
        return;
    if ((inc = (int8_t)satoi(argv[1])) == 0)
        return;
    if ((use_sem = (int8_t)satoi(argv[2])) < 0)
        return;

    if (use_sem) {
        if (!my_sem_open(SEM_ID, 1)) {
            printf("test_sync: ERROR abriendo semaforo\n");
            return;
        }
    }

    uint64_t i;
    for (i = 0; i < n; i++) {
        if (use_sem)
            my_sem_wait(SEM_ID);
        slowInc(&global, inc);
        if (use_sem)
            my_sem_post(SEM_ID);
    }

    if (use_sem)
        my_sem_close(SEM_ID);
}

static int64_t test_sync_internal(uint64_t argc, char *argv[]) {
    uint64_t pids[2 * TOTAL_PAIR_PROCESSES];

    if (argc != 2)
        return -1;

    char *argvDec[] = {argv[0], "-1", argv[1], 0};
    char *argvInc[] = {argv[0], "1",  argv[1], 0};

    global = 0;

    uint64_t i;
    for (i = 0; i < TOTAL_PAIR_PROCESSES; i++) {
        pids[i]                        = (uint64_t)my_create_process("my_process_inc", 3, argvDec);
        pids[i + TOTAL_PAIR_PROCESSES] = (uint64_t)my_create_process("my_process_inc", 3, argvInc);
    }

    for (i = 0; i < TOTAL_PAIR_PROCESSES; i++) {
        my_wait((int64_t)pids[i]);
        my_wait((int64_t)pids[i + TOTAL_PAIR_PROCESSES]);
    }

    printf("Valor final: %d\n", (int)global);

    return 0;
}

/* Entry point como proceso: test_sync <n> <use_sem> */
void test_sync_main(int argc, char **argv) {
    if (argc < 2 || argv == 0 || argv[0] == 0 || argv[1] == 0) {
        printf("uso: test_sync <n> <use_sem>\n");
        sys_exit(-1);
    }
    char *args[2] = {argv[0], argv[1]};
    test_sync_internal(2, args);
    sys_exit(0);
}

