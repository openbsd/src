/*	$OpenBSD: vmparam.h,v 1.2 2007/05/20 20:16:07 miod Exp $	*/
/*	$NetBSD: vmparam.h,v 1.1 2006/09/01 21:26:18 uwe Exp $	*/

#ifndef _LANDISK_VMPARAM_H_
#define _LANDISK_VMPARAM_H_

#include <sh/vmparam.h>

#define	KERNBASE		0x8c000000

#define VM_PHYSSEG_MAX		1
#define	VM_PHYSSEG_NOADD
#define	VM_PHYSSEG_STRAT	VM_PSTRAT_RANDOM

#define VM_NFREELIST		1
#define VM_FREELIST_DEFAULT	0

#endif /* _LANDISK_VMPARAM_H_ */
