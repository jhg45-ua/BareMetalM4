# BareMetalM4 v0.6 - Kernel Educativo ARM64

**BareMetalM4 v0.6** es un kernel *bare-metal* educativo para **ARM64 (AArch64)** diseñado para aprender los fundamentos de sistemas operativos ejecutándose en QEMU.

## ✨ Características Principales (v0.6)

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

## 📂 Estructura Modular (v0.6)

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
├── fs/             # Sistema de archivos (v0.6)
│   └── ramfs.c     # RamFS: VFS, iNodos, File Descriptors
├── shell/          # Interfaz de usuario
│   └── shell.c     # Shell + 11 comandos + parser
├── utils/          # Utilidades
│   ├── kutils.c    # panic, delay, strings (k_strlen)
│   └── tests.c     # Tests modulares (RR, Sem, PF)
└── semaphore.c     # Semáforos con Wait Queues
```

Ver [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) para documentación completa.

## 🛠️ Requisitos

- **QEMU:** `qemu-system-aarch64`
- **Toolchain AArch64:** `aarch64-none-elf-gcc` (o `aarch64-linux-gnu-gcc`)
- **Make**

### Instalación macOS (M1/M2/M3/M4)
```bash
brew install qemu aarch64-none-elf-gcc
```

### Instalación Linux (ARM64)
```bash
sudo apt install gcc-aarch64-linux-gnu qemu-system-arm make
```

*Nota: Ajusta `CC` y `LD` en el Makefile si usas `aarch64-linux-gnu-gcc`.*

## ⚙️ Compilación y Ejecución

```bash
# Compilar
make

# Ejecutar en QEMU
make run

# Limpiar
make clean
```

**Salir de QEMU:** `Ctrl+A` luego `x`

## 🎯 Comandos del Shell (v0.6)

Una vez ejecutado, el sistema arranca un shell interactivo con los siguientes comandos:

### Gestión del Sistema
- `help` - Muestra todos los comandos disponibles
- `ps` - Lista procesos (PID, prioridad, estado, tiempo de CPU, nombre)
- `clear` - Limpia la pantalla (códigos ANSI)
- `panic` - Provoca un kernel panic (demo)
- `poweroff` - Apaga el sistema (semihosting)

### Sistema de Archivos (v0.6)
- `touch [archivo]` - Crea un archivo vacío en el RamFS
- `rm [archivo]` - Elimina un archivo del disco virtual
- `ls` - Lista archivos (ID, tamaño, nombre)
- `cat [archivo]` - Muestra el contenido de un archivo
- `write [archivo]` - Escribe texto predefinido en un archivo

### Tests del Sistema (v0.6)
- `test all` - Ejecuta todos los tests disponibles
- `test rr` - Test de Round-Robin con Quantum
- `test sem` - Test de Semáforos con Wait Queues
- `test pf` - Test de Demand Paging (Page Faults)

## 📖 Documentación Completa

Para información detallada sobre la arquitectura, consulta:

**[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)** - Documentación completa v0.6 que incluye:
- Estructura modular detallada (~3,200 líneas de código)
- Subsistemas avanzados:
  - Planificador Round-Robin con Quantum
  - Semáforos con Wait Queues (sin busy-wait)
  - Demand Paging y gestión de Page Faults
  - Sistema de archivos RamFS con VFS
- Flujo de ejecución completo con diagramas ASCII
- Decisiones de diseño y limitaciones conocidas
- Historial de cambios (v0.4 → v0.5 → v0.6)

## 🚀 Novedades en v0.6 (Enero 26, 2026)

### Sistema de Archivos en Memoria (RamFS)
- ✅ **VFS (Virtual File System)** con soporte de iNodos y File Descriptors
- ✅ **64 archivos** simultáneos, 4KB por archivo
- ✅ **Operaciones**: create, open, read, write, close, remove, ls
- ✅ **Integración con shell**: 5 nuevos comandos de filesystem

### Mejoras del Shell
- ✅ **Parser de argumentos** completo
- ✅ **11 comandos totales** (6 nuevos desde v0.5)
- ✅ **Tests modulares**: `test [all|rr|sem|pf]`

### Preparación para Modo Usuario
- ✅ **Syscalls preparatorias**: `SYS_OPEN` (2), `SYS_READ` (3) como stubs
- ✅ **Reorganización de includes**: `fs/`, `shell/`, `utils/`

### Utilidades
- ✅ Nueva función: `k_strlen()` para soporte de strings
- ✅ Limpieza de zombies mejorada (`free_zombie`)

---

*Proyecto educativo para aprendizaje de sistemas operativos en ARM64*
