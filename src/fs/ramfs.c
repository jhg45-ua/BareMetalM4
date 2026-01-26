/**
* @file ramfs.c
 * @brief Implementación del Sistema de Ficheros en Memoria (RamFS)
 * * @details
 * Gestiona un disco virtual en la memoria RAM:
 * - Superbloque global
 * - Gestión de iNodos
 * - Creación y listado de ficheros
 */

#include "../../include/fs/vfs.h"
#include "../../include/utils//kutils.h"
#include "../../include/drivers/io.h"

/* ========================================================================== */
/* EL DISCO DURO VIRTUAL (Variable Global)                                   */
/* ========================================================================== */
static superblock_t ram_disk;

/* ========================================================================== */
/* TABLA DE FICHEROS ABIERTOS (FILE DESCRIPTORS)                             */
/* ========================================================================== */
static file_t fd_table[MAX_FILES];

/* ========================================================================== */
/* FUNCIONES DEL SISTEMA DE FICHEROS                                         */
/* ========================================================================== */

/**
 * @brief Formatea e inicializa el RamDisk
 * @param start_addr Dirección de memoria donde empieza el disco
 * @param size Tamaño total en bytes
 */
void ramfs_init(unsigned long start_addr, unsigned long size) {
    kprintf("   [VFS] Formateando RamDisk en 0x%x (Tamano: %d KB)...\n", start_addr, size / 1024);

    ram_disk.start_addr = start_addr;
    ram_disk.total_size = size;
    ram_disk.free_inodes = MAX_FILES;

    /* Limpiar todos los iNodos (marcarlos como libres) */
    for (int i = 0; i < MAX_FILES; i++) {
        ram_disk.inodes[i].id = i;
        ram_disk.inodes[i].is_used = 0;
        ram_disk.inodes[i].size = 0;
        ram_disk.inodes[i].type = FS_FILE;
        memset(ram_disk.inodes[i].name, 0, sizeof(ram_disk.inodes[i].name));

        /* Asignacion de bloques estatica: Cada archivo tiene 1 página (4KB) */
        ram_disk.inodes[i].data_ptr = start_addr + (i * MAX_FILE_SIZE);
    }
    kprintf("   [VFS] RamDisk montado con exito. iNodos libres: %d\n", ram_disk.free_inodes);
}

/**
 * @brief Crea un nuevo archivo vacío en el disco
 * @param name Nombre del archivo
 * @return 0 si éxito, -1 si error (disco lleno o nombre duplicado)
 */
int vfs_create(const char *name) {
    if (ram_disk.free_inodes <= 0) {
        kprintf("[VFS] Error: Disco lleno (No quedan iNodos)\n");
        return -1;
    }

    /* 1. Comprobar que no existe un archivo con ese nombre */
    for (int i = 0; i < MAX_FILES; i++) {
        if (ram_disk.inodes[i].is_used && k_strcmp(ram_disk.inodes[i].name, name) == 0) {
            kprintf("[VFS] Error: El archivo '%s' ya existe.\n", name);
            return -1;
        }
    }

    /* 2. Buscar el primer iNodo libre */
    for (int i = 0; i < MAX_FILES; i++) {
        if (!ram_disk.inodes[i].is_used) {
            /* Encontrado, ocupar iNodo */
            ram_disk.inodes[i].is_used = 1;
            ram_disk.inodes[i].size = 0; /* El archivo está vacio */

            /* Copiar nombre (con límite de seguridad) */
            int name_len = k_strlen(name);
            if (name_len >= FILE_NAME_LEN) name_len = FILE_NAME_LEN - 1;

            for (int c = 0; c < name_len; c++) ram_disk.inodes[i].name[c] = name[c];
            ram_disk.inodes[i].name[name_len] = '\0';

            ram_disk.free_inodes--;
            kprintf("[VFS] Archivo '%s' creado con exito (Inodo %d).\n", name, i);
            return 0;
        }
    }
    return -1;
}

/**
 * @brief Lista los archivos del directorio raíz (comando 'ls')
 */
void vfs_ls(void) {
    kprintf("\nID  |   Size (Bytes)   | Name\n");
    kprintf("----|------------------|----------------------\n");

    int count = 0;
    for (int i = 0; i < MAX_FILES; i++) {
        if (ram_disk.inodes[i].is_used) {
            kprintf("%d   |   %d              | %s\n",
                    ram_disk.inodes[i].id,
                    ram_disk.inodes[i].size,
                    ram_disk.inodes[i].name);
            count++;
        }
    }

    if (count == 0) {
        kprintf(" (Directorio vacio)\n");
    }
    kprintf("\n");
}

