# BareMetalM4: Arquitectura del Kernel

## 📋 Tabla de Contenidos
1. [Visión General](#visión-general)
2. [Estructura del Código](#estructura-del-código)
3. [Componentes Principales](#componentes-principales)
4. [Flujo de Ejecución](#flujo-de-ejecución)
5. [Subsistema de Memoria Virtual (MMU)](#subsistema-de-memoria-virtual-mmu)
6. [Subsistema de Planificación](#subsistema-de-planificación)
7. [Decisiones de Diseño](#decisiones-de-diseño)
8. [Estructura de Memoria](#estructura-de-memoria)
9. [Limitaciones y Mejoras Futuras](#limitaciones-y-mejoras-futuras)

---

## Visión General
[↑ Volver a Tabla de Contenidos](#-tabla-de-contenidos)

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

[↑ Volver a Tabla de Contenidos](#-tabla-de-contenidos)

---

## Estructura del Código
[↑ Volver a Tabla de Contenidos](#-tabla-de-contenidos)

El kernel está organizado en **módulos especializados** siguiendo el principio de **separación de responsabilidades**. 
Esta refactorización (Enero 2026) dividió el código monolítico original en componentes bien definidos.

### Organización de Directorios

```
BareMetalM4/
├── include/                    # Headers del sistema
│   ├── sched.h                 # Definiciones de PCB y estados de proceso
│   ├── semaphore.h             # Primitivas de sincronización
│   ├── types.h                 # Tipos básicos del sistema (uint64_t, etc.)
│   ├── tests.h                 # Interfaz de funciones de prueba
│   │
│   ├── drivers/                # Headers de controladores hardware
│   │   ├── io.h                #   Interfaz UART y kprintf
│   │   └── timer.h             #   Configuración GIC y timer del sistema
│   │
│   ├── kernel/                 # Headers de módulos del kernel
│   │   ├── kutils.h            #   Utilidades generales (panic, delay, strcmp)
│   │   ├── process.h           #   Gestión de procesos y threads
│   │   ├── scheduler.h         #   Planificador con aging
│   │   ├── shell.h             #   Shell interactivo y comandos
│   │   └── sys.h               #   Syscalls y manejo de excepciones
│   │
│   └── mm/                     # Headers de gestión de memoria
│       ├── malloc.h            #   Asignador dinámico (kmalloc/kfree)
│       └── mm.h                #   Interfaz MMU y memoria virtual
│
├── src/                        # Código fuente
│   ├── boot.S                  # Punto de entrada ARM64 (assembly)
│   ├── entry.S                 # Context switch, IRQ handlers, syscalls
│   ├── vectors.S               # Tabla de vectores de excepciones (VBAR_EL1)
│   ├── locks.S                 # Spinlocks con LDXR/STXR (atomic ops)
│   ├── utils.S                 # Utilidades de sistema (registros, timer)
│   ├── mm_utils.S              # Funciones MMU en assembly (get/set registros)
│   ├── semaphore.c             # Implementación de semáforos
│   │
│   ├── drivers/                # Controladores hardware
│   │   ├── io.c                #   Driver UART y kprintf formateado
│   │   └── timer.c             #   Inicialización GIC v2 y timer físico
│   │
│   ├── kernel/                 # Módulos principales del kernel
│   │   ├── kernel.c            #   Punto de entrada C e inicialización
│   │   ├── process.c           #   Gestión de PCB, create_process, exit
│   │   ├── scheduler.c         #   Algoritmo de scheduling con aging
│   │   └── sys.c               #   Syscalls (write, exit) y handle_fault
│   │
│   ├── mm/                     # Gestión de memoria
│   │   ├── mm.c                #   Implementación MMU, tablas de páginas
│   │   └── malloc.c            #   Asignador dinámico (kmalloc/kfree)
│   │
│   ├── shell/                  # Interfaz de usuario
│   │   └── shell.c             #   Shell con 8 comandos (help, ps, test, clear, etc.)
│   │
│   └── utils/                  # Utilidades generales
│       ├── kutils.c            #   panic, delay, strcmp, strncpy, memcpy
│       └── tests.c             #   Procesos de prueba (user_task, kamikaze_test)
│
├── docs/                       # Documentación del proyecto
│   └── ARCHITECTURE.md         # Este documento (arquitectura completa)
│
└── [build artifacts]           # Generados durante compilación
    ├── build/                  # Objetos intermedios (*.o)
    ├── kernel8.elf             # Ejecutable ELF
    └── kernel8.img             # Imagen binaria para QEMU
```

### Descripción de Archivos Clave

#### Assembly (.S)
| Archivo      | Líneas | Descripción                                             |
|--------------|--------|---------------------------------------------------------|
| `boot.S`     | ~100   | Bootstrap inicial, detección de CPU, setup de stack     |
| `entry.S`    | ~220   | Context switch, handlers de IRQ/sync, move_to_user_mode |
| `vectors.S`  | ~100   | Tabla de 16 vectores de excepciones ARM64               |
| `locks.S`    | ~50    | Spinlocks con instrucciones exclusivas LDXR/STXR        |
| `utils.S`    | ~80    | Acceso a registros de sistema (MSR/MRS)                 |
| `mm_utils.S` | ~150   | Configuración MMU, registros SCTLR/TCR/TTBR             |

#### Kernel Core (.c)
| Archivo       | Líneas | Descripción                                           |
|---------------|--------|-------------------------------------------------------|
| `kernel.c`    | ~200   | Inicialización del sistema, loop principal WFI        |
| `process.c`   | ~230   | PCB management, create_process, create_user_process   |
| `scheduler.c` | ~150   | Algoritmo aging, schedule(), sleep(), timer_tick      |
| `sys.c`       | ~100   | Syscall dispatcher, sys_write, sys_exit, handle_fault |

#### Drivers (.c)
| Archivo   | Líneas | Descripción                                          |
|-----------|--------|------------------------------------------------------|
| `io.c`    | ~150   | UART driver, kprintf con formato %s/%d/%x/%c         |
| `timer.c` | ~200   | Configuración GIC v2, timer físico, handle_timer_irq |

#### Memory Management (.c)
| Archivo    | Líneas | Descripción                                    |
|------------|--------|------------------------------------------------|
| `mm.c`     | ~200   | Configuración MMU, tablas L1, identity mapping |
| `malloc.c` | ~250   | kmalloc/kfree, lista enlazada, coalescing      |

#### User Interface & Tests (.c)
| Archivo       | Líneas | Descripción                                              |
|---------------|--------|----------------------------------------------------------|
| `shell.c`     | ~200   | Shell con 8 comandos, parser de entrada, eco y backspace |
| `tests.c`     | ~190   | user_task (EL0), kamikaze_test, test_memory              |
| `kutils.c`    | ~100   | panic, delay, strcmp, strncpy, memset, memcpy            |
| `semaphore.c` | ~80    | sem_init, sem_wait, sem_signal                           |

**Total de líneas de código**: ~2,500 líneas (sin contar comentarios y espacios)

### Módulos del Kernel

#### 1. **kutils** (Utilidades del Kernel)
**Archivos**: `src/utils/kutils.c`, `include/kernel/kutils.h`

**Responsabilidad**: Funciones de utilidad general del kernel

| Función       | Descripción                                       |
|---------------|---------------------------------------------------|
| `panic()`     | Detiene el sistema con mensaje de error crítico   |
| `delay()`     | Retardo activo (busy-wait) para timing preciso    |
| `k_strcmp()`  | Comparación de cadenas (sin libc)                 |
| `k_strncpy()` | Copia de cadenas con límite de longitud           |
| `memset()`    | Rellena bloque de memoria con un valor específico |
| `memcpy()`    | Copia bloques de memoria de origen a destino      |

**Uso**: Funciones base utilizadas por todos los módulos del sistema.

---

#### 2. **process** (Gestión de Procesos)
**Archivos**: `src/kernel/process.c`, `include/kernel/process.h`

**Responsabilidad**: Administración del ciclo de vida de procesos

| Componente             | Descripción                                           |
|------------------------|-------------------------------------------------------|
| **Variables Globales** | `process[]`, `current_process`, `num_process`         |
| `create_process()`     | Crea nuevos threads del kernel con prioridad y nombre |
| `exit()`               | Termina el proceso actual (estado → ZOMBIE)           |
| `schedule_tail()`      | Hook post-context-switch (futuras extensiones)        |

**Estructura de Datos**:
```c
struct pcb {
    struct cpu_context context;  // x19-x30, fp, pc, sp
    long state;                  // UNUSED, RUNNING, READY, BLOCKED, ZOMBIE
    long pid;                    // Process ID (0-63)
    int priority;                // 0=máxima, 255=mínima
    long prempt_count;           // Contador de desalojamiento
    unsigned long wake_up_time;  // Para sleep()
    char name[16];               // Nombre descriptivo
    unsigned long stack_addr;    // Dirección base de la pila
    unsigned long cpu_time;      // Tiempo de CPU usado
    int block_reason;            // NONE, SLEEP, WAIT
    int exit_code;               // Código de retorno
};
```

**Nota**: En la versión actual, `create_process()` utiliza `kmalloc()` para asignar dinámicamente las pilas de 4KB de cada proceso, en lugar de usar un array estático.

---

#### 3. **scheduler** (Planificador)
**Archivos**: `src/kernel/scheduler.c`, `include/kernel/scheduler.h`

**Responsabilidad**: Planificación de procesos con algoritmo de prioridades + aging

| Función           | Descripción                                      |
|-------------------|--------------------------------------------------|
| `schedule()`      | Selecciona próximo proceso (aging + prioridades) |
| `timer_tick()`    | Handler de interrupciones del timer              |
| `sleep()`         | Bloquea proceso actual por N ticks               |
| `sys_timer_count` | Contador global de ticks del sistema             |

**Algoritmo**: Ver sección [Subsistema de Planificación](#subsistema-de-planificación) más abajo.

---

#### 4. **shell** (Interfaz de Usuario)
**Archivos**: `src/shell/shell.c`, `include/kernel/shell.h`

**Responsabilidad**: Shell interactivo del sistema

| Función        | Descripción                                                              |
|----------------|--------------------------------------------------------------------------|
| `shell_task()` | Shell interactivo con línea de comandos y múltiples comandos del sistema |

**Comandos Disponibles**:

| Comando          | Descripción       | Funcionalidad                                                                                       |
|------------------|-------------------|-----------------------------------------------------------------------------------------------------|
| `help`           | Muestra ayuda     | Lista todos los comandos disponibles con descripción                                                |
| `ps`             | Process Status    | Lista procesos activos con PID, prioridad, estado (RUN/RDY/SLEEP/WAIT/ZOMB), tiempo de CPU y nombre |
| `test`           | Batería de tests  | Ejecuta test_memory(), test_processes(), test_scheduler()                                           |
| `test_user_mode` | Test modo usuario | Crea proceso en EL0 que ejecuta syscalls (user_task)                                                |
| `test_crash`     | Test protección   | Crea proceso kamikaze que intenta escribir en NULL para demostrar protección de memoria             |
| `clear`          | Limpiar pantalla  | Limpia terminal usando códigos ANSI (ESC[2J ESC[H)                                                  |
| `panic`          | Kernel Panic      | Provoca un kernel panic intencionalmente (demo)                                                     |
| `poweroff`       | Apagar sistema    | Apaga QEMU usando system_off() (PSCI)                                                               |

**Características del Shell**:
- ✅ Entrada interactiva con eco local
- ✅ Soporte de backspace (127 / '\b')
- ✅ Buffer de comando de 64 caracteres
- ✅ Sleep eficiente cuando no hay entrada (no consume CPU)
- ✅ Prompt visual (`> `)
- ✅ Mensajes de error para comandos desconocidos

**Ejemplos de Uso**:
```
> help
Comandos disponibles:
  help               - Muestra esta ayuda
  ps                 - Lista los procesos (simulado)
  test               - Ejecutando test de memoria, procesos y scheduler
  test_user_mode     - Ejecuta test del modo usuario
  test_crash         - Ejecuta test de proteccion de memoria basica
  clear              - Limpia la pantalla
  panic              - Provoca un Kernel Panic
  poweroff           - Apaga el sistema

> ps
PID | Prio | State | Time | Name
----|------|-------|------|------
 0  |  0   | RUN   | 1523 | Kernel
 1  |  1   | RDY   | 342  | Shell
 2  |  5   | SLEEP | 89   | proceso_1
 3  |  8   | RDY   | 156  | proceso_2

> test_user_mode
[KERNEL] Creando proceso de usuario...
[USER] Hola desde EL0!
[SYSCALL] Proceso solicitó salida con código 0

> clear
(pantalla limpiada)
BareMetalM4 Shell
> 
```

---

#### 5. **drivers** (Controladores Hardware)

##### 5.1 Driver UART (I/O)
**Archivos**: `src/drivers/io.c`, `include/drivers/io.h`

**Responsabilidad**: Comunicación serial y salida por consola

| Función       | Descripción                                          |
|---------------|------------------------------------------------------|
| `uart_init()` | Inicializa UART (placeholder, ya está listo en QEMU) |
| `uart_putc()` | Escribe un carácter a la UART                        |
| `uart_puts()` | Escribe una cadena                                   |
| `kprintf()`   | Printf del kernel (formato: %s, %d, %x, %c)          |

**Dirección MMIO**: `0x09000000` (UART0 en QEMU virt)

##### 5.2 Timer y GIC
**Archivos**: `src/drivers/timer.c`, `include/drivers/timer.h`

**Responsabilidad**: Configuración de interrupciones y timer del sistema

| Función                | Descripción                           |
|------------------------|---------------------------------------|
| `timer_init()`         | Inicializa GIC v2 y timer del sistema |
| `handle_timer_irq()`   | Manejador de interrupciones del timer |
| `enable_interrupts()`  | Habilita IRQs en DAIF                 |
| `disable_interrupts()` | Deshabilita IRQs en DAIF              |

**Componentes GIC v2**:
- Distribuidor: `0x08000000` - Controla qué interrupciones están activas
- CPU Interface: `0x08010000` - Interfaz de interrupción por CPU

---

#### 6. **mm** (Gestión de Memoria)

##### 6.1 MMU (Memory Management Unit)
**Archivos**: `src/mm/mm.c`, `include/mm/mm.h`

**Responsabilidad**: Configuración de memoria virtual con MMU

| Función      | Descripción                                        |
|--------------|----------------------------------------------------|
| `mem_init()` | Inicializa MMU, tablas de páginas, y activa caches |
| `map_page()` | Mapea una página virtual a física (L1)             |

**Características**:
- Tablas de páginas L1 con bloques de 1 GB
- Identity mapping (virtual = física)
- Tipos de memoria: Device (periféricos) y Normal (RAM)
- Activación de I-Cache y D-Cache

##### 6.2 Asignador Dinámico (malloc)
**Archivos**: `src/mm/malloc.c`, `include/mm/malloc.h`

**Responsabilidad**: Asignación dinámica de memoria del kernel

| Función         | Descripción                                |
|-----------------|--------------------------------------------|
| `kmalloc(size)` | Asigna memoria dinámica (similar a malloc) |
| `kfree(ptr)`    | Libera memoria previamente asignada        |

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

| Función            | Descripción                                           |
|--------------------|-------------------------------------------------------|
| `test_memory()`    | Valida kmalloc/kfree y estado de MMU                  |
| `proceso_1()`      | Proceso de prueba #1 (contador con sleep de 70 ticks) |
| `proceso_2()`      | Proceso de prueba #2 (contador con sleep de 10 ticks) |
| `proceso_mortal()` | Proceso que cuenta hasta 3 y termina automáticamente  |
| `test_scheduler()` | Pruebas del planificador (futuro)                     |
| `test_processes()` | Validación de creación de procesos (futuro)           |

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

#### 8. **kernel** (Inicialización)
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

| Ventaja                             | Descripción                                                |
|-------------------------------------|------------------------------------------------------------|
| **Separación de Responsabilidades** | Cada módulo tiene una función específica y bien definida   |
| **Organización por Subsistemas**    | drivers/, mm/, kernel/, shell/ reflejan componentes del SO |
| **Mantenibilidad**                  | Más fácil encontrar y modificar código específico          |
| **Reusabilidad**                    | Módulos pueden ser usados por otros componentes            |
| **Escalabilidad**                   | Agregar funcionalidades es más sencillo                    |
| **Legibilidad**                     | Archivos pequeños, más fáciles de entender                 |
| **Testabilidad**                    | Módulos pueden probarse de forma aislada                   |

**Ejemplo**: Para modificar el algoritmo de scheduling, solo se edita [scheduler.c](../src/kernel/scheduler.c) sin tocar código de procesos, shell, drivers o utilidades.

**Refactorización v0.3 → v0.4**: El código originalmente monolítico fue reorganizado en módulos especializados en enero de 2026. La versión 0.4 estandariza completamente la documentación y comentarios del código, mejorando significativamente la estructura y mantenibilidad del proyecto.

**Mejoras v0.4 → v0.5**: La versión 0.5 (enero 2026) añade tres características avanzadas completamente documentadas: Round-Robin Scheduler con Quantum para multitarea preemptiva eficiente, Semáforos con Wait Queues eliminando busy-waiting, y Demand Paging para asignación de memoria bajo demanda mediante Page Fault handling.

[↑ Volver a Tabla de Contenidos](#-tabla-de-contenidos)

---

## Componentes Principales
[↑ Volver a Tabla de Contenidos](#-tabla-de-contenidos)

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

| Módulo                  | Archivo              | Responsabilidad                       |
|-------------------------|----------------------|---------------------------------------|
| **Inicialización**      | `kernel/kernel.c`    | Punto de entrada, setup del sistema   |
| **Gestión de Procesos** | `kernel/process.c`   | PCB, create_process, exit             |
| **Planificador**        | `kernel/scheduler.c` | Algoritmo de aging, sleep, timer_tick |
| **Shell**               | `shell/shell.c`      | Interfaz de comandos, procesos demo   |
| **Utilidades**          | `utils/kutils.c`     | panic, delay, strcmp, strncpy         |
| **Driver UART**         | `drivers/io.c`       | Comunicación serial, kprintf          |
| **Timer/GIC**           | `drivers/timer.c`    | Interrupciones, timer del sistema     |
| **MMU**                 | `mm/mm.c`            | Memoria virtual, tablas de páginas    |
| **Asignador**           | `mm/malloc.c`        | kmalloc, kfree, gestión de heap       |

**Estructura de PCB**:
```c
struct pcb {
    struct cpu_context context;  // Registros guardados (x19-x30, fp, pc, sp)
    long state;                  // UNUSED, RUNNING, READY, BLOCKED, ZOMBIE
    long pid;                    // Identificador único (0-63)
    int priority;                // 0=máxima, 255=mínima
    long prempt_count;           // Contador de desalojamiento
    unsigned long wake_up_time;  // Tick para despertar (sleep)
    char name[16];               // Nombre descriptivo del proceso
    unsigned long stack_addr;    // Dirección base de la pila (kmalloc)
    unsigned long cpu_time;      // Contador de tiempo de CPU
    int block_reason;            // NONE, SLEEP, WAIT
    int exit_code;               // Código de retorno (cuando ZOMBIE)
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

Ubicación: `src/kernel/scheduler.c::schedule()`

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
│ 4. ¡¡CRITICAL: Escribir GICC_EOIR!!  │
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

| Registro          | Dirección  | Propósito                              |
|-------------------|------------|----------------------------------------|
| GICD_CTLR         | 0x08000000 | Distribuidor: enable/disable           |
| GICD_ISENABLER[0] | 0x08000100 | Enable interrupciones (bit 30 = timer) |
| GICC_CTLR         | 0x08010000 | CPU interface: enable/disable          |
| GICC_PMR          | 0x08010004 | Priority mask (0xFF = allow all)       |
| GICC_IAR          | 0x0801000C | Interrupt ACK (leer para obtener ID)   |
| GICC_EOIR         | 0x08010010 | End of Interrupt (CRÍTICO)             |

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

### 7. **Modo Usuario (User Mode) y Syscalls**

#### Visión General

BareMetalM4 implementa **separación de privilegios** usando los niveles de excepción de ARM64:
- **EL1 (Exception Level 1)**: Modo Kernel - Acceso completo al hardware
- **EL0 (Exception Level 0)**: Modo Usuario - Acceso restringido

Esta arquitectura permite crear procesos de usuario que ejecutan con privilegios limitados y deben usar **syscalls** para operaciones privilegiadas.

#### Arquitectura de Niveles de Excepción ARM64

```
┌──────────────────────────────────────────────┐
│  EL3 (Secure Monitor) - No usado             │
├──────────────────────────────────────────────┤
│  EL2 (Hypervisor) - No usado                 │
├──────────────────────────────────────────────┤
│  EL1 (Kernel) ← BareMetalM4 ejecuta aquí     │
│  - Acceso completo a registros de sistema    │
│  - Configuración de MMU, GIC, Timer          │
│  - Manejo de interrupciones y excepciones    │
├──────────────────────────────────────────────┤
│  EL0 (User) ← Procesos de usuario            │
│  - Sin acceso a registros de sistema         │
│  - No puede configurar hardware              │
│  - Debe usar SVC (syscalls) para E/S         │
└──────────────────────────────────────────────┘
```

#### Creación de Procesos de Usuario

**Ubicación**: `src/kernel/process.c:create_user_process()`

El kernel proporciona una función especializada para crear procesos que ejecutan en EL0:

```c
long create_user_process(void (*user_fn)(void), const char *name) {
    /* 1. Asignar stack del usuario (4KB) */
    void *user_stack = kmalloc(4096);
    
    /* 2. Crear contexto de arranque */
    struct user_context *ctx = kmalloc(sizeof(struct user_context));
    ctx->pc = (unsigned long)user_fn;  // Función a ejecutar
    ctx->sp = (unsigned long)user_stack + 4096;  // Tope del stack
    
    /* 3. Crear proceso kernel que transiciona a usuario */
    return create_process(kernel_to_user_wrapper, ctx, 10, name);
}
```

**Flujo de transición**:
```
1. create_user_process() crea proceso kernel normal
2. Proceso kernel ejecuta kernel_to_user_wrapper()
3. Wrapper llama move_to_user_mode(pc, sp)
4. move_to_user_mode() configura SPSR_EL1, ELR_EL1, SP_EL0
5. ERET salta a EL0 con el PC y SP del usuario
6. Proceso ahora ejecuta en modo usuario (EL0)
```

#### Transición Kernel → Usuario (entry.S)

**Ubicación**: `src/entry.S:move_to_user_mode`

```asm
move_to_user_mode:
    // 1. Configurar PSTATE para EL0
    mov x2, #0           // M[3:0] = 0000 (EL0t)
    msr spsr_el1, x2     // DAIF = 0000 (interrupts habilitadas)
    
    // 2. Configurar PC de destino
    msr elr_el1, x0      // x0 = dirección de user_fn
    
    // 3. Configurar stack del usuario
    msr sp_el0, x1       // x1 = tope del stack
    
    // 4. Salto a EL0
    eret                 // Exception Return: salta a EL0
```

**Registros clave**:
- `SPSR_EL1` (Saved Program Status Register): Estado del procesador al que retornar
- `ELR_EL1` (Exception Link Register): Dirección (PC) donde continuar
- `SP_EL0`: Stack pointer del nivel EL0
- `ERET`: Instrucción que realiza el salto y cambia nivel de excepción

#### Sistema de Llamadas (Syscalls)

Los procesos de usuario solicitan servicios del kernel mediante la instrucción **SVC** (SuperVisor Call).

**Syscalls Implementadas**:

| Número | Nombre    | Descripción         | Argumentos          |
|--------|-----------|---------------------|---------------------|
| 0      | SYS_WRITE | Escribir en consola | x19 = char *buffer  |
| 1      | SYS_EXIT  | Terminar proceso    | x19 = int exit_code |

**Convención de Llamada**:
- `x8`: Número de syscall
- `x19`: Primer argumento
- Instrucción: `svc #0`

#### Flujo de Syscall Completo

```
PROCESO USUARIO (EL0)
    │
    ├─ Preparar argumentos:
    │  mov x8, #0        // Número de syscall (SYS_WRITE)
    │  mov x19, buffer   // Argumento 1
    │
    ├─ Ejecutar: svc #0
    │
    ▼
CPU detecta SVC:
    ├─ Guarda PC en ELR_EL1
    ├─ Guarda PSTATE en SPSR_EL1
    ├─ Cambia a EL1
    └─ Salta a VBAR_EL1 + 0x400 (Lower EL sync)
    │
    ▼
KERNEL (vectors.S: el0_sync)
    │
    ├─ kernel_entry (guarda todos los registros)
    ├─ Lee ESR_EL1 (Exception Syndrome Register)
    ├─ Verifica: EC == 0x15 (SVC desde AArch64)
    └─ Salta a handle_svc
    │
    ▼
handle_svc (entry.S)
    │
    ├─ mov x0, sp       // arg1: struct pt_regs *
    ├─ mov x1, x8       // arg2: número de syscall
    └─ bl syscall_handler  // Llama a C
    │
    ▼
syscall_handler (sys.c)
    │
    ├─ switch (syscall):
    │   case SYS_WRITE:
    │       sys_write(regs->x19)
    │   case SYS_EXIT:
    │       sys_exit(regs->x19)
    │
    └─ Retorna
    │
    ▼
handle_svc continúa:
    │
    ├─ kernel_exit (restaura registros)
    └─ eret (retorna a EL0)
    │
    ▼
PROCESO USUARIO (EL0)
    └─ Continúa ejecución después de svc #0
```

#### Ejemplo de Proceso de Usuario

**Ubicación**: `src/utils/tests.c:user_task()`

```c
void user_task() {
    char *msg = "\n[USER] Hola desde EL0!\n";
    
    /* Syscall Write (0) */
    asm volatile(
        "mov x8, #0\n"      // SYS_WRITE
        "mov x19, %0\n"     // Argumento: buffer
        "svc #0\n"          // Llamar al kernel
        : : "r"(msg) : "x8", "x19"
    );
    
    /* Bucle de trabajo */
    for(int i=0; i<10000000; i++) 
        asm volatile("nop");
    
    /* Syscall Exit (1) */
    asm volatile(
        "mov x8, #1\n"      // SYS_EXIT
        "mov x19, #0\n"     // Exit code = 0
        "svc #0\n"
        : : : "x8", "x19"
    );
}
```

**Uso desde shell**:
```
> test_user_mode
[KERNEL] Saltando a Modo Usuario (EL0)...
[USER] Hola desde EL0!
[SYSCALL] Proceso solicitó salida con código 0
```

#### Estructura pt_regs

**Ubicación**: `include/kernel/sys.h`

Almacena el estado del proceso cuando ocurre una excepción:

```c
struct pt_regs {
    unsigned long x19-x28;  // Registros callee-saved
    unsigned long fp;       // Frame pointer (x29)
    unsigned long sp;       // Stack pointer
    unsigned long pc;       // Program counter
    unsigned long pstate;   // Processor state
};
```

Esta estructura es llenada por `kernel_entry` en `entry.S` y permite al kernel:
- Acceder a argumentos de syscalls
- Inspeccionar estado del proceso
- Modificar valores de retorno

#### Ventajas del Modelo User/Kernel

| Aspecto         | Beneficio                                  |
|-----------------|--------------------------------------------|
| **Seguridad**   | Procesos no pueden corromper el kernel     |
| **Estabilidad** | Fallos en EL0 no colapsan el sistema       |
| **Aislamiento** | Procesos separados del código privilegiado |
| **Educativo**   | Demuestra arquitectura de SO modernos      |

#### Limitaciones Actuales

- No hay protección de memoria entre procesos (todos comparten espacio)
- Syscalls limitadas (solo write y exit)
- No hay validación de punteros de usuario
- Stack de usuario compartido (no hay separación por proceso)

---

### 8. **Protección de Memoria y Manejo de Fallos**

#### Manejo de Excepciones Síncronas

El kernel implementa protección básica mediante el manejo de **excepciones síncronas** (synchronous exceptions), como:
- Acceso a memoria inválida (NULL pointer, direcciones no mapeadas)
- Violaciones de permisos de página
- Instrucciones inválidas

#### Handler de Fallos

**Ubicación**: `src/kernel/sys.c:handle_fault()`

Cuando un proceso causa una excepción no manejada:

```c
void handle_fault(void) {
    kprintf("\n[CPU] CRITICAL: Excepcion no controlada (Segmentation Fault)!\n");
    kprintf("[CPU] Matando al proceso actual por violacion de segmento.\n");
    exit();  // Termina proceso culpable sin colapsar el sistema
}
```

**Ventaja clave**: El kernel **sobrevive** a fallos de procesos individuales.

#### Flujo de Manejo de Fallo

```
PROCESO ejecuta: *ptr = 0x0 (escritura en NULL)
    │
    ▼
CPU detecta fallo:
    ├─ Genera excepción síncrona
    ├─ Cambia a EL1 (si estaba en EL0)
    ├─ Guarda PC en ELR_EL1
    └─ Salta a VBAR_EL1 + 0x400 (el0_sync)
    
    ▼
vectors.S: el0_sync
    │
    ├─ kernel_entry (guarda contexto)
    ├─ Lee ESR_EL1 (Exception Syndrome Register)
    ├─ Verifica tipo de excepción
    └─ Si no es SVC → error_invalid
    
    ▼
error_invalid (entry.S)
    │
    ├─ kernel_entry (asegura contexto guardado)
    └─ bl handle_fault  // Llama a C
    
    ▼
handle_fault() (sys.c)
    │
    ├─ Imprime mensaje de diagnóstico
    ├─ Llama a exit() para terminar proceso
    └─ Nunca retorna
    
    ▼
exit() (process.c)
    │
    ├─ Marca proceso como ZOMBIE
    ├─ Llama a schedule() para elegir otro proceso
    └─ Sistema continúa operando normalmente
```

#### Exception Syndrome Register (ESR_EL1)

El registro `ESR_EL1` contiene información sobre la excepción:

```
Bits [31:26] - EC (Exception Class):
    0x00: Unknown reason
    0x07: FP/SIMD access
    0x15: SVC instruction (syscall)
    0x20: Instruction Abort (página no mapeada)
    0x24: Data Abort (acceso inválido a datos)
    0x2F: SError interrupt
    
Bits [24:0] - ISS (Instruction Specific Syndrome):
    Información adicional según el tipo de excepción
```

**Uso en el código**:
```asm
el0_sync:
    kernel_entry
    mrs x25, esr_el1     // Leer ESR_EL1
    lsr x24, x25, #26    // Extraer EC (bits 31:26)
    cmp x24, #0x15       // ¿Es SVC (0x15)?
    b.eq handle_svc      // Si → manejar syscall
    b error_invalid      // No → fallo de segmentación
```

#### Proceso de Prueba: kamikaze_test

**Ubicación**: `src/utils/tests.c:kamikaze_test()`

Proceso diseñado para **provocar un fallo** y demostrar la protección:

```c
void kamikaze_test() {
    kprintf("\n[KAMIKAZE] Soy un proceso malo. Voy a escribir en NULL...\n");
    
    int *p = (int *)0x0;  // Puntero a dirección prohibida
    *p = 1234;             // ¡FALLO! Acceso inválido
    
    // Esta línea nunca se ejecuta
    kprintf("[KAMIKAZE] Si lees esto, la seguridad ha fallado\n");
}
```

**Ejecución desde shell**:
```
> test_crash
[KAMIKAZE] Soy un proceso malo. Voy a escribir en NULL...
[CPU] CRITICAL: Excepcion no controlada (Segmentation Fault)!
[CPU] Matando al proceso actual por violacion de segmento.
> _  ← Shell sigue funcionando
```

#### Tabla de Vectores y Protección

La **tabla de vectores** (`src/vectors.S`) define manejadores para cada tipo de excepción:

```
Offset   | Tipo          | Handler            | Uso
---------|---------------|--------------------|-----------------------
+0x200   | EL1 Sync      | el1_sync           | Syscalls desde kernel
+0x280   | EL1 IRQ       | irq_handler_stub   | Timer interrupts
+0x400   | EL0 Sync      | el0_sync           | Syscalls desde usuario
+0x480   | EL0 IRQ       | irq_handler_stub   | IRQ en modo usuario
Otros    | Variados      | hang               | Excepciones no manejadas
```

**Protección multinivel**:
- Kernel (EL1) y Usuario (EL0) tienen vectores separados
- Permite políticas diferentes según el origen de la excepción
- Usuario no puede evitar el control del kernel

#### Limitaciones de la Protección Actual

| Limitación                     | Descripción                                             | Mejora Futura                           |
|--------------------------------|---------------------------------------------------------|-----------------------------------------|
| **Sin validación de punteros** | Kernel no verifica que punteros de usuario sean válidos | Implementar copy_from_user/copy_to_user |
| **Espacio compartido**         | Todos los procesos ven la misma memoria                 | MMU con tablas de páginas por proceso   |
| **Sin permisos de página**     | No hay distinción RO/RW/RX en páginas                   | Configurar AP bits en descriptores      |
| **Diagnóstico básico**         | No se lee FAR_EL1 (dirección que causó fallo)           | Logging detallado de fallas             |

#### Registros de Depuración

Para diagnósticos avanzados, ARM64 proporciona:

```
FAR_EL1 (Fault Address Register):
    Contiene la dirección que causó el fallo
    
ELR_EL1 (Exception Link Register):
    Contiene el PC donde ocurrió la excepción
    
ESR_EL1 (Exception Syndrome Register):
    Tipo y detalles de la excepción
```

**Ejemplo de uso futuro**:
```c
void handle_fault(void) {
    unsigned long far, elr, esr;
    asm volatile("mrs %0, far_el1" : "=r"(far));
    asm volatile("mrs %0, elr_el1" : "=r"(elr));
    asm volatile("mrs %0, esr_el1" : "=r"(esr));
    
    kprintf("Fallo en PC=0x%x accediendo 0x%x (ESR=0x%x)\n", 
            elr, far, esr);
    exit();
}
```

---

### 9. **Sistema de E/S**

#### UART (Universal Asynchronous Receiver-Transmitter)

Ubicación: `src/drivers/io.c`, `include/drivers/io.h`

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

[↑ Volver a Tabla de Contenidos](#-tabla-de-contenidos)

---

## Flujo de Ejecución
[↑ Volver a Tabla de Contenidos](#-tabla-de-contenidos)

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

[↑ Volver a Tabla de Contenidos](#-tabla-de-contenidos)

---

## Subsistema de Memoria Virtual (MMU)
[↑ Volver a Tabla de Contenidos](#-tabla-de-contenidos)

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

| Rango Físico              | Tamaño | Tipo   | Contenido                           |
|---------------------------|--------|--------|-------------------------------------|
| `0x00000000 - 0x3FFFFFFF` | 1 GB   | Device | UART (0x09000000), GIC (0x08000000) |
| `0x40000000 - 0x7FFFFFFF` | 1 GB   | Normal | Código kernel, stack, datos         |

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

| Índice | Tipo             | Valor | Uso                                         |
|--------|------------------|-------|---------------------------------------------|
| 0      | Device nGnRnE    | 0x00  | Periféricos (sin cache, sin reordenamiento) |
| 1      | Normal sin cache | 0x44  | Memoria compartida CPU/DMA                  |
| 2      | Normal con cache | 0xFF  | RAM del kernel (máximo rendimiento)         |

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

| Ventaja                 | Descripción                                   |
|-------------------------|-----------------------------------------------|
| **Simplicidad**         | Identity mapping (virtual = física)           |
| **Rendimiento**         | Caches activos (I-Cache + D-Cache)            |
| **Protección básica**   | Separación Device/Normal memory               |
| **Asignación dinámica** | kmalloc/kfree para gestión eficiente del heap |
| **Educativo**           | Demuestra conceptos fundamentales de MMU      |

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

| Característica      | Descripción                                          |
|---------------------|------------------------------------------------------|
| **Estrategia**      | First-fit (primer bloque libre que cabe)             |
| **Coalescing**      | Fusión de bloques adyacentes libres                  |
| **Split**           | División de bloques grandes cuando es posible        |
| **Lista enlazada**  | Gestión simple de bloques libres y ocupados          |
| **Alineación**      | Bloques alineados a 16 bytes para ARM64              |
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
│ ├─ UART0: 0x09000000                   │
│ ├─ GIC Distribuidor: 0x08000000        │
│ └─ GIC CPU Intf: 0x08010000            │
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

[↑ Volver a Tabla de Contenidos](#-tabla-de-contenidos)

---

## Subsistema de Planificación
[↑ Volver a Tabla de Contenidos](#-tabla-de-contenidos)

### Estados de Proceso

El kernel implementa un sistema completo de estados de procesos:

```
┌──────────────────────────────────────────────────────────────────┐
│                    CICLO DE VIDA DE UN PROCESO                   │
└──────────────────────────────────────────────────────────────────┘

    ┌───────────┐
    │  UNUSED   │  Slot PCB vacío (no hay proceso)
    └─────┬─────┘
          │ create_process() / create_user_process()
          ▼
    ┌───────────┐
    │   READY   │  Creado, esperando turno de CPU
    └─────┬─────┘
          │ schedule() elige este proceso
          ▼
    ┌───────────┐
    │  RUNNING  │  Ejecutando activamente en CPU
    └─────┬─────┘
          │
          ├──────► sleep() / sem_wait() ───────┐
          │                                    │
          │                                    ▼
          │                             ┌─────────────┐
          │                             │   BLOCKED   │  Esperando evento
          │                             │ (+ reason)  │
          │                             └──────┬──────┘
          │                                    │
          │  ◄──────wake_up_time/sem_signal() ─┘
          │                                    │
          │                                    ▼
          │                              ┌───────────┐
          │                              │   READY   │  Vuelve a la cola
          │                              └───────────┘
          │
          ├──────► exit() ────────────►  ┌───────────┐
          │                              │  ZOMBIE   │  Terminado, esperando reaper
          │                              └───────────┘
          │                                    │
          └────────────────────────────────────┘ (futuro: cleanup automático)

```

#### Definiciones de Estados

| Estado      | Valor | Descripción                           | Transiciones Posibles                                             |
|-------------|-------|---------------------------------------|-------------------------------------------------------------------|
| **UNUSED**  | 0     | Slot PCB vacío, sin proceso asignado  | → READY (create_process)                                          |
| **RUNNING** | 1     | Proceso ejecutando activamente en CPU | → READY (preemption)<br>→ BLOCKED (sleep/wait)<br>→ ZOMBIE (exit) |
| **READY**   | 2     | En cola, esperando ser seleccionado   | → RUNNING (schedule)                                              |
| **BLOCKED** | 3     | Esperando evento (con block_reason)   | → READY (evento ocurre)                                           |
| **ZOMBIE**  | 4     | Terminado, esperando cleanup          | → UNUSED (reaper)                                                 |

#### Razones de Bloqueo (block_reason)

Cuando un proceso está en estado **BLOCKED**, el campo `block_reason` especifica **por qué** está bloqueado:

| Razón                  | Valor | Descripción                    | Desbloqueo                                              |
|------------------------|-------|--------------------------------|---------------------------------------------------------|
| **BLOCK_REASON_NONE**  | 0     | Sin bloqueo activo             | N/A                                                     |
| **BLOCK_REASON_SLEEP** | 1     | Dormido por sleep(ticks)       | `timer_tick()` cuando `sys_timer_count >= wake_up_time` |
| **BLOCK_REASON_WAIT**  | 2     | Esperando semáforo/recurso/I/O | `sem_signal()` o disponibilidad de recurso              |

**Ejemplo de uso**:
```c
// Proceso duerme por 100 ticks
sleep(100);  
// → state = BLOCKED, block_reason = BLOCK_REASON_SLEEP, wake_up_time = sys_timer_count + 100

// Proceso espera semáforo
sem_wait(&mutex);
// → Si semáforo no disponible: state = BLOCKED, block_reason = BLOCK_REASON_WAIT
```

### Estructura PCB Completa

```c
struct pcb {
    struct cpu_context context;  // x19-x30, fp, pc, sp
    long state;                  // UNUSED, RUNNING, READY, BLOCKED, ZOMBIE
    long pid;                    // Process ID (0-63)
    int priority;                // 0=máxima, 255=mínima
    long prempt_count;           // Contador de desalojamiento
    unsigned long wake_up_time;  // Tick para despertar (si BLOCKED por sleep)
    char name[16];               // Nombre descriptivo
    unsigned long stack_addr;    // Dirección base de la pila (kmalloc)
    unsigned long cpu_time;      // Contador de ticks en estado RUNNING
    int block_reason;            // NONE, SLEEP, WAIT
    int exit_code;               // Código de retorno (disponible cuando ZOMBIE)
};
```

**Campos nuevos en v0.4**:
- `cpu_time`: Contador de tiempo de CPU para estadísticas
- `block_reason`: Distingue entre diferentes tipos de bloqueo (NONE, SLEEP, WAIT)
- `exit_code`: Almacena valor de retorno del proceso

**Campos adicionales en v0.5**:
- `quantum`: Ticks restantes antes de preemption (Round-Robin)
- `next`: Puntero para formar listas enlazadas en Wait Queues de semáforos
- `wake_up_time`: Timestamp para sleep() sin busy-wait

### Algoritmo de Scheduling

**Ubicación**: `src/kernel/scheduler.c::schedule()`

El scheduler implementa **prioridades con aging** para prevenir inanición:

```
ALGORITMO: Prioridad + Envejecimiento
═══════════════════════════════════════

ENTRADA: process[] con estados variados
SALIDA: Próximo proceso a ejecutar

FASE 1: ENVEJECIMIENTO (Anti-starvation)
────────────────────────────────────────
Para cada proceso en estado READY (excepto el actual):
    Si priority > 0:
        priority--   // Aumenta prioridad (menor número = mayor prioridad)

Efecto: Procesos esperando gradualmente suben en la cola

FASE 2: SELECCIÓN (Elegir mejor candidato)
───────────────────────────────────────────
highest_priority_found = 1000
Para cada proceso:
    Si state == READY o RUNNING:
        Si priority < highest_priority_found:
            next_pid = este proceso
            highest_priority_found = priority

Si next_pid == -1:  // Nadie disponible
    next_pid = 0    // Kernel IDLE por defecto

FASE 3: PENALIZACIÓN (Evitar monopolio)
────────────────────────────────────────
Si next->priority < 10:
    next->priority += 2   // Baja prioridad del elegido

Efecto: Proceso que acaba de ejecutar debe esperar más

FASE 4: CAMBIO DE CONTEXTO
───────────────────────────
Si prev != next:
    prev->state = READY (si estaba RUNNING)
    next->state = RUNNING
    current_process = next
    cpu_switch_to(prev, next)   // Assembly context switch
```

### Tabla de Prioridades

| Rango   | Clasificación | Uso Típico                                |
|---------|---------------|-------------------------------------------|
| 0-9     | **Crítica**   | Procesos de sistema (shell, kernel tasks) |
| 10-63   | **Alta**      | Procesos interactivos, timeshare normal   |
| 64-127  | **Media**     | Procesos background                       |
| 128-191 | **Baja**      | Tareas de mantenimiento                   |
| 192-255 | **Mínima**    | Solo ejecutan si envejecen suficiente     |

**Notas importantes**:
- Menor número = Mayor prioridad (0 es máxima)
- El aging decrementa priority en 1 cada tick para procesos READY
- La penalización incrementa priority en 2 al ser seleccionado
- Esto garantiza **fair scheduling** sin inanición

### Sistema de Despertar (Wake-up)

**Ubicación**: `src/kernel/scheduler.c::timer_tick()`

En cada tick del timer (~10ms):

```c
void timer_tick(void) {
    sys_timer_count++;  // Incrementar reloj global
    
    // Contabilizar tiempo de CPU del proceso actual
    if (current_process->state == PROCESS_RUNNING) {
        current_process->cpu_time++;
    }
    
    // Revisar procesos bloqueados por sleep
    for (cada proceso) {
        if (state == BLOCKED && block_reason == BLOCK_REASON_SLEEP) {
            if (wake_up_time <= sys_timer_count) {
                state = READY;           // Despertar
                block_reason = NONE;     // Limpiar razón
            }
        }
    }
}
```

El envejecimiento garantiza que **todos los procesos eventualmente ejecutan** (previene inanición).

---

### Sleep: Dormir un Proceso por Tiempo Determinado

**Ubicación**: `src/kernel/scheduler.c::sleep()`

Mecanismo para que un proceso se bloquee **temporalmente** (a diferencia de semáforos que se bloquean indefinidamente).

#### Comparación: delay() vs sleep()

| Aspecto            | delay()                  | sleep()                      |
|--------------------|--------------------------|------------------------------|
| **Tipo**           | Busy-wait                | Bloqueo con timer            |
| **CPU**            | Consume (bucle infinito) | Libera (para otros procesos) |
| **Otros procesos** | NO pueden ejecutar       | PUEDEN ejecutar              |
| **Precisión**      | Exacta (ciclos de CPU)   | Aproximada (~10ms)           |
| **Uso**            | Timing preciso           | Delays normales              |

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

// En scheduler.c:
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

| Mecanismo         | Bloqueo          | Duración           | Uso             |
|-------------------|------------------|--------------------|-----------------|
| **delay()**       | No (consume CPU) | Precisa            | Timing exacto   |
| **sleep()**       | Sí (libera CPU)  | Aproximada         | Delays normales |
| **sem_wait()**    | Sí               | Indefinida         | Recursos/mutex  |
| **Condition var** | Sí               | Indefinida/timeout | Eventos         |

#### Limitaciones

- **No es exacto**: ±10ms de precisión
- **Overhead**: Chequeo en cada interrupt (~50 ciclos)
- **Proceso duerme más**: Espera a ser seleccionado nuevamente
- **Requiere interrupts**: Sin enable_interrupts(), nunca despierta

[↑ Volver a Tabla de Contenidos](#-tabla-de-contenidos)

---

## Decisiones de Diseño
[↑ Volver a Tabla de Contenidos](#-tabla-de-contenidos)

### ✅ Decisiones Acertadas

| Decisión                   | Razón                                             |
|----------------------------|---------------------------------------------------|
| **Bare-metal** (sin Linux) | Aprende cómo funciona todo internamente           |
| **ARM64**                  | Arquitectura moderna, común en móviles/servidores |
| **QEMU virt**              | Emulador accesible, GIC realista                  |
| **LDXR/STXR spinlocks**    | Corrección: imposible race condition              |
| **Envejecimiento**         | Previene inanición, educativo                     |
| **Timer interrupts**       | Preemption: cambios no cooperativos               |

### ⚠️ Limitaciones Intencionales

| Limitación              | Razón                         | Mejora Real             |
|-------------------------|-------------------------------|-------------------------|
| **Busy-wait semáforos** | Simplicidad educativa         | Wait queues + wakeup    |
| **Single-core**         | Evita sincronización compleja | Multicore con spinlocks |
| **Sin memoria virtual** | Omitir MMU                    | Paging + TLB            |
| **Sin filesystem**      | Scope limitado                | VFS + inode cache       |
| **Sin IPC avanzado**    | Educativo                     | Message queues, pipes   |
| **UART polling**        | Implementación simple         | Interrupts + buffers    |

[↑ Volver a Tabla de Contenidos](#-tabla-de-contenidos)

---

## Estructura de Memoria
[↑ Volver a Tabla de Contenidos](#-tabla-de-contenidos)

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
│ ├─ Proceso 0: 0x500000                 │
│ ├─ Proceso 1: 0x501000                 │
│ └─ Proceso N: 0x50N000                 │
├────────────────────────────────────────┤
│ MMIO (Memory-Mapped I/O)               │
│ ├─ UART0: 0x09000000                   │
│ ├─ GIC Distribuidor: 0x08000000        │
│ └─ GIC CPU Intf: 0x08010000            │
├────────────────────────────────────────┤
│ 0x00000000                             │
└────────────────────────────────────────┘
```

### Memory Map (QEMU virt)

| Rango                   | Propósito         |
|-------------------------|-------------------|
| 0x80000000 - 0x80FFFFFF | Kernel (8 MB)     |
| 0x09000000              | UART0             |
| 0x08000000              | GIC Distribuidor  |
| 0x08010000              | GIC CPU Interface |

[↑ Volver a Tabla de Contenidos](#-tabla-de-contenidos)

---

## Limitaciones y Mejoras Futuras
[↑ Volver a Tabla de Contenidos](#-tabla-de-contenidos)

### ✅ Características Implementadas (v0.5)
- [x] Arquitectura modular con separación de subsistemas
- [x] Asignación dinámica de memoria (kmalloc/kfree)
- [x] **Planificador Round-Robin con Quantum** - **✅ IMPLEMENTADO en v0.5**
- [x] **Semáforos con Wait Queues (sin busy-wait)** - **✅ IMPLEMENTADO en v0.5**
- [x] **Demand Paging (asignación bajo demanda)** - **✅ IMPLEMENTADO en v0.5**
- [x] Shell interactivo con múltiples comandos
- [x] MMU con memoria virtual y tablas de páginas de 3 niveles
- [x] Interrupciones de timer con GIC v2
- [x] Sincronización con spinlocks y semáforos eficientes
- [x] **Modo Usuario (EL0) con separación de privilegios**
- [x] **Sistema de syscalls (SYS_WRITE, SYS_EXIT)**
- [x] **Protección de memoria con manejo de excepciones**
- [x] **Manejo robusto de fallos de segmentación**
- [x] **Documentación completa estilo Doxygen en todos los archivos**
- [x] Código estandarizado y profesional

### Fase 1: Mejoras Educativas (Siguientes)
- [x] ~~Agregar syscalls (SVC exception)~~ - **✅ IMPLEMENTADO en v0.4**
- [x] ~~Procesos de usuario (EL0)~~ - **✅ IMPLEMENTADO en v0.4**
- [x] ~~Manejo de excepciones y fallos~~ - **✅ IMPLEMENTADO en v0.4**
- [x] ~~Implementar wait queues en semáforos~~ - **✅ IMPLEMENTADO en v0.5**
- [x] ~~Planificador Round-Robin con quantum~~ - **✅ IMPLEMENTADO en v0.5**
- [x] ~~Demand Paging (Page Fault handling)~~ - **✅ IMPLEMENTADO en v0.5**
- [ ] Expandir syscalls (read, open, close, fork)
- [ ] Soporte para múltiples CPUs (spinlocks existentes)
- [ ] Keyboard input vía UART (lectura) - Parcialmente implementado
- [ ] Mejorar asignador (best-fit, estadísticas)

### Fase 2: Características Reales
- [ ] Memory management avanzado (protección por proceso con MMU)
- [ ] Virtual memory con paginación dinámica y swap
- [ ] Validación de punteros de usuario (copy_from_user/copy_to_user)
- [ ] Permisos de página (RO/RW/RX) con AP bits
- [ ] Filesystem (FAT o ext2)
- [ ] System calls completos (fork, exec, wait, pipe)

### Fase 3: Optimizaciones
- [ ] Timer events (en lugar de polling)
- [ ] IPC avanzado (message passing, pipes, shared memory)
- [ ] Condition variables
- [ ] RCU (Read-Copy-Update)
- [ ] Multicore scheduling
- [ ] Diagnóstico avanzado (lectura de FAR_EL1, análisis de ESR_EL1)

[↑ Volver a Tabla de Contenidos](#-tabla-de-contenidos)

---

## Diagrama de Componentes

```
┌──────────────────────────────────────────────────────────────┐
│                   QEMU Virt (ARM64)                          │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  ┌─────────────────┐           ┌──────────────────┐          │
│  │  CPU (ARM64)    │           │  Memory          │          │
│  │  ┌───────────┐  │           │  - 128 MB        │          │
│  │  │ EL1       │  │           │  - MMIO          │          │
│  │  │ (Kernel)  │  │           │  - Code + Data   │          │
│  │  └─────┬─────┘  │           │  - Heap (64MB)   │          │
│  │        │        │           └──────────────────┘          │
│  │  ┌─────▼─────┐  │                                         │
│  │  │ EL0       │  │           ┌──────────────────┐          │
│  │  │ (User)    │  │           │  MMU + Caches    │          │
│  │  └───────────┘  │           │  - I-Cache       │          │
│  │  - Timer        │           │  - D-Cache       │          │
│  │  - GIC iface    │           │  - TLB           │          │
│  └────────┬────────┘           └──────────────────┘          │
│           │                                                  │
│           └──────────┬───────────────┬──────┐                │
│                      │               │      │                │
│           ┌──────────▼──┐    ┌───────▼─┐   │                 │
│           │   GIC Dist  │    │ Timer   │   │                 │
│           │ (0x08000000)│    │ (ARM64) │   │                 │
│           └─────────────┘    └────┬────┘   │                 │
│                                   │        │                 │
│           ┌───────────┬───────────┘        │                 │
│           │           │                    │                 │
│   ┌───────▼────┐  ┌──▼──────────┐   ┌────▼──────────┐        │
│   │ UART (TTY) │  │ GIC CPU If  │   │ Kernel Code   │        │
│   │(0x09000000)│  │(0x08010000) │   │ + User Procs  │        │
│   └────────────┘  └─────────────┘   └───────────────┘        │
│                                                              │
│  EXCEPTION FLOW:                                             │
│  User (EL0) ──SVC──> Kernel (EL1) ──ERET──> User (EL0)       │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

### Diagrama de Flujo: Syscall desde User Mode

```
┌─────────────────────────────────────────────────┐
│  PROCESO USUARIO (EL0)                          │
│  - Ejecuta código sin privilegios               │
│  - No puede acceder a registros de sistema      │
└─────────────────┬───────────────────────────────┘
                  │
                  │ svc #0 (syscall)
                  ▼
┌─────────────────────────────────────────────────┐
│  CPU: EXCEPTION (Synchronous from EL0)          │
│  1. Guarda PC → ELR_EL1                         │
│  2. Guarda PSTATE → SPSR_EL1                    │
│  3. Cambia a EL1                                │
│  4. Salta a VBAR_EL1 + 0x400                    │
└─────────────────┬───────────────────────────────┘
                  │
                  ▼
┌─────────────────────────────────────────────────┐
│  KERNEL (EL1): el0_sync handler                 │
│  1. kernel_entry (guarda x0-x30, sp, fp)        │
│  2. Lee ESR_EL1 para ver tipo de excepción      │
│  3. Si es SVC → handle_svc                      │
│  4. Si no es SVC → handle_fault                 │
└─────────────────┬───────────────────────────────┘
                  │
                  ▼
┌─────────────────────────────────────────────────┐
│  syscall_handler(regs, syscall_num)             │
│  1. Extrae argumentos de struct pt_regs         │
│  2. Switch según número de syscall:             │
│     - SYS_WRITE → sys_write(regs->x19)          │
│     - SYS_EXIT → sys_exit(regs->x19)            │
│  3. Retorna                                     │
└─────────────────┬───────────────────────────────┘
                  │
                  ▼
┌─────────────────────────────────────────────────┐
│  KERNEL: kernel_exit                            │
│  1. Restaura x0-x30, sp, fp                     │
│  2. eret (Exception Return)                     │
│  3. Restaura PC desde ELR_EL1                   │
│  4. Restaura PSTATE desde SPSR_EL1              │
│  5. Cambia a EL0                                │
└─────────────────┬───────────────────────────────┘
                  │
                  ▼
┌─────────────────────────────────────────────────┐
│  PROCESO USUARIO (EL0)                          │
│  - Continúa ejecución después de svc #0         │
│  - Resultado de syscall disponible              │
└─────────────────────────────────────────────────┘
```

---

## Referencias

### Registros ARM64 Importantes

#### Registros de Control
- **MPIDR_EL1**: Multiprocessor Affinity Register (CPU ID)
- **VBAR_EL1**: Vector Base Address Register (tabla de excepciones)
- **SCTLR_EL1**: System Control Register (MMU, caches, alignment)

#### Registros de Excepción
- **ELR_EL1**: Exception Link Register (dirección de retorno)
- **SPSR_EL1**: Saved Program Status Register (estado del procesador)
- **ESR_EL1**: Exception Syndrome Register (tipo y detalles de excepción)
- **FAR_EL1**: Fault Address Register (dirección que causó fallo)

#### Registros de Timer
- **CNTFRQ_EL0**: Counter Frequency Register (frecuencia del timer)
- **CNTP_TVAL_EL0**: Timer Value Register (ticks restantes)
- **CNTP_CTL_EL0**: Timer Control Register (enable/disable)

#### Registros de MMU
- **TTBR0_EL1**: Translation Table Base Register 0 (tabla de páginas user)
- **TTBR1_EL1**: Translation Table Base Register 1 (tabla de páginas kernel)
- **TCR_EL1**: Translation Control Register (configuración de paginación)
- **MAIR_EL1**: Memory Attribute Indirection Register (tipos de memoria)

#### Registros de Stack
- **SP_EL0**: Stack Pointer para EL0 (modo usuario)
- **SP_EL1**: Stack Pointer para EL1 (modo kernel)

### Exception Syndrome Register (ESR_EL1) - Valores Clave

| EC (bits 31:26) | Descripción                                |
|-----------------|--------------------------------------------|
| 0x00            | Unknown reason                             |
| 0x07            | Access to SIMD or FP register              |
| 0x15            | **SVC instruction executed (syscall)**     |
| 0x20            | Instruction Abort (página no mapeada)      |
| 0x24            | **Data Abort (acceso inválido a memoria)** |
| 0x2F            | SError interrupt                           |

### Publicaciones y Referencias
- ARM v8 Architecture Manual (Official)
- QEMU virt board documentation
- Linux kernel (scheduler fuente)

---

**Última actualización**: Enero 25, 2026  
**Versión**: 0.5  
**Refactorización**: Estructura modular, sistema de memoria dinámica, documentación completa y estandarización de código implementados (Enero 2026)

---

## Historial de Cambios

### v0.5 - Enero 25, 2026
- ✅ **Documentación Completa del Sistema**
  - Documentación exhaustiva de todos los subsistemas principales
  - Round-Robin Scheduler con Quantum completamente documentado
  - Semáforos con Wait Queues (sin busy-wait) explicados en detalle
  - Demand Paging con Page Fault handling documentado paso a paso
  - Integración entre subsistemas claramente explicada
  - Referencias cruzadas entre archivos y funciones
  - Proceso de creación de usuario (`create_user_process()`) documentado
  - Todas las funciones en process.c completamente documentadas

- ✅ **Mejoras en Consistencia de Código**
  - Estilo Doxygen consistente en todos los archivos C/H
  - Secciones visuales (`/* === */`) estandarizadas
  - Descripciones @details para todas las funciones principales
  - Documentación de campos PCB (quantum, next, block_reason, wake_up_time)
  - Ciclo de vida de procesos claramente documentado
  - Integración con timer, scheduler y memoria explicada

### v0.4 - Enero 21, 2026
- ✅ **Estandarización completa del código y documentación**
  - Estandarización de todos los comentarios del código siguiendo formato Doxygen
  - Organización sistemática de secciones en todos los archivos fuente
  - Headers de archivo actualizados con @file, @brief, @details, @author, @version
  - Documentación de funciones con @param, @return, @details
  - Eliminación de código informal y comentarios no profesionales
  - Corrección de errores de compatibilidad (nullptr → 0 en C)
  - Separadores de sección estandarizados (76 caracteres `=`)
  - Mejora significativa en la legibilidad y mantenibilidad del código
  
- ✅ **Implementación de Modo Usuario (User Mode)**
  - Separación de privilegios: EL1 (Kernel) y EL0 (Usuario)
  - Función `create_user_process()` para procesos restringidos
  - Transición Kernel→Usuario con `move_to_user_mode()` en assembly
  - Configuración de SPSR_EL1, ELR_EL1, SP_EL0 para cambio de nivel
  - Proceso de prueba `user_task()` que ejecuta en EL0
  
- ✅ **Sistema de Syscalls (System Calls)**
  - Infraestructura completa de syscalls usando instrucción SVC
  - SYS_WRITE (0): Escritura en consola desde modo usuario
  - SYS_EXIT (1): Terminación ordenada de procesos
  - Dispatcher `syscall_handler()` con tabla de syscalls
  - Manejo de excepciones síncronas desde EL0 y EL1
  - Estructura `pt_regs` para acceso a argumentos y estado
  
- ✅ **Protección de Memoria y Manejo de Fallos**
  - Handler `handle_fault()` para excepciones no controladas
  - Manejo robusto de segmentation faults sin colapsar el sistema
  - Terminación selectiva de procesos culpables
  - Proceso de prueba `kamikaze_test()` para validar protección
  - Análisis de ESR_EL1 para distinguir tipos de excepción
  - Vector tables separados para EL0 y EL1
  
- ✅ **Mejoras en Documentación**
  - Actualización completa de ARCHITECTURE.md con nuevas secciones
  - Documentación exhaustiva de User Mode y syscalls
  - Diagramas de flujo para transiciones EL1↔EL0
  - Ejemplos de código para cada funcionalidad
  - Explicación detallada de registros ARM64 (ESR_EL1, FAR_EL1, etc.)
  - Preparación del código para producción y uso educativo avanzado

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
  - 8 comandos: help, ps, test, test_user_mode, test_crash, clear, panic, poweroff
  - Comando `ps` muestra estado detallado (RUN/RDY/SLEEP/WAIT/ZOMB), tiempo de CPU
  - Comandos de test para validar modo usuario y protección de memoria
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
