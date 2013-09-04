/* 	$OpenBSD: isa_machdep.h,v 1.1 2013/09/04 14:38:27 patrick Exp $	*/
/* $NetBSD: isa_machdep.h,v 1.4 2002/01/07 22:58:08 chris Exp $ */

#ifndef _MACHINE_ISA_MACHDEP_H_
#define _MACHINE_ISA_MACHDEP_H_
#include <arm/isa_machdep.h>

#ifdef _KERNEL
#define ISA_FOOTBRIDGE_IRQ IRQ_IN_L2
void	isa_footbridge_init(u_int, u_int);
#endif /* _KERNEL */

#endif /* _MACHINE_ISA_MACHDEP_H_ */
