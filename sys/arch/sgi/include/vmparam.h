/*	$OpenBSD: vmparam.h,v 1.3 2009/05/08 18:42:04 miod Exp $ */

#ifndef _SGI_VMPARAM_H_
#define _SGI_VMPARAM_H_

#define	VM_PHYSSEG_MAX	32	/* Max number of physical memory segments */

#define	VM_NFREELIST		2
#define	VM_FREELIST_DMA32	1	/* memory under 2GB suitable for DMA */

#include <mips64/vmparam.h>

#endif	/* _SGI_VMPARAM_H_ */
