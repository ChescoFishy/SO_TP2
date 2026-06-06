// Conversion numerica para entorno freestanding (implementado en lib/format.c).
#ifndef FORMAT_H
#define FORMAT_H

#include <stdint.h>

// Convierte entero sin signo a string en base [2..16]. Devuelve la cantidad de
// caracteres escritos (sin el '\0').
uint64_t num_to_str(uint64_t value, char * dest, int base);

#endif
