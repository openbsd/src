/*	$OpenBSD: asm.h,v 1.4 2010/09/11 11:29:50 syuu Exp $ */

#ifdef MULTIPROCESSOR
#define HW_GET_CPU_INFO(ci, tmp)	\
	LOAD_XKPHYS(ci, CCA_CACHED);	\
	mfc0	tmp, COP_0_LLADDR;	\
	or	ci, ci, tmp
#endif

#include <mips64/asm.h>
