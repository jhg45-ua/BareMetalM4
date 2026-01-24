/**
 * @file sys.c
 * @brief Syscalls y manejo de excepciones con Demand Paging
 * 
 * @details
 *   Este archivo implementa:
 *   
 *   SYSCALLS BÁSICAS:
 *   - SYS_WRITE (0): Escritura en consola desde procesos de usuario
 *   - SYS_EXIT (1): Terminación de proceso con código de salida
 *   - Dispatcher central para manejo de SVC (Supervisor Call)
 *   
 *   DEMAND PAGING (Paginación por Demanda):
 *   - Asignación perezosa (lazy allocation) de memoria
 *   - Page Fault Handler que asigna páginas bajo demanda
 *   - Optimiza uso de RAM: Solo asigna cuando realmente se usa
 *   
 *   MANEJO DE EXCEPCIONES:
 *   - Data Abort Handler para page faults y segmentation faults
 *   - Lectura de FAR_EL1 (Fault Address Register)
 *   - Lectura de ESR_EL1 (Exception Syndrome Register)
 *   - Terminación segura de procesos con fallos inválidos
 * 
 * @author Sistema Operativo Educativo BareMetalM4
 * @version 0.4
 */

#include "../../include/kernel/sys.h"
#include "../../include/drivers/io.h"
#include "../../include/kernel/process.h"
#include "../../include/mm/vmm.h"
#include "../../include/mm/pmm.h"
#include "../../include/mm/mm.h"

/* ========================================================================== */
/* IMPLEMENTACION DE SYSCALLS                                                */
/* ========================================================================== */

/**
 * @brief Syscall para escritura en consola
 * @param buf Puntero al buffer de caracteres a imprimir
 * 
 * @details
 *   En un SO real, aquí verificaríamos que 'buf' apunta a memoria
 *   válida del usuario antes de acceder.
 */
void sys_write(char *buf) {
    kprintf("%s", buf);
}

/**
 * @brief Syscall para terminar proceso actual
 * @param code Código de salida del proceso
 * 
 * @details
 *   Imprime un mensaje de diagnóstico y llama a exit() para
 *   terminar el proceso actual y marcarlo como zombie.
 */
void sys_exit(int code) {
    kprintf("\n[SYSCALL] Proceso solicitó salida con código %d\n", code);
    exit();
}

/* ========================================================================== */
/* DISPATCHER DE SYSCALLS                                                     */
/* ========================================================================== */

/**
 * @brief Handler central de syscalls (dispatcher)
 * @param regs Puntero a registros guardados del proceso
 * @param syscall Número de syscall solicitada (contenido de x8)
 * 
 * @details
 *   Este handler es llamado desde sync_handler en entry.S cuando
 *   se ejecuta una instrucción SVC. Despacha la syscall apropiada
 *   según el número en el registro x8.
 */
void syscall_handler(struct pt_regs *regs, int syscall) {
    switch (syscall) {
        case SYS_WRITE:
            /* Argumento en x19 según convención actual */
            sys_write((char *)regs->x19);
            break;

        case SYS_EXIT:
            sys_exit((int)regs->x19);
            break;

        default:
            kprintf("Syscall desconocida: %d\n", syscall);
            break;
    }
}

/* ========================================================================== */
/* MANEJO DE EXCEPCIONES Y DEMAND PAGING                                    */
/* ========================================================================== */

