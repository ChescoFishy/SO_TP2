# Kernel Syscalls Module

## Overview (¿Qué es?)
El módulo de Llamadas al Sistema ([[kernel_syscalls.md|Syscalls]]) es la API que el [[kernel_kernel.md|Kernel]] ofrece a los procesos del entorno de usuario ([[userland_shell.md|Userland]]) para interactuar de forma segura con el hardware, solicitar recursos lógicos o solicitar servicios privilegiados. Constituye la frontera lógica y física de protección entre el espacio de usuario (Ring 3) y el espacio de núcleo (Ring 0).

## Functionality (¿Qué hace?)
- Atiende y decodifica las interrupciones de software gatilladas desde Userland a través de la instrucción `int 0x80`.
- Valida que el identificador numérico de la llamada al sistema (pasado en el registro `RAX`) se encuentre dentro de los límites válidos de la tabla.
- Extrae y mapea los parámetros enviados desde el espacio de usuario de forma directa a las firmas de las funciones en C.
- Invoca la función específica en el módulo del Kernel correspondiente (Memoria, Procesos, Drivers, Sonido, IPC).
- Retorna el código de respuesta del Kernel inyectándolo en el contexto del proceso de usuario en RAX.
- Coordina el cambio de contexto voluntario si la syscall requiere bloquear al proceso invocante (e.g. esperas de semáforos, pipes o lecturas de teclado).

## Internal Mechanics (¿Cómo funciona?)
1. **El Handler de Assembly (`_irq128Handler`):** Al ejecutarse `int 0x80`, la CPU guarda en el stack el frame de hardware y entra a `_irq128Handler` en `interrupts.asm`, el cual realiza un `pushState` para resguardar todos los registros.
2. **Despacho y Convención de Llamada:** 
   - El handler compara `RAX` con el límite superior de syscalls registradas (`CANT_SYS = 39`). Si `RAX >= 39`, escribe `-1` en el slot RAX del stack y sale.
   - Si es válido, ejecuta `call [syscalls + rax * 8]`. Como la arquitectura x86-64 utiliza la especificación System V ABI, los primeros seis parámetros pasados por el usuario ya se ubican en los registros: `RDI`, `RSI`, `RDX`, `RCX`, `R8`, y `R9`. El compilador de C mapea de forma transparente estos registros a los argumentos de la función de C invocada.
3. **Preservación del Valor de Retorno:** El valor devuelto por la función de C queda en `RAX`. Para evitar que el `popState` posterior de assembly lo pise con el valor original de RAX, la rutina de assembly copia `RAX` directamente en el slot de RAX de la pila guardada:
   `mov [rsp + 14 * 8], rax`
4. **Tratamiento de Preemoción Voluntaria:** Tras procesar la syscall, assembly verifica si la variable `force_switch` fue seteada en 1 (por llamadas como `sys_exit`, `sys_waitpid`, `sys_block`, o si un pipe/teclado bloqueó al proceso). Si es así, limpia `force_switch = 0`, invoca a `scheduler_yield_impl(rsp)` para re-planificar el sistema, actualiza `RSP` con el puntero al stack del nuevo proceso, ejecuta `popState` y hace `iretq` para saltar a Userland en el contexto del nuevo proceso.

## Key Files & Structs (Archivos y Estructuras Clave)
* **Archivos principales:**
  - `Kernel/c/syscalls/syscallDispatcher.c`: Implementación de los despachadores específicos y definición de la tabla global de punteros.
  - `Kernel/include/syscalls/syscallDispatcher.h`: Mapeo de prototipos de syscalls y definición de `CANT_SYS`.
  - `Kernel/asm/interrupts.asm` (sección `_irq128Handler`): Rutina de bajo nivel encargada de interceptar `int 0x80`, despachar la llamada y retornar el resultado en la pila.
