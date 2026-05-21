#include "include/test_prio.h"
#include "include/syscall.h"
#include "include/test_util.h"
#include "include/userlib.h"

#define TOTAL_PROCESSES 3

#define LOWEST  1
#define MEDIUM  3
#define HIGHEST 5

static int64_t prio[TOTAL_PROCESSES] = {LOWEST, MEDIUM, HIGHEST};

/* Proceso worker: cuenta de 0 a max_value (argv[0]) e imprime su PID al terminar.
   No-static para que el registro de syscall.c pueda referenciarla. */
void zero_to_max(int argc, char *argv[]) {
    if (argc < 1 || argv == 0 || argv[0] == 0)
        return;
    uint64_t max_value = (uint64_t)satoi(argv[0]);
    uint64_t value = 0;
    while (value++ != max_value)
        ;
    printf("PROCESS %d DONE!\n", (int)my_getpid());
}

static int64_t test_prio_internal(uint64_t argc, char *argv[]) {
    int64_t pids[TOTAL_PROCESSES];
    uint64_t i;

    if (argc != 1)
        return -1;

    if ((uint64_t)satoi(argv[0]) == 0)
        return -1;

    /* argv[0] vive en el stack de este proceso hasta que retorna test_prio_internal,
    ** y todos los workers son esperados antes del return → pasaje seguro. */
    char *ztm_argv[1] = {argv[0]};

    /* ── Fase 1: misma prioridad ─────────────────────────────────────────── */
    printf("SAME PRIORITY...\n");

    for (i = 0; i < TOTAL_PROCESSES; i++)
        pids[i] = my_create_process("zero_to_max", 1, ztm_argv);

    for (i = 0; i < TOTAL_PROCESSES; i++)
        my_wait(pids[i]);

    /* ── Fase 2: prioridades distintas (asignadas después de crear) ──────── */
    printf("SAME PRIORITY, THEN CHANGE IT...\n");

    for (i = 0; i < TOTAL_PROCESSES; i++) {
        pids[i] = my_create_process("zero_to_max", 1, ztm_argv);
        my_nice((uint64_t)pids[i], (uint64_t)prio[i]);
        printf("  PROCESS %d NEW PRIORITY: %d\n", (int)pids[i], (int)prio[i]);
    }

    for (i = 0; i < TOTAL_PROCESSES; i++)
        my_wait(pids[i]);

    /* ── Fase 3: prioridades distintas, asignadas mientras están bloqueados  */
    printf("SAME PRIORITY, THEN CHANGE IT WHILE BLOCKED...\n");

    for (i = 0; i < TOTAL_PROCESSES; i++) {
        pids[i] = my_create_process("zero_to_max", 1, ztm_argv);
        my_block((uint64_t)pids[i]);
        my_nice((uint64_t)pids[i], (uint64_t)prio[i]);
        printf("  PROCESS %d NEW PRIORITY: %d\n", (int)pids[i], (int)prio[i]);
    }

    for (i = 0; i < TOTAL_PROCESSES; i++)
        my_unblock((uint64_t)pids[i]);

    for (i = 0; i < TOTAL_PROCESSES; i++)
        my_wait(pids[i]);

    return 0;
}

/* Entry point como proceso: test_prio <max_value> */
void test_prio_main(int argc, char **argv) {
    if (argc < 1 || argv == 0 || argv[0] == 0) {
        printf("uso: test_prio <max_value>\n");
        sys_exit(-1);
    }
    char *args[1] = {argv[0]};
    test_prio_internal(1, args);
    sys_exit(0);
}

