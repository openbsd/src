/* $OpenBSD: cpu.h,v 1.5 2009/02/16 22:55:03 miod Exp $ */
/* public domain */
#ifndef	_LUNA88K_CPU_H_
#define	_LUNA88k_CPU_H_

#include <m88k/cpu.h>

#ifdef _KERNEL
void luna88k_ext_int(struct trapframe *eframe);
#define	md_interrupt_func	luna88k_ext_int
#endif	/* _KERNEL */

#endif	/* _LUNA88k_CPU_H_ */
