# Requerimientos y Especificaciones Detalladas — Trabajo Práctico N° 2

Este documento recopila de manera detallada todos los requerimientos formales del archivo de especificaciones, complementados de manera directa con las explicaciones, aclaraciones y énfasis pedagógicos brindados por los profesores en la clase y en el video explicativo del enunciado [cite: uploaded:Requerimientos.pdf].

---

## 1. Administración de Memoria Física (Physical Memory Management)

El kernel debe implementar un esquema monolítico de 64 bits con soporte de aislamiento de memoria física [cite: 205, 207]. Deberá contar con dos administradores independientes que compartan una misma interfaz transparente [cite: 212]:
* *Memory Manager Elegido por el Grupo:* Un administrador propio (ej. Free List, Next Fit, First Fit, etc.) que soporte de manera obligatoria la liberación de memoria física [cite: 209].
* *Buddy System:* Implementación exacta del algoritmo de asignación por particiones binarias [cite: 209].

### Aclaraciones del Profesor y Video:
* *Alternancia en Compilación:* Ambos mecanismos *no deben coexistir activos en tiempo de ejecución*. La selección de cuál utilizar se tiene que determinar exclusivamente en *tiempo de compilación* a través de directivas del preprocesador o reglas específicas del Makefile [cite: 210, 211]. Por ejemplo:
  ```
  make          # Compila con el Memory Manager del grupo
  make buddy    # Compila con el Buddy System
  ```
* *Transparencia Absoluta:* La interfaz genérica expuesta al resto del sistema operativo debe ser idéntica. Ningún otro subsistema (procesos, pipes, drivers) debe saber qué administrador está corriendo por debajo [cite: 212].
* *System Calls Involucradas:*
  * Reservar y liberar bloques de memoria [cite: 214].
  * Consultar el estado de la memoria: Debe devolver de forma precisa la memoria total del sistema, la cantidad ocupada y la memoria libre actual [cite: 215].
* *Validación Obligatoria:* Al menos uno de los dos administradores de memoria debe pasar de forma perfecta el test automatizado *test_mm* sin reportar solapamientos ni fugas de recursos [cite: 306].

---

## 2. Procesos, Context Switching y Scheduling

El núcleo del sistema operativo debe proveer un entorno de *multitasking preemptivo* con soporte para una cantidad variable y dinámica de procesos distribuidos en Kernel Space y User Space separados [cite: 205, 221].

### El Mecanismo de Planificación (Scheduler):
El algoritmo de scheduling obligatorio es un *Round Robin con Prioridades* [cite: 224]. Cada proceso contará con un nivel de prioridad asignado dinámicamente que determinará el tamaño o frecuencia de sus cuantums de ejecución en la CPU [cite: 222].

### Aclaraciones del Profesor y Video:
* *Pasaje de Parámetros:* Al crear un proceso a través de la shell o de manera programática, el kernel debe empaquetar y transferir los argumentos correctamente al stack del nuevo contexto de usuario, de forma análoga al funcionamiento de `argc` y `argv` de UNIX [cite: 226].
* *Visibilidad del Estado Interno:* La system call encargada de listar los procesos (*ps*) debe extraer y mostrar en pantalla de forma obligatoria los siguientes punteros y variables de control del bloque de control del proceso (PCB) [cite: 227]:
  * Nombre del proceso.
  * ID numérico único (PID).
  * Nivel de prioridad actual.
  * *Stack Pointer (SP)* y *Base Pointer (BP)* en el momento de la suspensión.
  * Indicador de si el proceso se encuentra en *Foreground* o *Background* [cite: 227].
* *System Calls Involucradas:* Crear proceso, finalizar proceso, obtener el PID actual, matar un proceso arbitrario por ID, modificar la prioridad de un proceso dado su PID, bloquear/desbloquear un proceso de la cola de listos, renunciar voluntariamente al uso de la CPU (`yield`) y esperar la finalización de los procesos hijos (`wait`) [cite: 226, 227, 229, 230, 231, 232, 233].

