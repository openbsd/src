/*	$OpenBSD: vmparam.h,v 1.3 2011/03/23 16:54:35 pirofti Exp $	*/
/*	$NetBSD: vmparam.h,v 1.1 2006/09/01 21:26:18 uwe Exp $	*/

#ifndef _MACHINE_VMPARAM_H_
#define _MACHINE_VMPARAM_H_

#include <sh/vmparam.h>

#define	KERNBASE		0x8c000000

#define VM_PHYSSEG_MAX		1
#define	VM_PHYSSEG_NOADD
#define	VM_PHYSSEG_STRAT	VM_PSTRAT_RANDOM

#define VM_NFREELIST		1
#define VM_FREELIST_DEFAULT	0

#endif /* _MACHINE_VMPARAM_H_ */
