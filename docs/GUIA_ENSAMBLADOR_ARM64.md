# Guía de Ensamblador ARM64: Aprende con BareMetalM4

## Índice

1. [Introducción](#introducción)
2. [Conceptos Básicos de ARM64](#conceptos-básicos-de-arm64)
3. [Registros y Convenciones](#registros-y-convenciones)
4. [Instrucciones Fundamentales](#instrucciones-fundamentales)
5. [Análisis del Código del Proyecto](#análisis-del-código-del-proyecto)
6. [Casos de Uso Prácticos](#casos-de-uso-prácticos)
7. [Ejercicios y Práctica](#ejercicios-y-práctica)

---

## Introducción

Esta guía te enseñará ensamblador ARM64 (AArch64) usando el código real del proyecto BareMetalM4. Aprenderás analizando código que realmente funciona en un sistema operativo bare-metal.

### ¿Por qué usar ensamblador?

En un sistema operativo bare-metal, ciertas operaciones **solo** pueden hacerse en ensamblador:
- Inicialización del procesador (boot)
- Cambio de contexto entre procesos
- Manejo de interrupciones
- Operaciones atómicas (sincronización)
- Acceso a registros del sistema (MMU, timers, etc.)

---

## Conceptos Básicos de ARM64

### Arquitectura

**ARM64** (también llamado **AArch64**) es una arquitectura RISC de 64 bits:
- **RISC**: Reduced Instruction Set Computer (instrucciones simples y regulares)
- **64 bits**: Puede direccionar hasta 2^64 bytes de memoria
- **Load-Store**: Solo `ldr`/`str` acceden a memoria, el resto opera en registros

### Niveles de Excepción (Exception Levels)

ARM64 tiene 4 niveles de privilegio:

```
┌────────────────────────────────────────┐
│ EL3 - Secure Monitor (TrustZone)       │
├────────────────────────────────────────┤
│ EL2 - Hypervisor (Virtualización)      │
├────────────────────────────────────────┤
│ EL1 - Kernel (Sistema Operativo) ←AQUÍ │
├────────────────────────────────────────┤
│ EL0 - User Mode (Aplicaciones)         │
└────────────────────────────────────────┘
```

Nuestro kernel corre en **EL1** y los procesos de usuario en **EL0**.

---

## Registros y Convenciones

### Registros de Propósito General

ARM64 tiene **31 registros de propósito general** más el stack pointer:

```assembly
x0-x30   ; Registros de 64 bits (0 a 30)
w0-w30   ; Versiones de 32 bits de los mismos registros

xzr/wzr  ; Registro "zero" (siempre contiene 0)
sp       ; Stack Pointer (puntero de pila)
pc       ; Program Counter (no accesible directamente)
```

### Convención de Llamada (AAPCS64)

| Registro | Uso                          | ¿Se preserva? |
|----------|------------------------------|---------------|
| x0-x7    | Argumentos y valores retorno | No            |
| x8       | Indirecto result/syscall     | No            |
| x9-x15   | Temporales                   | No            |
| x16-x17  | Intra-procedure-call scratch | No            |
| x18      | Platform register            | Depende       |
| x19-x28  | Callee-saved                 | **Sí**        |
| x29      | Frame Pointer (FP)           | **Sí**        |
| x30      | Link Register (LR)           | **Sí**        |
| sp       | Stack Pointer                | **Sí**        |

**Callee-saved**: La función llamada debe preservar estos registros.

### Registros de Sistema

```assembly
; Registros de control
SCTLR_EL1   ; System Control (MMU, caches)
VBAR_EL1    ; Vector Base Address (tabla de excepciones)
MPIDR_EL1   ; Multiprocessor ID (identificar core)

; Registros de estado
SPSR_EL1    ; Saved Program Status Register
ELR_EL1     ; Exception Link Register (dirección de retorno)
ESR_EL1     ; Exception Syndrome Register (razón de excepción)

; Gestión de memoria
TTBR0_EL1   ; Translation Table Base 0 (tabla de páginas user)
TTBR1_EL1   ; Translation Table Base 1 (tabla de páginas kernel)
TCR_EL1     ; Translation Control Register
MAIR_EL1    ; Memory Attribute Indirection Register

; Timer
CNTFRQ_EL0  ; Counter Frequency (Hz del timer)
CNTP_TVAL_EL0  ; Timer Value (ticks restantes)
CNTP_CTL_EL0   ; Timer Control (enable/disable)

; Control de interrupciones
DAIF        ; Debug, SError, IRQ, FIQ masks
```

---

## Instrucciones Fundamentales

### 1. Movimiento de Datos

```assembly
mov x0, x1          ; x0 = x1
mov x0, #42         ; x0 = 42 (inmediato)
mvn x0, x1          ; x0 = ~x1 (NOT bitwise)
```

### 2. Carga y Almacenamiento (Load/Store)

```assembly
; Cargar desde memoria
ldr x0, [x1]        ; x0 = *x1 (64 bits)
ldr w0, [x1]        ; w0 = *x1 (32 bits)
ldp x0, x1, [x2]    ; Load Pair: x0=*x2, x1=*(x2+8)

; Almacenar en memoria
str x0, [x1]        ; *x1 = x0 (64 bits)
str w0, [x1]        ; *x1 = w0 (32 bits)
stp x0, x1, [x2]    ; Store Pair: *x2=x0, *(x2+8)=x1

; Modos de direccionamiento
ldr x0, [x1, #8]    ; x0 = *(x1 + 8)
ldr x0, [x1], #8    ; x0 = *x1, luego x1 += 8 (post-increment)
ldr x0, [x1, #8]!   ; x1 += 8, luego x0 = *x1 (pre-increment)
```

### 3. Aritmética

```assembly
add x0, x1, x2      ; x0 = x1 + x2
sub x0, x1, x2      ; x0 = x1 - x2
add x0, x1, #10     ; x0 = x1 + 10
mul x0, x1, x2      ; x0 = x1 * x2
```

### 4. Lógica

```assembly
and x0, x1, x2      ; x0 = x1 & x2 (AND)
orr x0, x1, x2      ; x0 = x1 | x2 (OR)
eor x0, x1, x2      ; x0 = x1 ^ x2 (XOR)
lsl x0, x1, #2      ; x0 = x1 << 2 (Logical Shift Left)
lsr x0, x1, #2      ; x0 = x1 >> 2 (Logical Shift Right)
```

### 5. Comparación y Saltos

```assembly
; Comparación (afecta flags: N, Z, C, V)
cmp x0, x1          ; Compara x0 - x1 (no guarda resultado)
cbz x0, label       ; Compare and Branch if Zero
cbnz x0, label      ; Compare and Branch if Not Zero

; Saltos condicionales
b.eq label          ; Branch if Equal (Z=1)
b.ne label          ; Branch if Not Equal (Z=0)
b.lt label          ; Branch if Less Than (N!=V)
b.gt label          ; Branch if Greater Than (Z=0 && N==V)
b.le label          ; Branch if Less or Equal
b.ge label          ; Branch if Greater or Equal

; Saltos incondicionales
b label             ; Branch (salto simple)
bl function         ; Branch with Link (guarda PC en x30)
br x0               ; Branch to Register (saltar a dirección en x0)
blr x0              ; Branch with Link to Register
ret                 ; Return (equivale a br x30)
```

### 6. Acceso a Registros de Sistema

```assembly
; Leer registro de sistema -> registro general
mrs x0, SCTLR_EL1   ; x0 = SCTLR_EL1

; Escribir registro general -> registro de sistema
msr SCTLR_EL1, x0   ; SCTLR_EL1 = x0
```

### 7. Barreras de Memoria

```assembly
dmb sy              ; Data Memory Barrier (System)
dsb sy              ; Data Synchronization Barrier (System)
isb                 ; Instruction Synchronization Barrier
```

### 8. Operaciones Atómicas

```assembly
; Load/Store Exclusive (para locks)
ldxr w0, [x1]       ; Load Exclusive (marca dirección)
stxr w2, w0, [x1]   ; Store Exclusive (w2=0 si éxito, 1 si fallo)
stlr w0, [x1]       ; Store-Release (semántica de liberación)
```

### 9. Instrucciones Especiales

```assembly
wfe                 ; Wait For Event (bajo consumo)
wfi                 ; Wait For Interrupt
sev                 ; Send Event (despertar cores en WFE)
nop                 ; No Operation
hlt #imm            ; Halt (usado para semihosting/debugging)
eret                ; Exception Return (volver de excepción)
```

---

## Análisis del Código del Proyecto

### 1. boot.S - Inicialización del Sistema

**Ubicación**: `src/boot.S`

Este archivo es el **punto de entrada** del kernel. Es lo primero que ejecuta el CPU.

```assembly
_start:
    /* PASO 1: Identificar el Core (solo core 0 ejecuta el kernel) */
    mrs x0, MPIDR_EL1       /* Leer ID del procesador */
    and x0, x0, #3          /* Extraer Core ID (bits [1:0]) */
    cbz x0, master          /* Si core 0 -> saltar a master */
    b hang                  /* Cores secundarios -> dormir */
```

**Análisis línea por línea**:

1. `mrs x0, MPIDR_EL1`: 
   - **MRS** = "Move to Register from System register"
   - Lee el registro **MPIDR_EL1** (Multiprocessor Affinity Register)
   - Este registro contiene el ID único del core actual
   
2. `and x0, x0, #3`:
   - Aplica una máscara de bits `0b11` (3 en decimal)
   - Extrae solo los bits [1:0] que contienen el Core ID (0-3)
   - Ejemplo: Si MPIDR=0x80000001, después del AND: x0=0x1

3. `cbz x0, master`:
   - **CBZ** = "Compare and Branch if Zero"
   - Si x0 == 0 (somos el core 0), salta a `master`
   - Si x0 != 0 (cores secundarios), continúa a la siguiente línea

4. `b hang`:
   - Salto incondicional a `hang` (bucle infinito)
   - Los cores secundarios se quedan aquí esperando

**Continuación: Configurar el Stack**

```assembly
master:
    /* PASO 2: Configurar el Stack Pointer */
    ldr x0, =_stack_top     /* Cargar direccion del tope del stack */
    mov sp, x0              /* SP = Stack Pointer */
```

- `ldr x0, =_stack_top`: Carga la **dirección** del símbolo `_stack_top` (definido en link.ld)
- `mov sp, x0`: Establece el stack pointer al tope del stack

**Limpieza de la sección BSS**

```assembly
    ldr x0, =__bss_start
    ldr x1, =__bss_end
    sub x2, x1, x0          /* x2 = Tamaño de BSS */
    cbz x2, run_kernel      /* Si el tamaño es 0, saltar */

clear_bss_loop:
    str xzr, [x0], #8       /* Escribir 0 (xzr) y avanzar 8 bytes */
    sub x2, x2, #8          /* Restar 8 al contador */
    cbnz x2, clear_bss_loop /* Si no es 0, repetir */
```

**BSS** (Block Started by Symbol) = Sección de memoria para variables globales no inicializadas.

- `str xzr, [x0], #8`: 
  - Escribe cero en `[x0]`
  - Luego incrementa x0 en 8 (post-increment)
  - `xzr` = registro siempre-cero

**Saltar al kernel en C**

```assembly
run_kernel:
    bl kernel               /* Branch with Link a kernel() */
```

- `bl kernel`: Llama a la función `kernel()` en C
- Guarda la dirección de retorno en x30 (LR)

---

### 2. entry.S - Cambio de Contexto e Interrupciones

**Ubicación**: `src/entry.S`

Este archivo implementa el **context switch** (cambio de contexto entre procesos).

#### Macro: kernel_entry (Guardar Contexto)

```assembly
.macro kernel_entry
    sub sp, sp, #256
    stp x0, x1, [sp, #16 * 0]
    stp x2, x3, [sp, #16 * 1]
    ; ... (guarda x4-x29)
    mrs x21, spsr_el1
    mrs x22, elr_el1
    stp x30, x21, [sp, #16 * 15]
    str x22, [sp, #16 * 16]
.endm
```

**¿Qué hace?**
1. Reserva 256 bytes en el stack (`sub sp, sp, #256`)
2. Guarda todos los registros x0-x30 usando `stp` (Store Pair)
3. Guarda el estado de la excepción:
   - **SPSR_EL1**: Saved Program Status Register (flags del CPU)
   - **ELR_EL1**: Exception Link Register (dirección de retorno)

**¿Por qué es necesario?**
Cuando ocurre una interrupción, el proceso actual debe "congelar" su estado completo para poder continuar más tarde exactamente donde se quedó.

#### Función: cpu_switch_to (Cambio de Contexto)

```assembly
cpu_switch_to:
    /* Guardar contexto de 'prev' */
    stp x19, x20, [x0, #0]
    stp x21, x22, [x0, #16]
    stp x23, x24, [x0, #32]
    stp x25, x26, [x0, #48]
    stp x27, x28, [x0, #64]
    stp x29, x30, [x0, #80]    /* FP y LR */
    mov x9, sp
    str x9, [x0, #96]          /* SP */

    /* Restaurar contexto de 'next' */
    ldp x19, x20, [x1, #0]
    ldp x21, x22, [x1, #16]
    ; ... (restaura x23-x30)
    ldr x9, [x1, #96]
    mov sp, x9

    ret    /* Saltar a la direccion en x30 (LR de 'next') */
```

**Parámetros**:
- `x0`: Puntero al PCB (Process Control Block) del proceso **saliente**
- `x1`: Puntero al PCB del proceso **entrante**

**¿Qué hace?**
1. Guarda los registros **callee-saved** (x19-x30, sp) del proceso actual en su PCB
2. Restaura los registros del nuevo proceso desde su PCB
3. Hace `ret`, que salta a la dirección en x30 (el LR del nuevo proceso)

**Convención de registros callee-saved**:
Solo guardamos x19-x30 porque según la **AAPCS64** (ARM Procedure Call Standard), estos son los únicos que la función debe preservar. Los registros x0-x18 son "caller-saved" (el llamante los guarda si los necesita).

#### Función: ret_from_fork (Entrada de Nuevos Procesos)

```assembly
ret_from_fork:
    bl schedule_tail

    mov x0, x20

    blr x19

    /* Si la funcion retorna (acaba), caemos aqui */
    bl exit             /* Llamamos a exit() para morir ordenadamente */

    /* Nunca deberiamos llegar aqui, pero por si acaso... */
    bl hang
```

**¿Cuándo se ejecuta?**
Cuando un proceso **recién creado** obtiene CPU por primera vez.

**¿Qué hace?**
1. Llama a `schedule_tail()` (hook post-fork en C)
2. Mueve x20 a x0 (argumento para la función del proceso)
3. Salta a la dirección en x19 (función principal del proceso)
4. Si la función retorna, llama a `exit()` automáticamente

**¿Por qué x19 y x20?**
Cuando se crea un proceso en `copy_process()` (en C), se configuran:
- `x19 = función a ejecutar`
- `x20 = argumento de la función`

---

### 3. locks.S - Sincronización Atómica

**Ubicación**: `src/locks.S`

Este archivo implementa **spinlocks** usando instrucciones atómicas ARM64.

#### Función: spin_lock (Adquirir Lock)

```assembly
spin_lock:
    mov w1, #1
    
1:  /* Bucle de espera activa */
    ldxr w2, [x0]      /* Lee lock + marca como exclusiva */
    cbnz w2, 1b        /* Si tomado (!=0), reintentar */
    stxr w3, w1, [x0]  /* Intentar escribir 1 */
    cbnz w3, 1b        /* Si fallo (!=0), reintentar */
    dmb sy             /* Barrera de memoria (sincronizar) */
    ret
```

**Análisis**:

1. `mov w1, #1`: Preparar el valor 1 para escribir

2. **Label `1:`**: Etiqueta local (bucle de reintento)

3. `ldxr w2, [x0]`:
   - **LDXR** = "Load Exclusive Register"
   - Lee el valor del lock en w2
   - **Marca la dirección como "exclusiva"** para este core
   - Esto crea un "monitor" de exclusividad

4. `cbnz w2, 1b`:
   - Si w2 != 0 (lock tomado), vuelve al label `1` hacia atrás (`b` = backward)

5. `stxr w3, w1, [x0]`:
   - **STXR** = "Store Exclusive Register"
   - Intenta escribir w1 (1) en [x0]
   - **Solo tiene éxito si la exclusividad no fue rota** por otro core
   - Retorna resultado en w3: 0=éxito, 1=fallo

6. `cbnz w3, 1b`:
   - Si w3 != 0 (la escritura falló), reintentar

7. `dmb sy`:
   - **DMB** = "Data Memory Barrier"
   - Garantiza que todas las operaciones de memoria anteriores se completen
   - `sy` = system-wide (afecta a todos los cores)

**¿Por qué es atómico?**
La secuencia **LDXR + STXR** implementa un "**test-and-set atómico**":
- Si otro core modifica la dirección entre LDXR y STXR, el STXR falla
- Esto previene **race conditions** (condiciones de carrera)

#### Función: spin_unlock (Liberar Lock)

```assembly
spin_unlock:
    dmb sy             /* Completar operaciones previas */
    stlr wzr, [x0]     /* Escribir 0 (libre) con release semantics */
    ret
```

- `dmb sy`: Asegura que todas las operaciones de la sección crítica se completen
- `stlr wzr, [x0]`: 
  - **STLR** = "Store-Release Register"
  - Escribe 0 (registro `wzr` = zero register)
  - **Release semantics**: Garantiza que esta escritura sea visible después de las anteriores

---

### 4. mm_utils.S - Gestión de Memoria Virtual

**Ubicación**: `src/mm_utils.S`

Este archivo proporciona wrappers para acceder a registros de la **MMU** (Memory Management Unit).

#### Acceso a SCTLR_EL1 (Control del Sistema)

```assembly
get_sctlr_el1:
    mrs x0, SCTLR_EL1
    ret

set_sctlr_el1:
    msr SCTLR_EL1, x0
    ret
```

**SCTLR_EL1** controla características críticas:
- **Bit 0 (M)**: MMU Enable (paginación)
- **Bit 2 (C)**: Data Cache Enable
- **Bit 12 (I)**: Instruction Cache Enable

#### Acceso a TTBRx_EL1 (Tablas de Páginas)

```assembly
set_ttbr0_el1:
    msr TTBR0_EL1, x0
    ret
```

**TTBR** = Translation Table Base Register:
- **TTBR0_EL1**: Tabla de páginas para direcciones **bajas** (user space: 0x0...)
- **TTBR1_EL1**: Tabla de páginas para direcciones **altas** (kernel space: 0xFFFF...)

#### Invalidación del TLB

```assembly
tlb_invalidate_all:
    dsb ish         /* Data Sync Barrier - Inner Shareable */
    tlbi vmalle1is  /* TLB Invalidate All, EL1, Inner Shareable */
    dsb ish         /* Data Sync Barrier - Inner Shareable */
    isb             /* Instruction Sync Barrier */
    ret
```

**TLB** = Translation Lookaside Buffer (caché de traducciones virtuales→físicas)

**¿Cuándo es necesario?**
Después de modificar las tablas de páginas, debes invalidar el TLB para que la MMU vea los cambios.

**Secuencia de barreras**:
1. `dsb ish`: Espera a que todas las escrituras previas se completen (Inner Shareable)
2. `tlbi vmalle1is`: Invalida todas las entradas del TLB en todos los cores
3. `dsb ish`: Espera a que la invalidación se complete
4. `isb`: Sincroniza el pipeline de instrucciones

**Inner Shareable**: Afecta a todos los cores del mismo cluster de CPU.

---

### 5. utils.S - Utilidades de Bajo Nivel

**Ubicación**: `src/utils.S`

Este archivo proporciona funciones wrapper para operaciones comunes.

#### Acceso a Memoria MMIO

```assembly
put32:
    str w1, [x0]
    ret

get32:
    ldr w0, [x0]
    ret
```

**MMIO** = Memory-Mapped I/O:
- Los registros de hardware (GPIO, UART, Timer) se acceden como direcciones de memoria
- Ejemplo: `put32(0x3F200000, 0x1)` escribe en el GPIO

#### Control del Timer

```assembly
timer_get_freq:
    mrs x0, CNTFRQ_EL0
    ret

timer_set_tval:
    msr cntp_tval_el0, x0
    ret

timer_set_ctl:
    msr cntp_ctl_el0, x0
    ret
```

- **CNTFRQ_EL0**: Frecuencia del timer (típicamente 19.2 MHz en QEMU)
- **CNTP_TVAL_EL0**: Número de ticks hasta la próxima interrupción
- **CNTP_CTL_EL0**: Control del timer (enable/disable)

#### Configuración de la Tabla de Vectores

```assembly
set_vbar_el1:
    msr vbar_el1, x0
    ret
```

**VBAR_EL1** = Vector Base Address Register:
- Establece la dirección base de la tabla de vectores de excepciones
- Debe estar alineado a 2048 bytes (2 KB)

#### System Off (Semihosting)

```assembly
.align 3 
semihosting_param_block:
    .quad 0x20026   /* ADP_Stopped_Application */
    .quad 0         /* Exit Code (0 = Éxito) */

system_off:
    mov x0, #0x18
    ldr x1, =semihosting_param_block
    hlt #0xf000
    b hang
```

**Semihosting**: Interfaz de ARM para comunicarse con el debugger/emulador.

- `mov x0, #0x18`: Operación 0x18 = SYS_EXIT
- `ldr x1, =semihosting_param_block`: Parámetros de la operación
- `hlt #0xf000`: Instrucción especial que QEMU detecta como semihosting

---

### 6. vectors.S - Tabla de Vectores de Excepciones

**Ubicación**: `src/vectors.S`

Este archivo define la **Exception Vector Table** (tabla de vectores).

#### Estructura de la Tabla

```
┌─────────────────────────────────────────┐
│ Offset │ Excepción                       │
├─────────────────────────────────────────┤
│ +0x000 │ Sync (Current EL, SP0)          │
│ +0x080 │ IRQ                             │
│ +0x100 │ FIQ                             │
│ +0x180 │ SError                          │
├─────────────────────────────────────────┤
│ +0x200 │ Sync (Current EL, SPx) ←KERNEL  │
│ +0x280 │ IRQ                    ←TIMER   │
│ +0x300 │ FIQ                             │
│ +0x380 │ SError                          │
├─────────────────────────────────────────┤
│ +0x400 │ Sync (Lower EL, 64-bit) ←USER   │
│ +0x480 │ IRQ                             │
│ +0x500 │ FIQ                             │
│ +0x580 │ SError                          │
├─────────────────────────────────────────┤
│ +0x600 │ Sync (Lower EL, 32-bit)         │
│ +0x680 │ IRQ                             │
│ +0x700 │ FIQ                             │
│ +0x780 │ SError                          │
└─────────────────────────────────────────┘
```

#### Implementación

```assembly
.macro VENTRY label
    .align 7          /* Alinear a 128 bytes */
    b \label
.endm

.align 11             /* Alinear a 2048 bytes */
vectors:
    /* GRUPO 1: Current EL with SP0 */
    VENTRY hang
    VENTRY hang
    VENTRY hang
    VENTRY hang

    /* GRUPO 2: Current EL with SPx - KERNEL MODE */
    VENTRY el1_sync
    VENTRY irq_handler_stub  /* ← Timer interrupt salta aquí */
    VENTRY hang
    VENTRY hang

    /* GRUPO 3: Lower EL (AArch64) - USER MODE */
    VENTRY el0_sync
    VENTRY el0_irq
    VENTRY hang
    VENTRY hang

    /* GRUPO 4: Lower EL (AArch32) */
    VENTRY hang
    VENTRY hang
    VENTRY hang
    VENTRY hang
```

**Macro VENTRY**:
- `.align 7`: Alinea a 2^7 = 128 bytes
- `b \label`: Salta a la etiqueta especificada

**¿Por qué 128 bytes por entrada?**
Es un requisito de la arquitectura ARM64. Cada entrada debe estar en un offset múltiplo de 128.

**¿Cómo funciona?**
Cuando ocurre una excepción, el CPU:
1. Calcula: `dirección = VBAR_EL1 + offset`
2. Salta a esa dirección
3. Ejecuta el handler correspondiente

Ejemplo: Si ocurre un IRQ en modo kernel, salta a `VBAR_EL1 + 0x280`.

---

## Casos de Uso Prácticos

### Caso 1: Boot del Sistema

**Flujo completo desde el encendido**:

```
1. CPU enciende
   ↓
2. PC = _start (boot.S)
   ↓
3. Identificar core (MPIDR_EL1)
   ↓
4. Si core != 0 → hang (WFE)
   ↓
5. Si core == 0:
   - Configurar SP
   - Limpiar BSS
   - Saltar a kernel() en C
   ↓
6. kernel() inicializa drivers, scheduler, etc.
   ↓
7. enable_interrupts() (entry.S)
   ↓
8. Sistema operativo corriendo
```

### Caso 2: Interrupción del Timer

**Flujo cuando el timer genera IRQ**:

```
1. CPU ejecutando proceso A
   ↓
2. Timer expira → IRQ
   ↓
3. Hardware:
   - Guarda PC en ELR_EL1
   - Guarda PSTATE en SPSR_EL1
   - Salta a VBAR_EL1 + 0x280
   ↓
4. irq_handler_stub (entry.S):
   - kernel_entry (guarda x0-x30, ELR, SPSR)
   - bl handle_timer_irq (llama a C)
   ↓
5. handle_timer_irq() (en C):
   - Incrementa timer_tick
   - Llama a schedule()
   ↓
6. schedule() decide cambiar al proceso B
   ↓
7. schedule() llama a cpu_switch_to(A, B)
   ↓
8. cpu_switch_to() (entry.S):
   - Guarda contexto de A en su PCB
   - Restaura contexto de B desde su PCB
   - ret (salta a LR de B)
   ↓
9. Retorna a handle_timer_irq()
   ↓
10. kernel_exit (restaura x0-x30, ELR, SPSR)
   ↓
11. eret (retorna a proceso B)
   ↓
12. Proceso B continúa ejecutando
```

### Caso 3: Syscall desde Usuario

**Flujo cuando un proceso hace syscall**:

```
1. Proceso en EL0 ejecuta: svc #0
   ↓
2. Hardware:
   - Guarda PC en ELR_EL1
   - Guarda PSTATE en SPSR_EL1
   - Salta a VBAR_EL1 + 0x400
   ↓
3. el0_sync (entry.S):
   - kernel_entry
   - Lee ESR_EL1 para identificar excepción
   - Si es SVC → handle_svc
   ↓
4. handle_svc:
   - mov x0, sp (pt_regs)
   - mov x1, x8 (número de syscall)
   - bl syscall_handler
   ↓
5. syscall_handler() en C:
   - Switch según número de syscall
   - Ejecuta función correspondiente
   ↓
6. kernel_exit
   ↓
7. eret (retorna a EL0)
   ↓
8. Proceso continúa en user mode
```

### Caso 4: Crear un Nuevo Proceso

**Flujo en copy_process() y primera ejecución**:

```
1. copy_process(fn, arg) en C:
   - Aloca nuevo PCB
   - Copia contexto del padre
   - Configura:
     * x19 = fn (función a ejecutar)
     * x20 = arg (argumento)
     * x30 = ret_from_fork (dirección de retorno)
   ↓
2. schedule() eventualmente elige el nuevo proceso
   ↓
3. cpu_switch_to(prev, new):
   - Restaura contexto del nuevo proceso
   - ret → salta a x30 = ret_from_fork
   ↓
4. ret_from_fork (entry.S):
   - bl schedule_tail
   - mov x0, x20 (cargar argumento)
   - blr x19 (saltar a función)
   ↓
5. Función del proceso ejecuta
   ↓
6. Si retorna:
   - bl exit
   - Proceso termina ordenadamente
```

---

## Ejercicios y Práctica

### Ejercicio 1: Leer e Interpretar Código

**Ejercicio**: Analiza este fragmento y explica qué hace:

```assembly
foo:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    
    mov x0, #42
    bl some_function
    
    ldp x29, x30, [sp], #16
    ret
```

<details>
<summary>Solución</summary>

1. `stp x29, x30, [sp, #-16]!`:
   - Guarda FP (x29) y LR (x30) en el stack
   - Pre-decrementa sp en 16 bytes

2. `mov x29, sp`:
   - Establece el frame pointer al SP actual

3. `mov x0, #42`:
   - Primer argumento de some_function = 42

4. `bl some_function`:
   - Llama a some_function

5. `ldp x29, x30, [sp], #16`:
   - Restaura FP y LR
   - Post-incrementa sp en 16 bytes

6. `ret`:
   - Retorna (salta a x30)

**Esto es el prólogo y epílogo estándar de una función**.
</details>

### Ejercicio 2: Implementar una Función Simple

**Ejercicio**: Escribe una función en ensamblador que sume dos números:

```c
uint64_t add(uint64_t a, uint64_t b) {
    return a + b;
}
```

<details>
<summary>Solución</summary>

```assembly
.global add
add:
    add x0, x0, x1    ; x0 = x0 + x1
    ret               ; Retornar x0
```

**Explicación**:
- Parámetros: x0=a, x1=b (convención AAPCS64)
- Retorno: x0
- No necesita guardar registros porque no usa callee-saved
</details>

### Ejercicio 3: Bucle con Array

**Ejercicio**: Implementa una función que sume todos los elementos de un array:

```c
uint64_t sum_array(uint64_t *arr, size_t len) {
    uint64_t sum = 0;
    for (size_t i = 0; i < len; i++) {
        sum += arr[i];
    }
    return sum;
}
```

<details>
<summary>Solución</summary>

```assembly
.global sum_array
sum_array:
    mov x2, #0        ; sum = 0
    cbz x1, end       ; Si len == 0, retornar

loop:
    ldr x3, [x0], #8  ; x3 = *arr, luego arr++
    add x2, x2, x3    ; sum += x3
    sub x1, x1, #1    ; len--
    cbnz x1, loop     ; Si len != 0, repetir

end:
    mov x0, x2        ; Retornar sum
    ret
```

**Explicación**:
- x0 = puntero al array
- x1 = longitud
- x2 = acumulador de suma
- `ldr x3, [x0], #8`: Post-increment (lee y avanza 8 bytes)
</details>

### Ejercicio 4: Acceder a Estructuras

**Ejercicio**: Dada esta estructura en C:

```c
struct Point {
    int x;  // offset 0
    int y;  // offset 4
};

int get_x(struct Point *p) {
    return p->x;
}
```

Implementa `get_x` en ensamblador.

<details>
<summary>Solución</summary>

```assembly
.global get_x
get_x:
    ldr w0, [x0, #0]  ; Cargar 32 bits desde offset 0
    ret
```

O más simple:

```assembly
.global get_x
get_x:
    ldr w0, [x0]      ; Offset 0 puede omitirse
    ret
```

Para `get_y`:

```assembly
.global get_y
get_y:
    ldr w0, [x0, #4]  ; Cargar desde offset 4
    ret
```
</details>

### Ejercicio 5: Implementar un Mutex Simple

**Ejercicio**: Implementa `try_lock` que intenta tomar un lock sin esperar:

```c
// Retorna 1 si adquirió el lock, 0 si ya estaba tomado
int try_lock(int *lock);
```

<details>
<summary>Solución</summary>

```assembly
.global try_lock
try_lock:
    mov w1, #1
    ldxr w2, [x0]       ; Lee lock
    cbnz w2, failed     ; Si != 0, ya tomado
    stxr w3, w1, [x0]   ; Intenta escribir 1
    cbnz w3, failed     ; Si fallo, alguien se adelantó
    dmb sy
    mov x0, #1          ; Retornar 1 (éxito)
    ret

failed:
    clrex               ; Limpiar exclusividad
    mov x0, #0          ; Retornar 0 (fallo)
    ret
```

**Diferencia con spin_lock**:
- `try_lock` intenta **una sola vez**
- `spin_lock` intenta en **bucle infinito**
</details>

---

## Recursos Adicionales

### Documentación Oficial

1. **ARM Architecture Reference Manual** (ARM ARM):
   - Documento oficial completo de ARM64
   - Descarga: [developer.arm.com](https://developer.arm.com/documentation)

2. **Procedure Call Standard for the ARM 64-bit Architecture (AAPCS64)**:
   - Define convenciones de llamada
   - [Link directo](https://github.com/ARM-software/abi-aa/blob/main/aapcs64/aapcs64.rst)

3. **ARM Assembly by Example**:
   - [azeria-labs.com/writing-arm-assembly-part-1](https://azeria-labs.com/writing-arm-assembly-part-1/)

### Herramientas

- **QEMU**: Emulador de ARM64
- **GDB**: Debugger (con soporte ARM64)
- **objdump**: Desensamblar binarios (`aarch64-linux-gnu-objdump -d`)

### Práctica

**Experimenta modificando el código**:
1. Añade un nuevo spinlock global
2. Implementa una función en ensamblador que cuente caracteres
3. Modifica `boot.S` para imprimir un mensaje debug al arrancar

---

## Resumen de Conceptos Clave

| Concepto          | Descripción                                                                 |
|-------------------|-----------------------------------------------------------------------------|
| **Registros**     | x0-x30 (64-bit), w0-w30 (32-bit), SP, LR (x30), FP (x29)                    |
| **Load/Store**    | Solo `ldr`/`str` acceden a memoria, resto opera en registros               |
| **Callee-saved**  | x19-x29, x30, SP (la función debe preservarlos)                             |
| **MRS/MSR**       | Leer/escribir registros de sistema (MMU, timer, control)                    |
| **LDXR/STXR**     | Operaciones atómicas exclusivas (para locks)                                |
| **DMB/DSB/ISB**   | Barreras de memoria e instrucciones                                         |
| **ERET**          | Retorno de excepción (vuelve a EL0 o EL1)                                   |
| **VBAR_EL1**      | Dirección base de la tabla de vectores                                      |
| **Context Switch**| Guardar/restaurar registros para cambiar entre procesos                     |
| **TLB**           | Caché de traducciones virtuales→físicas (invalidar tras cambiar page tables)|

---

## Conclusión

Has aprendido ensamblador ARM64 analizando código real de un sistema operativo bare-metal. Los conceptos cubiertos son:

- **Boot y inicialización del sistema**
- **Cambio de contexto entre procesos**
- **Manejo de interrupciones y excepciones**
- **Sincronización atómica con spinlocks**
- **Gestión de memoria virtual (MMU)**
- **Acceso a hardware (timers, MMIO)**

**Próximos pasos**:
1. Experimenta modificando el código del proyecto
2. Lee el ARM Architecture Reference Manual para profundizar
3. Implementa nuevas funciones en ensamblador
4. Practica debugging con GDB y QEMU

¡Ahora tienes las herramientas para entender y escribir código ensamblador ARM64 de bajo nivel!
