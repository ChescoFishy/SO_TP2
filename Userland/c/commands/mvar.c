#include <stdint.h>
#include "lib/userlib.h"
#include "tests/test_util.h"
#include "commands/commands.h"

/* ── mvar: problema lectores/escritores con una MVar sincronizada ──────────────
** Una MVar es una celda que esta llena o vacia. put() espera a que este vacia,
** take() espera a que este llena. Se sincroniza con dos semaforos nombrados
** (procesos no relacionados podrian compartirlos). Como todos los procesos de
** usuario comparten el espacio de direcciones, la celda es una global. */

#define MVAR_EMPTY     "mvar_empty"   /* tokens de slot vacio (init 1) */
#define MVAR_FULL      "mvar_full"    /* tokens de slot lleno  (init 0) */
#define MVAR_ITEMS     5              /* items que produce cada escritor */

static int64_t mvar_cell;             /* la celda compartida */

/* Arma un argv en heap (leak intencional: lo consume el proceso hijo, que vive
** mas que mvar_main). Cada string entera ocupa 24 bytes de holgura. */
static char **mvar_make_argv(int n, int a, int b){
    uint64_t header = sizeof(char *) * (uint64_t)(n + 1);
    char **v = (char **)sys_malloc(header + 24 * (uint64_t)n);
    if(!v) return 0;
    char *s0 = (char *)v + header;
    num_to_str((uint64_t)a, s0, 10);
    v[0] = s0;
    if(n >= 2){
        char *s1 = s0 + 24;
        num_to_str((uint64_t)b, s1, 10);
        v[1] = s1;
    }
    v[n] = 0;
    return v;
}

static void mvar_writer(int argc, char **argv){
    int items = (argc >= 1 && argv && argv[0]) ? (int)satoi(argv[0]) : 0;
    int id    = (argc >= 2 && argv[1])         ? (int)satoi(argv[1]) : 0;
    for(int k = 0; k < items; k++){
        sys_sem_wait(MVAR_EMPTY);     /* esperar slot vacio */
        mvar_cell = id * 1000 + k;    /* poner valor */
        sys_sem_post(MVAR_FULL);      /* avisar slot lleno */
    }
    sys_exit(0);
}

static void mvar_reader(int argc, char **argv){
    int count = (argc >= 1 && argv && argv[0]) ? (int)satoi(argv[0]) : 0;
    uint64_t pid = sys_getpid();
    for(int k = 0; k < count; k++){
        sys_sem_wait(MVAR_FULL);      /* esperar slot lleno */
        int64_t v = mvar_cell;        /* tomar valor */
        sys_sem_post(MVAR_EMPTY);     /* avisar slot vacio */
        printf("[reader %d] leyo %d\n", (int)pid, (int)v);
    }
    sys_exit(0);
}

void mvar_main(int argc, char **argv){
    int W = (argc >= 1 && argv && argv[0]) ? (int)satoi(argv[0]) : 0;
    int R = (argc >= 2 && argv[1])         ? (int)satoi(argv[1]) : 0;
    if(W <= 0 || R <= 0){
        printf("uso: mvar <escritores> <lectores>\n");
        sys_exit(-1);
    }

    /* total de items: se reparten entre los lectores para que ninguno quede
    ** bloqueado esperando algo que nunca llega (sum lecturas == sum escrituras). */
    int total = W * MVAR_ITEMS;
    int base  = total / R;
    int extra = total % R;

    /* (re)inicializar los semaforos del MVar en valores conocidos. */
    sys_sem_close(MVAR_EMPTY);
    sys_sem_close(MVAR_FULL);
    sys_sem_open(MVAR_EMPTY, 1);
    sys_sem_open(MVAR_FULL, 0);

    for(int i = 0; i < W; i++){
        char **av = mvar_make_argv(2, MVAR_ITEMS, i);
        sys_create_process("mvar_wr", (void *)mvar_writer, 2, av, 0);
    }
    for(int i = 0; i < R; i++){
        int cnt = base + (i < extra ? 1 : 0);
        char **av = mvar_make_argv(1, cnt, 0);
        sys_create_process("mvar_rd", (void *)mvar_reader, 1, av, 0);
    }

    /* terminar inmediatamente, sin esperar a los hijos (como pide el enunciado). */
    sys_exit(0);
}
