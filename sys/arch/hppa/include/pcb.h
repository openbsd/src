/*	$OpenBSD: pcb.h,v 1.4 1999/06/22 18:01:12 mickey Exp $	*/

#ifndef _MACHINE_PCB_H_
#define _MACHINE_PCB_H_

#include <machine/reg.h>

struct pcb {
	struct trapframe pcb_tf;
	u_int64_t pcb_fpregs[HPPA_NFPREGS];	/* not included above */
	int (*pcb_onfault) __P((void));	/* SW copy fault handler */
	pa_space_t pcb_space;		/* copy pmap_space, for asm's sake */
};

struct md_coredump {
	struct reg md_reg;
}; 


#endif /* _MACHINE_PCB_H_ */