---

## 3. Sincronización entre Procesos

El kernel debe implementar primitivas de *Semáforos* para regular el acceso a secciones críticas y recursos compartidos por procesos que no necesariamente están relacionados por herencia de sangre [cite: 241].

### Aclaraciones del Profesor y Video:
* *Identificación Compartida:* El mecanismo debe permitir que procesos independientes puedan abrir el mismo semáforo mediante un identificador alfanumérico o numérico acordado *a priori* [cite: 241].
* *Aislamiento y Eficiencia:* La solución debe estar completamente libre de condiciones de carrera (*race conditions*), bloqueos mutuos (*deadlocks*) y, de manera crítica, *no se permite el uso de busy waiting* (espera activa) en los semáforos de uso general [cite: 242, 327]. Si un proceso no puede acceder al semáforo, debe ser removido de la cola de ejecución del scheduler y transicionar al estado *bloqueado* hasta que ocurra una señal de liberación [cite: 242].
* *Garantía de Atomicidad:* Para la implementación interna de los bloqueos en el espacio del kernel, es obligatorio emplear instrucciones de hardware que aseguren operaciones atómicas independientes del entrelazado de hilos (por ejemplo, instrucciones del tipo `lock xchg`, `test-and-set` o equivalentes de la arquitectura x86-64) [cite: 242].
* *System Calls Involucradas:* Crear, abrir, cerrar y modificar (incrementar/decrementar) el valor numérico del semáforo [cite: 244].

---

## 4. Comunicación Interprocesos (Inter Process Communication - IPC)

Se requiere la incorporación de *Pipes Unidireccionales* como canal base de transferencia de flujos de datos estructurados entre procesos [cite: 250].

### Aclaraciones del Profesor y Video:
* *Transparencia en la E/S:* El diseño del sistema operativo debe garantizar que leer o escribir desde un pipe sea *completamente transparente* para el programa de usuario [cite: 256]. Un proceso estándar debe leer desde su Entrada Estándar (*stdin*) y escribir en su Salida Estándar (*stdout*) sin enterarse ni requerir modificaciones en su código fuente si el flujo proviene de la terminal de texto o de un pipe intermedio [cite: 251, 256].
* *Operaciones Bloqueantes:* Las llamadas al sistema para leer desde un pipe vacío o escribir en un pipe cuyo buffer interno esté completamente saturado deben *bloquear al proceso invocante* de manera inmediata, cediendo el procesador a otra tarea lista [cite: 250].
* *Conexión en la Shell:* Este desacoplamiento transparente es lo que permitirá a la shell de usuario encadenar la salida de un programa con la entrada de otro utilizando el carácter especial clásico de tubería `|` [cite: 252, 268].
* *System Calls Involucradas:* Crear pipes, abrir pipes, y reutilizar las llamadas genéricas de lectura y escritura del sistema adaptadas al descriptor del pipe [cite: 255, 256].

---

## 5. Aplicaciones obligatorias del Espacio de Usuario (User Space)

Para validar de forma integral el correcto funcionamiento de las llamadas al sistema y la robustez del kernel desarrollado, se deben codificar de manera exacta las siguientes aplicaciones independientes dentro del espacio de usuario [cite: 262, 263]:

