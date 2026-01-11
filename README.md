# BareMetalM4 - Kernel Educativo para ARM64 (AArch64)

**BareMetalM4** es un kernel de sistema operativo *bare-metal* minimalista diseñado para aprender los fundamentos de la arquitectura **ARM64 (AArch64)**. Está configurado específicamente para ejecutarse en la placa virtual `virt` de QEMU.

Este proyecto demuestra cómo arrancar un procesador de 64 bits desde cero, gestionar la memoria de la pila, realizar cambios de contexto entre hilos y sincronizar tareas utilizando primitivas de bajo nivel y ensamblador puro.

## 🚀 Características Técnicas

*   **Arquitectura:** AArch64 (ARMv8-A). Configurado explícitamente para **Cortex-A72** (`-mcpu=cortex-a72`).
*   **Plataforma Objetivo:** QEMU `virt` machine.
*   **Compilación:** *Freestanding* (`-ffreestanding -nostdlib`), sin librerías estándar.
*   **Bootloader:** Arranque en ensamblador (`boot.S`) con filtrado de núcleos (SMP awareness) y configuración de stack.
*   **Driver UART:** Salida por consola serial simple mediante Memory Mapped I/O en la dirección física `0x09000000`.
*   **Gestión de Memoria:**
    *   **MMU (Memory Management Unit):** Sistema de memoria virtual con traducción de direcciones.
    *   **Tablas de Páginas:** L1 con bloques de 1 GB (39 bits de espacio virtual).
    *   **Identity Mapping:** Memoria física = virtual para simplificar acceso.
    *   **Tipos de Memoria:** Device (periféricos) y Normal (RAM con caches).
*   **Multitarea:**
    *   Modelo de **hilos del kernel** (Kernel Threads).
    *   **Planificador Expropiativo** con Aging que gestiona hasta 64 procesos (`MAX_PROCESS`).
    *   Cada proceso tiene su propia pila dedicada de 4KB.
    *   **Cambio de contexto por timer:** Interrupciones de timer generadas por el GIC (Generic Interrupt Controller) cada `TIMER_INTERVAL` ciclos.
*   **Gestión de Interrupciones:**
    *   **Vector de Excepciones (`VBAR_EL1`):** Tabla de manejo de interrupciones.
    *   **GIC v2:** Control de interrupciones hardware (distribuidor + interfaz de CPU).
    *   **Timer del Sistema:** Timer virtual de AArch64 (`CNTP_TVAL_EL0`) con período configurable.
    *   **IRQ Handler en Ensamblador:** Preservación completa de registros (all-save context) para interrupciones.
*   **Sincronización:**
    *   **Spinlocks:** Implementados con instrucciones exclusivas de ARMv8 (`ldxr`/`stxr`) para atomicidad hardware.
    *   **Semáforos:** Implementación personalizada con espera activa y cesión de CPU (`schedule()`) para bloqueo lógico.

## 📂 Estructura del Código

**Nota**: El kernel ha sido refactorizado en módulos especializados (v0.3 - Enero 2026). Ver [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) para detalles completos.

| Archivo | Descripción Técnica |
| :--- | :--- |
| **`src/boot.S`** | Punto de entrada (`_start`). Lee `MPIDR_EL1` para detener núcleos secundarios, configura el Stack Pointer (`sp`) y salta a C. |
| **`src/entry.S`** | Implementa `cpu_switch_to` (cambio de contexto en procesos). Guarda/restaura registros *callee-saved* (`x19`-`x30`) y stack pointer. Incluye `irq_handler_stub` para manejo de interrupciones con preservación completa de contexto. |
| **`src/mm.c`** | Subsistema de memoria virtual (MMU). Configura tablas de páginas L1, mapea periféricos y RAM, activa MMU y caches. |
| **`src/mm_utils.S`** | Funciones de bajo nivel para MMU (mrs/msr). Acceso a registros `MAIR_EL1`, `TCR_EL1`, `TTBR0/1_EL1`, `SCTLR_EL1` e invalidación del TLB. |
| **`src/kernel/kernel.c`** | Punto de entrada principal. Inicializa MMU, scheduler con Aging, crea shell y procesos de prueba, habilita timer. |
| **`src/kernel/process.c`** | Gestión de procesos: PCB, `create_thread()`, `exit()`, variables globales (`process[]`, `current_process`, stacks). |
| **`src/kernel/scheduler.c`** | Algoritmo de scheduling con aging y prioridades, `sleep()`, `timer_tick()`, gestión de `sys_timer_count`. |
| **`src/shell/shell.c`** | Shell interactivo con comandos (help, ps, clear, panic, poweroff). Procesos de prueba (`proceso_1`, `proceso_2`, `proceso_mortal`). |
| **`src/utils/kutils.c`** | Utilidades del kernel: `panic()`, `delay()`, `k_strcmp()`, `k_strncpy()`. |
| **`src/locks.S`** | Primitivas atómicas `spin_lock` y `spin_unlock` con instrucciones exclusivas (`ldxr`/`stxr`) y barreras de memoria (`dmb sy`). |
| **`src/timer.c`** | Inicialización del GIC v2, setup del timer virtual (`CNTP_TVAL_EL0`), y manejador `handle_timer_irq` para expropiación del scheduler. |
| **`src/vectors.S`** | Tabla de vectores de excepciones (`VBAR_EL1`). Enruta IRQs, SysCalls, excepciones de sincronización, etc. |
| **`src/utils.S`** | Funciones utilidad en ensamblador. Setup de registros del sistema (`VBAR_EL1`, `SPSEL`, etc.). |
| **`include/mm.h`** | Interfaz del subsistema de memoria. Define funciones para configurar MMU y registros de sistema. |
| **`include/kernel/`** | Headers de módulos: `kutils.h`, `process.h`, `scheduler.h`, `shell.h`. |
| **`include/sched.h`** | Define estructura del PCB (`struct pcb`) con contexto de CPU (`struct cpu_context`) y estados de proceso. |
| **`include/timer.h`** | Defines para el GIC v2 (registros y direcciones) y configuración del timer. |
| **`link.ld`** | Script de enlazado. Define mapa de memoria y símbolos globales como `_stack_top` y `vectors`. |

