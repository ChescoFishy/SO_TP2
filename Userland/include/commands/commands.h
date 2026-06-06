// Prototipos de los comandos con logica propia (uno por archivo en c/commands/).
// La tabla commands[] de shell/parser.c referencia estos simbolos. Los helpers
// de I/O compartidos (read_full/next_uint) viven en lib/io.h.
#ifndef COMMANDS_H
#define COMMANDS_H

#include <stdint.h>

// ─── Comandos-proceso (entry de sys_create_process, admiten & y |) ────────────
void cat_main(int argc, char **argv);
void wc_main(int argc, char **argv);
void filter_main(int argc, char **argv);
void loop_main(int argc, char **argv);
void mvar_main(int argc, char **argv);

// ─── Builtins de administracion de procesos/memoria ───────────────────────────
void mem_cmd(void);
void kill_cmd(void);
void nice_cmd(void);
void block_cmd(void);

#endif