/**
 * @brief Abre un archivo y devuelve un File Descriptor (FD)
 * @return FD (índice >= 0) o -1 si error
 */
int vfs_open(const char *name) {
    inode_t *target_inode = nullptr;

    /* 1. Buscar el iNodo por nombre */
    for (int i = 0; i < MAX_FILES; i++) {
        if (ram_disk.inodes[i].is_used && k_strcmp(ram_disk.inodes[i].name, name) == 0) {
            target_inode = &ram_disk.inodes[i];
            break;
        }
    }

    if (target_inode == nullptr) {
        kprintf("[VFS] Error: Archivo '%s' no encontrado.\n", name);
        return -1;
    }

    /* 2. Buscar un slot libre en la tabla de File Descriptors */
    for (int i = 0; i < MAX_FILES; i++) {
        if (fd_table[i].inode == nullptr) {
            fd_table[i].inode = target_inode;
            fd_table[i].position = 0; /* Empezamos a leer/escribir desde el principio */
            return i; /* Devolvemos el número de FD */
        }
    }
    return -1; /* Demasiados archivos abiertos */
}

/**
 * @brief Escribe datos en un archivo abierto
 */
int vfs_write(const int fd, const char *buf, int count) {
    if (fd < 0 || fd >= MAX_FILES || fd_table[fd].inode == nullptr) return -1;

    file_t *file = &fd_table[fd];
    inode_t *inode = file->inode;

    /* Calcular cuanto podemos escribir sin pasarnos de la página (4KB) */
    int space_left = MAX_FILE_SIZE - file->position;
    int bytes_to_write = (count > space_left) ? space_left : count;

    if (bytes_to_write <= 0) return 0;

    /* Escribir en la memoria RAM directamente (usando el data_ptr del iNodo) */
    char *dest = (char *)(inode->data_ptr + file->position);
    for (int i = 0; i < bytes_to_write; i++) {
        dest[i] = buf[i];
    }

    /* Actualizar punteros y tamaño del archivo */
    file->position += bytes_to_write;
    if (file->position > inode->size) {
        inode->size = file->position;
    }

    return bytes_to_write;
}

/**
 * @brief Lee datos de un archivo abierto
 */
int vfs_read(const int fd, char *buf, int count) {
    if (fd < 0 || fd >= MAX_FILES || fd_table[fd].inode == nullptr) return -1;

    file_t *file = &fd_table[fd];
    inode_t *inode = file->inode;

    /* Calcular cuánto podemos leer (no podemos leer más allá del tamaño del archivo) */
    int bytes_left = inode->size - file->position;
    int bytes_to_read = (count > bytes_left) ? bytes_left : count;

    if (bytes_to_read <= 0) return 0; /* Fin de archivo (EOF) */

    /* Leer desde la memoria RAM */
    char *src = (char *)(inode->data_ptr + file->position);
    for (int i = 0; i < bytes_to_read; i++) {
        buf[i] = src[i];
    }

    file->position += bytes_to_read;
    return bytes_to_read;
}

/**
 * @brief Cierra un archivo abierto (Libera el File Descriptor)
 */
int vfs_close(int fd) {
    if (fd < 0 || fd >= MAX_FILES || fd_table[fd].inode == nullptr) return -1;

    /* Limpiar el slot para que pueda ser reutilizado */
    fd_table[fd].inode = nullptr;
    fd_table[fd].position = 0;
    return 0;
}

/**
 * @brief Elimina un archivo del disco (Libera el Inodo)
 */
int vfs_remove(const char *name) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (ram_disk.inodes[i].is_used && k_strcmp(ram_disk.inodes[i].name, name) == 0) {
            /* 1. Marcar inodo como libre */
            ram_disk.inodes[i].is_used = 0;
            ram_disk.inodes[i].size = 0;

            /* 2. Borrar el nombre (opcional de seguridad) */
            memset(ram_disk.inodes[i].name, 0, FILE_NAME_LEN);

            /* 3. Limpiar los datos de la ram fisicos en RAM (security zeroing) */
            memset((void*)ram_disk.inodes[i].data_ptr, 0, MAX_FILE_SIZE);

            /* 4. Añadir el iNodo libre a al contador */
            ram_disk.free_inodes++;
            kprintf("[VFS] Archivo '%s' eliminado.\n", name);
            return 0;
        }
    }
    kprintf("[VFS] Error: Archivo '%s' no existe.\n", name);
    return -1;
}

