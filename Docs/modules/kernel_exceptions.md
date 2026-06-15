# Kernel Exceptions Module

## Overview (¿Qué es?)
El módulo de Excepciones del [[kernel_kernel.md|Kernel]] es el encargado de detectar, atrapar y procesar situaciones anómalas de ejecución a nivel de CPU. En lugar de permitir un reinicio silencioso o un fallo catastrófico del hardware, este módulo intercepta el flujo de ejecución, muestra un informe de diagnóstico completo y restaura el control de forma segura devolviendo al usuario a la consola de [[userland_shell.md|Userland]].

## Functionality (¿Qué hace?)
- Intercepta las excepciones críticas de la CPU configuradas en la IDT.
- Captura de forma exacta el valor de los 20 registros del procesador en el instante del fallo.
- Muestra en pantalla un informe detallado con el nombre de la excepción y el volcado de registros (Register Dump) serializado en hexadecimal.
- Habilita las interrupciones locales y pausa la ejecución de forma interactiva esperando que el usuario presione la tecla `ENTER` para confirmar lectura.
- Restaura la ejecución del sistema operativo limpiando el stack de ejecución y reiniciando el punto de entrada de [[userland_shell.md|Userland]] (`0x400000`).

## Internal Mechanics (¿Cómo funciona?)
1. **Configuración en la IDT:** El cargador de la IDT (`idtLoader.c`) registra los puntos de entrada físicos para las excepciones soportadas:
   - **Excepción 0x00 (Divide Error - #DE):** Apunta a `_exception0Handler`.
   - **Excepción 0x06 (Invalid Opcode - #UD):** Apunta a `_exception6Handler`.
2. **Captura del Contexto en Assembly:** Al ocurrir la excepción, la CPU realiza una llamada a la rutina correspondiente. En `interrupts.asm`, la macro `exceptionHandler` realiza un `pushState` para apilar los 15 registros de uso general. Seguidamente, lee el stack frame de hardware (donde la CPU apiló automáticamente `RIP`, `CS` y `RFLAGS`) y los copia junto con el resto en el arreglo estático global `regsArray` de 20 posiciones de 64 bits.
3. **Dispatch a C:** La macro invoca a `exceptionDispatcher` en `exceptions.c` pasando el número de excepción y el puntero al stack actual. El dispatcher llama a la función correspondiente (`zeroDivision()` o `invalidOpcode()`).
4. **Espera Interactiva:** `exceptionHandler()` en C imprime por pantalla el error y el volcado de registros formateado. Luego, ejecuta la instrucción de habilitación de interrupciones (`_sti()`) y entra en un bucle pasivo `_hlt()` esperando a que el driver de teclado registre la tecla `\n` (ENTER) mediante `getFromBuffer()`.
5. **Recuperación y Salto a Userland:** Al retornar a Assembly tras el dispatcher:
   - Se restaura el estado con `popState`.
   - Se invoca a `getStackBase()` para obtener la base limpia del stack del sistema.
   - Se sobrescribe la dirección de retorno en el stack frame de la CPU con la constante `userland` (que equivale al punto de entrada de la shell en `0x400000`).
   - Se ejecuta `iretq`, lo que provoca que el procesador salte a re-ejecutar la Shell de usuario con un stack limpio, impidiendo que el sistema colapse.

## Key Files & Structs (Archivos y Estructuras Clave)
* **Archivos principales:**
  - `Kernel/c/exceptions/exceptions.c`: Implementación de los handlers de excepción y la rutina interactiva de impresión y espera de teclado.
  - `Kernel/include/exceptions/exceptions.h`: Declaración de interfaces de excepciones.
  - `Kernel/asm/interrupts.asm` (sección de excepciones): Implementación de la macro `exceptionHandler`, guardado de registros en `regsArray`, y salto de re-inicialización con `iretq`.
* **Estructuras de datos:**
  - `regsArray` (en `interrupts.asm` / `keyboardDriver.h`): Buffer global de 20 palabras de 64 bits que guarda el contexto completo de la CPU.
  - `exceptionsArray[]` (en `exceptions.c`): Tabla estática indexada que mapea el ID de la excepción con su correspondiente función de tratamiento.
* **Funciones fundamentales:**
  - `exceptionDispatcher(int exception)`: Deriva el procesamiento al handler adecuado en base a la excepción capturada.
  - `exceptionHandler(char *msg)`: Imprime el diagnóstico y ejecuta la espera activa no-bloqueante del teclado hasta leer un retorno de carro.

## System Calls Relacionadas
No aplica. Las excepciones de hardware son eventos síncronos desencadenados por el procesador ante errores de ejecución. No se interactúa con ellas a través de syscalls. Sin embargo, para simular fallos desde Userland, se puede utilizar la syscall `gen_invalid_opcode` que ejecuta la instrucción x86-64 `ud2`.

## Comments and Limitations (Comentarios y Limitaciones)
- **Excepciones Soportadas:** El sistema únicamente tiene registradas y maneja las excepciones `#DE` (división por cero) y `#UD` (código de instrucción no definido/inválido). Cualquier otra excepción no registrada en la IDT (como `#PF` - Page Fault o `#GP` - General Protection Fault) provocará un comportamiento indefinido o un *triple fault* del hardware que reiniciará físicamente la máquina.
- **Reiniciación Global de Userland:** El mecanismo de recuperación descarta el contexto completo del proceso que falló y **reinicia la shell completa** (`0x400000`) de forma global. Esto significa que si un proceso en background falla, no solo muere ese proceso, sino que se aborta y reinicia la consola interactiva actual del usuario.
