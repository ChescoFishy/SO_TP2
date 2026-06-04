#include <stdint.h>
#include <stddef.h>
#include "lib/userlib.h"
#include "shell/shell.h"
#include "commands/commands.h"
#include "tests/testMM.h"
#include "tests/test_proc.h"
#include "tests/test_prio.h"
#include "tests/test_sync.h"

/* Forward decls de helpers definidos mas abajo. */
static size_t strlen(const char *s);
static int    strcmp(const char *a, const char *b);

static Command commands[] = {
    /* Builtins: corren sincronicamente en la shell. No admiten & ni |. */
    {"help",      help,                 0,             "muestra esta ayuda"},
    {"clear",     clear,                0,             "limpia la pantalla"},
    {"printTime", printTime,            0,             "imprime la hora actual"},
    {"printDate", printDate,            0,             "imprime la fecha actual"},
    {"registers", registers,            0,             "imprime registros (CTRL para capturar)"},
    {"testDiv0",  divideByZero,         0,             "dispara excepcion #DE"},
    {"invOp",     invOp,                0,             "dispara invalid opcode"},
    {"playBeep",  playBeep,             0,             "reproduce una secuencia de beeps"},
    {"bmFPS",     bmFPS,                0,             "benchmark de FPS"},
    {"bmCPU",     bmCPU,                0,             "benchmark de CPU"},
    {"bmMEM",     bmMEM,                0,             "benchmark de memoria"},
    {"bmKEY",     bmKEY,                0,             "mide latencia de tecla"},
    {"ps",        ps,                   0,             "lista los procesos activos"},
    {"mem",       mem_cmd,              0,             "estado de la memoria"},
    {"kill",      kill_cmd,             0,             "<pid>      mata un proceso"},
    {"nice",      nice_cmd,             0,             "<pid> <p>  cambia prioridad (1-5)"},
    {"block",     block_cmd,            0,             "<pid>      alterna BLOCKED/READY"},
    /* Procesos: la shell los lanza con sys_create_process. Admiten & y |. */
    {"test_mm",   0,        test_mm_main,    "<max>     test del memory manager"},
    {"test_proc", 0,        test_proc_main,  "<max>     test de procesos"},
    {"test_prio", 0,        test_prio_main,  "<target>  test de prioridades"},
    {"test_sync", 0,        test_sync_main,  "<n> <s>   test de sincronizacion"},
    {"cat",       0,        cat_main,        "copia stdin a stdout hasta EOF"},
    {"wc",        0,        wc_main,         "cuenta lineas en stdin"},
    {"filter",    0,        filter_main,     "filtra las vocales de stdin"},
    {"loop",      0,        loop_main,       "[ticks]   imprime su PID periodicamente"},
    {"mvar",      0,        mvar_main,       "<w> <r>   demo MVar lectores/escritores"},
    {0, 0, 0, 0},
};

/* Argumentos del comando actual (compat). cmd_args() devuelve la string
** completa post-primer espacio o NULL. Solo lo setean los builtins via shell. */
static const char *g_cmd_args = 0;

const char *cmd_args(void){
    return g_cmd_args;
}

/* Lectura que abstrae el reintento del teclado: si el proceso se bloqueo
** (READ_RETRY), reintenta tras despertar. Para pipes nunca devuelve READ_RETRY
** (pipe_read bloquea internamente). Devuelve n>0 con datos, 0 en EOF.
** Compartido por los comandos cat/wc/filter (ver commands/commands.h). */
uint64_t read_full(char *buf, uint64_t n){
    uint64_t r;
    while((r = sys_read(buf, n)) == READ_RETRY)
        ;
    return r;
}

/* Lee el siguiente entero positivo de *pp (saltea espacios). Devuelve -1 si no
** hay token numerico. Avanza *pp mas alla de los digitos leidos.
** Compartido por los comandos kill/nice/block (ver commands/commands.h). */
int64_t next_uint(const char **pp){
    const char *p = *pp;
    while(*p == ' ' || *p == '\t') p++;
    if(*p < '0' || *p > '9'){ *pp = p; return -1; }
    int64_t v = 0;
    while(*p >= '0' && *p <= '9'){ v = v * 10 + (*p - '0'); p++; }
    *pp = p;
    return v;
}


RedrawStruct redrawBuffer[REDRAW_BUFF];
uint32_t redrawLength = 0;
void redraw_reset(void){
    redrawLength = 0;
}

