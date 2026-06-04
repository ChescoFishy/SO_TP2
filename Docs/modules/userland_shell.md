# Userland Shell Module

## Overview (¿Qué es?)
El [[userland_shell.md|Shell]] de usuario es la interfaz de línea de comandos primaria y principal aplicación de [[userland_shell.md|Userland]]. Es el intérprete donde el usuario interacciona y solicita lanzar comandos o programas de prueba.

## Functionality (¿Qué hace?)
- Imprime un prompt (ej. `user@os$`).
- Recibe y procesa los comandos tecleados por el usuario.
- Ejecuta los distintos binarios o comandos *built-in* (incorporados), como listar procesos (ps), visualizar memoria, o ejecutar tests intensivos (test_mm, test_processes).
- Maneja la invocación de comandos en *background* (haciendo uso de pipes y de la creación de procesos independientes).

## Internal Mechanics (¿Cómo funciona?)
1. **Bucle Infinito:** Corre un bucle principal que bloquea esperando entrada de teclado (`getchar()` o similar).
2. **Parsing:** Al recibir `\n`, toma el buffer del comando, lo divide en tokens (nombre del comando y argumentos) e intenta matchearlo con su lista de comandos conocidos.
3. **Ejecución:** Si hace *match*, puede invocar funciones built-in directamente o pedirle al [[kernel_kernel.md|Kernel]] (vía `sys_exec`) que instancie un proceso nuevo para ejecutar la tarea.
4. **Pipes y Background:** Si se detecta un operador especial (como `|` para tuberías o `&` para background), el [[userland_shell.md|Shell]] se encarga de crear el pipe (`sys_pipe`), crear los procesos conectados a este pipe, y determinar si se queda bloqueado esperando (`sys_wait`) o vuelve a imprimir el prompt inmediatamente (caso background).

## Comments and Limitations
- **Limitaciones actuales:** La complejidad del parser es baja. Posiblemente no soporte scripts complejos, redirecciones avanzadas de I/O (como `>` o `<`), ni variables de entorno.
- **Comentarios:** Es el programa vitrina del SO. Cualquier feature agregada en el [[kernel_kernel.md|Kernel]] (nuevos [[kernel_ipc.md|IPC]], semáforos, listado de procesos) debe tener un reflejo de su uso en la [[userland_shell.md|Shell]] para poder probarlo y demostrar su funcionamiento.
