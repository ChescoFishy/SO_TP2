// Conversion numerica para entorno freestanding (sin libc).
#include <stdint.h>
#include "lib/userlib.h"

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
