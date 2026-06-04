# Kernel Drivers Module

## Overview (¿Qué es?)
El módulo de [[kernel_drivers.md|Drivers]] es la capa de abstracción del sistema operativo que permite comunicarse directamente con los dispositivos de hardware. Incluye principalmente los controladores básicos como teclado (Keyboard) y pantalla (Video).

## Functionality (¿Qué hace?)
- **Teclado:** Lee los scancodes del controlador del teclado a través del puerto de I/O cuando ocurre una interrupción de hardware.
- **Video:** Proporciona funciones de bajo nivel para dibujar píxeles y rectángulos en la pantalla si se usa VESA/Framebuffer.
- Centraliza la comunicación in/out (puertos 60h, 64h, etc.) y encapsula el comportamiento del hardware subyacente.

## Internal Mechanics (¿Cómo funciona?)
1. **Teclado:** Cuando el usuario presiona una tecla, se lanza una interrupción (IRQ 1). El handler de la interrupción llama al driver del teclado que ejecuta una instrucción `in` en el puerto apropiado para obtener el 'scancode'. El driver mapea este código a un carácter ASCII manejando el estado de modificadores (Shift, Caps Lock).
2. **Video:** Utiliza la información proporcionada por VESA u otras estructuras preestablecidas durante el boot para conocer la dirección base del framebuffer, la resolución y la profundidad de color (BPP). Para dibujar, calcula el offset `(y * width + x) * (bpp/8)` e inserta el valor del color.

## Comments and Limitations
- **Limitaciones actuales:** Soporte de hardware muy limitado. El teclado puede que sólo soporte una distribución de teclas fija (ej. US layout) y carece de soporte avanzado para combinaciones complejas.
- **Comentarios:** El teclado generalmente cuenta con un buffer circular implementado aquí para almacenar las teclas presionadas hasta que algún proceso (mediante read) las solicite.
