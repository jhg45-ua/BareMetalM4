CC = aarch64-elf-gcc
LD = aarch64-elf-LD

# Opciones para bare metal: sin librerías estándar, sin punto flotante complejo
CFLAGS = -Wall -O2 -ffreestanding -nostdinc -nostdlib -mcpu=cortex-a72


# Archivos fuente
SRCS = src/boot.S src/kernel.c src/entry.S
OBJS = build/boot.o build/kernel.o build/entry.o

all: clean build/baremetalm4.elf run

#Crear la carpeta build si no existe
build_dir:
	mkdir -p build

# Compilar ensamblador
build/boot.o: src/boot.S | build_dir
	$(CC) $(CFLAGS) -c src/boot.S -o build/boot.o
	
build/entry.o: src/entry.S | build_dir
	$(CC) $(CFLAGS) -c src/entry.S -o build/entry.o

# Compilar C
build/kernel.o: src/kernel.c | build_dir
	$(CC) $(CFLAGS) -c src/kernel.c -o build/kernel.o

# Enlazar
build/baremetalm4.elf: $(OBJS)
	$(LD) -T link.ld -o build/baremetalm4.elf $(OBJS)

# Ejecutar en QEMU
run:
	clear
	qemu-system-aarch64 -M virt -cpu cortex-a72 -nographic -kernel build/baremetalm4.elf

clean:
	rm -rf build
