# Kernel IPC Module (Inter-Process Communication)

## Overview (¿Qué es?)
El módulo [[kernel_ipc.md|IPC]] (Comunicación entre Procesos) provee los mecanismos necesarios para que procesos independientes o relacionados puedan intercambiar flujos de datos y coordinar su ejecución de forma segura. En nuestro sistema operativo, se compone de dos primitivas principales: **Semáforos Nombrados** para la sincronización y exclusión mutua, y **Pipes Unidireccionales** (nombrados o anónimos) para la transferencia de flujos de bytes.

## Functionality (¿Qué hace?)
### 1. Semáforos Nombrados
- Permiten la sincronización y protección de secciones críticas entre procesos no relacionados genéticamente mediante identificadores alfanuméricos.
- Ofrecen operaciones atómicas de espera (`sem_wait`), señalización (`sem_post`), y difusión (`sem_broadcast`).
- Manejan colas de espera por semáforo para evitar el uso de *busy waiting* (espera activa), bloqueando y suspendiendo procesos de la cola de ejecución.
- Garantizan exclusión mutua interna utilizando spinlocks atómicos por hardware (`lock xchg`).

### 2. Pipes (Tuberías)
- Proveen canales unidireccionales de transferencia de bytes con semántica FIFO.
- Ofrecen abstracción transparente de descriptores de archivos, integrados directamente con las lecturas (`sys_read`) y escrituras (`sys_write`) del sistema.
- Bloquean automáticamente al proceso lector si la tubería está vacía, y al escritor si el buffer interno está lleno.
- Admiten pipes anónimos (para redirección de comandos en la shell `p1 | p2`) y pipes nombrados (para procesos independientes).
- Gestionan de forma segura el fin de archivo (EOF) si se cierran los extremos de escritura, y el canal roto (Broken Pipe) si se cierran los extremos de lectura.

## Internal Mechanics (¿Cómo funciona?)
### 1. Sincronización: Semáforos
- **Spinlock de Control:** Toda mutación sobre la tabla de semáforos se serializa utilizando el cerrojo global `sem_lock` y las funciones ensamblador `acquire` y `release` de `libasm.asm`. `acquire` realiza un ciclo ocupado ejecutando `xchg rax, [rdi]` (donde `rax` es 1), garantizando atomicidad mediante el bloqueo del bus de memoria por hardware.
- **Mecanismo de Bloqueo (Semántica Mesa):** Al ejecutar `sem_wait`:
  - Si `s->value > 0`, se decrementa el valor y se otorga el recurso de inmediato.
  - Si `s->value == 0`, el proceso actual se agrega a la cola circular `s->wait_queue[MAX_PROCESSES]`, se marca su estado como `PROCESS_BLOCKED` mediante `process_block()`, se libera `sem_lock` y se cede el procesador llamando a `kernel_yield()` (`int 0x81`).
  - Al despertarse, el proceso vuelve al inicio del ciclo `while(1)` para re-chequear el valor del semáforo. Esto previene condiciones de carrera si otro proceso de mayor prioridad consume el recurso en el intervalo transcurrido desde el desbloqueo (Semántica Mesa).
- **Señalización:** Al ejecutar `sem_post`, se incrementa `s->value`. Si hay procesos bloqueados en la cola (`s->wait_count > 0`), se remueve el primero de la cola mediante `queue_pop` y se lo pasa a listos usando `process_unblock()`.

### 2. Comunicación: Pipes
- **Buffer Circular y Semáforos de Estado:** Cada `Pipe` gestiona un buffer circular de caracteres de tamaño `PIPE_BUF_SIZE = 4096`. El control de bloqueo por falta de espacio o falta de datos se realiza asociando internamente tres semáforos nombrados únicos al inicializar la tubería:
  - `data_sem` (nombre `__pipe_d_X`): Inicializado en 0. Cuenta la cantidad de bytes legibles disponibles en el buffer.
  - `space_sem` (nombre `__pipe_s_X`): Inicializado en `PIPE_BUF_SIZE`. Cuenta el espacio libre disponible para escribir.
  - `mutex_sem` (nombre `__pipe_m_X`): Inicializado en 1. Garantiza exclusión mutua para la modificación de los índices de lectura/escritura (`head`, `tail`) del buffer circular.
