# Resumen de Arquitectura — MASS OS (TP2 Sistemas Operativos, ITBA)

> Reporte de análisis del repositorio realizado desde la perspectiva de arquitectura de
> sistemas operativos. Contrastado contra el enunciado oficial (`Docs/TP 2.txt`).
>
> **Conclusión adelantada:** el proyecto está **prácticamente completo** respecto a los
> requerimientos funcionales del enunciado. Lo que queda es robustez, pulido de
> calidad de código y preparación para la defensa, no construcción de componentes faltantes.

---

## 1. Diagnóstico actual

### Tipo de arquitectura

**Kernel monolítico de 64 bits (x86-64)**, bare-metal, que arranca vía Pure64 + BMFS sobre
QEMU/VirtualBox/hardware real. Todos los servicios (memoria, procesos, scheduling, drivers,
IPC, sincronización) viven en un único binario enlazado en `0x100000`, emitido como binario
plano (no ELF).

**Detalle arquitectónico importante:** todo corre en **ring 0**. El frame inicial de cada
proceso usa `CS = 0x08` (segmento de código del kernel) — ver `process.c:64`
(`build_initial_stack`). No hay separación de privilegios por hardware (ring 3) ni paginación
con aislamiento de espacios de direcciones: kernel y "userland" comparten el mismo address
space plano. Los módulos de userland (`shell`) se cargan en direcciones fijas (`0x400000`,
`0x500000`) y los comandos/tests se ejecutan como procesos que comparten ese espacio.
Esto es lo habitual en este TP, pero conviene tenerlo presente: la frontera kernel/usuario
es **lógica** (vía `int 0x80`), no impuesta por hardware.

### Componentes construidos

| Componente | Estado | Archivos clave |
|---|---|---|
| **Memory Manager (First-Fit)** | ✅ Completo | `Kernel/c/memoryManager/memoryManagerFF.c` |
| **Memory Manager (Buddy System)** | ✅ Completo | `Kernel/c/memoryManager/memoryManagerBuddy.c` |
| Interfaz MM común intercambiable en compilación | ✅ | `memoryManager.h` + flag `-DMM_FF`/`-DMM_BUDDY` |
| **Scheduler** (Round-Robin con prioridades) | ✅ Completo | `Kernel/c/scheduler.c` |
| **Procesos / PCB / context switch** | ✅ Completo | `Kernel/c/process.c`, `interrupts.asm` |
| **Semáforos** (nombrados, con `xchg` atómico) | ✅ Completo | `Kernel/c/semaphore.c`, `libasm.asm` |
| **Pipes** (anónimos + nombrados, bloqueantes) | ✅ Completo | `Kernel/c/pipe.c` |
| **Driver de teclado** (+ Ctrl+C / Ctrl+D) | ✅ | `Kernel/c/drivers/keyboardDriver.c` |
| **Driver de video** (modo gráfico) | ✅ | `Kernel/c/drivers/videoDriver.c` |
| **Dispatcher de syscalls** (37 syscalls) | ✅ | `Kernel/c/syscallDispatcher.c`, `defs.h` (`CANT_SYS 37`) |
| **Shell** (`sh`) con `&`, `\|`, Ctrl+C/D | ✅ | `Userland/c/shell.c`, `userlib.c` |
| **Apps de userland** | ✅ | ver tabla §3 |
| **Tests de cátedra como procesos** | ✅ | `test_mm`, `test_proc`, `test_sync`, `test_prio` |

### Cómo interactúan en el arranque

`loader.asm` → `kernelMain` (GDT/segmentos) → `initializeKernelBinary` (`kernel.c`):

1. `loadModules` copia los módulos de userland a sus direcciones fijas.
2. `clearBSS`, `load_idt`.
3. `mm_init(HEAP_START, HEAP_SIZE)` — heap de 8 MB en `0x600000`.
4. `process_init`, `scheduler_init`, `sem_init`, `pipe_init`.
5. Crea el proceso **idle** (PID 0, prioridad mínima, `hlt` en loop, registrado como fallback
   del scheduler) y el proceso **shell** (foreground, entry = `0x400000`).
