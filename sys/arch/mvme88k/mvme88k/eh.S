/*	$OpenBSD: eh.S,v 1.64 2006/04/17 16:08:01 miod Exp $	*/
/*
 * Copyright (c) 2006, Miodrag Vallat
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <machine/m8820x.h>

/*
 * Save Pbus fault status register from data and inst CMMU.
 *
 * PFSR_SAVE is invoked with TMP, TMP2 and TMP3 available, and TMP contains
 * curcpu().
 */

/*
 * Single-processor SBC version (MVME187, MVME8120)
 */

#ifdef MVME187
#include <machine/mvme187.h>
#define	PFSR_SAVE_187 \
	ld	TMP2, TMP,  CI_PFSR_I0; \
	ld	TMP3, TMP2, r0; \
	st	TMP3, r31,  REG_OFF(EF_IPFSR); \
	ld	TMP2, TMP,  CI_PFSR_D0; \
	ld	TMP3, TMP2, r0; \
	st	TMP3, r31,  REG_OFF(EF_DPFSR);
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
	or.u	TMP3, r0,   hi16(_C_LABEL(brdtyp)); \
	ld	TMP2, TMP3, lo16(_C_LABEL(brdtyp)); \
	cmp	TMP3, TMP2, BRD_188; \
	bb1	eq,   TMP3, _ASM_LABEL(pfsr188); \
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
 * This implementation tries to favor, speedwise, simple (2 CMMUs per CPU)
 * configurations over any other configuration.
 */
	text

ASLOCAL(pfsr188)
	or.u	TMP2, r0,   hi16(_C_LABEL(cmmu_shift))
	ld	TMP2, TMP2, lo16(_C_LABEL(cmmu_shift))
	cmp	TMP3, TMP2, 2
	bb1	lt,   TMP3, _ASM_LABEL(pfsr188_straight)
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

	ld	TMP2, TMP,  CI_PFSR_I0
	ld	TMP2, TMP2, r0
	extu	TMP3, TMP2, 3<16>
	bcnd.n	ne0,  TMP3, 1f
	 st	r0,   TMP2, r0
	ld	TMP2, TMP,  CI_PFSR_I1
	ld	TMP2, TMP2, r0
	st	r0,   TMP2, r0
1:
	st	TMP2, r31,  REG_OFF(EF_IPFSR)

	ld	TMP2, TMP,  CI_PFSR_D0
	ld	TMP2, TMP2, r0
	extu	TMP3, TMP2, 3<16>
	bcnd.n	ne0,  TMP3, 2f
	 st	r0,   TMP2, r0
	ld	TMP2, TMP,  CI_PFSR_D1
	ld	TMP2, TMP2, r0
	st	r0,   TMP2, r0
2:
	br.n	_ASM_LABEL(pfsr_done)
	 st	TMP2, r31,  REG_OFF(EF_DPFSR)

ASLOCAL(pfsr188_straight)
	/*
	 * We have the simple 2 CMMUs per CPU mapping. Pick our couple;
	 * no need to reset fault status registers in this case.
	 * This is similar to the 187/8120 code.
	 */
	ld	TMP2, TMP,  CI_PFSR_I0
	ld	TMP3, TMP2, r0
	st	TMP3, r31,  REG_OFF(EF_IPFSR)
	ld	TMP2, TMP,  CI_PFSR_D0
	ld	TMP3, TMP2, r0
	br.n	_ASM_LABEL(pfsr_done)
	 st	TMP3, r31,  REG_OFF(EF_DPFSR)
#endif
