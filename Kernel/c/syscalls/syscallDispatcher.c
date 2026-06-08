#include "syscalls/syscallDispatcher.h"
#include "drivers/videoDriver.h"
#include "drivers/keyboardDriver.h"
#include "lib/lib.h"
#include "lib/defs.h"
#include "time/time.h"
#include "sound/sound.h"
#include "memoryManager/memoryManager.h"
#include "process/process.h"
#include "process/scheduler.h"
#include "ipc/semaphore.h"
#include "ipc/pipe.h"

// ─── Syscalls de memoria (16-18) ──────────────────────────────────────────────

uint64_t sys_malloc(uint64_t size) {
    return (uint64_t)mm_malloc(size);
}

void sys_free(uint64_t ptr) {
    mm_free((void *)ptr);
}

void sys_mem_status(uint64_t statusPtr) {
    mm_status((MemStatus *)statusPtr);
}

// ─── Syscalls de video/teclado ────────────────────────────────────────────────

void sys_increase_fontsize(void){
    increaseFontSize();
}
void sys_decrease_fontsize(void){
    decreaseFontSize();
}

void sys_clear(void){
    clearScreen(0x000000);
}

/* sys_write: el fd logico (0=stdin, 1=stdout) se mapea a traves del PCB.
** Si el fd fisico es un pipe, se redirige; si no, va a la consola. */
uint64_t sys_write(uint64_t fd, const char * buff, uint64_t count){
    PCB *cur = process_current();
    int phys_fd = 1;  /* default: consola */
    if (cur != NULL && fd < 2) {
        phys_fd = cur->fd[fd];
    }

    if (pipe_is_fd(phys_fd)) {
        int64_t n = pipe_write(phys_fd, buff, count);
        return (n < 0) ? (uint64_t)-1 : (uint64_t)n;
    }

    uint32_t color = 0xFFFFFF;
    for(uint64_t i = 0; i < count; i++){
        videoPutChar((uint8_t)buff[i], color);
    }
    return count;
}

/* sys_write_color: igual que sys_write pero permite elegir el color del texto en
** consola. Si el fd fisico es un pipe el color se ignora (los bytes viajan crudos),
** asi un comando coloreado (ej. red) sigue funcionando encadenado por pipe. */
uint64_t sys_write_color(uint64_t fd, const char * buff, uint64_t count, uint32_t color){
    PCB *cur = process_current();
    int phys_fd = 1;  /* default: consola */
    if (cur != NULL && fd < 2) {
        phys_fd = cur->fd[fd];
    }

    if (pipe_is_fd(phys_fd)) {
        int64_t n = pipe_write(phys_fd, buff, count);
        return (n < 0) ? (uint64_t)-1 : (uint64_t)n;
    }

    for(uint64_t i = 0; i < count; i++){
        videoPutChar((uint8_t)buff[i], color);
    }
    return count;
}

/* Valor de retorno de sys_read cuando el proceso se bloqueo esperando teclado:
** userland debe reintentar (ver READ_RETRY en userlib.h). Distinto de 0 (EOF)
** y de -1 (error de pipe). Permite que cat/wc/filter funcionen tanto con
** teclado (reintento) como con pipes (0 = EOF) usando el mismo bucle. */
#define SYS_READ_RETRY ((uint64_t)-2)

/* sys_read: usa el fd[0] del PCB. Si es pipe, bloquea via semaforo del pipe y
** devuelve datos (>0) o 0 en EOF. Si es teclado (0): devuelve datos si hay,
** 0 en EOF (Ctrl+D), o SYS_READ_RETRY tras bloquear el proceso (sin datos).
** El proceso se despierta cuando llega una tecla y reintenta desde userland. */
uint64_t sys_read(char * buff, uint64_t count){
    PCB *cur = process_current();
    int phys_fd = (cur != NULL) ? cur->fd[0] : 0;

    if (pipe_is_fd(phys_fd)) {
        int64_t n = pipe_read(phys_fd, buff, count);
        return (n < 0) ? (uint64_t)-1 : (uint64_t)n;
    }

    uint64_t n = readKeyBuff(buff, count);
    if (n == 0) {
        /* Si hay un EOF pendiente (Ctrl+D), reportarlo y no bloquear. */
        if (kbd_consume_eof()) {
            return 0;
        }
        if (cur != NULL) {
            kbd_set_waiting(cur);
            cur->state = PROCESS_BLOCKED;
            force_switch = 1;
        }
        /* Sin datos: el proceso quedo bloqueado. Userland reintentara. */
        return SYS_READ_RETRY;
    }
    return n;
}

void sys_date(uint8_t * buff){
    date(buff);
}

void sys_time(uint8_t * buff){
    time(buff);
}

uint64_t sys_registers(char * buff){
    return copyRegistersBuffer(buff);
}

void sys_beep(uint32_t freq, uint64_t ticks){
    beep(freq, ticks);
}

uint64_t sys_ticks(void){
    return deltaTicks();
}

void sys_speaker_start(uint32_t freq){
    startSpeaker(freq);
}

void sys_speaker_off(void){
    turnOff();
}

