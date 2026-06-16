# Tests de cátedra — MASS OS

> Documentación de los tests implementados en `Userland/c/tests/`. Explica **qué valida**
> cada uno y **cómo funciona** internamente, paso a paso.

---

## Generalidades

Los tests **no son built-ins de la shell**: son **procesos reales** (con su propio PCB y
stack), registrados en la tabla de comandos de `Userland/c/syscall/syscall.c:38`. Por eso
admiten ejecución en background (`&`) y, en principio, conexión por pipes (`|`), igual que
`cat`, `wc` o `loop`.

| Comando | Uso | Termina solo | Qué valida |
|---|---|---|---|
| `test_mm`   | `test_mm <max_memory>`   | ❌ loop infinito | Memory manager: integridad y ausencia de solapamientos |
| `test_proc` | `test_proc <max_procs>`  | ❌ loop infinito | Scheduler / gestión de PCBs bajo create/kill/block/unblock masivos |
| `test_sync` | `test_sync <pares> <iteraciones> <use_sem>`| ✅ | Sincronización: race condition vs. semáforos |
| `test_prio` | `test_prio <target>`     | ✅ | Prioridades del scheduler |

> ⚠️ `test_mm` y `test_proc` corren en un `while(1)` y **nunca retornan por sí solos**:
> hay que terminarlos con `Ctrl+C` (si están en foreground) o con `kill <pid>`. Conviene
> lanzarlos en background (`test_mm 4096 &`) para no perder la shell.

### Cómo se resuelve el nombre de un proceso

Los tests crean sus procesos hijos por **nombre** (string), no por puntero a función.
`my_create_process(name, argc, argv)` (`Userland/c/syscall/syscall.c:37`) busca ese nombre
en un *registry* estático y, si lo encuentra, llama a `sys_create_process` con el puntero a
función correspondiente:

```c
static const struct { const char *name; void *fn; } registry[] = {
    {"endless_loop",       endless_loop},        /* dummy: while(1); */
    {"endless_loop_print", endless_loop_print},  /* dummy: imprime su PID en loop */
    {"zero_to_max",        zero_to_max},          /* worker de test_prio */
    {"my_process_inc",     my_process_inc},       /* worker de test_sync */
};
```

Esto es necesario porque, al compartir un único espacio de direcciones, el "binario" de un
proceso hijo es simplemente una función ya cargada en memoria; el nombre es la forma de
seleccionarla.

---

## Infraestructura común — `test_util.c`

Todos los tests se apoyan en helpers de `Userland/c/tests/test_util.c`:

- **PRNG `GetUint()` / `GetUniform(max)`** — generador *multiply-with-carry* (MWC) de 32 bits
  con estado fijo inicial (semilla constante → secuencia **reproducible** entre corridas).
  `GetUniform(max)` devuelve un valor en `[0, max)` con aritmética entera (el kernel se
  compila sin x87/SSE, así que no se puede usar punto flotante).
- **`memcheck(start, value, size)`** — recorre `size` bytes desde `start` y devuelve `1` solo
  si **todos** valen `value`. Es el "assert" que usa `test_mm` para detectar corrupción.
- **`satoi(str)`** — convierte string a `int64_t` con signo; devuelve `0` ante cualquier
  carácter no numérico (así se validan los argumentos).
- **`bussy_wait(n)`** — busy-loop de `n` iteraciones (retardo activo, sin ceder CPU).
- **`endless_loop` / `endless_loop_print`** — procesos *dummy* usados como carga:
  el primero es `while(1);`, el segundo imprime su PID periódicamente con un retardo.
- **`printf`** — implementación mínima (`%d %u %s %c %%`) que escribe vía `putchar`.

---

## `test_mm` — Memory Manager

**Uso:** `test_mm <max_memory>` (bytes máximos a reservar por iteración).
**Archivo:** `Userland/c/tests/test_mm.c`.

### Qué hace
Somete al [memory manager](modules/kernel_memory_manager.md) (First-Fit o Buddy, según con
qué `MM` se compiló) a un ciclo continuo de `malloc`/`free` con tamaños aleatorios,
verificando que cada bloque conserve intacto su contenido. Detecta **leaks**, **corrupción de
metadata** y **solapamiento** entre bloques.

