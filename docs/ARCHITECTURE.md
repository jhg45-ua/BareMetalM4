# BareMetalM4: Arquitectura del Kernel

## 📋 Tabla de Contenidos
1. [Visión General](#visión-general)
2. [Estructura del Código](#estructura-del-código)
3. [Componentes Principales](#componentes-principales)
4. [Subsistema de Planificación](#subsistema-de-planificación)
5. [Decisiones de Diseño](#decisiones-de-diseño)
6. [Estructura de Memoria](#estructura-de-memoria)
7. [Limitaciones y Mejoras Futuras](#limitaciones-y-mejoras-futuras)
8. [Diagrama de Componentes](#diagrama-de-componentes)
9. [Referencias](#referencias)
10. [Historial de Cambios](#historial-de-cambios)

---

## Visión General
[↑ Volver a Tabla de Contenidos](#-tabla-de-contenidos)

**BareMetalM4** es un kernel operativo educativo para **ARM64** (AArch64) que demuestra conceptos fundamentales y avanzados de sistemas operativos:

- ✅ **Multitarea cooperativa y expropiativa**
- ✅ **Planificador Round-Robin con Quantum** (v0.6) - Preemption basada en tiempo
- ✅ **Planificación con prioridades y envejecimiento (aging)**
- ✅ **Manejo de interrupciones y excepciones**
- ✅ **Sincronización: spinlocks y semáforos con Wait Queues** (v0.6) - Sin busy-wait
- ✅ **Gestión de memoria virtual (MMU) con Demand Paging** (v0.6) - Asignación bajo demanda
- ✅ **Modo Usuario (EL0) con syscalls y protección de memoria**
- ✅ **Sistema de Archivos en Memoria (RamFS)** (v0.6) - VFS con persistencia en RAM
- ✅ **I/O a través de UART QEMU con interrupciones**

### Plataforma Objetivo
- **Arquitectura**: ARM64 (ARMv8)
- **Emulador**: QEMU virt (`qemu-system-aarch64`)
- **Sin Sistema Operativo Base**: Bare-metal (sin Linux, sin librería C estándar)

[↑ Volver a Tabla de Contenidos](#-tabla-de-contenidos)

---

## Estructura del Código
[↑ Volver a Tabla de Contenidos](#-tabla-de-contenidos)

El kernel está organizado en **módulos especializados** siguiendo el principio de **separación de responsabilidades**. 
Esta refactorización (enero 2026) dividió el código monolítico original en componentes bien definidos.

**Contenido de esta sección:**
- [Organización de Directorios](#organización-de-directorios)
- [Descripción de Archivos Clave](#descripción-de-archivos-clave)
- [Módulos del Kernel](#módulos-del-kernel)
- [Mejoras de cada version](#mejoras-de-cada-version)

### Organización de Directorios

```
BareMetalM4/
├── include/                    # Headers del sistema
│   ├── sched.h                 # Definiciones de PCB y estados de proceso
│   ├── semaphore.h             # Primitivas de sincronización con Wait Queues (v0.6)
│   ├── types.h                 # Tipos básicos del sistema (uint64_t, etc.)
│   ├── drivers/                # Headers de controladores hardware
│   │   ├── io.h                #   Interfaz UART y kprintf
│   │   └── timer.h             #   Configuración GIC y timer del sistema
│   │
│   ├── fs/                     # Headers del sistema de archivos (v0.6)
│   │   └── vfs.h               #   Definiciones VFS: iNodos, superbloque, File Descriptors
│   │
│   ├── kernel/                 # Headers de módulos del kernel
│   │   ├── process.h           #   Gestión de procesos y threads
│   │   ├── scheduler.h         #   Planificador Round-Robin con Quantum (v0.6)
│   │   └── sys.h               #   Syscalls (write, exit, open, read) y Demand Paging (v0.6)
│   │
│   ├── shell/                  # Headers de interfaz de usuario (v0.6)
│   │   └── shell.h             #   Shell interactivo con parser de argumentos
│   │
│   ├── utils/                  # Headers de utilidades (v0.6)
│   │   ├── kutils.h            #   Utilidades generales (panic, delay, strcmp, k_strlen)
│   │   └── tests.h             #   Interfaz de funciones de prueba
│   │
│   └── mm/                     # Headers de gestión de memoria
│       ├── malloc.h            #   Asignador dinámico (kmalloc/kfree)
│       ├── mm.h                #   Interfaz MMU y memoria virtual
│       ├── pmm.h               #   Gestor de memoria física (Physical Memory Manager)
│       └── vmm.h               #   Gestor de memoria virtual (Virtual Memory Manager)
│
├── src/                        # Código fuente
│   ├── boot.S                  # Punto de entrada ARM64 (assembly)
│   ├── entry.S                 # Context switch, IRQ handlers, syscalls
│   ├── vectors.S               # Tabla de vectores de excepciones (VBAR_EL1)
│   ├── locks.S                 # Spinlocks con LDXR/STXR (atomic ops)
│   ├── utils.S                 # Utilidades de sistema (registros, timer)
│   ├── mm_utils.S              # Funciones MMU en assembly (get/set registros)
│   ├── semaphore.c             # Implementación de semáforos con Wait Queues (v0.6)
│   │
│   ├── drivers/                # Controladores hardware
│   │   ├── io.c                #   Driver UART y kprintf formateado
│   │   └── timer.c             #   Inicialización GIC v2, timer físico, tick (v0.6)
│   │
│   ├── fs/                     # Sistema de archivos (v0.6)
│   │   └── ramfs.c             #   Implementación RamFS: iNodos, superbloque, operaciones
│   │
│   ├── kernel/                 # Módulos principales del kernel
│   │   ├── kernel.c            #   Punto de entrada C e inicialización
│   │   ├── process.c           #   Gestión de PCB, create_process, exit
│   │   ├── scheduler.c         #   Round-Robin + Aging + Quantum (v0.6)
│   │   └── sys.c               #   Syscalls (write, exit) y Demand Paging (v0.6)
│   │
│   ├── mm/                     # Gestión de memoria
│   │   ├── mm.c                #   Configuración MMU, tablas de páginas, TLB
│   │   ├── malloc.c            #   Asignador dinámico (kmalloc/kfree)
│   │   ├── pmm.c               #   Gestor de páginas físicas (bitmap, get_free_page)
│   │   └── vmm.c               #   Gestor de páginas virtuales (map_page, demand paging)
│   │
│   ├── shell/                  # Interfaz de usuario
│   │   └── shell.c             #   Shell con parser de argumentos y comandos de filesystem (v0.6)
│   │
│   └── utils/                  # Utilidades generales
│       ├── kutils.c            #   panic, delay, strcmp, strncpy, memcpy, k_strlen (v0.6)
│       └── tests.c             #   Procesos de prueba (user_task, kamikaze_test, demand_test)
│
├── docs/                       # Documentación del proyecto
│   └── ARCHITECTURE.md         # Este documento (arquitectura completa)
│
└── [build artifacts]           # Generados durante compilación
    └── build/                  # Directorio de compilación
        ├── *.o                 #   Objetos intermedios (.o)
        └── baremetalm4.elf     #   Ejecutable ELF final
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
| Archivo       | Líneas | Descripción                                                      |
|---------------|--------|------------------------------------------------------------------|
| `kernel.c`    | ~200   | Inicialización del sistema, loop principal WFI                   |
| `process.c`   | ~280   | PCB management, create_process, create_user_process              |
| `scheduler.c` | ~200   | Round-Robin + Aging + Quantum (v0.6), schedule(), timer_tick()   |
| `sys.c`       | ~205   | Syscall dispatcher, Demand Paging handler (v0.6), handle_fault() |

#### Drivers (.c)
| Archivo   | Líneas | Descripción                                                     |
|-----------|--------|-----------------------------------------------------------------|
| `io.c`    | ~150   | UART driver, kprintf con formato %s/%d/%x/%c                    |
| `timer.c` | ~200   | Configuración GIC v2, timer físico, tick para quantum (v0.6)    |

#### Memory Management (.c)
| Archivo    | Líneas | Descripción                                                      |
|------------|--------|------------------------------------------------------------------|
| `mm.c`     | ~250   | Configuración MMU, tablas L1/L2/L3, TLB invalidation             |
| `pmm.c`    | ~180   | Physical Memory Manager: bitmap, get_free_page() (v0.6)          |
| `vmm.c`    | ~200   | Virtual Memory Manager: map_page(), demand paging support (v0.6) |
| `malloc.c` | ~250   | kmalloc/kfree, heap dinámico, lista enlazada, coalescing parcial |

#### File System (.c)
| Archivo    | Líneas | Descripción                                                      |
|------------|--------|------------------------------------------------------------------|
| `ramfs.c`  | ~244   | RamFS: superbloque, iNodos, FD, create/open/read/write/remove   |

#### User Interface & Tests (.c)
| Archivo       | Líneas | Descripción                                                          |
|---------------|--------|----------------------------------------------------------------------|
| `shell.c`     | ~320   | Shell con parser de argumentos y 11 comandos (v0.6: +touch/rm/ls/cat/write) |
| `tests.c`     | ~250   | user_task (EL0), kamikaze_test, demand_test, semaphore tests        |
| `kutils.c`    | ~110   | panic, delay, strcmp, strncpy, memset, memcpy, k_strlen (v0.6)      |
| `semaphore.c` | ~150   | sem_init, sem_wait, sem_signal con Wait Queues (v0.6)               |

**Total de líneas de código**: ~3,200 líneas (sin comentarios y espacios en blanco)
**Total bruto**: ~4,600 líneas (incluyendo documentación)
**Incremento v0.5 → v0.6**: +300 líneas (~10% más funcionalidad)

### Módulos del Kernel

#### 1. **kutils** (Utilidades del Kernel)
**Archivos**: `src/utils/kutils.c`, `include/utils/kutils.h`

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
| `free_zombie()`        | Limpia procesos zombie y libera recursos (v0.6)     |
| `schedule_tail()`      | Hook post-context-switch (futuras extensiones)        |

**Estructura de Datos**:
```c
struct pcb {
    struct cpu_context context;  // x19-x30, fp, pc, sp
    long state;                  // UNUSED, RUNNING, READY, BLOCKED, ZOMBIE
    long pid;                    // Process ID (0-63)
    int priority;                // 0=máxima, 255=mínima
    long prempt_count;           // Contador de desalojamiento
    unsigned long wake_up_time;  // Para sleep() sin busy-wait
    char name[16];               // Nombre descriptivo
    unsigned long stack_addr;    // Dirección base de la pila
    unsigned long cpu_time;      // Tiempo de CPU usado
    int block_reason;            // NONE, SLEEP, WAIT
    int exit_code;               // Código de retorno
    int quantum;                 // (v0.6) Ticks restantes antes de preemption
    struct pcb *next;            // (v0.6) Para Wait Queues en semáforos
};
```

**Nota**: En la versión actual, `create_process()` utiliza `kmalloc()` para asignar dinámicamente las pilas de 4KB de cada proceso, en lugar de usar un array estático.

**Limpieza de Procesos Zombie (v0.6)**: La función `free_zombie()` realiza una limpieza exhaustiva de los procesos terminados:

```c
void free_zombie() {
    for (cada proceso en estado ZOMBIE) {
        /* 1. Liberar la memoria dinámica de la pila */
        if (process[i].stack_addr != 0) {
            kfree((void *)process[i].stack_addr);
            process[i].stack_addr = 0;
        }
        
        /* 2. Limpiar toda la estructura PCB */
        process[i].pid = 0;
        process[i].priority = 0;
        process[i].cpu_time = 0;
        process[i].wake_up_time = 0;
        process[i].quantum = 0;
        memset(process[i].name, 0, sizeof(process[i].name));
        
        /* 3. Marcar como libre para reutilización */
        process[i].state = PROCESS_UNUSED;
        
        /* Nota: Si tuviera archivos abiertos, se cerrarían aquí */
    }
}
```

**Mejoras en v0.6**:
- ✅ Prevención de memory leaks mediante `kfree()` explícito
- ✅ Limpieza completa del PCB para evitar datos residuales
- ✅ Reinicio de todos los campos antes de marcar como UNUSED
- ✅ Preparado para extensiones futuras (cierre de archivos, liberación de recursos)

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

| Comando            | Descripción          | Funcionalidad                                                                                       |
|--------------------|----------------------|-----------------------------------------------------------------------------------------------------|
| `help`             | Muestra ayuda        | Lista todos los comandos disponibles con descripción                                                |
| `ps`               | Process Status       | Lista procesos activos con PID, prioridad, estado (RUN/RDY/SLEEP/WAIT/ZOMB), tiempo de CPU y nombre |
| `touch [archivo]`  | Crear archivo        | **(v0.6)** Crea un archivo vacío en el RamFS                                                        |
| `rm [archivo]`     | Borrar archivo       | **(v0.6)** Elimina un archivo del disco virtual                                                     |
| `ls`               | Listar archivos      | **(v0.6)** Muestra ID, tamaño (bytes) y nombre de todos los archivos                                |
| `cat [archivo]`    | Leer archivo         | **(v0.6)** Muestra el contenido completo de un archivo                                              |
| `write [archivo]`  | Escribir archivo     | **(v0.6)** Escribe texto predefinido en el archivo                                                  |
| `test [módulo]`    | Tests modulares      | **(v0.6)** Acepta argumentos: `all` (completo), `rr` (quantum), `sem` (semáforos), `pf` (paging)   |
| `clear`            | Limpiar pantalla     | Limpia terminal usando códigos ANSI (ESC[2J ESC[H)                                                  |
| `panic`            | Kernel Panic         | Provoca un kernel panic intencionalmente (demo)                                                     |
| `poweroff`         | Apagar sistema       | Apaga QEMU usando system_off() (semihosting)                                                       |

**Características del Shell**:
- ✅ Entrada interactiva con eco local
- ✅ Soporte de backspace (127 / '\b')
- ✅ Buffer de comando de 64 caracteres
- ✅ **Parser de argumentos** (v0.6) - divide comando y parámetros
- ✅ Sleep eficiente cuando no hay entrada (no consume CPU)
- ✅ Prompt visual (`> `)
- ✅ Mensajes de error para comandos desconocidos

**Ejemplos de Uso**:
```
> help
Comandos disponibles:
  help               - Muestra esta ayuda
  ps                 - Lista los procesos
  touch [archivo]    - Crea un archivo vacio
  rm [archivo]       - Borra un archivo
  ls                 - Lista los archivos
  cat [archivo]      - Lee el contenido de un archivo
  write [archivo]    - Escribe texto en un archivo
  test [modulo]      - Ejecuta tests. Modulos: all, rr, sem, pf
  clear              - Limpia la pantalla
  panic              - Provoca un Kernel Panic
  poweroff           - Apaga el sistema

> touch readme.txt
[VFS] Archivo 'readme.txt' creado con éxito (Inodo 0).

> write readme.txt
Escritos 48 bytes en 'readme.txt'.

> ls

ID  |   Size (Bytes)   | Name
----|------------------|----------------------
0   |   48             | readme.txt

> cat readme.txt

Texto generado dinamicamente desde la Shell.

> rm readme.txt
[VFS] Archivo 'readme.txt' eliminado.

> test rr
[TEST] Iniciando test de Round-Robin con Quantum...

> ps
PID | Prio | State | Time | Name
----|------|-------|------|------
 0  |  0   | RUN   | 1523 | Kernel
 1  |  1   | RDY   | 342  | Shell
 2  |  5   | SLEEP | 89   | proceso_1
 3  |  8   | RDY   | 156  | proceso_2
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

| Función                | Descripción                                    |
|------------------------|------------------------------------------------|
| `timer_init()`         | Inicializa GIC v2 y timer del sistema          |
| `handle_timer_irq()`   | Manejador de interrupciones del timer          |
| `enable_interrupts()`  | Habilita IRQs en DAIF (limpia bit I)           |
| `disable_interrupts()` | Deshabilita IRQs en DAIF (activa bit I)        |

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
| `map_page()` | Mapea una página virtual a física (L1/L2/L3)       |

**Caracteristicas**:
- Tablas de paginas multinivel (L1/L2/L3) con paginas de 4KB
- Identity mapping (virtual = fisica)
- Tipos de memoria: Device (perifericos) y Normal (RAM)
- Activacion de I-Cache y D-Cache

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
- Coalescing parcial de bloques adyacentes (hacia adelante)
- Gestión de heap desde dirección base configurable
- Reduce fragmentación externa, pero no elimina toda

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
**Archivos**: `src/utils/tests.c`, `include/utils/tests.h`

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

#### 8. **vfs** (Sistema de Archivos Virtual)
**Archivos**: `src/fs/ramfs.c`, `include/fs/vfs.h`

**Responsabilidad**: Gestión de archivos en memoria RAM (RamFS)

| Función         | Descripción                                        |
|-----------------|----------------------------------------------------|
| `ramfs_init()`  | Formatea disco virtual en RAM (1MB por defecto)    |
| `vfs_create()`  | Crea archivo vacío, asigna iNodo                   |
| `vfs_open()`    | Abre archivo, devuelve File Descriptor (FD)        |
| `vfs_read()`    | Lee datos desde un FD abierto                      |
| `vfs_write()`   | Escribe datos en un FD abierto                     |
| `vfs_close()`   | Cierra archivo, libera FD                          |
| `vfs_remove()`  | Elimina archivo, libera iNodo y datos en RAM       |
| `vfs_ls()`      | Lista archivos del directorio raíz                 |

**Estructuras de Datos**:

```c
/* iNodo: Representación de un archivo en el disco */
typedef struct {
    int id;                     // ID único (0-63)
    int type;                   // FS_FILE o FS_DIRECTORY
    int size;                   // Tamaño en bytes
    unsigned long data_ptr;     // Puntero a datos en RAM
    char name[FILE_NAME_LEN];   // Nombre del archivo (32 chars)
    int is_used;                // 1=ocupado, 0=libre
} inode_t;

/* Superbloque: Información maestra del disco */
typedef struct {
    unsigned long total_size;   // Tamaño total del RamDisk
    int free_inodes;            // iNodos disponibles
    unsigned long start_addr;   // Dirección física base (0x41000000)
    inode_t inodes[MAX_FILES];  // Tabla de iNodos (directorio raíz)
} superblock_t;

/* File Descriptor: Archivo abierto por un proceso */
typedef struct {
    inode_t *inode;             // Puntero al iNodo
    int position;               // Offset de lectura/escritura
    int flags;                  // Permisos (futuro)
} file_t;
```

**Arquitectura del Filesystem**:

```
┌─────────────────────────────────────────────────┐
│  RAMDISK (1MB en 0x41000000)                    │
│                                                 │
│  ┌─────────────────┐                            │
│  │  Superbloque    │  ← Metadatos globales      │
│  │  - free_inodes  │                            │
│  │  - total_size   │                            │
│  └─────────────────┘                            │
│                                                 │
│  ┌─────────────────┐                            │
│  │  iNodo[0]       │  ← readme.txt              │
│  │  - id: 0        │                            │
│  │  - size: 42     │                            │
│  │  - data_ptr: ───┼───┐                        │
│  └─────────────────┘   │                        │
│                        │                        │
│  ┌─────────────────┐   │                        │
│  │  iNodo[1]       │   │                        │
│  │  - id: 1        │   │                        │
│  │  - size: 128    │   │                        │
│  │  - data_ptr: ───┼───┼───┐                    │
│  └─────────────────┘   │   │                    │
│                        │   │                    │
│  ...                   │   │                    │
│                        ▼   ▼                    │
│  ┌──────────────────────────────────────────┐   │
│  │  Bloques de Datos (4KB cada uno)         │   │
│  │  [Datos readme.txt] [Datos config.sys]   │   │
│  └──────────────────────────────────────────┘   │
└─────────────────────────────────────────────────┘
```

**Características**:
- ✅ **Superbloque global** con gestión de 64 iNodos
- ✅ **Bloques estáticos**: Cada archivo tiene 4KB (1 página)
- ✅ **Directorio raíz plano** (sin subdirectorios)
- ✅ **Identity mapping**: RamDisk mapeado en 0x41000000
- ✅ **Tabla de File Descriptors**: Hasta 64 archivos abiertos simultáneamente
- ✅ **Validación de nombres**: Previene duplicados

**Limitaciones Actuales**:
- 📌 Tamaño fijo: 4KB por archivo (MAX_FILE_SIZE)
- 📌 Máximo 64 archivos (MAX_FILES)
- 📌 Sin subdirectorios (directorio raíz plano)
- 📌 Sin persistencia (datos volátiles)
- 📌 Sin permisos o usuarios

**Uso desde Kernel**:
```c
// En kernel_init()
ramfs_init(0x41000000, 1 * 1024 * 1024); // 1MB

// Crear archivo
vfs_create("test.txt");

// Escribir
int fd = vfs_open("test.txt");
vfs_write(fd, "Hola mundo", 10);
vfs_close(fd);

// Leer
fd = vfs_open("test.txt");
char buf[128];
int bytes = vfs_read(fd, buf, 127);
buf[bytes] = '\0';
kprintf("%s\n", buf);
vfs_close(fd);
```

---

#### 9. **kernel** (Inicialización)
**Archivo**: `src/kernel/kernel.c`

**Responsabilidad**: Punto de entrada e inicialización del sistema

```c
void kernel() {
    // 1. Inicializar sistema de memoria (MMU + Heap)
    init_memory_system();
    //    - Configura MMU y tablas de páginas
    //    - Inicializa heap de 64MB con kheap_init()
    
    // 2. Inicializar RamDisk (v0.6)
    ramfs_init(0x41000000, 1 * 1024 * 1024); // 1MB en 0x41000000
    //    - Formatea superbloque
    //    - Inicializa tabla de iNodos (64 archivos)
    //    - Crear archivos de prueba: readme.txt, config.sys
    
    // 3. Inicializar sistema de procesos
    init_process_system();
    //    - Configura proceso 0 (Kernel/IDLE)
    //    - Inicializa estructuras de PCB
    
    // 4. Inicializar timer (GIC + interrupciones)
    timer_init();
    
    // 5. Crear shell y procesos del sistema
    create_process(shell_task, 1, "Shell");
    
    // 6. Loop principal (IDLE)
    while(1) {
        asm volatile("wfi");  // Wait For Interrupt
    }
}
```

### Mejoras de cada version

**Refactorización v0.3 → v0.4**: El código originalmente monolítico fue reorganizado en módulos especializados en enero de 2026. La versión 0.4 estandariza completamente la documentación y comentarios del código, mejorando significativamente la estructura y mantenibilidad del proyecto.

**Mejoras v0.4 → v0.5**: La versión 0.5 (enero 2026) añade tres características avanzadas completamente documentadas: Round-Robin Scheduler con Quantum para multitarea preemptiva eficiente, Semáforos con Wait Queues eliminando busy-waiting, y Demand Paging para asignación de memoria bajo demanda mediante Page Fault handling.

**Mejoras v0.5 → v0.6**: La versión 0.6 (enero 2026) introduce un **sistema de archivos completo en memoria (RamFS)** con soporte de VFS, iNodos, File Descriptors y operaciones estándar (create/open/read/write/close/remove). El shell ahora incluye parser de argumentos y 5 nuevos comandos de filesystem (`touch`, `rm`, `ls`, `cat`, `write`). Se añaden syscalls preparatorias (`SYS_OPEN`, `SYS_READ`) y se reorganiza la estructura de includes (`fs/`, `shell/`, `utils/`).

[↑ Volver a Tabla de Contenidos](#-tabla-de-contenidos)

---

## Componentes Principales
[↑ Volver a Tabla de Contenidos](#-tabla-de-contenidos)

**Contenido de esta sección:**
- [1. Boot y Inicialización (`boot.S`)](#1-boot-e-inicialización-boots)
- [2. Núcleo del Kernel (Módulos)](#2-núcleo-del-kernel-módulos)
- [3. Conmutación de Contexto (`entry.S`)](#3-conmutación-de-contexto-entrys)
- [4. Planificador (Scheduler)](#4-planificador-scheduler)
- [5. Gestor de Interrupciones](#5-gestor-de-interrupciones)
- [6. Subsistema de Sincronización](#6-subsistema-de-sincronización)
- [7. Modo Usuario (User Mode) y Syscalls](#7-modo-usuario-user-mode-y-syscalls)
- [8. Protección de Memoria y Manejo de Fallos](#8-protección-de-memoria-y-manejo-de-fallos)
- [9. Demand Paging y Gestión de Page Faults (v0.6)](#9-demand-paging-y-gestión-de-page-faults-v06)
- [10. Sistema de Archivos (VFS/RamFS) (v0.6)](#10-sistema-de-archivos-vfsramfs-v06)

### 1. **Boot e Inicialización** (`boot.S`)
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
│  - Inicializa procesos      │
│  - timer_init()             │
│  - Crea procesos            │
│  - WFI loop                 │
└─────────────────────────────┘
```

**Archivo**: `src/boot.S`

Responsabilidades:
- Detección de CPU (multicore)
- Inicialización de la pila desde `link.ld`
- Salto a punto de entrada en C (`kernel()`)

[↑ Volver a Componentes Principales](#componentes-principales)

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

[↑ Volver a Componentes Principales](#componentes-principales)

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

[↑ Volver a Componentes Principales](#componentes-principales)

---

### 4. **Planificador (Scheduler)**

Ubicación: `src/kernel/scheduler.c::schedule()`

**Contenido de esta sección:**
- [Algoritmo Híbrido: Round-Robin con Quantum + Prioridades](#algoritmo-híbrido-round-robin-con-quantum--prioridades)
- [¿Por qué Round-Robin con Quantum?](#por-qué-round-robin-con-quantum)

#### Algoritmo Híbrido: Round-Robin con Quantum + Prioridades

El planificador de v0.6 combina **Round-Robin con Quantum** (preemption forzosa) y **Prioridades con Aging** (fairness):

```
═══════════════════════════════════════════════════════════════
ROUND-ROBIN CON QUANTUM (Preemptive Multitasking)
═══════════════════════════════════════════════════════════════
En cada timer_tick() (~104ms):
1. Decrementa quantum del proceso actual
2. Si quantum == 0:
   ├─ need_reschedule = 1
   └─ Al retornar de IRQ, se llama schedule()

Constante: DEFAULT_QUANTUM = 5 ticks (~520ms)

═══════════════════════════════════════════════════════════════
SCHEDULE() - ALGORITMO DE SELECCIÓN
═══════════════════════════════════════════════════════════════
ENTRADA: Lista de procesos [RUNNING, READY, BLOCKED]
SALIDA: Nuevo proceso a ejecutar

FASE 1: ENVEJECIMIENTO (Aging)
────────────────────────────────
Para cada proceso READY (excepto el actual):
    if (priority > 0) priority--
    
Efecto: Procesos esperando incrementan su prioridad gradualmente

FASE 2: SELECCIÓN
──────────────────
Encontrar proceso con MENOR priority entre READY y RUNNING
    0 = máxima prioridad (seleccionar primero)
    255 = mínima (seleccionar último)

Si no hay nadie → Kernel (PID 0) toma el control

FASE 3: PENALIZACIÓN Y QUANTUM
────────────────────────────────
Proceso seleccionado:
    priority += 2        // Penalizar para próxima ronda
    quantum = DEFAULT_QUANTUM  // Asignar tiempo de CPU

FASE 4: CONTEXT SWITCH
────────────────────────
if (prev != next):
    prev.state = READY
    next.state = RUNNING
    cpu_switch_to(prev, next)  // Cambiar contexto (entry.S)
```

#### ¿Por qué Round-Robin con Quantum?

**Problema sin quantum** → Un proceso egoísta puede monopolizar la CPU:
```
Proceso A: while(1) {}  // Nunca cede la CPU voluntariamente
Proceso B: Nunca ejecuta (sistema congelado)
```

**Solución con quantum** → Preemption automática cada 5 ticks:
```
Tick 0:  A ejecuta (quantum=5)
Tick 1:  A ejecuta (quantum=4)
...
Tick 5:  A ejecuta (quantum=0) → need_reschedule=1
         ↓
         schedule() forzoso → B ejecuta (quantum=5)
```

**Ventajas del Aging** → Sin aging, prioridades fijas causan starvation:
```
Proceso A: prioridad 5 (alta, se ejecuta siempre)
Proceso B: prioridad 200 (baja, nunca ejecuta)
↓
Con aging cada tick: B.priority = 199, 198, ... 5, 4, ...
Eventualmente B obtiene mayor prioridad que A y ejecuta.
```

**Resultado**: Sistema justo (fairness) y responsivo (responsive) con soporte para prioridades.

[↑ Volver a Componentes Principales](#componentes-principales)

---

### 5. **Gestor de Interrupciones**

**Contenido de esta sección:**
- [Tabla de Excepciones (`vectors.S`)](#tabla-de-excepciones-vectorss)
- [Flujo de Interrupción de Timer](#flujo-de-interrupción-de-timer)

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
│ 3. Escribir GICC_EOIR (End IRQ)      │
│ 4. Llamar schedule() para cambiar P  │
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

[↑ Volver a Componentes Principales](#componentes-principales)

---

### 6. **Subsistema de Sincronización**

**Contenido de esta sección:**
- [Spinlocks (Locks)](#spinlocks-locks)
- [Semáforos (Semaphore)](#semáforos-semaphore)

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
    volatile int count;      // Contador de recursos disponibles
    int lock;                // Spinlock protegiendo count
    struct pcb *wait_head;   // (v0.6) Inicio de Wait Queue
    struct pcb *wait_tail;   // (v0.6) Final de Wait Queue
};
```

**Operaciones clásicas (Dijkstra) con Wait Queues (v0.6)**:

```
P() [sem_wait]:              V() [sem_signal]:
────────────────────────     ─────────────────────────
disable_interrupts()         disable_interrupts()
spin_lock(&sem->lock)        spin_lock(&sem->lock)
                            
Si count > 0:                 count++
    count--                   
    spin_unlock()             Si wait_head != NULL:
    enable_interrupts()           Sacar proceso de Wait Queue
    return                        proceso->state = READY
Sino:                         spin_unlock()
    // NUEVO v0.6: Wait Queue  enable_interrupts()
    Agregar proceso actual     
    a Wait Queue (FIFO)       Efecto: Despierta primer proceso
    proceso->state = BLOCKED   en espera (FIFO)
    proceso->block_reason = 
        BLOCK_REASON_WAIT
    spin_unlock()
    enable_interrupts()
    schedule()  // Cede CPU
    
Efecto: Proceso duerme hasta 
que sem_signal() lo despierte
```

**MEJORA CRÍTICA: Protección contra Race Conditions del Timer**

A partir de enero 2026, los semáforos implementan **deshabilitación de interrupciones** durante las operaciones críticas:

```
¿Por qué deshabilitar interrupciones?
────────────────────────────────────
SIN disable_interrupts():
1. Proceso A ejecuta sem_wait()
2. Obtiene spinlock
3. Timer IRQ interrumpe → schedule()
4. Proceso B intenta sem_wait()
5. Spinlock ocupado → Deadlock

CON disable_interrupts():
1. Proceso A ejecuta sem_wait()
2. Deshabilita IRQs → Timer no puede interrumpir
3. Obtiene spinlock
4. Modifica semáforo
5. Libera spinlock
6. Habilita IRQs → Timer puede interrumpir
7. Si es necesario, llama schedule()
```

**Implementación**:
- `disable_interrupts()`: Activa bit I en DAIF (enmascarar IRQs)
- `enable_interrupts()`: Limpia bit I en DAIF (permitir IRQs)
- Ubicación: `src/entry.S`

**Beneficios**:
- ✅ **Sin deadlocks**: Timer no interrumpe operaciones críticas
- ✅ **Atomicidad**: Operaciones de semáforo son atómicas
- ✅ **Seguridad**: No hay race conditions con el scheduler
- ✅ **Simplicidad**: Patrón estándar en kernels reales

**DIFERENCIA CLAVE v0.4 vs v0.5**:

| Aspecto        | v0.4 (Busy-Wait)            | v0.5 (Wait Queue)               |
|----------------|-----------------------------|---------------------------------|
| **CPU**        | Desperdicia ciclos en bucle | Proceso BLOCKED, no consume CPU |
| **Eficiencia** | Baja (spinning)             | Alta (sleeping)                 |
| **Despertar**  | Detecta automáticamente     | Explícitamente despertado       |
| **Orden**      | No garantizado              | FIFO (justo)                    |
| **IRQ Safety** | Sin protección              | Interrupciones deshabilitadas   |

**Implementación Wait Queue (v0.6)**:

```
Wait Queue como lista enlazada FIFO:
────────────────────────────────────

semaphore:                   
  wait_head → [PCB_A] → [PCB_B] → [PCB_C] → NULL
  wait_tail  ────────────────────────────────┘

sem_wait() cuando count == 0:
  1. current_process->next = NULL
  2. Si wait_tail == NULL:  // Cola vacía
         wait_head = wait_tail = current_process
     Sino:                     // Agregar al final
         wait_tail->next = current_process
         wait_tail = current_process
  3. current_process->state = BLOCKED
  4. schedule()  // Cede CPU

sem_signal():
  1. count++
  2. Si wait_head != NULL:
         proceso = wait_head
         wait_head = wait_head->next
         Si wait_head == NULL:
             wait_tail = NULL
         proceso->state = READY
         proceso->next = NULL
```

**Ventajas Wait Queue vs Busy-Wait**:
- ✅ **0% CPU usage** cuando bloqueado (vs 100% en busy-wait)
- ✅ **FIFO ordering** garantiza fairness
- ✅ **Explicit wake-up** permite optimizaciones
- ✅ **Scalable** con muchos waiters

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

[↑ Volver a Componentes Principales](#componentes-principales)

---

### 7. **Modo Usuario (User Mode) y Syscalls**

**Contenido de esta sección:**
- [Visión General](#visión-general-1)
- [Arquitectura de Niveles de Excepción ARM64](#arquitectura-de-niveles-de-excepción-arm64)
- [Creación de Procesos de Usuario](#creación-de-procesos-de-usuario)
- [Transición Kernel → Usuario (entry.S)](#transición-kernel--usuario-entrys)
- [Sistema de Llamadas (Syscalls)](#sistema-de-llamadas-syscalls)
- [Flujo de Syscall Completo](#flujo-de-syscall-completo)
- [Ejemplo de Proceso de Usuario](#ejemplo-de-proceso-de-usuario)
- [Estructura pt_regs](#estructura-pt_regs)
- [Ventajas del Modelo User/Kernel](#ventajas-del-modelo-userkernel)
- [Limitaciones Actuales](#limitaciones-actuales)

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

**Syscalls Implementadas y Preparatorias**:

| Número | Nombre    | Descripción              | Argumentos                   | Estado         |
|--------|-----------|--------------------------|------------------------------|----------------|
| 0      | SYS_WRITE | Escribir en consola      | x19 = char *buffer           | ✅ Implementada |
| 1      | SYS_EXIT  | Terminar proceso         | x19 = int exit_code          | ✅ Implementada |
| 2      | SYS_OPEN  | Abrir archivo **(v0.6)** | x0 = path, x1 = mode         | 🔄 Stub        |
| 3      | SYS_READ  | Leer archivo **(v0.6)**  | x0 = fd, x1 = buf, x2 = size | 🔄 Stub        |

**Nota sobre Syscalls Preparatorias (v0.6)**: 
`SYS_OPEN` y `SYS_READ` están definidas y tienen handlers stub que imprimen mensajes de debug. 
No están conectadas al VFS aún - las operaciones de filesystem se realizan directamente desde 
el kernel por ahora. Preparadas para extensión futura a procesos de usuario con acceso a archivos.

**Convención de Llamada**:
- `x8`: Número de syscall
- `x19`: Primer argumento para SYS_WRITE y SYS_EXIT (implementadas)
- `x0`, `x1`, `x2`: Argumentos para SYS_OPEN y SYS_READ (stubs actuales)
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
    unsigned long x0;
    unsigned long x1;
    unsigned long x2;
    unsigned long x3;
    unsigned long x4;
    unsigned long x5;
    unsigned long x6;
    unsigned long x7;
    unsigned long x8;
    unsigned long x9;
    unsigned long x10;
    unsigned long x11;
    unsigned long x12;
    unsigned long x13;
    unsigned long x14;
    unsigned long x15;
    unsigned long x16;
    unsigned long x17;
    unsigned long x18;
    unsigned long x19;
    unsigned long x20;
    unsigned long x21;
    unsigned long x22;
    unsigned long x23;
    unsigned long x24;
    unsigned long x25;
    unsigned long x26;
    unsigned long x27;
    unsigned long x28;
    unsigned long x29;      // Frame Pointer (FP)
    unsigned long x30;      // Link Register (LR)
    unsigned long pstate;   // SPSR_EL1 (Estado del procesador)
    unsigned long pc;       // ELR_EL1 (Program Counter)
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

[↑ Volver a Componentes Principales](#componentes-principales)

---

### 8. **Protección de Memoria y Manejo de Fallos**

**Contenido de esta sección:**
- [Manejo de Excepciones Síncronas](#manejo-de-excepciones-síncronas)
- [Handler de Fallos](#handler-de-fallos)
- [Flujo de Manejo de Fallo](#flujo-de-manejo-de-fallo)
- [Exception Syndrome Register (ESR_EL1)](#exception-syndrome-register-esr_el1)
- [Proceso de Prueba: kamikaze_test](#proceso-de-prueba-kamikaze_test)
- [Tabla de Vectores y Protección](#tabla-de-vectores-y-protección)
- [Limitaciones de la Protección Actual](#limitaciones-de-la-protección-actual)
- [Registros de Depuración](#registros-de-depuración)

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
    │
    ▼
error_invalid (entry.S)
    │
    ├─ kernel_entry (asegura contexto guardado)
    └─ bl handle_fault  // Llama a C
    │
    ▼
handle_fault() (sys.c)
    │
    ├─ Imprime mensaje de diagnóstico
    ├─ Llama a exit() para terminar proceso
    └─ Nunca retorna
    │
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

[↑ Volver a Componentes Principales](#componentes-principales)

---

### 9. **Demand Paging y Gestión de Page Faults** (v0.6)

**Contenido de esta sección:**
- [Visión General](#visión-general-2)
- [Registros ARM64 Involucrados](#registros-arm64-involucrados)
- [Flujo del Demand Paging (8 Pasos)](#flujo-del-demand-paging-8-pasos)
- [Código Simplificado](#código-simplificado)
- [Integración con el Subsistema de Memoria](#integración-con-el-subsistema-de-memoria)
- [Esquema de Traducción Multilevel (L1/L2/L3)](#esquema-de-traducción-multilevel-l1l2l3)
- [Tipos de Descriptores](#tipos-de-descriptores)
- [Registros Clave del Sistema](#registros-clave-del-sistema)
- [Estructura de Bloques](#estructura-de-bloques)
- [Algoritmo de Asignación](#algoritmo-de-asignación)

#### Visión General

BareMetalM4 v0.6 implementa **Demand Paging** (paginación por demanda), una técnica de gestión de memoria donde las páginas físicas solo se asignan cuando son accedidas por primera vez, no al crear el proceso.

**Concepto clave**: Asignación perezosa (lazy allocation)
- Las páginas virtuales se marcan como "no presentes" en las tablas de páginas
- El primer acceso genera un **Page Fault** (excepción)
- El kernel asigna una página física y actualiza las tablas
- El acceso se reintenta automáticamente → ¡Éxito!
- El proceso nunca se entera (transparente)

**Beneficios del Demand Paging**:

| Ventaja                     | Descripción                                                     |
|-----------------------------|-----------------------------------------------------------------|
| ✅ **Optimización de RAM**   | Solo asigna memoria que realmente se usa                        |
| ✅ **Arranque más rápido**   | No pre-asigna todo el heap/stack al crear procesos              |
| ✅ **Sobrecompromiso**       | Puede prometer más memoria virtual que RAM física disponible    |
| ✅ **Estándar de industria** | Linux, Windows, macOS, BSD usan esta técnica                    |
| ✅ **Base para swap**        | Permite paginación a disco (no implementado en BareMetalM4 aún) |

#### Registros ARM64 Involucrados

El manejo de page faults requiere consultar registros de sistema especiales:

**FAR_EL1 (Fault Address Register)**:
```
Contiene la dirección virtual que causó el fallo

Lectura desde C:
    unsigned long far;
    asm volatile("mrs %0, far_el1" : "=r"(far));
    
Ejemplo: Si el proceso hace *ptr = 0x50000000 y falla,
         FAR_EL1 contendrá 0x50000000
```

**ESR_EL1 (Exception Syndrome Register)**:
```
Bits [31:26] - EC (Exception Class):
    0x24 = Data Abort desde EL1 (kernel)
    0x25 = Data Abort desde EL0 (usuario)
    0x15 = SVC instruction (syscall)
    0x20 = Instruction Abort
    ...otros códigos
    
Bits [24:0] - ISS (Instruction Specific Syndrome):
    Detalles adicionales (RW bit, tipo de acceso, etc.)

Lectura desde C:
    unsigned long esr;
    asm volatile("mrs %0, esr_el1" : "=r"(esr));
    unsigned long ec = esr >> 26;  // Extraer Exception Class
```

#### Flujo del Demand Paging (8 Pasos)

**Ubicación**: `src/kernel/sys.c:handle_fault()`

```
PROCESO USUARIO (EL0):
    int *ptr = (int*)0x50000000;
    *ptr = 42;  // ← Acceso a página no mapeada
    │
    ▼
╔══════════════════════════════════════════════════════════╗
║ 1. CPU detecta que 0x50000000 no está en TLB             ║
║    └─ Busca en tablas de páginas → No existe entrada     ║
╚══════════════════════════════════════════════════════════╝
    │
    ▼
╔══════════════════════════════════════════════════════════╗
║ 2. CPU genera Data Abort (Page Fault)                    ║
║    ├─ Guarda dirección culpable en FAR_EL1               ║
║    ├─ Guarda causa en ESR_EL1 (EC=0x25)                  ║
║    ├─ Cambia a EL1 (modo kernel)                         ║
║    └─ Salta a VBAR_EL1 + 0x400 (el0_sync)                ║
╚══════════════════════════════════════════════════════════╝
    │
    ▼
vectors.S: el0_sync
    │
    ├─ kernel_entry (guarda contexto)
    ├─ mrs x25, esr_el1
    ├─ lsr x24, x25, #26  // Extraer EC
    ├─ cmp x24, #0x15     // ¿Es SVC?
    ├─ b.ne error_invalid // No → Data Abort
    └─ b handle_fault     // Saltar a C
    │
    ▼
╔══════════════════════════════════════════════════════════╗
║ 3. handle_fault() lee FAR_EL1 y ESR_EL1                  ║
║    unsigned long far, esr;                               ║
║    asm volatile("mrs %0, far_el1" : "=r"(far));          ║
║    asm volatile("mrs %0, esr_el1" : "=r"(esr));          ║
║    unsigned long ec = esr >> 26;                         ║
╚══════════════════════════════════════════════════════════╝
    │
    ▼
╔══════════════════════════════════════════════════════════╗
║ 4. ¿Es Page Fault legítimo? (EC == 0x24 || EC == 0x25)   ║
║    └─ Si → Continuar con demand paging                   ║
║    └─ No → Segmentation Fault → exit()                   ║
╚══════════════════════════════════════════════════════════╝
    │ (Sí)
    ▼
╔══════════════════════════════════════════════════════════╗
║ 5. Asignar página física del PMM                         ║
║    unsigned long phys_page = get_free_page();            ║
║    └─ Retorna dirección física de 4KB libre (ej: 0x...)  ║
╚══════════════════════════════════════════════════════════╝
    │
    ▼
╔══════════════════════════════════════════════════════════╗
║ 6. Mapear en tablas de páginas L1/L2/L3                  ║
║    unsigned long virt_aligned = far & ~(PAGE_SIZE - 1);  ║
║    map_page(kernel_pgd, virt_aligned, phys_page, flags); ║
║    └─ Crea entrada en tabla L3: Virtual → Física         ║
╚══════════════════════════════════════════════════════════╝
    │
    ▼
╔══════════════════════════════════════════════════════════╗
║ 7. Invalidar TLB (Translation Lookaside Buffer)          ║
║    tlb_invalidate_all();                                 ║
║    └─ La TLB cachea traducciones. Hay que invalidarla    ║
║       para que la MMU vea la nueva entrada               ║
╚══════════════════════════════════════════════════════════╝
    │
    ▼
╔══════════════════════════════════════════════════════════╗
║ 8. RETORNAR al código de usuario                         ║
║    return; ← CPU automáticamente reintenta instrucción   ║
║    └─ Ahora la página está mapeada → Acceso exitoso      ║
╚══════════════════════════════════════════════════════════╝
    │
    ▼
PROCESO USUARIO (EL0):
    *ptr = 42;  // ← ¡Ahora funciona! La página existe
    // Continúa ejecución sin saber que hubo un page fault
```

#### Código Simplificado

**Ubicación**: `src/kernel/sys.c:handle_fault()`

```c
void handle_fault(void) {
    unsigned long far, esr;
    
    /* 1. Leer registros de excepción */
    asm volatile("mrs %0, far_el1" : "=r"(far));  // Qué dirección
    asm volatile("mrs %0, esr_el1" : "=r"(esr));  // Por qué falló
    
    /* 2. Extraer Exception Class */
    unsigned long ec = esr >> 26;
    
    /* 3. ¿Es Page Fault? (0x24=kernel, 0x25=usuario) */
    if (ec == 0x24 || ec == 0x25) {
        kprintf("\n[MMU] Page Fault en dir: 0x%x\n", far);
        
        /* 4. Asignar página física */
        unsigned long phys_page = get_free_page();
        if (phys_page) {
            /* 5. Alinear a 4KB */
            unsigned long virt_aligned = far & ~(PAGE_SIZE - 1);
            
            /* 6. Configurar permisos según nivel de privilegio */
            unsigned long flags = (ec == 0x25) ? 
                (MM_RW | MM_USER | MM_SH | (ATTR_NORMAL << 2)) :
                (MM_RW | MM_KERNEL | MM_SH | (ATTR_NORMAL << 2));
            
            /* 7. Mapear en tablas de páginas */
            map_page(kernel_pgd, virt_aligned, phys_page, flags);
            
            /* 8. Invalidar TLB */
            tlb_invalidate_all();
            
            /* 9. Retornar → CPU reintenta → Éxito */
            return;
        } else {
            kprintf("[PMM] Out of Memory!\n");
        }
    }
    
    /* No es page fault o no hay RAM → Matar proceso */
    kprintf("\n[CPU] Segmentation Fault en 0x%x\n", far);
    exit();
}
```

#### Integración con el Subsistema de Memoria

El demand paging conecta tres componentes clave:

```
┌─────────────────────────────────────────────────────────┐
│ 1. GESTOR DE MEMORIA FÍSICA (PMM)                       │
│    src/mm/pmm.c: get_free_page()                        │
│    └─ Busca página libre en bitmap                      │
│    └─ Marca como usada                                  │
│    └─ Retorna dirección física (ej: 0x10000000)         │
└────────────────┬────────────────────────────────────────┘
                 │
                 ▼
┌─────────────────────────────────────────────────────────┐
│ 2. GESTOR DE MEMORIA VIRTUAL (VMM)                      │
│    src/mm/vmm.c: map_page()                             │
│    └─ Navega tablas L1 → L2 → L3                        │
│    └─ Crea descriptores si faltan                       │
│    └─ Inserta entrada: Virtual → Física + Permisos      │
└────────────────┬────────────────────────────────────────┘
                 │
                 ▼
┌─────────────────────────────────────────────────────────┐
│ 3. TLB (Translation Lookaside Buffer)                   │
│    src/mm/mm.c: tlb_invalidate_all()                    │
│    └─ Invalida cachés de traducción                     │
│    └─ Fuerza a MMU a releer tablas de páginas           │
│    └─ Garantiza que los cambios se vean inmediatamente  │
└─────────────────────────────────────────────────────────┘
```

[↑ Volver a Demand Paging](#9-demand-paging-y-gestión-de-page-faults-v05)

### Arquitectura de la MMU ARM64

#### Esquema de Traducción Multilevel (L1/L2/L3)

BareMetalM4 v0.6 implementa un esquema completo de paginación de 3 niveles:

```
DIRECCION VIRTUAL (39 bits) - Configuracion con 4KB pages
│
├─ Bits [38:30] (9 bits) → Indice L1 (512 entradas) - Table Descriptor
│   └─ Cada entrada apunta a tabla L2
│
├─ Bits [29:21] (9 bits) → Indice L2 (512 entradas) - Table Descriptor
│   └─ Cada entrada apunta a tabla L3
│
├─ Bits [20:12] (9 bits) → Indice L3 (512 entradas) - Page Descriptor
│   └─ Cada entrada = página física de 4KB
│
└─ Bits [11:0] (12 bits) → Offset dentro de la pagina (4KB)

TAMAÑOS:
- Página L3: 4 KB (2^12)
- Entrada L3: 4 KB (pagina)
- Entrada L2: 2 MB (si fuera bloque, no se usa en el mapeo actual)
- Entrada L1: 1 GB (si fuera bloque, no se usa en el mapeo actual)
- Total espacio: 512 GB (512 entradas L1 × 1GB)

CONFIGURACION:
├─ T0SZ=25: 39 bits de espacio virtual (512 GB)
├─ TG0=4KB: Granularidad de pagina = 4096 bytes
└─ 3 niveles activos: L1 → L2 → L3
```

#### Tipos de Descriptores

```c
/* Descriptor de Tabla (L1 y L2) */
uint64_t table_descriptor =
    (direccion_fisica_siguiente_tabla) |
    0x3;  // Bits[1:0] = 0b11 (valido + tipo tabla)

/* Descriptor de Pagina (L3) */
uint64_t page_descriptor =
    (direccion_fisica_pagina) |
    (atributos << 2) |  // MAIR index
    (AP << 6) |         // Access Permissions
    (SH << 8) |         // Shareability
    (AF << 10) |        // Access Flag
    0x3;                // Bits[1:0] = 0b11 (valido + tipo pagina)
```

#### Registros Clave del Sistema

```
REGISTROS CLAVE:
├─ MAIR_EL1: Memory Attribute Indirection Register
│   └─ Define tipos de memoria (Device, Normal con/sin cache)
│
├─ TCR_EL1: Translation Control Register
│   ├─ T0SZ=25: 39 bits de direccion virtual
│   ├─ TG0=4KB: Granularidad de pagina
│   └─ Configura comportamiento de traducción
│
├─ TTBR0_EL1: Translation Table Base Register 0
│   └─ Apunta a tabla raiz (kernel_pgd)
│
├─ TTBR1_EL1: Translation Table Base Register 1
│   └─ Apunta a tabla raiz (kernel_pgd)
│
└─ SCTLR_EL1: System Control Register
    ├─ M bit: MMU Enable/Disable
    ├─ C bit: D-Cache Enable/Disable
    └─ I bit: I-Cache Enable/Disable
```

### Mapa de Memoria QEMU virt

| Rango Físico              | Tamaño | Tipo   | Contenido                           |
|---------------------------|--------|--------|-------------------------------------|
| `0x00000000 - 0x3FFFFFFF` | 1 GB   | Device | UART (0x09000000), GIC (0x08000000) |
| `0x40000000 - 0x47FFFFFF` | 128 MB | Normal | Código kernel, stack, datos         |

**Identity Mapping**: Direccion virtual = Direccion fisica (simplifica acceso inicial)

### Tabla de Páginas (L1/L2/L3) y Mapeo Inicial

```c
    // Mapeo inicial por páginas de 4KB (identity mapping)
    // RAM QEMU virt: 128MB desde 0x40000000 hasta 0x47FFFFFF
    for (addr = 0x40000000; addr < 0x48000000; addr += 0x1000) {
        map_page(kernel_pgd, addr, addr, FLAGS_NORMAL);
    }
```

**Formato de descriptor de página**:
```
Bits [47:12] - Dirección física base (4 KB alineado)
Bits [11:2]  - Atributos:
  ├─ Bit 0: Válido (1)
  ├─ Bit 1: Tipo (1 = pagina)
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
mem_init() - Secuencia de activacion (modelo actual):
│
├─ 1. Limpiar tabla raiz (kernel_pgd) y tablas auxiliares
│
├─ 2. Mapear memoria con paginas de 4KB:
│   ├─ Perifericos (UART, GIC) como Device
│   └─ RAM completa (0x40000000-0x48000000) como Normal
│
├─ 3. Configurar registros:
│   ├─ MAIR_EL1 ← Tipos de memoria
│   ├─ TCR_EL1 ← T0SZ=25 (39 bits), TG0=4KB
│   └─ TTBR0/1_EL1 ← kernel_pgd
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
└─ Sistema ahora ejecuta en memoria virtual (identity mapping)
```

### Translation Lookaside Buffer (TLB)

El **TLB** es una caché que almacena traducciones recientes:

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
   - Retorna NULL (no hay expansión dinámica del heap)
4. Retorna puntero al área de datos (después del header)

**kfree(ptr)**:
1. Obtiene el header del bloque desde el puntero
2. Marca el bloque como libre
3. Intenta fusionar (coalesce) con bloques adyacentes libres (hacia adelante)
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
| **Coalescing**      | Parcial (solo hacia adelante)                        |
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

### Limitaciones y Mejoras Futuras

| Limitación Actual             | Estado en v0.6                       | Mejora Futura                         |
|-------------------------------|--------------------------------------|---------------------------------------|
| **Protección entre procesos** | ❌ Todos comparten espacio virtual    | Tablas de páginas por proceso (TTBR0) |
| **Paginación dinámica**       | ✅ **Implementado con Demand Paging** | Completado en v0.6                    |
| **Swapping a disco**          | ❌ Sin soporte de disco               | Sistema de archivos + swap partition  |
| **Copy-on-Write (COW)**       | ❌ No implementado                    | Para fork() eficiente                 |
| **Estadísticas de memoria**   | ❌ Sin tracking                       | Contadores de uso RAM/páginas         |
| **Validación de rangos**      | ❌ No verifica límites heap/stack     | Implementar límites por proceso       |
| **Asignador first-fit**       | ⚠️ Funcional pero no óptimo          | Migrar a best-fit o slab allocator    |
| **Tablas L1/L2/L3**           | ✅ **Implementado multinivel**        | Completado en v0.6                    |
| **Gestión física separada**   | ✅ **PMM con bitmap implementado**    | Completado en v0.6                    |

**Progreso v0.4 → v0.5**:
- ✅ Paginación multilevel (L1/L2/L3)
- ✅ Physical Memory Manager (PMM)
- ✅ Virtual Memory Manager (VMM)
- ✅ Demand Paging con page fault handler
- ✅ Asignación perezosa de memoria

[↑ Volver a Componentes Principales](#componentes-principales)

[↑ Volver a Tabla de Contenidos](#-tabla-de-contenidos)

---

### 10. **Sistema de Archivos (VFS/RamFS)** (v0.6)

**Contenido de esta sección:**
- [Arquitectura del Filesystem](#arquitectura-del-filesystem-1)
- [Flujo de Operaciones de Archivo](#flujo-de-operaciones-de-archivo)
- [Inicialización del RamDisk](#inicialización-del-ramdisk-1)

#### Arquitectura del Filesystem

BareMetalM4 v0.6 implementa un **sistema de archivos virtual en memoria** (RamFS) que permite crear, leer, escribir y eliminar archivos directamente en RAM.

**Componentes Principales**:

```
┌──────────────────────────────────────────────────────┐
│                  KERNEL SPACE                        │
│                                                      │
│  ┌───────────────┐         ┌──────────────────┐      │
│  │  Shell/Apps   │────────▶│  VFS API         │      │
│  │  - touch      │         │  - vfs_create()  │      │
│  │  - cat        │         │  - vfs_open()    │      │
│  │  - write      │         │  - vfs_read()    │      │
│  └───────────────┘         │  - vfs_write()   │      │
│                            │  - vfs_close()   │      │
│                            └────────┬─────────┘      │
│                                     │                │
│                            ┌────────▼─────────┐      │
│                            │  RamFS Driver    │      │
│                            │  - Superbloque   │      │
│                            │  - iNodo mgmt    │      │
│                            │  - FD table      │      │
│                            └────────┬─────────┘      │
└─────────────────────────────────────┼────────────────┘
                                      │
                            ┌─────────▼─────────┐
                            │  RAMDISK          │
                            │  0x41000000       │
                            │  (1MB en RAM)     │
                            └───────────────────┘
```

#### Flujo de Operaciones de Archivo

**Creación de Archivo**:

```
Usuario: touch archivo.txt
    │
    ├─▶ shell.c: detecta comando "touch"
    │
    ├─▶ vfs_create("archivo.txt")
    │       │
    │       ├─ Verificar si existe (buscar en inodes[])
    │       ├─ Buscar iNodo libre (is_used == 0)
    │       ├─ Asignar iNodo:
    │       │   - id = índice
    │       │   - name = "archivo.txt"
    │       │   - size = 0 (vacío)
    │       │   - data_ptr = start_addr + (id * 4KB)
    │       │   - is_used = 1
    │       └─ Decrementar free_inodes
    │
    └─▶ Retorna 0 (éxito) o -1 (error)
```

**Lectura de Archivo**:

```
Usuario: cat archivo.txt
    │
    ├─▶ shell.c: detecta comando "cat"
    │
    ├─▶ fd = vfs_open("archivo.txt")
    │       │
    │       ├─ Buscar iNodo por nombre
    │       ├─ Buscar slot libre en fd_table[]
    │       ├─ Asignar FD:
    │       │   - inode = puntero al iNodo
    │       │   - position = 0 (inicio)
    │       └─ Retorna índice (FD)
    │
    ├─▶ bytes = vfs_read(fd, buf, 127)
    │       │
    │       ├─ Obtener iNodo desde fd_table[fd]
    │       ├─ Calcular bytes disponibles (size - position)
    │       ├─ Copiar desde (data_ptr + position) a buf
    │       ├─ Actualizar position += bytes_leídos
    │       └─ Retorna bytes leídos
    │
    ├─▶ kprintf("%s", buf)
    │
    └─▶ vfs_close(fd)
            │
            └─ Limpiar fd_table[fd] (inode = NULL)
```

**Escritura de Archivo**:

```
Usuario: write archivo.txt
    │
    ├─▶ shell.c: detecta comando "write"
    │
    ├─▶ fd = vfs_open("archivo.txt")
    │
    ├─▶ bytes = vfs_write(fd, "texto...", len)
    │       │
    │       ├─ Obtener iNodo desde fd_table[fd]
    │       ├─ Verificar espacio (MAX_FILE_SIZE - position)
    │       ├─ Copiar desde buf a (data_ptr + position)
    │       ├─ Actualizar position += bytes_escritos
    │       ├─ Actualizar inode->size si position > size
    │       └─ Retorna bytes escritos
    │
    └─▶ vfs_close(fd)
```

#### Inicialización del RamDisk

**Ubicación**: `src/kernel/kernel.c`

```c
void kernel() {
    /* 1. Inicializar sistema de memoria (MMU + Heap) */
    init_memory_system();
    
    /* 2. Inicializar RamDisk en región segura de RAM */
    ramfs_init(0x41000000, 1 * 1024 * 1024); // 1MB
    //    - Formatea superbloque
    //    - Marca todos los iNodos como libres (is_used = 0)
    //    - Asigna data_ptr a cada iNodo (4KB estático)
    //    - Inicializa fd_table[] (todos NULL)
    
    /* 3. Crear archivos de prueba (opcional) */
    vfs_create("readme.txt");
    vfs_create("config.sys");
    
    /* ... resto de inicialización ... */
}
```

**Asignación de Memoria**:

```
Mapa de Memoria del RamDisk:
────────────────────────────────────────
0x41000000 - 0x41001000  →  iNodo 0 data (4KB)
0x41001000 - 0x41002000  →  iNodo 1 data (4KB)
0x41002000 - 0x41003000  →  iNodo 2 data (4KB)
...
0x4103F000 - 0x41040000  →  iNodo 63 data (4KB)

Total: 64 archivos × 4KB = 256KB de datos
      + Superbloque y metadatos
      = ~1MB total
```

**Ventajas del Diseño**:
- ✅ **Simplicidad**: Asignación estática, sin fragmentación
- ✅ **Velocidad**: Acceso directo a RAM, sin I/O físico
- ✅ **Determinístico**: Tamaño predecible, sin allocaciones dinámicas
- ✅ **Educativo**: Fácil de entender y depurar

**Limitaciones y Mejoras Futuras**:
- 📌 **Tamaño fijo**: 4KB por archivo (considerar bloques dinámicos)
- 📌 **Sin subdirectorios**: Implementar árbol de directorios
- 📌 **Sin persistencia**: Añadir serialización a disco virtual
- 📌 **Syscalls stub**: Conectar SYS_OPEN/SYS_READ al VFS
- 📌 **Sin permisos**: Implementar usuarios y permisos (rwx)

[↑ Volver a Componentes Principales](#componentes-principales)

[↑ Volver a Tabla de Contenidos](#-tabla-de-contenidos)

---

## Subsistema de Planificación
[↑ Volver a Tabla de Contenidos](#-tabla-de-contenidos)

**Contenido de esta sección:**
- [Estados de Proceso](#estados-de-proceso)
- [Estructura PCB Completa](#estructura-pcb-completa)
- [Algoritmo de Scheduling](#algoritmo-de-scheduling)
- [Mecanismo de Quantum (v0.6)](#mecanismo-de-quantum-v06)
- [Tabla de Prioridades](#tabla-de-prioridades)
- [Sistema de Despertar (Wake-up)](#sistema-de-despertar-wake-up)
- [Sleep: Dormir un Proceso por Tiempo Determinado](#sleep-dormir-un-proceso-por-tiempo-determinado)

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

**Campos adicionales en v0.6**:
- `quantum`: Ticks restantes antes de preemption (Round-Robin)
- `next`: Puntero para formar listas enlazadas en Wait Queues de semáforos
- `wake_up_time`: Timestamp para sleep() sin busy-wait

### Algoritmo de Scheduling

**Ubicación**: `src/kernel/scheduler.c::schedule()`

El scheduler implementa un **algoritmo híbrido** que combina:
1. **Prioridades con Aging** (prevenir inanición)
2. **Round-Robin con Quantum** (v0.6) - Preemption basada en tiempo

```
ALGORITMO: Prioridad + Aging + Round-Robin con Quantum (v0.6)
═════════════════════════════════════════════════════════════

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

FASE 3: PENALIZACIÓN + ASIGNACIÓN DE QUANTUM (v0.6)
────────────────────────────────────────────────────
Si next->priority < 10:
    next->priority += 2   // Baja prioridad del elegido

// NUEVO en v0.6: Asignar quantum para Round-Robin
next->quantum = DEFAULT_QUANTUM  // 5 ticks
// El quantum se decrementa en timer_tick() cada interrupción
// Cuando quantum llega a 0, se marca need_reschedule = 1

Efecto: Proceso ejecuta máximo 5 ticks antes de preemption obligatoria

FASE 4: CAMBIO DE CONTEXTO
───────────────────────────
Si prev != next:
    prev->state = READY (si estaba RUNNING)
    next->state = RUNNING
    current_process = next
    cpu_switch_to(prev, next)   // Assembly context switch
```

### Mecanismo de Quantum (v0.6)

El **quantum** es el tiempo máximo que un proceso puede ejecutar antes de ser forzosamente desalojado:

```
TIMER_TICK (cada ~104ms):
┌────────────────────────────────────────┐
│ 1. Incrementa sys_timer_count          │
│ 2. Despierta procesos BLOCKED por sleep│
│ 3. Decrementa quantum proceso actual   │
│ 4. Si quantum == 0:                    │
│    ├─ need_reschedule = 1              │
│    └─ schedule() será llamado          │
└────────────────────────────────────────┘
        │
        ▼
KERNEL_EXIT (al retornar de IRQ):
┌────────────────────────────────────────┐
│ 1. Verifica need_reschedule            │
│ 2. Si == 1:                            │
│    └─ Llama schedule()                 │
│       └─ Elige nuevo proceso (puede    │
│          ser el mismo si tiene mayor   │
│          prioridad)                    │
└────────────────────────────────────────┘
```

**Ventajas del Quantum**:
- ✅ **Fairness**: Todos los procesos obtienen tiempo de CPU
- ✅ **Preemption automática**: No depende de yield() voluntario
- ✅ **Responsiveness**: Sistema responde rápido a eventos
- ✅ **Previene monopolio**: Procesos no pueden acaparar CPU indefinidamente

**Constantes**:
```c
#define DEFAULT_QUANTUM 5  // Ticks antes de preemption
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

En cada tick del timer (~104ms):

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

#### Comparación: delay() vs. sleep()

| Aspecto            | delay()                  | sleep()                      |
|--------------------|--------------------------|------------------------------|
| **Tipo**           | Busy-wait                | Bloqueo con timer            |
| **CPU**            | Consume (bucle infinito) | Libera (para otros procesos) |
| **Otros procesos** | NO pueden ejecutar       | PUEDEN ejecutar              |
| **Precisión**      | Exacta (ciclos de CPU)   | Aproximada (~104ms)          |
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
    
    Timer interrupt cada ~104ms incrementa sys_timer_count
    
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

- **Cada timer interrupt**: ~104 ms
- **sleep(10)**: ~1 segundo
- **sleep(1)**: ~100 milisegundos
- **Precisión**: ±104ms (depende de cuando se chequea)

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

- **No es exacto**: ±104ms de precisión
- **Overhead**: Chequeo en cada interrupt (~50 ciclos)
- **Proceso duerme más**: Espera a ser seleccionado nuevamente
- **Requiere interrupts**: Sin enable_interrupts(), nunca despierta

[↑ Volver a Tabla de Contenidos](#-tabla-de-contenidos)

---

## Decisiones de Diseño
[↑ Volver a Tabla de Contenidos](#-tabla-de-contenidos)

**Contenido de esta sección:**
- [✅ Decisiones Acertadas](#-decisiones-acertadas)
- [⚠️ Limitaciones Intencionales](#-limitaciones-intencionales)

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
| **UART polling**        | Implementación simple         | Interrupts + buffers (actual) |

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
| 0x40000000 - 0x40FFFFFF | Kernel (16 MB)    |
| 0x09000000              | UART0             |
| 0x08000000              | GIC Distribuidor  |
| 0x08010000              | GIC CPU Interface |

[↑ Volver a Tabla de Contenidos](#-tabla-de-contenidos)

---

## Limitaciones y Mejoras Futuras
[↑ Volver a Tabla de Contenidos](#-tabla-de-contenidos)

**Contenido de esta sección:**
- [✅ Características Implementadas (v0.6)](#-características-implementadas-v06)
- [Fase 1: Mejoras Educativas (Siguientes)](#fase-1-mejoras-educativas-siguientes)
- [Fase 2: Características Reales](#fase-2-características-reales)
- [Fase 3: Optimizaciones](#fase-3-optimizaciones)
- [Diagrama de Flujo: Syscall desde User Mode](#diagrama-de-flujo-syscall-desde-user-mode)

### ✅ Características Implementadas (v0.6)
- [x] Arquitectura modular con separación de subsistemas
- [x] Asignación dinámica de memoria (kmalloc/kfree)
- [x] **Planificador Round-Robin con Quantum** - **✅ IMPLEMENTADO en v0.6**
- [x] **Semáforos con Wait Queues (sin busy-wait)** - **✅ IMPLEMENTADO en v0.6**
- [x] **Demand Paging (asignación bajo demanda)** - **✅ IMPLEMENTADO en v0.6**
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
- [x] ~~Implementar wait queues en semáforos~~ - **✅ IMPLEMENTADO en v0.6**
- [x] ~~Planificador Round-Robin con quantum~~ - **✅ IMPLEMENTADO en v0.6**
- [x] ~~Demand Paging (Page Fault handling)~~ - **✅ IMPLEMENTADO en v0.6**
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

**Contenido de esta sección:**
- [Registros ARM64 Importantes](#registros-arm64-importantes)
- [Exception Syndrome Register (ESR_EL1) - Valores Clave](#exception-syndrome-register-esr_el1---valores-clave)

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
│           ┌──────────▼──┐    ┌───────▼─┐    │                │
│           │   GIC Dist  │    │ Timer   │    │                │
│           │ (0x08000000)│    │ (ARM64) │    │                │
│           └─────────────┘    └────┬────┘    │                │
│                                   │         │                │
│           ┌───────────┬───────────┘         │                │
│           │           │                     │                │
│   ┌───────▼────┐  ┌──▼──────────┐   ┌───────▼───────┐        │
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

**Contenido de esta sección:**
- [Registros ARM64 Importantes](#registros-arm64-importantes)
- [Exception Syndrome Register (ESR_EL1) - Valores Clave](#exception-syndrome-register-esr_el1---valores-clave)
- [Publicaciones y Referencias](#publicaciones-y-referencias)

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
- **TTBR0_EL1**: Translation Table Base Register 0 (tabla raiz, kernel_pgd)
- **TTBR1_EL1**: Translation Table Base Register 1 (tabla raiz, kernel_pgd)
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
**Refactorización**: Estructura modular, sistema de memoria dinámica, documentación completa y estandarización de código implementados (enero 2026)

---

## Historial de Cambios

**Contenido de esta sección:**
- [v0.6 - Enero 26, 2026](#v06---enero-26-2026)
- [v0.5.1 - Enero 24, 2026](#v051---enero-24-2026)
- [v0.5 - Enero 25, 2026](#v05---enero-25-2026)
- [v0.4 - Enero 21, 2026](#v04---enero-21-2026)
- [v0.3.5 - Enero 20, 2026](#v035---enero-20-2026)
- [v0.3 - Enero 2026](#v03---enero-2026)
- [v0.2 - 2025](#v02---2025)
- [v0.1 - 2025](#v01---2025)

### v0.6 - Enero 26, 2026

**Commits**: 
- `2014972` - Preparación syscalls (SYS_OPEN, SYS_READ)
- `3c111e0` - Reorganización de directorios (shell/, utils/, fs/)
- `bb63405` - Implementación básica de archivos (VFS + RamFS)
- `6bb6a5c` - Sistema completo + parser de comandos en shell

- ✅ **Sistema de Archivos Virtual (VFS) con RamFS**
  - Implementación completa de filesystem en memoria RAM (1MB en 0x41000000)
  - Superbloque global con gestión de hasta 64 iNodos
  - File Descriptors para archivos abiertos (hasta 64 simultáneos)
  - Directorio raíz plano con nombres de archivo de 32 caracteres
  - Bloques estáticos de 4KB por archivo (MAX_FILE_SIZE)
  - Identity mapping del RamDisk en región segura de memoria

- ✅ **Operaciones de Filesystem**
  - `vfs_create(name)` - Crear archivos vacíos, asignar iNodo
  - `vfs_open(name)` - Abrir archivo y obtener File Descriptor
  - `vfs_read(fd, buf, count)` - Leer desde FD con offset
  - `vfs_write(fd, buf, count)` - Escribir a FD con offset
  - `vfs_close(fd)` - Cerrar archivo y liberar FD
  - `vfs_remove(name)` - Eliminar archivo y liberar recursos
  - `vfs_ls()` - Listar directorio raíz con ID, tamaño y nombre

- ✅ **Comandos de Shell para Filesystem**
  - `touch [archivo]` - Crear archivo vacío en RamFS
  - `rm [archivo]` - Eliminar archivo del disco virtual
  - `ls` - Listar archivos con ID, tamaño (bytes) y nombre
  - `cat [archivo]` - Leer y mostrar contenido completo
  - `write [archivo]` - Escribir texto predefinido en archivo
  - Parser de comandos mejorado con soporte de argumentos

- ✅ **Tests Modulares con Argumentos**
  - Comando `test [módulo]` acepta parámetros específicos:
    - `test all` - Bateria global de tests del sistema (memoria + scheduler)
    - `test rr` - Test de Round-Robin con Quantum (v0.6)
    - `test sem` - Test de Semáforos con Wait Queues (v0.6)
    - `test pf` - Test de Demand Paging con Page Faults (v0.6)
    - `test demo` - Demo integrada (heap + RamFS en dos procesos)
  - Mensajes de error para módulos no reconocidos

- ✅ **Syscalls Preparatorias para Filesystem**
  - `SYS_OPEN` (número 2) - Handler stub con debug output
  - `SYS_READ` (número 3) - Handler stub con debug output
  - Preparadas para conexión futura con VFS desde procesos de usuario
  - Actualmente las operaciones de filesystem se realizan desde kernel

- ✅ **Reorganización de Estructura del Proyecto**
  - Nuevo directorio `fs/` para código de sistema de archivos
  - Headers `include/fs/vfs.h` con definiciones de iNodos y superbloque
  - Headers movidos a subdirectorios especializados:
    - `include/shell/shell.h` - Interfaz del shell
    - `include/utils/kutils.h` - Utilidades generales
    - `include/utils/tests.h` - Funciones de prueba
  - Actualización de includes en todos los archivos afectados
  - Mejor separación de responsabilidades y modularidad

- ✅ **Nuevas Utilidades del Kernel**
  - `k_strlen(str)` - Calcular longitud de cadena (src/utils/kutils.c)
  - Necesaria para operaciones de filesystem y validación de nombres
  - Implementación sin libc, compatible con bare-metal

- ✅ **Mejoras en Documentación (ARCHITECTURE.md)**
  - Nueva sección 8 (vfs) en "Módulos del Kernel"
  - Nueva sección 10 en "Componentes Principales"
  - Diagramas de arquitectura del filesystem con componentes
  - Diagramas de flujo para operaciones de archivo (create, open, read, write)
  - Tabla actualizada de comandos del shell con ejemplos de uso
  - Tabla de syscalls actualizada con SYS_OPEN y SYS_READ
  - Actualización de totales de líneas de código (~+300 líneas)

**Archivos Nuevos**: 
- `include/fs/vfs.h` (58 líneas) - Definiciones VFS
- `src/fs/ramfs.c` (244 líneas) - Implementación RamFS

**Archivos Modificados**:
- `include/kernel/sys.h` - Añadidas SYS_OPEN (2) y SYS_READ (3)
- `include/utils/kutils.h` - Añadido prototipo k_strlen()
- `src/kernel/sys.c` - Handlers stub para nuevas syscalls
- `src/kernel/kernel.c` - Inicialización de RamDisk con ramfs_init()
- `src/shell/shell.c` - Parser de argumentos y 5 comandos de filesystem
- `src/utils/kutils.c` - Implementación de k_strlen()
- `Makefile` - Añadido src/fs/ramfs.c a compilación
- `docs/ARCHITECTURE.md` - 9 secciones actualizadas con v0.6

**Incremento de Código**:
- v0.5 → v0.6: +~300 líneas (~10% más funcionalidad)
- Total v0.6: ~3,500 líneas de código (sin comentarios)
- Total bruto: ~4,600 líneas (incluyendo documentación)

**Características del Sistema de Archivos**:
- Dirección base: 0x41000000 (mapeado en memoria virtual)
- Tamaño total: 1MB (1,048,576 bytes)
- Capacidad: 64 archivos de 4KB cada uno
- Archivos de prueba: readme.txt, config.sys (creados en boot)
- Gestión de iNodos con verificación de nombres duplicados
- File Descriptors con offset de lectura/escritura

**Limitaciones Conocidas**:
- 📌 Tamaño fijo de archivo: 4KB máximo por archivo
- 📌 Sin subdirectorios: directorio raíz plano
- 📌 Sin persistencia: datos volátiles en RAM
- 📌 Syscalls preparatorias: SYS_OPEN/SYS_READ no conectadas al VFS
- 📌 Sin permisos: no hay control de acceso ni usuarios
- 📌 Sin fragmentación: bloques estáticos asignados al inicio

**Ventajas del Diseño**:
- ✅ Simplicidad: asignación estática, sin fragmentación
- ✅ Velocidad: acceso directo a RAM, sin I/O físico
- ✅ Determinístico: tamaño predecible por archivo
- ✅ Educativo: fácil de entender y depurar
- ✅ Eficiente: operaciones O(n) con n=64 máximo

### v0.5.1 - Enero 24, 2026
- ✅ **Mejora en Sincronización: Protección contra Race Conditions**
  - Implementación de `disable_interrupts()` en `src/entry.S`
  - Protección de secciones críticas en semáforos contra interrupciones del timer
  - `sem_wait()` y `sem_signal()` ahora deshabilitan IRQs durante operaciones críticas
  - Prevención de deadlocks causados por el timer interrumpiendo spinlocks
  - Patrón estándar: disable_interrupts() → spinlock → operación → unlock → enable_interrupts()
  - Mejora en la atomicidad y seguridad de las operaciones de sincronización

- ✅ **Mejora en Limpieza de Procesos Zombie**
  - Limpieza exhaustiva de la estructura PCB en `free_zombie()` (`src/kernel/process.c`)
  - Liberación explícita de memoria dinámica de la pila con `kfree()`
  - Reinicio de todos los campos del PCB: pid, priority, cpu_time, wake_up_time, quantum
  - Limpieza del nombre del proceso con `memset()` para evitar datos residuales
  - Marcado como UNUSED para permitir reutilización por `create_process()`
  - Nota documentada para futura extensión: cierre de archivos abiertos
  - Prevención de memory leaks y comportamiento indefinido por datos residuales

- ✅ **Mejoras en Documentación**
  - Actualización de ARCHITECTURE.md con explicación de disable_interrupts()
  - Documentación del patrón de protección contra race conditions del timer
  - Explicación de los cambios en la limpieza de procesos zombie
  - Referencias a ubicaciones específicas de código (process.c:245, semaphore.c:112, entry.S)

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
