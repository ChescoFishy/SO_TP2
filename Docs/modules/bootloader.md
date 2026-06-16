# Bootloader Module

## Overview (¿Qué es?)
El módulo [[bootloader.md|Bootloader]] es el componente encargado de iniciar y preparar el sistema desde el encendido del hardware hasta transferir el control al [[kernel_kernel.md|Kernel]]. Transiciona el procesador desde el modo real de 16 bits heredado de la BIOS, pasando por el modo protegido de 32 bits, hasta llegar al modo largo (Long Mode) de 64 bits.

## Functionality (¿Qué hace?)
- Realiza la transición de los modos del procesador (16-bit real -> 32-bit protegido -> 64-bit largo).
- Configura e inicializa las estructuras básicas de segmentación (GDT) y paginación inicial (Identity Mapping).
- Detecta y mapea la memoria física disponible a través de ACPI y las especificaciones de hardware.
- Lee y carga del sistema de archivos BMFS (BareMetal File System) los binarios correspondientes al [[kernel_kernel.md|Kernel]] y a [[userland_shell.md|Userland]].
- Transfiere el control de ejecución saltando a la dirección donde reside la rutina de entrada de C del [[kernel_kernel.md|Kernel]].

## Internal Mechanics (¿Cómo funciona?)
1. **Inicio en MBR:** Al encender el equipo, la BIOS carga el sector de arranque de 512 bytes (`bmfs_mbr.sys`/`bmfs_mbr.asm`) en la dirección `0x7C00`. Este código formatea el disco o lee la estructura BMFS para ubicar el cargador secundario (`pure64.sys`).
2. **Setup y Modo Protegido (32-bit):** En `pure64.asm`, se deshabilitan las interrupciones del procesador (`cli`), se define una GDT temporal y se activa el bit PE (Protection Enable) del registro de control `CR0` para pasar a Modo Protegido de 32 bits.
3. **Paginación y Modo Largo (64-bit):** Se inicializan las tablas de paginación iniciales de 4 niveles (PML4, PDPT, PD, PT) mapeando la memoria física directamente (identity-mapped) para que las direcciones virtuales coincidan con las físicas. Se activa el bit LME (Long Mode Enable) en el registro EFER (Extended Feature Enable Register) y se habilita la paginación activando el bit PG de `CR0`. Finalmente, se realiza un salto largo (Far Jump) cargando el selector de segmento de código de 64 bits para entrar formalmente en Long Mode.
4. **Inicialización de SMP y ACPI:** Se detectan cores adicionales (Application Processors) y se analiza el ACPI para mapear el mapa de memoria y la información de la CPU.
5. **Carga y Salto al Kernel:** Se lee el archivo `kernel.bin` desde el BMFS en disco y se lo copia a la dirección de memoria física preestablecida (`0x100000`). Posteriormente se realiza un salto directo al punto de entrada del [[kernel_kernel.md|Kernel]].

## Key Files & Structs (Archivos y Estructuras Clave)
* **Archivos principales:**
  - `Bootloader/Pure64/src/bootsectors/bmfs_mbr.asm`: Código del primer sector (MBR) que interactúa con la BIOS y localiza a `pure64.sys`.
  - `Bootloader/Pure64/src/pure64.asm`: Archivo ensamblador principal de Pure64 que implementa la transición a 64 bits y el salto al Kernel.
  - `Bootloader/BMFS/bmfs.c`: Herramienta en C para formatear e interactuar con imágenes de disco bajo el BareMetal File System.
  - `Bootloader/Pure64/src/init/acpi.asm`: Mapeo y detección de tablas ACPI para conocer la topología de hardware.
  - `Bootloader/Pure64/src/init/cpu.asm`: Verificación y habilitación del soporte para modo largo y características de la CPU.

* **Estructuras de datos:**
  - No aplica a nivel de código de C en el Kernel, pero conceptualmente existe la tabla del directorio de archivos BMFS en el primer sector.
  
* **Funciones fundamentales:**
  - `pure64.asm (código ASM)`: Punto de inicio del cargador secundario. Configura GDT, activa paginación y carga el kernel.

## System Calls Relacionadas
No aplica. El módulo [[bootloader.md|Bootloader]] corre exclusivamente antes de la existencia de procesos de usuario o del dispatcher de interrupciones, por lo que no expone ninguna llamada al sistema directa para Userland.

## Comments and Limitations (Comentarios y Limitaciones)
- **Rigidez del Mapeo:** La configuración de paginación inicial provista por Pure64 es de tipo identidad y estática. El [[kernel_kernel.md|Kernel]] hereda este esquema.
- **Dependencia de BIOS:** Depende de llamadas de interrupción de la BIOS heredada (Legacy BIOS), lo que limita su portabilidad en plataformas modernas basadas únicamente en UEFI.
- **Ausencia de Modificación Directa:** Al ser código base provisto por la cátedra, no se modifica durante el desarrollo de los laboratorios del TP2, pero su entendimiento es crucial ya que determina la dirección inicial del [[kernel_kernel.md|Kernel]] (`0x100000`) y de [[userland_shell.md|Userland]] (`0x400000`).
