CC = aarch64-elf-gcc
LD = aarch64-elf-ld

# Directorio de cabeceras internas de GCC
GCC_INC = $(shell $(CC) -print-file-name=include)

# Flags de compilación
CFLAGS = -Wall -O2 -ffreestanding -nostdinc -I$(GCC_INC) -nostdlib \
         -mcpu=cortex-a72 -mgeneral-regs-only

# Directorios
SRC_DIR = src
BUILD_DIR = build

# Archivos fuente (se buscan automáticamente en subdirectorios)
ASM_SRCS = $(wildcard $(SRC_DIR)/*.S)
C_SRCS = $(wildcard $(SRC_DIR)/*.c) \
         $(wildcard $(SRC_DIR)/kernel/*.c) \
         $(wildcard $(SRC_DIR)/shell/*.c) \
         $(wildcard $(SRC_DIR)/utils/*.c)

# Archivos objeto (conversión automática con soporte para subdirectorios)
ASM_OBJS = $(patsubst $(SRC_DIR)/%.S, $(BUILD_DIR)/%.o, $(ASM_SRCS))
C_OBJS = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(C_SRCS))
OBJS = $(ASM_OBJS) $(C_OBJS)

# Directorios de build necesarios
BUILD_SUBDIRS = $(BUILD_DIR)/kernel $(BUILD_DIR)/shell $(BUILD_DIR)/utils

# Archivos finales
ELF = $(BUILD_DIR)/baremetalm4.elf
LINKER_SCRIPT = link.ld

# --- REGLAS ---

.PHONY: all run clean

all: $(ELF)

# Regla para ensamblar archivos .S
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.S | $(BUILD_DIR)
	@echo "[ASM] $<"
	@$(CC) $(CFLAGS) -c $< -o $@

# Regla para compilar archivos .c (con soporte para subdirectorios)
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR) $(BUILD_SUBDIRS)
	@echo "[CC]  $<"
	@$(CC) $(CFLAGS) -c $< -o $@

# Enlazar todos los objetos
$(ELF): $(OBJS) $(LINKER_SCRIPT)
	@echo "[LD]  $@"
	@$(LD) -T $(LINKER_SCRIPT) -o $@ $(OBJS)
	@echo "Build completado: $@"

# Crear directorio de build
$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

# Crear subdirectorios de build
$(BUILD_SUBDIRS): | $(BUILD_DIR)
	@mkdir -p $@

# Ejecutar en QEMU
run: $(ELF)
	@qemu-system-aarch64 -M virt -cpu cortex-a72 -nographic -kernel $(ELF)

# Limpiar archivos generados
clean:
	@echo "Limpiando build..."
	@rm -rf $(BUILD_DIR)

# Mostrar variables (útil para debug)
debug:
	@echo "ASM_SRCS: $(ASM_SRCS)"
	@echo "C_SRCS:   $(C_SRCS)"
	@echo "OBJS:     $(OBJS)"