# Kernel Interrupts Module

## Overview (¿Qué es?)
El módulo de Interrupciones en el [[kernel_kernel.md|Kernel]] es la infraestructura que gestiona de manera asíncrona los eventos de hardware externos y las solicitudes de software (llamadas al sistema, yields). Controla la Interrupt Descriptor Table (IDT) y configura el controlador de interrupciones (PIC) en modo de 64 bits.

## Functionality (¿Qué hace?)
- Define y carga la estructura de la IDT con las rutinas de servicio (ISRs) correspondientes.
- Configura e inicializa el controlador de interrupciones clásico (8259A PIC) enmascarando y desenmascarando canales físicos.
- Provee los controladores de bajo nivel (ensamblador) para las interrupciones del Timer Tick (IRQ 0), Teclado (IRQ 1) y llamadas al sistema (`int 0x80`).
- Implementa una compuerta especial para la planificación y entrega voluntaria del procesador (`int 0x81`).
- Asegura la preservación del estado completo de los registros del procesador antes de derivar la ejecución a los despachadores en C.

## Internal Mechanics (¿Cómo funciona?)
1. **Configuración de la IDT:** El cargador `idtLoader.c` genera un arreglo de descriptor de interrupción `DESCR_INT idt[256]`. Llena las compuertas de interrupción de 64 bits apuntando a los handlers definidos en `interrupts.asm`. Luego, carga la dirección base en el registro de hardware mediante la estructura descriptor `IDTR` y la llamada ensamblador `load_idt_asm`.
2. **Máscara del PIC:** Se deshabilitan todas las interrupciones de hardware excepto las necesarias. Se escribe en los puertos del PIC Master (`0x21`) con el valor `0xFC` (desenmascara IRQ0 [Timer] e IRQ1 [Teclado]) y PIC Slave (`0xA1`) con `0xFF` (enmascara todas).
3. **Mapeo de Rutinas de Interrupción (ISRs):**
   - **Timer Tick (IRQ 0 - `_irq00Handler`):** Guarda el contexto en el stack (`pushState`). Llama directamente a `scheduler_tick` enviando el puntero del RSP actual. Si hay cambio de contexto, `scheduler_tick` devuelve el RSP del nuevo proceso y se actualiza `rsp = rax`. Se envía la señal de fin de interrupción (EOI, `0x20`) al puerto del PIC Master (`0x20`) y se ejecuta `iretq` para retomar la ejecución del proceso planificado.
   - **Teclado (IRQ 1 - `_irq01Handler`):** Guarda el contexto. Lee el scancode del puerto `0x60`, lo guarda en `pressed_key`, y si coincide con la tecla modificadora de snapshot (`L_CONTROL`), copia todos los registros apilados en el arreglo `regsArray` para diagnósticos. Deriva a `irqDispatcher()` con el código 1, envía el EOI al PIC, restaura el contexto (`popState`) y vuelve con `iretq`.
   - **Syscall Gate (`int 0x80` - `_irq128Handler`):** Interrupción de software. Pone en marcha el despacho de llamadas al sistema invocando la función correspondiente en la tabla `syscalls` usando el índice en `rax`. El retorno se inyecta en el slot del stack de `rax` (`[rsp + 14 * 8]`). Si la syscall solicita un cambio de contexto (coloca `force_switch = 1`), limpia la bandera y fuerza la ejecución de `scheduler_yield_impl` pasando el RSP, actualizando el stack pointer antes de hacer `popState` e `iretq`.
   - **Yield Gate (`int 0x81` - `_irq129Handler`):** Interrupción de software especializada para ceder la CPU desde dentro de la ejecución del Kernel (por ejemplo, cuando un semáforo bloquea un proceso). No pasa por el despachador ni envía EOI al PIC. Invoca directamente a `scheduler_yield_impl` para realizar el cambio de contexto de forma inmediata.

## Key Files & Structs (Archivos y Estructuras Clave)
* **Archivos principales:**
  - `Kernel/c/interrupts/idtLoader.c`: Construcción y registro de compuertas en la tabla IDT y seteo de máscaras de puertos del PIC.
  - `Kernel/asm/interrupts.asm`: Implementación en ensamblador de las rutinas de servicio (ISRs) de interrupciones, la macro `pushState`/`popState`, y wrappers de hardware (`_hlt`, `_cli`, `_sti`).
  - `Kernel/c/interrupts/irqDispatcher.c`: Derivador de interrupciones físicas (IRQ 1 a keyboard driver).
* **Estructuras de datos:**
  - `typedef struct DESCR_INT`: Representa la compuerta de interrupción de 16 bytes compatible con el modo largo de 64 bits de la arquitectura x86-64.
  - `typedef struct IDTR`: Estructura descriptora de 10 bytes que contiene el límite y la dirección base de la tabla de la IDT.
* **Funciones fundamentales:**
  - `load_idt()`: Orquesta la configuración de la IDT y setea las máscaras del PIC.
  - `kernel_yield()` (en C/ASM): Wrapper que ejecuta `int 0x81` para forzar un context switch desde el kernel.

## System Calls Relacionadas
La existencia del dispatcher de syscalls completo depende directamente de la compuerta de software `int 0x80` (`_irq128Handler`). Las syscalls que provocan bloqueos interactúan indirectamente provocando una interrupción `int 0x81` para ceder inmediatamente el procesador.

## Comments and Limitations (Comentarios y Limitaciones)
- **PIC Clásico vs APIC:** El sistema utiliza el controlador clásico 8259A PIC que está limitado a sistemas monoprocesador. No hay soporte para APIC (Advanced Programmable Interrupt Controller) ni para la gestión de interrupciones en entornos reales SMP (Multiprocesamiento Simétrico).
- **Habilitación Segura de Interrupciones:** Durante toda la inicialización del kernel en `initializeKernelBinary()`, las interrupciones permanecen deshabilitadas (`_cli()`). Se habilitan de manera controlada únicamente cuando el scheduler despacha el primer proceso (la shell) mediante el `iretq` de `scheduler_start_asm` que carga `RFLAGS` con el bit `IF` en 1. Esto previene triple faults si el Timer Tick intenta interrumpir antes de que las estructuras del planificador estén configuradas.
