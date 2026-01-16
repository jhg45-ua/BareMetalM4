# BareMetalM4 - Kernel Educativo ARM64

**BareMetalM4** es un kernel *bare-metal* educativo para **ARM64 (AArch64)** diseñado para aprender los fundamentos de sistemas operativos ejecutándose en QEMU.

## ✨ Características Principales

- **Arquitectura:** ARM64 (ARMv8-A) Cortex-A72
- **Plataforma:** QEMU `virt` machine
- **Multitarea Expropiatoria:** Planificador con prioridades y aging (hasta 64 procesos)
- **Gestión de Memoria:** MMU con memoria virtual, asignador dinámico (`kmalloc`/`kfree`) con heap de 64MB
- **Interrupciones:** GIC v2 + Timer de sistema con cambio de contexto automático
- **Sincronización:** Spinlocks (LDXR/STXR) y semáforos
- **Shell Interactivo:** Comandos para gestión de procesos y diagnóstico
- **Sistema de Tests:** Validación automática de subsistemas en boot
- **Sin dependencias:** Sin librerías estándar (`-ffreestanding -nostdlib`)

## 📂 Estructura Modular

El kernel está organizado en módulos especializados:

```
src/
├── kernel/         # Núcleo del sistema
│   ├── kernel.c    # Inicialización
│   ├── process.c   # Gestión de procesos
│   └── scheduler.c # Planificador con aging
├── drivers/        # Controladores hardware
│   ├── io.c        # Driver UART
│   └── timer.c     # GIC + Timer
├── mm/             # Gestión de memoria
│   ├── mm.c        # MMU (tablas de páginas)
│   └── malloc.c    # Asignador dinámico (64MB heap)
├── shell/          # Interfaz de usuario
│   └── shell.c     # Shell + comandos
└── utils/          # Utilidades
    ├── kutils.c    # panic, delay, strings
    └── tests.c     # Tests del sistema
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

## 🎯 Comandos del Shell

Una vez ejecutado, el sistema arranca un shell interactivo con los siguientes comandos:

- `help` - Muestra comandos disponibles
- `ps` - Lista procesos (PID, prioridad, estado, nombre)
- `clear` - Limpia la pantalla
- `panic` - Provoca un kernel panic (demo)
- `poweroff` - Apaga el sistema

## 📖 Documentación Completa

Para información detallada sobre la arquitectura, consulta:

**[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)** - Documentación completa que incluye:
- Estructura modular detallada
- Subsistemas (MMU, Scheduler, Interrupciones, Sincronización)
- Flujo de ejecución completo
- Decisiones de diseño y limitaciones

---

*Proyecto educativo para aprendizaje de sistemas operativos en ARM64*