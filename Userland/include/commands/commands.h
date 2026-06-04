// Prototipos de los comandos con logica propia (uno por archivo en c/commands/)
// y de los helpers compartidos entre ellos. La tabla commands[] de userlib.c
// referencia estos simbolos. Los comandos triviales (help/clear/registers/...)
// y el resto de utilitarios siguen viviendo en userlib.c.
#ifndef COMMANDS_H
#define COMMANDS_H

#include <stdint.h>

// ─── Helpers compartidos (implementados en userlib.c) ─────────────────────────
/* Lectura que abstrae el reintento del teclado: si el proceso se bloqueo
** (READ_RETRY), reintenta tras despertar. Para pipes nunca devuelve READ_RETRY
** (pipe_read bloquea internamente). Devuelve n>0 con datos, 0 en EOF. */
uint64_t read_full(char *buf, uint64_t n);

/* Lee el siguiente entero positivo de *pp (saltea espacios). Devuelve -1 si no
** hay token numerico. Avanza *pp mas alla de los digitos leidos. */
int64_t next_uint(const char **pp);

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
