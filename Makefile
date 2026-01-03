CC = aarch64-elf-gcc
LD = aarch64-elf-ld

# --- TRUCO ---
# Preguntamos a GCC dónde guarda sus cabeceras internas (stdarg.h, stdint.h, etc.)
# y guardamos esa ruta en la variable GCC_INC
GCC_INC = $(shell $(CC) -print-file-name=include)

# --- CFLAGS ---
# Añadimos -I$(GCC_INC) para decirle: "Usa tus cabeceras internas, pero ignora las del sistema operativo"
CFLAGS = -Wall -O2 -ffreestanding -nostdinc -I$(GCC_INC) -nostdlib -mcpu=cortex-a72

# Archivos fuente
SRCS = src/boot.S src/kernel.c src/entry.S src/locks.S src/io.c
OBJS = build/boot.o build/kernel.o build/entry.o build/locks.o build/io.o

# ... (El resto del Makefile sigue igual a partir de aquí) ...
all: clean build/baremetalm4.elf run

build_dir:
	mkdir -p build

# Compilar ensamblador
build/boot.o: src/boot.S | build_dir
	$(CC) $(CFLAGS) -c src/boot.S -o build/boot.o

# Compilar C (Kernel)
build/kernel.o: src/kernel.c | build_dir
	$(CC) $(CFLAGS) -c src/kernel.c -o build/kernel.o

# Compilar C (IO)
build/io.o: src/io.c | build_dir
	$(CC) $(CFLAGS) -c src/io.c -o build/io.o

# Compilar entry
build/entry.o: src/entry.S | build_dir
	$(CC) $(CFLAGS) -c src/entry.S -o build/entry.o

# Compilar locks
build/locks.o: src/locks.S | build_dir
	$(CC) $(CFLAGS) -c src/locks.S -o build/locks.o

# Enlazar
build/baremetalm4.elf: $(OBJS)
	$(LD) -T link.ld -o build/baremetalm4.elf $(OBJS)

run:
	qemu-system-aarch64 -M virt -cpu cortex-a72 -nographic -kernel build/baremetalm4.elf

clean:
	rm -rf build