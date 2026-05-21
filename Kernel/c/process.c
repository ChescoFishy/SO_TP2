#include "process.h"
#include "scheduler.h"
#include "memoryManager.h"
#include "lib.h"
#include "pipe.h"
#include <stddef.h>

/* Tabla de procesos del Kernel. */
static PCB process_table[MAX_PROCESSES];

static uint64_t next_pid = 0;
static PCB* current_process = NULL;
static uint64_t fg_pid = 0;   /* ultimo proceso foreground creado y vivo */

// ─── helpers ─────────────────────────────────────────────────────────────────

static int str_len(const char *s){

    int n = 0;
    while(s && s[n]){ 
        n++;
    }

    return n;
}

static void str_copy(char *dst, const char *src, int max){
    int i = 0;
    
    while(i < max - 1 && src && src[i]){ 
        dst[i] = src[i];
        i++; 
    }
    
    dst[i] = '\0';
}

// ─── stack frame inicial ──────────────────────────────────────────────────────
//
// Layout del stack tras una interrupcion en este kernel (ring 0):
//
//   [rsp+17*8] RFLAGS          <- empujado por CPU (parte alta del frame)
//   [rsp+16*8] CS
//   [rsp+15*8] RIP
//   [rsp+14*8] RAX  (primero en pushState)
//   [rsp+13*8] RBX
//   ...
//   [rsp+ 0*8] R15  (ultimo en pushState, primer pop)
//
// Para que popState+iretq arranquen un proceso nuevo, construimos exactamente
// este layout desde la cima del stack hacia abajo.


/* 
** Construye el frame inicial del stack. 
** Prepara el mecanismo para el context switching.
*/
static uint64_t* build_initial_stack(void *stack_top, ProcessEntry entry, int argc, char **argv){
    uint64_t* sp = (uint64_t *)stack_top;

    /* Hardware frame. Instruccion iretq espera encontrar estos valores
    ** cuando retorna de una interrupcion o excepcion. */
    *(--sp) = 0x202;               /* RFLAGS: Interrupt Flag = 1 */
    *(--sp) = 0x08;                /* CS: segmento de codigo del kernel */ 
    *(--sp) = (uint64_t)entry;     /* RIP: punto de entrada. Asegura que iretq transiciona al segmento de codigo correcto. */
    

    /* Registros General Purpose en orden de pushState */
    *(--sp) = 0;                   /* RAX */
    *(--sp) = 0;                   /* RBX */
    *(--sp) = 0;                   /* RCX */
    *(--sp) = 0;                   /* RDX */
    *(--sp) = 0;                   /* RBP */
    *(--sp) = (uint64_t)argc;      /* RDI → arg1 de la funcion de entrada */
    *(--sp) = (uint64_t)argv;      /* RSI → arg2 de la funcion de entrada */
    *(--sp) = 0;                   /* R8 */
    *(--sp) = 0;                   /* R9 */
    *(--sp) = 0;                   /* R10 */
    *(--sp) = 0;                   /* R11 */
    *(--sp) = 0;                   /* R12 */
    *(--sp) = 0;                   /* R13 */
    *(--sp) = 0;                   /* R14 */
    *(--sp) = 0;                   /* R15 (ultimo en pushState → primer pop) */

    return sp;   /* PCB.rsp apunta aqui*/ 
}

// ─── API publica ──────────────────────────────────────────────────────────────

/* Inicializa la tabla de procesos. */
void process_init(void){
    memset(process_table, 0, sizeof(process_table));
    next_pid = 0;
    current_process = NULL;
}

/* Devuelve el proceso actual. */
PCB* process_current(void){
    return current_process;
}

/* Establece el proceso actual. */
void process_set_current(PCB* p){
    current_process = p;
}

/* Devuelve el proceso con el PID especificado. */
PCB* process_get(uint64_t pid){

    if(pid == 0){
        return NULL;
    }

    for(int i = 0; i < MAX_PROCESSES; i++){
        if(process_table[i].pid == pid && process_table[i].state != PROCESS_FREE){
            return &process_table[i];
        }
    }

    return NULL;
}

