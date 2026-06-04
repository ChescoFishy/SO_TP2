# Kernel Process Module (Scheduler)

## Overview (¿Qué es?)
El módulo de Procesos o [[kernel_process.md|Scheduler]] (Planificador) es el encargado del pseudo-paralelismo o la multitarea del sistema operativo. Administra el concepto abstracto de "Proceso" o "Hilo", permitiendo que múltiples flujos de ejecución compartan el uso de la CPU.

## Functionality (¿Qué hace?)
- Define e implementa la estructura PCB (Process Control Block) para mantener el estado de un proceso (registros, PID, estado, stack, etc).
- Permite crear nuevos procesos (exec) recibiendo un puntero a función o un binario.
- Mata y libera recursos de procesos finalizados (exit).
- Cambia de contexto (Context Switch) equitativamente entre los procesos que están listos para ejecutar (Ready).
- Gestiona los estados de los procesos (Running, Ready, Blocked, Zombie/Killed).

## Internal Mechanics (¿Cómo funciona?)
1. **Creación:** Al crearse un proceso, se asigna memoria para su Stack, se inicializa su stack frame falseando que ha sido "interrumpido", seteando `RIP` a la primera instrucción que deba ejecutar y `RSP` apuntando a este nuevo bloque.
2. **Planificación:** Cada vez que dispara el Timer Tick (por interrupción de tiempo), se invoca al [[kernel_process.md|Scheduler]]. Éste decide si el proceso actual consumió su quantum (Round Robin).
3. **Context Switch:** De ser necesario un cambio, guarda el `RSP` del proceso actual en su PCB y recupera el `RSP` del siguiente proceso a ejecutar. Al retornar (IRET), la CPU levanta el nuevo contexto y ejecuta el nuevo proceso.
4. **Bloqueos:** Si un proceso pide un recurso no disponible (read en teclado vacío, semáforo, etc.), se marca como Blocked y no se agenda hasta que otro proceso o interrupción lo pase de nuevo a Ready.

## Comments and Limitations
- **Limitaciones actuales:** Algoritmo de scheduling básico (típicamente Round Robin con o sin prioridades estáticas). La falta de aislamiento real de memoria puede hacer que un proceso sobreescriba datos de otro o del [[kernel_kernel.md|Kernel]].
- **Comentarios:** El Context Switch requiere manipulación delicada en Assembly para guardar y restaurar los registros correctamente.
