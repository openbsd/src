/* $OpenBSD: cpu.h,v 1.7 2014/06/03 22:43:51 aoyama Exp $ */
/* public domain */
#ifndef	_MACHINE_CPU_H_
#define	_MACHINE_CPU_H_

#include <m88k/cpu.h>

#ifdef _KERNEL
void luna88k_ext_int(struct trapframe *eframe);
#define	md_interrupt_func	luna88k_ext_int
#endif	/* _KERNEL */

#endif	/* _MACHINE_CPU_H_ */
