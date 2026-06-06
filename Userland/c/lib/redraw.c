// Buffer de redraw: registra la salida por fd para re-renderizar al cambiar el
// tamaño de fuente. Compartido por la shell (shell.c llama redraw_* via shellPutchar).
#include <stdint.h>
#include "lib/redraw.h"
#include "lib/userlib.h"

// Detalles internos del buffer (antes en userlib.h): solo los usa este modulo.
#define REDRAW_BUFF 4096

typedef struct{
    char character;
    uint64_t fd;
}RedrawStruct;

RedrawStruct redrawBuffer[REDRAW_BUFF];
uint32_t redrawLength = 0;

void redraw_reset(void){
    redrawLength = 0;
}

void redraw_append_char(char c, uint64_t fd){
    if(redrawLength >= REDRAW_BUFF){
        // drop oldest
        for(uint32_t i = 1; i < redrawLength; i++){
            redrawBuffer[i-1] = redrawBuffer[i];
        }
        redrawLength--;
    }
    redrawBuffer[redrawLength].character = c;
    redrawBuffer[redrawLength].fd = fd;
    redrawLength++;
}

// Redibuja la pantalla luego de cambiar el tamaño de fuente
void redrawFont(){
    sys_clear();

    if(redrawLength == 0){
        return;
    }

    char buffer[REDRAW_BUFF];

    uint64_t current = redrawBuffer[0].fd;
    uint32_t idx = 0;

    for(uint32_t i = 0; i < redrawLength; i++){
        if(redrawBuffer[i].fd != current || idx >= sizeof(buffer) - 1){
            if(idx > 0){
                sys_write(current, buffer, idx);
                idx = 0;
            }
            current = redrawBuffer[i].fd;
        }
        buffer[idx++] = redrawBuffer[i].character;
    }

    if(idx > 0){
        sys_write(current, buffer, idx);
    }
}

// Aumenta tamaño de fuente y refresca contenido
void shellIncreaseFontSize(){
    sys_increase_fontsize();
    redrawFont();
}

// Disminuye tamaño de fuente y refresca contenido
void shellDecreaseFontSize(){
    sys_decrease_fontsize();
    redrawFont();
}
