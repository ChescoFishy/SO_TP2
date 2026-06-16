# Kernel Core Module

## Overview (¿Qué es?)
El módulo principal del [[kernel_kernel.md|Kernel]] es el punto de partida en C de la ejecución del sistema operativo. Orquesta la carga de los módulos lógicos (Userland), inicializa las variables globales (segmento BSS), configura las estructuras de interrupción (IDT) y arranca todos los subsistemas (Memoria, Procesos, Sincronización, Tuberías) antes de ceder el control al planificador de procesos.

## Functionality (¿Qué hace?)
- Es el destinatario del salto de ejecución del [[bootloader.md|Bootloader]] (en la dirección física `0x100000`).
- Copia y prepara en sus direcciones de ejecución virtuales los módulos del espacio de usuario (código y datos).
- Blanquea la sección BSS del binario del Kernel para asegurar la consistencia de variables globales no inicializadas.
- Llama de manera secuencial a los inicializadores de la IDT, del [[kernel_memory_manager.md|Memory Manager]], de la tabla de procesos, del planificador, de los semáforos y de los pipes.
- Crea el proceso del sistema `idle` y lo registra como proceso de fallback.
- Crea el proceso inicial de la `shell` como proceso foreground de usuario.
- Arranca el [[kernel_process.md|Scheduler]] para dar inicio a la multiprogramación preemptiva.

## Internal Mechanics (¿Cómo funciona?)
1. **Doble Fase de Arranque:**
   - **`initializeKernelBinary()`:** Es la primera función invocada (desde el cargador assembler del Kernel). Su primera tarea es deshabilitar las interrupciones por hardware (`_cli()`) de forma preventiva.
   - Llama a `loadModules()` que lee secuencialmente desde el final del binario del Kernel (`endOfKernelBinary`) y copia los ejecutables del espacio de usuario en las ubicaciones fijas:
     - `sampleCodeModuleAddress = 0x400000` (Binario ejecutable de Userland).
     - `sampleDataModuleAddress = 0x500000` (Datos de Userland).
   - Llama a `clearBSS()` para limpiar a cero la región de memoria de datos no inicializados.
   - Ejecuta `load_idt()` para armar la tabla de interrupciones, inicializar las máscaras del PIC (desenmascarando timer y teclado) y configurar el wrapper de syscalls.
   - Inicializa el [[kernel_memory_manager.md|Memory Manager]] llamando a `mm_init` sobre el bloque de memoria física del heap:
     - `HEAP_START = 0x600000`
     - `HEAP_SIZE = 8 MB`
   - Inicializa los subsistemas lógicos en orden: `process_init()`, `scheduler_init()`, `sem_init()`, `pipe_init()`.
   - Crea el proceso de respaldo del sistema `idle` ejecutando `idle_entry` (un ciclo infinito `while(1) _hlt()`), setea su prioridad en `MIN_PRIORITY = 1` y lo registra en el scheduler mediante `scheduler_set_idle(idle)`.
   - Crea el proceso principal `shell` apuntando a la dirección de código de usuario `0x400000` con privilegios de ejecución en primer plano (`foreground = 1`).
   - Retorna la dirección de la base de pila obtenida en `getStackBase()` para re-ajustar la pila de ejecución antes de la segunda fase.
2. **`main()` (Fase de Ejecución):**
   - Invocado inmediatamente tras retornar de `initializeKernelBinary`.
   - Ejecuta `scheduler_start()`, el cual selecciona al proceso `shell` (primer proceso READY en la cola de planificación), marca su estado como `PROCESS_RUNNING` y llama a `scheduler_start_asm` pasándole el puntero a su stack inicial para dar inicio al multitasking mediante el retorno por interrupción (`iretq`).

## Key Files & Structs (Archivos y Estructuras Clave)
* **Archivos principales:**
  - `Kernel/c/kernel/kernel.c`: Lógica de orquestación de arranque, definición de `initializeKernelBinary` y `main`.
  - `Kernel/c/kernel/moduleLoader.c`: Responsable de leer del disco los payloads y copiarlos en las direcciones fijas de memoria.
  - `Kernel/include/kernel/moduleLoader.h`: Definición de la interfaz del cargador de módulos.
* **Estructuras de datos:**
  - `moduleAddresses[]` (en `kernel.c`): Arreglo local estático que define las direcciones de destino de código (`0x400000`) y datos (`0x500000`) de Userland.
* **Funciones fundamentales:**
  - `initializeKernelBinary()`: Inicializador principal de BSS, IDT, MM, Scheduler e IPC.
  - `getStackBase()`: Calcula la base de pila limpia del Kernel sumando un offset de 8 páginas (`PageSize * 8`) sobre el final binario del Kernel (`endOfKernel`).
  - `main()`: Punto de inicio del sistema; arranca el scheduler.

## System Calls Relacionadas
No aplica de forma directa. El módulo principal no responde a llamadas al sistema directamente; es el responsable de configurar la IDT y el despachador (`syscallDispatcher.c`) que permite que Userland invoque al resto del Kernel.

## Comments and Limitations (Comentarios y Limitaciones)
- **Heap y Mapeos Estáticos:** El heap de memoria física se define estáticamente en la dirección `0x600000` con un tamaño inamovible de `8 MB`.
- **Deshabilitación Preventiva:** El sistema debe ejecutar `_cli()` durante toda la inicialización. Habilitar interrupciones antes del final de la configuración del planificador provocará fallos fatales inmediatos (triple fault) por la llegada asíncrona de ticks del timer.
- **Ausencia de Aislamiento Real de Páginas:** El sistema de paginación inicial mapea de forma identidad toda la memoria, lo que significa que el Kernel y los procesos de Userland comparten físicamente la base de direcciones del sistema, implementando aislamiento a nivel de APIs de asignación de stacks y variables.