/**
 * @brief Handler para excepciones síncronas (Page Faults y Seg Faults)
 * 
 * @details
 *   Handler principal para excepciones Data Abort. Implementa:
 *   
 *   PAGINACIÓN POR DEMANDA (Demand Paging):
 *   - Detecta Page Faults legítimos (EC=0x24 kernel, EC=0x25 usuario)
 *   - Asigna página física bajo demanda con get_free_page()
 *   - Mapea la página en las tablas de páginas (L1/L2/L3)
 *   - Invalida TLB para que la MMU vea la nueva entrada
 *   - Reintenta la instrucción → ¡Éxito!
 *   
 *   VENTAJAS DEL DEMAND PAGING:
 *   - ✅ Lazy Allocation: Solo asigna memoria cuando se usa
 *   - ✅ Optimiza RAM: Evita reservar memoria nunca usada
 *   - ✅ Similar a Linux/BSD: Técnica estándar en kernels modernos
 *   - ✅ Transparente: El proceso no sabe que hubo un page fault
 *   
 *   FLUJO DEL DEMAND PAGING:
 *   1. Proceso intenta acceder a dirección no mapeada (ej: 0x50000000)
 *   2. CPU genera Data Abort (Page Fault)
 *   3. Se ejecuta este handler
 *   4. Leemos FAR_EL1 (dirección que falló) y ESR_EL1 (causa)
 *   5. Si es page fault legítimo:
 *      a. Asignamos página física del PMM
 *      b. Mapeamos virtual→física en tablas de páginas
 *      c. Invalidamos TLB
 *      d. Retornamos → CPU reintenta la instrucción → Éxito
 *   6. Si no hay RAM o es acceso inválido:
 *      - Imprimimos error
 *      - Terminamos el proceso con exit()
 *   
 *   REGISTROS ARM64 CONSULTADOS:
 *   - FAR_EL1: Fault Address Register (qué dirección falló)
 *   - ESR_EL1: Exception Syndrome Register (por qué falló)
 *     - EC (bits 31:26): Exception Class
 *       - 0x24: Data Abort desde EL1 (kernel)
 *       - 0x25: Data Abort desde EL0 (usuario)
 * 
 * @note En un SO real, aquí también se verificaría:
 *   - Límites del heap/stack del proceso
 *   - Copy-on-Write para fork()
 *   - Swap a disco si no hay RAM
 */
void handle_fault(void) {
    unsigned long far, esr;

    /* === 1. LEER REGISTROS DE EXCEPCIÓN === */
    /* FAR_EL1: Qué dirección provocó el fallo */
    asm volatile("mrs %0, far_el1" : "=r"(far));
    
    /* ESR_EL1: Por qué ocurrió (lectura, escritura, permisos, etc.) */
    asm volatile("mrs %0, esr_el1" : "=r"(esr));

    /* Extraer Exception Class (EC) - bits 31:26 del ESR */
    unsigned long ec = esr >> 26;

    /* === 2. VERIFICAR SI ES UN PAGE FAULT === */
    /* EC = 0x24 (Data Abort en Kernel - EL1) */
    /* EC = 0x25 (Data Abort en Usuario - EL0) */
    if (ec == 0x24 || ec == 0x25) {
        kprintf("\n[MMU] Page Fault (Demand Paging) en dir: 0x%x\n", far);

        /* === 3. ASIGNAR PÁGINA FÍSICA DEL PMM === */
        unsigned long phys_page = get_free_page();
        if (phys_page) {
            kprintf("      -> Resolviendo: Asignando página física 0x%x\n", phys_page);

            /* Alinear dirección virtual al inicio de la página (múltiplo de 4KB) */
            unsigned long virt_aligned = far & ~(PAGE_SIZE - 1);

            /* === 4. CONFIGURAR PERMISOS SEGÚN NIVEL DE PRIVILEGIO === */
            /* Si el fallo vino de EL0 (Usuario), damos permisos de usuario */
            unsigned long flags = (ec == 0x25) ? (MM_RW | MM_USER | MM_SH | (ATTR_NORMAL << 2))
                                               : (MM_RW | MM_KERNEL | MM_SH | (ATTR_NORMAL << 2));

            /* === 5. MAPEAR EN TABLAS DE PÁGINAS (L1/L2/L3) === */
            map_page(kernel_pgd, virt_aligned, phys_page, flags);

            /* === 6. INVALIDAR TLB === */
            /* La TLB cachea traducciones. Hay que invalidarla para que
               la MMU vea la nueva entrada en las tablas de páginas */
            tlb_invalidate_all();

            /* === 7. RETORNAR Y REINTENTAR === */
            /* ÉXITO: Al hacer 'return', la CPU automáticamente reintentará
               la instrucción que causó el fault. Ahora la página está mapeada
               y el acceso funcionará correctamente. El proceso continúa
               sin saber que hubo un page fault (transparente). */
            return;
        } else {
            /* ERROR CRÍTICO: No hay RAM disponible */
            kprintf("[PMM] CRITICAL: Out of Memory. Imposible resolver Page Fault.\n");
        }
    }

    /* === 8. SI LLEGAMOS AQUÍ, FUE UN ACCESO INVÁLIDO === */
    /* - No era un page fault legítimo (EC diferente)
       - O no quedaba RAM para asignar
       - O era un acceso a dirección completamente inválida (ej: NULL) */
    kprintf("\n[CPU] Violación de Segmento (Segmentation Fault) en 0x%x. Matando proceso.\n", far);
    kprintf("      ESR_EL1: 0x%x (EC: 0x%x)\n", esr, ec);
    
    /* Terminar el proceso actual de forma segura */
    exit();
}