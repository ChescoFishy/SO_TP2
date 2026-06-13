#include "shell/shell.h"
#include "lib/userlib.h"

// Bucle principal de la shell de usuario
int main(void){
    shellPrintString(WELCOME);
    shellNewline();
    shellPrintString("Escriba 'help' para listar comandos.\n");

    char buff[BUFF_LENGTH];
    while(1){
        shellPrintString("> ");
        sys_set_cursor(1);
        shellReadLine(buff, BUFF_LENGTH);
        sys_set_cursor(0);
        shellNewline();
        processLine(buff, 0);
    }

    return 0;
}

/* Lee una línea desde teclado. El cursor parpadeante lo dibuja el kernel
** (sys_set_cursor): cualquier escritura a consola — de la shell o de un
** proceso en background — lo desplaza por delante, como en una terminal. */
void shellReadLine(char * buffer, uint64_t max){
    char c;
    uint32_t idx = 0;

    while(1){
        if(sys_read(&c, 1) != 1){
            continue; // READ_RETRY: el proceso durmió esperando una tecla
        }

        if(c == '\n'){
            break;
        }

        if(c == '\b'){
            if(idx > 0){
                idx--;
                shellPutchar('\b', STDOUT); // borrar último carácter
            }
        } else if(c == '+'){
            // Aumentar fuente y redibujar contenido
            sys_increase_fontsize();
            redrawFont();
        } else if(c == '-'){
            // Disminuir fuente y redibujar contenido
            sys_decrease_fontsize();
            redrawFont();
        } else {
            if(idx + 1 < max){ // dejar lugar para terminador NUL
                buffer[idx++] = c;
                shellPutchar(c, STDOUT);
            }
        }
    }

    buffer[idx] = 0;
}

// Imprime una cadena en STDOUT
void shellPrintString(char *str){
    if(str == 0){
        return;
    }
    for(uint32_t i = 0; str[i] != '\0'; i++){
        shellPutchar(str[i], STDOUT);
    }
}

// Escribe un caracter en el descriptor indicado
void shellPutchar(char c, uint64_t fd){
    // Registrar en redraw buffer antes de imprimir
    redraw_append_char(c, fd);
    sys_write(fd, &c, 1); // escribo el caracter
}

// Salto de línea
void shellNewline(){
    shellPutchar('\n', STDOUT);
}