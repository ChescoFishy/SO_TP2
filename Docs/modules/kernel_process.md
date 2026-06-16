# Kernel Process Module (Scheduler)

## Overview (¿Qué es?)
El módulo de Procesos y Planificación ([[kernel_process.md|Scheduler]]) es el subsistema encargado de proveer soporte para multitarea preemptiva en el sistema operativo. Administra el concepto de Proceso mediante el Bloque de Control de Proceso (PCB), gestiona las colas de ejecución y realiza los cambios de contexto (Context Switching) a bajo nivel, implementando un algoritmo de **Round Robin con Prioridades**.

## Functionality (¿Qué hace?)
- Permite la creación dinámica de procesos en espacio de usuario (`process_create` / `process_create_fd`), asignando memoria para su stack e inicializando su contexto de registros.
- Empaqueta y transfiere argumentos al nuevo contexto del proceso de forma análoga a `argc` y `argv`.
- Mantiene el ciclo de vida y los estados lógicos de los procesos: `FREE`, `READY`, `RUNNING`, `BLOCKED`, `ZOMBIE`.
- Soporta prioridades dinámicas del 1 (mínima) al 5 (máxima) para definir la duración del quantum de CPU.
- Implementa el cambio de contexto preemptivo por hardware (Timer Tick) y voluntario por software (Syscall yield / bloqueos).
- Resuelve la redirección de entrada y salida estándar (`stdin` / `stdout`) conectando descriptores a pipes o dispositivos.
- Sincroniza la finalización de procesos mediante la cosecha de códigos de retorno (`waitpid`) evitando fugas de PCBs (zombies).

## Internal Mechanics (¿Cómo funciona?)
### 1. Estructura y Creación de Procesos
Al crear un proceso (`process_create_fd`):
- Se busca un slot libre en la tabla estática `process_table[MAX_PROCESSES]`.
- Se reserva un stack de `4 KB` (`STACK_SIZE`) usando `mm_malloc_kernel`.
- La función `build_initial_stack` simula una interrupción apilando en la parte superior de este stack el frame de hardware (`SS = 0`, `RSP = stack_top`, `RFLAGS = 0x202` [interrupciones habilitadas], `CS = 0x08`, `RIP = process_entry_wrapper`) seguido por el frame de registros de `pushState` inicializando `RDI` con la dirección de la función de entrada (`entry`), `RSI` con `argc` y `RDX` con `argv`.
- Todo proceso arranca en `process_entry_wrapper` el cual invoca a `entry(argc, argv)` y, si esta retorna de forma natural, ejecuta `kernel_exit()` de forma defensiva para evitar saltos a direcciones basura en la pila.

### 2. Algoritmo de Planificación: Round Robin con Prioridades
- La cola de listos `run_queue` es un arreglo estático de punteros a PCBs. Al agregar un proceso, se lo inserta secuencialmente. Al eliminar un proceso, se realiza un swap con el último elemento para mantener la cola contigua.
- Cada proceso posee un nivel de prioridad (1 a 5) que determina cuántos ticks consecutivos (quanta) puede correr en la CPU por turno.
- **Timer Tick (`scheduler_tick`):** Con cada tick del PIT, se decrementa `remaining_quanta` del proceso actual.
  - Si `remaining_quanta > 0`, el proceso actual continúa su ejecución retornando su mismo RSP.
  - Si `remaining_quanta == 0`, se re-establece `remaining_quanta = priority`, se cambia el estado del proceso a `PROCESS_READY` y se selecciona el siguiente proceso READY llamando a `scheduler_next_ready()`.
- **Round-Robin Circular:** `scheduler_next_ready` avanza circularmente el índice `queue_idx` sobre `run_queue` y retorna el primer proceso `PROCESS_READY`. El proceso de respaldo `idle` se omite y solo se selecciona si ningún otro proceso no-idle está listo.

### 3. Cambio de Contexto en Assembler
- Cuando ocurre un tick del PIT (`_irq00Handler`) o una solicitud voluntaria (`_irq128Handler`/`_irq129Handler` con `force_switch == 1`):
  1. Se ejecuta la macro `pushState` que apila en el stack los 15 registros de uso general del proceso actual.
  2. Se guarda el stack pointer resultante (`RSP`) en el campo `rsp` del PCB del proceso suspendido.
  3. Se invoca a `scheduler_tick` o `scheduler_yield_impl`, los cuales eligen el nuevo proceso `RUNNING` y retornan su RSP guardado.
  4. Se actualiza el registro de hardware `RSP` de la CPU con el valor retornado (`mov rsp, rax`).
  5. Se ejecuta `popState` y la instrucción `iretq`, restaurando el estado completo de los registros del nuevo proceso seleccionado y continuando su ejecución.

