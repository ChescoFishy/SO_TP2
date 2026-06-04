# Userland Lib Module

## Overview (¿Qué es?)
El módulo de la Librería de Usuario ([[userland_shell.md|Userland]] Lib / libc equivalente) es un conjunto de funciones provistas en el espacio de usuario (Ring 3) que facilita el desarrollo de aplicaciones. Evita que los programas tengan que invocar manualmente [[kernel_syscalls.md|Syscalls]] en Assembly para realizar tareas repetitivas o dar formato a los datos.

## Functionality (¿Qué hace?)
- **Entrada/Salida Estándar:** Provee funciones similares a `printf`, `scanf`, `putchar`, `getchar`.
- **Manejo de Strings:** Provee `strlen`, `strcpy`, `strcmp` (similar al lib del [[kernel_kernel.md|Kernel]] pero en espacio de usuario).
- **Asignación de Memoria:** Implementa `malloc` y `free` delegando en llamadas de sistema de alloc/free del [[kernel_kernel.md|Kernel]].
- **Conversión de datos:** Funciones como `atoi` o `itoa` para facilitar el formateo en pantalla.

## Internal Mechanics (¿Cómo funciona?)
1. **Wrappers de E/S:** `printf` acepta un string de formato (`%d`, `%s`, etc.), procesa los argumentos con macros `va_list`, convierte todo a una cadena final (usando `itoa` y similares) y finalmente invoca `sys_write` (enviando el string al File Descriptor estándar).
2. **Buffer:** Para la entrada como `scanf`, se acumulan los caracteres leídos desde `sys_read` hasta encontrar un salto de línea (`\n`), permitiendo procesar el texto completo.
3. **Paso a [[kernel_syscalls.md|Syscalls]]:** Para cualquier operación privilegiada, estas funciones terminan llamando a los envoltorios definidos en el módulo de [[kernel_syscalls.md|Syscalls]] de [[userland_shell.md|Userland]].

## Comments and Limitations
- **Limitaciones actuales:** Al ser una librería minimalista para un entorno académico, no incluye la inmensa cantidad de funciones y estándares POSIX de una libc real como glibc o musl.
- **Comentarios:** Mantener la separación estricta: código en [[userland_shell.md|Userland]] lib *no* puede usar direcciones físicas ni funciones definidas en el módulo del [[kernel_kernel.md|Kernel]]; solo puede interactuar por [[kernel_syscalls.md|Syscalls]].
