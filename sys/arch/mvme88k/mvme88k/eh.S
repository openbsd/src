/*   $OpenBSD: eh.S,v 1.57 2004/07/27 12:16:06 miod Exp $   */
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

/*
 * Save Pbus fault status register from data and inst CMMU.
 */

/* Single-processor SBC version (MVME187, MVME8120) */
#ifdef MVME187
#define	PFSR_SAVE_187 \
	or.u	TMP,  r0,   hi16(SBC_CMMU_I); \
	ld	TMP2, TMP,  lo16(SBC_CMMU_I) + 0x108; \
	st	TMP2, r31,  REG_OFF(EF_IPFSR); \
	or.u	TMP,  r0,   hi16(SBC_CMMU_D); \
	ld	TMP2, TMP,  lo16(SBC_CMMU_D) + 0x108; \
	st	TMP2, r31,  REG_OFF(EF_DPFSR);
#else
#define	PFSR_SAVE_187
#endif

/* MVME188 version */
#ifdef MVME188
#define	PFSR_SAVE_188 \
	extu	TMP,  FLAGS, FLAG_CPU_FIELD_WIDTH<0>;	/* TMP = cpu# */ \
	cmp	TMP2, TMP, 0x0;				/* CPU0 ? */ \
	bb1	eq,   TMP2, 1f; \
	cmp	TMP2, TMP, 0x1;				/* CPU1 ? */ \
	bb1	eq,   TMP2, 2f; \
	cmp	TMP2, TMP, 0x2;				/* CPU2 ? */ \
	bb1	eq,   TMP2, 3f; \
	br	4f; \
	/* XXX WHAT ABOUT MODULES WITH SPLIT U/S CMMUS ??? */ \
1: \
	/* must be CPU0 */ \
	or.u	TMP,  r0,   hi16(VME_CMMU_I0); \
	ld	TMP2, TMP,  lo16(VME_CMMU_I0) + 0x108; \
	st	TMP2, r31,  REG_OFF(EF_IPFSR); \
	or.u	TMP,  r0,   hi16(VME_CMMU_D0); \
	ld	TMP2, TMP,  lo16(VME_CMMU_D0) + 0x108; \
	br.n	_ASM_LABEL(pfsr_done); \
	 st	TMP2, r31,  REG_OFF(EF_DPFSR); \
2: \
	/* must be CPU1 */ \
	or.u	TMP,  r0,   hi16(VME_CMMU_I1); \
	ld	TMP2, TMP,  lo16(VME_CMMU_I1) + 0x108; \
	st	TMP2, r31,  REG_OFF(EF_IPFSR); \
	or.u	TMP,  r0,   hi16(VME_CMMU_D1); \
	ld	TMP2, TMP,  lo16(VME_CMMU_D1) + 0x108; \
	br.n	_ASM_LABEL(pfsr_done); \
	 st	TMP2, r31,  REG_OFF(EF_DPFSR); \
3: \
	/* must be CPU2 */ \
	or.u	TMP,  r0,   hi16(VME_CMMU_I2); \
	ld	TMP2, TMP,  lo16(VME_CMMU_I2) + 0x108; \
	st	TMP2, r31,  REG_OFF(EF_IPFSR); \
	or.u	TMP,  r0,   hi16(VME_CMMU_D2); \
	ld	TMP2, TMP,  lo16(VME_CMMU_D2) + 0x108; \
	br.n	_ASM_LABEL(pfsr_done); \
	 st	TMP2, r31,  REG_OFF(EF_DPFSR); \
4: \
	/* must be CPU3 */ \
	or.u	TMP,  r0,   hi16(VME_CMMU_I3); \
	ld	TMP2, TMP,  lo16(VME_CMMU_I3) + 0x108; \
	st	TMP2, r31,  REG_OFF(EF_IPFSR); \
	or.u	TMP,  r0,   hi16(VME_CMMU_D3); \
	ld	TMP2, TMP,  lo16(VME_CMMU_D3) + 0x108; \
	br.n	_ASM_LABEL(pfsr_done); \
	 st	TMP2, r31,  REG_OFF(EF_DPFSR);
#else
#define	PFSR_SAVE_188
#endif

#if defined(MVME187) && defined(MVME188)
#define	PFSR_SAVE \
	/* check if it's a mvme188 */ \
	or.u	TMP,  r0,   hi16(_C_LABEL(brdtyp)); \
	ld	TMP2, TMP,  lo16(_C_LABEL(brdtyp)); \
	cmp	TMP,  TMP2, BRD_188; \
	bb1	ne,   TMP,  9f; \
	PFSR_SAVE_188 \
9:	PFSR_SAVE_187
#else
#define	PFSR_SAVE \
	PFSR_SAVE_188 \
	PFSR_SAVE_187
#endif

#include <m88k/m88k/eh_common.S>