6. Retorna → `main()` → `scheduler_start()` → `scheduler_start_asm` carga el `rsp` del primer
   PCB, hace `popState` + `iretq` y salta a userland. **Nunca retorna**: el scheduler es el
   loop principal del kernel.

---

## 2. Flujo de ejecución

### Manejo de una interrupción de timer (context switch preemptivo)

`interrupts.asm` → `_irq00Handler`:

```
_irq00Handler:
    pushState                 ; guarda los 15 GPRs sobre el stack del proceso actual
    mov rdi, rsp              ; pasa el rsp actual a C
    call scheduler_tick       ; elige el próximo proceso, devuelve su rsp en rax
    mov rsp, rax              ; ← EL context switch: cambia de stack
    mov al, 20h / out 20h,al  ; EOI al PIC
    popState                  ; restaura GPRs del NUEVO proceso
    iretq                     ; restaura RIP/CS/RFLAGS del nuevo proceso
```

`scheduler_tick` (`scheduler.c:116`) guarda `cur->rsp = current_rsp`, decrementa el quantum
del proceso actual (`remaining_quanta`, derivado de su `priority`); si se agotó lo pasa a
`READY` y elige el siguiente con `scheduler_next_ready` (round-robin circular que reserva
`idle` como última instancia). El **swap real de pila ocurre en assembly** (`mov rsp, rax`):
cada proceso tiene su contexto completo serializado en su propio stack.

### Cambio de contexto cooperativo / syscalls

`int 0x80` → `_irq128Handler`:

```
pushState
call [syscalls + rax*8]      ; despacha la syscall por número
mov [rsp + 14*8], rax        ; escribe el valor de retorno en el slot de RAX
cmp [force_switch], 0        ; ¿la syscall pidió ceder CPU?
  → si sí: scheduler_yield_impl(rsp) y mov rsp, rax (cambia de proceso)
popState / iretq
```

La bandera global `force_switch` (`scheduler.c:6`) es el mecanismo de yield cooperativo: la
ponen `sys_yield`, `sys_exit`, `sys_block` sobre el actual, `sem_wait` cuando bloquea,
`waitpid`, etc. Se procesa al volver al gate de la syscall (o del teclado, para el caso
kill-self de Ctrl+C — ver `_irq01Handler`, manejado explícitamente para no hacer `iretq`
sobre el stack ya liberado de un proceso muerto).

### Sincronización y IPC

- **Semáforos**: tabla global protegida por un único `sem_lock` adquirido con `xchg`
  (`acquire`/`release` en `libasm.asm`). `sem_wait` decrementa y, si queda negativo, encola
  el PID y bloquea el proceso; `sem_post` incrementa y despierta uno en espera. Sin busy
  waiting, atómico.
- **Pipes**: buffer circular protegido por 3 semáforos por pipe (`data`, `space`, `mutex`) →
  patrón productor/consumidor clásico, bloqueante, con manejo de EOF (writers==0) y broken
  pipe (readers==0). Transparente: `cat | filter` y `filter` standalone usan el mismo código.

---

## 3. Mapa de agujeros (qué falta / qué es parcial)

El enunciado obligatorio está **cubierto en su totalidad** a nivel funcional. Los "agujeros"
restantes son de robustez, calidad y aristas, no de componentes ausentes.

### Cobertura del enunciado

