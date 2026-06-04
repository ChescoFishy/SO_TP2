# Kernel Sound Module

## Overview (¿Qué es?)
El módulo de Sonido (Sound) administra el PC Speaker nativo presente (o emulado) en la arquitectura x86. Permite generar pitidos (beeps) de frecuencias específicas.

## Functionality (¿Qué hace?)
- Enciende y apaga el altavoz del sistema.
- Configura la frecuencia del sonido emitido.
- Permite emitir "beeps" audibles para alertas del sistema o para ser utilizado por programas de espacio de usuario.

## Internal Mechanics (¿Cómo funciona?)
1. **PIT (Programmable Interval Timer):** Utiliza el canal 2 del PIT (puerto 42h) para generar una onda cuadrada que controlará el PC speaker.
2. **Frecuencia:** Para emitir una frecuencia `F`, calcula el divisor como `1193180 / F` y se lo envía al PIT.
3. **Puertos I/O:** Para que empiece a sonar, se modifican bits específicos del puerto de control de hardware general (puerto 61h), conectando la salida del PIT al speaker. Para silenciar, se apagan esos bits.

## Comments and Limitations
- **Limitaciones actuales:** Solo permite tonos cuadrados básicos monofónicos y carece de control de volumen real. No es un driver de tarjeta de sonido avanzado (como AC97/HDAudio) por lo que no puede reproducir archivos PCM (WAV/MP3).
- **Comentarios:** Útil para crear notificaciones audibles o juegos sencillos en la terminal.
