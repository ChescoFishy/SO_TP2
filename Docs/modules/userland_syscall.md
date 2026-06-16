# Userland Syscall Wrapper Module

## Overview (¿Qué es?)
El módulo de Envoltorios de Llamadas al Sistema de Usuario ([[userland_syscall.md|Userland Syscall]]) es la capa de ensamblador de bajo nivel en el espacio de usuario (Ring 3) que actúa como puente de comunicación con el Kernel (Ring 0). Contiene las funciones en assembly que cargan los identificadores de llamada e invocan a la interrupción de software `int 0x80`.

## Functionality (¿Qué hace?)
- Mapea las firmas de funciones de C (`sys_*`) a rutinas de ensamblador en el archivo de biblioteca `userlib.asm`.
- Carga el registro `RAX` con el identificador numérico exacto de la syscall solicitada.
- Ejecuta la interrupción de software `int 0x80` para cambiar el modo de privilegio de la CPU a Ring 0 y saltar al handler del Kernel.
- Retorna el valor devuelto por el Kernel (ubicado en `RAX` tras la interrupción) de forma transparente a las aplicaciones llamadoras en C.

## Internal Mechanics (¿Cómo funciona?)
1. **Paso de Parámetros (System V ABI):** Cuando un programa en C de Userland realiza una llamada como `sys_write(fd, buff, count)`:
   - El compilador de C (GCC) coloca automáticamente el primer parámetro (`fd`) en `RDI`, el segundo (`buff`) en `RSI`, y el tercero (`count`) en `RDX`, siguiendo la especificación estándar ABI para x86-64.
2. **Ejecución del Wrapper Assembly:** La función en ensamblador se implementa de la siguiente manera:
   ```nasm
   sys_write:
       mov rax, 4      ; Carga el ID de la syscall (4 = sys_write) en RAX
       int 0x80        ; Dispara la interrupción de software
       ret             ; Retorna al programa invocante
   ```
   Debido a que la convención de llamadas del Kernel coincide con la de usuario (ambas utilizan System V ABI), la rutina de assembly **no necesita mover los registros de parámetros** (`RDI`, `RSI`, `RDX`, etc.). Solo debe asignar el ID en `RAX` y gatillar la interrupción.
3. **Retorno de Datos:** Tras procesar la llamada en el Kernel, el resultado final se almacena en el slot RAX del stack frame guardado. Al ejecutarse la restauración por `iretq`, el registro físico `RAX` del procesador contendrá el valor devuelto. Al retornar de la llamada de ensamblador mediante `ret`, la función de C invocante lee de inmediato el valor de `RAX` como el valor de retorno nativo.

## Key Files & Structs (Archivos y Estructuras Clave)
* **Archivos principales:**
  - `Userland/asm/userlib.asm`: Archivo ensamblador que contiene las implementaciones de bajo nivel de las 39 syscalls disponibles.
  - `Userland/include/lib/userlib.h`: Declaración de los prototipos en C de las llamadas `sys_*` consumidas por Userland.
  - `Userland/c/syscall/syscall.c`: Implementa los wrappers extendidos `my_*` que interactúan con el registro de funciones del usuario y simplifican el pasaje de PIDs.
* **Estructuras de datos:**
  - No posee estructuras internas; es una capa puramente funcional y de traducción en lenguaje ensamblador NASM.

## Catálogo de Identificadores de Syscalls (0-38)
Los siguientes números deben cargarse en `RAX` y coincidir exactamente con el dispatcher del Kernel:
- `0`: `sys_registers`
- `1`: `sys_time`
- `2`: `sys_date`
- `3`: `sys_read`
- `4`: `sys_write`
- `5`: `sys_increase_fontsize`
- `6`: `sys_decrease_fontsize`
- `7`: `sys_beep`
- `8`: `sys_ticks`
- `9`: `sys_clear`
- `10`: `sys_speaker_start`
- `11`: `sys_speaker_off`
- `12`: `sys_screen_width`
- `13`: `sys_screen_height`
- `14`: `sys_putpixel`
- `15`: `sys_fill_rect`
- `16`: `sys_malloc`
- `17`: `sys_free`
- `18`: `sys_mem_status`
- `19`: `sys_create_process`
- `20`: `sys_exit`
- `21`: `sys_getpid`
- `22`: `sys_ps`
- `23`: `sys_kill`
- `24`: `sys_nice`
- `25`: `sys_block`
- `26`: `sys_unblock`
- `27`: `sys_yield`
- `28`: `sys_waitpid`
- `29`: `sys_sem_open`
- `30`: `sys_sem_wait`
- `31`: `sys_sem_post`
- `32`: `sys_sem_close`
- `33`: `sys_pipe`
- `34`: `sys_pipe_close`
- `35`: `sys_create_process_fd`
- `36`: `sys_pipe_open`
- `37`: `sys_write_color`
- `38`: `sys_set_cursor`

## Comments and Limitations (Comentarios y Limitaciones)
- **Consistencia Crítica:** Cualquier alteración del orden de la tabla `syscalls` en `syscallDispatcher.c` requiere modificar de forma simétrica los valores de `RAX` asignados en `userlib.asm`. De lo contrario, las llamadas al sistema se derivarán a rutinas erróneas, provocando fallos generales.
- **Paso de Fds Compactados:** La llamada `sys_create_process_fd` (ID 35) empaqueta los parámetros `fg`, `fd_in` y `fd_out` en un único entero de 64 bits para cumplir con el límite máximo de parámetros de la llamada en ensamblador.
