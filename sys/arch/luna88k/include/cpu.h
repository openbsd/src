/* $OpenBSD: cpu.h,v 1.4 2004/11/08 16:39:28 miod Exp $ */
/* public domain */
#ifndef	_LUNA88K_CPU_H_
#define	_LUNA88k_CPU_H_

#include <m88k/cpu.h>

#ifdef _KERNEL
void luna88k_ext_int(u_int v, struct trapframe *eframe);
#define	md_interrupt_func	luna88k_ext_int
#endif	/* _KERNEL */

#endif	/* _LUNA88k_CPU_H_ */
