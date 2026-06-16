# Kernel Time Module

## Overview (¿Qué es?)
El módulo de Tiempo ([[kernel_time.md|Kernel Time]]) es el subsistema encargado de medir y rastrear el paso del tiempo en el sistema operativo. Administra el contador de ticks del procesador generados por el temporizador de intervalos programable (PIT) y proporciona acceso al chip de reloj de tiempo real (RTC) de la placa madre para obtener fecha y hora del hardware.

## Functionality (¿Qué hace?)
- Mantiene un contador global de ticks transcurridos desde el arranque del sistema (`ticks`).
- Implementa una rutina de suspensión temporal (`sleep`) en milisegundos para procesos del Kernel.
- Controla el temporizador de parpadeo del cursor gráfico en la pantalla de la consola (invocando a `videoCursorBlink`).
- Ofrece rutinas para consultar la fecha (`date`) y la hora (`time`) actuales del reloj del hardware.

## Internal Mechanics (¿Cómo funciona?)
1. **Timer Tick y PIT (IRQ 0):** El PIT está configurado de forma predeterminada por el hardware para emitir interrupciones periódicas en IRQ 0. En cada tick, la rutina `_irq00Handler` en assembly llama a `scheduler_tick()`, la cual invoca a `timer_handler()` en `time.c`.
   - `timer_handler` incrementa la variable global `ticks`.
   - Cada vez que `ticks` es múltiplo de `CURSOR_BLINK_TICKS = 10` (aproximadamente cada 550 ms bajo la frecuencia estándar de ~18.2 Hz del PIT), se ejecuta `videoCursorBlink()` para alternar el dibujado del cursor de la terminal en el driver de video.
2. **Rutina Sleep:** La suspensión de ejecución en Kernel se realiza mediante la función `sleep(ms)`.
   - Se calcula la cantidad de ticks objetivo como `target = ms / 10`.
   - Se ejecuta un bucle pasivo `while((ticks - start) < target)` que suspende la CPU ejecutando la instrucción ensamblador `hlt` (`_hlt()`) esperando que ocurran nuevas interrupciones de timer para actualizar la variable `ticks` y evitar el consumo innecesario de energía.
3. **Acceso al RTC (Real Time Clock):** Para obtener la fecha y la hora del hardware, el módulo utiliza funciones en ensamblador de `libasm.asm` que leen los registros del chip de reloj CMOS del microprocesador:
   - Se selecciona el registro de interés enviando su offset (segundos = 0, minutos = 2, hora = 4, día = 7, mes = 8, año = 9) al puerto de control `0x70 CMOS Address`.
   - Se lee el byte resultante desde el puerto `0x71 CMOS Data` mediante instrucciones `in`.
   - Los datos se devuelven codificados en formato BCD (Binary-Coded Decimal) crudo (por ejemplo, el valor decimal 25 se codifica como el byte `0x25`).

## Key Files & Structs (Archivos y Estructuras Clave)
* **Archivos principales:**
  - `Kernel/c/time/time.c`: Implementación de la suma de ticks, lógica de sleep y cargadores de fecha/hora CMOS en BCD.
  - `Kernel/include/time/time.h`: Prototipos e interfaz del módulo de tiempo.
  - `Kernel/asm/libasm.asm` (sección RTC): Implementación de las lecturas en ensamblador a los puertos `0x70` y `0x71` CMOS (`getSeconds`, `getMinutes`, `getHour`, `getDayOfMonth`, `getMonth`, `getYear`).
* **Estructuras de datos:**
  - No requiere estructuras complejas; utiliza la variable global estática `ticks` de tipo `unsigned long`.

## System Calls Relacionadas
- **`sys_time` (Syscall 1):** Llena un buffer de 3 bytes con la hora (`HH:MM:SS`) obtenida del RTC en formato BCD.
- **`sys_date` (Syscall 2):** Llena un buffer de 3 bytes con la fecha (`DD/MM/AA`) obtenida del RTC en formato BCD.
- **`sys_ticks` (Syscall 8):** Retorna la cantidad total de ticks acumulados desde el inicio del sistema.

## Comments and Limitations (Comentarios y Limitaciones)
- **Frecuencia Estática:** La granularidad de la función `sleep` depende directamente de la frecuencia del PIT (aproximadamente 55 ms por tick).
- **Formato BCD en Retorno:** Las llamadas `sys_time` y `sys_date` devuelven la fecha y hora sin decodificar (en formato BCD crudo). La conversión a caracteres legibles y el ajuste del huso horario (zona horaria local) es responsabilidad exclusiva de las aplicaciones en el espacio de usuario (Userland) mediante funciones auxiliares.
