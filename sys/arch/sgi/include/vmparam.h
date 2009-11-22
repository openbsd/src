/*	$OpenBSD: vmparam.h,v 1.5 2009/11/22 00:07:04 miod Exp $ */
/* public domain */
#ifndef _SGI_VMPARAM_H_
#define _SGI_VMPARAM_H_

#define	VM_PHYSSEG_MAX	32	/* Max number of physical memory segments */

/*
 * On Origin and Octane families, DMA to 32-bit PCI devices is restricted.
 *
 * Systems with physical memory after the 2GB boundary needs to ensure
 * memory which may used for DMA transfers is allocated from the low
 * memory range.
 *
 * Other systems, like the O2, do not have such a restriction, but can not
 * have more than 2GB of physical memory, so this doesn't affect them.
 */

#define	VM_NFREELIST		2
#define	VM_FREELIST_DMA32	1	/* memory suitable for 32-bit DMA */

/*
 * On systems with may use R5000 processors, we limit the kernel virtual
 * address space to KSSEG and KSEG3.
 * On systems with R10000 family processors, we use the XKSEG which allows
 * for a much larger virtual memory size.
 *
 * All Octane and Origin class systems are R10000 family based only,
 * so TGT_COHERENT is safe to use so far.
 */

#ifdef TGT_COHERENT
#define	VM_MIN_KERNEL_ADDRESS	((vaddr_t)0xc000000000000000L)
#define	VM_MAX_KERNEL_ADDRESS	((vaddr_t)0xc000000040000000L)
#else
#define	VM_MIN_KERNEL_ADDRESS	((vaddr_t)0xffffffffc0000000L)
#define	VM_MAX_KERNEL_ADDRESS	((vaddr_t)0xfffffffffffff000L)
#endif

#include <mips64/vmparam.h>

#endif	/* _SGI_VMPARAM_H_ */
