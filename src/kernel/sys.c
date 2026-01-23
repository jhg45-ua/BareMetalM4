/**
 * @file sys.c
 * @brief Implementación de llamadas al sistema (syscalls)
 * 
 * @details
 *   Este archivo contiene:
 *   - Implementación de syscalls básicas (write, exit)
 *   - Dispatcher central para manejo de excepciones síncronas (SVC)
 *   - Handler para fallas de segmentación
 * 
 *   SYSCALLS IMPLEMENTADAS:
 *   - SYS_WRITE (0): Escritura en consola desde procesos de usuario
 *   - SYS_EXIT (1): Terminación de proceso con código de salida
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
/* MANEJO DE FALLAS                                                          */
/* ========================================================================== */

/**
 * @brief Handler para excepciones no controladas
 * 
 * @details
 *   Se ejecuta cuando un proceso causa una excepción síncrona no manejada,
 *   como acceso a memoria inválida (segmentation fault).
 *   Termina el proceso culpable en lugar de colgar el sistema.
 */
void handle_fault(void) {
    unsigned long far, esr;

    /* 1. Leer registros del procesador
       FAR_EL1 (Fault Address Register): Qué dirección provocó el fallo
       ESR_EL1 (Exception Syndrome Register): Por qué ocurrió (lectura, escritura, etc.) */
    asm volatile("mrs %0, far_el1" : "=r"(far));
    asm volatile("mrs %0, esr_el1" : "=r"(esr));

    /* Extraer la clase de excepción (EC - bits 31:26 del ESR) */
    unsigned long ec = esr >> 26;

    /* EC = 0x24 (Data Abort en Kernel) o 0x25 (Data Abort en Usuario) */
    if (ec == 0x24 || ec == 0x25) {
        kprintf("\n[MMU] Page Fault (Falta de Pagina) en dir: 0x%x\n", far);

        /* 2. Asignar una página física libre del PMM */
        unsigned long phys_page = get_free_page();
        if (phys_page) {
            kprintf("      -> Resolviendo: Paginacion por Demanda (Fisica: 0x%x)\n", phys_page);

            /* Alinear la dirección virtual al inicio de la página (múltiplo de 4KB) */
            unsigned long virt_aligned = far & ~(PAGE_SIZE - 1);

            /* Configurar permisos: Si el fallo vino de EL0 (Usuario), damos permisos de usuario */
            unsigned long flags = (ec == 0x25) ? (MM_RW | MM_USER | MM_SH | (ATTR_NORMAL << 2))
                                               : (MM_RW | MM_KERNEL | MM_SH | (ATTR_NORMAL << 2));

            /* 3. Mapear en la tabla de páginas L3 */
            map_page(kernel_pgd, virt_aligned, phys_page, flags);

            /* 4. Invalidar TLB para que la MMU vea la nueva traducción */
            tlb_invalidate_all();

            /* ÉXITO: Al hacer 'return', la CPU reintentará la instrucción y funcionará */
            return;
        } else {
            kprintf("[PMM] CRITICAL: Out of Memory. Imposible resolver Page Fault.\n");
        }
    }

    /* Si llegamos aquí, fue un acceso inválido o no queda RAM */
    kprintf("\n[CPU] Violacion de Segmento (Segmentation Fault) en 0x%x. Matando proceso.\n", far);
    exit();
}