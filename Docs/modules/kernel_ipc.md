# Kernel IPC Module (Inter-Process Communication)

## Overview (¿Qué es?)
El módulo [[kernel_ipc.md|IPC]] (Comunicación entre Procesos) proporciona los mecanismos para que distintos procesos o hilos en ejecución puedan intercambiar datos o sincronizarse entre sí de manera segura y eficiente, rompiendo el aislamiento natural del espacio de memoria de cada proceso.

## Functionality (¿Qué hace?)
- Provee mecanismos de sincronización mediante **Semáforos**.
- Provee mecanismos de transferencia de datos unidireccional o bidireccional mediante **Pipes** (tuberías).
- Gestiona la creación, acceso (apertura), uso, y cierre (destrucción) de estas estructuras.
- Bloquea procesos cuando intentan leer de un pipe vacío o esperan un semáforo cerrado, e interactúa con el [[kernel_process.md|Scheduler]] para despertarlos.

## Internal Mechanics (¿Cómo funciona?)
1. **Pipes:** Típicamente implementado como un buffer circular en memoria compartida del [[kernel_kernel.md|Kernel]]. Tiene un extremo de lectura y uno de escritura. Cada extremo puede tener semáforos asociados o colas de espera en el [[kernel_process.md|Scheduler]] para gestionar la lectura/escritura bloqueante.
2. **Semáforos:** Estructuras de datos que mantienen un valor entero (contador) y una cola de procesos (PIDs) bloqueados a la espera. Emplea primitivas atómicas de CPU (como `xchg` o `lock xadd`) para evitar condiciones de carrera (Race Conditions) al decrementar/incrementar el valor.
3. **Integración:** Cuando un proceso hace `sem_wait()` y el valor es <= 0, el módulo [[kernel_ipc.md|IPC]] notifica al módulo Process/[[kernel_process.md|Scheduler]] para cambiar el estado de este proceso a BLOQUEADO y des-agendarlo. Al hacer `sem_post()`, si hay procesos en la cola, el módulo los pasa a estado LISTO.

## Comments and Limitations
- **Limitaciones actuales:** La cantidad máxima de Pipes y Semáforos puede estar limitada estáticamente por arreglos fijos en el [[kernel_kernel.md|Kernel]]. Los pipes pueden tener un tamaño de buffer fijo.
- **Comentarios:** La correctitud atómica es crítica en este módulo; un error de diseño puede llevar a Deadlocks o Memory Corruptions en el [[kernel_kernel.md|Kernel]].