void redraw_append_char(char c, uint64_t fd){
    if(redrawLength >= REDRAW_BUFF){
        // drop oldest
        for(uint32_t i = 1; i < redrawLength; i++){
            redrawBuffer[i-1] = redrawBuffer[i];
        }
        redrawLength--;
    }
    redrawBuffer[redrawLength].character = c;
    redrawBuffer[redrawLength].fd = fd;
    redrawLength++;
}


/* Convierte un entero sin signo a cadena en la base indicada.
   - value: número a convertir.
   - dest: buffer destino (debe tener espacio suficiente), se escribe NUL-terminated.
   - base: base entre 2 y 16 (por ejemplo 10 para decimal, 16 para hex).
   Retorna la cantidad de caracteres escritos (sin incluir el '\0').
*/
// Convierte entero sin signo a string en base [2..16]
uint64_t num_to_str(uint64_t value, char * dest, int base){
    if(!dest) return 0;
    if(base < 2 || base > 16) base = 10;

    char tmp[65];
    int pos = 0;

    if(value == 0){
        tmp[pos++] = '0';
    } else {
        while(value){
            int d = value % base;
            tmp[pos++] = (d < 10) ? ('0' + d) : ('A' + (d - 10));
            value /= base;
        }
    }

    /* volcar en orden correcto */
    for(int i = 0; i < pos; i++){
        dest[i] = tmp[pos - 1 - i];
    }
    dest[pos] = '\0';
    return (uint64_t)pos;
}

// Redibuja la pantalla luego de cambiar el tamaño de fuente
void redrawFont(){
    sys_clear();

    if(redrawLength == 0){
        return;
    }

    char buffer[REDRAW_BUFF];

    uint64_t current = redrawBuffer[0].fd;
    uint32_t idx = 0;

    for(uint32_t i = 0; i < redrawLength; i++){
        if(redrawBuffer[i].fd != current || idx >= sizeof(buffer) - 1){
            if(idx > 0){
                sys_write(current, buffer, idx);
                idx = 0;
            }
            current = redrawBuffer[i].fd;
        }
        buffer[idx++] = redrawBuffer[i].character;
    }

    if(idx > 0){
        sys_write(current, buffer, idx);
    }
}

// Aumenta tamaño de fuente y refresca contenido
void shellIncreaseFontSize(){
    sys_increase_fontsize();
    redrawFont();
}

// Disminuye tamaño de fuente y refresca contenido
void shellDecreaseFontSize(){
    sys_decrease_fontsize();
    redrawFont();
}

// Lista de comandos disponibles, generada desde la tabla commands[].
void help(){
    shellPrintString("Comandos disponibles:\n");
    for(int i = 0; commands[i].name != 0; i++){
        shellPrintString("  ");
        shellPrintString(commands[i].name);
        /* padding hasta columna 12 */
        int pad = 12 - (int)strlen(commands[i].name);
        for(int j = 0; j < pad; j++) shellPrintString(" ");
        shellPrintString(commands[i].description ? (char *)commands[i].description : "");
        shellPrintString("\n");
    }
    shellPrintString("Operadores:\n");
    shellPrintString("  cmd &        ejecuta en background\n");
    shellPrintString("  cmd1 | cmd2  pipe (ambos comandos deben ser procesos)\n");
    shellPrintString("  Ctrl+C       mata el proceso foreground actual\n");
    shellPrintString("  Ctrl+D       envia EOF al stdin\n");
}

// Limpia la pantalla
void clear(){
    sys_clear();
    redraw_reset();
}

// Provoca excepción de división por cero
void divideByZero(){
    clear();
    int x = 1;
    int y = 0;
    int z;
    z = x / y; // dispara #DE
    (void)z;   // evitar warning de variable no usada (si no se dispara la excepción)
}

void invOp(){
    gen_invalid_opcode();
}

// Imprime el snapshot de registros (CTRL para capturar)
void registers(){
    char buffer[REGSBUFF];

    if(sys_registers(buffer)){
        shellPrintString(buffer);
    } else{
        shellPrintString("Presione CTRL para guardar los registros.\n");
    }
}

