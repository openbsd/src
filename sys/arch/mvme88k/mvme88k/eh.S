/*   $OpenBSD: eh.S,v 1.63 2005/12/03 14:30:06 miod Exp $   */
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

#include <machine/m8820x.h>

/*
 * Save Pbus fault status register from data and inst CMMU.
 */

/*
 * Single-processor SBC version (MVME187, MVME8120)
 */

#ifdef MVME187
#include <machine/mvme187.h>
#define	PFSR_SAVE_187 \
	or.u	TMP,  r0,   hi16(SBC_CMMU_I); \
	ld	TMP2, TMP,  lo16(SBC_CMMU_I) + REG_OFF(CMMU_PFSR); \
	st	TMP2, r31,  REG_OFF(EF_IPFSR); \
	or.u	TMP,  r0,   hi16(SBC_CMMU_D); \
	ld	TMP2, TMP,  lo16(SBC_CMMU_D) + REG_OFF(CMMU_PFSR); \
	st	TMP2, r31,  REG_OFF(EF_DPFSR);
#else
#define	PFSR_SAVE_187
#endif

/*
 * MVME188 version (see pfsr188 below)
 */
#ifdef MVME188
#define	PFSR_SAVE_188 \
	br	_ASM_LABEL(pfsr188);
#else
#define	PFSR_SAVE_188
#endif


#if defined(MVME187) && defined(MVME188)
#define	PFSR_SAVE \
	/* check if it's a mvme188 */ \
	or.u	TMP,  r0,   hi16(_C_LABEL(brdtyp)); \
	ld	TMP2, TMP,  lo16(_C_LABEL(brdtyp)); \
	cmp	TMP,  TMP2, BRD_188; \
	bb1	eq,   TMP,  _ASM_LABEL(pfsr188); \
	PFSR_SAVE_187
#else
#define	PFSR_SAVE \
	PFSR_SAVE_188 \
	PFSR_SAVE_187
#endif

#include <m88k/m88k/eh_common.S>

#ifdef MVME188
#include <machine/mvme188.h>
/*
 * Handling faults can be very tricky, as it is necessary to check all
 * the CMMUs of the faulting processor.
 *
 * This implementation tries to favor, speedwise:
 * - simple (2 CMMUs per CPU) configurations over any other configuration;
 * - faults on CPU0 over the other CPUs.
 */
	text

ASLOCAL(pfsr188)
	or.u	TMP,  r0,   hi16(_C_LABEL(cmmu_shift))
	ld	TMP,  TMP,  lo16(_C_LABEL(cmmu_shift))
	cmp	TMP2, TMP,  2
	ldcr	TMP,  CPU
	bb1.n	lt,   TMP2, _ASM_LABEL(pfsr188_straight)
	 ld	TMP,  TMP,  CI_CPUID			/* TMP = cpu# */
	bb1	eq,   TMP2, _ASM_LABEL(pfsr188_double)

ASLOCAL(pfsr188_quad)
	/*
	 * We're in configuration 2 (1 CPU, 8 CMMU).
	 *
	 * Reading the MVME188 error register could tell us which
	 * CMMU faulted, but it's easier to check every one until
	 * we find the first fault.
	 *
	 * If two or more CMMU faulted simultaneously, restarting from
	 * the trap will fault again, and we'll handle the remaining
	 * exceptions here.
	 *
	 * However, to be sure we don't get fooled with already-processed
	 * exceptions, we have to reset the fault status registers
	 * after reading them.
	 *
	 * XXX It will probably be worth changing this so that we can
	 * handle a bunch of CMMU faults at once in trap.c.
	 */
	or.u	TMP,  r0,   hi16(VME_CMMU_I0)
	ld	TMP2, TMP,  lo16(VME_CMMU_I0) + REG_OFF(CMMU_PFSR)
	extu	TMP3, TMP2, 3<16>
	bcnd.n	ne0,  TMP3, 1f
	 st	r0,   TMP,  lo16(VME_CMMU_I0) + REG_OFF(CMMU_PFSR)
	or.u	TMP,  r0,   hi16(VME_CMMU_I1)
	ld	TMP2, TMP,  lo16(VME_CMMU_I1) + REG_OFF(CMMU_PFSR)
	extu	TMP3, TMP2, 3<16>
	bcnd.n	ne0,  TMP3, 1f
	 st	r0,   TMP,  lo16(VME_CMMU_I1) + REG_OFF(CMMU_PFSR)
	or.u	TMP,  r0,   hi16(VME_CMMU_I2)
	ld	TMP2, TMP,  lo16(VME_CMMU_I2) + REG_OFF(CMMU_PFSR)
	extu	TMP3, TMP2, 3<16>
	bcnd.n	ne0,  TMP3, 1f
	 st	r0,   TMP,  lo16(VME_CMMU_I2) + REG_OFF(CMMU_PFSR)
	or.u	TMP,  r0,   hi16(VME_CMMU_I3)
	ld	TMP2, TMP,  lo16(VME_CMMU_I3) + REG_OFF(CMMU_PFSR)
	st	r0,   TMP,  lo16(VME_CMMU_I3) + REG_OFF(CMMU_PFSR)
