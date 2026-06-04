# Kernel Syscalls Module

## Overview (¿Qué es?)
El módulo de Llamadas al Sistema ([[kernel_syscalls.md|Syscalls]]) es la API que el [[kernel_kernel.md|Kernel]] ofrece a los procesos del entorno de usuario ([[userland_shell.md|Userland]]) para interactuar con hardware, solicitar recursos o llamar a servicios privilegiados. Es la frontera de seguridad entre Ring 3 y Ring 0.

## Functionality (¿Qué hace?)
- Atiende las interrupciones de software provenientes de [[userland_shell.md|Userland]] (típicamente a través de `int 80h`).
- Identifica la operación solicitada mediante un número de Syscall (registrado en RAX).
- Recupera los argumentos pasados por el usuario desde los registros.
- Despacha la solicitud al módulo del [[kernel_kernel.md|Kernel]] apropiado (Ej. a [[kernel_drivers.md|Drivers]] para sys_read, a [[kernel_console.md|Console]] para sys_write, a [[kernel_memory_manager.md|Memory Manager]] para sys_alloc).
- Retorna el resultado o código de error al proceso llamador.

## Internal Mechanics (¿Cómo funciona?)
1. **Interrupción:** Cuando [[userland_shell.md|Userland]] ejecuta `int 80h`, se lanza una excepción de software manejada por la IDT.
2. **Dispatcher:** La ejecución entra en el handler de assembly `_syscallHandler`, que extrae los parámetros (guardados en RDI, RSI, RDX, R10, R8, R9) e invoca a `syscallDispatcher` en C.
3. **Mapeo:** El Dispatcher usa un switch-case o un arreglo de punteros a función indexado por el número de syscall.
4. **Retorno:** El valor de retorno de la función específica es puesto en RAX antes de ejecutar `iretq`, y [[userland_shell.md|Userland]] recibe este resultado de inmediato al recuperar el control.

## Comments and Limitations
- **Limitaciones actuales:** Las [[kernel_syscalls.md|Syscalls]] son sincrónicas, el proceso llamador bloquea su flujo hasta que el [[kernel_kernel.md|Kernel]] responda. La convención de paso de parámetros usada debe respetarse estrictamente en [[userland_shell.md|Userland]].
- **Comentarios:** Añadir una nueva syscall implica definir su ID, mapearla en el dispatcher del [[kernel_kernel.md|Kernel]] y crear el correspondiente wrapper en la librería de [[userland_shell.md|Userland]].
