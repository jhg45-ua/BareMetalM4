# BareMetalM4: Arquitectura del Kernel

## 📋 Tabla de Contenidos
1. [Visión General](#visión-general)
2. [Estructura del Código](#estructura-del-código)
3. [Componentes Principales](#componentes-principales)
4. [Flujo de Ejecución](#flujo-de-ejecución)
5. [Subsistema de Planificación (Scheduler)](#subsistema-de-planificación)
6. [Subsistema de Interrupciones](#subsistema-de-interrupciones)
7. [Sincronización entre Procesos](#sincronización-entre-procesos)
8. [Sistema de E/S](#sistema-de-es)
9. [Estructura de Memoria](#estructura-de-memoria)
10. [Decisiones de Diseño](#decisiones-de-diseño)
11. [Limitaciones y Mejoras Futuras](#limitaciones-y-mejoras-futuras)

---

## Visión General

**BareMetalM4** es un kernel operativo educativo para **ARM64** (AArch64) que demuestra conceptos fundamentales de sistemas operativos:

- ✅ **Multitarea cooperativa y expropiatoria**
- ✅ **Planificación con prioridades y envejecimiento (aging)**
- ✅ **Manejo de interrupciones y excepciones**
- ✅ **Sincronización: spinlocks y semáforos**
- ✅ **Gestor de memoria (MMU deshabilitado)**
- ✅ **I/O a través de UART QEMU**

### Plataforma Objetivo
- **Arquitectura**: ARM64 (ARMv8)
- **Emulador**: QEMU virt (`qemu-system-aarch64`)
- **Sin Sistema Operativo Base**: Bare-metal (sin Linux, sin librería C estándar)

---

## Estructura del Código

El kernel está organizado en **módulos especializados** siguiendo el principio de **separación de responsabilidades**. Esta refactorización (Enero 2026) dividió el código monolítico original en componentes bien definidos.

### Organización de Directorios

```
BareMetalM4/
├── include/
│   ├── io.h              # Interfaz UART yprintf
│   ├── sched.h           # Definiciones de PCB y estados
│   ├── semaphore.h       # Primitivas de sincronización
│   ├── timer.h           # Configuración GIC y timer
│   ├── types.h           # Tipos básicos del sistema
│   └── kernel/           # Headers de módulos del kernel
│       ├── kutils.h      #   Utilidades generales
│       ├── process.h     #   Gestión de procesos
│       ├── scheduler.h   #   Planificador
│       └── shell.h       #   Shell y procesos de prueba
│
├── src/
│   ├── boot.S            # Punto de entrada (ensamblador)
│   ├── entry.S           # Context switch y handlers IRQ
│   ├── vectors.S         # Tabla de excepciones (VBAR_EL1)
│   ├── locks.S           # Spinlocks (LDXR/STXR)
│   ├── utils.S           # Utilidades de sistema
│   ├── io.c              # Driver UART y kprintf
│   ├── timer.c           # Inicialización GIC y timer
│   ├── semaphore.c       # Implementación de semáforos
│   │
│   ├── kernel/           # Módulos del kernel
│   │   ├── kernel_main.c #   Punto de entrada e inicialización
│   │   ├── process.c     #   Gestión de PCB y threads
│   │   └── scheduler.c   #   Algoritmo de scheduling
│   │
│   ├── shell/            # Interfaz de usuario
│   │   └── shell.c       #   Shell interactivo y procesos demo
│   │
│   └── utils/            # Utilidades generales
│       └── kutils.c      #   panic, delay, strcmp, strncpy
│
└── docs/
    └── ARCHITECTURE.md   # Este documento
```

### Módulos del Kernel

#### 1. **kutils** (Utilidades del Kernel)
**Archivos**: `src/utils/kutils.c`, `include/kernel/kutils.h`

**Responsabilidad**: Funciones de utilidad general del kernel

| Función | Descripción |
|---------|-------------|
| `panic()` | Detiene el sistema con mensaje de error crítico |
| `delay()` | Retardo activo (busy-wait) para timing preciso |
| `k_strcmp()` | Comparación de cadenas (sin libc) |
| `k_strncpy()` | Copia de cadenas con límite de longitud |

**Uso**: Funciones base utilizadas por todos los módulos del sistema.

---

#### 2. **process** (Gestión de Procesos)
**Archivos**: `src/kernel/process.c`, `include/kernel/process.h`

**Responsabilidad**: Administración del ciclo de vida de procesos

| Componente | Descripción |
|------------|-------------|
| **Variables Globales** | `process[]`, `current_process`, `num_process`, `process_stack[]` |
| `create_thread()` | Crea nuevos threads del kernel con prioridad y nombre |
| `exit()` | Termina el proceso actual (estado → ZOMBIE) |
| `schedule_tail()` | Hook post-context-switch (futuras extensiones) |

**Estructura de Datos**:
```c
struct pcb {
    struct cpu_context context;  // x19-x30, sp, pc
    int state;                   // RUNNING, READY, BLOCKED, ZOMBIE
    int pid;                     // Process ID
    int priority;                // 0=máxima, 255=mínima
    unsigned long wake_up_time;  // Para sleep()
    char name[16];               // Nombre descriptivo
};
```

---

#### 3. **scheduler** (Planificador)
**Archivos**: `src/kernel/scheduler.c`, `include/kernel/scheduler.h`

**Responsabilidad**: Planificación de procesos con algoritmo de prioridades + aging

| Función | Descripción |
|---------|-------------|
| `schedule()` | Selecciona próximo proceso (aging + prioridades) |
| `timer_tick()` | Handler de interrupciones del timer |
| `sleep()` | Bloquea proceso actual por N ticks |
| `sys_timer_count` | Contador global de ticks del sistema |

**Algoritmo**: Ver sección [Subsistema de Planificación](#subsistema-de-planificación) más abajo.

---

#### 4. **shell** (Interfaz de Usuario)
**Archivos**: `src/shell/shell.c`, `include/kernel/shell.h`

**Responsabilidad**: Shell interactivo y procesos de prueba

| Función | Descripción |
|---------|-------------|
| `shell_task()` | Shell con comandos: help, ps, clear, panic, poweroff |
| `proceso_1()` | Proceso de prueba #1 (contador con sleep) |
| `proceso_2()` | Proceso de prueba #2 (contador con sleep) |
| `proceso_mortal()` | Proceso que termina automáticamente |

**Comandos Disponibles**:
- `help` - Muestra comandos disponibles
- `ps` - Lista procesos (PID, prioridad, estado, nombre)
- `clear` - Limpia la pantalla (ANSI codes)
- `panic` - Provoca un kernel panic (demo)
- `poweroff` - Apaga el sistema (QEMU)

---

#### 5. **kernel_main** (Inicialización)
**Archivo**: `src/kernel/kernel_main.c`

**Responsabilidad**: Punto de entrada e inicialización del sistema

```c
void kernel() {
    // 1. Inicializar kernel como Proceso 0
    current_process = &process[0];
    current_process->pid = 0;
    current_process->state = PROCESS_RUNNING;
    
    // 2. Crear shell y procesos de prueba
    create_thread(shell_task, 1, "Shell");
    create_thread(proceso_mortal, 5, "Proceso Mortal");
    
    // 3. Inicializar timer (GIC + interrupciones)
    timer_init();
    
    // 4. Loop principal (WFI)
    while(1) {
        asm volatile("wfi");  // Wait For Interrupt
    }
}
```

---

### Ventajas de la Arquitectura Modular

| Ventaja | Descripción |
|---------|-------------|
| **Modularidad** | Cada módulo tiene responsabilidad única y bien definida |
| **Mantenibilidad** | Más fácil encontrar y modificar código específico |
| **Reusabilidad** | Módulos pueden ser usados por otros componentes |
| **Escalabilidad** | Agregar funcionalidades es más sencillo |
| **Legibilidad** | Archivos pequeños, más fáciles de entender |
| **Testabilidad** | Módulos pueden probarse de forma aislada |

**Ejemplo**: Para modificar el algoritmo de scheduling, solo se edita `scheduler.c` sin tocar código de procesos, shell o utilidades.

---

## Componentes Principales

### 1. **Boot y Inicialización** (`boot.S`)
```
┌─────────────────────────────┐
│  Reset / Entrada HW         │
└──────────┬──────────────────┘
           │
           ▼
┌─────────────────────────────┐
│  boot.S:                    │
│  - Lee MPIDR_EL1 (CPU ID)   │
│  - Inicializa pila (SP)     │
│  - Salta a kernel() en C    │
└──────────┬──────────────────┘
           │
           ▼
┌─────────────────────────────┐
│  kernel() [kernel_main.c]   │
│  - Inicializa scheduler     │
│  - Crea procesos            │
│  - timer_init()             │
│  - WFI loop                 │
└─────────────────────────────┘
```

**Archivo**: `src/boot.S`

Responsabilidades:
- Detección de CPU (multicore)
- Inicialización de la pila desde `link.ld`
- Salto a punto de entrada en C (`kernel()`)

---

### 2. **Núcleo del Kernel** (Módulos)

El kernel está dividido en módulos especializados (ver [Estructura del Código](#estructura-del-código)):

**Módulos Principales**:

| Módulo | Archivo | Responsabilidad |
|--------|---------|----------------|
| **Inicialización** | `kernel_main.c` | Punto de entrada, setup del sistema |
| **Gestión de Procesos** | `process.c` | PCB, create_thread, exit |
| **Planificador** | `scheduler.c` | Algoritmo de aging, sleep, timer_tick |
| **Shell** | `shell.c` | Interfaz de comandos, procesos demo |
| **Utilidades** | `kutils.c` | panic, delay, strcmp, strncpy |

**Estructura de PCB**:
```c
struct pcb {
    struct cpu_context context;  // Registros guardados (x19-x30, sp, pc)
    int state;                   // RUNNING, READY, BLOCKED, ZOMBIE
    int pid;                     // Identificador único
    int priority;                // 0=máxima, 255=mínima
    int preempt_count;           // Contador de desalojamiento
    unsigned long wake_up_time;  // Tick para despertar (sleep)
    char name[16];               // Nombre descriptivo del proceso
};
```

**Variables Globales** (definidas en `process.c`):
```c
struct pcb process[MAX_PROCESS];              // Array de 64 PCBs
struct pcb *current_process;                  // Proceso en ejecución
int num_process;                              // Contador de procesos
uint8_t process_stack[MAX_PROCESS][4096];    // Stacks (256KB total)
```

---

### 3. **Conmutación de Contexto** (`entry.S`)

Cuando el scheduler elige un nuevo proceso:

```
CPU_SWITCH_TO (núcleo del cambio de contexto)
├─ Guardar registros del proceso actual (x19-x30)
│  └─ sp (stack pointer), pc (program counter, aka x30)
│
├─ Cargar registros del nuevo proceso (x19-x30)
│  └─ El "magic": x30 (LR) apunta al código del nuevo proceso
│
└─ RET: Salta al código del nuevo proceso
   └─ Equivale a "return" desde cpu_switch_to,
      pero en realidad ejecuta código completamente diferente
```

**Archivo**: `src/entry.S`

```asm
; Pseudocódigo simplificado:
cpu_switch_to:
    ; Guardar contexto actual
    stp x19, x20, [sp]  ; Callee-saved registers
    ...
    stp x29, x30, [sp]  ; Frame pointer, Link register
    
    ; Cargar contexto nuevo
    ldp x19, x20, [x0]
    ...
    ldp x29, x30, [x0]  ; ← x30 = dirección del código nuevo
    
    ret                 ; Salta a x30 (código del nuevo proceso)
```

**Punto clave**: El "magic" es que x30 se sobrescribe con la dirección del nuevo proceso, así cuando `ret` ejecuta, salta a ese código en lugar de retornar.

---

### 4. **Planificador (Scheduler)** 

Ubicación: `kernel.c::schedule()`

#### Algoritmo: Prioridad + Envejecimiento (Aging)

```
ENTRADA: Lista de procesos [RUNNING, READY, BLOCKED]
SALIDA: Nuevo proceso a ejecutar

FASE 1: ENVEJECIMIENTO
────────────────────
Para cada proceso READY:
    priority -= priority >> 2  // Baja prioridad (envejece)
    
Efecto: Procesos esperando obtienen prioridad gradualmente

FASE 2: SELECCIÓN
────────────────
Encontrar proceso READY con MENOR prioridad (número)
    0 = máxima (seleccionar primero)
    255 = mínima (seleccionar al final)

Cambiar su estado: RUNNING

FASE 3: PENALIZACIÓN
────────────────────
Aumentar prioridad del proceso anterior:
    priority >>= 2

Efecto: Proceso que acaba de ejecutar se envía a final de cola
```

#### ¿Por qué Aging?

Sin aging → **inanición (starvation)**:
```
Proceso A: prioridad 10 (alta, se ejecuta siempre)
Proceso B: prioridad 250 (baja, nunca se ejecuta)
↓
Con aging, prioridad de B → 249, 248, ... 10, 9, ...
Eventualmente B se ejecuta y A espera.
```

---

### 5. **Gestor de Interrupciones**

#### Tabla de Excepciones (`vectors.S`)

El **Vector Table** (tabla de excepciones) tiene 16 entradas:

```
VBAR_EL1 (Vector Base Address Register)
    ↓
Tabla 16×128 bytes (alineada a 2KB)
    ├─ Grupo 1: Excepciones de EL1 (actual level)
    │   ├─ Entrada 0: Synchronous (SVC, syscalls)
    │   ├─ Entrada 1: IRQ (Interrupciones)
    │   ├─ Entrada 2: FIQ (Fast Interrupts)
    │   └─ Entrada 3: SError (System Error)
    │
    ├─ Grupo 2: Excepciones de EL0 (aplicación)
    │   ├─ Entrada 4-7: Idem
    │
    └─ ...Grupos 3 y 4 para cambios de nivel
```

Cuando ocurre una **excepción**, la CPU:
1. Guarda contexto en registros especiales (ELR_EL1, SPSR_EL1)
2. Salta a dirección en tabla de excepciones
3. Handler ejecuta código (ej: `irq_handler_stub`)

---

#### Flujo de Interrupción de Timer

```
┌──────────────────────────────────────┐
│ Timer HW genera IRQ cada ~104ms      │
└────────────┬─────────────────────────┘
             │
             ▼
┌──────────────────────────────────────┐
│ CPU salta a vector[1]                │
│ (IRQ handler en entry.S)             │
└────────────┬─────────────────────────┘
             │
             ▼
┌──────────────────────────────────────┐
│ irq_handler_stub:                    │
│ 1. Guardar x0-x30 en stack           │
│ 2. Llamar handle_timer_irq()         │
│ 3. Restaurar registros               │
│ 4. ERET (retornar de excepción)      │
└────────────┬─────────────────────────┘
             │
             ▼
┌──────────────────────────────────────┐
│ handle_timer_irq() [timer.c]:        │
│ 1. Leer GICC_IAR (interrupt ACK)     │
│ 2. Recargar timer (CNTP_TVAL_EL0)    │
│ 3. Llamar schedule() para cambiar P  │
│ 4. ¡¡CRITICAL: Escribir GICC_EOIR!! │
│    (End of Interrupt) - sin esto     │
│    el timer se congela               │
└────────────┬─────────────────────────┘
             │
             ▼
┌──────────────────────────────────────┐
│ schedule() elige nuevo proceso       │
│ (posiblemente diferente)             │
└────────────┬─────────────────────────┘
             │
             ▼
┌──────────────────────────────────────┐
│ ERET: Salta a nuevo proceso          │
│ (o continúa el actual)               │
└──────────────────────────────────────┘
```

**Componente GIC** (Generic Interrupt Controller):

| Registro | Dirección | Propósito |
|----------|-----------|-----------|
| GICD_CTLR | 0x08000000 | Distribuidor: enable/disable |
| GICD_ISENABLER[0] | 0x08000100 | Enable interrupciones (bit 30 = timer) |
| GICC_CTLR | 0x08010000 | CPU interface: enable/disable |
| GICC_PMR | 0x08010004 | Priority mask (0xFF = allow all) |
| GICC_IAR | 0x0801000C | Interrupt ACK (leer para obtener ID) |
| GICC_EOIR | 0x08010010 | End of Interrupt (CRÍTICO) |

---

### 6. **Subsistema de Sincronización**

#### Spinlocks (Locks)

Ubicación: `src/locks.S`

Problema: Dos procesos modifican variable simultáneamente → **race condition**

```
SIN SPINLOCK (INCORRECTO):
┌─────────────────────┬──────────────────────┐
│ Proceso A           │ Proceso B            │
├─────────────────────┼──────────────────────┤
│ Lee count = 1       │                      │
│                     │ Lee count = 1        │
│ count = 1 - 1 = 0   │                      │
│                     │ count = 1 - 1 = 0    │
│ Escribe 0           │ Escribe 0            │
└─────────────────────┴──────────────────────┘
Resultado: count = 0 (pero ambos deberían decrementar = -2)
```

**Solución: LDXR/STXR (Load/Store eXclusive)**

```asm
spin_lock:
    ; Intentar adquirir spinlock
    ldxr    w0, [x0]        ; Load eXclusive: lee, marca como "exclusivo"
    cbnz    w0, retry       ; Si no es 0, alguien lo tiene
    mov     w0, 1           ; Valor a escribir: 1 (locked)
    stxr    w1, w0, [x1]    ; Store eXclusive: escribe SOLO si sigue "exclusivo"
    cbnz    w1, retry       ; Si STXR falló (alguien mas escribió), reintentar
    dmb     sy              ; Memory Barrier: sincronizar
    ret

spin_unlock:
    dmb     sy              ; Barrera ANTES de soltar
    stlr    wzr, [x0]       ; Store with Release (atomic write 0)
    ret
```

**Hardware**: Monitor de exclusividad en CPU
- LDXR marca dirección como "exclusiva"
- Si otro core escribe en esa dirección, el flag se limpia
- STXR falla (w1 = 1) si flag fue limpiado

#### Semáforos (Semaphore)

Ubicación: `src/semaphore.c`

**Uso**: Controlar acceso a recursos limitados

```c
struct semaphore {
    volatile int count;  // Contador de recursos disponibles
    int lock;            // Spinlock protegiendo count
};
```

**Operaciones clásicas (Dijkstra)**:

```
P() [sem_wait]:     V() [sem_signal]:
while (count <= 0)  count++
    schedule()      (despierta waiters)
count--
```

**Ejemplo: Mutex**
```c
struct semaphore mutex;
sem_init(&mutex, 1);  // 1 recurso = sección crítica

// En 2 procesos diferentes:
sem_wait(&mutex);      // Primer proceso: entra
// ... código crítico ...
sem_signal(&mutex);    // Libera

// Segundo proceso estaba esperando, ahora puede entrar
```

---

### 7. **Sistema de E/S**

#### UART (Universal Asynchronous Receiver-Transmitter)

Ubicación: `src/io.c`, `include/io.h`

```
┌────────────────────┐
│ kprintf()          │ (printf-like con %c, %s, %d, %x)
└────────┬───────────┘
         │
         ▼
┌────────────────────┐
│ uart_puts()        │ (cadena)
└────────┬───────────┘
         │
         ▼
┌────────────────────┐
│ uart_putc()        │ (carácter individual)
└────────┬───────────┘
         │
         ▼
┌────────────────────────────────┐
│ *UART0_DIR = (unsigned int)c   │
│ Escritura en 0x09000000        │
└────────────────────────────────┘
         │
         ▼
┌────────────────────────────────┐
│ Consola QEMU (stdout)          │
└────────────────────────────────┘
```

**Limitaciones**:
- Solo escritura (no hay lectura de entrada)
- Sin buffering de hardware (asumimos que siempre acepta)
- Sin interrupciones (polling es responsabilidad de usuario)

---

## Flujo de Ejecución

### Secuencia Boot → Kernel → Multitarea

```
1. RESET (ARM64 HW)
   └─ PC = 0x80000000 (configurado en QEMU)

2. boot.S ejecuta:
   ├─ Lee MPIDR_EL1: ¿Soy CPU 0 o secundaria?
   ├─ Si secundaria: WFE (Wait For Event, sleep)
   ├─ Si CPU 0:
   │   ├─ Limpia sección BSS
   │   ├─ Inicializa pila (SP)
   │   └─ Salta a kernel() en kernel.c
   └─ (No retorna)

3. kernel_main.c ejecuta:
   ├─ Inicializa Kernel como Proceso 0
   │   ├─ PID = 0, state = RUNNING
   │   ├─ priority = 20 (media-baja)
   │   └─ name = "Kernel"
   ├─ Crea procesos:
   │   ├─ shell_task (prioridad 1) - Shell interactivo
   │   └─ proceso_mortal (prioridad 5) - Demo
   │   └─ Cada uno con stack de 4 KB
   │   └─ cpu_context con x30 = dirección de función
   ├─ timer_init() configura:
   │   ├─ Tabla de excepciones (VBAR_EL1)
   │   ├─ GIC distribuidor (0x08000000)
   │   ├─ GIC CPU interface (0x08010000)
   │   ├─ Timer físico (CNTP_TVAL_EL0)
   │   └─ Interrupciones habilitadas
   ├─ WFI (Wait For Interrupt)
   │   └─ CPU duerme hasta que llega IRQ
   └─ (Loop infinito)

4. Timer genera IRQ cada ~104ms:
   ├─ CPU despierta del WFI
   ├─ Salta a irq_handler_stub (entry.S)
   ├─ Llama handle_timer_irq() (timer.c)
   │   ├─ Lee GICC_IAR (acknowledges IRQ)
   │   ├─ Recarga timer
   │   ├─ Llama schedule() ← CAMBIO DE CONTEXTO
   │   └─ Escribe GICC_EOIR (importante!)
   ├─ schedule() elige nuevo proceso
   ├─ entry.S cpu_switch_to cambia contexto
   └─ Ejecuta nuevo proceso (o el mismo)

5. Ciclo 4 se repite cada ~104ms
```

---

## Subsistema de Planificación

### Estados de Proceso

```
                    ┌─────────────┐
                    │   BLOCKED   │
                    │ (esperando) │
                    └──────┬──────┘
                           │ sem_signal()
                           ▼
        ┌──────────┐    ┌─────────────┐
        │ RUNNING  │───►│    READY    │
        │(ejecuta) │    │(en cola)    │
        └──────┬───┘    └──────┬──────┘
               │               │ schedule()
               └───────────────┘

schedule() cada timer interrupt (~104ms):
├─ Envejecer: priority -= priority >> 2
├─ Seleccionar: proceso con menor prioridad
├─ Cambiar estado: RUNNING
└─ Penalizar anterior: priority >>= 2
```

### Tabla de Prioridades

| Prioridad | Significado |
|-----------|-------------|
| 0-63 | Alta (timeshare normal) |
| 64-127 | Media |
| 128-191 | Baja |
| 192-255 | Mínima (solo si envejecen) |

El envejecimiento garantiza que **todos los procesos eventualmente ejecutan** (previene inanición).

---

### Sleep: Dormir un Proceso por Tiempo Determinado

**Ubicación**: `kernel.c::sleep()`

Mecanismo para que un proceso se bloquee **temporalmente** (a diferencia de semáforos que se bloquean indefinidamente).

#### Comparación: delay() vs sleep()

| Aspecto | delay() | sleep() |
|--------|---------|---------|
| **Tipo** | Busy-wait | Bloqueo con timer |
| **CPU** | Consume (bucle infinito) | Libera (para otros procesos) |
| **Otros procesos** | NO pueden ejecutar | PUEDEN ejecutar |
| **Precisión** | Exacta (ciclos de CPU) | Aproximada (~10ms) |
| **Uso** | Timing preciso | Delays normales |

#### Flujo de Ejecución

```
Proceso 1 ejecuta: sleep(50)
    │
    ├─ Calcula: wake_up_time = sys_timer_count + 50
    ├─ Cambia: state = BLOCKED
    ├─ Llama: schedule()
    │
    └─ (Duerme aqui - no ejecuta más código)
    
    ▼ Mientras Proceso 1 duerme:
    
    Proceso 2 ejecuta (elegido por schedule)
    
    Timer interrupt cada ~10ms incrementa sys_timer_count
    
    Cuando sys_timer_count == wake_up_time:
    ├─ handle_timer_irq() revisa todos BLOCKED
    ├─ Encuentra: wake_up_time <= sys_timer_count ✓
    ├─ Cambia: state = READY
    └─ [KERNEL] Despertando proceso 1...
    
    Siguiente schedule():
    └─ Puede elegir Proceso 1 nuevamente
```

#### Implementación Detallada

```c
// En PCB (sched.h):
unsigned long wake_up_time;  // Momento (tick) para despertar

// En kernel.c:
void sleep(unsigned int ticks) {
    // 1. Calcular cuando despertar
    current_process->wake_up_time = sys_timer_count + ticks;
    
    // 2. Bloquear (scheduler no lo elige)
    current_process->state = PROCESS_BLOCKED;
    
    // 3. Ceder CPU (otro proceso ejecuta)
    schedule();
    
    // ← Proceso duerme aqui hasta timer lo despierte
}

// En handle_timer_irq():
for (int i = 0; i < num_process; i++) {
    if (process[i].state == BLOCKED) {
        if (process[i].wake_up_time <= sys_timer_count) {
            process[i].state = READY;  // Despertar
        }
    }
}
```

#### Timing

- **Cada timer interrupt**: ~10 ms
- **sleep(100)**: ~1 segundo
- **sleep(10)**: ~100 milisegundos
- **Precisión**: ±10ms (depende de cuando se chequea)

#### Ejemplo de Uso

```c
void proceso_1() {
    enable_interrupts();  // Crítico: permitir timer
    
    int count = 0;
    while(1) {
        kprintf("[P1] Contador: %d\n", count++);
        
        // Dormir 50 ticks (~500ms)
        // Otros procesos pueden ejecutar mientras tanto
        sleep(50);
    }
}
```

#### Comparación con Otros Mecanismos

| Mecanismo | Bloqueo | Duración | Uso |
|-----------|---------|----------|-----|
| **delay()** | No (consume CPU) | Precisa | Timing exacto |
| **sleep()** | Sí (libera CPU) | Aproximada | Delays normales |
| **sem_wait()** | Sí | Indefinida | Recursos/mutex |
| **Condition var** | Sí | Indefinida/timeout | Eventos |

#### Limitaciones

- **No es exacto**: ±10ms de precisión
- **Overhead**: Chequeo en cada interrupt (~50 ciclos)
- **Proceso duerme más**: Espera a ser seleccionado nuevamente
- **Requiere interrupts**: Sin enable_interrupts(), nunca despierta

---

## Decisiones de Diseño

### ✅ Decisiones Acertadas

| Decisión | Razón |
|----------|-------|
| **Bare-metal** (sin Linux) | Aprende cómo funciona todo internamente |
| **ARM64** | Arquitectura moderna, común en móviles/servidores |
| **QEMU virt** | Emulador accesible, GIC realista |
| **LDXR/STXR spinlocks** | Corrección: imposible race condition |
| **Envejecimiento** | Previene inanición, educativo |
| **Timer interrupts** | Preemption: cambios no cooperativos |

### ⚠️ Limitaciones Intencionales

| Limitación | Razón | Mejora Real |
|-----------|-------|------------|
| **Busy-wait semáforos** | Simplicidad educativa | Wait queues + wakeup |
| **Single-core** | Evita sincronización compleja | Multicore con spinlocks |
| **Sin memoria virtual** | Omitir MMU | Paging + TLB |
| **Sin filesystem** | Scope limitado | VFS + inode cache |
| **Sin IPC avanzado** | Educativo | Message queues, pipes |
| **UART polling** | Implementación simple | Interrupts + buffers |

---

## Estructura de Memoria

```
┌────────────────────────────────────────┐
│ 0xFFFF_FFFF (64-bit)                   │
├────────────────────────────────────────┤
│ Kernel code/data                       │
│ (nuestro binario)                      │
├────────────────────────────────────────┤
│ Stack (crece hacia abajo)              │
│ _stack_top (definido en link.ld)       │
├────────────────────────────────────────┤
│ Procesos (stacks de 4KB cada uno)      │
│ ├─ Proceso 0: 0x500000                │
│ ├─ Proceso 1: 0x501000                │
│ └─ Proceso N: 0x50N000                │
├────────────────────────────────────────┤
│ MMIO (Memory-Mapped I/O)               │
│ ├─ UART0: 0x09000000                  │
│ ├─ GIC Distribuidor: 0x08000000       │
│ └─ GIC CPU Intf: 0x08010000           │
├────────────────────────────────────────┤
│ 0x00000000                             │
└────────────────────────────────────────┘
```

### Memory Map (QEMU virt)

| Rango | Propósito |
|-------|-----------|
| 0x80000000 - 0x80FFFFFF | Kernel (8 MB) |
| 0x09000000 | UART0 |
| 0x08000000 | GIC Distribuidor |
| 0x08010000 | GIC CPU Interface |

---

## Limitaciones y Mejoras Futuras

### Fase 1: Mejoras Educativas (Siguientes)
- [ ] Agregar syscalls (SVC exception)
- [ ] Implementar wait queues en semáforos
- [ ] Soporte para múltiples CPUs (spinlocks existentes)
- [ ] Keyboard input vía UART

### Fase 2: Características Reales
- [ ] Memory management (malloc/free)
- [ ] Virtual memory (paging)
- [ ] Filesystem (FAT o ext2)
- [ ] Networking (if applicable)

### Fase 3: Optimizaciones
- [ ] Timer events (en lugar de polling)
- [ ] IPC avanzado (message passing)
- [ ] Condition variables
- [ ] RCU (Read-Copy-Update)

---

## Diagrama de Componentes

```
┌──────────────────────────────────────────────────┐
│                   QEMU Virt (ARM64)              │
├──────────────────────────────────────────────────┤
│                                                  │
│  ┌───────────────┐         ┌──────────────┐    │
│  │  CPU (ARM64)  │         │  Memory      │    │
│  │  - EL1 mode   │         │  - 128 MB    │    │
│  │  - Timer      │         │  - MMIO      │    │
│  │  - GIC iface  │         │  - Code      │    │
│  └───────┬───────┘         └──────────────┘    │
│          │                                       │
│          └──────────┬───────────────┬──────┐    │
│                     │               │      │    │
│          ┌──────────▼──┐    ┌───────▼─┐   │    │
│          │   GIC Dist  │    │ Timer   │   │    │
│          │ (0x08000000)│    │ (ARM64) │   │    │
│          └─────────────┘    └────┬────┘   │    │
│                                  │        │    │
│          ┌───────────┬───────────┘        │    │
│          │           │                    │    │
│  ┌───────▼────┐  ┌──▼──────────┐   ┌────▼────┐
│  │ UART (TTY) │  │ GIC CPU If  │   │ Kernel  │
│  │(0x09000000)│  │(0x08010000) │   │ Code    │
│  └────────────┘  └─────────────┘   └─────────┘
│                                                  │
└──────────────────────────────────────────────────┘
```

---

## Referencias

### Registros ARM64 Importantes
- **MPIDR_EL1**: Identification (CPU ID)
- **VBAR_EL1**: Vector Base Address (tabla excepciones)
- **ELR_EL1**: Exception Link Register (dirección a retornar)
- **SPSR_EL1**: Saved Program Status Register (flags)
- **CNTP_TVAL_EL0**: Timer ticks restantes
- **CNTP_CTL_EL0**: Control del timer

### Publicaciones
- ARM v8 Architecture Manual (Official)
- QEMU virt board documentation
- Linux kernel (scheduler fuente)

---

**Última actualización**: Enero 6, 2026  
**Versión**: 0.3  
**Refactorización**: Estructura modular implementada (Enero 2026)

---

## Historial de Cambios

### v0.3 - Enero 2026
- ✅ Refactorización completa del kernel en módulos especializados
- ✅ Separación de responsabilidades: process, scheduler, shell, kutils
- ✅ Headers organizados en `include/kernel/`
- ✅ Shell interactivo con comandos (help, ps, clear, panic, poweroff)
- ✅ Nombres descriptivos para procesos (campo `name` en PCB)
- ✅ Mejora en mantenibilidad y escalabilidad del código

### v0.2 - 2025
- Implementación de sleep() con wake_up_time
- Sistema de semáforos con spinlocks
- Planificador con aging para prevenir starvation

### v0.1 - 2025
- Boot en ARM64 con soporte multicore
- Context switch básico
- Timer interrupts con GIC v2
- Driver UART simple
