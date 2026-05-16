#include "pipe.h"
#include "semaphore.h"
#include <stddef.h>

typedef struct {
    int      in_use;
    char     buffer[PIPE_BUF_SIZE];
    int      head;       /* posicion de lectura */
    int      tail;       /* posicion de escritura */
    int      readers;    /* cantidad de extremos de lectura abiertos */
    int      writers;    /* cantidad de extremos de escritura abiertos */
    char     data_sem[16];   /* cuenta bytes disponibles */
    char     space_sem[16];  /* cuenta espacio libre */
    char     mutex_sem[16];  /* exclusion mutua sobre buffer */
    char     name[PIPE_NAME_LEN]; /* nombre para pipe_open (identificador a priori) */
} Pipe;

static Pipe pipes[MAX_PIPES];

/* ── helpers ─────────────────────────────────────────────────────────────── */

static void itoa2(int n, char *out) {
    /* n en [0, MAX_PIPES) <= 16, alcanza 2 digitos */
    if (n < 10) {
        out[0] = '0' + n;
        out[1] = '\0';
    } else {
        out[0] = '0' + (n / 10);
        out[1] = '0' + (n % 10);
        out[2] = '\0';
    }
}

static void build_name(char *out, const char *prefix, int idx) {
    int i = 0;
    while (prefix[i]) { out[i] = prefix[i]; i++; }
    char num[4];
    itoa2(idx, num);
    int j = 0;
    while (num[j]) { out[i++] = num[j++]; }
    out[i] = '\0';
}

static int str_eq(const char *a, const char *b) {
    if (!a || !b) return 0;
    while (*a && (*a == *b)) { a++; b++; }
    return (unsigned char)*a == (unsigned char)*b;
}

static void str_copy_n(char *dst, const char *src, int max) {
    int i = 0;
    while (i < max - 1 && src && src[i]) { dst[i] = src[i]; i++; }
    dst[i] = '\0';
}

static int fd_to_index(int fd) {
    if (fd >= PIPE_FD_WRITE_BASE && fd < PIPE_FD_WRITE_BASE + MAX_PIPES)
        return fd - PIPE_FD_WRITE_BASE;
    if (fd >= PIPE_FD_READ_BASE && fd < PIPE_FD_READ_BASE + MAX_PIPES)
        return fd - PIPE_FD_READ_BASE;
    return -1;
}

static int fd_is_write(int fd) {
    return fd >= PIPE_FD_WRITE_BASE && fd < PIPE_FD_WRITE_BASE + MAX_PIPES;
}

/* ── API ─────────────────────────────────────────────────────────────────── */

void pipe_init(void) {
    int i;
    for (i = 0; i < MAX_PIPES; i++) {
        pipes[i].in_use  = 0;
        pipes[i].head    = 0;
        pipes[i].tail    = 0;
        pipes[i].readers = 0;
        pipes[i].writers = 0;
        pipes[i].name[0] = '\0';
    }
}

int pipe_is_fd(int fd) {
    return (fd >= PIPE_FD_READ_BASE && fd < PIPE_FD_READ_BASE + MAX_PIPES) ||
           (fd >= PIPE_FD_WRITE_BASE && fd < PIPE_FD_WRITE_BASE + MAX_PIPES);
}

int pipe_create(int fds[2]) {
    if (!fds) return -1;
    int i;
    for (i = 0; i < MAX_PIPES; i++) {
        if (!pipes[i].in_use) break;
    }
    if (i == MAX_PIPES) return -1;

    Pipe *p = &pipes[i];
    p->in_use  = 1;
    p->head    = 0;
    p->tail    = 0;
    p->readers = 1;
    p->writers = 1;
    p->name[0] = '\0';

    build_name(p->data_sem,  "__pipe_d_", i);
    build_name(p->space_sem, "__pipe_s_", i);
    build_name(p->mutex_sem, "__pipe_m_", i);

    if (!sem_open(p->data_sem,  0) ||
        !sem_open(p->space_sem, PIPE_BUF_SIZE) ||
        !sem_open(p->mutex_sem, 1)) {
        /* limpieza parcial: cerrar los que se hayan abierto */
        sem_close(p->data_sem);
        sem_close(p->space_sem);
        sem_close(p->mutex_sem);
        p->in_use = 0;
        return -1;
    }

    fds[0] = PIPE_FD_READ_BASE  + i;
    fds[1] = PIPE_FD_WRITE_BASE + i;
    return 0;
}

