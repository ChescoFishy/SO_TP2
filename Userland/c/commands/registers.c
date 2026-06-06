// Builtin registers: imprime el snapshot de registros capturado con CTRL.
#include "lib/userlib.h"
#include "shell/shell.h"

// Imprime el snapshot de registros (CTRL para capturar)
void registers(){
    char buffer[REGSBUFF];

    if(sys_registers(buffer)){
        shellPrintString(buffer);
    } else{
        shellPrintString("Presione CTRL para guardar los registros.\n");
    }
}
