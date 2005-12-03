/*   $OpenBSD: eh.S,v 1.6 2005/12/03 14:30:05 miod Exp $   */
/*
 * Mach Operating System
 * Copyright (c) 1993-1991 Carnegie Mellon University
 * Copyright (c) 1991 OMRON Corporation
 * Copyright (c) 1996 Nivas Madhur
 * Copyright (c) 1998 Steve Murphree, Jr.
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON AND OMRON ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON AND OMRON DISCLAIM ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <machine/board.h>
#include <machine/m8820x.h>

/*
 * Save Pbus fault status register from data and inst CMMU.
 */

#define	PFSR_SAVE \
	ldcr	TMP,  CPU; \
	ld	TMP,  TMP, CI_CPUID;			/* TMP = cpu# */ \
	cmp	TMP2, TMP, 0x0;				/* CPU0 ? */ \
	bb1	eq,   TMP2, 1f; \
	cmp	TMP2, TMP, 0x1;				/* CPU1 ? */ \
	bb1	eq,   TMP2, 2f; \
	cmp	TMP2, TMP, 0x2;				/* CPU2 ? */ \
	bb1	eq,   TMP2, 3f; \
	br	4f; \
1: \
	/* must be CPU0 */ \
	or.u	TMP,  r0,   hi16(CMMU_I0); \
	ld	TMP2, TMP,  lo16(CMMU_I0) + REG_OFF(CMMU_PFSR); \
	st	TMP2, r31,  REG_OFF(EF_IPFSR); \
	or.u	TMP,  r0,   hi16(CMMU_D0); \
	ld	TMP2, TMP,  lo16(CMMU_D0) + REG_OFF(CMMU_PFSR); \
	br.n	_ASM_LABEL(pfsr_done); \
	 st	TMP2, r31,  REG_OFF(EF_DPFSR); \
2: \
	/* must be CPU1 */ \
	or.u	TMP,  r0,   hi16(CMMU_I1); \
	ld	TMP2, TMP,  lo16(CMMU_I1) + REG_OFF(CMMU_PFSR); \
	st	TMP2, r31,  REG_OFF(EF_IPFSR); \
	or.u	TMP,  r0,   hi16(CMMU_D1); \
	ld	TMP2, TMP,  lo16(CMMU_D1) + REG_OFF(CMMU_PFSR); \
	br.n	_ASM_LABEL(pfsr_done); \
	 st	TMP2, r31,  REG_OFF(EF_DPFSR); \
3: \
	/* must be CPU2 */ \
	or.u	TMP,  r0,   hi16(CMMU_I2); \
	ld	TMP2, TMP,  lo16(CMMU_I2) + REG_OFF(CMMU_PFSR); \
	st	TMP2, r31,  REG_OFF(EF_IPFSR); \
	or.u	TMP,  r0,   hi16(CMMU_D2); \
	ld	TMP2, TMP,  lo16(CMMU_D2) + REG_OFF(CMMU_PFSR); \
	br.n	_ASM_LABEL(pfsr_done); \
	 st	TMP2, r31,  REG_OFF(EF_DPFSR); \
4: \
	/* must be CPU3 */ \
	or.u	TMP,  r0,   hi16(CMMU_I3); \
	ld	TMP2, TMP,  lo16(CMMU_I3) + REG_OFF(CMMU_PFSR); \
	st	TMP2, r31,  REG_OFF(EF_IPFSR); \
	or.u	TMP,  r0,   hi16(CMMU_D3); \
	ld	TMP2, TMP,  lo16(CMMU_D3) + REG_OFF(CMMU_PFSR); \
	st	TMP2, r31,  REG_OFF(EF_DPFSR)

#include <m88k/m88k/eh_common.S>
