#include "include/testMM.h"
#include "include/syscall.h"
#include "include/test_util.h"
#include "include/userlib.h"

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
