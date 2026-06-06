// Builtins de prueba de excepciones: dispara #DE e invalid opcode.
#include "lib/userlib.h"

// Provoca excepción de división por cero
void divideByZero(){
    clear();
    int x = 1;
    int y = 0;
    int z;
    z = x / y; // dispara #DE
    (void)z;   // evitar warning de variable no usada (si no se dispara la excepción)
}

void invOp(){
    gen_invalid_opcode();
}