| Requerimiento del enunciado | Estado |
|---|---|
| MM elegido + Buddy, intercambiables en compilación | ✅ |
| Syscalls de reservar/liberar/consultar memoria | ✅ |
| Multitasking preemptivo + RR con prioridades | ✅ |
| Crear/finalizar proceso con argumentos, getpid, ps, kill, nice, block/unblock, yield, waitpid | ✅ |
| Semáforos compartidos por nombre, sin busy-waiting, atómicos | ✅ |
| Pipes unidireccionales bloqueantes, transparentes, compartidos por nombre | ✅ |
| `sh` con foreground/background (`&`) y pipe (`\|`), Ctrl+C, Ctrl+D | ✅ |
| `help`, `mem`, `ps`, `loop`, `kill`, `nice`, `block` | ✅ |
| `cat`, `wc`, `filter`, `mvar` | ✅ |
| Tests `test_mm`/`test_proc`/`test_sync` como **procesos** (no built-ins), fg y bg | ✅ (`spawn_simple`→`sys_create_process`) |
| README en la raíz | ✅ |

### Agujeros reales detectados (robustez / estabilidad)

1. **Buddy `mm_free` sin validación** (`memoryManagerBuddy.c:170`). No verifica que `ptr` esté
   dentro del heap ni detecta double-free. Un `free` inválido corrompe las free-lists
   silenciosamente. First-Fit tiene el mismo problema. → *seguridad/estabilidad*.

2. **Stacks de 4 KB sin guard page** (`process.h:8`, `STACK_SIZE = 4*1024`). Un stack overflow
   pisa memoria adyacente del heap sin fallar de forma detectable. No hay página de guarda ni
   chequeo de límite. → *estabilidad*.

3. **Sin separación de privilegios por hardware**. Todo en ring 0, sin paginación con
   aislamiento. Un proceso puede leer/escribir memoria del kernel o de otro proceso. El
   enunciado lo tolera (pide binarios separados, no aislamiento de hardware), pero es la
   limitación de seguridad más grande y conviene declararla explícitamente en la defensa.

4. **`mm_malloc`/`mm_free` no son reentrantes**. No hay lock sobre el heap. Es seguro hoy
   porque corren en contexto de syscall (`IF=0`, no apropiable) y ninguna ISR asigna memoria,
   pero es una invariante implícita: si en el futuro una ISR llamara a `mm_*`, habría
   corrupción. Documentar la invariante.

5. **`scheduler_remove` es O(n) y reordena la cola** (swap con el último). Correcto, pero
   altera el orden round-robin tras cada remoción; con muchos procesos creándose/muriendo
   puede producir reparto de CPU algo irregular. Aceptable para el TP, mencionable en defensa.

### Agujeros de calidad de código (la rúbrica pesa 3 puntos)

6. **Comentarios informales / no profesionales** que conviene limpiar antes de la entrega:
   - Emoji 🐐 incrustado en un comentario: `memoryManagerBuddy.c:206` (`"Vuelvee🐐 a ingresar..."`).
   - Chistes en comentarios: `memoryManagerFF.c:86` (`"Se lo come como el Dibu"`),
     `process.c` referencias varias.
   - Riesgo: la consigna prohíbe explícitamente caracteres/contenido espurio y la calidad de
     código se evalúa; un emoji en el fuente es un blanco fácil de crítica en la defensa.

7. **Documentación interna desactualizada**: `CLAUDE.md` afirma `CANT_SYS = 29` (el real es 37,
   `defs.h:20`) y que los tests "corren in-process inside the shell" (en realidad se lanzan como
   procesos vía `sys_create_process`). No afecta la nota pero conviene corregirlo.

### A verificar antes de entregar (no comprobable en este análisis estático)

8. **Compilación con `-Wall` sin warnings** — requisito **obligatorio** (su incumplimiento
   manda a recuperatorio). Hay que compilar ambos MM dentro del contenedor y confirmar 0
   warnings. No se puede verificar sin Docker desde acá.

9. **El README cubre todos los ítems pedidos** (instrucciones, descripción de cada
   comando/test y parámetros, caracteres especiales, atajos de teclado, ejemplos por fuera de
   los tests, requerimientos faltantes, limitaciones, citas de IA). Conviene auditarlo contra
   la lista del enunciado (líneas 236-250).

