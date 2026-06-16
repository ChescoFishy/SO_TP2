# Kernel Sound Module

## Overview (¿Qué es?)
El módulo de Sonido ([[kernel_sound.md|Kernel Sound]]) es el controlador encargado de administrar el parlante interno de la computadora (PC Speaker). Utiliza el temporizador de intervalos programable (PIT Intel 8253/8254) y los puertos de I/O de la placa madre para emitir tonos monofónicos de frecuencia y duración controlada.

## Functionality (¿Qué hace?)
- Activa el altavoz físico configurando una frecuencia de tono específica en Hz (`startSpeaker`).
- Apaga el altavoz del sistema interrumpiendo el flujo de oscilación (`turnOff`).
- Genera señales sonoras bloqueantes de duración determinada (`beep`), combinando la generación de tonos con la suspensión de ejecución basada en ticks del reloj (`sleep`).

## Internal Mechanics (¿Cómo funciona?)
1. **Detección de Frecuencia y Divisor:** El altavoz utiliza el canal 2 del chip PIT. El PIT recibe una frecuencia base constante de $1.193181 \text{ MHz}$ (`PIT_BASE_HZ = 1193180`). Para generar un tono de frecuencia `freq`, se calcula el divisor entero de hardware:
   $$\text{Divisor} = \frac{\text{PIT\_BASE\_HZ}}{\text{freq}}$$
2. **Programación del PIT:**
   - Se envía el comando de configuración de onda cuadrada `PIT_SQUARE_WAVE_MODE = 0xB6` (modo 3, lectura/escritura de byte bajo y alto secuencial para canal 2) al puerto de control del PIT `0x43` (`PIT_CONTROL_PORT`).
   - Se escribe el divisor resultante secuencialmente (primero el byte bajo, luego el byte alto) en el puerto de datos del canal 2 del PIT `0x42` (`PIT_CHANNEL2_DATA_PORT`).
3. **Activación de Puertos (PC Speaker):** Para hacer oscilar el cono del altavoz, se lee el estado del puerto del altavoz `0x61` (`PC_SPEAKER_PORT`) mediante `inb`. Si los dos bits inferiores no están encendidos, se realiza una operación OR lógica con `SPEAKER_ENABLE_BITS = 3` (bit 0 activa la salida del PIT canal 2 hacia el altavoz; bit 1 activa el altavoz físico) y se escribe el valor de retorno en el puerto `0x61` mediante `outb`.
4. **Desactivación de Puertos:** Para apagar el sonido, se lee el puerto `0x61`, se realiza un AND lógico con `SPEAKER_OFF_MASK = 0xFC` (apaga los bits 0 y 1) y se escribe el resultado en el mismo puerto.
5. **Beep Temporizado:** La función `beep` programa la frecuencia con `startSpeaker`, suspende el proceso actual llamando a `sleep(ticks)` (el cual cede voluntariamente la CPU usando la instrucción `hlt` hasta transcurridos los ticks correspondientes), y finalmente silencia el altavoz con `turnOff()`.

## Key Files & Structs (Archivos y Estructuras Clave)
* **Archivos principales:**
  - `Kernel/c/sound/sound.c`: Implementación de encendido/apagado del altavoz y lógica del beep.
  - `Kernel/include/sound/sound.h`: Definición de puertos de hardware, máscaras de bits e interfaces del módulo.
* **Estructuras de datos:**
  - No requiere estructuras complejas; interactúa puramente con registros de I/O de la CPU.
* **Constantes Clave y Puertos:**
  - `PC_SPEAKER_PORT = 0x61`: Puerto del controlador del altavoz del sistema.
  - `PIT_CONTROL_PORT = 0x43` / `PIT_CHANNEL2_DATA_PORT = 0x42`: Puertos de control y datos del PIT.
  - `PIT_BASE_HZ = 1193180`: Frecuencia base de oscilación del PIT.

## System Calls Relacionadas
- **`sys_beep` (Syscall 7):** Genera un sonido de frecuencia especificada por un tiempo determinado en ticks del planificador.
- **`sys_speaker_start` (Syscall 10):** Enciende el PC Speaker a una frecuencia determinada de forma asíncrona.
- **`sys_speaker_off` (Syscall 11):** Apaga el PC Speaker.

## Comments and Limitations (Comentarios y Limitaciones)
- **Monofonía Estricta:** El PC Speaker está limitado a un solo tono o frecuencia de onda cuadrada a la vez. No es posible mezclar múltiples canales de audio ni reproducir flujos de sonido digitalizados estructurados (archivos WAV/MP3).
- **Espera Activa en Beep:** El uso de `sys_beep` interrumpe/pausa temporalmente la ejecución del proceso invocante (debido al sleep bloqueante), lo que ralentiza el rendimiento de la tarea si se realizan secuencias musicales complejas.
