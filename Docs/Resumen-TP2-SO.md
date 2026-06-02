# Resumen y Diagnóstico del Sistema Operativo (TP2)

Como Ingeniero de Sistemas y Arquitecto de Sistemas Operativos, he analizado el código fuente de la implementación del SO provista para el TP2. A continuación, presento un reporte detallado del estado actual del proyecto, su arquitectura, y los pasos a seguir.

## 1. Diagnóstico Actual

**Arquitectura:** 
El sistema implementa una **arquitectura monolítica** de 64 bits. El núcleo (kernel) y los servicios principales (gestión de procesos, memoria, IPC y drivers) se ejecutan en el mismo espacio de direcciones y nivel de privilegios (típicamente Ring 0 en este tipo de implementaciones bare-bones). Se ha logrado la separación lógica entre el código del kernel y los binarios de espacio de usuario (Userland), interactuando exclusivamente a través de un despachador de llamadas al sistema (*syscalls*).

**Componentes construidos y su interacción:**
*   **Gestor de Memoria (Memory Manager):** Se implementaron dos enfoques intercambiables (Buddy System y First-Fit), aislando la lógica detrás de una interfaz común. El kernel los utiliza para reservar memoria dinámica (ej. stacks de los procesos).
*   **Planificador (Scheduler):** Implementa un algoritmo *Round-Robin con Prioridades*. Mantiene una tabla de procesos estática (`process_table`) y una cola de procesos listos (`run_queue`). Interacciona íntimamente con el despachador de interrupciones del timer.
*   **Drivers:** Se cuenta con drivers de Video (para salida en pantalla) y Teclado (con manejo de buffers para la lectura asincrónica de caracteres). Adicionalmente, hay un driver básico de Sonido.
*   **Sincronización e IPC:** Se desarrollaron semáforos nombrados (`semaphore.c`) y pipes unidireccionales nombrados (`pipe.c`). Estos interactúan con el scheduler para bloquear y despertar procesos (*wait*/*post*).
*   **Userland / Aplicaciones:** Contiene una shell muy completa (`sh`) que soporta ejecución en background (`&`), tuberías (`|`), EOF (`Ctrl+D`) y kill de foreground (`Ctrl+C`). Las aplicaciones requeridas por el enunciado (`ps`, `mem`, `loop`, `kill`, `nice`, `block`, `cat`, `wc`, `filter`, `mvar`) están integradas como "built-ins" o procesos lanzados por la shell.

## 2. Flujo de Ejecución

El control y los cambios de contexto en el sistema operativo fluyen de la siguiente manera:

1.  **Arranque y Setup:** El sistema es cargado por el bootloader, se inicializan las interrupciones (IDT), los drivers, el gestor de memoria, la tabla de procesos y el scheduler. Finalmente, el kernel lanza el primer proceso (usualmente la shell en Userland) configurando el stack inicial simulando que acaba de ser interrumpido, y ejecuta un `iretq` para saltar a su código.
2.  **Manejo de Interrupciones y Context Switching:**
    *   **Timer Tick (IRQ0):** Periódicamente, el hardware interrumpe el procesador. El handler en assembly (`interrupts.asm`) guarda el estado completo de los registros (`pushState`), y llama a `scheduler_tick` en C pasándole el puntero al stack (RSP) actual.
    *   **Decisión del Scheduler:** En `scheduler_tick`, se decrementa el *quantum* del proceso en ejecución. Si se agota, su estado cambia a `READY` y la CPU se asigna al próximo proceso en la cola (el cual pasa a `RUNNING`). Se retorna el RSP del nuevo proceso.
    *   **Restauración de Contexto:** El handler en assembly actualiza el RSP, restaura los registros del nuevo proceso (`popState`) y ejecuta `iretq`, retomando la ejecución exactamente donde se quedó.
3.  **Llamadas al Sistema (Syscalls):** Cuando un proceso necesita un recurso del kernel (ej. leer del teclado o escribir en un pipe), ejecuta una interrupción por software (`int 0x80`). El manejador llama a `syscallDispatcher`, el cual enruta la petición. Si la operación es bloqueante (ej. leer un pipe vacío o hacer `wait` en un semáforo cerrado), el kernel cambia el estado del proceso a `BLOCKED` y fuerza un cambio de contexto voluntario llamando a `scheduler_yield_impl`.

## 3. Mapa de Agujeros (Qué Falta o Incompletitudes)

A nivel de requerimientos del **TP2**, el proyecto está sumamente completo y cumple con la práctica totalidad del enunciado. Sin embargo, desde una perspectiva de arquitectura avanzada, existen oportunidades de mejora y componentes fundamentales "hardcodeados" o faltantes:

*   **Límites Estáticos (Hardcodeados):** 
    *   Las tablas principales del kernel tienen tamaños fijos definidos por macros: `MAX_PROCESSES`, `MAX_PIPES`, `MAX_SEMAPHORES`. En un SO moderno, estas estructuras se gestionan dinámicamente utilizando el Memory Manager.
    *   El tamaño del buffer de los pipes (`PIPE_BUF_SIZE`) está hardcodeado.
*   **Falta de Aislamiento de Memoria (Ring 3 vs Ring 0):** Aunque los binarios estén separados lógicamente, los procesos de usuario no operan bajo protección de memoria virtual. Todos los procesos comparten el mismo espacio de direcciones y pueden modificar la memoria del kernel u otros procesos. El caso práctico es el comando `mvar`, que utiliza una variable global compartida en Userland, demostrando la ausencia de espacios de memoria virtual independientes.
*   **Sistema de Archivos (VFS):** Faltan las abstracciones de descriptores de archivo (File Descriptors) universales. Si bien los pipes se manejan con pseudo-FDs (índices), no existe un sistema de archivos persistente o un Virtual File System (VFS) donde "todo es un archivo".
*   **Liberación de Recursos Huérfanos:** Si un proceso muere abruptamente, no hay un mecanismo robusto tipo "Garbage Collector" en el SO que limpie semáforos abiertos o cierre pipes de forma general (aunque existe una función de limpieza en `release_pcb_resources`, pero los semáforos siguen siendo globales).

## 4. Próximos Pasos (Backlog Priorizado)

Para llevar este sistema operativo al siguiente nivel (ideal para un TP3 o evolución natural del proyecto), sugiero el siguiente Backlog ordenado desde lo más crítico hacia lo secundario:

**Prioridad 1: Seguridad y Estabilidad**
1.  **Aislamiento y Memoria Virtual (Paging):** Implementar la tabla de páginas para independizar los espacios de direcciones de cada proceso de usuario. Esto evitará variables globales compartidas inadvertidamente y protegerá la memoria del kernel contra accesos indebidos de las aplicaciones.
2.  **Transición a Ring 3:** Configurar los segmentos en la GDT y TSS para que los procesos de usuario corran en modo no privilegiado (Ring 3), restringiendo el uso de instrucciones críticas (como `cli`, `sti`, `hlt`).

**Prioridad 2: Flexibilidad y Abstracciones Core**
3.  **Dynamic Kernel Structures:** Modificar las tablas estáticas (`process_table`, `pipes`, `semaphores`) para que el kernel las asigne dinámicamente empleando el `mm_malloc_kernel`. Esto eliminará los cuellos de botella arbitrarios impuestos por el tamaño fijo de los arrays.
4.  **Sistema de Archivos (VFS) Básico:** Diseñar una capa de *Virtual File System* donde teclado, pantalla, pipes y archivos de texto compartan una misma estructura `file_descriptor`. De esta manera, el SO tendrá una tabla unificada de archivos abiertos por proceso.

**Prioridad 3: Mejoras al Planificador e IPC**
5.  **Scheduler con Colas Multinivel (MLFQ):** Evolucionar el algoritmo de Round-Robin hacia uno que penalice procesos CPU-Bound y premie procesos I/O-Bound, brindando mayor fluidez a la interfaz de usuario.
6.  **Señales (Signals):** Extender el mecanismo actual de *kill* (y *Ctrl+C*) hacia un sistema estándar de señales POSIX (SIGINT, SIGTERM, SIGKILL) permitiendo a los procesos registrar manejadores propios.

**Prioridad 4: Mejoras de Usuario y Shell**
7.  **Redirección de I/O en la Shell:** Sumar operadores `<`, `>`, `>>` a la shell para redirigir la entrada y salida de los procesos, aprovechando el nuevo esquema de File Descriptors del paso 4.
8.  **Manejo de Variables de Entorno:** Integrar conceptos básicos de variables de entorno para los procesos, útiles para futuros scripts o comandos enriquecidos.
