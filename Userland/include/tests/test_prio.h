#ifndef TEST_PRIO_H
#define TEST_PRIO_H

#include "lib/userlib.h"

void test_prio_main(int argc, char **argv);

/* Entry point de proceso worker. No-static para el registro de syscall.c. */
void zero_to_max(int argc, char *argv[]);

#endif