// Ajusta hora BCD por offset (0-23)
uint8_t adjustHour(uint8_t hour, int offset){
    int decimalHour = ((hour >> 4) * 10) + (hour & 0x0F);
    decimalHour += offset;

     // Ajustar para que esté en el rango 0-23
    if (decimalHour < 0){
        decimalHour += 24;
    }else{
          if(decimalHour >= 24){
            decimalHour -= 24;
          }
    }

     return ((decimalHour / 10) << 4) | (decimalHour % 10);
}

// Imprime HH:MM:SS o DD/MM/AA desde buffer BCD
void printTimeAndDate(uint8_t* buff, char separator){
    char outBuff[10];

    for(int i = 0; i < 3; i++){
        int value = ((buff[i] >> 4) & 0x0F) * 10 + (buff[i] & 0x0F);
        outBuff[3 * i] = (char)('0' + (value / 10));
        outBuff[3 * i + 1] = (char)('0' + (value % 10));

        if(i < 2){
            outBuff[3 * i + 2] = separator;
        }
    }

    outBuff[8] = '\n';
    outBuff[9] = 0;

    shellPrintString(outBuff);
}

// Imprime hora local (UTC-3)
void printTime(){
    uint8_t timeBuff[3];
    sys_time(timeBuff);
    timeBuff[0] = adjustHour(timeBuff[0], -3);
    printTimeAndDate(timeBuff, ':');
}

// Imprime fecha local considerando rollover por UTC-3
void printDate(){
    uint8_t timeBuff[3];
    uint8_t dateBuff[3];

    sys_time(timeBuff);
    sys_date(dateBuff);

    int hour = ((timeBuff[0] >> 4) * 10) + (timeBuff[0] & 0x0F);

    if(hour < 3){
        int day = ((dateBuff[0] >> 4) * 10) + (dateBuff[0] & 0x0F);
        day--;

        if(day <= 0){
            day = 30;
            int month = ((dateBuff[1] >> 4) * 10) + (dateBuff[1] & 0x0F);
            month--;

            if(month <= 0){
               month = 12;
               int year = ((dateBuff[2] >> 4) * 10) + (dateBuff[2] & 0x0F);
               year--;
               dateBuff[2] = ((year / 10) << 4) | (year % 10);
            }

            dateBuff[1] = ((month / 10) << 4) | (month % 10);
        }

        dateBuff[0] = ((day / 10) << 4) | (day % 10);
    }

    printTimeAndDate(dateBuff, '/');
}

// Implementaciones mínimas de string para entorno freestanding
// strlen mínimo para entorno freestanding (interno a este módulo)
static size_t strlen(const char *s){
    size_t n = 0;
    if(s == 0) return 0;
    while(s[n] != '\0') n++;
    return n;
}

// strcmp mínimo para entorno freestanding (interno a este módulo)
static int strcmp(const char *a, const char *b){
    if(a == 0 && b == 0){
          return 0;
    }

    if(a == 0){
          return -1;
    }

    if(b == 0){
          return 1;
    }

    while(*a && (*a == *b)){
        a++;
        b++;
    }

    return (unsigned char)*a - (unsigned char)*b;
}

// putchar usando sys_write
uint64_t putchar(char c){
    char buff[1];
    buff[0] = c;
    redraw_append_char(c, STDOUT);
    return sys_write(STDOUT, buff, 1);
}

char getchar(){
    char c;
    uint64_t r;
    /* Reintentar mientras el teclado bloquee; devolver 0 en EOF (Ctrl+D). */
    while((r = sys_read(&c, 1)) == READ_RETRY)
        ;
    return (r >= 1) ? c : 0;
}

/* ── Parser de la shell ───────────────────────────────────────────────────── */

#define MAX_ARGS 16

/* Tokeniza `line` in-place: inserta NULs y llena argv[]. Devuelve argc. */
static int tokenize(char *line, char *argv[], int max){
    int argc = 0;
    char *p = line;
    while(*p && argc < max - 1){
        while(*p == ' ' || *p == '\t') p++;
        if(!*p) break;
        argv[argc++] = p;
        while(*p && *p != ' ' && *p != '\t') p++;
        if(*p) *p++ = '\0';
    }
    argv[argc] = 0;
    return argc;
}

static Command *find_cmd(const char *name){
    for(int i = 0; commands[i].name != 0; i++){
        if(strcmp(commands[i].name, name) == 0) return &commands[i];
    }
    return 0;
}

