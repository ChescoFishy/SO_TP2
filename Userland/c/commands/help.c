// Builtin help: lista los comandos disponibles desde la tabla commands[].
#include <stdint.h>
#include "lib/userlib.h"
#include "lib/strings.h"
#include "shell/shell.h"

// La tabla commands[] vive en shell/parser.c; la exponemos via find/iteracion.
extern Command commands[];

// Tests provistos por la catedra. El resto de los test_* (p. ej. test_pipe) son
// propios del grupo y se listan junto al resto de los comandos.
static int is_test_catedra(const char *name){
    return strcmp(name, "test_mm")   == 0 || strcmp(name, "test_proc") == 0
        || strcmp(name, "test_prio") == 0 || strcmp(name, "test_sync") == 0;
}

// ─── Paginador estilo `more` ──────────────────────────────────────────────────
// Con fuente grande caben pocas filas y la consola no tiene scrollback: si help
// imprimiera todo de una vez, las primeras lineas se irian por arriba sin poder
// recuperarse. Pausamos cada pantalla esperando que el usuario presione Enter.
//
// Importante: contamos filas FISICAS, no lineas logicas. Con la fuente agrandada
// una linea larga no entra en el ancho y hace wrap, ocupando mas de una fila; si
// contaramos de a una se desincronizaria con el scroll real y se perderia texto.
#define HELP_LINE_MAX 128

static int g_rows;   // filas que caben en pantalla (cacheado al inicio del help)
static int g_cols;   // columnas que caben en pantalla
static int g_used;   // filas fisicas ya usadas en la pagina actual

static void pager_reset(void){
    g_rows = (int)sys_console_rows();
    g_cols = (int)sys_console_cols();
    if(g_rows < 3) g_rows = 3;   // sanity: dejar lugar para contenido + prompt
    if(g_cols < 1) g_cols = 1;
    g_used = 0;
}

// Filas fisicas que ocupa una linea de 'len' caracteres (cuenta el wrap).
static int rows_for(int len){
    if(len <= 0) return 1;
    return (len + g_cols - 1) / g_cols;
}

// Espera Enter, limpia la pantalla (borra el prompt "-- mas --") y arranca una
// pagina nueva desde arriba. Limpiar evita ademas el scroll automatico, que
// volveria a desincronizar el conteo de filas.
static void pager_pause(void){
    shellPrintString("-- mas -- (Enter para continuar)");
    char c;
    do { c = getchar(); } while(c != '\n' && c != 0);  // espera Enter / EOF
    sys_clear();
    g_used = 0;
}

// Emite una linea (ya armada, sin '\n'); pagina antes si no entra en la pagina
// actual reservando una fila para el prompt.
static void pager_emit(const char *line){
    int need = rows_for((int)strlen(line));
    if(g_used + need > g_rows - 1){   // -1: reservar fila para el prompt
        pager_pause();
    }
    shellPrintString((char *)line);
    shellPrintString("\n");
    g_used += need;
}

// Atajo: emite una linea de texto literal.
static void pager_line(const char *s){
    pager_emit(s);
}

// Agrega 'src' a 'dst' a partir de *k (sin pasar HELP_LINE_MAX). Devuelve cuantos
// chars escribio.
static void buf_append(char *dst, int *k, const char *src){
    while(*src && *k < HELP_LINE_MAX - 1){
        dst[(*k)++] = *src++;
    }
}

// Imprime una fila "  <nombre><padding hasta columna 12><descripcion>".
static void print_command(const Command *cmd){
    char line[HELP_LINE_MAX];
    int k = 0;
    buf_append(line, &k, "  ");
    buf_append(line, &k, cmd->name);
    int pad = 12 - (int)strlen(cmd->name);
    for(int j = 0; j < pad && k < HELP_LINE_MAX - 1; j++) line[k++] = ' ';
    buf_append(line, &k, cmd->description ? cmd->description : "");
    line[k] = 0;
    pager_emit(line);
}

// Lista de comandos disponibles, generada desde la tabla commands[]. Los tests
// de la catedra se agrupan en un apartado propio al final. La salida se pagina
// (ver pager_*) para que no se pierdan lineas con la fuente agrandada.
void help(){
    pager_reset();
    pager_line("Comandos disponibles:");
    for(int i = 0; commands[i].name != 0; i++){
        if(is_test_catedra(commands[i].name)) continue;   // se listan aparte
        print_command(&commands[i]);
    }
    pager_line("Operadores:");
    pager_line("  cmd &        ejecuta en background");
    pager_line("  cmd1 | cmd2  pipe (ambos comandos deben ser procesos)");
    pager_line("  Ctrl+C       mata el proceso foreground actual");
    pager_line("  Ctrl+D       envia EOF al stdin");
    pager_line("Tests de la catedra:");
    for(int i = 0; commands[i].name != 0; i++){
        if(!is_test_catedra(commands[i].name)) continue;
        print_command(&commands[i]);
    }
}
