#ifndef TESTMM_H
#define TESTMM_H

#include <stdint.h>

/* Entry point como proceso (registrado en sys_create_process). */
void test_mm_main(int argc, char **argv);

/* Wrapper de shell: crea test_mm como proceso foreground y espera. */
void testMM(void);

#endif
