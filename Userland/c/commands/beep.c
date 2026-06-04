#include <stdint.h>
#include "lib/userlib.h"

// Reproduce una secuencia corta de beeps
void playBeep(){
    sys_beep(NOTE_E5, EIGHTH);
    sys_beep(NOTE_DS5, EIGHTH);
    sys_beep(NOTE_E5, EIGHTH);
    sys_beep(NOTE_DS5, EIGHTH);
    sys_beep(NOTE_E5, EIGHTH);
    sys_beep(NOTE_B4, EIGHTH);
    sys_beep(NOTE_D5, EIGHTH);
    sys_beep(NOTE_C5, EIGHTH);
    sys_beep(NOTE_A4, QUARTER);

    sys_beep(NOTE_C4, EIGHTH);
    sys_beep(NOTE_E4, EIGHTH);
    sys_beep(NOTE_A4, EIGHTH);
    sys_beep(NOTE_B4, QUARTER);

    sys_beep(NOTE_E4, EIGHTH);
    sys_beep(NOTE_GS4, EIGHTH);
    sys_beep(NOTE_B4, EIGHTH);
    sys_beep(NOTE_C5, QUARTER);

    sys_beep(NOTE_E4, EIGHTH);
    sys_beep(NOTE_E5, EIGHTH);
    sys_beep(NOTE_DS5, EIGHTH);
    sys_beep(NOTE_E5, EIGHTH);
    sys_beep(NOTE_DS5, EIGHTH);
    sys_beep(NOTE_E5, EIGHTH);
    sys_beep(NOTE_B4, EIGHTH);
    sys_beep(NOTE_D5, EIGHTH);
    sys_beep(NOTE_C5, EIGHTH);
    sys_beep(NOTE_A4, QUARTER);

    sys_beep(NOTE_C4, EIGHTH);
    sys_beep(NOTE_E4, EIGHTH);
    sys_beep(NOTE_A4, EIGHTH);
    sys_beep(NOTE_B4, QUARTER);

    sys_beep(NOTE_E4, EIGHTH);
    sys_beep(NOTE_C5, EIGHTH);
    sys_beep(NOTE_B4, EIGHTH);
    sys_beep(NOTE_A4, QUARTER);
}
