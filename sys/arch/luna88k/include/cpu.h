/* $OpenBSD: cpu.h,v 1.6 2011/03/23 16:54:35 pirofti Exp $ */
/* public domain */
#ifndef	_MACHINE_CPU_H_
#define	_LUNA88k_CPU_H_

#include <m88k/cpu.h>

#ifdef _KERNEL
void luna88k_ext_int(struct trapframe *eframe);
#define	md_interrupt_func	luna88k_ext_int
#endif	/* _KERNEL */

#endif	/* _LUNA88k_CPU_H_ */
