// Parser de la shell: tabla de comandos, tokenizacion, soporte de '&' (background)
// y '|' (pipe), y spawn de procesos (simples y conectados por pipe).
#include <stdint.h>
#include <stddef.h>
#include "lib/userlib.h"
#include "lib/strings.h"
#include "shell/shell.h"
#include "commands/commands.h"
#include "tests/testMM.h"
#include "tests/test_proc.h"
#include "tests/test_prio.h"
#include "tests/test_sync.h"
#include "tests/test_pipe.h"

Command commands[] = {
    /* Builtins: corren sincronicamente en la shell. No admiten & ni |. */
    {"help",      help,                 0,             "muestra esta ayuda"},
    {"clear",     clear,                0,             "limpia la pantalla"},

/*  ====================== COMANDOS DE ARQUI ===========================================*/
    // {"printTime", printTime,            0,             "imprime la hora actual"},
    // {"printDate", printDate,            0,             "imprime la fecha actual"},
    // {"registers", registers,            0,             "imprime registros (CTRL para capturar)"},
    // {"testDiv0",  divideByZero,         0,             "dispara excepcion #DE"},
    // {"invOp",     invOp,                0,             "dispara invalid opcode"},
    // {"playBeep",  playBeep,             0,             "reproduce una secuencia de beeps"},
    // {"bmFPS",     bmFPS,                0,             "benchmark de FPS"},
    // {"bmCPU",     bmCPU,                0,             "benchmark de CPU"},
    // {"bmMEM",     bmMEM,                0,             "benchmark de memoria"},
    // {"bmKEY",     bmKEY,                0,             "mide latencia de tecla"},
/*  ===================================================================================*/

    {"ps",        ps,                   0,             "lista los procesos activos"},
    {"mem",       mem_cmd,              0,             "estado de la memoria"},
    {"kill",      kill_cmd,             0,             "<pid>      mata un proceso"},
    {"nice",      nice_cmd,             0,             "<pid> <p>  cambia prioridad (1-5)"},
    {"block",     block_cmd,            0,             "<pid>      alterna BLOCKED/READY"},
    /* Procesos: la shell los lanza con sys_create_process. Admiten & y |. */
    {"test_mm",   0,        test_mm_main,    "<max>     test del memory manager"},
    {"test_proc", 0,        test_proc_main,  "<max>     test de procesos"},
    {"test_prio", 0,        test_prio_main,  "<target>  test de prioridades"},
    {"test_sync", 0,        test_sync_main,  "<p> <n> <s> test de sincronizacion"},
    {"test_pipe", 0,        test_pipe_main,  "[bytes]   test de pipes (prod/cons)"},
    {"cat",       0,        cat_main,        "imprime stdin tal como lo recibe"},
    {"echo",      0,        echo_main,       "imprime sus argumentos"},
    {"astdin",    0,        astdin_main,     "emite la letra 'a' por stdout (para pipes)"},
    {"wc",        0,        wc_main,         "cuenta lineas en stdin"},
    {"filter",    0,        filter_main,     "filtra las vocales de stdin"},
    {"red",       0,        red_main,        "imprime stdin en rojo"},
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