### 4. Bloqueo y Sincronización
- **Bloqueo:** `process_block` cambia el estado del proceso a `PROCESS_BLOCKED`. Si el proceso bloqueado es el que está en ejecución, se fuerza el cambio de contexto seteando `force_switch = 1`.
- **Espera de Hijos (`waitpid`):** Si el hijo solicitado ya es un `PROCESS_ZOMBIE`, se recupera su `retval`, se marca su PCB como `PROCESS_FREE` y retorna inmediatamente. Si está vivo, el padre transiciona a `PROCESS_BLOCKED` con `waiting_for = pid` y cede la CPU. Al terminar, `process_exit` del hijo detecta que el padre espera, escribe el código de retorno en el slot de `rax` de la pila guardada del padre (`parent->rsp[14]`) y lo despierta (`PROCESS_READY`).

## Key Files & Structs (Archivos y Estructuras Clave)
* **Archivos principales:**
  - `Kernel/c/process/process.c`: Inicialización de procesos, wrappers de API y rutinas de creación, wait, bloqueo e inserción de argumentos.
  - `Kernel/include/process/process.h`: Estructura del PCB y de la información expuesta a Userland (`ProcessInfo`).
  - `Kernel/c/process/scheduler.c`: Lógica de Round-Robin con Prioridades, ticks del timer y yield de CPU.
  - `Kernel/include/process/scheduler.h`: Prototipos del planificador y bandera `force_switch`.
  - `Kernel/asm/interrupts.asm` (sección de context switch): Rutinas de intercambio de RSP a bajo nivel.
* **Estructuras de datos:**
  - `PCB` (Block de Control de Proceso):
    ```c
    typedef struct PCB {
        uint64_t pid;               /* PID único */
        char name[MAX_NAME_LEN];    /* Nombre identificador */
        uint8_t priority;           /* Prioridad (1-5) */
        uint8_t remaining_quanta;   /* Ticks restantes del turno */
        ProcessState state;         /* Estado del proceso */
        uint64_t *rsp;              /* RSP guardado del proceso */
        uint64_t *stack_base;       /* Dirección base del stack asignado */
        uint8_t foreground;         /* 1 si corre en fg, 0 si corre en bg */
        int fd[2];                  /* Descriptores redireccionados a pipes/consola */
        uint64_t parent_pid;        /* PID del proceso creador */
        uint64_t waiting_for;       /* PID que está esperando en waitpid */
        int argc; char **argv;      /* Argumentos */
        int retval;                 /* Valor de salida */
    } PCB;
    ```
* **Funciones fundamentales:**
  - `build_initial_stack()`: Construye el stack frame inicial simulando el estado de CPU post-interrupción.
  - `scheduler_tick(uint64_t *current_rsp)`: Invocada en cada PIT tick para actualizar quántums y resolver cambios de contexto.
  - `scheduler_yield_impl(uint64_t *current_rsp)`: Implementación de yield voluntario de CPU.

## System Calls Relacionadas
- **`sys_create_process` (Syscall 19) / `sys_create_process_fd` (Syscall 35):** Crean procesos de usuario de forma básica o con fds redirigidos.
- **`sys_exit` (Syscall 20):** Finaliza el proceso actual devolviendo un valor de salida.
- **`sys_getpid` (Syscall 21):** Devuelve el PID del proceso en ejecución.
- **`sys_ps` (Syscall 22):** Llena un buffer de estructuras `ProcessInfo` para listar procesos activos.
- **`sys_kill` (Syscall 23):** Envía señal de muerte a un proceso por su PID.
- **`sys_nice` (Syscall 24):** Altera el nivel de prioridad de un proceso dado.
- **`sys_block` (Syscall 25) / `sys_unblock` (Syscall 26):** Cambian el estado del proceso entre bloqueado y listo.
- **`sys_yield` (Syscall 27):** Cede voluntariamente el procesador.
- **`sys_waitpid` (Syscall 28):** Suspende al invocante hasta la finalización de un proceso hijo.

## Comments and Limitations (Comentarios y Limitaciones)
- **Límite de Procesos:** La cantidad máxima de procesos concurrentes está acotada por `MAX_PROCESSES = 64`.
- **Aislamiento Físico Inexistente:** Al no estar implementada la paginación de memoria virtual a nivel de procesos (esquema Identity Mapping), no existe protección real de memoria. Cualquier proceso de usuario errante puede leer o escribir datos del stack o sección de datos de otro proceso o del propio Kernel.
- **Fuga de Zombies:** Si un proceso padre finaliza antes de cosechar a sus hijos zombies, el Kernel limpia los recursos de los huérfanos marcándolos como `PROCESS_FREE` para evitar filtraciones en la tabla de procesos.
