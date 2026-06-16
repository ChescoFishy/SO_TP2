# Kernel Console Module

## Overview (¿Qué es?)
El módulo de [[kernel_console.md|Console]] (Consola) en el [[kernel_kernel.md|Kernel]] es el componente encargado de proveer soporte visual para la interacción y visualización de caracteres por pantalla. En nuestro sistema operativo, conviven dos esquemas: la consola nativa VGA de texto heredada (utilizada para depuración básica) y la consola gráfica VBE principal que maneja fuentes vectorizadas/escalables mediante mapas de bits en el framebuffer.

## Functionality (¿Qué hace?)
- Imprime caracteres y cadenas de texto en pantalla en modo gráfico (VBE) y modo de texto básico.
- Proporciona utilidades para dar formato e imprimir números en bases decimal, hexadecimal y binaria (`ncPrintDec`, `ncPrintHex`, `ncPrintBin`).
- Maneja un cursor de texto dinámico (`_`) que parpadea controlado por las interrupciones del PIT.
- Soporta el cambio dinámico del tamaño de fuente a través de atajos de teclado (`+` y `-`).
- Implementa desplazamiento de pantalla (scrolling) automático cuando el cursor alcanza el borde inferior.
- Permite limpiar la pantalla y configurar colores tanto de texto como de fondo.

## Internal Mechanics (¿Cómo funciona?)
1. **Consola Naiva (VGA):** Implementada en `naiveConsole.c`. Accede directamente a la dirección física `0xB8000` (memoria de video VGA clásica en modo texto). Modifica pares de bytes (carácter y atributos) en una matriz de 80x25.
2. **Consola Gráfica (VBE):** Implementada en `videoDriver.c`. Lee la información del hardware del bloque `vbe_mode_info` en `0x5C00`. Obtiene la dirección del *framebuffer*, el ancho, alto y bits por píxel (BPP) del modo gráfico actual.
3. **Renderizado de Fuentes:** Al imprimir un carácter gráfico, la función `drawChar` lee su representación de mapa de bits (bitmap) desde la matriz de la fuente definida en `font.h` (fuente bitmap de 8x16 píxeles). Escala el ancho y alto como `(FONT_WIDTH * size) / 4` y `(FONT_HEIGHT * size) / 4` y pinta pixel a pixel en el framebuffer mediante `putPixel`.
4. **Mapeo del Cursor:** El cursor se representa mediante el carácter `_` en la coordenada actual de escritura `(currentX, currentY)`. Antes de imprimir un carácter, se borra el cursor (`cursorErase`) rellenando la caja de texto con el color de fondo, se escribe el carácter real, y se vuelve a dibujar el cursor (`cursorDraw`) en su nueva ubicación.
5. **Mecanismo de Scroll:** Si `currentY + stepY` excede la altura de la pantalla, `scroll()` copia los bloques de memoria del framebuffer correspondientes a las filas visibles hacia arriba utilizando `memcpy`, calculando la compensación basada en la anchura de paso de la línea (`pitch`). La última línea se limpia pintándola con el color de fondo mediante `v_fillRectangle`.

## Key Files & Structs (Archivos y Estructuras Clave)
* **Archivos principales:**
  - `Kernel/c/console/naiveConsole.c`: Impresión básica en modo texto de 80x25 en `0xB8000`.
  - `Kernel/include/console/naiveConsole.h`: Definición de la interfaz de la consola de texto básica.
  - `Kernel/include/console/font.h`: Contiene la matriz de bytes `font` que representa los bitmaps de los caracteres ASCII (fuente de 8x16).
  - `Kernel/c/drivers/videoDriver.c`: Renderizador de texto y primitivas gráficas sobre el framebuffer de VBE.
* **Estructuras de datos:**
  - `vbe_mode_info_t` (en `videoDriver.c`): Contiene la información física del controlador VBE, incluyendo la dirección física del `framebuffer`, el `pitch` (bytes por línea), la resolución (`width`, `height`) y la profundidad de color (`bpp`).
* **Funciones fundamentales:**
  - `uintToBase(uint64_t value, char *buff, uint32_t base)`: Convierte un entero a una cadena en la base especificada.
  - `drawChar(uint32_t x, uint32_t y, uint8_t c, uint32_t color, uint64_t size)`: Dibuja un carácter píxel a píxel sobre el framebuffer gráfico escalando según `size`.
  - `scroll()`: Desplaza la memoria del framebuffer una línea hacia arriba usando `memcpy`.
  - `videoCursorBlink()`: Alterna la visibilidad del cursor; llamada por el PIT cada 10 ticks (aproximadamente 500 ms).

## System Calls Relacionadas
- **`sys_write` (Syscall 4):** Envía texto al flujo estándar del proceso; si este se mapea a la consola, imprime a través del video driver.
- **`sys_write_color` (Syscall 37):** Escribe en la consola con un color determinado por el parámetro `color` en formato RGB de 24 bits.
- **`sys_clear` (Syscall 9):** Limpia la pantalla gráfica y resetea las coordenadas del cursor a `(0,0)`.
- **`sys_increase_fontsize` (Syscall 5):** Aumenta el tamaño de letra en consola en 1 unidad.
- **`sys_decrease_fontsize` (Syscall 6):** Disminuye el tamaño de letra en consola en 1 unidad (mínimo 1).
- **`sys_set_cursor` (Syscall 38):** Habilita (`on != 0`) o deshabilita (`on == 0`) la presencia del cursor parpadeante en consola.

## Comments and Limitations (Comentarios y Limitaciones)
- **Ausencia de Aceleración:** Todo el renderizado y scrolling se hace de manera puramente software usando ciclos de CPU y `memcpy`. Esto puede generar ralentizaciones visuales menores si el tamaño de fuente es pequeño y la pantalla realiza scroll a alta velocidad.
- **Modos Mutuamente Excluyentes:** La consola nativa VGA (`naiveConsole.c`) y la gráfica VBE (`videoDriver.c`) escriben en áreas de memoria de video diferentes (`0xB8000` vs. dirección del Framebuffer). El sistema se ejecuta en modo gráfico VBE por defecto, por lo que la salida de naiveConsole no se ve en pantalla a menos que se configure el hardware explícitamente en modo texto 80x25.
