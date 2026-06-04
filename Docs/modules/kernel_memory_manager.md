# Kernel Memory Manager Module

## Overview (¿Qué es?)
El módulo de [[kernel_memory_manager.md|Memory Manager]] (Manejador de Memoria) es responsable de administrar de forma dinámica la memoria libre del sistema. Provee la capacidad a procesos y al propio [[kernel_kernel.md|Kernel]] de solicitar y liberar bloques de memoria en tiempo de ejecución.

## Functionality (¿Qué hace?)
- Realiza el seguimiento de qué partes de la memoria principal (RAM) están libres y cuáles ocupadas.
- Satisface pedidos de alocación (malloc/alloc) de un tamaño arbitrario.
- Permite la liberación (free) de bloques previamente alocados para su reúso.
- Maneja fragmentación y maximiza el aprovechamiento del espacio disponible.

## Internal Mechanics (¿Cómo funciona?)
1. **Inicialización:** Durante el arranque, el [[kernel_kernel.md|Kernel]] lee cuanta memoria total y libre dispone (provisto por el [[bootloader.md|Bootloader]]) y designa un rango de direcciones físicas grande como "heap".
2. **Estructuras de Control:** Utiliza estructuras lógicas para administrar los bloques libres. Generalmente en este nivel se implementa usando algoritmos como **Buddy Allocation System** (que divide bloques en mitades potencias de 2) o un manejador de listas enlazadas **Free List** (con algoritmos First Fit / Best Fit).
3. **Paginación (Opcional en SO básicos):** En algunas implementaciones puede integrarse con el mecanismo de MMU (Memory Management Unit) para devolver páginas virtuales (4KB) mapeadas a frames físicos en demanda, aunque muchos OS de TP universitarios operan con *identity mapping* o solo gestionan direcciones base fijas.

## Comments and Limitations
- **Limitaciones actuales:** Dependiendo del algoritmo (Buddy o Free List), puede existir un grado considerable de fragmentación interna o externa. A veces no se provee protección de memoria estricta (Ring 0 vs Ring 3) por página si todo está mapeado igual.
- **Comentarios:** Es el subsistema base de todo lo dinámico en el SO (creación de procesos, [[kernel_ipc.md|IPC]], etc). Cualquier corrupción en los punteros del [[kernel_memory_manager.md|Memory Manager]] usualmente deriva en un Page Fault masivo.