1:
	st	TMP2, r31,  REG_OFF(EF_IPFSR)
	
	or.u	TMP,  r0,   hi16(VME_CMMU_D0)
	ld	TMP2, TMP,  lo16(VME_CMMU_D0) + REG_OFF(CMMU_PFSR)
	extu	TMP3, TMP2, 3<16>
	bcnd.n	ne0,  TMP3, 2f
	 st	r0,   TMP,  lo16(VME_CMMU_D0) + REG_OFF(CMMU_PFSR)
	or.u	TMP,  r0,   hi16(VME_CMMU_D1)
	ld	TMP2, TMP,  lo16(VME_CMMU_D1) + REG_OFF(CMMU_PFSR)
	extu	TMP3, TMP2, 3<16>
	bcnd.n	ne0,  TMP3, 2f
	 st	r0,   TMP,  lo16(VME_CMMU_D1) + REG_OFF(CMMU_PFSR)
	or.u	TMP,  r0,   hi16(VME_CMMU_D2)
	ld	TMP2, TMP,  lo16(VME_CMMU_D2) + REG_OFF(CMMU_PFSR)
	extu	TMP3, TMP2, 3<16>
	bcnd.n	ne0,  TMP3, 2f
	 st	r0,   TMP,  lo16(VME_CMMU_D2) + REG_OFF(CMMU_PFSR)
	or.u	TMP,  r0,   hi16(VME_CMMU_D3)
	ld	TMP2, TMP,  lo16(VME_CMMU_D3) + REG_OFF(CMMU_PFSR)
	st	r0,   TMP,  lo16(VME_CMMU_D3) + REG_OFF(CMMU_PFSR)
2:
	br.n	_ASM_LABEL(pfsr_done)
	 st	TMP2, r31,  REG_OFF(EF_DPFSR)

ASLOCAL(pfsr188_double)
	/*
	 * 4 CMMUs per CPU. Almost as simple as the 2 CMMUs per CPU
	 * situation...
	 *
	 * However, to be sure we don't get fooled with already-processed
	 * exceptions, we have to reset the fault status registers
	 * after reading them.
	 */
	bcnd	ne0,  TMP,  5f

	/* CPU 0 -> CMMU 0 and 1 */
	or.u	TMP,  r0,   hi16(VME_CMMU_I0)
	ld	TMP2, TMP,  lo16(VME_CMMU_I0) + REG_OFF(CMMU_PFSR)
	extu	TMP3, TMP2, 3<16>
	bcnd.n	ne0,  TMP3, 1f
	 st	r0,   TMP,  lo16(VME_CMMU_I0) + REG_OFF(CMMU_PFSR)
	or.u	TMP,  r0,   hi16(VME_CMMU_I1)
	ld	TMP2, TMP,  lo16(VME_CMMU_I1) + REG_OFF(CMMU_PFSR)
	st	r0,   TMP,  lo16(VME_CMMU_I1) + REG_OFF(CMMU_PFSR)
1:
	st	TMP2, r31,  REG_OFF(EF_IPFSR)

	or.u	TMP,  r0,   hi16(VME_CMMU_D0)
	ld	TMP2, TMP,  lo16(VME_CMMU_D0) + REG_OFF(CMMU_PFSR)
	extu	TMP3, TMP2, 3<16>
	bcnd.n	ne0,  TMP3, 2f
	 st	r0,   TMP,  lo16(VME_CMMU_D0) + REG_OFF(CMMU_PFSR)
	or.u	TMP,  r0,   hi16(VME_CMMU_D1)
	ld	TMP2, TMP,  lo16(VME_CMMU_D1) + REG_OFF(CMMU_PFSR)
	st	r0,   TMP,  lo16(VME_CMMU_D1) + REG_OFF(CMMU_PFSR)
2:
	br.n	_ASM_LABEL(pfsr_done)
	 st	TMP2, r31,  REG_OFF(EF_DPFSR)

