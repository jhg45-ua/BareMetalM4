# BareMetalM4: Arquitectura del Kernel

## 📋 Tabla de Contenidos
1. [Visión General](#visión-general)
2. [Estructura del Código](#estructura-del-código)
3. [Componentes Principales](#componentes-principales)
4. [Flujo de Ejecución](#flujo-de-ejecución)
5. [Subsistema de Memoria Virtual (MMU)](#subsistema-de-memoria-virtual-mmu)
6. [Subsistema de Planificación (Scheduler)](#subsistema-de-planificación)
7. [Subsistema de Interrupciones](#subsistema-de-interrupciones)
8. [Sincronización entre Procesos](#sincronización-entre-procesos)
9. [Sistema de E/S](#sistema-de-es)
10. [Estructura de Memoria](#estructura-de-memoria)
11. [Decisiones de Diseño](#decisiones-de-diseño)
12. [Limitaciones y Mejoras Futuras](#limitaciones-y-mejoras-futuras)

---

## Visión General

**BareMetalM4** es un kernel operativo educativo para **ARM64** (AArch64) que demuestra conceptos fundamentales de sistemas operativos:

- ✅ **Multitarea cooperativa y expropiatoria**
- ✅ **Planificación con prioridades y envejecimiento (aging)**
- ✅ **Manejo de interrupciones y excepciones**
- ✅ **Sincronización: spinlocks y semáforos**
- ✅ **Gestión de memoria virtual (MMU)**
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
│   ├── sched.h           # Definiciones de PCB y estados
│   ├── semaphore.h       # Primitivas de sincronización
│   ├── types.h           # Tipos básicos del sistema
│   ├── drivers/          # Headers de controladores
│   │   ├── io.h          #   Interfaz UART y printf
│   │   └── timer.h       #   Configuración GIC y timer
│   ├── kernel/           # Headers de módulos del kernel
│   │   ├── kutils.h      #   Utilidades generales
│   │   ├── process.h     #   Gestión de procesos
│   │   ├── scheduler.h   #   Planificador
│   │   └── shell.h       #   Shell y procesos de prueba
│   └── mm/               # Headers de gestión de memoria
│       ├── malloc.h      #   Asignador dinámico (kmalloc/kfree)
│       └── mm.h          #   Interfaz MMU y memoria virtual
│
├── src/
│   ├── boot.S            # Punto de entrada (ensamblador)
│   ├── entry.S           # Context switch y handlers IRQ
│   ├── vectors.S         # Tabla de excepciones (VBAR_EL1)
│   ├── locks.S           # Spinlocks (LDXR/STXR)
│   ├── utils.S           # Utilidades de sistema
│   ├── mm_utils.S        # Funciones MMU en assembly
│   ├── semaphore.c       # Implementación de semáforos
│   │
│   ├── drivers/          # Controladores hardware
│   │   ├── io.c          #   Driver UART y kprintf
│   │   └── timer.c       #   Inicialización GIC y timer
│   │
│   ├── kernel/           # Módulos del kernel
│   │   ├── kernel.c      #   Punto de entrada e inicialización
│   │   ├── process.c     #   Gestión de PCB y threads
│   │   └── scheduler.c   #   Algoritmo de scheduling
│   │
│   ├── mm/               # Gestión de memoria
│   │   ├── mm.c          #   Implementación MMU
│   │   └── malloc.c      #   Asignador dinámico (kmalloc/kfree)
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
| `memset()` | Rellena bloque de memoria con un valor específico |
| `memcpy()` | Copia bloques de memoria de origen a destino |

**Uso**: Funciones base utilizadas por todos los módulos del sistema.

---

#### 2. **process** (Gestión de Procesos)
**Archivos**: `src/kernel/process.c`, `include/kernel/process.h`

**Responsabilidad**: Administración del ciclo de vida de procesos

| Componente | Descripción |
|------------|-------------|
| **Variables Globales** | `process[]`, `current_process`, `num_process` |
| `create_process()` | Crea nuevos threads del kernel con prioridad y nombre |
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

**Nota**: En la versión actual, `create_process()` utiliza `kmalloc()` para asignar dinámicamente las pilas de 4KB de cada proceso, en lugar de usar un array estático.

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

**Responsabilidad**: Shell interactivo del sistema

| Función | Descripción |
|---------|-------------|
| `shell_task()` | Shell con comandos: help, ps, clear, panic, poweroff |

**Comandos Disponibles**:
- `help` - Muestra comandos disponibles
- `ps` - Lista procesos (PID, prioridad, estado, nombre)
- `clear` - Limpia la pantalla (ANSI codes)
- `panic` - Provoca un kernel panic (demo)
- `poweroff` - Apaga el sistema (QEMU)

---

#### 5. **drivers** (Controladores Hardware)

##### 5.1 Driver UART (I/O)
**Archivos**: `src/drivers/io.c`, `include/drivers/io.h`

**Responsabilidad**: Comunicación serial y salida por consola

| Función | Descripción |
|---------|-------------|
| `uart_init()` | Inicializa UART (placeholder, ya está listo en QEMU) |
| `uart_putc()` | Escribe un carácter a la UART |
| `uart_puts()` | Escribe una cadena |
| `kprintf()` | Printf del kernel (formato: %s, %d, %x, %c) |

**Dirección MMIO**: `0x09000000` (UART0 en QEMU virt)

##### 5.2 Timer y GIC
**Archivos**: `src/drivers/timer.c`, `include/drivers/timer.h`

**Responsabilidad**: Configuración de interrupciones y timer del sistema

| Función | Descripción |
|---------|-------------|
| `timer_init()` | Inicializa GIC v2 y timer del sistema |
| `handle_timer_irq()` | Manejador de interrupciones del timer |
| `enable_interrupts()` | Habilita IRQs en DAIF |
| `disable_interrupts()` | Deshabilita IRQs en DAIF |

**Componentes GIC v2**:
- Distribuidor: `0x08000000` - Controla qué interrupciones están activas
- CPU Interface: `0x08010000` - Interfaz de interrupción por CPU

---

#### 6. **mm** (Gestión de Memoria)

##### 6.1 MMU (Memory Management Unit)
**Archivos**: `src/mm/mm.c`, `include/mm/mm.h`

**Responsabilidad**: Configuración de memoria virtual con MMU

| Función | Descripción |
|---------|-------------|
| `mem_init()` | Inicializa MMU, tablas de páginas, y activa caches |
| `map_page()` | Mapea una página virtual a física (L1) |

**Características**:
- Tablas de páginas L1 con bloques de 1 GB
- Identity mapping (virtual = física)
- Tipos de memoria: Device (periféricos) y Normal (RAM)
- Activación de I-Cache y D-Cache

##### 6.2 Asignador Dinámico (malloc)
**Archivos**: `src/mm/malloc.c`, `include/mm/malloc.h`

**Responsabilidad**: Asignación dinámica de memoria del kernel

| Función | Descripción |
|---------|-------------|
| `kmalloc(size)` | Asigna memoria dinámica (similar a malloc) |
| `kfree(ptr)` | Libera memoria previamente asignada |

**Implementación**: Asignador de memoria simple con lista enlazada de bloques libres y ocupados. Utiliza estrategias de first-fit para encontrar bloques disponibles.

**Características**:
- Header de bloque con tamaño y estado (libre/ocupado)
- Coalescing de bloques adyacentes libres
- Gestión de heap desde dirección base configurable
- Sin fragmentación externa gracias a coalescing

**Uso**:
```c
// Asignar memoria
char *buffer = (char *)kmalloc(256);

// Usar memoria
k_strncpy(buffer, "Hola mundo", 256);

// Liberar memoria
kfree(buffer);
```

---

#### 7. **tests** (Sistema de Pruebas)
**Archivos**: `src/utils/tests.c`, `include/tests.h`

**Responsabilidad**: Validación, diagnóstico y procesos de prueba del sistema

| Función | Descripción |
|---------|-------------|
| `test_memory()` | Valida kmalloc/kfree y estado de MMU |
| `proceso_1()` | Proceso de prueba #1 (contador con sleep de 70 ticks) |
| `proceso_2()` | Proceso de prueba #2 (contador con sleep de 10 ticks) |
| `proceso_mortal()` | Proceso que cuenta hasta 3 y termina automáticamente |
| `test_scheduler()` | Pruebas del planificador (futuro) |
| `test_processes()` | Validación de creación de procesos (futuro) |

**Características**:
- Tests ejecutados en boot para validar subsistemas
- Verificación de asignación y liberación de memoria
- Diagnóstico del estado de registros de sistema (SCTLR_EL1)
- Procesos de prueba para demostrar multitarea expropiativa
- Útil para debugging y desarrollo

**Detalles de Procesos de Prueba**:
- `proceso_1()` y `proceso_2()` demuestran el scheduler con diferentes sleep times
- `proceso_mortal()` demuestra la terminación correcta de procesos con `exit()`
- Todos llaman a `enable_interrupts()` para permitir el timer

---

#### 8. **kernel_main** (Inicialización)
**Archivo**: `src/kernel/kernel.c`

**Responsabilidad**: Punto de entrada e inicialización del sistema

```c
void kernel() {
    // 1. Inicializar sistema de memoria (MMU + Heap)
    init_memory_system();
    //    - Configura MMU y tablas de páginas
    //    - Inicializa heap de 64MB con kheap_init()
    
    // 2. Inicializar sistema de procesos
    init_process_system();
    //    - Configura proceso 0 (Kernel/IDLE)
    //    - Inicializa estructuras de PCB
    
    // 3. Inicializar timer (GIC + interrupciones)
    timer_init();
    
    // 4. Ejecutar tests del sistema (opcional)
    test_memory();
    //    - Valida kmalloc/kfree
    //    - Verifica estado de MMU
    
    // 5. Crear shell y procesos del sistema
    create_process(shell_task, 1, "Shell");
    
    // 6. Loop principal (IDLE)
    while(1) {
        asm volatile("wfi");  // Wait For Interrupt
    }
}
```

### Ventajas de la Arquitectura Modular

| Ventaja | Descripción |
|---------|-------------|
| **Separación de Responsabilidades** | Cada módulo tiene una función específica y bien definida |
| **Organización por Subsistemas** | drivers/, mm/, kernel/, shell/ reflejan componentes del SO |
| **Mantenibilidad** | Más fácil encontrar y modificar código específico |
| **Reusabilidad** | Módulos pueden ser usados por otros componentes |
| **Escalabilidad** | Agregar funcionalidades es más sencillo |
| **Legibilidad** | Archivos pequeños, más fáciles de entender |
| **Testabilidad** | Módulos pueden probarse de forma aislada |

**Ejemplo**: Para modificar el algoritmo de scheduling, solo se edita [scheduler.c](../src/kernel/scheduler.c) sin tocar código de procesos, shell, drivers o utilidades.

**Refactorización v0.3.5**: El código originalmente monolítico fue reorganizado en módulos especializados en enero de 2026, mejorando significativamente la estructura del proyecto.

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
│  kernel() [kernel.c]        │
│  - Inicializa MMU           │
│  - Prueba kmalloc/kfree     │
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
| **Inicialización** | `kernel/kernel.c` | Punto de entrada, setup del sistema |
| **Gestión de Procesos** | `kernel/process.c` | PCB, create_process, exit |
| **Planificador** | `kernel/scheduler.c` | Algoritmo de aging, sleep, timer_tick |
| **Shell** | `shell/shell.c` | Interfaz de comandos, procesos demo |
| **Utilidades** | `utils/kutils.c` | panic, delay, strcmp, strncpy |
| **Driver UART** | `drivers/io.c` | Comunicación serial, kprintf |
| **Timer/GIC** | `drivers/timer.c` | Interrupciones, timer del sistema |
| **MMU** | `mm/mm.c` | Memoria virtual, tablas de páginas |
| **Asignador** | `mm/malloc.c` | kmalloc, kfree, gestión de heap |

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
```

**Nota**: Las pilas de procesos ahora se asignan dinámicamente con `kmalloc()` en lugar de usar un array estático.

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

3. kernel.c ejecuta:
   ├─ init_memory_system() - Inicializa sistema de memoria:
   │   ├─ mem_init() configura MMU:
   │   │   ├─ Crea tabla de páginas L1
   │   │   ├─ Mapea periféricos (Device memory)
   │   │   ├─ Mapea RAM del kernel (Normal memory)
   │   │   ├─ Configura MAIR, TCR, TTBR0/1
   │   │   ├─ Activa MMU y caches (SCTLR_EL1)
   │   │   └─ Sistema ahora ejecuta en memoria virtual
   │   └─ kheap_init() inicializa heap dinámico:
   │       ├─ Heap base: símbolo _end del linker
   │       ├─ Tamaño: 64 MB
   │       ├─ Alineación a 16 bytes
   │       └─ Bloque libre inicial cubre todo el heap
   ├─ init_process_system() - Inicializa gestión de procesos:
   │   ├─ Configura Proceso 0 (Kernel/IDLE)
   │   │   ├─ PID = 0, state = RUNNING
   │   │   ├─ priority = 0
   │   │   └─ name = "Kernel"
   │   ├─ Inicializa tabla de PCBs
   │   └─ Apunta current_process al proceso 0
   ├─ timer_init() configura interrupciones:
   │   ├─ Tabla de excepciones (VBAR_EL1)
   │   ├─ GIC distribuidor (0x08000000)
   │   ├─ GIC CPU interface (0x08010000)
   │   ├─ Timer físico (CNTP_TVAL_EL0)
   │   └─ Interrupciones habilitadas
   ├─ test_memory() valida subsistemas:
   │   ├─ Prueba kmalloc(16) / kfree()
   │   ├─ Verifica SCTLR_EL1
   │   └─ Valida escritura en memoria dinámica
   ├─ create_process() crea procesos del sistema:
   │   ├─ shell_task (prioridad 1) - Shell interactivo
   │   └─ Cada proceso con stack de 4 KB (kmalloc)
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

## Subsistema de Memoria Virtual (MMU)

### Visión General

El kernel implementa un **sistema de memoria virtual** usando la MMU (Memory Management Unit) de ARM64. Esto proporciona:

- **Traducción de direcciones**: Virtual → Física
- **Tipos de memoria**: Device (periféricos) y Normal (RAM con caches)
- **Protección**: Separación lógica entre regiones de memoria
- **Caches**: Aceleración de accesos a RAM
- **Asignación dinámica**: Sistema de `kmalloc`/`kfree` para gestión del heap

### Arquitectura de la MMU ARM64

```
DIRECCION VIRTUAL (39 bits)
│
├─ Bits [38:30] → Índice L1 (512 entradas)
│                 Cada entrada = 1 GB
│
└─ Con T0SZ=25:
   - Espacio virtual: 2^39 = 512 GB
   - Tabla L1 directa (sin L2/L3)
   - Bloques de 1 GB (simplificado)

REGISTROS CLAVE:
├─ MAIR_EL1: Tipos de memoria (Device, Normal)
├─ TCR_EL1: Configuración (T0SZ, granularidad)
├─ TTBR0_EL1: Tabla de páginas (direcciones bajas)
├─ TTBR1_EL1: Tabla de páginas (direcciones altas)
└─ SCTLR_EL1: Control (MMU, I-Cache, D-Cache)
```

### Mapa de Memoria QEMU virt

| Rango Físico | Tamaño | Tipo | Contenido |
|--------------|--------|------|-----------|
| `0x00000000 - 0x3FFFFFFF` | 1 GB | Device | UART (0x09000000), GIC (0x08000000) |
| `0x40000000 - 0x7FFFFFFF` | 1 GB | Normal | Código kernel, stack, datos |

**Identity Mapping**: Dirección virtual = Dirección física (simplifica acceso inicial)

### Tabla de Páginas L1

```c
uint64_t page_table_l1[512] __attribute__((aligned(4096)));

// Entrada 0: Periféricos (Device memory)
page_table_l1[0] = 0x00000000 | MM_DEVICE;

// Entrada 1: RAM del kernel (Normal memory)
page_table_l1[1] = 0x40000000 | MM_NORMAL;
```

**Formato de descriptor de bloque**:
```
Bits [47:30] - Dirección física base (1 GB alineado)
Bits [11:2]  - Atributos:
  ├─ Bit 0: Válido (1)
  ├─ Bit 1: Tipo (1 = bloque)
  ├─ Bits [3:2]: Índice MAIR (tipo de memoria)
  ├─ Bits [9:8]: Shareability (Inner Shareable)
  └─ Bit 10: Access Flag (debe ser 1)
```

### Tipos de Memoria (MAIR_EL1)

| Índice | Tipo | Valor | Uso |
|--------|------|-------|-----|
| 0 | Device nGnRnE | 0x00 | Periféricos (sin cache, sin reordenamiento) |
| 1 | Normal sin cache | 0x44 | Memoria compartida CPU/DMA |
| 2 | Normal con cache | 0xFF | RAM del kernel (máximo rendimiento) |

**Device memory** garantiza:
- Sin fusionar accesos (cada read/write es individual)
- Sin reordenar operaciones (orden de programa)
- Sin confirmación temprana de escrituras

**Normal memory** permite:
- Caching (I-Cache + D-Cache)
- Reordenamiento de accesos por el hardware
- Write buffers y prefetching

### Proceso de Inicialización

```
mem_init() - Secuencia de activación:
│
├─ 1. Limpiar tabla L1 (512 entradas a 0)
│
├─ 2. Mapear memoria:
│   ├─ Entrada 0: Periféricos (Device)
│   └─ Entrada 1: RAM (Normal)
│
├─ 3. Configurar registros:
│   ├─ MAIR_EL1 ← Tipos de memoria
│   ├─ TCR_EL1 ← T0SZ=25 (39 bits), TG0=4KB
│   └─ TTBR0/1_EL1 ← &page_table_l1
│
├─ 4. Activar MMU:
│   ├─ SCTLR_EL1 |= (M | C | I)
│   │   ├─ M: MMU Enable
│   │   ├─ C: D-Cache Enable
│   │   └─ I: I-Cache Enable
│   │
│   └─ tlb_invalidate_all()
│       └─ Limpiar TLB (cache de traducciones)
│
└─ Sistema ahora ejecuta en memoria virtual
```

### Translation Lookaside Buffer (TLB)

El **TLB** es una cache que almacena traducciones recientes:

```
ACCESO A MEMORIA:
│
├─ CPU genera dirección virtual
│
├─ 1. Buscar en TLB (hardware)
│   ├─ HIT → Dirección física directa (rápido)
│   └─ MISS → Caminar tabla de páginas (lento)
│       └─ Guardar resultado en TLB
│
└─ Acceder memoria física
```

**Invalidación del TLB**:
```asm
tlb_invalidate_all:
    dsb ish         ; Sincronizar escrituras a memoria
    tlbi vmalle1is  ; Invalidar TLB (todos los cores)
    dsb ish         ; Asegurar invalidación completa
    isb             ; Sincronizar pipeline
    ret
```

Se invalida cuando:
- Se modifican tablas de páginas
- Se activa/desactiva la MMU
- Se cambia de contexto de memoria

### Funciones de Bajo Nivel (Assembly)

**Acceso a registros de sistema** (solo en Assembly):

```asm
// Leer registro
get_sctlr_el1:
    mrs x0, SCTLR_EL1  ; Move from System Register
    ret

// Escribir registro
set_sctlr_el1:
    msr SCTLR_EL1, x0  ; Move to System Register
    ret
```

**Archivos**:
- `src/mm/mm.c` - Lógica de inicialización MMU
- `src/mm/malloc.c` - Asignador dinámico (kmalloc/kfree)
- `src/mm_utils.S` - Acceso a registros (mrs/msr)
- `include/mm/mm.h` - Interfaz pública MMU
- `include/mm/malloc.h` - Interfaz pública asignador

### Ventajas del Sistema Actual

| Ventaja | Descripción |
|---------|-------------|
| **Simplicidad** | Identity mapping (virtual = física) |
| **Rendimiento** | Caches activos (I-Cache + D-Cache) |
| **Protección básica** | Separación Device/Normal memory |
| **Asignación dinámica** | kmalloc/kfree para gestión eficiente del heap |
| **Educativo** | Demuestra conceptos fundamentales de MMU |

### Asignador Dinámico de Memoria (kmalloc/kfree)

El kernel incluye un **asignador de memoria dinámico** que permite la asignación y liberación de memoria en tiempo de ejecución.

#### Estructura de Bloques

Cada bloque de memoria tiene un header que contiene metadatos:

```c
struct block_header {
    uint32_t size;      // Tamaño del bloque (sin incluir header)
    uint32_t is_free;   // 1 = libre, 0 = ocupado
    struct block_header *next;  // Siguiente bloque en la lista
};
```

#### Algoritmo de Asignación

**kmalloc(size)**:
1. Busca en la lista de bloques un bloque libre con tamaño suficiente (first-fit)
2. Si encuentra uno:
   - Marca el bloque como ocupado
   - Si el bloque es mucho más grande, lo divide (split)
3. Si no encuentra:
   - Expande el heap creando un nuevo bloque
4. Retorna puntero al área de datos (después del header)

**kfree(ptr)**:
1. Obtiene el header del bloque desde el puntero
2. Marca el bloque como libre
3. Intenta fusionar (coalesce) con bloques adyacentes libres
4. Reduce fragmentación externa

**kheap_init(start, end)**:
1. Alinea la dirección de inicio a 16 bytes
2. Crea el header inicial en esa dirección
3. Marca todo el espacio como un único bloque libre
4. Calcula tamaño: (end - start) - sizeof(header)

#### Características

| Característica | Descripción |
|----------------|-------------|
| **Estrategia** | First-fit (primer bloque libre que cabe) |
| **Coalescing** | Fusión de bloques adyacentes libres |
| **Split** | División de bloques grandes cuando es posible |
| **Lista enlazada** | Gestión simple de bloques libres y ocupados |
| **Alineación** | Bloques alineados a 16 bytes para ARM64 |
| **Tamaño del Heap** | 64 MB configurables (definido en init_memory_system) |

#### Ejemplo de Uso

```c
// Asignar memoria para un buffer
char *buffer = (char *)kmalloc(256);
if (buffer) {
    k_strncpy(buffer, "Hola mundo", 256);
    kprintf("Buffer: %s\n", buffer);
    kfree(buffer);  // Liberar cuando ya no se necesite
}

// Asignar memoria para un array
int *numeros = (int *)kmalloc(10 * sizeof(int));
if (numeros) {
    for (int i = 0; i < 10; i++) {
        numeros[i] = i * 2;
    }
    kfree(numeros);
}

// Asignar pila de proceso (usado internamente por create_process)
void *stack = kmalloc(4096);  // 4KB de pila
```

#### Mapa de Memoria con Heap

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
│ HEAP (asignación dinámica)             │
│ ├─ Bloques kmalloc                     │
│ ├─ Pilas de procesos                   │
│ └─ Crece hacia arriba                  │
├────────────────────────────────────────┤
│ MMIO (Memory-Mapped I/O)               │
│ ├─ UART0: 0x09000000                  │
│ ├─ GIC Distribuidor: 0x08000000       │
│ └─ GIC CPU Intf: 0x08010000           │
├────────────────────────────────────────┤
│ 0x00000000                             │
└────────────────────────────────────────┘
```

#### Ventajas del Sistema

- **Flexibilidad**: Asignación dinámica según necesidades
- **Eficiencia**: Reutilización de bloques liberados
- **Anti-fragmentación**: Coalescing reduce fragmentación externa
- **Simple**: Implementación educativa, fácil de entender
- **Determinista**: Sin llamadas al sistema, control total
- **Heap de 64MB**: Espacio amplio para procesos y estructuras

#### Limitaciones

- **First-fit**: No es la estrategia más eficiente (best-fit sería mejor)
- **Sin compactación**: Fragmentación interna puede persistir
- **Sin protección**: Todos los procesos comparten el mismo heap
- **Sin estadísticas**: No hay tracking de memoria usada/libre
- **Sin coalescing completo**: Implementación básica (mejora pendiente)

### Limitaciones del Subsistema de Memoria

- No hay protección entre procesos (todos comparten espacio)
- No hay paginación dinámica (todo mapeado al inicio)
- No hay swapping (sin disco)
- Tabla L1 única (sin separación user/kernel)
- El asignador usa first-fit (no es óptimo)

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

### ✅ Características Implementadas (v0.3.5)
- [x] Arquitectura modular con separación de subsistemas
- [x] Asignación dinámica de memoria (kmalloc/kfree)
- [x] Planificador expropiativo con aging
- [x] Shell interactivo con múltiples comandos
- [x] MMU con memoria virtual
- [x] Interrupciones de timer con GIC v2
- [x] Sincronización con spinlocks y semáforos

### Fase 1: Mejoras Educativas (Siguientes)
- [ ] Agregar syscalls (SVC exception)
- [ ] Implementar wait queues en semáforos
- [ ] Soporte para múltiples CPUs (spinlocks existentes)
- [ ] Keyboard input vía UART (lectura)
- [ ] Mejorar asignador (best-fit, estadísticas)

### Fase 2: Características Reales
- [ ] Memory management avanzado (protección por proceso)
- [ ] Virtual memory con paginación dinámica
- [ ] Filesystem (FAT o ext2)
- [ ] Procesos de usuario (EL0)
- [ ] System calls completos

### Fase 3: Optimizaciones
- [ ] Timer events (en lugar de polling)
- [ ] IPC avanzado (message passing, pipes)
- [ ] Condition variables
- [ ] RCU (Read-Copy-Update)
- [ ] Multicore scheduling

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

**Última actualización**: Enero 16, 2026  
**Versión**: 0.3.5  
**Refactorización**: Estructura modular, sistema de memoria dinámica e inicialización estructurada implementados (Enero 2026)

---

## Historial de Cambios

### v0.3.5 - Enero 20, 2026
- ✅ **Actualización de versión y mantenimiento**
  - Actualización del número de versión en todo el proyecto
  - Mejoras en la documentación del sistema
  - Refinamiento de comentarios en código fuente
  - Preparación para futuras características

### v0.3 - Enero 2026
- ✅ **Refactorización completa del kernel en módulos especializados**
  - Separación de responsabilidades: process, scheduler, shell, kutils
  - Organización por subsistemas: drivers/, mm/, kernel/, utils/
  - Headers organizados en `include/kernel/`, `include/drivers/`, `include/mm/`
- ✅ **Sistema de asignación dinámica de memoria**
  - Implementación de `kmalloc()` y `kfree()`
  - Asignador con lista enlazada de bloques (first-fit)
  - Coalescing de bloques libres adyacentes
  - Heap de 64 MB configurado dinámicamente
  - Pilas de procesos asignadas con kmalloc()
- ✅ **Inicialización estructurada del sistema**
  - `init_memory_system()` - Configura MMU + Heap
  - `init_process_system()` - Inicializa gestión de procesos
  - Proceso 0 (Kernel/IDLE) configurado automáticamente
- ✅ **Sistema de tests integrado**
  - `test_memory()` - Valida kmalloc/kfree y MMU
  - Tests ejecutados en boot para diagnóstico
  - Framework extensible para futuras pruebas
- ✅ **Shell interactivo mejorado**
  - Comandos: help, ps, clear, panic, poweroff
  - Nombres descriptivos para procesos (campo `name` en PCB)
- ✅ **Mejoras en mantenibilidad y escalabilidad**
  - Mejor organización de directorios
  - Documentación actualizada y completa
  - Código más modular y reutilizable

### v0.2 - 2025
- Implementación de sleep() con wake_up_time
- Sistema de semáforos con spinlocks
- Planificador con aging para prevenir starvation
- MMU con tablas de páginas y memoria virtual

### v0.1 - 2025
- Boot en ARM64 con soporte multicore
- Context switch básico
- Timer interrupts con GIC v2
- Driver UART simple
