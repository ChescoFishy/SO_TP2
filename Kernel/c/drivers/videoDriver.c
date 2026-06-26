#include "drivers/videoDriver.h"
#include "lib/defs.h"
#include "console/font.h"
#include <stdint.h>
#include <string.h>

typedef struct vbe_mode_info_structure {
  uint16_t attributes;
  uint8_t window_a;
  uint8_t window_b;
  uint16_t granularity;
  uint16_t window_size;
  uint16_t segment_a;
  uint16_t segment_b;
  uint32_t win_func_ptr;
  uint16_t pitch;
  uint16_t width;
  uint16_t height;
  uint8_t w_char;
  uint8_t y_char;
  uint8_t planes;
  uint8_t bpp;
  uint8_t banks;
  uint8_t memory_model;
  uint8_t bank_size;
  uint8_t image_pages;
  uint8_t reserved0;
  uint8_t red_mask;
  uint8_t red_position;
  uint8_t green_mask;
  uint8_t green_position;
  uint8_t blue_mask;
  uint8_t blue_position;
  uint8_t reserved_mask;
  uint8_t reserved_position;
  uint8_t direct_color_attributes;
  uint32_t framebuffer;
  uint32_t off_screen_mem_off;
  uint16_t off_screen_mem_size;
  uint8_t reserved1[206];
} __attribute__((packed)) vbe_mode_info_t;

static vbe_mode_info_t *vbe_mode_info = (vbe_mode_info_t *)0x5C00;

static uint64_t currentX = 0;
static uint64_t currentY = 0;
static const int bgColor = 0x000000;         // Negro
static uint64_t defaultTextSize = TEXT_SIZE; // Tamaño por defecto
static void updateCursor(void);
static void moveRight(void);
static void newLineRaw(void);
static void cursorErase(void);
static void cursorDraw(void);
static void drawChar(uint32_t x, uint32_t y, uint8_t c, uint32_t color,
                     uint64_t size);
static void v_fillRectangle(uint64_t x0, uint64_t y0, uint64_t x1, uint64_t y1,
                            uint32_t color);
static void drawFilledRect(uint32_t x, uint32_t y, uint32_t width,
                           uint32_t height, uint32_t color);

/* Cursor de texto del driver: un '_' en la posicion actual de escritura que
** parpadea desde el timer del kernel (videoCursorBlink). Toda salida de texto
** lo borra antes de tocar la pantalla y lo redibuja despues, asi lo que
** escribe cualquier proceso queda siempre delante del cursor, como si se
** tipeara en una terminal. */
#define CURSOR_CHAR '_'
#define CURSOR_COLOR 0xFFFFFF
static int cursorEnabled = 0; // la shell lo enciende mientras lee una linea
static int cursorDrawn = 0;   // hay un '_' pintado en (currentX, currentY)

// Ancho en píxeles del modo actual
uint16_t getScreenWidth() { return vbe_mode_info->width; }

// Alto en píxeles del modo actual
uint16_t getScreenHeight() { return vbe_mode_info->height; }

// Permite ajustar el tamaño por defecto usado por putChar
void setDefaultTextSize(uint64_t size) {
  defaultTextSize = (size == 0) ? (1) : (size);
}

uint64_t getDefaultTextSize(void) { return defaultTextSize; }

// Filas de texto que caben en pantalla con el tamaño de fuente actual. Usa la
// misma altura de fila que scroll()/newLineRaw(): (defaultTextSize*FONT_HEIGHT)/4.
uint64_t getConsoleRows(void) {
  uint64_t lineHeight = (defaultTextSize * FONT_HEIGHT) / 4;
  if (lineHeight == 0)
    lineHeight = 1;
  uint64_t rows = vbe_mode_info->height / lineHeight;
  return (rows == 0) ? 1 : rows;
}

// Columnas de texto que caben en pantalla con el tamaño de fuente actual. Usa el
// mismo avance horizontal que moveRight(): (FONT_WIDTH*defaultTextSize)/4.
uint64_t getConsoleCols(void) {
  uint64_t stepX = (FONT_WIDTH * defaultTextSize) / 4;
  if (stepX == 0)
    stepX = 1;
  uint64_t cols = vbe_mode_info->width / stepX;
  return (cols == 0) ? 1 : cols;
}

int validPosition(uint64_t x, uint64_t y) {
  return x < vbe_mode_info->width && y < vbe_mode_info->height;
}

/* FUNCIONES DE MODO TEXTO. */

void increaseFontSize() {
  cursorErase(); // borrar el '_' con el tamaño viejo antes de cambiarlo
  if (defaultTextSize < MAX_FONT_SIZE) {
    defaultTextSize++;
  }

  updateCursor();
  cursorDraw();
}

