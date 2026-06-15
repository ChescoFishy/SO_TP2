# Userland Lib Module

## Overview (¿Qué es?)
El módulo de la Librería de Usuario ([[userland_lib.md|Userland Lib]]) es el equivalente a una biblioteca estándar de C (libc) adaptada para nuestro sistema operativo. Corre enteramente en espacio de usuario (Ring 3) y encapsula la complejidad de las llamadas al sistema, proveyendo abstracciones de alto nivel para entrada/salida formateada, manipulación de cadenas de caracteres, conversión de datos y la administración de un búfer de redibujado de pantalla.

## Functionality (¿Qué hace?)
- **Abstracción de Entrada/Salida:** Proporciona funciones de consola de alto nivel como `putchar(c)` y `getchar()`.
- **Salida Formateada:** Implementa `print(fmt, ...)` (equivalente a `printf`) que soporta especificadores de formato `%d`, `%u`, `%x`, `%s`, `%c` y `%%`.
- **Manejo de Cadenas:** Provee implementaciones nativas en espacio de usuario de `strlen`, `strcmp`, `strcpy`, `strcat`, entre otras.
- **Lectura Adaptativa de Dispositivos/Pipes:** Implementa `read_full` que maneja el reintento automático ante bloqueos de lectura de teclado.
- **Búfer de Redibujado de Pantalla (Redraw Buffer):** Almacena los últimos caracteres impresos para poder reconstruir y volver a renderizar la salida de consola al alterar dinámicamente el tamaño de la fuente.
- **Gestión de Memoria Dinámica:** Redirecciona solicitudes de memoria (`malloc` y `free`) a las syscalls correspondientes del Kernel.
- **Parsing de Argumentos:** Proporciona utilidades para convertir y saltear tokens numéricos (`next_uint`) y obtener argumentos de línea de comandos (`cmd_args`).

## Internal Mechanics (¿Cómo funciona?)
1. **Lógica de Entrada No Bloqueante a Nivel de Syscall:** A bajo nivel, la syscall `sys_read` sobre el teclado retorna `READ_RETRY` (-2) si no hay teclas disponibles en el buffer del Kernel (colocando al proceso a dormir). La librería de usuario abstrae esto en `getchar()` y `read_full()` mediante un ciclo ocupado:
   `while((r = sys_read(&c, 1)) == READ_RETRY);`
   Esto bloquea de forma limpia la ejecución del proceso de usuario hasta que el driver de teclado despierte al lector y retorne un byte válido o `0` (en caso de detectar Ctrl+D/EOF). Para los pipes, `sys_read` no retorna `READ_RETRY` ya que bloquea de forma interna en el Kernel usando semáforos.
2. **Formateo de Salida (`print`):** Recibe un número indeterminado de argumentos mediante macros variádicas (`va_list`, `va_start`, `va_end`). Lee la cadena de formato carácter por carácter; al detectar `%` deriva la conversión del argumento leído:
   - Los enteros decimales o hexadecimales se formatean usando la función `num_to_str` y luego se imprimen por pantalla llamando de forma secuencial a `putchar`.
3. **El Búfer de Redibujado (Redraw):**
   - Cada llamada a `shellPutchar` o `putchar` guarda el carácter impreso y su correspondiente descriptor en el arreglo circular `redrawBuffer` de tamaño `REDRAW_BUFF = 4096`. Si el buffer se llena, se descartan los caracteres más antiguos haciendo un corrimiento a la izquierda.
   - Cuando el usuario presiona `+` o `-` en la shell, esta invoca a `sys_increase_fontsize` o `sys_decrease_fontsize` en el Kernel, y luego ejecuta `redrawFont()`.
   - `redrawFont()` limpia la pantalla mediante `sys_clear()` y vuelve a escribir en lote toda la cadena de caracteres almacenada en el buffer. Dado que el Kernel actualizó el tamaño de letra del renderizador de VBE, la salida se dibuja de forma automática con la nueva escala.

## Key Files & Structs (Archivos y Estructuras Clave)
* **Archivos principales:**
  - `Userland/c/lib/io.c`: Implementación de `putchar`, `getchar`, `read_full`, `next_uint` e impresión formateada con variables variádicas.
  - `Userland/include/lib/io.h`: Definición de prototipos de E/S.
  - `Userland/c/lib/redraw.c`: Manejo del búfer para fuentes dinámicas, guardado y redibujado de la consola.
  - `Userland/include/lib/redraw.h`: Interfaz del búfer de redibujado.
  - `Userland/c/lib/strings.c` / `format.c`: Lógica de strings y conversión numérica.
  - `Userland/include/lib/userlib.h`: Cabecera unificada (fachada) para todas las utilidades de usuario.
* **Estructuras de datos:**
  - `RedrawStruct`:
    ```c
    typedef struct {
        char character;     /* Carácter impreso */
        uint64_t fd;        /* Descriptor de archivo de destino (STDOUT/STDERR) */
    } RedrawStruct;
    ```
* **Funciones fundamentales:**
  - `print(const char *fmt, ...)`: Escribe salida estructurada en la terminal.
  - `redrawFont()`: Re-dibuja el búfer de salida luego de un cambio de escala tipográfica.

## System Calls Relacionadas
El módulo de librería no expone syscalls; las consume. Traduce llamadas locales a invocaciones de `sys_write` (para `putchar`/`print`), `sys_read` (para `getchar`), `sys_malloc`/`sys_free` (para `malloc`/`free`), y `sys_clear` / `sys_increase_fontsize` / `sys_decrease_fontsize` (para el redibujado).

## Comments and Limitations (Comentarios y Limitaciones)
- **Límite del Búfer de Redibujado:** El búfer tiene un tamaño estático rígido de `REDRAW_BUFF = 4096` elementos. Si un programa imprime un volumen de salida superior a 4 KB, las primeras líneas de la consola se perderán y no serán redibujadas tras un cambio de escala.
- **Implementación Reducida de printf:** La función `print` soporta un subconjunto simplificado de formateadores y carece de opciones de justificación, rellenos de ceros o coma flotante.
