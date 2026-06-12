#include <stdint.h>
#include "lib/userlib.h"
#include "lib/io.h"
#include "tests/test_util.h"
#include "commands/commands.h"

/* mvar: demo de una MVar (celda compartida de un caracter) con N escritores
** y M lectores sincronizados por dos semaforos nombrados (empty/full).
** Cada escritor deposita su letra cuando la celda esta vacia; cada lector
** la consume y la imprime en su color. Los hijos corren en background y se
** frenan con kill (ver sus pids con ps). */

#define INTERVAL_MAX 20
#define MAX_WRITERS  26
#define MAX_READERS  7

#define SEM_EMPTY "mvar_empty"
#define SEM_FULL  "mvar_full"

/* Celda compartida: alcanza una global porque todo userland comparte el
** espacio de direcciones. */
static char mvar_value;

/* Los argv de los hijos deben sobrevivir al retorno de mvar_main (su stack
** se libera al terminar), por eso viven en estaticos y no en el stack. */
static char  writer_letters[MAX_WRITERS][2];
static char *writer_argvs[MAX_WRITERS][1];
static char  reader_index[MAX_READERS][2];
static char *reader_argvs[MAX_READERS][1];

static const uint32_t reader_colors[MAX_READERS] = {
    0xFFFFFF, /* blanco   */
    0xFF5555, /* rojo     */
    0x55FF55, /* verde    */
    0x5555FF, /* azul     */
    0xFFFF55, /* amarillo */
    0xFF55FF, /* magenta  */
    0x55FFFF, /* cyan     */
};

/* Espera un intervalo aleatorio de ticks del timer (~18/seg) cediendo la CPU
** mientras tanto. GetUniform comparte estado entre procesos (mismo address
** space), lo que desfasa a los hijos entre si sin necesidad de seed propia. */
static void random_wait(void) {
    uint64_t target = sys_ticks() + GetUniform(INTERVAL_MAX);
    while (sys_ticks() < target)
        sys_yield();
}

/* Entry point del escritor. argv[0] = su letra ("A".."Z"). */
static void writer_main(int argc, char **argv) {
    if (argc < 1 || argv == 0 || argv[0] == 0)
        sys_exit(-1);
    char letter = argv[0][0];

    while (1) {
        random_wait();

        sys_sem_wait(SEM_EMPTY);
        mvar_value = letter;
        sys_sem_post(SEM_FULL);
    }
}

/* Entry point del lector. argv[0] = indice de color ("0".."6"). */
static void reader_main(int argc, char **argv) {
    if (argc < 1 || argv == 0 || argv[0] == 0)
        sys_exit(-1);
    uint32_t color = reader_colors[argv[0][0] - '0'];

    while (1) {
        random_wait();

        sys_sem_wait(SEM_FULL);
        char c = mvar_value;
        sys_write_color(STDOUT, &c, 1, color);
        sys_sem_post(SEM_EMPTY);
    }
}

/* mvar <n_escritores> <n_lectores> */
void mvar_main(int argc, char **argv) {
    if (argc < 2 || argv == 0 || argv[0] == 0 || argv[1] == 0) {
        printf("uso: mvar <escritores> <lectores>\n");
        sys_exit(-1);
    }

    int64_t writers = satoi(argv[0]);
    int64_t readers = satoi(argv[1]);

    if (writers < 1 || writers > MAX_WRITERS) {
        printf("mvar: escritores debe estar entre 1 y %d\n", MAX_WRITERS);
        sys_exit(-1);
    }
    if (readers < 1 || readers > MAX_READERS) {
        printf("mvar: lectores debe estar entre 1 y %d\n", MAX_READERS);
        sys_exit(-1);
    }

    /* Purgar semaforos de corridas anteriores: sem_open sobre un nombre
    ** existente conserva el valor viejo y romperia el invariante inicial. */
    while (sys_sem_close(SEM_EMPTY) == 0)
        ;
    while (sys_sem_close(SEM_FULL) == 0)
        ;

    mvar_value = 0;
    if (!sys_sem_open(SEM_EMPTY, 1) || !sys_sem_open(SEM_FULL, 0)) {
        printf("mvar: ERROR abriendo semaforos\n");
        sys_exit(-1);
    }

    for (int i = 0; i < writers; i++) {
        writer_letters[i][0] = (char)('A' + i);
        writer_letters[i][1] = '\0';
        writer_argvs[i][0]   = writer_letters[i];
        if (sys_create_process("mvar_writer", (void *)writer_main,
                               1, writer_argvs[i], 0) <= 0) {
            printf("mvar: ERROR creando escritor\n");
            sys_exit(-1);
        }
    }

    for (int i = 0; i < readers; i++) {
        reader_index[i][0] = (char)('0' + i);
        reader_index[i][1] = '\0';
        reader_argvs[i][0] = reader_index[i];
        if (sys_create_process("mvar_reader", (void *)reader_main,
                               1, reader_argvs[i], 0) <= 0) {
            printf("mvar: ERROR creando lector\n");
            sys_exit(-1);
        }
    }

    sys_exit(0);
}