- **Lectura/Escritura Bloqueante:**
  - `pipe_write` hace `sem_wait(space_sem)` para asegurar que haya espacio libre, luego toma exclusión con `mutex_sem`, escribe el byte en `buffer[tail]`, incrementa `tail` de forma circular, libera `mutex_sem` y hace `sem_post(data_sem)` para notificar al lector de la llegada de datos.
  - `pipe_read` funciona de forma análoga haciendo `sem_wait(data_sem)` y `sem_post(space_sem)`.
- **Cierre y EOF:** 
  - Al cerrar un pipe de lectura (`pipe_close` de un lector), si la cantidad de lectores llega a 0, se postean `MAX_PROCESSES` tokens sobre `space_sem` para despertar a los escritores bloqueados, que inmediatamente retornan `-1` indicando error de tubería rota (*broken pipe*).
  - Al cerrar un pipe de escritura, si los escritores llegan a 0, se postea sobre `data_sem` para despertar a los lectores, quienes detectan `head == tail` y retornan `0` indicando fin de archivo (EOF).

## Key Files & Structs (Archivos y Estructuras Clave)
* **Archivos principales:**
  - `Kernel/c/ipc/semaphore.c`: Administración de la tabla de semáforos, encolamiento de procesos bloqueados y primitivas de sincronización.
  - `Kernel/include/ipc/semaphore.h`: Interfaz del módulo de semáforos.
  - `Kernel/c/ipc/pipe.c`: Manejo de buffers de tuberías, inicialización de semáforos de estado, lógica de cierre y direccionamiento de pipes.
  - `Kernel/include/ipc/pipe.h`: Definición de constantes de pipes (`PIPE_FD_READ_BASE = 100`, `PIPE_FD_WRITE_BASE = 200`).
  - `Kernel/asm/libasm.asm`: Código del spinlock atómico de control (`acquire`/`release`).
* **Estructuras de datos:**
  - `struct Semaphore`:
    ```c
    typedef struct {
        char     name[SEM_NAME_LEN];        /* Identificador alfanumérico */
        int64_t  value;                     /* Contador de tokens */
        uint64_t wait_queue[MAX_PROCESSES]; /* Cola de PIDs bloqueados */
        int      wait_head, wait_tail, wait_count;
        int      open_count;                /* Cantidad de referencias abiertas */
    } Semaphore;
    ```
  - `struct Pipe`:
    ```c
    typedef struct {
        int      in_use;
        char     buffer[PIPE_BUF_SIZE];
        int      head, tail;                /* Índices circulares */
        int      readers, writers;          /* Cantidad de descriptores activos */
        char     data_sem[16];              /* Semáforo de bytes disponibles */
        char     space_sem[16];             /* Semáforo de espacio libre */
        char     mutex_sem[16];             /* Exclusión mutua del buffer */
        char     name[PIPE_NAME_LEN];       /* Nombre si es pipe nombrado */
    } Pipe;
    ```
* **Funciones fundamentales:**
  - `acquire(uint64_t *lock)`: Adquiere de manera atómica el spinlock de control mediante `xchg`.
  - `pipe_create(int fds[2])`: Inicializa un canal de comunicación anónimo y devuelve los FDs asignados.

## System Calls Relacionadas
- **`sys_sem_open` (Syscall 29) / `sys_sem_close` (Syscall 32):** Crean/abren y cierran semáforos.
- **`sys_sem_wait` (Syscall 30) / `sys_sem_post` (Syscall 31):** Primitivas de decremento/incremento de semáforo.
- **`sys_pipe` (Syscall 33) / `sys_pipe_close` (Syscall 34):** Creación y cierre de tuberías anónimas.
- **`sys_pipe_open` (Syscall 36):** Apertura o creación de tuberías nombradas compartidas.

## Comments and Limitations (Comentarios y Limitaciones)
- **Límites Fijos:** La tabla de semáforos está limitada a `MAX_SEMS = 32` y la de pipes a `MAX_PIPES = 16`. Exceder este límite provocará fallos de apertura/creación en las llamadas al sistema.
- **Capacidad Estática:** El tamaño de buffer de cada pipe es rígido (`PIPE_BUF_SIZE = 4096`).
- **Mapeo de Descriptores:** Los fds virtuales de los procesos se corresponden físicamente con índices estáticos en la tabla global: `100 + idx` para lectura y `200 + idx` para escritura.
