#ifndef SHELL_H
#define SHELL_H

#include <stdint.h>

#define CURSOR '_'
#define STDIN  0
#define STDOUT 1
#define WELCOME "  ____ ___ _____ _   ___     _______ _   _ ___ ____   ___  ____  _ \n" \
                " | __ )_ _| ____| \\ | \\ \\   / / ____| \\ | |_ _|  _ \\ / _ \\/ ___|| |\n" \
                " |  _ \\| ||  _| |  \\| |\\ \\ / /|  _| |  \\| || || | | | | | \\___ \\| |\n" \
                " | |_) | || |___| |\\  | \\ V / | |___| |\\  || || |_| | |_| |___) |_|\n" \
                " |____/___|_____|_| \\_|  \\_/  |_____|_| \\_|___|____/ \\___/|____/(_)\n\n"
#define BUFF_LENGTH 100

typedef void (*Runnable)(void);
typedef void (*ProcessMain)(int argc, char **argv);

/* Comando de la shell. Uno solo de los dos punteros debe ser != NULL:
**   builtin  != NULL → se ejecuta sincronicamente en la shell (no admite & ni |).
**   entry    != NULL → la shell lo lanza con sys_create_process (admite & y |). */
typedef struct Command{
     char        *name;
     Runnable     builtin;
     ProcessMain  entry;
     const char  *description;
} Command;

void shellPrintString(char *str);
void shellPutchar(char c, uint64_t fd);
void shellNewline(void);
void shellReadLine(char * buffer, uint64_t max);

#endif