* **Estructuras de datos:**
  - `syscalls[]` (en `syscallDispatcher.c`): Tabla estática de `CANT_SYS` punteros a función de tipo `void*` indexada por el número de syscall.
* **Catálogo Completo de Syscalls (0-38):**
  - **0: `sys_registers`:** Copia el volcado de registros tomado al presionar `L_CONTROL`.
  - **1: `sys_time` / 2: `sys_date`:** Devuelven la hora y fecha actual del RTC en BCD.
  - **3: `sys_read`:** Lee desde el teclado o un pipe. Devuelve `READ_RETRY` (-2) si bloquea.
  - **4: `sys_write` / 37: `sys_write_color`:** Escribe texto en la consola gráfica o en un pipe (con o sin color).
  - **5: `sys_increase_fontsize` / 6: `sys_decrease_fontsize`:** Ajusta el tamaño de la letra de la consola.
  - **7: `sys_beep`:** Genera un pitido monofónico bloqueante.
  - **8: `sys_ticks`:** Retorna la cantidad de ticks acumulados del timer.
  - **9: `sys_clear`:** Limpia la pantalla gráfica.
  - **10: `sys_speaker_start` / 11: `sys_speaker_off`:** Control asíncrono del parlante.
  - **12: `sys_screen_width` / 13: `sys_screen_height`:** Devuelve dimensiones de la pantalla.
  - **14: `sys_putpixel` / 15: `sys_fill_rect`:** Dibujo de píxeles y rectángulos en el framebuffer.
  - **16: `sys_malloc` / 17: `sys_free`:** Interfaz de alocación de memoria dinámica de usuario.
  - **18: `sys_mem_status`:** Consulta de totales de memoria y de asignaciones.
  - **19: `sys_create_process` / 35: `sys_create_process_fd`:** Creación de procesos (básicos o con redirecciones de I/O).
  - **20: `sys_exit`:** Termina el proceso actual con código de retorno.
  - **21: `sys_getpid`:** Retorna el PID del proceso invocante.
  - **22: `sys_ps`:** Obtiene listado descriptivo de procesos activos en la tabla de PCBs.
  - **23: `sys_kill`:** Fuerza la terminación de un proceso por PID.
  - **24: `sys_nice`:** Actualiza la prioridad de ejecución de un proceso (1 a 5).
  - **25: `sys_block` / 26: `sys_unblock`:** Bloquea o desbloquea procesos.
  - **27: `sys_yield`:** Cede voluntariamente el procesador.
  - **28: `sys_waitpid`:** Bloquea al padre hasta la finalización de un proceso hijo.
  - **29: `sys_sem_open` / 32: `sys_sem_close`:** Creación/apertura y cierre de semáforos nombrados.
  - **30: `sys_sem_wait` / 31: `sys_sem_post`:** Operaciones de sincronización sobre semáforos.
  - **33: `sys_pipe` / 34: `sys_pipe_close`:** Creación y destrucción de pipes anónimos.
  - **36: `sys_pipe_open`:** Creación o apertura de pipes nombrados compartidos.
  - **38: `sys_set_cursor`:** Controla la visibilidad del cursor parpadeante en consola.

## System Calls Relacionadas
Este módulo es el despachador que encapsula la totalidad de las syscalls del sistema.

## Comments and Limitations (Comentarios y Limitaciones)
- **Límite Rígido:** La tabla está acotada a `CANT_SYS = 39`. El agregado de llamadas requiere extender este límite, registrar el puntero en `syscallDispatcher.c`, y declarar el prototipo coincidente tanto en el header del Kernel como en la librería de Userland (`userlib.asm` y `userlib.h`).
- **Compuertas de Interrupción (IF=0):** La IDT configura la syscall como una "Interrupt Gate", lo que apaga automáticamente el flag de interrupción (`IF`) al ingresar al handler. Esto garantiza que la ejecución de la syscall en el Kernel sea atómica y no sea interrumpida por el planificador, previniendo condiciones de carrera en secciones críticas.
