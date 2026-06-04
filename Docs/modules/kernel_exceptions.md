# Kernel Exceptions Module

## Overview (¿Qué es?)
El módulo de Excepciones del [[kernel_kernel.md|Kernel]] se encarga de interceptar y manejar situaciones anómalas o errores de ejecución provocados por el procesador, como divisiones por cero o instrucciones inválidas.

## Functionality (¿Qué hace?)
- Intercepta fallos críticos del sistema generados a nivel de CPU.
- Evita que un error en [[userland_shell.md|Userland]] o [[kernel_kernel.md|Kernel]] genere un reinicio silencioso o comportamiento indefinido.
- Realiza un volcado (dump) del estado de los registros en el momento de la excepción para facilitar el debugging.
- Decide qué acción tomar tras el error (ej. matar el proceso problemático o pausar la ejecución).

## Internal Mechanics (¿Cómo funciona?)
1. **IDT:** Cuando ocurre una excepción (por ejemplo, excepción 0 - Divide by Zero, o excepción 6 - Invalid Opcode), la CPU consulta la Interrupt Descriptor Table (IDT) y salta al handler en Assembly configurado para esa excepción.
2. **Dispatcher:** El handler en ASM salva todos los registros en el stack y llama al dispatcher en C (`exceptionDispatcher`).
3. **Manejo:** El código en C identifica el tipo de excepción, imprime en pantalla el estado de los registros (Registers dump) y, dependiendo de la política, puede reiniciar el proceso de [[userland_shell.md|Userland]] actual o abortar el sistema (si la excepción proviene del [[kernel_kernel.md|Kernel]]).

## Comments and Limitations
- **Limitaciones actuales:** Muchas excepciones complejas (como Page Fault - 14) pueden no tener mecanismos completos de recuperación y terminan matando irremediablemente el proceso o colgando el sistema.
- **Comentarios:** Muy útil durante el desarrollo. Es clave asegurarse de que al matar un proceso por excepción se liberen sus recursos correctamente.
