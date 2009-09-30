/*	$OpenBSD: asm.h,v 1.2 2009/09/30 06:22:00 syuu Exp $ */

/* Use Mips generic include file */

#ifdef MULTIPROCESSOR
#define HW_CPU_NUMBER(reg)			\
	LA reg, HW_CPU_NUMBER_REG;		\
	PTR_L reg, 0(reg)
#else  /* MULTIPROCESSOR */
#define HW_CPU_NUMBER(reg)			\
	LI reg, 0
#endif /* MULTIPROCESSOR */

#include <mips64/asm.h>
