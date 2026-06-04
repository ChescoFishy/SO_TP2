# Kernel Console Module

## Overview (¿Qué es?)
El módulo de [[kernel_console.md|Console]] (Consola) en el [[kernel_kernel.md|Kernel]] es el encargado de proveer un entorno de texto rudimentario para la interacción directa y visualización por pantalla. Trabaja a bajo nivel para dibujar caracteres utilizando el framebuffer gráfico (o modo texto según corresponda) de la pantalla.

## Functionality (¿Qué hace?)
- Permite la impresión de caracteres, cadenas de texto y números en diferentes formatos (decimal, hexadecimal, etc.).
- Gestiona la posición actual del cursor gráfico o de texto (coordenadas x, y).
- Implementa funcionalidades de retroceso (backspace) y salto de línea (newline).
- Administra el desplazamiento de la pantalla (scrolling) cuando el texto llega al borde inferior.

## Internal Mechanics (¿Cómo funciona?)
1. **Dibujo de caracteres:** Si se usa un framebuffer, la consola toma una fuente mapeada en memoria (bitmaps de caracteres) y "dibuja" píxel por píxel en la dirección de memoria de video (VMEM).
2. **Control de cursor:** Mantiene variables de estado (fila, columna) que se actualizan cada vez que se imprime un carácter.
3. **Scrolling:** Cuando la fila actual excede el límite de la pantalla, el módulo desplaza todo el contenido visible hacia arriba (copiando la memoria de video) y limpia la última línea para continuar escribiendo.

## Comments and Limitations
- **Limitaciones actuales:** La velocidad de dibujo en pantalla mediante framebuffer puede ser lenta debido a copias masivas de memoria, sobre todo durante el scroll. No posee aceleración gráfica.
- **Comentarios:** Es fundamental para poder debugear (con `print` [[kernel_kernel.md|Kernel]]-side) durante el desarrollo y para mostrar resultados antes de que [[userland_shell.md|Userland]] o la terminal tomen control.