### Cómo funciona (por iteración del `while(1)`)
1. **Reserva:** pide bloques de tamaño aleatorio (`GetUniform`) acumulando hasta llenar
   `MAX_BLOCKS = 128` punteros o hasta que la suma alcance `max_memory`. Guarda dirección y
   tamaño de cada uno en el arreglo `mm_rqs[]`. Los `malloc` que devuelven `NULL` se descartan.
2. **Escritura:** rellena cada bloque `i` con el byte `i` (`memset(addr, i, size)`).
   Como `MAX_BLOCKS < 256`, cada bloque queda con un "sello" único.
3. **Verificación:** con `memcheck(addr, i, size)` comprueba que el bloque `i` siga conteniendo
   solo el byte `i`. Si algún byte cambió → otro bloque pisó esta región (overlap/corrupción):
   imprime `test_mm ERROR` y sale con `sys_exit(-1)`.
4. **Liberación:** libera todos los bloques y vuelve a empezar.

### Resultado esperado
**Silencio.** Mientras no imprima `test_mm ERROR`, el allocator es correcto. Para comparar
First-Fit vs. Buddy hay que **recompilar** con `MM=BUDDY ./compile.sh` y reejecutar (el
allocator es un switch de compilación, no de runtime).

---

## `test_proc` — Procesos y scheduler

**Uso:** `test_proc <max_procs>` (cantidad de procesos vivos por ronda).
**Archivo:** `Userland/c/tests/test_proc.c` (función interna `test_processes`).

### Qué hace
Estresa la creación/destrucción de procesos y las transiciones de estado del
[scheduler](modules/kernel_process.md): comprueba que crear, matar, bloquear y desbloquear
procesos en masa y de forma aleatoria no rompa la tabla de PCBs ni cuelgue el sistema.

### Cómo funciona
1. Reserva en stack un arreglo `p_rqs[max_processes]`, cada entrada con `{pid, state}` donde
   `state ∈ {RUNNING, BLOCKED, KILLED}`.
2. **Crear:** lanza `max_processes` procesos `endless_loop` (un `while(1);` que no hace nada
   salvo consumir quantum). Si algún `create` devuelve `-1`, reporta error y aborta.
3. **Caos aleatorio:** mientras queden procesos vivos (`alive > 0`), recorre el arreglo y para
   cada proceso elige al azar (`GetUniform(100) % 2`):
   - **Matar** (`my_kill`) si está `RUNNING` o `BLOCKED` → marca `KILLED`, hace `my_wait`
     (cosecha el zombie) y decrementa `alive`.
   - **Bloquear** (`my_block`) si está `RUNNING` → marca `BLOCKED`.
   Luego, en una segunda pasada, **desbloquea** (`my_unblock`) al azar los que están `BLOCKED`.
4. Cuando todos murieron, **vuelve a empezar** (loop infinito): crea otra tanda y repite.

Cualquier syscall que falle (`my_kill`/`my_block`/`my_unblock == -1`) provoca un mensaje de
error y el aborto del test.

### Resultado esperado
Que corra indefinidamente **sin colgarse ni imprimir errores**. Se termina con `Ctrl+C` o
`kill`. Útil para verificar que no haya fugas de PCBs (crear/destruir miles de procesos sin
agotar la tabla `MAX_PROCESSES`).

---

## `test_sync` — Sincronización (semáforos)

**Uso:** `test_sync <pares> <iteraciones> <use_sem>` — `pares` = cantidad de pares de procesos a
crear (se lanzan `2 × pares`); `iteraciones` = veces que cada worker incrementa/decrementa la
variable compartida; `use_sem` = `1` usa semáforo, `0` no.
**Archivo:** `Userland/c/tests/test_sync.c`.

### Qué hace
Recrea de forma deliberada una **condición de carrera** sobre una variable global compartida y
demuestra que **sin** semáforo el resultado es impredecible, mientras que **con** semáforo es
siempre `0` (la sección crítica queda protegida).

### Cómo funciona
1. **Valida los argumentos:** `pares` e `iteraciones` deben ser `> 0` y `use_sem ∈ {0, 1}`;
   además `2 × pares` no puede superar `MAX_PROCESSES - 2`. Reserva con `sys_malloc` el arreglo de
   PIDs (`2 × pares` entradas); si la reserva falla, aborta.
2. Si `use_sem = 1`, **limpia** cualquier instancia previa del semáforo (`my_sem_close` en bucle)
   y abre el semáforo nombrado `"sem"` con valor inicial `1`.
