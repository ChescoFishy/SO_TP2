# Kernel Memory Manager Module

## Overview (¿Qué es?)
El módulo de [[kernel_memory_manager.md|Memory Manager]] (Manejador de Memoria) es el componente responsable de administrar de forma dinámica la memoria libre del heap del sistema. Proporciona una interfaz unificada y transparente para reservar y liberar bloques de memoria en tiempo de ejecución, aislando a los subsistemas del Kernel de la lógica de asignación subyacente. El sistema cuenta con dos manejadores independientes seleccionables en tiempo de compilación: **First Fit (FF)** y **Buddy System**.

## Functionality (¿Qué hace?)
- Administra el bloque de memoria física designado como heap (de `8 MB` ubicado en `0x600000`).
- Satisface pedidos de alocación de memoria de tamaño arbitrario (`mm_malloc` para usuarios y `mm_malloc_kernel` para estructuras internas del Kernel).
- Permite la liberación voluntaria (`mm_free`) de bloques previamente asignados para su posterior reutilización.
- Reporta el estado actual de la memoria (memoria total, libre, ocupada y cantidad de bloques activos) mediante `mm_status`.
- Ofrece transparencia absoluta: el resto del sistema interactúa con la misma firma de funciones sin importar qué administrador esté activo.

## Internal Mechanics (¿Cómo funciona?)
La selección del administrador se realiza exclusivamente en **tiempo de compilación** mediante la variable `MM` en el Makefile (`MM=BUDDY` o `MM=FF`). Por defecto se compila First Fit, y con `make buddy` se activa Buddy System. Ambos implementan la interfaz descrita en `memoryManager.h`.

### 1. Algoritmo First Fit
- **Estructura:** La memoria se organiza como una lista simplemente enlazada de bloques libres y ocupados. Cada bloque es precedido por un encabezado `MemBlock` (metadata).
- **Asignación:** Al solicitar memoria, se alinea el tamaño a múltiplos de 8 bytes para evitar accesos desalineados de hardware. Se recorre la lista desde el inicio (`heap_start`) buscando el primer bloque que esté libre (`is_free == 1`) y que tenga un tamaño suficiente (`size >= solicitado`).
- **División (Split):** Si al seleccionar un bloque sobra espacio suficiente para alojar un bloque útil nuevo (mínimo `sizeof(MemBlock) + 8` bytes, constante `MIN_SPLIT`), el bloque se divide (split): la porción sobrante se transforma en un nuevo bloque libre insertado en la lista.
- **Liberación y Coalescencia:** Al liberar un bloque (`mm_free`), se marca `is_free = 1`. Inmediatamente se realiza una coalescencia hacia adelante (si el bloque siguiente está libre, se absorbe sumando su tamaño) y hacia atrás (se busca el bloque previo, y si está libre, absorbe al bloque actual), combatiendo la fragmentación externa.

### 2. Algoritmo Buddy System
- **Estructura:** La memoria se divide en particiones binarias de potencias de 2. El tamaño mínimo de bloque es $2^{4} = 16$ bytes (`MIN_ORDER`, que deja 8 bytes para metadata y 8 bytes para payload útil) y el máximo es $2^{23} = 8$ MB (`MAX_ORDER`). Se mantiene un arreglo de listas enlazadas `free_lists[ORDERS]` donde cada entrada agrupa los bloques libres del orden correspondiente.
- **Inicialización:** Descompone el heap de manera codiciosa (*greedy*) de mayor a menor orden, agregando bloques a las respectivas listas libres.
- **Asignación:** Calcula el orden óptimo necesario para alojar el tamaño pedido más el encabezado (`BlockHdr`). Busca un bloque libre partiendo de ese orden hacia arriba.
  - Si encuentra un bloque de orden mayor, lo extrae de su lista y lo subdivide sucesivamente a la mitad (generando "buddies" o compañeros) hasta alcanzar el orden solicitado. Los buddies sobrantes de órdenes menores se registran en sus correspondientes listas libres.
