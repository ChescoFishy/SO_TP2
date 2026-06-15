# Kernel Lib Module

## Overview (¿Qué es?)
El módulo de utilidades de bajo nivel del [[kernel_kernel.md|Kernel]] ([[kernel_lib.md|Kernel Lib]]) provee implementaciones básicas y optimizadas para el copiado de bloques de memoria, el llenado de estructuras y el acceso a los puertos de I/O de la CPU. Dado que el Kernel corre en un entorno "freestanding" sin enlazarse con la biblioteca estándar de C (libc), estas utilidades sirven como las primitivas fundamentales para el resto de los módulos del sistema operativo.

## Functionality (¿Qué hace?)
- Realiza el copiado de bloques de memoria física (`memcpy`) optimizando la transferencia mediante accesos alineados a 32 bits (4 bytes).
- Permite rellenar regiones de memoria con un valor de byte específico (`memset`), utilizado principalmente para limpiar estructuras (BSS, PCBs, Semáforos, Tablas IDT).
- Ofrece envoltorios para la lectura y escritura directa de bytes a puertos de hardware (`inb` y `outb`).

## Internal Mechanics (¿Cómo funciona?)
1. **`memset`:** Rellena de forma iterativa el bloque de memoria de destino byte a byte comenzando por el final de la longitud especificada.
2. **`memcpy` (Copiado Optimizado):**
   - **Camino Optimizado:** Comprueba si la dirección de destino, de origen y la longitud a copiar son múltiplos de `sizeof(uint32_t)` (4 bytes). Si es así, realiza un casteo e itera copiando palabras de 32 bits, reduciendo a la cuarta parte la cantidad de accesos al bus de memoria física.
   - **Camino por Defecto:** Si las direcciones o la longitud no están alineadas, realiza el copiado clásico byte a byte.
3. **Acceso a Puertos (Assembly):** Las funciones `inb` y `outb` se implementan en `libasm.asm` y emplean instrucciones específicas del procesador x86-64:
   - `inb` carga el puerto en `dx` y ejecuta `in al, dx` para leer el byte del puerto hacia `al`.
   - `outb` toma el puerto en `dx`, el byte en `al` (proveniente de `sil`) y ejecuta `out dx, al` para enviar el byte al periférico de hardware (PIT, Parlante, RTC).

## Key Files & Structs (Archivos y Estructuras Clave)
* **Archivos principales:**
  - `Kernel/c/lib/lib.c`: Implementación en C de `memset` y `memcpy`.
  - `Kernel/include/lib/lib.h`: Declaración de las interfaces del módulo y de las primitivas de I/O.
  - `Kernel/asm/libasm.asm` (sección I/O): Implementación ensamblador de `inb` y `outb` (y funciones de lectura del RTC como `getSeconds`, `getHour`, etc.).
  - `Kernel/include/lib/defs.h`: Definiciones de constantes globales de la arquitectura (segmentos, selectores, códigos de interrupción).
* **Estructuras de datos:**
  - No posee estructuras internas complejas; actúa como biblioteca de utilidades puras.
* **Funciones fundamentales:**
  - `memset(void *destination, int32_t c, uint64_t length)`: Llenado de memoria.
  - `memcpy(void *destination, const void *source, uint64_t length)`: Copiado de memoria optimizado por alineación.
  - `inb(uint16_t port)`: Lee un byte del puerto I/O.
  - `outb(uint16_t port, uint8_t value)`: Escribe un byte en el puerto I/O.

## System Calls Relacionadas
No aplica. Estas funciones de utilidad corren en espacio de Kernel y asisten a otros módulos del Kernel; no se exponen de forma directa como llamadas al sistema (aunque la librería estándar de Userland cuenta con implementaciones análogas de `memcpy` y `memset` que corren en el espacio de usuario).

## Comments and Limitations (Comentarios y Limitaciones)
- **Implementación Minimalista:** El módulo carece de funciones clásicas de strings (`strlen`, `strcmp`, `strcpy`) a nivel de `lib.c` (están embebidas localmente dentro de cada módulo que las necesita, como `process.c` o `pipe.c`).
- **Alineación a 32 bits:** El copiado optimizado de `memcpy` está restringido a alineaciones de 32 bits (`uint32_t`). No utiliza instrucciones vectoriales o de 64 bits (`uint64_t` / SSE) para mover memoria, lo que limita levemente el ancho de banda del bus.
