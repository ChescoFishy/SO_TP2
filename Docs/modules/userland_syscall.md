# Userland Syscall Wrapper Module

## Overview (¿Qué es?)
Este módulo contiene las funciones escritas en Assembly que los programas de usuario invocan para desencadenar una llamada al sistema. Son los "wrappers" (envoltorios) del lado de [[userland_shell.md|Userland]] para la instrucción `int 80h`.

## Functionality (¿Qué hace?)
- Proporciona firmas de funciones en C (ej. `int sys_read(int fd, char* buffer, int count);`) implementadas en Assembly.
- Carga los registros correctos con los parámetros y el ID de la Syscall, según lo esperado por el [[kernel_kernel.md|Kernel]].
- Desencadena la interrupción por software (`int 80h`).
- Extrae el valor de retorno entregado por el [[kernel_kernel.md|Kernel]] (típicamente en `RAX`) y lo retorna al programa llamador en C.

## Internal Mechanics (¿Cómo funciona?)
1. **Llamada de usuario:** Un programa C llama, por ejemplo, a `sys_read(0, buf, 100)`.
2. **Setup Registers (ASM):** En el wrapper de asm, siguiendo la convención de llamadas de AMD64 y la convención propia del OS, coloca el valor 0 (ID de sys_read) en `RAX`, el parámetro 1 (0) en `RDI`, el parámetro 2 (`buf`) en `RSI`, y el parámetro 3 (100) en `RDX`.
3. **Interrupt:** Ejecuta `int 80h`. La CPU pausa el entorno de [[userland_shell.md|Userland]], salta al [[kernel_kernel.md|Kernel]] (Ring 0) y despacha la orden.
4. **Respuesta:** Al volver el [[kernel_kernel.md|Kernel]] a Ring 3, `RAX` contiene el resultado. El wrapper en ASM simplemente retorna (`ret`), dejando intacto el valor en `RAX`, por lo que el programa en C lo recibe como retorno de la función.

## Comments and Limitations
- **Limitaciones actuales:** La cantidad de argumentos que se pueden pasar está limitada estrictamente por los registros acordados a usar (generalmente hasta 6: RDI, RSI, RDX, R10, R8, R9).
- **Comentarios:** El mapeo de números/IDs de [[kernel_syscalls.md|Syscalls]] debe coincidir 1 a 1 entre lo definido en este módulo de [[userland_shell.md|Userland]] y el Dispatcher de [[kernel_syscalls.md|Syscalls]] en el módulo del [[kernel_kernel.md|Kernel]].