void decreaseFontSize() {
  cursorErase();
  if (defaultTextSize > 1) {
    defaultTextSize--;
  }
  updateCursor();
  cursorDraw();
}

// Dibuja un pixel validando límites y BPP
void putPixel(uint32_t hexColor, uint64_t x, uint64_t y) {
  if (!validPosition(x, y)) {
    return;
  }

  uint8_t *framebuffer = (uint8_t *)(uintptr_t)vbe_mode_info->framebuffer;
  uint32_t bpp = vbe_mode_info->bpp;

  uint32_t bytesPerPixel = (bpp + 7) / 8;
  uint64_t offset = y * (uint64_t)vbe_mode_info->pitch + x * bytesPerPixel;

  for (uint32_t i = 0; i < bytesPerPixel; i++) {
    framebuffer[offset + i] = (hexColor >> (8 * i)) & 0xFF;
  }
}

// Desplaza la pantalla una línea hacia arriba
void scroll() {
  uint64_t lineHeight = (defaultTextSize * FONT_HEIGHT) / 4;
  if (lineHeight == 0)
    lineHeight = 1;
  uint8_t *framebuffer = (uint8_t *)(uintptr_t)vbe_mode_info->framebuffer;

  for (uint64_t srcY = lineHeight; srcY < vbe_mode_info->height; srcY++) {
    uint64_t dstY = srcY - lineHeight;
    uint64_t srcOffset = srcY * vbe_mode_info->pitch;
    uint64_t dstOffset = dstY * vbe_mode_info->pitch;
    memcpy(framebuffer + dstOffset, framebuffer + srcOffset,
           vbe_mode_info->pitch);
  }

  uint64_t lastLineStart = vbe_mode_info->height - lineHeight;

  v_fillRectangle(0, lastLineStart, vbe_mode_info->width, vbe_mode_info->height,
                  bgColor);
}

// Salta a la línea siguiente con scroll automático (sin gestionar el cursor)
static void newLineRaw() {
  currentX = 0;

  uint64_t stepY = ((uint64_t)defaultTextSize * FONT_HEIGHT) / 4;
  if (stepY == 0)
    stepY = 1;

  if (currentY + stepY < vbe_mode_info->height) {

    currentY += stepY;
    v_fillRectangle(0, currentY, vbe_mode_info->width, currentY + stepY,
                    bgColor);

  } else {

    scroll();
    currentY = vbe_mode_info->height - stepY;
  }
}

// Salta a la línea siguiente con scroll automático
void newLine() {
  cursorErase();
  newLineRaw();
  cursorDraw();
}

// Imprime un carácter en modo texto (sin gestionar el cursor)
static void putCharRaw(uint8_t c, uint32_t color) {
  if (c == '\n' || c == '\r') {
    newLineRaw();
    return;
  }

  if (c == '\b') {
    uint64_t stepX = ((uint64_t)FONT_WIDTH * defaultTextSize) / 4;
    uint64_t stepY = ((uint64_t)FONT_HEIGHT * defaultTextSize) / 4;
    if (stepX == 0)
      stepX = 1;
    if (stepY == 0)
      stepY = 1;
    if (currentX >= stepX) {
      currentX -= stepX;
      v_fillRectangle(currentX, currentY, currentX + stepX, currentY + stepY,
                      bgColor);
      updateCursor();
    }
    return;
  }

  drawChar((uint32_t)currentX, currentY, c, color, defaultTextSize);
  moveRight();
  updateCursor();
}

// Imprime un carácter en modo gráfico con soporte para \n y \b
void videoPutChar(uint8_t c, uint32_t color) {
  cursorErase();
  putCharRaw(c, color);
  cursorDraw();
}

// Imprime una cadena completa en color
void videoPrint(const char *str, uint32_t color) {
  if (str == 0) {
    return;
  }

  for (unsigned int i = 0; str[i] != '\0'; i++) {
    videoPutChar((uint8_t)str[i], color);
  }
}

// Avanza el cursor horizontalmente con wrap
static void moveRight() {
  uint64_t stepX = ((uint64_t)FONT_WIDTH * defaultTextSize) / 4;
  if (stepX == 0)
    stepX = 1;
  if (currentX + stepX < vbe_mode_info->width) {
    currentX += stepX;
  } else {
    newLineRaw();
  }
}

// Asegura que el cursor esté en una posición válida
static void updateCursor() {
  uint64_t stepX = ((uint64_t)FONT_WIDTH * defaultTextSize) / 4;
  uint64_t stepY = ((uint64_t)FONT_HEIGHT * defaultTextSize) / 4;
  if (stepX == 0)
    stepX = 1;
  if (stepY == 0)
    stepY = 1;
  if (!validPosition(currentX + stepX - 1, currentY + stepY - 1)) {
    newLineRaw();
  }
}

