#ifndef TEST_PROC_H
#define TEST_PROC_H

#include "userlib.h"

/* Entry point como proceso. */
void test_proc_main(int argc, char **argv);

/* Wrapper de shell: crea test_proc como proceso foreground y espera. */
void test_proc(void);

#endif
