# Kernel Core Module (kernel.c)

## Overview (¿Qué es?)
El módulo principal del [[kernel_kernel.md|Kernel]] (generalmente alojado en `[[kernel_kernel.md|Kernel]].c`) es el corazón del sistema operativo inicial. Actúa como el puente entre el código del [[bootloader.md|Bootloader]] que finaliza la transición a 64-bits y el inicio formal de todos los subsistemas lógicos del [[kernel_kernel.md|Kernel]].

## Functionality (¿Qué hace?)
- Es el punto de entrada principal (Entry Point) del código en C del [[kernel_kernel.md|Kernel]] (`_start` o `main`).
- Limpia la sección BSS del [[kernel_kernel.md|Kernel]] (variables no inicializadas).
- Orquesta y llama a las funciones de inicialización del resto de módulos (IDT, [[kernel_memory_manager.md|Memory Manager]], [[kernel_process.md|Scheduler]], [[kernel_drivers.md|Drivers]]).
- Realiza la carga y ejecución del primer proceso de espacio de usuario ([[userland_shell.md|Userland]]).

## Internal Mechanics (¿Cómo funciona?)
1. **Bootstraping:** El [[bootloader.md|Bootloader]] llama a una función en ensamblador que a su vez invoca a la función principal en C.
2. **Limpieza:** Una de sus primeras tareas es blanquear (poner a cero) el segmento BSS para asegurar que variables globales no inicializadas sean nulas.
3. **Inicialización:** Inicializa las estructuras globales críticas llamando a métodos como `load_idt()`, `initialize_memory()`, `initialize_scheduler()`, etc.
4. **Arranque de [[userland_shell.md|Userland]]:** Usando la dirección provista por el [[bootloader.md|Bootloader]] (donde se descomprimió [[userland_shell.md|Userland]]), se crea el primer proceso mediante el Process Manager y se invoca al [[kernel_process.md|Scheduler]] para cederle el control por primera vez, o se hace un salto artificial a ring 3.

## Comments and Limitations
- **Limitaciones actuales:** Al ser secuencial, si uno de los módulos falla en su inicialización, el [[kernel_kernel.md|Kernel]] entra en pánico (hang) inmediatamente sin mecanismos avanzados de recuperación en esta etapa.
- **Comentarios:** Este módulo no debería ser muy grande; funciona como un orquestador que delega la complejidad a los subsistemas específicos.