3. Inicializa `global = 0` (variable global → compartida, porque hay un único espacio de
   direcciones para todo userland).
4. Crea `2 × pares` procesos `my_process_inc`, uno decrementador (`inc = -1`) y uno incrementador
   (`inc = +1`) por par. Cada worker recibe `(iteraciones, inc, use_sem)` y ejecuta `iteraciones`
   veces la operación. Si algún `create` falla, mata los procesos ya creados, cierra el semáforo,
   libera el arreglo de PIDs y aborta.
5. Cada worker, en su bucle, hace `slowInc(&global, inc)`, que es un incremento **no atómico
   a propósito**: lee `global` en una variable local, con 30 % de probabilidad cede la CPU
   (`my_yield`) **en medio** de la operación, luego escribe de vuelta. Ese `yield` intercalado
   maximiza la chance de que dos procesos lean el mismo valor viejo y se pisen. Si `use_sem = 1`,
   cada worker abre el mismo semáforo `"sem"` y envuelve `slowInc` entre `my_sem_wait` /
   `my_sem_post`, serializando los accesos.
6. El proceso principal hace `my_wait` de los `2 × pares` hijos, imprime `Valor final: <global>`,
   cierra el semáforo y libera el arreglo de PIDs.

### Resultado esperado
- `test_sync <pares> <iteraciones> 1` → **`Valor final: 0`** siempre (igual cantidad de
  incrementos y decrementos se cancelan; el semáforo garantiza atomicidad).
- `test_sync <pares> <iteraciones> 0` → **valor distinto de 0** (con `iteraciones` grande, p. ej.
  `1000`), evidenciando la condición de carrera. El valor exacto varía, pero rara vez es `0`.

---

## `test_prio` — Prioridades del scheduler

**Uso:** `test_prio <target>` (`target` = hasta cuánto cuenta cada worker; debe ser distinto
de `0`).
**Archivo:** `Userland/c/tests/test_prio.c`.

### Qué hace
Verifica que el scheduler respete las prioridades: a igual prioridad los procesos progresan de
forma pareja, y al subir la prioridad de uno, ese termina **primero** (recibe más quantum por
unidad de tiempo). Las prioridades válidas son `1..5`; el test usa `LOWEST=1`, `MEDIUM=3`,
`HIGHEST=5`.

### Cómo funciona
El worker es `zero_to_max`: cuenta de `0` a `target` con un bucle ocupado y al terminar imprime
`PROCESS <pid> DONE!`. El orden en que aparecen esos mensajes revela quién terminó antes. El
test corre **tres fases** con 3 procesos cada una:

1. **Misma prioridad** — crea 3 `zero_to_max` (todos con la prioridad por defecto) y los espera.
   Deberían terminar de forma entrelazada/pareja.
2. **Cambiar prioridad después de crear** — crea los 3 y, a cada uno, le asigna una prioridad
   distinta con `my_nice` (1, 3 y 5). El de prioridad `5` debería imprimir `DONE!` primero y el
   de `1`, último.
3. **Cambiar prioridad mientras están bloqueados** — crea cada proceso, lo **bloquea**
   (`my_block`) y recién entonces le cambia la prioridad; luego los **desbloquea** a todos
   (`my_unblock`) y los espera. Comprueba que el cambio de prioridad sobre un proceso bloqueado
   se aplique correctamente al reanudarse.

### Resultado esperado
En las fases 2 y 3, el orden de los `PROCESS <pid> DONE!` debe correlacionar con la prioridad:
**mayor prioridad → termina antes**. La fase 1 sirve de línea de base (sin sesgo de prioridad).

---

## Resumen de ejecución

```sh
# Memory manager (infinito → background + kill)
test_mm 8192 &
# ... dejarlo correr; luego:
kill <pid>

# Procesos (infinito → background + kill)
test_proc 10 &

# Sincronización (terminan solos)
test_sync 2 1000 1     # → Valor final: 0
test_sync 2 1000 0     # → Valor final: != 0 (race condition)

# Prioridades (termina solo)
test_prio 1000000
```

Para probar **ambos allocators** con `test_mm`, recompilar con cada `MM` y reejecutar:

```sh
./compile.sh            # First-Fit (default)
MM=BUDDY ./compile.sh   # Buddy System
```