/* Abre (o crea) un pipe nombrado. Procesos no relacionados pueden compartir
** un pipe acordando el mismo nombre a priori. Retorna 0 en exito. */
int pipe_open(const char *name, int fds[2]) {
    if (!name || !fds) return -1;

    int i;
    for (i = 0; i < MAX_PIPES; i++) {
        if (pipes[i].in_use && str_eq(pipes[i].name, name)) {
            pipes[i].readers++;
            pipes[i].writers++;
            fds[0] = PIPE_FD_READ_BASE  + i;
            fds[1] = PIPE_FD_WRITE_BASE + i;
            return 0;
        }
    }

    for (i = 0; i < MAX_PIPES; i++) {
        if (!pipes[i].in_use) break;
    }
    if (i == MAX_PIPES) return -1;

    Pipe *p = &pipes[i];
    p->in_use  = 1;
    p->head    = 0;
    p->tail    = 0;
    p->readers = 1;
    p->writers = 1;
    str_copy_n(p->name, name, PIPE_NAME_LEN);

    build_name(p->data_sem,  "__pipe_d_", i);
    build_name(p->space_sem, "__pipe_s_", i);
    build_name(p->mutex_sem, "__pipe_m_", i);

    if (!sem_open(p->data_sem,  0) ||
        !sem_open(p->space_sem, PIPE_BUF_SIZE) ||
        !sem_open(p->mutex_sem, 1)) {
        sem_close(p->data_sem);
        sem_close(p->space_sem);
        sem_close(p->mutex_sem);
        p->in_use = 0;
        p->name[0] = '\0';
        return -1;
    }

    fds[0] = PIPE_FD_READ_BASE  + i;
    fds[1] = PIPE_FD_WRITE_BASE + i;
    return 0;
}

int pipe_close(int fd) {
    int idx = fd_to_index(fd);
    if (idx < 0) return -1;
    Pipe *p = &pipes[idx];
    if (!p->in_use) return -1;

    if (fd_is_write(fd)) {
        if (p->writers > 0) p->writers--;
        /* despertar a todos los readers bloqueados para que vean EOF */
        if (p->writers == 0) sem_broadcast(p->data_sem);
    } else {
        if (p->readers > 0) p->readers--;
        /* despertar a todos los writers bloqueados (verán broken pipe) */
        if (p->readers == 0) sem_broadcast(p->space_sem);
    }

    if (p->readers == 0 && p->writers == 0) {
        sem_close(p->data_sem);
        sem_close(p->space_sem);
        sem_close(p->mutex_sem);
        p->in_use = 0;
    }
    return 0;
}

int64_t pipe_read(int fd, char *buf, uint64_t n) {
    int idx = fd_to_index(fd);
    if (idx < 0 || fd_is_write(fd)) return -1;
    Pipe *p = &pipes[idx];
    if (!p->in_use || !buf) return -1;
    if (n == 0) return 0;

    uint64_t read = 0;
    while (read < n) {
        /* Si no hay datos y no hay writers, EOF */
        if (p->head == p->tail && p->writers == 0) {
            return (int64_t)read;
        }

        sem_wait(p->data_sem);

        /* tras el wait, si seguimos sin datos y no hay writers → EOF */
        if (p->head == p->tail && p->writers == 0) {
            return (int64_t)read;
        }

        sem_wait(p->mutex_sem);
        buf[read++] = p->buffer[p->head];
        p->head = (p->head + 1) % PIPE_BUF_SIZE;
        sem_post(p->mutex_sem);

        sem_post(p->space_sem);

        /* leemos byte a byte hasta n, pero si el buffer queda vacío
           devolvemos lo leído sin bloquear (semántica POSIX). */
        if (p->head == p->tail) return (int64_t)read;
    }
    return (int64_t)read;
}

int64_t pipe_write(int fd, const char *buf, uint64_t n) {
    int idx = fd_to_index(fd);
    if (idx < 0 || !fd_is_write(fd)) return -1;
    Pipe *p = &pipes[idx];
    if (!p->in_use || !buf) return -1;
    if (n == 0) return 0;

    uint64_t written = 0;
    while (written < n) {
        if (p->readers == 0) return -1;  /* broken pipe */

        sem_wait(p->space_sem);

        if (p->readers == 0) {
            return -1;
        }

        sem_wait(p->mutex_sem);
        p->buffer[p->tail] = buf[written++];
        p->tail = (p->tail + 1) % PIPE_BUF_SIZE;
        sem_post(p->mutex_sem);

        sem_post(p->data_sem);
    }
    return (int64_t)written;
}
