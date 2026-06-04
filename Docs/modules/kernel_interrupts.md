# Kernel Interrupts Module

## Overview (¿Qué es?)
El módulo de Interrupciones en el [[kernel_kernel.md|Kernel]] es el mecanismo que permite al hardware y al software notificar a la CPU sobre eventos que requieren atención inmediata. Es la base de un sistema operativo interactivo y asíncrono.

## Functionality (¿Qué hace?)
- Inicializa el Controlador de Interrupciones Programable (PIC) y habilita interrupciones específicas de hardware (como el Timer y el Teclado).
- Configura la IDT (Interrupt Descriptor Table) poblándola con punteros a las rutinas de servicio (ISRs) correspondientes a cada evento.
- Administra el flujo cuando ocurre una interrupción: guarda el estado, despacha al handler correcto, y restaura el estado.

## Internal Mechanics (¿Cómo funciona?)
1. **Configuración Inicial:** El [[kernel_kernel.md|Kernel]] crea arreglos o estructuras (la IDT) donde mapea un número de interrupción (ej. IRQ0 - Timer = Int 20h, IRQ1 - Keyboard = Int 21h, Int 80h - [[kernel_syscalls.md|Syscalls]]) a una dirección de memoria donde reside el código ASM que lo atenderá.
2. **Ciclo de Interrupción:** Al ocurrir una interrupción, la CPU desactiva temporalmente las interrupciones, guarda `RIP`, `CS`, y `RFLAGS` en la pila y salta al código ASM mapeado. Este wrapper guarda el resto de registros (`pushState`) y llama a `irqDispatcher` (en C).
3. **Despacho:** Dependiendo del número de interrupción (irq), se llama al handler específico (ej. `timer_handler` o `keyboard_handler`).
4. **Restauración:** Se envía la señal de fin de interrupción (EOI) al PIC, se restauran los registros (`popState`) y se ejecuta la instrucción `iretq` para volver donde estaba la ejecución.

## Comments and Limitations
- **Limitaciones actuales:** Utiliza el mecanismo Legacy PIC en lugar del APIC más moderno presente en sistemas multicore. Maneja rutinas bloqueantes o muy rápidas, lo que puede afectar la latencia.
- **Comentarios:** El código assembly asume el tamaño de los registros en 64-bits. Es imperativo no romper la pila (stack frame) durante el manejo.
