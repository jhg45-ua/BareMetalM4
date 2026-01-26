#ifndef VFS_H
#define VFS_H

#define MAX_FILES 64
#define FILE_NAME_LEN 32
#define MAX_FILE_SIZE 4096 /* Ficheros de 1 pagina por ahora */

/* Tipos de iNodo */
#define FS_FILE      1
#define FS_DIRECTORY 2

/* ========================================================================== */
/* 1. EL INODO: La representación de un archivo en el disco                   */
/* ========================================================================== */
typedef struct {
    int id;                     /* Nombre de iNodo (ID unico) */
    int type;                   /* FS_FILE o FS_DIRECTORY */
    int size;                   /* Tamaño en bytes */
    unsigned long data_ptr;     /* Puntero a la memoria donde están los datos */
    char name[FILE_NAME_LEN];   /* Nombre del archivo (Simplificación para RamFS) */
    int is_used;                /* 1 si está ocupado, 0 si está libre */
} inode_t;

/* ========================================================================== */
/* 2. EL SUPERBLOQUE: La información maestra del disco                        */
/* ========================================================================== */
typedef struct {
    unsigned long total_size;   /* Tamaño total del RamDisk */
    int free_inodes;            /* iNodos disponibles */
    unsigned long start_addr;   /* Dirección física donde empieza el disco en RAM */
    inode_t inodes[MAX_FILES];  /* Tabla de iNodos (Directorio raíz plano) */
} superblock_t;

/* ========================================================================== */
/* 3. EL FILE DESCRIPTOR: El archivo abierto por un proceso                   */
/* ========================================================================== */
typedef struct {
    inode_t *inode;             /* A qué iNodo apunta */
    int position;               /* Puntero de lectura/escritura (Offset) */
    int flags;                  /* Permisos (Lectura, Escritura, etc.) */
} file_t;

/* ========================================================================== */
/* API PÚBLICA DEL VFS                                                       */
/* ========================================================================== */

void ramfs_init(unsigned long start_addr, unsigned long size);
int vfs_create(const char *name);
int vfs_open(const char *name);
int vfs_read(int fd, char *buf, int count);
int vfs_write(int fd, const char *buf, int count);
void vfs_ls(void); /* Para listar el contenido en la Shell */
int vfs_close(int fd);
int vfs_remove(const char *name);


#endif // VFS_H