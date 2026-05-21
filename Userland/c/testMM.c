#include "include/testMM.h"
#include "include/syscall.h"
#include "include/test_util.h"
#include "include/userlib.h"
#include "include/shell.h"

#define MAX_BLOCKS 128

typedef struct MM_rq {
    void    *address;
    uint32_t size;
} mm_rq;

static void test_mm_loop(uint64_t max_memory) {
    mm_rq    mm_rqs[MAX_BLOCKS];
    uint8_t  rq;
    uint32_t total;
    uint32_t i;

    while (1) {
        rq    = 0;
        total = 0;

        /* Pedir tantos bloques como sea posible */
        while (rq < MAX_BLOCKS && total < max_memory) {
            mm_rqs[rq].size    = GetUniform((uint32_t)(max_memory - total - 1)) + 1;
            mm_rqs[rq].address = malloc(mm_rqs[rq].size);

            if (mm_rqs[rq].address) {
                total += mm_rqs[rq].size;
                rq++;
            } else {
                break;
            }
        }

        /* Inicializar cada bloque con su índice */
        for (i = 0; i < rq; i++)
            if (mm_rqs[i].address)
                memset(mm_rqs[i].address, (int32_t)i, mm_rqs[i].size);

        /* Verificar integridad */
        for (i = 0; i < rq; i++) {
            if (mm_rqs[i].address) {
                if (!memcheck(mm_rqs[i].address, (uint8_t)i, mm_rqs[i].size)) {
                    printf("test_mm ERROR\n");
                    sys_exit(-1);
                }
            }
        }

        /* Liberar */
        for (i = 0; i < rq; i++)
            if (mm_rqs[i].address)
                free(mm_rqs[i].address);
    }
}

/* Entry point como proceso: la shell pasa argv via sys_create_process y se
** encarga de su lifetime (free post-waitpid). El child no debe liberarlo. */
void test_mm_main(int argc, char **argv) {
    int64_t max_memory = -1;
    if (argc >= 1 && argv != 0 && argv[0] != 0)
        max_memory = satoi(argv[0]);

    if (max_memory <= 0) {
        printf("uso: test_mm <max_memory>\n");
        sys_exit(-1);
    }
    test_mm_loop((uint64_t)max_memory);
    sys_exit(0);
}

/* Crea test_mm como proceso (foreground + waitpid). El buffer argv vive en el
** heap y lo libera el child, asi que es seguro tambien para background. */
void testMM(void) {
    const char *args = cmd_args();
    if (!args) {
        shellPrintString("uso: test_mm <max_memory>\n");
        return;
    }

    uint64_t len = 0;
    while (args[len]) len++;
    char **argv = (char **)sys_malloc(sizeof(char *) + len + 1);
    if (!argv) {
        shellPrintString("test_mm: sin memoria\n");
        return;
    }
    char *s = (char *)(argv + 1);
    for (uint64_t i = 0; i <= len; i++) s[i] = args[i];
    argv[0] = s;

    int64_t pid = sys_create_process("test_mm", test_mm_main, 1, argv, 1);
    if (pid <= 0) {
        shellPrintString("test_mm: error creando proceso\n");
        sys_free(argv);
        return;
    }
    sys_waitpid((uint64_t)pid);
}
