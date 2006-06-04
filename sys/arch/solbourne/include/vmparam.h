/* $OpenBSD: vmparam.h,v 1.2 2006/06/04 17:21:24 miod Exp $ */
/* public domain */

#ifndef _SOLBOURNE_VMPARAM_H_
#define _SOLBOURNE_VMPARAM_H_

#include <sparc/vmparam.h>

/*
 * User/kernel map constants. We slightly differ from sparc here.
 */
#undef	VM_MIN_KERNEL_ADDRESS
#define	VM_MIN_KERNEL_ADDRESS	((vaddr_t)0xf8000000)
#undef	VM_MAX_KERNEL_ADDRESS
#define VM_MAX_KERNEL_ADDRESS	((vaddr_t)0xfd000000)

#undef	IOSPACE_BASE
#define	IOSPACE_BASE		((vaddr_t)0xff000000)
#undef	IOSPACE_LEN
#define	IOSPACE_LEN		0x00f00000		/* 15 MB of iospace */

#undef	VM_PHYSSEG_MAX
#define	VM_PHYSSEG_MAX		2
#undef	VM_PHYSSEG_STRAT
#define	VM_PHYSSEG_STRAT	VM_PSTRAT_BIGFIRST

#endif /* _SOLBOURNE_VMPARAM_H_ */
