# Kernel Time Module

## Overview (¿Qué es?)
El módulo de Time (Tiempo) agrupa las responsabilidades de contabilizar el paso del tiempo en el sistema y de acceder al reloj de tiempo real del hardware.

## Functionality (¿Qué hace?)
- **Timer (PIT):** Mantiene cuenta de la cantidad de "ticks" que han ocurrido desde que inició el sistema.
- Proveé la base del cálculo de uptime, sleep de milisegundos y el motor del [[kernel_process.md|Scheduler]].
- **RTC (Real Time Clock):** Interactúa con el chip RTC de la placa base para obtener la hora y fecha real (horas, minutos, segundos, día, mes, año).

## Internal Mechanics (¿Cómo funciona?)
1. **Timer Ticks:** El PIT está programado para generar una interrupción en el IRQ0 a una frecuencia fija (por defecto ~18.2 Hz o a una frecuencia custom de 1000Hz). Cada vez que esto ocurre, el handler incrementa una variable estática `ticks`.
2. **RTC:** Para obtener la hora, se utilizan puertos `out` (puerto 70h para seleccionar registro de hora/min/seg) e `in` (puerto 71h para leer el valor). Los valores obtenidos generalmente vienen codificados en formato BCD (Binary-Coded Decimal) y el módulo se encarga de convertirlos a valores decimales/enteros utilizables por C.

## Comments and Limitations
- **Limitaciones actuales:** La resolución del comando sleep está ligada estrechamente a la frecuencia con la que se dispare la interrupción de timer.
- **Comentarios:** Es importante tratar la desincronización y los husos horarios; típicamente el RTC entrega el tiempo en UTC, lo que amerita un ajuste manual según la zona si es necesario.
