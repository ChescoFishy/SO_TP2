#include <stdint.h>
#include <stddef.h>
#include "../c/include/userlib.h"
#include "../c/include/shell.h"
#include "include/testMM.h"
#include "include/test_proc.h"
#include "include/test_prio.h"
#include "include/test_sync.h"
#include "include/test_util.h"

/* Forward decls de helpers definidos mas abajo. */
static size_t strlen(const char *s);
static int    strcmp(const char *a, const char *b);

static void cat_main(int argc, char **argv);
static void wc_main(int argc, char **argv);
static void filter_main(int argc, char **argv);
static void loop_main(int argc, char **argv);
static void mvar_main(int argc, char **argv);
static void mem_cmd(void);
static void kill_cmd(void);
static void nice_cmd(void);
static void block_cmd(void);

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

static void cat_main(int argc, char **argv){
    (void)argc; (void)argv;
    char buf[128];
    uint64_t n;
    while((n = sys_read(buf, sizeof(buf))) > 0){
        sys_write(STDOUT, buf, n);
    }
    sys_exit(0);
}

/* wc: cuenta lineas en stdin hasta EOF y reporta el total. */
static void wc_main(int argc, char **argv){
    (void)argc; (void)argv;
    char buf[128];
    uint64_t n;
    uint64_t lines = 0;
    while((n = sys_read(buf, sizeof(buf))) > 0){
        for(uint64_t i = 0; i < n; i++){
            if(buf[i] == '\n') lines++;
        }
    }
    char out[24];
    uint64_t len = num_to_str(lines, out, 10);
    out[len] = '\n';
    sys_write(STDOUT, out, len + 1);
    sys_exit(0);
}

static int is_vowel(char c){
    switch(c){
        case 'a': case 'e': case 'i': case 'o': case 'u':
        case 'A': case 'E': case 'I': case 'O': case 'U':
            return 1;
        default:
            return 0;
    }
}

/* filter: lee de stdin y reimprime todo salvo las vocales. Pensado para pipes
** (ej. cat | filter) pero tambien funciona con datos de un pipe nombrado. */
static void filter_main(int argc, char **argv){
    (void)argc; (void)argv;
    char buf[128], out[128];
    uint64_t n;
    while((n = sys_read(buf, sizeof(buf))) > 0){
        uint64_t j = 0;
        for(uint64_t i = 0; i < n; i++){
            if(!is_vowel(buf[i])) out[j++] = buf[i];
        }
        if(j > 0) sys_write(STDOUT, out, j);
    }
    sys_exit(0);
}

/* loop: imprime su PID con un saludo cada cierto tiempo usando espera ACTIVA
** (no se bloquea), como pide el enunciado. argv[0] opcional = periodo en ticks. */
static void loop_main(int argc, char **argv){
    uint64_t pid = sys_getpid();
    uint64_t period = (argc >= 1 && argv != 0 && argv[0] != 0)
                    ? (uint64_t)satoi(argv[0]) : 18;
    if(period == 0) period = 18;

    while(1){
        printf("Hola! soy el proceso %d\n", (int)pid);
        uint64_t start = sys_ticks();
        while(sys_ticks() - start < period)
            ;  /* espera activa: no cede CPU voluntariamente */
    }
}

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

