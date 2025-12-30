# BareMetalM4 - Kernel Educativo para ARM64 (AArch64)

**BareMetalM4** es un kernel de sistema operativo *bare-metal* minimalista diseñado para aprender los fundamentos de la arquitectura **ARM64 (AArch64)**. Está configurado específicamente para ejecutarse en la placa virtual `virt` de QEMU.

Este proyecto demuestra cómo arrancar un procesador de 64 bits desde cero, gestionar la memoria de la pila, realizar cambios de contexto entre hilos y sincronizar tareas utilizando primitivas de bajo nivel y ensamblador puro.

## 🚀 Características Técnicas

*   **Arquitectura:** AArch64 (ARMv8-A). Configurado explícitamente para **Cortex-A72** (`-mcpu=cortex-a72`).
*   **Plataforma Objetivo:** QEMU `virt` machine.
*   **Compilación:** *Freestanding* (`-ffreestanding -nostdlib`), sin librerías estándar.
*   **Bootloader:** Arranque en ensamblador (`boot.S`) con filtrado de núcleos (SMP awareness) y configuración de stack.
*   **Driver UART:** Salida por consola serial simple mediante Memory Mapped I/O en la dirección física `0x09000000`.
*   **Multitarea:**
    *   Modelo de **hilos del kernel** (Kernel Threads).
    *   **Planificador Cooperativo** (Round-Robin) que gestiona hasta 64 procesos (`MAX_PROCESS`).
    *   Cada proceso tiene su propia pila dedicada de 4KB.
*   **Sincronización:**
    *   **Spinlocks:** Implementados con instrucciones exclusivas de ARMv8 (`ldxr`/`stxr`) para atomicidad hardware.
    *   **Semáforos:** Implementación personalizada con espera activa y cesión de CPU (`schedule()`) para bloqueo lógico.

## 📂 Estructura del Código

| Archivo | Descripción Técnica |
| :--- | :--- |
| **`src/boot.S`** | Punto de entrada (`_start`). Lee `MPIDR_EL1` para detener núcleos secundarios, configura el Stack Pointer (`sp`) y salta a C. |
| **`src/entry.S`** | Implementa `cpu_switch_to`. Guarda/restaura los registros *callee-saved* (`x19`-`x30`) y el stack pointer para cambiar de tarea. |
| **`src/kernel.c`** | Lógica principal. Inicializa el sistema, implementa el scheduler, las primitivas de semáforos y la demo Productor-Consumidor. |
| **`src/locks.S`** | Primitivas atómicas `spin_lock` y `spin_unlock` utilizando barreras de memoria (`dmb sy`) para asegurar coherencia. |
| **`src/sched.h`** | Define la estructura del PCB (`struct pcb`) y el contexto de CPU (`struct cpu_context`) necesario para el cambio de contexto. |
| **`link.ld`** | Script de enlazado. Define el mapa de memoria y símbolos globales como `_stack_top`. |

## 🛠️ Requisitos

Para compilar y ejecutar este proyecto, necesitas:

*   **QEMU:** `qemu-system-aarch64`
*   **Toolchain:** `aarch64-elf-gcc` (o `aarch64-linux-gnu-gcc`)
*   **Make**

**Instalación en Debian/Ubuntu:**
```bash
sudo apt update
sudo apt install gcc-aarch64-linux-gnu qemu-system-arm make
```

*Nota: El Makefile está configurado por defecto para `aarch64-elf-gcc`. Si instalas el paquete `gcc-aarch64-linux-gnu`, deberás editar el `Makefile` cambiando las variables `CC` y `LD` a `aarch64-linux-gnu-gcc` y `aarch64-linux-gnu-ld` respectivamente.*

## ⚙️ Compilación y Ejecución

1.  **Compilar el Kernel:**
    ```bash
    make
    ```

2.  **Ejecutar en QEMU:**
    ```bash
    make run
    ```
    *Para salir de QEMU: Presiona `Ctrl+A` y luego suelta y presiona `x`.*

3.  **Limpiar:**
    ```bash
    make clean
    ```

## 🧠 Análisis Interno: ¿Cómo funciona?

### 1. El Arranque (`boot.S`)
El sistema inicia en la etiqueta `_start`. La primera instrucción crítica lee el registro de sistema **MPIDR_EL1** (Multiprocessor Affinity Register) para identificar el ID del núcleo actual.
- **Núcleo 0 (Master):** Continúa la ejecución, carga la dirección de `_stack_top` en `sp` y salta a `kernel()`.
- **Otros Núcleos:** Entran en un bucle infinito (`wfe` - Wait For Event) para no interferir, ya que este kernel es *Uniprocesor* en su lógica actual.

### 2. Gestión de Procesos (`kernel.c` y `sched.h`)
El kernel mantiene un array estático de PCBs: `struct pcb process[MAX_PROCESS]`.
- **Creación (`create_thread`):** Asigna un PID libre, configura el estado a `PROCESS_READY` y prepara una pila "falsa" de 4KB. En esta pila, escribe la dirección de la función a ejecutar en el campo `pc` (Program Counter) del contexto guardado, de modo que al "restaurar" este proceso por primera vez, la CPU salte a esa función.
- **Scheduling (`schedule`):** Implementa un Round-Robin simple. Itera circularmente sobre el array de procesos buscando uno que esté `READY` o `RUNNING`. Si encuentra un candidato diferente al actual, invoca el cambio de contexto.

### 3. Cambio de Contexto (`entry.S`)
La función clave es `cpu_switch_to(struct pcb *prev, struct pcb *next)`, escrita en ensamblador puro:
1.  **Guardar Contexto (`prev`):** Almacena los registros `x19` a `x30` en la memoria apuntada por `prev`. En ARM64, estos son los registros que una función debe preservar (*callee-saved*). También guarda el `sp` actual.
    *   *Nota: `x29` es el Frame Pointer y `x30` es el Link Register (dirección de retorno).*
2.  **Cargar Contexto (`next`):** Lee los valores guardados en la estructura de `next` y los vuelca en los registros físicos de la CPU.
3.  **Salto:** Al ejecutar la instrucción `ret`, el procesador usa el valor restaurado en `x30` (LR) para saltar, reanudando efectivamente la ejecución donde se quedó el nuevo proceso.

### 4. Sincronización y Demo
El kernel arranca demostrando el problema **Productor-Consumidor**:
- **Productor:** Genera números incrementales y llena un buffer circular de tamaño 4. Simula trabajo con un retardo (~200ms).
- **Consumidor:** Lee del buffer. Simula ser más lento (~800ms) para demostrar cómo el productor se bloquea cuando el buffer se llena.
- **Semáforos (`sem_wait` / `sem_signal`):**
    - Protegen el acceso al buffer (`buffer`, `in`, `out`).
    - Si un semáforo está en 0 (rojo), `sem_wait` llama explícitamente a `schedule()`, cediendo voluntariamente la CPU hasta que otro hilo libere el recurso.

---
*Proyecto educativo para demostración de sistemas operativos en AArch64.*