uint64_t sys_screen_width(void){
    return (uint64_t)getScreenWidth();
}

uint64_t sys_screen_height(void){
    return (uint64_t)getScreenHeight();
}

void sys_putpixel(uint32_t color, uint64_t x, uint64_t y){
    putPixel(color, x, y);
}

void sys_fill_rect(uint64_t x, uint64_t y, uint64_t w, uint64_t h, uint32_t color){
    fillRect(x, y, w, h, color);
}

// ─── Syscalls de procesos (19-28) ─────────────────────────────────────────────

int64_t sys_create_process(uint64_t name, uint64_t entry,
                           uint64_t argc, uint64_t argv, uint64_t fg) {
    return (int64_t)process_create((const char *)name,
                                   (ProcessEntry)entry,
                                   (int)argc,
                                   (char **)argv,
                                   (uint8_t)fg);
}

void sys_exit(uint64_t retval) {
    process_exit((int)retval);
}

uint64_t sys_getpid(void) {
    PCB *cur = process_current();
    return (cur != NULL) ? cur->pid : 0;
}

uint64_t sys_ps(uint64_t buffer, uint64_t max_count) {
    return process_ps((ProcessInfo *)buffer, max_count);
}

void sys_kill(uint64_t pid) {
    process_kill(pid);
}

void sys_nice(uint64_t pid, uint64_t new_priority) {
    process_nice(pid, (uint8_t)new_priority);
}

void sys_block(uint64_t pid) {
    process_block(pid);
}

void sys_unblock(uint64_t pid) {
    process_unblock(pid);
}

void sys_yield(void) {
    PCB *cur = process_current();
    if (cur != NULL) {
        cur->state = PROCESS_READY;
        force_switch = 1;
    }
}

int64_t sys_waitpid(uint64_t pid) {
    return (int64_t)process_waitpid(pid);
}

// ─── Syscalls de semáforos (29-32) ────────────────────────────────────────────

int64_t sys_sem_open(const char *name, uint64_t initial_value) {
    return sem_open(name, initial_value);
}

int64_t sys_sem_wait(const char *name) {
    return sem_wait(name);
}

int64_t sys_sem_post(const char *name) {
    return sem_post(name);
}

int64_t sys_sem_close(const char *name) {
    return sem_close(name);
}

// ─── Syscalls de pipes (33-35) ────────────────────────────────────────────────

int64_t sys_pipe(uint64_t fds) {
    return (int64_t)pipe_create((int *)fds);
}

int64_t sys_pipe_close(uint64_t fd) {
    return (int64_t)pipe_close((int)fd);
}

/* fg_fdin_fdout empaqueta tres campos chicos: bits[0:7]=fg,
** bits[16:31]=fd_in, bits[32:47]=fd_out. */
int64_t sys_create_process_fd(uint64_t name, uint64_t entry,
                              uint64_t argc, uint64_t argv,
                              uint64_t fg_fdin_fdout) {
    uint8_t  fg     = (uint8_t)(fg_fdin_fdout & 0xFF);
    int      fd_in  = (int)(int16_t)((fg_fdin_fdout >> 16) & 0xFFFF);
    int      fd_out = (int)(int16_t)((fg_fdin_fdout >> 32) & 0xFFFF);
    return (int64_t)process_create_fd((const char *)name,
                                      (ProcessEntry)entry,
                                      (int)argc,
                                      (char **)argv,
                                      fg, fd_in, fd_out);
}

/* Abre (o crea) un pipe nombrado. Retorna 0 en exito. */
int64_t sys_pipe_open(const char *name, uint64_t fds) {
    return (int64_t)pipe_open(name, (int *)fds);
}

// ─── Tabla de syscalls ────────────────────────────────────────────────────────

void * syscalls[CANT_SYS] = {
    &sys_registers,         // 0
    &sys_time,              // 1
    &sys_date,              // 2
    &sys_read,              // 3
    &sys_write,             // 4
    &sys_increase_fontsize, // 5
    &sys_decrease_fontsize, // 6
    &sys_beep,              // 7
    &sys_ticks,             // 8
    &sys_clear,             // 9
    &sys_speaker_start,     // 10
    &sys_speaker_off,       // 11
    &sys_screen_width,      // 12
    &sys_screen_height,     // 13
    &sys_putpixel,          // 14
    &sys_fill_rect,         // 15
    &sys_malloc,            // 16
    &sys_free,              // 17
    &sys_mem_status,        // 18
    &sys_create_process,    // 19
    &sys_exit,              // 20
    &sys_getpid,            // 21
    &sys_ps,                // 22
    &sys_kill,              // 23
    &sys_nice,              // 24
    &sys_block,             // 25
    &sys_unblock,           // 26
    &sys_yield,             // 27
    &sys_waitpid,           // 28
    &sys_sem_open,          // 29
    &sys_sem_wait,          // 30
    &sys_sem_post,          // 31
    &sys_sem_close,         // 32
    &sys_pipe,              // 33
    &sys_pipe_close,        // 34
    &sys_create_process_fd, // 35
    &sys_pipe_open,         // 36
    &sys_write_color,       // 37
};