static void mvar_main(int argc, char **argv){
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

/* Lee el siguiente entero positivo de *pp (saltea espacios). Devuelve -1 si no
** hay token numerico. Avanza *pp mas alla de los digitos leidos. */
static int64_t next_uint(const char **pp){
    const char *p = *pp;
    while(*p == ' ' || *p == '\t') p++;
    if(*p < '0' || *p > '9'){ *pp = p; return -1; }
    int64_t v = 0;
    while(*p >= '0' && *p <= '9'){ v = v * 10 + (*p - '0'); p++; }
    *pp = p;
    return v;
}

/* mem: imprime el estado de la memoria via sys_mem_status. */
static void mem_cmd(void){
    MemStatus st;
    sys_mem_status(&st);
    char b[24];
    shellPrintString("Estado de memoria (bytes):\n");
    shellPrintString("  Total: "); num_to_str(st.total, b, 10); shellPrintString(b); shellNewline();
    shellPrintString("  Usada: "); num_to_str(st.used,  b, 10); shellPrintString(b); shellNewline();
    shellPrintString("  Libre: "); num_to_str(st.free,  b, 10); shellPrintString(b); shellNewline();
    shellPrintString("  Bloques asignados: ");
    num_to_str(st.alloc_count, b, 10); shellPrintString(b); shellNewline();
}

/* kill <pid>: mata el proceso indicado. */
static void kill_cmd(void){
    const char *a = cmd_args();
    int64_t pid = (a != 0) ? next_uint(&a) : -1;
    if(pid <= 0){
        shellPrintString("uso: kill <pid>\n");
        return;
    }
    sys_kill((uint64_t)pid);
}

/* nice <pid> <prioridad>: cambia la prioridad de un proceso. */
static void nice_cmd(void){
    const char *a = cmd_args();
    int64_t pid  = (a != 0) ? next_uint(&a) : -1;
    int64_t prio = (a != 0) ? next_uint(&a) : -1;
    if(pid <= 0 || prio < 0){
        shellPrintString("uso: nice <pid> <prioridad>\n");
        return;
    }
    sys_nice((uint64_t)pid, (uint64_t)prio);
}

/* block <pid>: alterna el estado del proceso entre BLOCKED y READY. */
#define STATE_BLOCKED 3   /* coincide con PROCESS_BLOCKED del kernel */
static void block_cmd(void){
    const char *a = cmd_args();
    int64_t pid = (a != 0) ? next_uint(&a) : -1;
    if(pid <= 0){
        shellPrintString("uso: block <pid>\n");
        return;
    }
    static ProcessInfo buf[MAX_PROCESSES];
    uint64_t cnt = sys_ps(buf, MAX_PROCESSES);
    for(uint64_t i = 0; i < cnt; i++){
        if(buf[i].pid == (uint64_t)pid){
            if(buf[i].state == STATE_BLOCKED)
                sys_unblock((uint64_t)pid);
            else
                sys_block((uint64_t)pid);
            return;
        }
    }
    shellPrintString("block: PID no encontrado\n");
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

// Benchmark de CPU (operaciones int/float)
void bmCPU(){
    /*
     * bmCPU - simple CPU benchmark
     * - Runs a mix of integer and floating-point operations N times
     * - Measures elapsed ticks using sys_ticks() and prints total time
     *   and a rough operations-per-tick metric.
     *
     * Notes:
     * - N is kept reasonably large to get measurable tick counts.
     * - This is a coarse benchmark (no warming, no cycle-accurate timing).
     */
    const uint32_t N = 1000000u;
    uint64_t ticks = sys_ticks();

    uint64_t result = 0;
    double float_result = 0.0;

    for(uint32_t i = 0; i < N; ++i){
        result += (uint64_t)i * 7ull;
        result = result % 2147483647ull;

        float_result += (double)i * 3.14159;
        if ((i % 100000u) == 0 && i != 0) {
            float_result *= 0.5;
        }
    }

    uint64_t end_ticks = sys_ticks();
    uint64_t delta = end_ticks - ticks;

    shellPrintString("Tiempo: ");

    char timeBuff[32];
    num_to_str(delta, timeBuff, 10);
    shellPrintString(timeBuff);
    shellPrintString(" ticks\n");

    if(delta > 0){
        /* promote N to 64-bit before division to avoid surprises */
        uint64_t ops_per_tick = ((uint64_t)N) / delta;
        shellPrintString("Operaciones por tick: ");
        num_to_str(ops_per_tick, timeBuff, 10);
        shellPrintString(timeBuff);
        shellPrintString("\n");
    } else {
        shellPrintString("Elapsed ticks = 0, no se puede calcular ops/tick.\n");
    }

    return;
}

// Benchmark de FPS aproximado (limpia pantalla en loop)
void bmFPS(){    
    /*
     * bmFPS - crude frame-rate benchmark
     * - Repeatedly clears the screen for ~3 seconds and counts iterations.
     * - Assumes sys_ticks() increments roughly 18 times per second (BIOS-like).
     * - Avoid printing inside the loop to not skew the results.
     */
    uint64_t ticks = sys_ticks();
    uint64_t count = 0;
    /* 18 ticks ~= 1 second on many systems that emulate BIOS ticks */
    uint64_t duration = 18 * 7; /* ~3 seconds */

    shellPrintString("Inicio de test.\n");
    /* busy loop that only clears the screen and increments counter */
    while((sys_ticks() - ticks) < duration){
        sys_clear();
        count++;
    }

    /* count iterations over ~7 seconds -> approximate frames per second */
    uint64_t fps = count / 7;
    shellPrintString("FPS: ");

    char fpsBuff[BM_BUFF];
    num_to_str(fps, fpsBuff, 10);
    shellPrintString(fpsBuff);
    shellPrintString("\n");
}

// Benchmark simple de memoria (llenado/copia/checksum)
void bmMEM(){
    /*
     * bmMEM - simple memory benchmark
     * - Fills a 4KB buffer many times, computes a checksum and does copies.
     * - Measures elapsed ticks and reports operations per tick.
     *
     * Notes:
     * - Make sure the operations count is calculated using 64-bit to avoid
     *   intermediate overflow on 32-bit platforms.
     */
    char buffer[4 * KB];
    uint64_t totalChecksum = 0;
    uint64_t ticks = sys_ticks();

    for(int iteration = 0; iteration < 10000; iteration++){
        for(int i = 0; i < 4 * KB; i++){
            buffer[i] = (i + iteration) % 256;
        }

        /* use 64-bit checksum to avoid truncation issues */
        uint64_t checksum = 0;
        for(int i = 0; i < 4 * KB; i++){
            checksum += (unsigned char)buffer[i];
            checksum = checksum % 1000000ULL;
        }

        for(int i = 0; i < 2 * KB; i++){
            buffer[i + 2 * KB] = buffer[i];
        }

        /* make checksum observable to prevent over-optimization */
        totalChecksum += checksum;
    }

    uint64_t finalTicks = sys_ticks();
    uint64_t delta = finalTicks - ticks;

    shellPrintString("Tiempo: ");

    char buff[BM_BUFF];
    num_to_str(delta, buff, 10);
    shellPrintString(buff);
    shellPrintString(" ticks\n");

    if(delta > 0){
        /* compute operations using 64-bit arithmetic to be safe */
        uint64_t operations = (uint64_t)10000 * (uint64_t)(4 * KB) * 3ULL;
        uint64_t operationsPerCycle = operations / delta;
        shellPrintString("Operaciones por tick: ");
        num_to_str(operationsPerCycle, buff, 10);
        shellPrintString(buff);
        shellPrintString("\n");
    }

    /* Print a checksum summary to ensure computations aren't optimized away */
    shellPrintString("Checksum: ");
    num_to_str(totalChecksum, buff, 10);
    shellPrintString(buff);
    shellPrintString("\n");
}

// Mide tiempo hasta presionar una tecla
void bmKEY(){
    shellPrintString("Presione cualquier tecla: \n");
    uint64_t ticks = sys_ticks();
    getchar();

    uint64_t finalTicks = sys_ticks();
    uint64_t delta = finalTicks - ticks;
    shellPrintString("Tiempo: ");
    char buff[BM_BUFF];

    num_to_str(delta, buff, 10);
    shellPrintString(buff);
    shellPrintString(" ticks\n");
}

// Reproduce una secuencia corta de beeps
void playBeep(){
    sys_beep(NOTE_E5, EIGHTH);
    sys_beep(NOTE_DS5, EIGHTH);
    sys_beep(NOTE_E5, EIGHTH);
    sys_beep(NOTE_DS5, EIGHTH);
    sys_beep(NOTE_E5, EIGHTH);
    sys_beep(NOTE_B4, EIGHTH);
    sys_beep(NOTE_D5, EIGHTH);
    sys_beep(NOTE_C5, EIGHTH);
    sys_beep(NOTE_A4, QUARTER);

    sys_beep(NOTE_C4, EIGHTH);
    sys_beep(NOTE_E4, EIGHTH);
    sys_beep(NOTE_A4, EIGHTH);
    sys_beep(NOTE_B4, QUARTER);

    sys_beep(NOTE_E4, EIGHTH);
    sys_beep(NOTE_GS4, EIGHTH);
    sys_beep(NOTE_B4, EIGHTH);
    sys_beep(NOTE_C5, QUARTER);

    sys_beep(NOTE_E4, EIGHTH);
    sys_beep(NOTE_E5, EIGHTH);
    sys_beep(NOTE_DS5, EIGHTH);
    sys_beep(NOTE_E5, EIGHTH);
    sys_beep(NOTE_DS5, EIGHTH);
    sys_beep(NOTE_E5, EIGHTH);
    sys_beep(NOTE_B4, EIGHTH);
    sys_beep(NOTE_D5, EIGHTH);
    sys_beep(NOTE_C5, EIGHTH);
    sys_beep(NOTE_A4, QUARTER);

    sys_beep(NOTE_C4, EIGHTH);
    sys_beep(NOTE_E4, EIGHTH);
    sys_beep(NOTE_A4, EIGHTH);
    sys_beep(NOTE_B4, QUARTER);

    sys_beep(NOTE_E4, EIGHTH);
    sys_beep(NOTE_C5, EIGHTH);
    sys_beep(NOTE_B4, EIGHTH);
    sys_beep(NOTE_A4, QUARTER);
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
    while(sys_read(&c, 1) == 0)
        ;
    return c;
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