* *sh (Shell de Usuario):* El intérprete de comandos principal. Debe permitir la ejecución de programas en primer plano (*foreground*) y segundo plano (*background*) utilizando el sufijo `&` [cite: 264, 266]. Debe implementar el operador de tubería `|` para conectar exactamente dos procesos (p1 | p2) [cite: 268]. Asimismo, debe procesar de forma nativa los atajos de teclado globales: `Ctrl+C` para matar de forma inmediata al proceso que se encuentra en foreground, y `Ctrl+D` para enviar la señal de fin de archivo (*EOF*) a la entrada interactiva [cite: 270].
* *help:* Despliega en pantalla el listado de comandos disponibles junto con un menú explicativo especial con los nombres de los tests provistos por la cátedra [cite: 271].
* *mem:* Aplicación de monitoreo que invoca las syscalls de memoria e imprime un reporte estructurado del estado físico (total, ocupado y libre) [cite: 273].
* *ps:* Imprime en la consola de comandos el estado actual detallado de la tabla de procesos del sistema operativo (nombre, PID, prioridad, SP, BP y modo de ejecución) [cite: 275].
* *loop:* Proceso que imprime en bucle continuo su propio ID (PID) junto con un mensaje de saludo a intervalos regulares de tiempo [cite: 276]. *Aclaración del Profesor:* Este programa específico debe realizar una *espera activa* deliberada para demostrar que la preemoción del scheduler funciona correctamente, impidiendo que monopolice el sistema [cite: 277, 327].
* *kill:* Envía la señal de finalización forzada a un proceso específico a partir de su ID provisto como argumento [cite: 278].
* *nice:* Modifica dinámicamente la prioridad de ejecución asignada a un proceso en el scheduler a partir de su ID y del nuevo valor numérico de prioridad [cite: 279].
* *block:* Wrapper interactivo que conmuta el estado de un proceso específico entre bloqueado y listo utilizando su ID identificador [cite: 280].
* *cat:* Lee el flujo continuo de caracteres proveniente del stdin y lo replica de forma exacta en el stdout [cite: 282].
* *wc:* Cuenta de manera exacta e imprime la cantidad total de líneas recibidas a través de su entrada estándar [cite: 282].
* *filter:* Aplicación de procesamiento de texto que filtra y remueve de forma completa todas las vocales de la cadena recibida por el stdin antes de imprimir el resultado [cite: 283].
* *mvar:* Aplicación especial encargada de modelar y simular el problema clásico de sincronización de múltiples lectores y escritores sobre una variable global estructurada (similar conceptualmente al tipo de datos MVar de Haskell) [cite: 284].
  * Recibe como parámetros fijos la cantidad exacta de hilos de lectura y escritura [cite: 285].
  * Utiliza esperas activas aleatorias seguidas de un control estricto por semáforos para asegurar que solo un proceso a la vez acceda a mutar o leer la variable común [cite: 285, 288].
  * Debe respetar de manera estricta la matriz de comportamiento y salidas esperadas provista en el enunciado interactivo para las pruebas de concurrencia y alteración de prioridades en vivo [cite: 290].

---

## 6. Entorno de Compilación y Criterios de Entrega Obligatorios

### Restricciones del Entorno:
* Es completamente mandatario compilar la totalidad del código fuente utilizando la imagen de Docker oficial provista por la cátedra para asegurar la consistencia multiarquitectura [cite: 308]:
  ```
  docker pull agodio/itba-so-multiarch:3.1
  ```
* Las reglas base de construcción del Makefile (`make`, `make all`, `make <memory_manager>`) deben estar estrictamente restringidas a las tareas de compilación pura dentro del contenedor [cite: 310]. Cualquier automatización adicional como el inicio del emulador QEMU, descarga de dependencias externas o configuración del contenedor Docker debe resolverse mediante reglas Make independientes y diferenciadas [cite: 311].
* El código debe compilar con el flag `-Wall` activo y *no debe reportar ningún tipo de warning o advertencia* en la terminal [cite: 304].

### Directivas de Entrega en el Repositorio:
* El repositorio del proyecto debe contener un archivo de documentación obligatorio denominado `README.md` localizado en la raíz del mismo [cite: 303, 313]. Este documento debe detallar explícitamente las instrucciones de compilación, ejecución, replicación exacta de pruebas, descripción sintáctica de comandos, caracteres especiales y la cita explícita de cualquier fragmento de código externo o uso de asistentes de IA en el desarrollo [cite: 314, 315, 319, 320, 321, 324].
* Queda estrictamente *prohibido incluir archivos binarios compilados o archivos temporales de prueba* en el árbol de commits del control de versiones [cite: 330].
