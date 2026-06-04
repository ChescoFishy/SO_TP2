# Userland Tests Module

## Overview (¿Qué es?)
El módulo de Tests agrupa todos los programas diseñados para llevar al límite (stress test) las funcionalidades del [[kernel_kernel.md|Kernel]]. Sirven como validación empírica de que componentes críticos como la administración de memoria, la planificación y sincronización, no tienen fugas (leaks) ni deadlocks.

## Functionality (¿Qué hace?)
- **Test [[kernel_memory_manager.md|Memory Manager]] (`test_mm`):** Aloja y libera repetidamente bloques de memoria de tamaños aleatorios, llenándolos con valores predecibles y luego validando que mantengan su contenido, para descubrir memory leaks o corrupción de metadata.
- **Test Processes (`test_processes`):** Crea y destruye procesos masivamente para verificar el correcto funcionamiento del [[kernel_process.md|Scheduler]], asignación de PCBs y Stack, asegurándose de que el sistema no colapse.
- **Test Priorities (`test_prio`):** Comprueba que los procesos cambian su prioridad correctamente, y que se les otorgan los tiempos de CPU (quantums) acordes.
- **Test Synchronization (`test_sync`):** Crea escenarios de competencia (Race Conditions) lanzando varios procesos que acceden a una misma variable compartida, probando el acceso sin protección vs con Semáforos para verificar la atómica de la solución.

## Internal Mechanics (¿Cómo funciona?)
1. Estos módulos operan interactuando intensivamente con el módulo `userland_syscall`, llamando a la API del [[kernel_kernel.md|Kernel]].
2. Contienen asserts implícitos (verificando que lo leído sea igual a lo escrito) y reportan "OK" o "ERROR" en pantalla.
3. Están diseñados para ejecutar largos bucles (loops) iterativos simulando cargas de trabajo complejas.

## Comments and Limitations
- **Limitaciones actuales:** Estos tests están definidos a alto nivel. No pueden verificar la fragmentación interna a menos que las primitivas de memoria provean info (ej: una syscall `sys_mem_status`).
- **Comentarios:** Es vital ejecutar toda la batería de tests después de cada cambio arquitectónico en el [[kernel_kernel.md|Kernel]] para asegurar que no se introdujeron regresiones o fallas de sincronización.
