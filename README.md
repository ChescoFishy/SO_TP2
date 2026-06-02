# MASS OS — TP2 Sistemas Operativos (ITBA)

Kernel bare-metal x86-64 que corre directamente sobre QEMU o hardware real, sin sistema operativo subyacente. Implementa memoria, procesos, scheduling y syscalls desde cero.

## Requisitos

- Docker (único requisito en el host)
- QEMU (`qemu-system-x86_64`) para ejecutar la imagen

---

## Primer uso: crear el contenedor

```bash
./create.sh
```

Descarga la imagen `agodio/itba-so-multiarch:3.1` y crea el contenedor `TP_SO_2` con el directorio actual montado en `/root`. Solo es necesario hacerlo una vez.

---

## Compilar

```bash
./compile.sh           # compila con First-Fit (default)
./compile.sh vbox      # ídem, genera imagen VirtualBox (.vmdk)
```

### Selección del memory manager

El kernel soporta dos implementaciones de memoria, seleccionables en tiempo de compilación mediante la variable de entorno `MM`:

| Comando | Memory Manager |
|---------|---------------|
| `./compile.sh` | First-Fit (default) |
| `MM=FF ./compile.sh` | First-Fit (explícito) |
| `MM=BUDDY ./compile.sh` | Buddy System |

```bash
# Buddy System + imagen QEMU
MM=BUDDY ./compile.sh

# Buddy System + imagen VirtualBox
MM=BUDDY ./compile.sh vbox
```

> **First-Fit**: lista enlazada de bloques libres; busca el primer bloque que ajuste.
> **Buddy System**: bloques de potencia de 2; divide y fusiona en pares (`buddies`).

La selección es **exclusiva en compilación**: una imagen usa un único MM. Para comparar ambos hay que compilar y correr por separado.

---

## Ejecutar

```bash
./run.sh           # QEMU (busca .qcow2, fallback a .img)
./run.sh vbox      # muestra pasos para VirtualBox
./run.sh usb       # muestra comando dd para grabar en USB
```

**Permiso denegado en la imagen**: el contenedor corre como root, por lo que el `.qcow2` queda con permisos de root. Si `./run.sh` falla:

```bash
sudo chown $USER Image/x64BareBonesImage.qcow2
```

---

## Compilar y correr en un paso

```bash
./compile.sh && ./run.sh              # First-Fit
MM=BUDDY ./compile.sh && ./run.sh     # Buddy System
```

---

## Shell de usuario

Al bootear se inicia una shell interactiva (`> `). La shell parsea la línea,
crea procesos hijos para los comandos que son procesos y espera (`waitpid`) a
los de foreground antes de mostrar de nuevo el prompt.

### Operadores

| Operador | Significado |
|----------|-------------|
| `cmd &` | Ejecuta `cmd` en **background** (la shell no cede el foreground). |
| `cmd1 \| cmd2` | **Pipe**: conecta el stdout de `cmd1` al stdin de `cmd2`. Ambos lados deben ser comandos de tipo *proceso*. Se soporta un solo `\|` por línea. |

> Los **builtins** (corren sincrónicamente dentro de la shell) **no** admiten `&` ni `|`.

### Atajos de teclado

| Atajo | Acción |
|-------|--------|
| `Ctrl+C` | Mata el proceso en foreground. |
| `Ctrl+D` | Envía EOF al stdin del proceso en foreground. |
| `+` / `-` | Aumenta / disminuye el tamaño de fuente. |
| `Backspace` | Borra el último carácter de la línea. |

### Comandos *builtin* (corren en la shell)

| Comando | Parámetros | Descripción |
|---------|-----------|-------------|
| `help` | — | Lista todos los comandos y operadores. |
| `clear` | — | Limpia la pantalla. |
| `mem` | — | Estado de la memoria: total, usada, libre y bloques asignados. |
| `ps` | — | Lista procesos: PID, prioridad, foreground, estado y nombre. |
| `kill` | `<pid>` | Mata el proceso con ese PID. |
| `nice` | `<pid> <prioridad>` | Cambia la prioridad (1–5) de un proceso. |
| `block` | `<pid>` | Alterna el estado del proceso entre BLOCKED y READY. |
| `printTime` / `printDate` | — | Hora y fecha del sistema (UTC-3). |
| `registers` | — | Dump de registros (presionar `Ctrl` antes para capturarlos). |
| `testDiv0` / `invOp` | — | Disparan excepción #DE / #UD. |
| `playBeep` | — | Toca una melodía por el parlante. |
| `bmFPS` / `bmCPU` / `bmMEM` / `bmKEY` | — | Benchmarks. |

