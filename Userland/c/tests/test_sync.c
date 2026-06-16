#include "tests/test_sync.h"
#include "syscall/syscall.h"
#include "tests/test_util.h"
#include "lib/userlib.h"

#define SEM_ID              "sem"

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
    if (argc != 3)
        return -1;

    int64_t pair_arg = satoi(argv[0]);
    int64_t iter_arg = satoi(argv[1]);
    int64_t sem_arg  = satoi(argv[2]);

    if (pair_arg <= 0 || iter_arg <= 0 || sem_arg < 0 || sem_arg > 1)
        return -1;

    uint64_t pair_count = (uint64_t)pair_arg;
    uint64_t iterations = (uint64_t)iter_arg;
    int8_t   use_sem    = (int8_t)sem_arg;
    uint64_t total_processes = 2 * pair_count;
    if (total_processes > MAX_PROCESSES - 2)
        return -1;

    uint64_t *pids = (uint64_t *)sys_malloc(sizeof(uint64_t) * total_processes);
    if (pids == 0)
        return -1;

    if (use_sem) {
        while (my_sem_close(SEM_ID) == 0)
            ;
        if (!my_sem_open(SEM_ID, 1)) {
            sys_free(pids);
            return -1;
        }
    }

    char *argvDec[] = {argv[1], "-1", argv[2], 0};
    char *argvInc[] = {argv[1], "1",  argv[2], 0};

    global = 0;

    uint64_t i;
    for (i = 0; i < pair_count; i++) {
        int64_t pid_dec = my_create_process("my_process_inc", 3, argvDec);
        int64_t pid_inc = my_create_process("my_process_inc", 3, argvInc);
        if (pid_dec < 0 || pid_inc < 0) {
            printf("test_sync: ERROR creando procesos\n");
            if (pid_dec > 0)
                my_kill((uint64_t)pid_dec);
            if (pid_inc > 0)
                my_kill((uint64_t)pid_inc);
            for (uint64_t j = 0; j < 2 * i; j++)
                my_kill(pids[j]);
            if (use_sem)
                my_sem_close(SEM_ID);
            sys_free(pids);
            return -1;
        }
        pids[2 * i]     = (uint64_t)pid_dec;
        pids[2 * i + 1] = (uint64_t)pid_inc;
    }

    (void)iterations;
    for (i = 0; i < total_processes; i++)
        my_wait((int64_t)pids[i]);

    printf("Valor final: %d\n", (int)global);

    if (use_sem)
        my_sem_close(SEM_ID);
    sys_free(pids);
    return 0;
}

/* Entry point como proceso: test_sync <pairs> <iterations> <use_sem> */
void test_sync_main(int argc, char **argv) {
    if (argc < 3 || argv == 0 || argv[0] == 0 || argv[1] == 0 || argv[2] == 0) {
        printf("uso: test_sync <pares> <iteraciones> <use_sem>\n");
        sys_exit(-1);
    }
    char *args[3] = {argv[0], argv[1], argv[2]};
    if (test_sync_internal(3, args) < 0) {
        printf("uso: test_sync <pares> <iteraciones> <use_sem>\n");
        sys_exit(-1);
    }
    sys_exit(0);
}
