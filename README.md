# BareMetalM4 v0.6.1 - Kernel Educativo ARM64

**BareMetalM4 v0.6.1** es un kernel *bare-metal* educativo para **ARM64 (AArch64)** diseñado para aprender los fundamentos de sistemas operativos ejecutándose en QEMU.

## Características Principales

- **Arquitectura:** ARM64 (ARMv8-A) Cortex-A72
- **Plataforma:** QEMU `virt` machine
- **Multitarea Expropiativa:** Planificador Round-Robin con Quantum + Prioridades + Aging (hasta 64 procesos)
- **Gestión de Memoria Avanzada:** 
  - MMU con memoria virtual multinivel (L1/L2/L3)
  - **Demand Paging** (asignación bajo demanda mediante Page Faults)
  - Asignador dinámico (`kmalloc`/`kfree`) con heap de 64MB
  - Physical Memory Manager (PMM) con bitmap
- **Sistema de Archivos:** **RamFS** con VFS (Virtual File System)
  - Soporte de iNodos, File Descriptors y operaciones estándar
  - Comandos: `touch`, `rm`, `ls`, `cat`, `write`
- **Sincronización Avanzada:** 
  - Spinlocks (LDXR/STXR) con operaciones atómicas
  - **Semáforos con Wait Queues** (sin busy-wait)
- **Interrupciones:** GIC v2 + Timer de sistema con cambio de contexto automático
- **Shell Interactivo:** 11 comandos con parser de argumentos
- **Sistema de Tests Modular:** Validación de Round-Robin, Semáforos y Demand Paging
- **Syscalls:** Interfaz para modo usuario (SYS_WRITE, SYS_EXIT, stubs SYS_OPEN/READ)
- **Sin dependencias:** Sin librerías estándar (`-ffreestanding -nostdlib`)
- **Compilación limpia:** Cero warnings con `-Wall -Wextra`

## Estructura Modular

El kernel está organizado en módulos especializados:

```
src/
├── kernel/         # Núcleo del sistema
│   ├── kernel.c    # Inicialización del sistema
│   ├── process.c   # Gestión de procesos (PCB, quantum)
│   ├── scheduler.c # Round-Robin + Quantum + Aging
│   └── sys.c       # Syscalls y Demand Paging handler
├── drivers/        # Controladores hardware
│   ├── io.c        # Driver UART + kprintf
│   └── timer.c     # GIC v2 + Timer (interrupciones)
├── mm/             # Gestión de memoria avanzada
│   ├── mm.c        # MMU (tablas multinivel L1/L2/L3)
│   ├── malloc.c    # Asignador dinámico (64MB heap)
│   ├── pmm.c       # Physical Memory Manager (bitmap)
│   └── vmm.c       # Virtual Memory Manager (Demand Paging)
├── fs/             # Sistema de archivos
│   └── ramfs.c     # RamFS: VFS, iNodos, File Descriptors
├── shell/          # Interfaz de usuario
│   └── shell.c     # Shell + 11 comandos + parser
├── utils/          # Utilidades
│   ├── kutils.c    # panic, strings (k_strlen, k_strcmp)
│   ├── tests.c     # Tests modulares (RR, Sem, PF)
│   └── demos.c     # Código educativo de referencia
└── semaphore.c     # Semáforos con Wait Queues
```

## Documentación

| Documento | Descripción |
|-----------|-------------|
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | Arquitectura completa del kernel (~3,300 líneas) |
| [docs/TEMARIO.md](docs/TEMARIO.md) | Mapeo Temario universitario → Código fuente |
| [docs/GUIA_ENSAMBLADOR_ARM64.md](docs/GUIA_ENSAMBLADOR_ARM64.md) | Tutorial de ensamblador ARM64 con ejemplos del proyecto |

## Requisitos

- **QEMU:** `qemu-system-aarch64`
- **Toolchain AArch64:** `aarch64-elf-gcc` (o `aarch64-linux-gnu-gcc`)
- **Make**

