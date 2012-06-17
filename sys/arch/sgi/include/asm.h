/*	$OpenBSD: asm.h,v 1.5 2012/06/17 12:34:19 miod Exp $ */
#ifndef	_SGI_ASM_H_
#define	_SGI_ASM_H_

#include <mips64/asm.h>
#ifdef TGT_OCTANE
#include <sgi/sgi/ip30.h>
#endif

#ifdef MULTIPROCESSOR

#ifdef TGT_OCTANE
#include <sgi/xbow/xheartreg.h>

/* Returns the physical cpu identifier */
#define HW_GET_CPU_PRID(prid, tmp)				\
	LOAD_XKPHYS(tmp, CCA_NC);				\
	PTR_L	prid, (HEART_PIU_BASE + HEART_PRID)(tmp)	\

/* Return the cpu_info pointer - see locore.S for the logic behind this */
#define HW_GET_CPU_INFO(ci, tmp)			\
	HW_GET_CPU_PRID(ci, tmp);			\
	LOAD_XKPHYS(tmp, CCA_COHERENT_EXCLWRITE);	\
	PTR_SLL	ci, MPCONF_SHIFT;			\
	PTR_ADD	ci, MPCONF_BASE;			\
	or	tmp, ci;				\
	PTR_L	ci, (MPCONF_LEN - REGSZ)(tmp)
#endif

#endif

#endif	/* _SGI_ASM_H_ */
