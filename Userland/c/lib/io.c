// I/O de bajo nivel y parsing de argumentos compartido por los comandos.
#include <stdint.h>
#include "lib/io.h"
#include "lib/userlib.h"

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
