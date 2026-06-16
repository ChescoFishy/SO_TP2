# Userland Shell Module

## Overview (¿Qué es?)
El modulo de la Shell de Usuario ([[userland_shell.md|Userland Shell]]) es la aplicación principal del espacio de usuario (Ring 3). Provee la interfaz de línea de comandos (CLI) interactiva a través de la cual el usuario puede ejecutar comandos incorporados (built-ins) u orquestar la ejecución paralela y encadenada de procesos independientes.

## Functionality (¿Qué hace?)
- Muestra el prompt interactivo de entrada `> ` y habilita/deshabilita el cursor de consola del Kernel.
- Lee líneas completas de caracteres desde el teclado, procesando retrocesos (`\b`) y cambios de escala tipográfica en caliente (`+` y `-`).
- Tokeniza la entrada distinguiendo el nombre de la aplicación, los parámetros del comando y operadores especiales.
- Permite la ejecución en segundo plano (`background`) de procesos mediante el sufijo `&`.
- Implementa el operador de tubería `|` para comunicar la salida estándar de un proceso con la entrada estándar de otro.
- Soporta comandos incorporados (*built-ins*) ejecutados de forma síncrona en el contexto de la shell.
- Ofrece soporte de llamadas para la totalidad de aplicaciones y tests del sistema.

## Internal Mechanics (¿Cómo funciona?)
### 1. Bucle Principal y Entrada
- La shell ejecuta un bucle infinito en `main()` (en `shell.c`). En cada ciclo, escribe el prompt `> `, enciende el cursor llamando a `sys_set_cursor(1)` y ejecuta `shellReadLine()`.
- `shellReadLine()` lee bytes llamando a `sys_read`. Al recibir un salto de línea (`\n`), rompe el ciclo. Al recibir `+` o `-`, aumenta o disminuye la escala de fuente llamando a `sys_increase_fontsize`/`sys_decrease_fontsize` y refrescando la pantalla con `redrawFont()`. Al recibir `\b`, borra visualmente y decrementa el índice.
- Luego de la lectura, apaga el cursor con `sys_set_cursor(0)` e invoca a `processLine(buff)`.

### 2. Tokenización y Búsqueda de Comandos
- `processLine` remueve espacios en blanco al final y detecta si termina con `&` (seteando `fg = 0` para ejecución en background).
- `tokenize` divide la cadena in-place reemplazando espacios y tabulaciones por caracteres nulos (`\0`) y cargando el arreglo `argv` con punteros a cada token.
- Busca el comando en la tabla estática `commands[]` mediante `find_cmd()`.
  - **Comandos Built-in (`c->builtin != 0`):** Se ejecutan síncronamente. Antes de llamar a `c->builtin()`, se restauran temporalmente los espacios entre argumentos en la cadena original y se la expone globalmente mediante `g_cmd_args` (permitiendo que el built-in parsee sus argumentos usando `next_uint` sobre `cmd_args()`). Los built-ins no admiten ejecución en background `&` ni tuberías `|`.
  - **Procesos de Usuario (`c->entry != 0`):** Se ejecutan llamando a `spawn_simple` o `spawn_pipe`.

### 3. Lanzamiento de Procesos Simples
Para evitar que el buffer original de la shell (que se sobrescribirá en el próximo ciclo de lectura) corrompa los argumentos del proceso hijo, la función `dup_argv` realiza una copia contigua de las cadenas de argumentos (`argv`) en el heap de usuario mediante `sys_malloc`.
- Si el proceso corre en primer plano (`fg = 1`), la shell invoca `sys_create_process` y luego bloquea su propia ejecución llamando a `sys_waitpid(pid)`. Una vez que el hijo termina, la shell libera la memoria de los argumentos copiados (`sys_free`) y vuelve a iterar.
- Si corre en segundo plano (`fg = 0`), la shell crea el proceso, imprime el mensaje descriptivo con su PID y vuelve a mostrar el prompt de inmediato (dejando que el bloque de argumentos persista en memoria).

### 4. Mecanismo de Tuberías (`p1 | p2`)
La shell soporta la conexión por pipe de exactamente dos comandos de proceso.
- Al detectar el caracter `|`, divide la línea en dos mitades (izquierda y derecha). Las tokeniza de forma independiente buscando ambos comandos en la tabla.
- Crea una tubería anónima en el Kernel mediante `sys_pipe(fds)` (que devuelve el descriptor de lectura en `fds[0]` y de escritura en `fds[1]`).
- Crea los dos procesos utilizando la llamada `sys_create_process_fd` empaquetando los fds de redirección en un entero de 64 bits (`fg_fdin_fdout`):
  - **Proceso Izquierdo (Productor):** Recibe la redirección de salida `stdout = fds[1]` (extremo de escritura del pipe) e input de consola `stdin = 0`.
  - **Proceso Derecho (Consumidor):** Recibe la redirección de entrada `stdin = fds[0]` (extremo de lectura del pipe) y salida de consola `stdout = 1`.
- Cierra inmediatamente los descriptores duplicados en el contexto de la shell (`sys_pipe_close(fds[0])` y `sys_pipe_close(fds[1])`).
- Si es foreground, la shell ejecuta `sys_waitpid(pid2)` esperando que el consumidor termine. Inmediatamente después, envía `sys_kill(pid1)` por seguridad (en caso de que el productor haya quedado en un bucle infinito sin leer de I/O) y espera su retorno antes de liberar la memoria de ambos argumentos y re-mostrar el prompt.

## Key Files & Structs (Archivos y Estructuras Clave)
* **Archivos principales:**
  - `Userland/c/shell/shell.c`: Bucle de lectura de consola y entrada de caracteres.
  - `Userland/c/shell/parser.c`: Tokenizador de cadenas, despachador de built-ins, orquestador de pipes e iniciador de procesos simples.
  - `Userland/include/shell/shell.h`: Constantes del buffer y mensaje de bienvenida.
* **Estructuras de datos:**
  - `Command`:
    ```c
    typedef struct {
        const char *name;                       /* Nombre del comando */
        void       (*builtin)(void);            /* Puntero a función si es builtin */
        void       (*entry)(int, char**);       /* Puntero a función si es un proceso */
        const char *desc;                       /* Descripción de ayuda */
    } Command;
    ```
* **Funciones fundamentales:**
  - `processLine(char *buff, uint32_t *history_len)`: Analiza sintácticamente y ejecuta la línea ingresada.
  - `spawn_pipe(Command *c1, int argc1, char **argv1, Command *c2, int argc2, char **argv2, uint8_t fg)`: Instancia la tubería anónima, redirige los fds de los procesos y los ejecuta de forma coordinada.

## System Calls Relacionadas
Consume directamente las syscalls `sys_set_cursor`, `sys_read`, `sys_write`, `sys_pipe`, `sys_pipe_close`, `sys_create_process`, `sys_create_process_fd`, `sys_waitpid` y `sys_kill`.

## Comments and Limitations (Comentarios y Limitaciones)
- **Límite de Redirección:** El sistema solo soporta la conexión de hasta dos comandos en paralelo mediante un único operador `|`. No es posible encadenar 3 o más comandos (`p1 | p2 | p3`) ni realizar redirecciones clásicas de archivos (`>` o `<`).
- **Restricciones de Built-ins:** Los comandos marcados como built-ins (tales como `help`, `clear`, `ps`, `mem`, `kill`, `nice`, `block`) corren en el contexto de ejecución del hilo de la shell. Por lo tanto, no admiten ejecutarse en background con `&` ni formar parte de un extremo de una tubería `|`.
