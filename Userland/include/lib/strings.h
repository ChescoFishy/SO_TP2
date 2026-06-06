// Helpers de strings minimos para entorno freestanding (sin libc).
#ifndef STRINGS_H
#define STRINGS_H

#include <stddef.h>

size_t strlen(const char *s);
int    strcmp(const char *a, const char *b);

#endif
