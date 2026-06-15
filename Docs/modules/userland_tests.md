# Userland Tests Module

## Overview (¿Qué es?)
El modulo de Pruebas de Usuario ([[userland_tests.md|Userland Tests]]) agrupa la suite de programas diseñados por la cátedra para estresar, validar y verificar la correctitud y robustez de los componentes centrales del Kernel (Memory Manager, Scheduler, Sincronización y Procesos). Corren como procesos estándar e independientes del espacio de usuario (Ring 3), interactuando con el sistema mediante la interfaz de syscalls.

## Functionality (¿Qué hace?)
- **`test_mm` (Memory Manager):** Valida la integridad, solapamientos de memoria y memory leaks mediante reservas (`malloc`) y liberaciones (`free`) continuas de tamaños aleatorios.
- **`test_proc` (Procesos):** Estresa la creación, bloqueo, desbloqueo y matado masivo de procesos para verificar la solidez de la tabla de PCBs y el planificador.
- **`test_prio` (Prioridades):** Verifica que el planificador respete el quántum asignado a cada proceso según sus niveles de prioridad (1 a 5).
- **`test_sync` (Sincronización):** Recrea condiciones de carrera sobre una variable compartida y demuestra cómo los semáforos garantizan la exclusión mutua de la sección crítica.

## Internal Mechanics (¿Cómo funciona?)
### 1. Resolución de Procesos por Nombre
Debido a que el sistema operativo ejecuta todos los binarios de usuario bajo un mismo espacio lógico de direcciones de memoria, la creación de procesos hijos no lee binarios de disco (ELF). En su lugar, el cargador de la shell (`Userland/c/syscall/syscall.c`) mantiene un registro estático `registry[]` que mapea un nombre (string) con el puntero a la función de C correspondiente (`endless_loop`, `endless_loop_print`, `zero_to_max`, `my_process_inc`). Al llamar a `my_create_process(name, ...)` la librería busca en esta lista y deriva al Kernel la dirección de la función real.

### 2. helpers Comunes (`test_util.c`)
- **PRNG de 32 bits:** Generador de números pseudoaleatorios *multiply-with-carry* (MWC) con semilla constante para garantizar la reproducibilidad de las pruebas. Utiliza solo aritmética de enteros debido a que el Kernel se compila sin SSE/x87.
- **`memcheck(start, value, size)`:** Verifica que todos los bytes del bloque valgan `value`. Actúa como la aserción principal de corrupción de memoria.
- **`bussy_wait(n)`:** Bucle ocupado de `n` ciclos que no cede la CPU, utilizado para simular carga real de cómputo.

### 3. Funcionamiento de cada Prueba
- **`test_mm <max_memory>`:** Ejecuta un ciclo `while(1)`. En cada ronda, realiza reservas aleatorias hasta alcanzar `MAX_BLOCKS = 128` bloques o la suma `max_memory`. Llena cada bloque `i` con el byte `i`. Verifica mediante `memcheck` que los datos sigan intactos (si hay solapamiento o error de metadata, aborta imprimiendo error). Libera todo y repite.
- **`test_proc <max_procs>`:** En un bucle, crea `max_procs` procesos `endless_loop`. Luego, realiza pasadas aleatorias: mata procesos (`my_kill`) y los cosecha con `my_wait` para evitar zombies, o los bloquea (`my_block`). En una segunda pasada, desbloquea (`my_unblock`) de forma aleatoria a los bloqueados.
- **`test_sync <n> <use_sem>`:** Inicializa una variable global `global = 0`. Lanza 4 procesos `my_process_inc` (2 de incremento, 2 de decremento) de `n` iteraciones. Cada uno modifica la variable mediante `slowInc` (que cede la CPU con `my_yield` un 30% de las veces en medio de la operación para maximizar el riesgo de condición de carrera).
  - Si `use_sem = 1`, encierra la llamada entre `my_sem_wait` y `my_sem_post` utilizando el semáforo `"sem"`. Al terminar, el principal hace wait de todos y verifica que `global == 0` (lo cual ocurre únicamente si los semáforos funcionan correctamente).
- **`test_prio <target>`:** Lanza tres hilos contadores en bucles ocupados. Realiza 3 fases:
  - Fase 1: Todos los procesos corren con la misma prioridad base.
  - Fase 2: Cambia prioridades a 1, 3 y 5 mediante `my_nice` tras crearlos. Comprueba que el de prioridad 5 termine primero.
  - Fase 3: Bloquea a los procesos, cambia la prioridad con `my_nice` y luego los desbloquea a todos juntos. Comprueba que la prioridad se aplique correctamente tras despertar del bloqueo.

## Key Files & Structs (Archivos y Estructuras Clave)
* **Archivos principales:**
  - `Userland/c/tests/test_mm.c`: Lógica de estresado de memoria.
  - `Userland/c/tests/test_proc.c`: Estresado de colas y transiciones de estados del scheduler.
  - `Userland/c/tests/test_sync.c`: Demostración de exclusión mutua de sección crítica.
  - `Userland/c/tests/test_prio.c`: Verificación de asignación de quanta por prioridades.
  - `Userland/c/tests/test_util.c` y `Userland/include/tests/test_util.h`: Funciones de números aleatorios, memcheck, satoi y bussy_wait.
* **Estructuras de datos:**
  - `registry[]` (en `syscall.c`): Tabla estática de nombres y funciones asociadas para spawn de procesos hijos.

## System Calls Relacionadas
Los programas de prueba invocan de forma masiva los wrappers lógicos de usuario `my_create_process`, `my_kill`, `my_block`, `my_unblock`, `my_nice`, `my_yield`, `my_wait`, `my_sem_open`, `my_sem_wait`, `my_sem_post` y `my_sem_close`.

## Comments and Limitations (Comentarios y Limitaciones)
- **Ejecución Infinita:** Las aplicaciones `test_mm` y `test_proc` nunca retornan por sí solas y se ejecutan en bucles infinitos. Se aconseja ejecutarlas en segundo plano (`test_mm 4096 &`) y detenerlas manualmente utilizando `kill <pid>` o mediante el atajo `Ctrl+C` si corren en primer plano.
- **Dependencia de Compilación en MM:** El resultado de `test_mm` validará el manejador que se encuentre compilado en ese momento (First Fit por defecto, o Buddy System si se compiló con `make buddy`).
