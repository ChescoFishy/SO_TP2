# Kernel Lib Module

## Overview (¿Qué es?)
El módulo de utilidades del [[kernel_kernel.md|Kernel]] ([[kernel_kernel.md|Kernel]] Lib) contiene implementaciones de funciones básicas de manipulación de cadenas, manejo de bloques de memoria e impresión formateada (como printf básico) necesarias dentro del espacio de [[kernel_kernel.md|Kernel]], ya que en este nivel no se cuenta con la librería estándar de C (libc).

## Functionality (¿Qué hace?)
- Provee manipulación de cadenas: `strlen`, `strcpy`, `strcmp`.
- Provee copiado y modificación de bloques de memoria: `memcpy`, `memset`.
- Puede incluir funciones para convertir enteros a strings y viceversa (ej. `itoa`, `atoi`).
- Auxilia a otros módulos del [[kernel_kernel.md|Kernel]] para no tener que reescribir estas funciones elementales.

## Internal Mechanics (¿Cómo funciona?)
1. **C:** La mayoría de las funciones de strings se implementan de forma tradicional iterando sobre el arreglo de caracteres hasta encontrar el terminador `\0`.
2. **Assembly Optimizado:** `memcpy` y `memset` frecuentemente poseen implementaciones híbridas o totalmente en Assembly utilizando instrucciones rep (como `rep movsb` o `rep stosb`) para maximizar el rendimiento al mover grandes volúmenes de datos.

## Comments and Limitations
- **Limitaciones actuales:** Conjunto de funciones limitadas al mínimo necesario. No implementa la totalidad de `string.h` o `stdlib.h` de POSIX.
- **Comentarios:** Es un módulo independiente que no interactúa con hardware o subsistemas lógicos, lo cual lo hace fácil de probar (unit testing).
