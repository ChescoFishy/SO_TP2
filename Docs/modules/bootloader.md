# Bootloader Module

## Overview (¿Qué es?)
El módulo [[bootloader.md|Bootloader]] es el componente responsable de cargar el sistema operativo en la memoria RAM cuando se enciende o reinicia la computadora. En este proyecto (x64BareBones), se encarga de realizar la transición desde el entorno inicial de BIOS (16-bit) hasta el modo protegido y, finalmente, al modo largo (64-bit) necesario para ejecutar el [[kernel_kernel.md|Kernel]].

## Functionality (¿Qué hace?)
- Inicializa el hardware básico.
- Configura las tablas de descriptores globales (GDT) necesarias para cambiar los modos del procesador.
- Pasa el procesador a modo protegido (32-bit) y posteriormente a modo largo (64-bit).
- Carga en memoria la sección del [[kernel_kernel.md|Kernel]] y el entorno de usuario ([[userland_shell.md|Userland]]).
- Transfiere el flujo de ejecución (salto) al punto de entrada del [[kernel_kernel.md|Kernel]].

## Internal Mechanics (¿Cómo funciona?)
1. **Punto de entrada:** La BIOS carga el primer sector (MBR) que contiene el código de inicio (generalmente 16 bits).
2. **Setup y Modo Protegido:** Se establecen registros de segmento y se activa el bit PE en el registro CR0 para entrar en Modo Protegido, habilitando el uso de 32 bits y memoria por encima de 1MB.
3. **Paginación y Modo Largo (64-bit):** Se preparan las tablas de paginación (PML4, PDP, PD, PT) de forma identity-mapped y se configura el registro EFER (Extended Feature Enable Register) para habilitar el Modo Largo (Long Mode).
4. **Carga y Salto al [[kernel_kernel.md|Kernel]]:** Descomprime y sitúa los binarios del [[kernel_kernel.md|Kernel]] y [[userland_shell.md|Userland]] en sus direcciones preestablecidas. Una vez todo listo, se realiza un salto absoluto a la dirección de memoria donde reside el [[kernel_kernel.md|Kernel]].

## Comments and Limitations
- **Limitaciones actuales:** El [[bootloader.md|Bootloader]] es proporcionado por la cátedra (Pure64/BareBones) y no es parte de las implementaciones a modificar, lo que significa que su configuración de memoria y paginación inicial es rígida.
- **Comentarios:** No es necesario modificar este módulo durante el desarrollo habitual del trabajo práctico, pero comprender cómo deja la memoria mapeada es vital para la implementación del [[kernel_memory_manager.md|Memory Manager]] y el [[kernel_kernel.md|Kernel]].
