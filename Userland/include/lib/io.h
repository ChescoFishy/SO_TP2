// I/O de bajo nivel y parsing de argumentos (implementado en lib/io.c).
#ifndef IO_H
#define IO_H

#include <stdint.h>

// putchar usando sys_write; getchar bloquea hasta tecla (0 en EOF/Ctrl+D).
uint64_t putchar(char c);
char     getchar(void);

/* Lectura que abstrae el reintento del teclado: si el proceso se bloqueo
** (READ_RETRY), reintenta tras despertar. Para pipes nunca devuelve READ_RETRY
** (pipe_read bloquea internamente). Devuelve n>0 con datos, 0 en EOF.
** Compartido por los comandos cat/wc/filter. */
uint64_t read_full(char *buf, uint64_t n);

/* Lee el siguiente entero positivo de *pp (saltea espacios). Devuelve -1 si no
** hay token numerico. Avanza *pp mas alla de los digitos leidos.
** Compartido por los comandos kill/nice/block. */
int64_t next_uint(const char **pp);

// print/scan con formato (simil printf/scanf): %d %u %x %s %c %%.
// print devuelve los caracteres escritos; scan, la cantidad de campos asignados.
int print(const char *fmt, ...);
int scan(const char *fmt, ...);

#endif
