# OS Initialization Roadmap

Este documento sirve como un **índice de lectura ordenado cronológicamente** según el flujo de inicialización y ejecución del Sistema Operativo. 

> [!TIP]
> Los enlaces están diseñados en formato Wikilink / Markdown nativo para que, al importar esta carpeta en **Obsidian**, se genere automáticamente el **Graph View** (Grafo) mostrando este archivo como el nodo central que conecta secuencialmente toda la arquitectura.

---

## 1. Fase de Arranque (Booting)
El viaje del Sistema Operativo comienza cuando la BIOS le cede el control al primer sector del disco.
1. [[bootloader.md]] - Cambia el procesador de modo real a modo protegido/largo y carga el Kernel en memoria.

## 2. Inicialización Temprana del Kernel (Core & Basics)
Una vez en 64 bits, el Bootloader salta al punto de entrada en C del Kernel. Aquí se establecen los cimientos antes de habilitar cosas complejas.
2. [[kernel_kernel.md]] - El Entry Point (`_start`). Limpia la memoria BSS y orquesta el llamado al resto de inicializadores.
3. [[kernel_lib.md]] - Proveé funciones básicas (como `memcpy`, `strlen`) vitales para inicializar otras estructuras.
4. [[kernel_console.md]] - Se inicializa temprano para poder imprimir mensajes de log o errores en la pantalla.

## 3. Manejo de Hardware y Excepciones
El sistema necesita prepararse para reaccionar ante eventos asíncronos del hardware o errores críticos de la CPU.
5. [[kernel_exceptions.md]] - Se preparan las rutinas para atajar divisiones por cero o fallos de página.
6. [[kernel_interrupts.md]] - Se configura la IDT y el PIC para recibir señales externas.
7. [[kernel_time.md]] - El Timer (PIT) comienza a "tickear" y se configura el acceso al reloj real (RTC).
8. [[kernel_drivers.md]] - Se inicializa el teclado (para poder recibir input) y utilidades de video.
9. [[kernel_sound.md]] - Inicialización del PC Speaker para emitir alertas sonoras (si aplica).

## 4. Gestión de Recursos Avanzados
Con el hardware básico bajo control, el Kernel prepara las estructuras complejas para ejecutar programas.
10. [[kernel_memory_manager.md]] - Se inicializa el asignador dinámico de memoria (heap/buddy/freelist).
11. [[kernel_process.md]] - Se configuran los PCBs y el Scheduler, posibilitando la multitarea.
12. [[kernel_ipc.md]] - Se crean las estructuras (Pipes, Semáforos) para que los futuros procesos se comuniquen y sincronicen.

## 5. El Puente a Userland
El Kernel ya está listo. Ahora debe ofrecer una interfaz segura para que los programas de usuario pidan recursos sin romper el sistema.
13. [[kernel_syscalls.md]] - Se registra el dispatcher en la interrupción `int 80h` (Llamadas al Sistema).

## 6. Espacio de Usuario (Userland)
El Kernel hace el salto (Ring 0 -> Ring 3) e inicializa el primer proceso de usuario.
14. [[userland_syscall.md]] - Los envoltorios en Assembly que los programas usarán para invocar la `int 80h`.
15. [[userland_lib.md]] - La librería estándar (libc) que formatea datos y simplifica el uso de las Syscalls.
16. [[userland_shell.md]] - El primer programa real en ejecutarse, proveyendo la terminal interactiva al usuario.
17. [[userland_tests.md]] - Programas accesorios ejecutados desde la Shell para probar que todo lo anterior (Memoria, Procesos, IPC) no colapsa bajo estrés.

---
*Importa esta carpeta como un "Vault" en Obsidian para visualizar la arquitectura.*