### Comandos *proceso* (admiten `&` y `|`)

| Comando | Parámetros | Descripción |
|---------|-----------|-------------|
| `loop` | `[ticks]` | Imprime su PID periódicamente con espera activa (default 18 ticks ≈ 1 s). |
| `cat` | — | Copia stdin a stdout hasta EOF. Funciona standalone (teclado + `Ctrl+D`) y con pipes. |
| `wc` | — | Cuenta las líneas recibidas por stdin. |
| `filter` | — | Reimprime stdin filtrando las vocales. |
| `mvar` | `<escritores> <lectores>` | Demo lectores/escritores con una MVar sincronizada por dos semáforos nombrados. Crea los hijos y termina de inmediato. |
| `test_mm` | `<max_memoria>` | Test del memory manager (ver abajo). |
| `test_proc` | `<max_procesos>` | Stress de creación/kill/block/unblock de procesos. |
| `test_prio` | `<valor_objetivo>` | Muestra diferencias de ejecución según prioridad. |
| `test_sync` | `<n> <use_sem>` | Lectores/escritores sobre una variable compartida. |

### Ejemplos

```text
> help                      # lista de comandos
> mem                       # estado de memoria
> loop &                    # lanza loop en background, imprime su PID
> ps                        # ver el PID del loop
> nice 3 5                  # subir la prioridad del PID 3 al máximo
> block 3                   # bloquear/desbloquear el PID 3
> kill 3                    # matar el loop
> cat | wc                  # escribir, terminar con Ctrl+D: imprime # de líneas
> cat | filter              # escribir vocales: se reimprime sin vocales
> mvar 2 2                  # 2 escritores y 2 lectores sobre la MVar
> test_mm 1000000           # test de memoria en foreground
> test_mm 1000000 &         # ídem en background
> test_sync 100 1           # con semáforos: el valor final siempre es 0
> test_sync 100 0           # sin semáforos: el valor final varía
> test_prio 1000000         # se ve que las prioridades altas terminan antes
> test_proc 5               # stress de procesos (Ctrl+C para cortar)
```

---

## Test del Memory Manager

Dentro de la shell:

```
test_mm 1000000
```

Es el test de la cátedra: en un bucle infinito pide bloques de tamaño aleatorio
hasta `max_memory`, los inicializa, **verifica su integridad** y los libera. Si
detecta una corrupción imprime `test_mm ERROR` y termina; mientras no imprima
nada, está pasando. Se corta con `Ctrl+C`. Puede correrse en background:
`test_mm 1000000 &`.

Para testear **ambas implementaciones** compilar y ejecutar por separado:

```bash
# First-Fit
./compile.sh && ./run.sh        # → dentro del OS: test_mm 1000000

# Buddy System
MM=BUDDY ./compile.sh && ./run.sh   # → dentro del OS: test_mm 1000000
```

---

## Limitaciones conocidas

- Se soporta **un solo `|`** por línea (no cadenas de varios pipes).
- Los **builtins** no admiten `&` ni `|`.
- No existe `sys_dup2`: la shell conecta los pipes asignando stdin/stdout del
  hijo al crearlo (`sys_create_process_fd`).
- Los procesos **background** que terminan quedan como ZOMBIE hasta llenar la
  tabla (no hay reaper de huérfanos); el límite es `MAX_PROCESSES = 64`.
- Solo se registra **un** proceso esperando teclado a la vez (en la práctica
  solo el foreground lee de stdin).
- Sin paginación: todos los procesos comparten el mismo espacio de direcciones.

---

## Limpiar artefactos

```bash
make clean
```

---

## Uso de IA

Durante el desarrollo se utilizó **Claude Code** (Anthropic) como asistente para
revisión de código, detección de bugs y redacción de documentación/comentarios.
Las decisiones de diseño, la integración y la verificación finales fueron
realizadas y validadas por los integrantes del grupo.
