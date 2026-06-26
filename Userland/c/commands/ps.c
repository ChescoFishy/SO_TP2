// Proceso ps: lista los procesos activos con PID, prioridad, fg, estado, SP, BP y nombre.
// Escribe a STDOUT (sys_write via print/putchar), por lo que admite & y |.
#include <stdint.h>
#include "lib/userlib.h"
#include "lib/strings.h"
#include "lib/format.h"
#include "lib/io.h"
#include "commands/commands.h"

static const char *state_names[] = {"FREE", "READY", "RUNNING", "BLOCKED", "ZOMBIE"};

// Anchos de cada columna. Compartidos por encabezado y filas para que todo alinee.
#define W_PID    5
#define W_PRI    5
#define W_FG     4
#define W_STATE  9
#define W_SP    14
#define W_BP    14
#define W_NAME  10

// Imprime n espacios.
static void print_spaces(int n) {
    while (n-- > 0)
        putchar(' ');
}

// Imprime s centrado dentro de un campo de ancho width.
static void print_centered(const char *s, int width) {
    int len = (int)strlen(s);
    if (len >= width) {
        print("%s", s);
        return;
    }
    int pad = width - len;
    int left = pad / 2;
    print_spaces(left);
    print("%s", s);
    print_spaces(pad - left);
}

// Construye "0x<hex>" en dest a partir de value.
static void hex_field(uint64_t value, char *dest) {
    dest[0] = '0';
    dest[1] = 'x';
    num_to_str(value, dest + 2, 16);
}

// Muestra la lista de procesos activos con PID, nombre, prioridad, estado, SP y BP.
void ps_main(int argc, char **argv) {
    (void)argc; (void)argv;
    static ProcessInfo buf[MAX_PROCESSES];
    uint64_t count = sys_ps(buf, MAX_PROCESSES);
    char tmp[24];

    print_centered("PID", W_PID);
    print_centered("PRI", W_PRI);
    print_centered("FG", W_FG);
    print_centered("STATE", W_STATE);
    print_centered("SP", W_SP);
    print_centered("BP", W_BP);
    print_centered("NAME", W_NAME);
    putchar('\n');

    print_centered("---", W_PID);
    print_centered("---", W_PRI);
    print_centered("--", W_FG);
    print_centered("-------", W_STATE);
    print_centered("------------", W_SP);
    print_centered("------------", W_BP);
    print_centered("--------", W_NAME);
    putchar('\n');

    for (uint64_t i = 0; i < count; i++) {
        // PID
        num_to_str(buf[i].pid, tmp, 10);
        print_centered(tmp, W_PID);

        // Prioridad
        num_to_str(buf[i].priority, tmp, 10);
        print_centered(tmp, W_PRI);

        // Foreground
        print_centered(buf[i].foreground ? "Y" : "N", W_FG);

        // Estado
        uint8_t st = buf[i].state;
        print_centered(st <= 4 ? state_names[st] : "?", W_STATE);

        // Stack Pointer (SP) guardado en el momento de la suspension
        hex_field(buf[i].rsp, tmp);
        print_centered(tmp, W_SP);

        // Base Pointer (BP) guardado en el momento de la suspension
        hex_field(buf[i].rbp, tmp);
        print_centered(tmp, W_BP);

        // Nombre
        print_centered(buf[i].name, W_NAME);
        putchar('\n');
    }

    sys_exit(0);
}