/* Crea un nuevo proceso con stdin/stdout customizables. */
int process_create_fd(const char *name, ProcessEntry entry, int argc, char **argv,
                      uint8_t fg, int fd_in, int fd_out){

    /* Buscar slot libre. */
    int slot = -1;
    for(int i = 0; i < MAX_PROCESSES && slot == -1; i++){
        if(process_table[i].state == PROCESS_FREE){
            slot = i;
        }
    }

    if(slot < 0){
        return -1;
    }

    PCB* p = &process_table[slot];

    p->stack_base = (uint64_t *)mm_malloc_kernel(STACK_SIZE);
    if(p->stack_base == NULL){
        return -1;
    }

    void* stack_top = (void*)((uint8_t *)p->stack_base + STACK_SIZE);
    p->rsp = build_initial_stack(stack_top, entry, argc, argv);

    p->pid = ++next_pid;
    p->priority = DEFAULT_PRIORITY;
    p->remaining_quanta = DEFAULT_PRIORITY;
    p->state = PROCESS_READY;
    p->foreground = fg;
    p->fd[0] = fd_in;
    p->fd[1] = fd_out;
    /* Heredar refcount de pipes: el padre creó/abrió el pipe y le pasó el fd
    ** al hijo. Bumpeamos para que el cierre del padre no libere el pipe antes
    ** de que el hijo termine de usarlo. */
    if(pipe_is_fd(fd_in))  pipe_inherit_fd(fd_in);
    if(pipe_is_fd(fd_out)) pipe_inherit_fd(fd_out);
    p->parent_pid = ((current_process != NULL) ? (current_process->pid) : 0);
    p->waiting_for = 0;
    p->argc = argc;
    p->argv = argv;
    p->retval = 0;

    str_copy(p->name, name, MAX_NAME_LEN);

    if(str_len(p->name) == 0){
        p->name[0] = '?';
        p->name[1] = '\0';
    }

    /* Tracking del PID foreground actual para Ctrl+C. Solo cuentan procesos
    ** creados desde otro proceso (la shell): los del kernel init no deben
    ** quedar registrados como foreground (sino Ctrl+C mataria la shell). */
    if(fg && current_process != NULL){
        fg_pid = p->pid;
    }

    scheduler_add(p);
    return (int)p->pid;
}

uint64_t process_get_foreground(void){
    PCB *p = process_get(fg_pid);
    if(p == NULL || p->state == PROCESS_FREE || p->state == PROCESS_ZOMBIE){
        return 0;
    }
    return fg_pid;
}

/* Crea un nuevo proceso con stdin=teclado / stdout=consola por defecto. */
int process_create(const char *name, ProcessEntry entry, int argc, char **argv, uint8_t fg){
    return process_create_fd(name, entry, argc, argv, fg, 0, 1);
}

/* Cierra cualquier fd que sea pipe en el PCB y los resetea a -1
** para evitar dobles cierres si la rutina se invoca dos veces sobre el mismo PCB. */
static void close_process_pipes(PCB *p){
    for(int i = 0; i < 2; i++){
        if(pipe_is_fd(p->fd[i])){
            pipe_close(p->fd[i]);
            p->fd[i] = -1;
        }
    }
}

/* parent->rsp apunta al slot R15; el slot RAX esta 14 qwords mas arriba.
** Sign-extender retval a 64 bits para que int64_t -1 se preserve en userland. */
static void wake_waiting_parent(PCB *p){
    PCB *parent = process_get(p->parent_pid);
    if(parent != NULL && parent->state == PROCESS_BLOCKED && parent->waiting_for == p->pid){
        parent->rsp[14] = (uint64_t)(int64_t)p->retval;
        parent->waiting_for = 0;
        parent->state = PROCESS_READY;
    }
}

/* Limpieza comun a exit y kill: pipes, fg_pid, retval, despertar padre. */
static void release_pcb_resources(PCB *p, int retval){
    close_process_pipes(p);
    if(p->pid == fg_pid) fg_pid = 0;
    p->retval = retval;
    wake_waiting_parent(p);
}

void process_exit(int retval){
    if(current_process == NULL) return;

    release_pcb_resources(current_process, retval);

    /* El slot queda como ZOMBIE hasta que el padre haga waitpid (recoge retval).
    ** El stack si lo liberamos ya: nadie mas lo va a usar. */
    current_process->state = PROCESS_ZOMBIE;
    mm_free(current_process->stack_base);
    current_process->stack_base = NULL;
    current_process->rsp = NULL;

    force_switch = 1;
}

