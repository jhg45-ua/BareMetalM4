# Temario de Sistemas Operativos - Mapeo al Código Fuente

**BareMetalM4 v0.6.1** | ARM64 (AArch64) | QEMU virt  
Documento de referencia: cada tema del temario universitario y su implementación concreta en el código fuente.

---

## Indice

1. [Tema 1: Introducción a los Sistemas Operativos / Llamadas al Sistema](#tema-1-introducción-a-los-sistemas-operativos--llamadas-al-sistema)
2. [Tema 2: Procesos y Planificación](#tema-2-procesos-y-planificación)
3. [Tema 3: Concurrencia y Sincronización](#tema-3-concurrencia-y-sincronización)
4. [Tema 4: Gestion de Memoria](#tema-4-gestion-de-memoria)
5. [Tema 5: Sistemas de Archivos](#tema-5-sistemas-de-archivos)
6. [Tema 6: Entrada/Salida y Drivers](#tema-6-entradasalida-y-drivers)
7. [Referencia cruzada: Archivos por Tema](#referencia-cruzada)

---

## Tema 1: Introducción a los Sistemas Operativos / Llamadas al Sistema

**Conceptos cubiertos:** Arranque del sistema, niveles de privilegio, tabla de vectores de excepción, interfaz de 
llamadas al sistema (syscalls), inicialización del kernel.

### Archivos principales

| Archivo                | Rol                                                         |
|------------------------|-------------------------------------------------------------|
| `src/boot.S`           | Punto de entrada del CPU, configuracion de stack, salto a C |
| `src/entry.S`          | Macros de guardado/restauracion de contexto, manejador SVC  |
| `src/vectors.S`        | Tabla de vectores de excepcion ARM64 (16 entradas)          |
| `src/kernel/kernel.c`  | Funcion `kernel()`: orquestador de inicializacion           |
| `src/kernel/sys.c`     | Dispatcher de syscalls y handler de Page Faults             |
| `include/kernel/sys.h` | Numeros de syscall y `struct pt_regs`                       |
| `link.ld`              | Script del linker: direccion de carga, secciones, stack     |

### Secuencia de arranque

```
CPU reset (EL1)
  |
  v
_start (boot.S:31)        Lee MPIDR_EL1, filtra nucleos secundarios
  |
  v
master (boot.S:38)         Carga _stack_top en SP, limpia BSS
  |
  v
kernel() (kernel.c:48)     Inicializacion en C:
  |-- init_memory_system()    1. MMU + PMM + VMM + Heap
  |-- ramfs_init()            2. Sistema de archivos
  |-- init_process_system()   3. Procesos (PID 0)
  |-- timer_init()            4. GIC + Timer + UART IRQ
  |-- create_process(shell)   5. Shell interactivo
  |
  v
while(1) { wfi; }          Loop IDLE con reaper de zombies
```

### Llamadas al sistema

El mecanismo usa la instrucción `SVC` de ARM64. Las excepciones síncronas llegan al vector correspondiente 
(`el1_sync` o `el0_sync` en `entry.S`), donde se lee `ESR_EL1` para verificar que la clase de excepción es 
`EC=0x15` (SVC). Luego se llama a `syscall_handler()` pasando el frame de registros salvados y el número de 
syscall (desde `x8`).

| Syscall     | Numero | Funcion                                    | Archivo:Linea |
|-------------|--------|--------------------------------------------|---------------|
| `SYS_WRITE` | 0      | `sys_write()` — imprime buffer por consola | `sys.c:47`    |
| `SYS_EXIT`  | 1      | `sys_exit()` — termina el proceso          | `sys.c:59`    |
| `SYS_OPEN`  | 2      | Stub preparatorio                          | `sys.c:86`    |
| `SYS_READ`  | 3      | Stub preparatorio                          | `sys.c:89`    |

### Funciones clave

| Funcion                | Archivo:Linea | Descripcion                                        |
|------------------------|---------------|----------------------------------------------------|
| `_start`               | `boot.S:31`   | Entry point; filtra cores con MPIDR_EL1            |
| `master`               | `boot.S:38`   | Configura SP, limpia BSS, salta a `kernel()`       |
| `kernel_entry` (macro) | `entry.S:33`  | Salva x0-x30 + SPSR_EL1 + ELR_EL1 (256 bytes)      |
| `kernel_exit` (macro)  | `entry.S:58`  | Restaura contexto completo y ejecuta ERET          |
| `el1_sync`             | `entry.S:188` | Manejador de excepciones sincronas desde EL1       |
| `el0_sync`             | `entry.S:199` | Manejador de excepciones sincronas desde EL0       |
| `handle_svc`           | `entry.S:207` | Despacha SVC: pasa pt_regs (x0) y syscall num (x1) |
| `syscall_handler`      | `sys.c:78`    | Switch sobre numero de syscall                     |
| `move_to_user_mode`    | `entry.S:235` | Transicion EL1->EL0 via ERET                       |

### Estructuras

| Estructura       | Archivo:Linea | Campos                                          |
|------------------|---------------|-------------------------------------------------|
| `struct pt_regs` | `sys.h:39`    | `x0`-`x30`, `pstate` (SPSR_EL1), `pc` (ELR_EL1) |

### Decisiones de diseño

- **Sin transición EL2->EL1**: QEMU arranca directamente en EL1, por lo que `boot.S` solo filtra cores y configura el stack.
- **Limpieza de BSS en ensamblador**: Se hace antes de entrar a C para garantizar que las variables globales empiezan en cero.
- **Convención de syscall**: Número en `x8`, primer argumento leido desde `regs->x19` (registro preservado a traves del fork trampoline).

---

## Tema 2: Procesos y Planificación

**Conceptos cubiertos:** PCB (Process Control Block), estados de proceso, cambio de contexto, planificación Round-Robin 
con quantum, prioridades, aging, creación y terminación de procesos.

### Archivos principales

| Archivo                      | Rol                                                     |
|------------------------------|---------------------------------------------------------|
| `include/sched.h`            | `struct pcb`, `struct cpu_context`, estados, constantes |
| `src/kernel/process.c`       | Creacion, terminacion, reaper de zombies                |
| `src/kernel/scheduler.c`     | Planificador Round-Robin + Aging                        |
| `src/entry.S`                | `cpu_switch_to`, `ret_from_fork`, `irq_handler_stub`    |
| `include/kernel/process.h`   | API publica de procesos                                 |
| `include/kernel/scheduler.h` | API publica del planificador                            |

### Estados de proceso

```
              create_process()
                    |
                    v
  UNUSED -------> READY <--------+
    ^               |            |
    |               | schedule() |
    |               v            |
    |            RUNNING         |
    |               |            |
    |    +----------+----------+ |
    |    |                     | |
    |    v                     v |
    | BLOCKED              ZOMBIE
    | (sleep/sem)            |
    |    |              free_zombie()
    |    | wake_up           |
    |    +----> READY        |
    |                        |
    +------------------------+
```

| Estado            | Valor | Definido en  |
|-------------------|-------|--------------|
| `PROCESS_UNUSED`  | 0     | `sched.h:42` |
| `PROCESS_RUNNING` | 1     | `sched.h:43` |
| `PROCESS_READY`   | 2     | `sched.h:44` |
| `PROCESS_BLOCKED` | 3     | `sched.h:45` |
| `PROCESS_ZOMBIE`  | 4     | `sched.h:46` |

Razones de bloqueo:

| Razon                | Valor | Definido en  |
|----------------------|-------|--------------|
| `BLOCK_REASON_NONE`  | 0     | `sched.h:62` |
| `BLOCK_REASON_SLEEP` | 1     | `sched.h:63` |
| `BLOCK_REASON_WAIT`  | 2     | `sched.h:64` |

### Estructuras

| Estructura           | Archivo:Linea | Descripcion                                                                                                                                       |
|----------------------|---------------|---------------------------------------------------------------------------------------------------------------------------------------------------|
| `struct cpu_context` | `sched.h:99`  | 13 campos: x19-x28, fp (x29), pc, sp                                                                                                              |
| `struct pcb`         | `sched.h:143` | PCB completo: context, state, pid, priority, preempt_count, wake_up_time, name[16], stack_addr, cpu_time, block_reason, exit_code, quantum, *next |

### Algoritmo de planificación

El planificador (`schedule()`, `scheduler.c:84`) implementa **Round-Robin con Quantum + Prioridades + Aging**:

```
schedule():
  1. AGING: Para cada proceso READY (excepto el actual),
     decrementar priority-- (scheduler.c:88-94)
  
  2. SELECCION: Buscar el proceso READY con la priority
     mas baja (scheduler.c:97-116)
  
  3. PENALIZACION: Al elegido, priority += 2
     (scheduler.c:125-127)
  
  4. QUANTUM: Asignar quantum = DEFAULT_QUANTUM (5 ticks)
     (scheduler.c:130)
  
  5. CAMBIO DE CONTEXTO: Si el elegido != actual,
     cpu_switch_to(prev, next) (scheduler.c:137)
```

**Preempcion**: `timer_tick()` (`scheduler.c:172`) decrementa el quantum en cada tick del timer. Cuando `quantum <= 0`, activa el flag `need_reschedule`. Él `irq_handler_stub` (`entry.S:141`) verifica este flag después del IRQ y llama a `schedule()` condicionalmente. De esta forma el cambio de contexto ocurre en un punto seguro, no dentro del handler de interrupción.

### Funciones clave

| Funcion               | Archivo:Linea     | Descripcion                                                                                               |
|-----------------------|-------------------|-----------------------------------------------------------------------------------------------------------|
| `init_process_system` | `process.c:141`   | Inicializa PID 0 (Kernel/IDLE) como RUNNING                                                               |
| `create_process`      | `process.c:74`    | Crea proceso: `kmalloc(4096)` para stack, configura PCB, pc=`ret_from_fork`                               |
| `exit`                | `process.c:178`   | Marca como ZOMBIE, llama a `schedule()`                                                                   |
| `free_zombie`         | `process.c:219`   | Reaper: libera stacks con `kfree()`, marca UNUSED                                                         |
| `cpu_switch_to`       | `entry.S:93`      | Salva x19-x30+SP de prev (x0), restaura de next (x1), `ret` salta al LR de next                           |
| `ret_from_fork`       | `entry.S:169`     | Trampoln para procesos nuevos: `schedule_tail()`, `enable_interrupts()`, salta a fn en x19 con arg en x20 |
| `schedule`            | `scheduler.c:84`  | Planificador hibrido: aging + seleccion + penalizacion + quantum + cambio de contexto                     |
| `timer_tick`          | `scheduler.c:172` | Decrementa quantum, despierta procesos dormidos, activa `need_reschedule`                                 |
| `sleep`               | `scheduler.c:238` | Marca BLOCKED(SLEEP), fija `wake_up_time`, cede CPU                                                       |

### Constantes

| Constante         | Valor | Archivo:Linea |
|-------------------|-------|---------------|
| `MAX_PROCESS`     | 64    | `sched.h:80`  |
| `DEFAULT_QUANTUM` | 5     | `sched.h:81`  |

### Decisiones de diseño

- **Aging**: Cada ronda de scheduling decrementa la prioridad de todos los procesos READY, garantizando que incluso los de baja prioridad eventualmente serán elegidos. Al elegido se le penaliza con `+2` para evitar monopolización.
- **Preempcion diferida**: `timer_tick()` nunca llama a `schedule()` directamente; activa un flag que el stub de IRQ verifica después del handler. Esto evita cambios de contexto en mitad de una sección crítica.
- **PID 0 exento de quantum**: El proceso IDLE (scheduler.c:130) no recibe quantum; usa el stack de arranque.
- **Reciclaje completo**: El ciclo UNUSED -> READY -> RUNNING -> ZOMBIE -> UNUSED (via `free_zombie`) reutiliza slots del array `process[]` y libera memoria con `kfree()`.

### Test

| Test                      | Archivo:Linea | Que demuestra                                                      |
|---------------------------|---------------|--------------------------------------------------------------------|
| `test_roundrobin_quantum` | `tests.c:143` | Crea procesos que compiten por CPU; se verifica reparto equitativo |
| `tarea_egoista`           | `tests.c:199` | Proceso en bucle infinito; demuestra que el quantum lo preempta    |

---

## Tema 3: Concurrencia y Sincronización

**Conceptos cubiertos:** Exclusion mutua, spinlocks con instrucciones atómicas, semáforos con colas de espera, problema productor-consumidor, busy-wait vs. blocking.

### Archivos principales

| Archivo               | Rol                                                   |
|-----------------------|-------------------------------------------------------|
| `src/locks.S`         | Spinlocks con LDXR/STXR (ARM64 exclusives)            |
| `src/semaphore.c`     | Semaforos con wait queues (P y V de Dijkstra)         |
| `include/semaphore.h` | Declaraciones: `struct semaphore`, spin_lock/unlock   |
| `src/utils/tests.c`   | Tests de concurrencia: holder/waiter, round-robin     |
| `src/drivers/io.c`    | `console_mutex`: semaforo binario protegiendo kprintf |

### Spinlocks (bajo nivel)

Los spinlocks usan el mecanismo de **monitor exclusivo** de ARM64:

```asm
spin_lock (locks.S:37):
    1. LDXR  w1, [x0]      // Lectura exclusiva del lock
    2. CBNZ  w1, 1b         // Si ocupado (!=0), reintentar
    3. MOV   w1, #1
    4. STXR  w2, w1, [x0]   // Escritura exclusiva
    5. CBNZ  w2, 1b         // Si fallo (otro core gano), reintentar
    6. DMB   SY             // Barrera de memoria completa

spin_unlock (locks.S:58):
    1. DMB   SY             // Barrera antes de liberar
    2. STLR  wzr, [x0]     // Store-Release de 0 (libera el lock)
```

La pareja LDXR/STXR garantiza atomicidad sin necesidad de instrucciones CAS explícitas. Él `DMB SY` en la adquisición 
asegura que las operaciones de la sección crítica no se reordenen antes del lock. Él `STLR` (store-release) en la 
liberación garantiza que las escrituras de la sección crítica sean visibles antes de que el lock aparezca como 
libre.

### Semáforos (alto nivel)

```
struct semaphore {
    volatile int count;     // Contador del semaforo
    struct pcb *head;       // Cabeza de la cola FIFO
    struct pcb *tail;       // Cola de la cola FIFO
}
```

**Operación P (`sem_wait`, `semaphore.c:109`)**:
```
1. disable_interrupts()
2. spin_lock(&sem_lock)
3. if (count > 0):
      count--
      spin_unlock(); enable_interrupts(); return
4. else:
      Encolar proceso actual al final de wait queue
      Marcar BLOCKED (BLOCK_REASON_WAIT)
      spin_unlock()
      schedule()   // Cede CPU — se despertara cuando otro haga signal
5. enable_interrupts()
```

**Operación V (`sem_signal`, `semaphore.c:175`)**:
```
1. disable_interrupts()
2. spin_lock(&sem_lock)
3. if (wait queue no vacia):
      Desencolar head
      Marcar READY (direct handoff, NO incrementa count)
4. else:
      count++
5. spin_unlock()
6. enable_interrupts()
```

### Funciones clave

| Funcion       | Archivo:Linea     | Descripcion                                     |
|---------------|-------------------|-------------------------------------------------|
| `spin_lock`   | `locks.S:37`      | LDXR/STXR loop con DMB SY                       |
| `spin_unlock` | `locks.S:58`      | DMB SY + STLR wzr                               |
| `sem_init`    | `semaphore.c:78`  | Inicializa: count=value, head=NULL, tail=NULL   |
| `sem_wait`    | `semaphore.c:109` | Operacion P: decrementa o bloquea en wait queue |
| `sem_signal`  | `semaphore.c:175` | Operacion V: despierta o incrementa             |

### Patron de uso: Mutex de consola

```c
// io.c:38 — Semaforo binario que protege kprintf
static struct semaphore console_mutex;

// io.c:179 — kprintf adquiere el mutex antes de imprimir
void kprintf(const char *fmt, ...) {
    sem_wait(&console_mutex);
    // ... formatear y escribir por UART ...
    sem_signal(&console_mutex);
}
```

### Decisiones de diseño

- **Direct handoff en `sem_signal`**: Cuando hay un proceso esperando, el count NO se incrementa. El "turno" pasa directamente al proceso despertado, evitando que un tercer proceso robe el semáforo entre el signal y la readquisición.
- **Spinlock global** (`sem_lock`, `semaphore.c:67`): Un unico spinlock protege todas las operaciones de semáforo. Aceptable en un kernel educativo con pocos puntos de contención.
- **IRQ disable + spinlock**: Ambas operaciones deshabilitan interrupciones antes de adquirir el spinlock, previniendo que un timer IRQ cause preempcion durante la manipulación crítica del semáforo.

### Tests

| Test                         | Archivo:Linea | Que demuestra                                                                 |
|------------------------------|---------------|-------------------------------------------------------------------------------|
| `tarea_holder`               | `tests.c:248` | Adquiere semaforo, duerme 500 ticks, libera                                   |
| `tarea_waiter`               | `tests.c:275` | Intenta `sem_wait` sobre semaforo ocupado, se bloquea en wait queue           |
| `test_semaphores_efficiency` | `tests.c:301` | Lanza holder+waiter para demostrar sincronizacion con blocking (no busy-wait) |

---

## Tema 4: Gestion de Memoria

**Conceptos cubiertos:** Paginación multilevel, MMU (Memory Management Unit), memoria fisica vs. virtual, bitmap de páginas, asignación bajo demanda (demand paging), page faults, asignador dinámico (heap).

### Archivos principales

| Archivo               | Rol                                                |
|-----------------------|----------------------------------------------------|
| `src/mm/mm.c`         | Configuracion de la MMU: mapeo, MAIR, TCR, SCTLR   |
| `src/mm/pmm.c`        | Physical Memory Manager: bitmap de paginas libres  |
| `src/mm/vmm.c`        | Virtual Memory Manager: tablas de paginas L1/L2/L3 |
| `src/mm/malloc.c`     | Asignador dinamico `kmalloc`/`kfree` (First-Fit)   |
| `src/kernel/sys.c`    | Handler de Page Faults (`handle_fault`)            |
| `src/mm_utils.S`      | Funciones ASM para registros de la MMU             |
| `include/mm/pmm.h`    | `PAGE_SIZE`, API del PMM                           |
| `include/mm/vmm.h`    | Descriptores de pagina, flags, macros de indices   |
| `include/mm/mm.h`     | API de inicializacion de memoria                   |
| `include/mm/malloc.h` | API de `kmalloc`/`kfree`                           |

### Mapa de memoria (QEMU virt)

```
0x00000000 - 0x08000000    Perifricos (sin mapear hasta que se necesite)
0x08000000 - 0x08010000    GIC Distributor (Device memory)
0x08010000 - 0x08020000    GIC CPU Interface (Device memory)
0x09000000 - 0x09001000    UART PL011 (Device memory)
0x40000000 - _end          Imagen del kernel (codigo + datos + BSS)
_end       - _end+64MB     HEAP del kernel (kmalloc/kfree)
_end+64MB  - 0x48000000    PMM: paginas libres gestionadas por bitmap
0x50000000+                 Demand Paging: se mapean paginas bajo demanda
```

### Tablas de páginas (3 niveles)

```
Virtual Address (39 bits):
+--------+--------+--------+--------+
| L1[8:0]| L2[8:0]| L3[8:0]|Offset  |
| bits   | bits   | bits   |[11:0]  |
| 38-30  | 29-21  | 20-12  | 11-0   |
+--------+--------+--------+--------+
   |          |         |
   v          v         v
kernel_pgd   L2 table  L3 table --> Pagina fisica (4KB)
 (512 ent)  (512 ent) (512 ent)
```

| Macro          | Archivo:Linea | Descripcion       |
|----------------|---------------|-------------------|
| `L1_INDEX(va)` | `vmm.h:93`    | Extrae bits 38-30 |
| `L2_INDEX(va)` | `vmm.h:94`    | Extrae bits 29-21 |
| `L3_INDEX(va)` | `vmm.h:95`    | Extrae bits 20-12 |

### PMM: Bitmap de paginas

El PMM gestiona 128MB de RAM mediante un bitmap donde cada bit representa una página de 4KB:

```
mem_map[4096 bytes] = 32768 bits = 32768 paginas = 128MB
                         ^
                         |
                    1 = ocupada
                    0 = libre
```

| Funcion         | Archivo:Linea | Descripcion                                                          |
|-----------------|---------------|----------------------------------------------------------------------|
| `pmm_init`      | `pmm.c:38`    | Limpia bitmap a ceros (todas libres)                                 |
| `get_free_page` | `pmm.c:64`    | First-Fit: busca primer bit en 0, marca como 1, zeroing de seguridad |
| `free_page`     | `pmm.c:92`    | Pone el bit correspondiente a 0                                      |

### VMM: map_page (3 niveles)

La función `map_page` (`vmm.c:63`) implementa el page walk completo:

```
map_page(pgd, va, pa, flags):
  1. idx1 = L1_INDEX(va)
  2. if pgd[idx1] no existe:
        l2 = get_free_page()    // Asigna tabla L2
        pgd[idx1] = l2 | PT_TABLE
  3. idx2 = L2_INDEX(va)
  4. if l2[idx2] no existe:
        l3 = get_free_page()    // Asigna tabla L3
        l2[idx2] = l3 | PT_TABLE
  5. idx3 = L3_INDEX(va)
  6. l3[idx3] = pa | PT_PAGE | MM_ACCESS | flags
```

### Demand Paging

Cuando un proceso accede a una dirección virtual no mapeada, la MMU genera un **Data Abort** que es capturado por `handle_fault` (`sys.c:154`):

```
Page Fault (Data Abort):
  1. Leer FAR_EL1 (direccion que causo el fallo)
  2. Leer ESR_EL1 (causa: EC=0x24 kernel / EC=0x25 user)
  3. page = get_free_page()         // Asignar pagina fisica
  4. map_page(pgd, fault_addr, page, flags)  // Mapear
  5. tlb_invalidate_all()           // Invalidar TLB
  6. return                         // CPU reintenta instruccion
```

| Funcion              | Archivo:Linea    | Descripcion                                     |
|----------------------|------------------|-------------------------------------------------|
| `handle_fault`       | `sys.c:154`      | Lee FAR/ESR, asigna pagina, mapea, invalida TLB |
| `tlb_invalidate_all` | `mm_utils.S:139` | DSB ISH + TLBI VMALLE1IS + DSB ISH + ISB        |

### Asignador dinámico (Heap)

`kmalloc`/`kfree` implementan un asignador First-Fit con lista enlazada:

```
struct block_header {       // malloc.c:22
    unsigned long size;     // Tamano de DATOS (sin header)
    struct block_header *next;
    uint8_t is_free;
    uint8_t padding[7];     // Alineacion a 16 bytes (ARM64)
};
```

| Funcion      | Archivo:Linea  | Descripcion                                          |
|--------------|----------------|------------------------------------------------------|
| `kheap_init` | `malloc.c:40`  | Alinea inicio a 16 bytes, crea bloque unico libre    |
| `kmalloc`    | `malloc.c:62`  | First-Fit con splitting; zeroing de memoria asignada |
| `kfree`      | `malloc.c:118` | Marca libre, coalescing hacia adelante               |

### Configuración de la MMU

| Funcion              | Archivo:Linea    | Descripcion                                                                                   |
|----------------------|------------------|-----------------------------------------------------------------------------------------------|
| `init_memory_system` | `mm.c:116`       | Orquestador: calcula regiones, inicializa PMM+VMM+MMU+Heap                                    |
| `mem_init`           | `mm.c:70`        | Mapea perifericos (Device) y 128MB RAM (Normal), configura MAIR/TCR/TTBR, activa MMU y caches |
| `init_vmm`           | `vmm.c:129`      | Limpia `kernel_pgd[512]`                                                                      |
| `set_mair_el1`       | `mm_utils.S:39`  | MSR MAIR_EL1 + ISB                                                                            |
| `set_tcr_el1`        | `mm_utils.S:54`  | MSR TCR_EL1 + ISB                                                                             |
| `set_ttbr0_el1`      | `mm_utils.S:69`  | MSR TTBR0_EL1 + ISB                                                                           |
| `set_sctlr_el1`      | `mm_utils.S:112` | MSR SCTLR_EL1 + ISB (activa MMU, D-Cache, I-Cache)                                            |

### Flags de pagina

| Flag        | Valor | Archivo:Linea | Descripcion                                     |
|-------------|-------|---------------|-------------------------------------------------|
| `PT_TABLE`  | 3     | `vmm.h:42`    | Descriptor de tabla (apunta al siguiente nivel) |
| `PT_PAGE`   | 3     | `vmm.h:43`    | Descriptor de pagina (entrada final L3)         |
| `MM_ACCESS` | 1<<10 | `vmm.h:57`    | Access Flag                                     |
| `MM_SH`     | 3<<8  | `vmm.h:58`    | Inner Shareable                                 |
| `MM_RW`     | 0<<7  | `vmm.h:59`    | Lectura/Escritura                               |
| `MM_USER`   | 1<<6  | `vmm.h:60`    | Accesible desde EL0                             |
| `MM_KERNEL` | 0<<6  | `vmm.h:61`    | Solo kernel (EL1)                               |

### Decisiones de diseño

- **Identity mapping**: Toda la RAM (0x40000000-0x47FFFFFF) se mapea 1:1 (`mm.c:84-86`), simplificando la traducción a costa de flexibilidad.
- **Asignación lazy via demand paging**: Las páginas no sé pre-asignan; acceder a direcciones no mapeadas genera un page fault controlado que mapea la página bajo demanda.
- **Flags según privilegio**: `handle_fault` (`sys.c:183-184`) distingue EC=0x24 (kernel) vs EC=0x25 (user) para aplicar `MM_KERNEL` o `MM_USER`.
- **Security zeroing**: Tanto `get_free_page` (`pmm.c:79`) como `kmalloc` (`malloc.c:101`) llenan con ceros antes de devolver.
- **Coalescing solo hacia adelante**: `kfree` (`malloc.c:128-134`) solo fusiona con el bloque siguiente; el coalescing hacia atrás requeriría lista doblemente enlazada.

### Test

| Test          | Archivo:Linea | Que demuestra                                                                          |
|---------------|---------------|----------------------------------------------------------------------------------------|
| `test_demand` | `tests.c:340` | Escribe en 0x50000000 (no mapeado), provoca page fault, verifica que el valor persiste |

---

## Tema 5: Sistemas de Archivos

**Conceptos cubiertos:** Inodos, superbloque, file descriptors, operaciones sobre archivos (create, open, read, write, close, remove), VFS (Virtual File System), sistema de archivos en memoria (RamFS).

### Archivos principales

| Archivo             | Rol                                                           |
|---------------------|---------------------------------------------------------------|
| `include/fs/vfs.h`  | Estructuras: `inode_t`, `superblock_t`, `file_t`; API publica |
| `src/fs/ramfs.c`    | Implementacion completa del RamFS                             |
| `src/shell/shell.c` | Integracion con el shell (5 comandos de filesystem)           |

### Arquitectura del RamFS

```
superblock_t (ram_disk)
  |
  |-- total_size: tamano del RamDisk
  |-- free_inodes: inodos disponibles
  |-- start_addr: base en RAM
  |-- inodes[64]: tabla de inodos
        |
        |-- inode[0]: {id=0, name="readme.txt", data_ptr=start+0*4096, ...}
        |-- inode[1]: {id=1, name="config.sys", data_ptr=start+1*4096, ...}
        |-- inode[2]: {id=2, is_used=0, ...}  (libre)
        |-- ...

fd_table[64]: tabla de file descriptors
  |
  |-- fd[0]: {inode=&inodes[0], position=0, flags=...}
  |-- fd[1]: NULL (libre)
  |-- ...
```

### Estructuras

| Estructura     | Archivo:Linea | Campos                                                                  |
|----------------|---------------|-------------------------------------------------------------------------|
| `inode_t`      | `vfs.h:20`    | `id`, `type`, `size`, `data_ptr` (unsigned long), `name[32]`, `is_used` |
| `superblock_t` | `vfs.h:32`    | `total_size`, `free_inodes`, `start_addr`, `inodes[MAX_FILES]`          |
| `file_t`       | `vfs.h:42`    | `*inode`, `position`, `flags`                                           |

### Constantes

| Constante       | Valor | Archivo:Linea |
|-----------------|-------|---------------|
| `MAX_FILES`     | 64    | `vfs.h:9`     |
| `FILE_NAME_LEN` | 32    | `vfs.h:10`    |
| `MAX_FILE_SIZE` | 4096  | `vfs.h:11`    |
| `FS_FILE`       | 1     | `vfs.h:14`    |
| `FS_DIRECTORY`  | 2     | `vfs.h:15`    |

### Operaciones del VFS

| Operacion | Funcion      | Archivo:Linea | Descripcion                                           |
|-----------|--------------|---------------|-------------------------------------------------------|
| Formatear | `ramfs_init` | `ramfs.c:36`  | Inicializa superbloque, pre-asigna data_ptr por inodo |
| Crear     | `vfs_create` | `ramfs.c:62`  | Busca duplicados, asigna inodo libre, copia nombre    |
| Abrir     | `vfs_open`   | `ramfs.c:126` | Busca inodo por nombre, asigna FD en fd_table         |
| Escribir  | `vfs_write`  | `ramfs.c:156` | Copia buf a data_ptr+position, actualiza size         |
| Leer      | `vfs_read`   | `ramfs.c:186` | Copia data_ptr+position a buf, avanza position        |
| Cerrar    | `vfs_close`  | `ramfs.c:211` | Libera FD (inode=NULL, position=0)                    |
| Eliminar  | `vfs_remove` | `ramfs.c:223` | Marca inodo libre, zeroing de nombre y datos          |
| Listar    | `vfs_ls`     | `ramfs.c:101` | Itera inodos, imprime ID/Size/Name de los usados      |

### Integración con el Shell

| Comando           | Linea en shell.c | Operaciones VFS                          |
|-------------------|------------------|------------------------------------------|
| `touch [archivo]` | `shell.c:161`    | `vfs_create(arg)`                        |
| `rm [archivo]`    | `shell.c:165`    | `vfs_remove(arg)`                        |
| `ls`              | `shell.c:158`    | `vfs_ls()`                               |
| `cat [archivo]`   | `shell.c:169`    | `vfs_open` -> `vfs_read` -> `vfs_close`  |
| `write [archivo]` | `shell.c:182`    | `vfs_open` -> `vfs_write` -> `vfs_close` |

### Decisiones de diseño

- **Asignación de bloques estática**: Cada Inodo recibe exactamente 1 página de 4KB pre-asignada (`ramfs.c:51`). Limita archivos a 4KB pero simplifica enormemente la implementación.
- **Directorio plano**: No hay subdirectorios; todos los archivos viven en un namespace unico buscado linealmente.
- **Security zeroing al borrar**: `vfs_remove` (`ramfs.c:234`) llena con ceros tanto el nombre como los 4KB de datos.
- **Sin capa de abstracción real**: Las funciones `vfs_*` llaman directamente al RamFS (no hay tabla de punteros a función para multiples backends). La nomenclatura VFS es pedagógica.

---

## Tema 6: Entrada/Salida y Drivers

**Conceptos cubiertos:** Drivers de dispositivo, MMIO (Memory-Mapped I/O), UART (comunicación serial), controlador de interrupciones (GIC), timer hardware, tabla de vectores de excepción, interrupciones vs. polling, buffer circular.

### Archivos principales

| Archivo                   | Rol                                           |
|---------------------------|-----------------------------------------------|
| `src/drivers/io.c`        | Driver UART PL011: envio, recepcion, kprintf  |
| `src/drivers/timer.c`     | Configuracion de GIC v2 + Timer fisico ARM64  |
| `src/vectors.S`           | Tabla de vectores de excepcion (16 entradas)  |
| `src/utils.S`             | Funciones MMIO (`put32`/`get32`), timer, VBAR |
| `src/entry.S`             | IRQ stub, enable/disable interrupts           |
| `include/drivers/io.h`    | API publica de I/O                            |
| `include/drivers/timer.h` | Registros GIC, constantes del timer           |

### UART PL011 (Comunicación Serial)

El driver de UART opera sobre la dirección MMIO 0x09000000 (PL011 en QEMU virt):

| Registro     | Direccion  | Archivo:Linea | Uso                               |
|--------------|------------|---------------|-----------------------------------|
| `UART0_DR`   | 0x09000000 | `io.c:32`     | Data Register (lectura/escritura) |
| `UART0_FR`   | 0x09000018 | `io.c:33`     | Flag Register (bit 4 = RXFE)      |
| `UART0_IMSC` | 0x09000038 | `io.c:34`     | Interrupt Mask Set/Clear          |
| `UART0_ICR`  | 0x09000044 | `io.c:35`     | Interrupt Clear Register          |

**Salida**: `uart_putc` (`io.c:45`) escribe directamente al Data Register.

**Entrada (interrupt-driven)**:
```
uart_irq_init (io.c:78)
  |-- Activa bit RXIM (bit 4) en UART0_IMSC
  
UART IRQ (ID 33, GIC):
  |-- handle_timer_irq detecta ID=33
  |-- Llama a uart_handle_irq (io.c:91)
  |     |-- Mientras FIFO no vacia (UART0_FR bit 4 = 0):
  |     |     Lee caracter de UART0_DR
  |     |     Lo almacena en kb_buffer[kb_head++]
  |     |-- Limpia interrupcion en UART0_ICR
  
Shell lee con uart_getc_nonblocking (io.c:119)
  |-- Si kb_tail != kb_head: retorna kb_buffer[kb_tail++]
  |-- Si vacio: retorna 0
```

El buffer circular (`kb_buffer[128]`, `io.c:63`) implementa un patron productor-consumidor:
- **Productor**: ISR de UART (escribe en `kb_head`)
- **Consumidor**: Shell (lee desde `kb_tail`)

### GIC v2 (Generic Interrupt Controller)

```
                    +------------------+
                    |   GIC Distributor |  0x08000000
                    |   (GICD)          |
                    +--------+---------+
                             |
                    IRQ 30 (Timer)
                    IRQ 33 (UART)
                             |
                    +--------+---------+
                    |  GIC CPU Interface|  0x08010000
                    |  (GICC)           |
                    +--------+---------+
                             |
                             v
                         CPU Core 0
```

Configuración en `timer_init` (`timer.c:67`):

| Paso | Registro        | Valor     | Descripcion                    |
|------|-----------------|-----------|--------------------------------|
| 1    | VBAR_EL1        | `vectors` | Base de la tabla de vectores   |
| 2    | GICD_CTLR       | 1         | Activa el distribuidor         |
| 3    | GICD_ISENABLER0 | bit 30    | Habilita IRQ 30 (timer)        |
| 4    | GICD_ISENABLER1 | bit 1     | Habilita IRQ 33 (UART)         |
| 5    | GICC_CTLR       | 1         | Activa interfaz de CPU         |
| 6    | GICC_PMR        | 0xFF      | Acepta todas las prioridades   |
| 7    | CNTP_TVAL_EL0   | 2000000   | Carga timer (~104ms @ 19.2MHz) |
| 8    | CNTP_CTL_EL0    | 1         | Activa timer fisico            |
| 9    | DAIFCLR         | #2        | Habilita IRQs en el CPU        |

### Tabla de Vectores de Excepción

La tabla (`vectors.S:70`) tiene 16 entradas organizadas en 4 grupos de 4, cada entrada alineada a 128 bytes:

| Grupo               | Offset       | Excepcion | Handler                |
|---------------------|--------------|-----------|------------------------|
| 1 (SP_EL0)          | +0x000       | Sync      | `hang`                 |
| 1                   | +0x080       | IRQ       | `hang`                 |
| 1                   | +0x100       | FIQ       | `hang`                 |
| 1                   | +0x180       | SError    | `hang`                 |
| **2 (EL1h)**        | **+0x200**   | **Sync**  | **`el1_sync`**         |
| **2**               | **+0x280**   | **IRQ**   | **`irq_handler_stub`** |
| 2                   | +0x300       | FIQ       | `hang`                 |
| 2                   | +0x380       | SError    | `hang`                 |
| **3 (EL0, 64-bit)** | **+0x400**   | **Sync**  | **`el0_sync`**         |
| **3**               | **+0x480**   | **IRQ**   | **`el0_irq`**          |
| 3                   | +0x500       | FIQ       | `hang`                 |
| 3                   | +0x580       | SError    | `hang`                 |
| 4 (EL0, 32-bit)     | +0x600-0x780 | *         | `hang`                 |

### Flujo de una interrupción de timer

```
Timer expira (CNTP_TVAL_EL0 llega a 0)
  |
  v
GIC senaliza IRQ al CPU
  |
  v
CPU salta a VBAR_EL1 + 0x280 (vectors.S:79)
  |
  v
irq_handler_stub (entry.S:141):
  1. kernel_entry (salva todos los registros)
  2. bl handle_timer_irq
  3. bl is_reschedule_pending
  4. if (x0 != 0): bl schedule
  5. kernel_exit (restaura registros + ERET)
  
handle_timer_irq (timer.c:106):
  1. Lee GICC_IAR -> interrupt_id
  2. if (id == 30):  // Timer
       Recarga CNTP_TVAL_EL0
       timer_tick()   // Decrementa quantum
       schedule()
  3. if (id == 33):  // UART
       uart_handle_irq()
  4. Escribe GICC_EOIR (End of Interrupt)
```

### Funciones clave

| Funcion                 | Archivo:Linea | Descripcion                                   |
|-------------------------|---------------|-----------------------------------------------|
| `uart_putc`             | `io.c:45`     | Escribe un caracter al UART                   |
| `uart_puts`             | `io.c:53`     | Escribe string via loop de `uart_putc`        |
| `uart_irq_init`         | `io.c:78`     | Activa interrupcion RX del UART               |
| `uart_handle_irq`       | `io.c:91`     | ISR: drena FIFO a buffer circular             |
| `uart_getc_nonblocking` | `io.c:119`    | Lee del buffer circular (no bloqueante)       |
| `kprintf`               | `io.c:179`    | Printf del kernel (%c,%s,%d,%x) con mutex     |
| `timer_init`            | `timer.c:67`  | Configura GIC + Timer + UART IRQ              |
| `handle_timer_irq`      | `timer.c:106` | Demultiplexor de IRQs: timer (30) y UART (33) |
| `put32`                 | `utils.S:50`  | MMIO write: STR w1, [x0]                      |
| `get32`                 | `utils.S:63`  | MMIO read: LDR w0, [x0]                       |
| `set_vbar_el1`          | `utils.S:105` | Establece base de vectores de excepcion       |
| `enable_interrupts`     | `entry.S:121` | MSR DAIFCLR, #2                               |
| `disable_interrupts`    | `entry.S:130` | MSR DAIFSET, #2                               |
| `system_off`            | `utils.S:125` | Apagado via semihosting ARM (HLT 0xF000)      |

### Decisiones de diseño

- **Demultiplexor unico**: `handle_timer_irq` (`timer.c:106`) lee GICC_IAR y despacha según ID de interrupción. Evita un dispatcher separado.
- **Teclado interrupt-driven con buffer circular**: En lugar de polling, se usa IRQ 33 con productor (ISR) y consumidor (shell), permitiendo que el shell duerma eficientemente (`sleep(1)` en `shell.c:74`).
- **ERET para restauración**: Tanto el stub de IRQ como los handlers de excepciones síncronas usan `kernel_exit` (`entry.S:58`) que restaura SPSR_EL1 y ELR_EL1 antes de ERET.
- **Semihosting para apagado**: `system_off` (`utils.S:125`) usa la interfaz de semihosting ARM (HLT 0xF000 con operacion 0x18=SYS_EXIT) para terminar QEMU limpiamente.

---

## Referencia cruzada

### Archivos por Tema

| Archivo                  | T1 | T2 | T3 | T4 | T5 | T6 |
|--------------------------|:--:|:--:|:--:|:--:|:--:|:--:|
| `src/boot.S`             | X  |    |    |    |    |    |
| `src/entry.S`            | X  | X  |    |    |    | X  |
| `src/vectors.S`          | X  |    |    |    |    | X  |
| `src/locks.S`            |    |    | X  |    |    |    |
| `src/utils.S`            |    |    |    |    |    | X  |
| `src/mm_utils.S`         |    |    |    | X  |    |    |
| `src/kernel/kernel.c`    | X  |    |    |    |    |    |
| `src/kernel/sys.c`       | X  |    |    | X  |    |    |
| `src/kernel/process.c`   |    | X  |    |    |    |    |
| `src/kernel/scheduler.c` |    | X  |    |    |    |    |
| `src/semaphore.c`        |    |    | X  |    |    |    |
| `src/mm/mm.c`            |    |    |    | X  |    |    |
| `src/mm/pmm.c`           |    |    |    | X  |    |    |
| `src/mm/vmm.c`           |    |    |    | X  |    |    |
| `src/mm/malloc.c`        |    |    |    | X  |    |    |
| `src/fs/ramfs.c`         |    |    |    |    | X  |    |
| `src/drivers/io.c`       |    |    |    |    |    | X  |
| `src/drivers/timer.c`    |    |    |    |    |    | X  |
| `src/shell/shell.c`      |    |    |    |    | X  |    |
| `src/utils/tests.c`      |    | X  | X  | X  |    |    |
| `link.ld`                | X  |    |    |    |    |    |

### Temas por Archivo

| Tema                     | Archivos fuente                                                        |
|--------------------------|------------------------------------------------------------------------|
| **T1**: Intro / Syscalls | `boot.S`, `entry.S`, `vectors.S`, `kernel.c`, `sys.c`, `link.ld`       |
| **T2**: Procesos         | `sched.h`, `process.c`, `scheduler.c`, `entry.S`, `tests.c`            |
| **T3**: Concurrencia     | `locks.S`, `semaphore.c`, `semaphore.h`, `tests.c`, `io.c` (mutex)     |
| **T4**: Memoria          | `mm.c`, `pmm.c`, `vmm.c`, `malloc.c`, `sys.c`, `mm_utils.S`, `tests.c` |
| **T5**: Archivos         | `vfs.h`, `ramfs.c`, `shell.c`                                          |
| **T6**: E/S y Drivers    | `io.c`, `timer.c`, `vectors.S`, `utils.S`, `entry.S`                   |
