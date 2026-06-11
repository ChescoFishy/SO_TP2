#ifndef INTERRUPS_H_
#define INTERRUPS_H_
 
#include <interrupts/idtLoader.h>
 
void _irq00Handler(void);
void _irq01Handler(void);
void _irq02Handler(void);
void _irq03Handler(void);
void _irq04Handler(void);
void _irq05Handler(void);
void _irq128Handler(void);
void _irq129Handler(void);
void _exception0Handler(void);
void _exception6Handler(void);
 
/* Cede la CPU desde adentro del kernel (int 0x81). Usado por sem_wait para
** bloquear al proceso actual en medio de una syscall. */
void kernel_yield(void);

/* Termina el proceso actual via la syscall sys_exit(0) (int 0x80). No
** retorna. Usado por el trampolin de entrada cuando un entry retorna. */
void kernel_exit(void);

void _cli(void);
void _sti(void);
void _hlt(void);
void load_idt_asm(void * idtr);
 
void picMasterMask(uint8_t mask);
void picSlaveMask(uint8_t mask);
void haltcpu(void);
 
#endif