5:
	/* CPU 1 -> CMMU 2 and 3 */
	or.u	TMP,  r0,   hi16(VME_CMMU_I2)
	ld	TMP2, TMP,  lo16(VME_CMMU_I2) + REG_OFF(CMMU_PFSR)
	extu	TMP3, TMP2, 3<16>
	bcnd.n	ne0,  TMP3, 1f
	 st	r0,   TMP,  lo16(VME_CMMU_I2) + REG_OFF(CMMU_PFSR)
	or.u	TMP,  r0,   hi16(VME_CMMU_I3)
	ld	TMP2, TMP,  lo16(VME_CMMU_I3) + REG_OFF(CMMU_PFSR)
	st	r0,   TMP,  lo16(VME_CMMU_I3) + REG_OFF(CMMU_PFSR)
1:
	st	TMP2, r31,  REG_OFF(EF_IPFSR)

	or.u	TMP,  r0,   hi16(VME_CMMU_D2)
	ld	TMP2, TMP,  lo16(VME_CMMU_D2) + REG_OFF(CMMU_PFSR)
	extu	TMP3, TMP2, 3<16>
	bcnd.n	ne0,  TMP3, 2f
	 st	r0,   TMP,  lo16(VME_CMMU_D2) + REG_OFF(CMMU_PFSR)
	or.u	TMP,  r0,   hi16(VME_CMMU_D3)
	ld	TMP2, TMP,  lo16(VME_CMMU_D3) + REG_OFF(CMMU_PFSR)
	st	r0,   TMP,  lo16(VME_CMMU_D3) + REG_OFF(CMMU_PFSR)
2:
	br.n	_ASM_LABEL(pfsr_done)
	 st	TMP2, r31,  REG_OFF(EF_DPFSR)

ASLOCAL(pfsr188_straight)
	/*
	 * We have the simple 2 CMMUs per CPU mapping. Pick our couple;
	 * no need to reset fault status registers in this case.
	 */
	bcnd	eq0,  TMP,  1f	/* CPU0 */
	cmp	TMP2, TMP, 0x2
	bb1	lt,   TMP2, 2f	/* CPU1 */
	bb1	eq,   TMP2, 3f	/* CPU2 */

	/* must be CPU3 */
	or.u	TMP,  r0,   hi16(VME_CMMU_I3)
	ld	TMP2, TMP,  lo16(VME_CMMU_I3) + REG_OFF(CMMU_PFSR)
	st	TMP2, r31,  REG_OFF(EF_IPFSR)
	or.u	TMP,  r0,   hi16(VME_CMMU_D3)
	ld	TMP2, TMP,  lo16(VME_CMMU_D3) + REG_OFF(CMMU_PFSR)
	br.n	_ASM_LABEL(pfsr_done)
	 st	TMP2, r31,  REG_OFF(EF_DPFSR)
1:
	/* must be CPU0 */
	or.u	TMP,  r0,   hi16(VME_CMMU_I0)
	ld	TMP2, TMP,  lo16(VME_CMMU_I0) + REG_OFF(CMMU_PFSR)
	st	TMP2, r31,  REG_OFF(EF_IPFSR)
	or.u	TMP,  r0,   hi16(VME_CMMU_D0)
	ld	TMP2, TMP,  lo16(VME_CMMU_D0) + REG_OFF(CMMU_PFSR)
	br.n	_ASM_LABEL(pfsr_done)
	 st	TMP2, r31,  REG_OFF(EF_DPFSR)
2:
	/* must be CPU1 */
	or.u	TMP,  r0,   hi16(VME_CMMU_I1)
	ld	TMP2, TMP,  lo16(VME_CMMU_I1) + REG_OFF(CMMU_PFSR)
	st	TMP2, r31,  REG_OFF(EF_IPFSR)
	or.u	TMP,  r0,   hi16(VME_CMMU_D1)
	ld	TMP2, TMP,  lo16(VME_CMMU_D1) + REG_OFF(CMMU_PFSR)
	br.n	_ASM_LABEL(pfsr_done)
	 st	TMP2, r31,  REG_OFF(EF_DPFSR)
3:
	/* must be CPU2 */
	or.u	TMP,  r0,   hi16(VME_CMMU_I2)
	ld	TMP2, TMP,  lo16(VME_CMMU_I2) + REG_OFF(CMMU_PFSR)
	st	TMP2, r31,  REG_OFF(EF_IPFSR)
	or.u	TMP,  r0,   hi16(VME_CMMU_D2)
	ld	TMP2, TMP,  lo16(VME_CMMU_D2) + REG_OFF(CMMU_PFSR)
	br.n	_ASM_LABEL(pfsr_done)
	 st	TMP2, r31,  REG_OFF(EF_DPFSR)
#endif
