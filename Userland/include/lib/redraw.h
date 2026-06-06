// Buffer de redraw: registra la salida por fd para re-renderizar al cambiar el
// tamaño de fuente (implementado en lib/redraw.c).
#ifndef REDRAW_H
#define REDRAW_H

#include <stdint.h>

void redraw_reset(void);
void redraw_append_char(char c, uint64_t fd);

// Redibuja la pantalla con el contenido registrado (tras cambiar el fontsize).
void redrawFont(void);

// Cambian el tamaño de fuente y refrescan el contenido.
void shellIncreaseFontSize(void);
void shellDecreaseFontSize(void);

#endif