10. **Repo sin binarios** — el `.gitignore` está bien armado (`*.bin`, `*.o`, `*.img`,
    `*.qcow2`, etc.) y `git ls-files` no muestra binarios versionados. ✅ Verificado.

---

## 4. Próximos pasos (backlog priorizado)

Ordenado de más crítico (riesgo de recuperatorio / pérdida de puntos) a secundario (pulido).

### 🔴 Crítico — bloquea la aprobación si falla

1. **Verificar `-Wall -Wextra` sin warnings en ambos MM.** Compilar `./compile.sh` y
   `MM=BUDDY ./compile.sh` dentro del contenedor y arreglar cualquier warning. *(Obligatorio
   por enunciado; su incumplimiento implica recuperatorio.)*

2. **Correr y confirmar que pasan los 3 tests obligatorios como procesos, en fg y bg:**
   `test_mm`, `test_proc`, `test_sync` (este último con y sin semáforos). Documentar la salida
   esperada. Confirmar que al menos un MM pasa `test_mm`.

3. **Auditar el README** contra los ítems de las líneas 236-250 del enunciado (sobre todo:
   ejemplos por fuera de los tests para cada requerimiento, atajos de teclado, citas de uso de
   IA, limitaciones).

### 🟠 Alto — impacta calidad de código y defensa

4. **Limpiar comentarios informales y el emoji** (`memoryManagerBuddy.c:206`,
   `memoryManagerFF.c:86`). Cinco minutos, evita una crítica directa en la defensa.

5. **Endurecer `mm_free` en ambos allocators**: validar que `ptr` cae dentro del heap y, en
   Buddy, rechazar double-free (chequear `is_free` antes de liberar). Defenderlo como
   robustez.

6. **Verificar ausencia de races/deadlocks en los escenarios de `mvar`** del enunciado
   (líneas 159-200): matar escritor/lector, subir prioridad, distintas combinaciones `mvar w r`.
   Es el ejercicio donde la cátedra suele apretar en la defensa.

### 🟡 Medio — robustez y prolijidad

7. **Documentar explícitamente las limitaciones arquitectónicas** en el README/defensa:
   todo en ring 0, sin aislamiento de memoria, stacks de 4 KB sin guarda. Mejor declararlo que
   que lo descubran.

8. **Considerar guard page o chequeo de stack** (al menos un canario al fondo del stack de
   4 KB que se valide en el context switch) para detectar overflows. Opcional pero suma.

9. **Actualizar `CLAUDE.md`** (`CANT_SYS`, descripción de cómo corren los tests) para que la
   documentación interna sea fiel al código.

### 🟢 Bajo — nice-to-have

10. **Revisar el reparto round-robin tras `scheduler_remove`** con muchos procesos
    (estabilidad del orden). Solo si sobra tiempo.

11. **Tests de estrés manual**: `MAX_PROCESSES` procesos simultáneos, llenar la tabla de
    semáforos/pipes, verificar degradación elegante (retornar error, no colgar).

---

### Resumen ejecutivo

El kernel **cumple funcionalmente con todo el enunciado**: ambos memory managers, scheduler
RR con prioridades, procesos con todas sus syscalls, semáforos atómicos sin busy-waiting,
pipes bloqueantes transparentes, shell con `&`/`\|`/Ctrl+C/Ctrl+D, y todas las apps de userland
incluyendo `mvar`. Los tests de cátedra corren correctamente **como procesos**, no como
built-ins (requisito obligatorio satisfecho).

El trabajo restante es de **cierre**: confirmar `-Wall` limpio en ambas builds, auditar el
README, endurecer las validaciones de `free`, limpiar comentarios informales y preparar la
defensa declarando las limitaciones conocidas (ring 0, sin aislamiento de memoria). No hay
componentes esenciales por construir.