/* Duplica argv (apuntadores + strings) en un solo bloque heap. Usado para
** procesos background, ya que el buff de la shell se reescribe en el
** proximo readline. Layout: [char*[argc]] | [strings concatenadas].
** Retorna NULL si no hay memoria. El bloque queda en leak (proceso bg). */
static char **dup_argv(int argc, char **argv){
    if(argc <= 0) return 0;
    uint64_t strs = 0;
    for(int i = 0; i < argc; i++) strs += strlen(argv[i]) + 1;
    uint64_t header = sizeof(char *) * (uint64_t)argc;
    char **out = (char **)sys_malloc(header + strs);
    if(!out) return 0;
    char *dst = (char *)out + header;
    for(int i = 0; i < argc; i++){
        out[i] = dst;
        const char *src = argv[i];
        while((*dst++ = *src++))
            ;
    }
    return out;
}

/* Spawnea un proceso entry con los args dados. Para fg, hace waitpid y libera
** el bloque argv heap. Para bg, deja correr y reporta el PID. */
static void spawn_simple(Command *c, int argc, char **argv, uint8_t fg){
    char **owned_argv = dup_argv(argc, argv);
    if(argc > 0 && owned_argv == 0){
        shellPrintString("error: sin memoria\n");
        return;
    }
    int64_t pid = sys_create_process(c->name, (void *)c->entry,
                                     argc, owned_argv, fg);
    if(pid <= 0){
        shellPrintString("error: no se pudo crear proceso\n");
        if(owned_argv) sys_free(owned_argv);
        return;
    }
    if(fg){
        sys_waitpid((uint64_t)pid);
        if(owned_argv) sys_free(owned_argv);
    } else {
        /* bg: leak intencional de owned_argv hasta el fin del kernel. */
        char numbuf[16];
        num_to_str((uint64_t)pid, numbuf, 10);
        shellPrintString("[bg] pid ");
        shellPrintString(numbuf);
        shellPrintString("\n");
    }
}

/* Spawnea dos procesos conectados por un pipe. Foreground espera ambos. */
static void spawn_pipe(Command *c1, int argc1, char **argv1,
                       Command *c2, int argc2, char **argv2, uint8_t fg){
    int fds[2];
    if(sys_pipe(fds) < 0){
        shellPrintString("error: no se pudo crear pipe\n");
        return;
    }

    char **a1 = dup_argv(argc1, argv1);
    char **a2 = dup_argv(argc2, argv2);
    if((argc1 > 0 && !a1) || (argc2 > 0 && !a2)){
        shellPrintString("error: sin memoria\n");
        if(a1) sys_free(a1);
        if(a2) sys_free(a2);
        sys_pipe_close(fds[0]);
        sys_pipe_close(fds[1]);
        return;
    }

    uint64_t pack1 = (uint64_t)fg
                   | ((uint64_t)(uint16_t)0       << 16)
                   | ((uint64_t)(uint16_t)fds[1]  << 32);
    uint64_t pack2 = (uint64_t)fg
                   | ((uint64_t)(uint16_t)fds[0]  << 16)
                   | ((uint64_t)(uint16_t)1       << 32);

    int64_t pid1 = sys_create_process_fd(c1->name, (void *)c1->entry,
                                         argc1, a1, pack1);
    int64_t pid2 = sys_create_process_fd(c2->name, (void *)c2->entry,
                                         argc2, a2, pack2);

    sys_pipe_close(fds[0]);
    sys_pipe_close(fds[1]);

    if(pid1 <= 0 || pid2 <= 0){
        shellPrintString("error: no se pudo crear algun hijo del pipe\n");
        if(pid1 > 0) sys_kill((uint64_t)pid1);
        if(pid2 > 0) sys_kill((uint64_t)pid2);
    }

    if(fg){
        if(pid2 > 0) sys_waitpid((uint64_t)pid2);
        /* Si pid2 murio (e.g. Ctrl+C) y pid1 esta en un loop sin IO,
        ** no detectaria broken pipe; lo matamos explicitamente. */
        if(pid1 > 0){
            sys_kill((uint64_t)pid1);
            sys_waitpid((uint64_t)pid1);
        }
        if(a1) sys_free(a1);
        if(a2) sys_free(a2);
    } else {
        char numbuf[16];
        num_to_str((uint64_t)pid1, numbuf, 10);
        shellPrintString("[bg] pid ");
        shellPrintString(numbuf);
        shellPrintString(" | ");
        num_to_str((uint64_t)pid2, numbuf, 10);
        shellPrintString(numbuf);
        shellPrintString("\n");
    }
}