## 🛠️ Requisitos

Para compilar y ejecutar este proyecto, necesitas:

*   **QEMU:** `qemu-system-aarch64`
*   **Toolchain AArch64 bare-metal:** `aarch64-none-elf-gcc` (o `aarch64-linux-gnu-gcc` con ajuste en `Makefile`)
*   **Make**

### macOS (Apple Silicon M1/M2/M3/M4)
> Probado en MacBook M4 (ARM). QEMU usa aceleración HVF para AArch64.
```bash
brew update
brew install qemu
brew install aarch64-none-elf-gcc
```
* Si usas `aarch64-linux-gnu-gcc`, ajusta en el `Makefile` las variables `CC` y `LD` a `aarch64-linux-gnu-gcc` y `aarch64-linux-gnu-ld`.

### Linux (host ARM64 requerido)
> Se necesita CPU ARM. En x86_64 solo funcionaría con emulación TCG muy lenta y no recomendada.

**Instalación en Debian/Ubuntu (ARM64):**
```bash
sudo apt update
sudo apt install gcc-aarch64-linux-gnu qemu-system-arm make
```

*Nota: El `Makefile` está configurado por defecto para `aarch64-elf-gcc`. Si usas `gcc-aarch64-linux-gnu`, cambia `CC` y `LD` a `aarch64-linux-gnu-gcc` y `aarch64-linux-gnu-ld` respectivamente.*

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

### 2. Inicialización del Sistema (`kernel.c`)
En `kernel()`:
1. **Llamada a `timer_init()`:** Configura el GIC v2 (distribuidor e interfaz de CPU), activa la interrupción del timer (ID 30) y establece `VBAR_EL1` apuntando a la tabla de vectores.
2. **Creación de procesos:** Se lanzan dos hilos de ejemplo (`proceso_1` y `proceso_2`) que imprimen contadores y llaman `enable_interrupts()` para permitir las IRQ de timer.
3. **Bucle de espera:** El kernel ejecuta `wfi()` (Wait For Interrupt) en un ciclo infinito. Las interrupciones del timer disparan `irq_handler_stub` y fuerzan `schedule()` cada `TIMER_INTERVAL` ciclos.

### 3. Gestión de Procesos (`kernel.c` y `sched.h`)
El kernel mantiene un array estático de PCBs: `struct pcb process[MAX_PROCESS]`.

**Creación (`create_thread`):**
- Asigna un PID libre
- Configura el estado a `PROCESS_READY`
- Prepara una pila de 4KB e inicializa el contexto guardado con la dirección de la función (`pc`)
- Al "restaurar" este proceso por primera vez, la CPU salta directamente a esa función

**Scheduling (`schedule`) - Planificador con Aging:**
1. **Fase de Envejecimiento:** Para cada proceso `READY` que NO es el actual, decrementa su prioridad (si es mayor que 0). Esto "premia" a procesos que esperan.
2. **Fase de Selección:** Busca el proceso `READY/RUNNING` con la **menor** variable `priority` (números bajos = más urgentes).
3. **Penalización:** El nuevo proceso seleccionado recibe incremento de prioridad para evitar monopolio de CPU.
4. **Context Switch:** Se invoca `cpu_switch_to()` para guardar contexto del proceso actual y cargar el del nuevo.

