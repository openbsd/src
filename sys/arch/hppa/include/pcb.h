/*	$OpenBSD: pcb.h,v 1.3 1999/04/20 19:46:22 mickey Exp $	*/

#ifndef _MACHINE_PCB_H_
#define _MACHINE_PCB_H_

#include <machine/reg.h>

struct pcb {
	struct trapframe pcb_tf;
	/* would be nice to align to cache line size here XXX */
	int (*pcb_onfault) __P((void));	/* SW copy fault handler */
	pa_space_t pcb_space;		/* copy pmap_space, for asm's sake */
};

struct md_coredump {
	struct reg md_reg;
}; 


#endif /* _MACHINE_PCB_H_ */