void process_kill(uint64_t pid){
    PCB* p = process_get(pid);
    if(p == NULL || p->state == PROCESS_FREE || p->state == PROCESS_ZOMBIE){
        return;
    }

    release_pcb_resources(p, -1);

    if(p == current_process){
        /* Se quiere matar el mismo. */
        current_process->state = PROCESS_ZOMBIE;
        mm_free(current_process->stack_base);
        current_process->stack_base = NULL;
        current_process->rsp = NULL;
        /* Fuerza el switch para seguir con otro proceso. */
        force_switch = 1;       
    }else{
        /* Matar otro proceso: liberar recursos inmediatamente y removerlo del scheduler. 
        ** No es necesario marcarlo como PROCESS_ZOMBIE porque el proceso actual no esta esperando por el.
        ** Entonces no hay riesgo si hago scheduler_remove y libero el slot del proceso.
        */ 
        scheduler_remove(p);
        if(p->stack_base != NULL){
            mm_free(p->stack_base);
            p->stack_base = NULL;
        }
        p->state = PROCESS_FREE;
        p->pid = 0;
    }
}

/* Bloquea el proceso con el PID especificado. */
void process_block(uint64_t pid){
    PCB* p = process_get(pid);

    if(p == NULL || p->state == PROCESS_FREE || p->state == PROCESS_ZOMBIE || p->state == PROCESS_BLOCKED){
        return;
    }

    p->state = PROCESS_BLOCKED;

    /* Si el proceso bloqueado es el actual, forzar cambio de contexto. */
    if(p == current_process){
        force_switch = 1;
    }
}

/* Desbloquea el proceso con el PID especificado. */
void process_unblock(uint64_t pid){
    PCB* p = process_get(pid);

    if(p == NULL || p->state != PROCESS_BLOCKED){
        return;
    }

    p->state = PROCESS_READY;
}

/* Cambia la prioridad del proceso con el PID especificado. */
void process_nice(uint64_t pid, uint8_t new_priority){
    PCB* p = process_get(pid);

    if(p == NULL){
        return;
    }
    
    if(new_priority < MIN_PRIORITY){
        new_priority = MIN_PRIORITY;
    }
    
    if(new_priority > MAX_PRIORITY){
        new_priority = MAX_PRIORITY;
    }
    
    p->priority = new_priority;

    /* Actualizar remaining_quanta incondicionalmente: si se sube la prioridad el
       proceso recibe un quantum completo en el proximo tick; si se baja, se corta
       el quantum actual para que no supere la nueva prioridad. */
    p->remaining_quanta = new_priority;
}

/* Espera a que un proceso hijo termine. */
int process_waitpid(uint64_t pid){
    PCB* child = process_get(pid);
    if(child == NULL){
        return -1;
    }

    if(child->state == PROCESS_ZOMBIE){
        /* Proceso hijo ya termino. */

        int retval = child->retval;

        /* Reclama el proceso de la tabla de procesos. */
        child->state = PROCESS_FREE;
        child->pid = 0;

        return retval;
    }

    if(current_process != NULL){
        /* Hijo todavia vivo: bloquear y esperar */
        current_process->state = PROCESS_BLOCKED;
        current_process->waiting_for = pid;
    }

    force_switch = 1;
    // El retval real sera escrito por process_exit en rsp[14].
    return 0;
}

/* Obtiene información sobre los procesos en ejecución. */
uint64_t process_ps(ProcessInfo *buf, uint64_t max){
    uint64_t count = 0;

    for(int i = 0; i < MAX_PROCESSES && count < max; i++){
        PCB* p = &process_table[i];

        if(p->state != PROCESS_FREE){
            buf[count].pid = p->pid;
            buf[count].priority = p->priority;
            buf[count].state = (uint8_t)p->state;
            buf[count].foreground = p->foreground;
            buf[count].rsp = (uint64_t)p->rsp;
            str_copy(buf[count].name, p->name, MAX_NAME_LEN);
            count++;
        }
    }

    return count;
}