/* Parsea una linea de shell con soporte para '&' (background) y '|' (pipe). */
void processLine(char * buff, uint32_t * history_len){
    (void)history_len;
    size_t L = strlen(buff);
    if(L == 0) return;

    /* Trim trailing spaces. */
    while(L > 0 && buff[L-1] == ' ') buff[--L] = '\0';
    if(L == 0) return;

    /* Trailing '&' → background. */
    uint8_t fg = 1;
    if(buff[L-1] == '&'){
        fg = 0;
        buff[--L] = '\0';
        while(L > 0 && buff[L-1] == ' ') buff[--L] = '\0';
        if(L == 0){ shellPrintString("error: '&' sin comando\n"); return; }
    }

    /* Buscar '|' (un solo pipe soportado). */
    char *pipe_pos = 0;
    for(size_t i = 0; buff[i]; i++){
        if(buff[i] == '|'){ pipe_pos = &buff[i]; break; }
    }

    if(pipe_pos != 0){
        *pipe_pos = '\0';
        char *left  = buff;
        char *right = pipe_pos + 1;

        char *argv_l[MAX_ARGS], *argv_r[MAX_ARGS];
        int argc_l = tokenize(left,  argv_l, MAX_ARGS);
        int argc_r = tokenize(right, argv_r, MAX_ARGS);
        if(argc_l == 0 || argc_r == 0){
            shellPrintString("error: pipe con lado vacio\n");
            return;
        }
        Command *c1 = find_cmd(argv_l[0]);
        Command *c2 = find_cmd(argv_r[0]);
        if(!c1 || !c2){
            shellPrintString("Comando no reconocido en pipe.\n");
            return;
        }
        if(c1->entry == 0 || c2->entry == 0){
            shellPrintString("error: ambos lados del pipe deben ser procesos\n");
            return;
        }
        spawn_pipe(c1, argc_l - 1, argv_l + 1,
                   c2, argc_r - 1, argv_r + 1, fg);
        return;
    }

    /* Comando simple. */
    char *argv[MAX_ARGS];
    int argc = tokenize(buff, argv, MAX_ARGS);
    if(argc == 0) return;

    Command *c = find_cmd(argv[0]);
    if(!c){
        shellPrintString("Comando no reconocido! Escriba 'help' para ver los comandos disponibles.\n");
        return;
    }

    if(c->builtin != 0){
        if(!fg){
            shellPrintString("error: los builtins no admiten '&'\n");
            return;
        }
        /* Compat: exponer la string completa post-primer-espacio via cmd_args(). */
        g_cmd_args = 0;
        if(argc > 1){
            /* argv[1] apunta dentro del buff original; el byte previo fue
            ** sobrescrito con '\0' por tokenize. Restaurarlo a ' ' para
            ** reconstruir la string original. */
            for(int k = 1; k < argc; k++){
                if(k > 1) *(argv[k] - 1) = ' ';
            }
            g_cmd_args = argv[1];
        }
        c->builtin();
        g_cmd_args = 0;
        return;
    }

    /* Proceso. */
    spawn_simple(c, argc - 1, argv + 1, fg);
}

static const char *state_names[] = {"FREE", "READY", "RUNNING", "BLOCKED", "ZOMBIE"};

// Muestra la lista de procesos activos con PID, nombre, prioridad y estado
void ps(void) {
    static ProcessInfo buf[MAX_PROCESSES];
    uint64_t count = sys_ps(buf, MAX_PROCESSES);
    char tmp[24];

    shellPrintString("PID  PRI  FG  STATE    NAME\n");
    shellPrintString("---  ---  --  -------  --------\n");

    for (uint64_t i = 0; i < count; i++) {
        // PID
        num_to_str(buf[i].pid, tmp, 10);
        shellPrintString(tmp);
        shellPrintString("    ");

        // Prioridad
        num_to_str(buf[i].priority, tmp, 10);
        shellPrintString(tmp);
        shellPrintString("    ");

        // Foreground
        shellPrintString(buf[i].foreground ? "Y " : "N ");
        shellPrintString("  ");

        // Estado
        uint8_t st = buf[i].state;
        if (st <= 4)
            shellPrintString((char *)state_names[st]);
        else
            shellPrintString("?");
        shellPrintString("  ");

        // Nombre
        shellPrintString(buf[i].name);
        shellPrintString("\n");
    }
}
