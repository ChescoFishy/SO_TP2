#include <stdint.h>
#include "lib/userlib.h"
#include "commands/commands.h"

#define CAT_BUFFER_LENGTH 512

/* cat: copia stdin a stdout hasta EOF (Ctrl+D o cierre del pipe), volcando la
** salida recien cuando el buffer se llena o al llegar EOF. En el TP de
** referencia ese comportamiento sale gratis porque read bloquea hasta llenar
** el buffer; aca sys_read devuelve lecturas cortas (lo que haya disponible),
** asi que se acumula manualmente para diferir la impresion igual que alla. */
void cat_main(int argc, char **argv) {
    (void)argc; (void)argv;
    char buf[CAT_BUFFER_LENGTH];
    uint64_t used = 0;
    uint64_t read_bytes;
    while ((read_bytes = read_full(buf + used, CAT_BUFFER_LENGTH - used)) > 0) {
        used += read_bytes;
        if (used == CAT_BUFFER_LENGTH) {
            sys_write(STDOUT, buf, used);
            used = 0;
        }
    }
    if (used > 0) {
        sys_write(STDOUT, buf, used);
    }

    sys_write(STDOUT, "\n", 1);
    sys_exit(0);
}
