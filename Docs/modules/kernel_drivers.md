# Kernel Drivers Module

## Overview (¿Qué es?)
El módulo de [[kernel_drivers.md|Drivers]] es la capa de abstracción del sistema operativo que permite la comunicación directa con los dispositivos periféricos de hardware. En nuestra implementación, se enfoca en dos controladores esenciales: el controlador de teclado (Keyboard Driver) y el controlador de pantalla gráfico (VBE Video Driver).

## Functionality (¿Qué hace?)
### 1. Controlador de Teclado
- Captura interrupciones físicas de teclado (IRQ 1) y lee scancodes crudos desde el puerto `0x60` usando la instrucción `kbd_scancode_read`.
- Traduce los scancodes a caracteres ASCII utilizando mapas de traducción física para configuraciones estándar de teclado (US layout).
- Gestiona el estado de los modificadores especiales: Shift (izquierdo y derecho), Caps Lock y Control (izquierdo).
- Implementa un buffer circular para encolar teclas presionadas hasta que sean consumidas.
- Procesa atajos globales del sistema:
  - `Ctrl + C`: Finaliza forzadamente al proceso en primer plano (`foreground`).
  - `Ctrl + D`: Señaliza el fin de archivo (EOF) a los procesos de lectura.
- Toma un snapshot instantáneo de los 20 registros del procesador cuando se presiona la tecla `Control` (izquierda).
- Gestiona el bloqueo de procesos de lectura (`sys_read`) que quedan esperando la llegada de datos del teclado.

### 2. Controlador de Video
- Obtiene la configuración de video (resolución, framebuffer, pitch, bits por píxel) mediante la estructura VBE mapeada en la dirección `0x5C00`.
- Ofrece primitivas para dibujar píxeles individuales (`putPixel`) y rellenar rectángulos completos (`fillRect`).
- Implementa el renderizado de caracteres utilizando fuentes bitmap de 8x16 píxeles (`font.h`), con soporte para escalado dinámico (tamaños de 1 a 5).
- Administra el scrolling de pantalla desplazando la memoria de video por software mediante `memcpy`.

## Internal Mechanics (¿Cómo funciona?)
### Teclado
1. **Buffer Circular:** Se define un buffer `buff[BUFF_LENGTH]` (donde `BUFF_LENGTH = 256`) indexado por `start_index` y `end_index`.
2. **Tratamiento de Modificadores y Combos:** Al recibir una tecla en `handlePressedKey()`:
   - Si se presiona `Control` (`L_CONTROL`), se invoca `storeSnapshot()` que lee el arreglo global `regsArray` (cargado previamente por la rutina de interrupción de teclado `_irq01Handler` en `interrupts.asm`) y serializa los nombres y valores de los registros en hexadecimal dentro de `registersBuff`.
   - Si `ctrl` está activo y se presiona `C`, se obtiene el PID del proceso foreground (`process_get_foreground()`) y se lo termina con `process_kill()`.
   - Si `ctrl` está activo y se presiona `D`, se marca `eof_pending = 1` y se despierta al proceso bloqueado.
3. **Bloqueo/Desbloqueo de Lectores:** Si `sys_read` no encuentra datos en el buffer circular y no hay un EOF pendiente, guarda el puntero al proceso actual en `kbd_waiting_process`, cambia el estado del proceso a `PROCESS_BLOCKED` y setea `force_switch = 1`. Cuando se presiona una tecla o se ingresa `Ctrl+D`, la función `kbd_wake_waiting()` cambia el estado del proceso en `kbd_waiting_process` a `PROCESS_READY` para que el scheduler lo planifique nuevamente.

### Video
1. **Direccionamiento de Píxeles:** La función `putPixel` calcula la dirección física del pixel `(x, y)` como:
   $$\text{Offset} = (y \times \text{pitch}) + (x \times \text{bytesPerPixel})$$
   donde `pitch` y `bytesPerPixel` se extraen de la estructura VBE en `0x5C00`. El color se escribe directamente byte a byte en esa dirección física.
2. **Scroll por Software:** La función `scroll()` desplaza la memoria del framebuffer desde la línea `lineHeight` hasta la parte superior del framebuffer usando `memcpy` con el tamaño de línea `pitch`, y luego blanquea la última línea restante con el color de fondo.

## Key Files & Structs (Archivos y Estructuras Clave)
* **Archivos principales:**
  - `Kernel/c/drivers/keyboardDriver.c`: Implementación del buffer, traducción de scancodes, procesamiento de combos y control de esperas de procesos.
  - `Kernel/include/drivers/keyboardDriver.h`: Interfaz pública del driver de teclado.
  - `Kernel/c/drivers/videoDriver.c`: Control de modo gráfico, dibujo de píxeles, rectángulos, scroll y escala tipográfica.
  - `Kernel/include/drivers/videoDriver.h`: Interfaz pública del driver de video.
* **Estructuras de datos:**
  - `struct vbe_mode_info_structure` (en `videoDriver.c`): Estructura empacada que describe las propiedades físicas del controlador gráfico en `0x5C00`.
  - `regsArray` (en `interrupts.asm` / `keyboardDriver.h`): Buffer de 20 enteros de 64 bits donde la ISR guarda el estado de los registros de la CPU en el momento del tick del teclado.
* **Funciones fundamentales:**
  - `handlePressedKey()`: Se ejecuta como respuesta a la interrupción de teclado.
  - `readKeyBuff(char *buff, uint64_t count)`: Copia caracteres del buffer circular de teclado a un buffer intermedio.
  - `putPixel(uint32_t hexColor, uint64_t x, uint64_t y)`: Dibuja un píxel en las coordenadas dadas del framebuffer.
  - `scroll()`: Desplaza la memoria de video verticalmente.

## System Calls Relacionadas
- **`sys_read` (Syscall 3):** Lee datos del teclado; si está vacío, bloquea al proceso y retorna `READ_RETRY` (-2) al reanudarse.
- **`sys_write` (Syscall 4) / `sys_write_color` (Syscall 37):** Utilizan la salida de consola escribiendo caracteres mediante el driver de video.
- **`sys_putpixel` (Syscall 14):** Dibuja un píxel en pantalla.
- **`sys_fill_rect` (Syscall 15):** Rellena un rectángulo.
- **`sys_screen_width` (Syscall 12) / `sys_screen_height` (Syscall 13):** Devuelven el ancho y alto físico en píxeles.
- **`sys_registers` (Syscall 0):** Copia el buffer serializado de registros (`registersBuff`) tomado en el último press de `L_CONTROL`.

## Comments and Limitations (Comentarios y Limitaciones)
- **Diseño de Un Solo Buffer:** El buffer circular de teclado es global y único. Si múltiples procesos leen simultáneamente de la entrada estándar sin sincronización, sus lecturas se intercalarán.
- **Sin Doble Buffer de Pantalla:** Las escrituras de píxeles se realizan directamente sobre el framebuffer físico de video. Esto puede producir parpadeo (screen tearing) si se realizan dibujos muy seguidos por pantalla.
- **Distribución de Teclado Estática:** Solo soporta la distribución de teclado de Estados Unidos (US Layout) codificada estáticamente en los arreglos del driver.
