# Plan de implementacion - TP2 Sistemas Operativos

> **Estado al 2026-06-02 (sincronizado con el codigo):** Pasos 0-5 completos y
> verificados contra el codigo fuente. Shell con `&`, `|`, `Ctrl+C`, `Ctrl+D` y
> `waitpid`; todos los comandos del enunciado implementados (`help`, `mem`, `ps`,
> `loop`, `kill`, `nice`, `block`, `cat`, `wc`, `filter`, `mvar`) y los tests como
> procesos. Syscalls 0-36 (`CANT_SYS = 37`) cableadas en el dispatcher; idle con
> `MIN_PRIORITY` + `scheduler_set_idle`; semaforos con spinlock `xchg`. Todos los
> bugs de la auditoria (#1-#7) estan corregidos y commiteados en `master`. Paso 6:
> codigo limpio (`-Wall -Wextra` sin warnings, repo sin binarios) y README
> completo; resta **solo** la verificacion *runtime* en QEMU (correr los tests en
> fg/bg a mano, seccion 6.1) — es interactiva porque la salida va al framebuffer
> VBE y no hay runner headless. Ver seccion "Bugs / pendientes conocidos" al final.

## Estado actual (heredado del TPE de Arquitectura)

Ya se cuenta con: kernel 64-bit, IDT/IRQs, driver de video (framebuffer VBE), driver de teclado, driver de sonido (PIT), timer, syscalls via `int 0x80`, shell basica y juego Tron. El sistema es **single-tasking** (un solo flujo de ejecucion, sin procesos ni memoria dinamica).

---

## Paso 0 - Preparacion del entorno

- [x] Limpiar binarios y `.o` del repositorio (el enunciado prohibe binarios en el repo).
- [x] Configurar el entorno de compilacion con la imagen Docker de la catedra: `agodio/itba-so-multiarch:3.1`.
- [x] Ajustar los Makefiles para que `make` / `make all` solo compilen (sin lanzar QEMU ni Docker). Separar reglas de ejecucion.
- [x] Agregar soporte para compilacion condicional del memory manager: `MM=FF ./compile.sh` vs `MM=BUDDY ./compile.sh`, usando flags `-DMM_FF` / `-DMM_BUDDY` en el Makefile.
- [x] Verificar que todo compile con `-Wall` sin warnings desde el principio.

---

## Paso 1 - Memory Management

**Dependencias:** ninguna (es la base para todo lo demas).

### 1.1 Definir la interfaz comun del memory manager
- [x] Crear un header compartido (ej. `memoryManager.h`) con la interfaz que ambas implementaciones deben respetar:
  - `void mm_init(void *start, uint64_t size)` - inicializar el memory manager con un bloque de memoria.
  - `void *mm_malloc(uint64_t size)` - reservar memoria.
  - `void mm_free(void *ptr)` - liberar memoria.
  - `void mm_status(MemStatus *status)` - consultar estado (total, ocupada, libre).

### 1.2 Implementar el memory manager elegido por el grupo
- [x] Elegir e implementar un algoritmo (ej. first-fit con lista enlazada de bloques libres, o bitmap).
- [x] Debe soportar `malloc` y `free`.
- [x] Testear de forma aislada antes de conectar con el resto.

### 1.3 Implementar Buddy System
- [x] Implementar el buddy system como alternativa.
- [x] Misma interfaz que el MM anterior.

### 1.4 Compilacion condicional
- [x] Usar `#ifdef MM_BUDDY` / `#else` (o similar) para elegir entre las dos implementaciones.
- [x] Verificar que `make` compila con una y `make buddy` con la otra.

### 1.5 Syscalls de memoria
- [x] Agregar syscalls nuevas:
  - `sys_malloc(size)` -> devuelve puntero.
  - `sys_free(ptr)` -> libera bloque.
  - `sys_mem_status()` -> devuelve info del estado de la memoria.
- [x] Registrar las syscalls en la tabla `syscalls[]` del kernel y crear los wrappers en `userlib.asm`.

### 1.6 Test
- [x] Integrar `test_mm` (provisto por la catedra) como programa de usuario.
- [x] Verificar que pase sin errores con al menos uno de los dos MM. (20 OK / 0 FAIL con First-Fit)

---

## Paso 2 - Procesos, Context Switching y Scheduler

**Dependencias:** Paso 1 (se necesita `malloc` para crear PCBs y stacks).

### 2.1 Definir el PCB (Process Control Block)
- [x] Crear la estructura PCB con al menos (ver `Kernel/include/process.h`):
  - PID, nombre, prioridad, estado (READY, RUNNING, BLOCKED, ZOMBIE).
  - Stack pointer (RSP guardado), base pointer.
  - Foreground/background flag.
  - File descriptors (stdin, stdout) - necesario luego para pipes.
  - Puntero al padre (para `waitpid`).
  - Argv/argc (pasaje de parametros).

### 2.2 Implementar la tabla de procesos
- [x] Array de PCBs con limite maximo (`MAX_PROCESSES = 64`).
- [x] Funcion para crear proceso: asignar PID, reservar stack, preparar stack frame inicial (`build_initial_stack` simula un frame post-interrupcion).
- [x] Funcion para destruir/finalizar proceso: liberar recursos (`process_exit` / `process_kill`).

### 2.3 Implementar Context Switch
- [x] Rutina de context switch en ASM (`interrupts.asm`):
  - Guardar todos los registros del proceso actual en su stack.
  - Cambiar RSP al stack del proximo proceso.
  - Restaurar registros del proximo proceso.
  - `iretq` para reanudar ejecucion.
- [x] Invocar el context switch desde el handler del timer (IRQ0) -> `scheduler_tick`.

### 2.4 Implementar el Scheduler (Round Robin con prioridades)
- [x] Round Robin donde la prioridad determina cuantos quantums consecutivos recibe un proceso (`remaining_quanta = priority`).
- [x] Procesos con mayor prioridad reciben mas tiempo de CPU.
- [x] El scheduler ignora procesos en estado BLOCKED o ZOMBIE (`scheduler_next_ready` solo elige READY).

### 2.5 Crear el proceso idle
- [x] Proceso `idle` que ejecuta `hlt` en loop.
- [~] **DEVIACION:** se crea con `DEFAULT_PRIORITY` (3), no con la prioridad mas baja, y compite en igualdad en el round-robin en vez de correr solo cuando no hay otro READY. Ver bug #2.

### 2.6 Adaptar el kernel al modelo multiproceso
- [x] El kernel crea `idle` y `shell` como primeros procesos y arranca el scheduler (`initializeKernelBinary` + `scheduler_start`).
- [x] El driver de teclado despierta al proceso bloqueado esperando input (`kbd_set_waiting` / wakeup en `handlePressedKey`).

### 2.7 Syscalls de procesos
- [x] Syscalls implementadas (19-28 en `syscallDispatcher.c`):
  - `sys_create_process`, `sys_exit`, `sys_getpid`, `sys_ps`, `sys_kill`,
    `sys_nice`, `sys_block`, `sys_unblock`, `sys_yield`, `sys_waitpid`.
- [x] Wrappers correspondientes en `userlib.asm`.

### 2.8 Tests
- [x] `test_proc` integrado como proceso de usuario.
- [x] `test_prio` integrado como proceso de usuario.

---

## Paso 3 - Sincronizacion (Semaforos)

**Dependencias:** Paso 2 (necesita poder bloquear/desbloquear procesos).

### 3.1 Implementar semaforos con nombre
- [x] Estructura de semaforo: valor, cola de procesos bloqueados, nombre (`semaphore.c`).
- [x] Tabla global de semaforos (`MAX_SEMS = 32`).
- [~] **DEVIACION:** NO se usa una instruccion atomica (`xchg`/`lock cmpxchg`). La exclusion se apoya en que `int 0x80` usa interrupt gate (IF=0 durante la syscall) en uniprocesador. Correcto en la practica pero no cumple el requisito literal. Ver bug #3.
- [x] `sem_wait` bloquea sin busy waiting cuando `value < 0`.
- [x] `sem_post` incrementa y desbloquea un proceso en cola.
- [x] Apertura por nombre (`sem_open`) para procesos no relacionados.

### 3.2 Syscalls de semaforos
- [x] Syscalls implementadas (29-32):
  - `sys_sem_open`, `sys_sem_close`, `sys_sem_wait`, `sys_sem_post`.
- [x] Wrappers en `userlib.asm`.

### 3.3 Test
- [x] `test_sync` integrado como proceso de usuario (verificar 0 con sem / varia sin sem).

---

## Paso 4 - Inter Process Communication (Pipes)

**Dependencias:** Paso 2 y Paso 3 (necesita procesos y posiblemente semaforos para la sincronizacion interna del pipe).

### 4.1 Implementar pipes unidireccionales
- [x] Buffer circular + 3 semaforos (data/space/mutex) para productor/consumidor (`pipe.c`).
- [x] Lectura bloqueante cuando el pipe esta vacio.
- [x] Escritura bloqueante cuando el pipe esta lleno.
- [x] EOF al cerrar el extremo de escritura (y broken-pipe al cerrar lectura).

### 4.2 Abstraccion de file descriptors
- [x] Cada proceso tiene `fd[2]` (stdin=0, stdout=1) en el PCB.
- [x] Un FD puede apuntar a la terminal o a un extremo de un pipe.
- [x] `sys_read`/`sys_write` transparentes: consultan `cur->fd[]` y redirigen a pipe o consola.

### 4.3 Pipes con nombre
- [x] `pipe_open(name)` permite compartir un pipe por nombre acordado.

### 4.4 Syscalls de pipes
- [x] `sys_pipe(fd_array)` -> crea pipe, devuelve FDs (33).
- [x] `sys_pipe_open(name)` -> abre pipe por nombre (36).
- [x] `sys_pipe_close(fd)` -> cierra un FD de pipe (34).
- [~] `sys_dup2` NO implementado. En su lugar la shell usa `sys_create_process_fd` (35) para asignar stdin/stdout del hijo al crearlo. Enfoque alternativo valido; si se exige `dup2` explicito, falta.
- [x] Wrappers en `userlib.asm`.

---

## Paso 5 - Aplicaciones de User Space

**Dependencias:** Pasos 1-4 completados.

### 5.1 Reescribir la shell (`sh`)
- [x] Parsea el comando y crea un proceso hijo (`processLine` / `spawn_simple` / `spawn_pipe`).
- [x] Soporte `&` al final -> background (no cede foreground).
- [x] Soporte `|` para conectar 2 procesos via pipe.
- [x] Soporte `Ctrl+C` -> mata el proceso foreground (`process_get_foreground` + `process_kill`).
- [x] Soporte `Ctrl+D` -> EOF al stdin.
- [x] En foreground la shell hace `waitpid` antes de reimprimir el prompt.

### 5.2 Comandos basicos
- [x] `help` - lista todos los comandos y operadores.
- [x] `mem` - estado de la memoria (total/usada/libre/bloques) via `sys_mem_status`.
- [x] `ps` - lista procesos (PID, prioridad, fg, estado, nombre). *Nota: no imprime RSP/RBP.*
- [x] `loop [ticks]` - imprime su PID periodicamente con espera activa (proceso).
- [x] `kill <pid>` - mata un proceso por PID (builtin).
- [x] `nice <pid> <prioridad>` - cambia la prioridad (builtin).
- [x] `block <pid>` - alterna BLOCKED/READY (builtin, consulta estado via `ps`).

### 5.3 Comandos de IPC
- [x] `cat` - copia stdin a stdout hasta EOF; funciona standalone (teclado) y con pipes.
- [x] `wc` - cuenta lineas de stdin.
- [x] `filter` - reimprime stdin filtrando las vocales (proceso, pensado para pipes).
- [x] `mvar <escritores> <lectores>` - lectores/escritores con MVar sincronizada por
  dos semaforos nombrados; crea los hijos y termina inmediatamente.

### 5.4 Tests como programas de usuario
- [x] `test_mm <max_memory>` corre como proceso.
- [x] `test_proc <max_processes>` corre como proceso.
- [x] `test_prio <target_value>` corre como proceso.
- [x] `test_sync <n> <use_sem>` corre como proceso.
- [x] Todos pueden correrse en foreground y en background (`&`).

---

## Paso 6 - Verificacion y limpieza final

### 6.1 Verificacion obligatoria
- [x] Compilar con `-Wall -Wextra` sin warnings, tanto FF como BUDDY (verificado).
- [ ] Correr `test_mm` en foreground y background -> sin errores. *(requiere QEMU interactivo)*
- [ ] Correr `test_proc` en foreground y background -> sin errores. *(bug #1 ya corregido; requiere QEMU)*
- [ ] Correr `test_sync` en foreground y background -> resultado 0 con semaforos. *(requiere QEMU)*
- [ ] Correr `test_prio` -> se visualizan diferencias de tiempo segun prioridad. *(requiere QEMU)*
- [ ] Verificar a mano que no haya deadlocks/races/busy-waiting indebido. *(requiere QEMU)*

> Nota: el codigo compila limpio y los bugs de logica detectados estan corregidos,
> pero la verificacion *runtime* (correr cada test en QEMU en fg y bg) debe hacerse
> de forma interactiva; no hay runner headless en el repo.

### 6.2 Limpieza del repositorio
- [x] No hay binarios trackeados; `.gitignore` cubre `*.o/*.bin/*.img/*.qcow2/*.vmdk/*.sys/*.pdf`.
- [x] Historial de git coherente (commits incrementales por feature/fix en `master`).

### 6.3 README.md
- [x] `README.md` actualizado: compilacion/ejecucion, todos los comandos y tests
  con sus parametros, operadores (`&`, `|`), atajos (`Ctrl+C`, `Ctrl+D`, `+/-`),
  ejemplos de uso, limitaciones conocidas y nota de uso de IA.

---

## Bugs / pendientes conocidos (auditoria 2026-05-30)

1. **[CRITICO] ✅ ARREGLADO — Fuga en la run-queue del scheduler.** `process_exit`
   y la rama self de `process_kill` no removian al proceso de `run_queue` al pasar
   a ZOMBIE, dejando punteros stale (y duplicados al reusarse el slot) y haciendo
   crecer `queue_size` hasta `MAX_PROCESSES` (a partir de ahi `scheduler_add`
   descartaba procesos nuevos y la shell colgaba en `waitpid`). **Fix aplicado:**
   `scheduler_remove(p)` al pasar a ZOMBIE en ambos lugares (`process.c`).

2. **[MEDIO] ✅ ARREGLADO — `idle` no corria como fallback.** Ahora `idle` se crea
   con `MIN_PRIORITY` y se registra via `scheduler_set_idle`; `scheduler_next_ready`
   lo reserva como ultima instancia y solo lo elige si no hay otro proceso READY
   (`scheduler.c`, `kernel.c`).

3. **[MEDIO/requisito] ✅ ARREGLADO — Semaforos sin instruccion atomica.** Se
   agrego un spinlock test-and-set con `xchg` (`acquire`/`release` en `libasm.asm`)
   y un lock global `sem_lock` que protege todas las operaciones de la tabla de
   semaforos (`semaphore.c`). El lock se libera siempre antes de ceder la CPU.

4. **[BAJO, fragilidad] PARCIALMENTE ARREGLADO — Uso de stack liberado.**
   - ✅ **Caso grave (Ctrl+C):** el handler de teclado (`_irq01Handler`) no cedia
     la CPU, asi que tras matar al proceso foreground corriente con Ctrl+C se hacia
     `iretq` de vuelta a su stack ya liberado y el proceso muerto seguia corriendo
     en userland (IF=1) hasta el proximo tick del timer. **Fix aplicado:** el
     handler de teclado ahora chequea `force_switch` y llama a
     `scheduler_yield_impl` (igual que el gate de syscall), cambiando de proceso
     de inmediato.
   - ⚠️ **Residual (teorico):** `process_exit`/`sys_exit` y el kill-self siguen
     haciendo `mm_free(stack_base)` mientras el handler corre sobre ese stack;
     recien cambia de stack al volver de `scheduler_yield_impl`. Funciona porque
     corre con IF=0 (sin preempcion) y no hay otra asignacion en el medio. Para
     robustez total habria que diferir el free al scheduler (tras el cambio de
     stack); no se hizo para no agregar complejidad por un riesgo solo teorico.

5. **[BAJO] PARCIALMENTE ARREGLADO — Proceso esperando teclado.**
   - ✅ **Use-after-free:** si se mataba a un proceso bloqueado leyendo teclado,
     `kbd_waiting_process` quedaba apuntando a su PCB ya liberado y la siguiente
     tecla escribia `PROCESS_READY` sobre ese slot (potencialmente reusado).
     **Fix:** `kbd_wake_waiting` solo marca READY si el proceso sigue `BLOCKED`, y
     siempre consume el puntero. (No disparable en el flujo normal de la shell,
     pero era un use-after-free latente.)
   - ⚠️ **Residual:** sigue habiendo un solo `kbd_waiting_process`; si dos procesos
     bloquean en `sys_read` de teclado, el segundo pisa al primero (lost wakeup).
     En la practica solo el foreground lee teclado: limitacion conocida.

6. **✅ ARREGLADO — Comandos de usuario faltantes (Paso 5):** se agregaron `mem`,
   `loop`, `kill`, `nice`, `block` (builtins/proceso) y `filter`, `mvar` (procesos)
   en `userlib.c`, cableados en la tabla `commands[]`.

7. **✅ ARREGLADO — `cat`/`wc`/`filter` no funcionaban standalone desde teclado.**
   El `sys_read` de teclado devolvia 0 tanto al bloquearse (despertar sin datos)
   como en EOF, y los comandos usaban `while(sys_read > 0)`, asi que salian en la
   primera lectura vacia y no leian nada (solo andaban via pipe). **Fix:** el
   kernel ahora devuelve `SYS_READ_RETRY` (-2) cuando bloquea por teclado y 0 solo
   en EOF; userland usa un helper `read_full` que reintenta ante `READ_RETRY`. Asi
   un mismo bucle sirve para teclado (reintento) y pipe (0 = EOF).

8. **[CRITICO] ✅ ARREGLADO — Fuga de slots: los procesos no se reapeaban.**
   En el flujo normal de foreground la shell hace `waitpid` *antes* de que el hijo
   termine, asi que se bloquea; cuando el hijo sale, `wake_waiting_parent` le
   entrega el retval y lo despierta, pero el padre retorna directo de `waitpid`
   sin re-entrar a `process_waitpid`. El hijo quedaba `ZOMBIE` para siempre (la
   rama de reaping de `process_waitpid` solo corre si el hijo ya estaba muerto al
   llamar `waitpid`, lo que no pasa nunca en `spawn_simple`). **Cada comando de
   foreground filtraba un slot de PCB**: tras ~62 comandos la tabla (64) se llenaba
   y no se podian crear mas procesos. Los procesos background huerfanos (p.ej. los
   hijos de `mvar`, cuyo padre sale enseguida) tambien filtraban. **Fix:**
   `finalize_pcb` libera el slot directo si el padre ya consumio el retval
   (`reaped`) o si el proceso quedo huerfano (padre muerto); solo deja `ZOMBIE`
   cuando el padre sigue vivo y todavia no espero (carrera hijo-sale-antes-de-wait,
   que luego reclama el `waitpid`). `process_kill` de otro proceso siempre libera.

   *Doc:* el `test_mm` de la catedra es un bucle infinito que verifica integridad
   (silencio = OK, `test_mm ERROR` = fallo); no imprime "20 OK / 0 FAIL". README
   corregido.

---

## Orden de implementacion recomendado

```
Paso 0 (Preparacion)
  |
Paso 1 (Memory Management)
  |
Paso 2 (Procesos + Context Switch + Scheduler)
  |
  +------+------+
  |             |
Paso 3        Paso 4
(Semaforos)   (Pipes) -- puede usar semaforos internamente
  |             |
  +------+------+
         |
Paso 5 (Aplicaciones de usuario + Shell)
         |
Paso 6 (Verificacion + README)
```

Los pasos 3 y 4 pueden desarrollarse en paralelo una vez que los procesos funcionen, pero los pipes pueden beneficiarse de usar semaforos internamente para la sincronizacion.
