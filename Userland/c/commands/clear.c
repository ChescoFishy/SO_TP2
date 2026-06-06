// Builtin clear: limpia la pantalla y resetea el buffer de redraw.
#include "lib/userlib.h"

// Limpia la pantalla
void clear(){
    sys_clear();
    redraw_reset();
}