// Borra el '_' pintado en la posición actual, si lo hay
static void cursorErase() {
  if (!cursorDrawn)
    return;
  uint64_t stepX = ((uint64_t)FONT_WIDTH * defaultTextSize) / 4;
  uint64_t stepY = ((uint64_t)FONT_HEIGHT * defaultTextSize) / 4;
  if (stepX == 0)
    stepX = 1;
  if (stepY == 0)
    stepY = 1;
  v_fillRectangle(currentX, currentY, currentX + stepX, currentY + stepY,
                  bgColor);
  cursorDrawn = 0;
}

// Dibuja el '_' en la posición actual sin avanzarla, si el cursor está activo
static void cursorDraw() {
  if (!cursorEnabled || cursorDrawn)
    return;
  drawChar((uint32_t)currentX, currentY, CURSOR_CHAR, CURSOR_COLOR,
           defaultTextSize);
  cursorDrawn = 1;
}

// Muestra (1) u oculta (0) el cursor parpadeante de la consola
void videoCursorSet(int enabled) {
  if (enabled) {
    cursorEnabled = 1;
    cursorDraw();
  } else {
    cursorErase();
    cursorEnabled = 0;
  }
}

// Alterna la visibilidad del cursor; lo llama el timer del kernel
void videoCursorBlink() {
  if (!cursorEnabled)
    return;
  if (cursorDrawn)
    cursorErase();
  else
    cursorDraw();
}

// Imprime una cadena en (x,y) con tamaño escalado
void setTextSize(uint8_t size) {
  if (size == 0) {
    size = 1;
  }

  defaultTextSize = size;
}

/* FUNCIONES DE MODO GRAFICO */

// Rellena un rectángulo [x0,y0) x [x1,y1)
static void v_fillRectangle(uint64_t x0, uint64_t y0, uint64_t x1, uint64_t y1,
                            uint32_t color) {
  if (x1 <= x0 || y1 <= y0) {
    return;
  }

  if (!validPosition(x0, y0)) {
    return;
  }

  if (x1 > vbe_mode_info->width)
    x1 = vbe_mode_info->width;
  if (y1 > vbe_mode_info->height)
    y1 = vbe_mode_info->height;

  uint32_t bytes_per_pixel = (vbe_mode_info->bpp + 7) / 8;
  uint8_t *framebuffer = (uint8_t *)(uintptr_t)vbe_mode_info->framebuffer;
  uint64_t pitch = vbe_mode_info->pitch;

  for (uint64_t y = y0; y < y1; y++) {
    uint64_t row_offset = y * pitch + x0 * bytes_per_pixel;

    for (uint64_t x = x0; x < x1; x++) {
      uint64_t off = row_offset + (x - x0) * bytes_per_pixel;

      for (uint32_t b = 0; b < bytes_per_pixel; b++) {
        framebuffer[off + b] = (color >> (8 * b)) & 0xFF;
      }
    }
  }
}

// Rellena un rectángulo usando ancho/alto
static void drawFilledRect(uint32_t x, uint32_t y, uint32_t width,
                           uint32_t height, uint32_t color) {
  v_fillRectangle(x, y, (uint64_t)x + width, (uint64_t)y + height, color);
}

// Dibuja un carácter usando la bitmap de la fuente
static void drawChar(uint32_t x, uint32_t y, uint8_t c, uint32_t color,
                     uint64_t size) {
  if (c >= 128) {
    return;
  }

  uint64_t targetWidth = (FONT_WIDTH * size) / 4;
  uint64_t targetHeight = (FONT_HEIGHT * size) / 4;
  if (targetWidth == 0)
    targetWidth = 1;
  if (targetHeight == 0)
    targetHeight = 1;

  for (uint64_t dy = 0; dy < targetHeight; dy++) {
    uint64_t srcY = (dy * 4) / size;
    if (srcY >= FONT_HEIGHT)
      srcY = FONT_HEIGHT - 1;

    uint8_t line = font[c][srcY];

    for (uint64_t dx = 0; dx < targetWidth; dx++) {
      uint64_t srcX = (dx * 4) / size;
      if (srcX >= FONT_WIDTH)
        srcX = FONT_WIDTH - 1;

      if ((line << srcX) & 0x80) {
        putPixel(color, x + dx, y + dy);
      }
    }
  }
}

// Limpia toda la pantalla y resetea el cursor
void clearScreen(uint32_t color) {
  drawFilledRect(0, 0, vbe_mode_info->width, vbe_mode_info->height, color);
  currentX = 0;
  currentY = 0;
  cursorDrawn = 0; // el fill ya borro el '_' pintado
  cursorDraw();
}

void fillRect(uint64_t x, uint64_t y, uint64_t w, uint64_t h, uint32_t color) {
  if (w == 0 || h == 0) {
    return;
  }
  v_fillRectangle(x, y, x + w, y + h, color);
}