### 4. Cambio de Contexto (`entry.S`)
La función clave es `cpu_switch_to(struct pcb *prev, struct pcb *next)`, escrita en ensamblador puro:

1. **Guardar Contexto (`prev`):** Almacena los registros `x19` a `x30` en memoria. En ARM64, estos son *callee-saved* (una función debe preservarlos). También guarda `sp`.
   - `x29` = Frame Pointer
   - `x30` = Link Register (dirección de retorno)
2. **Cargar Contexto (`next`):** Lee valores guardados y los restaura en registros físicos.
3. **Salto:** La instrucción `ret` usa el `x30` restaurado para saltar, reanudando la ejecución donde se quedó el nuevo proceso.

### 5. Manejo de Interrupciones
El timer del sistema genera una interrupción cada `TIMER_INTERVAL` ciclos:

1. **Vector de Excepciones (`vectors.S`):** La tabla apuntada por `VBAR_EL1` redirige IRQs a `irq_handler_stub`.
2. **IRQ Handler Stub (`entry.S`):**
   - Guarda **todos** los registros (x0-x29, sp, x30) en la pila (all-save context)
   - Guarda estado de excepción: `SPSR_EL1` (CPU state) y `ELR_EL1` (dirección de retorno)
   - Llama a la función C `handle_timer_irq()`
   - Restaura todo lo anterior y ejecuta `eret` (exception return)
3. **Handler C (`timer.c`):**
   - Lee el IAR (Interrupt Acknowledge Register) del GIC para obtener el ID de la interrupción
   - Escribir en EOIR (End of Interrupt Register) señaliza fin de la interrupción
   - Si es el timer (ID 30), recarga el valor del timer y llama a `schedule()`

### 6. Demo actual y Shell interactivo
- **Shell Interactivo:** El sistema arranca con un shell que acepta comandos:
  - `help` - Muestra comandos disponibles
  - `ps` - Lista todos los procesos (PID, prioridad, estado, nombre)
  - `clear` - Limpia la pantalla
  - `panic` - Provoca un kernel panic (demo)
  - `poweroff` - Apaga el sistema
- **Procesos de Prueba:** Se pueden crear procesos que imprimen contadores mientras el timer expropia periódicamente, mostrando el cambio de contexto en vivo.
- **Semáforos (`sem_wait` / `sem_signal`):** Disponibles en `src/semaphore.c` con spinlock global para atomicidad. Puedes integrarlos en tus propios procesos para experimentar bloqueo/desbloqueo cooperativo + expropiativo.

---

## 📖 Documentación Completa

Para una guía detallada de la arquitectura, módulos y funcionamiento interno del sistema, consulta:

- **[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md)** - Arquitectura completa del kernel
  - Estructura modular del código
  - Componentes principales
  - Flujo de ejecución
  - Subsistemas (scheduler, interrupciones, sincronización)
  - Decisiones de diseño

---
*Proyecto educativo para demostración de sistemas operativos en AArch64.*

## 📚 Apéndice: El Planificador con Aging

El scheduler de BareMetalM4 implementa un algoritmo **Round-Robin mejorado con Aging** para prevenir inanición (starvation):

```
ESTADO INICIAL (3 procesos):
┌──────┬──────────┬──────────┐
│ PID  │ Estado   │ Priority │
├──────┼──────────┼──────────┤
│  0   │ RUNNING  │    5     │  ← Proceso actual
│  1   │ READY    │    8     │
│  2   │ READY    │    8     │
└──────┴──────────┴──────────┘

CICLO 1 - SCHEDULE():
  Fase Envejecimiento: P1 (8 → 7), P2 (8 → 7)
  Fase Selección: P0 tiene prioridad 5 (más urgente) → se queda
  
CICLO 2 - SCHEDULE() (por timer):
  Fase Envejecimiento: P1 (7 → 6), P2 (7 → 6)
  Fase Selección: P0 (5) vs P1 (6) → P0 gana de nuevo
  
CICLO 3 - SCHEDULE():
  Fase Envejecimiento: P1 (6 → 5), P2 (6 → 5)
  Fase Selección: Empate (todos en 5) → FIFO, P0 sigue
  
CICLO 4 - SCHEDULE():
  Fase Envejecimiento: P1 (5 → 4), P2 (5 → 4)
  Fase Selección: P1 (4) vs P0 (5) → P1 GANA, context switch
  Penalización: P1 (4 + 2 = 6)
  
RESULTADO: Se garantiza que incluso procesos de baja prioridad
           eventualmente ejecutarán (prevención de starvation)
```

Este modelo es educativo y demuestra cómo sistemas reales (Linux con `nice`, etc.) manejan prioridades sin dejar procesos sin ejecutar nunca.