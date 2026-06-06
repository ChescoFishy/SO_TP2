// Implementaciones minimas de string para entorno freestanding (sin libc).
#include "lib/strings.h"

// strlen minimo para entorno freestanding
size_t strlen(const char *s){
    size_t n = 0;
    if(s == 0) return 0;
    while(s[n] != '\0') n++;
    return n;
}

// strcmp minimo para entorno freestanding
int strcmp(const char *a, const char *b){
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