### Instalación macOS (Apple Silicon)
```bash
brew install qemu
brew install --cask gcc-aarch64-embedded
```

### Instalación Linux
```bash
sudo apt install gcc-aarch64-linux-gnu qemu-system-arm make
```

*Nota: Ajusta `CC` y `LD` en el Makefile si usas `aarch64-linux-gnu-gcc`.*

## Compilación y Ejecución

```bash
# Compilar
make

# Ejecutar en QEMU
make run

# Limpiar
make clean
```

**Salir de QEMU:** `Ctrl+A` luego `x`

## Comandos del Shell

Una vez ejecutado, el sistema arranca un shell interactivo:

### Gestión del Sistema
| Comando | Descripción |
|---------|-------------|
| `help` | Muestra todos los comandos disponibles |
| `ps` | Lista procesos (PID, prioridad, estado, tiempo de CPU, nombre) |
| `clear` | Limpia la pantalla (códigos ANSI) |
| `panic` | Provoca un kernel panic (demo) |
| `poweroff` | Apaga el sistema (semihosting) |

### Sistema de Archivos
| Comando | Descripción |
|---------|-------------|
| `touch [archivo]` | Crea un archivo vacío en el RamFS |
| `rm [archivo]` | Elimina un archivo del disco virtual |
| `ls` | Lista archivos (ID, tamaño, nombre) |
| `cat [archivo]` | Muestra el contenido de un archivo |
| `write [archivo]` | Escribe texto predefinido en un archivo |

### Tests del Sistema
| Comando | Descripción |
|---------|-------------|
| `test all` | Batería global de tests (memoria + scheduler) |
| `test rr` | Test de Round-Robin con Quantum |
| `test sem` | Test de Semáforos con Wait Queues |
| `test pf` | Test de Demand Paging (Page Faults) |
| `test demo` | Demo integrada (heap + RamFS en dos procesos) |

## Historial de Versiones

### v0.6.1 (Febrero 23, 2026)
- Corrección de bug en `vectors.S`: vector EL0 IRQ (+0x480) redirigía a `el0_sync` en vez de `el0_irq`
- Corrección de parsing en `shell.c`: comandos comparaban contra buffer completo en vez de primer token
- Eliminación de código muerto: funciones educativas movidas a `demos.c`
- Eliminación de funciones no utilizadas (`delay`, `memcpy`, `create_thread`)
- Unificación de `NULL` (`#define NULL ((void*)0)` en `types.h`) sustituyendo `nullptr`
- Corrección de typo `prempt_count` → `preempt_count`
- Deduplicación de `PAGE_SIZE` (canónico en `pmm.h`)
- Consolidación de declaraciones `extern` dispersas en headers propios
- Compilación limpia: cero warnings con `-Wall -Wextra`
- Versionado uniforme: `@version 0.6.1` en todos los archivos fuente (.c, .h, .S)
- Nuevo documento: `docs/TEMARIO.md` — mapeo temario universitario → código

### v0.6 (Enero 26, 2026)
- VFS (Virtual File System) con RamFS: iNodos, File Descriptors, 64 archivos
- 5 nuevos comandos de filesystem: `touch`, `rm`, `ls`, `cat`, `write`
- Parser de argumentos en shell (11 comandos totales)
- Tests modulares: `test [all|rr|sem|pf|demo]`
- Syscalls preparatorias: `SYS_OPEN`, `SYS_READ` como stubs
- Reorganización modular de includes (`fs/`, `shell/`, `utils/`)

### v0.5
- Planificador Round-Robin con Quantum + Prioridades + Aging
- Semáforos con Wait Queues (sin busy-wait)
- Demand Paging mediante Page Faults
- Asignador dinámico `kmalloc`/`kfree` con heap de 64MB

### v0.4
- MMU con tablas multinivel (L1/L2/L3), páginas de 4KB
- GIC v2 + Timer con cambio de contexto automático
- Shell interactivo básico

---

*Proyecto educativo para la asignatura de Sistemas Operativos — ARM64 bare-metal en QEMU*