- **Liberación y Fusión:** Al liberar, se marca el bloque como libre y se calcula la dirección de su compañero (*buddy*) usando operaciones a nivel de bits:
  $$\text{Buddy Offset} = \text{Offset de Bloque} \oplus 2^{\text{orden}}$$
  Si el buddy se encuentra libre, está dentro de los límites del heap, y es del mismo orden, se lo remueve de su lista libre y se los fusiona en un único bloque del orden inmediatamente superior. Este proceso se repite recursivamente hacia arriba hasta encontrar un compañero ocupado o alcanzar el `MAX_ORDER`.

## Key Files & Structs (Archivos y Estructuras Clave)
* **Archivos principales:**
  - `Kernel/include/memoryManager/memoryManager.h`: Interfaz unificada del asignador de memoria y definición de `MemStatus`.
  - `Kernel/c/memoryManager/memoryManagerFF.c`: Implementación del administrador First Fit con lista enlazada y coalescencia.
  - `Kernel/c/memoryManager/memoryManagerBuddy.c`: Implementación del administrador Buddy System con listas enlazadas por órdenes y fusión por XOR.
  - `Kernel/Makefile`: Regla de selección condicional del archivo fuente a compilar según la variable de entorno `MM`.
* **Estructuras de datos:**
  - `MemStatus`: Utilizada para reportar el estado de la memoria a Userland.
  - `struct MemBlock` (en `memoryManagerFF.c`):
    ```c
    typedef struct MemBlock {
        uint64_t size;              /* Tamaño del payload útil (sin header) */
        int is_free;                /* Flag de estado */
        int is_kernel;              /* 1 si fue asignado por mm_malloc_kernel */
        struct MemBlock* next;      /* Puntero al siguiente bloque del heap */
    } MemBlock;
    ```
  - `BlockHdr` / `FreeNode` (en `memoryManagerBuddy.c`):
    ```c
    typedef struct {
        uint8_t order;      /* Potencia de 2 del tamaño total del bloque */
        uint8_t is_free;    /* 1 libre, 0 ocupado */
        uint8_t is_kernel;  /* 1 si fue asignado por mm_malloc_kernel */
        uint8_t _pad[5];    /* Relleno de alineación a 8 bytes */
    } BlockHdr;
    ```
* **Funciones fundamentales:**
  - `mm_init(void *start, uint64_t size)`: Configura los límites y estructuras iniciales del heap.
  - `mm_malloc(uint64_t size)`: Reserva memoria de uso general para usuario.
  - `mm_malloc_kernel(uint64_t size)`: Reserva memoria para estructuras internas del Kernel sin incrementar el recuento de bloques del usuario.
  - `mm_free(void *ptr)`: Libera un puntero.

## System Calls Relacionadas
- **`sys_malloc` (Syscall 16):** Permite a los procesos de Userland solicitar memoria dinámica.
- **`sys_free` (Syscall 17):** Libera memoria dinámica reservada por el proceso.
- **`sys_mem_status` (Syscall 18):** Copia una estructura `MemStatus` con los totales de memoria del sistema para monitoreo.

## Comments and Limitations (Comentarios y Limitaciones)
- **Tamaño Rígido y Sin Paginación Dinámica:** La memoria física asignada al heap es fija de `8 MB` a partir de `0x600000`. No existe un mecanismo de memoria virtual paginada que expanda el heap bajo demanda ni aislamiento de páginas por hardware (Ring 0 vs Ring 3 comparten espacio).
- **Fragmentación Interna en Buddy:** El Buddy System redondea las solicitudes a potencias de 2, lo que puede provocar un desperdicio considerable de memoria (hasta un 50% de fragmentación interna en el peor de los casos).
- **Cálculo de alloc_count:** El uso de `mm_malloc_kernel` evita que las estructuras internas de los procesos o semáforos distorsionen el reporte de asignaciones del usuario en la syscall `sys_mem_status`.
