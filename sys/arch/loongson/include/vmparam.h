/*	$OpenBSD: vmparam.h,v 1.3 2011/03/23 16:54:35 pirofti Exp $ */
/* public domain */
#ifndef _MACHINE_VMPARAM_H_
#define _MACHINE_VMPARAM_H_

#define	VM_PHYSSEG_MAX		2 /* Max number of physical memory segments */
#define	VM_PHYSSEG_STRAT	VM_PSTRAT_BIGFIRST

#include <mips64/vmparam.h>

#endif	/* _MACHINE_VMPARAM_H_ */
