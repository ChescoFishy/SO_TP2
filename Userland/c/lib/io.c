// I/O de bajo nivel y parsing de argumentos compartido por los comandos.
#include <stdint.h>
#include <stdarg.h>
#include "lib/io.h"
#include "lib/format.h"
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

/* ── Salida con formato: print (simil printf) ──────────────────────────────── */

/* Imprime una cadena cruda con putchar y devuelve cuantos chars escribio. */
static int print_str(const char *s){
    int n = 0;
    while(*s){ putchar(*s++); n++; }
    return n;
}

/* Convierte value a la base dada y lo imprime; devuelve los chars escritos. */
static int print_num(uint64_t value, int base){
    char buf[21]; /* alcanza para uint64 en decimal (20 digitos + '\0') */
    num_to_str(value, buf, base);
    return print_str(buf);
}

/* print con formato. Soporta %d, %u, %x, %s, %c y %%. Devuelve la cantidad de
** caracteres efectivamente escritos. */
int print(const char *fmt, ...){
    va_list args;
    va_start(args, fmt);

    int count = 0;
    while(*fmt){
        if(*fmt != '%'){
            putchar(*fmt++);
            count++;
            continue;
        }
        fmt++; /* saltear el '%' */
        switch(*fmt){
            case 'd': {
                int64_t v = (int64_t)va_arg(args, int);
                if(v < 0){ putchar('-'); count++; v = -v; }
                count += print_num((uint64_t)v, 10);
                break;
            }
            case 'u':
                count += print_num((uint64_t)va_arg(args, unsigned int), 10);
                break;
            case 'x':
                count += print_num((uint64_t)va_arg(args, unsigned int), 16);
                break;
            case 's': {
                const char *s = va_arg(args, const char *);
                count += print_str(s ? s : "(null)");
                break;
            }
            case 'c':
                putchar((char)va_arg(args, int));
                count++;
                break;
            case '%':
                putchar('%');
                count++;
                break;
            default: /* especificador desconocido: emitir tal cual */
                putchar('%');
                putchar(*fmt);
                count += 2;
                break;
        }
        if(*fmt) fmt++;
    }

    va_end(args);
    return count;
}

/* ── Entrada con formato: scan (simil scanf) ───────────────────────────────── */

/* Pushback de un caracter para poder "devolver" el delimitador no consumido. */
static int scan_pb = -1;

/* Siguiente caracter del input (teclado/pipe). Devuelve 0 en EOF. */
static char scan_next(void){
    if(scan_pb >= 0){ char c = (char)scan_pb; scan_pb = -1; return c; }
    return getchar();
}

/* Devuelve un caracter al stream para la proxima lectura. */
static void scan_unget(char c){
    scan_pb = (unsigned char)c;
}

static int is_space(char c){
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

/* Valor de un digito en la base dada, o -1 si c no es digito valido. */
static int digit_val(char c, int base){
    int d;
    if(c >= '0' && c <= '9')      d = c - '0';
    else if(c >= 'a' && c <= 'f') d = c - 'a' + 10;
    else if(c >= 'A' && c <= 'F') d = c - 'A' + 10;
    else                          return -1;
    return (d < base) ? d : -1;
}

/* Lee un entero sin signo en la base dada. Guarda el valor en *out y devuelve 1
** si leyo al menos un digito; 0 en caso contrario (y deja el char en pushback). */
static int scan_uint(uint64_t *out, int base){
    char c = scan_next();
    int got = 0;
    uint64_t v = 0;
    int d;
    while((d = digit_val(c, base)) >= 0){
        v = v * (uint64_t)base + (uint64_t)d;
        got = 1;
        c = scan_next();
    }
    if(c) scan_unget(c);
    if(!got) return 0;
    *out = v;
    return 1;
}

/* scan con formato. Soporta %d, %u, %x, %s, %c. Un espacio en fmt consume cero o
** mas whitespace del input. Devuelve la cantidad de campos asignados. */
int scan(const char *fmt, ...){
    va_list args;
    va_start(args, fmt);

    int assigned = 0;
    while(*fmt){
        if(is_space(*fmt)){
            /* consumir whitespace del input */
            char c;
            while((c = scan_next()) && is_space(c))
                ;
            if(c) scan_unget(c);
            fmt++;
            continue;
        }
        if(*fmt != '%'){
            /* caracter literal: debe coincidir con el input */
            char c = scan_next();
            if(c != *fmt){ if(c) scan_unget(c); break; }
            fmt++;
            continue;
        }

        fmt++; /* saltear el '%' */
        switch(*fmt){
            case 'd': {
                /* saltar whitespace, signo opcional, luego digitos decimales */
                char c = scan_next();
                while(c && is_space(c)) c = scan_next();
                int neg = 0;
                if(c == '-'){ neg = 1; c = scan_next(); }
                else if(c == '+'){ c = scan_next(); }
                if(c) scan_unget(c);
                uint64_t v;
                if(!scan_uint(&v, 10)) goto done;
                *va_arg(args, int *) = neg ? -(int)v : (int)v;
                assigned++;
                break;
            }
            case 'u': {
                char c = scan_next();
                while(c && is_space(c)) c = scan_next();
                if(c) scan_unget(c);
                uint64_t v;
                if(!scan_uint(&v, 10)) goto done;
                *va_arg(args, unsigned int *) = (unsigned int)v;
                assigned++;
                break;
            }
            case 'x': {
                char c = scan_next();
                while(c && is_space(c)) c = scan_next();
                if(c) scan_unget(c);
                uint64_t v;
                if(!scan_uint(&v, 16)) goto done;
                *va_arg(args, unsigned int *) = (unsigned int)v;
                assigned++;
                break;
            }
            case 's': {
                /* saltar whitespace, leer hasta el proximo whitespace/EOF */
                char *dst = va_arg(args, char *);
                char c = scan_next();
                while(c && is_space(c)) c = scan_next();
                if(!c) goto done;
                int n = 0;
                while(c && !is_space(c)){ dst[n++] = c; c = scan_next(); }
                dst[n] = '\0';
                if(c) scan_unget(c);
                assigned++;
                break;
            }
            case 'c': {
                char c = scan_next();
                if(!c) goto done;
                *va_arg(args, char *) = c;
                assigned++;
                break;
            }
            default: /* especificador desconocido: ignorar */
                break;
        }
        if(*fmt) fmt++;
    }

done:
    va_end(args);
    return assigned;
}
