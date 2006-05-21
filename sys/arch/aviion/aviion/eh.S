/*	$OpenBSD: eh.S,v 1.2 2006/05/21 12:22:01 miod Exp $	*/
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
 */

#define	PFSR_SAVE \
	br.n	_ASM_LABEL(pfsr_av400); \
	 or.u	TMP2, r0,   hi16(_C_LABEL(cmmu_shift))

#include <m88k/m88k/eh_common.S>

	text

ASLOCAL(pfsr_av400)
#if 0	/* done in delay slot from PFSR_SAVE */
	or.u	TMP2, r0,   hi16(_C_LABEL(cmmu_shift))
#endif
	ld	TMP2, TMP2, lo16(_C_LABEL(cmmu_shift))
	cmp	TMP3, TMP2, 2
	bb1	lt,   TMP2, _ASM_LABEL(pfsr_av400_straight)

ASLOCAL(pfsr_av400_double)
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

ASLOCAL(pfsr_av400_straight)
	/*
	 * We have the simple 2 CMMUs per CPU mapping. Pick our couple;
	 * no need to reset fault status registers in this case.
	 */
	ld	TMP2, TMP,  CI_PFSR_I0
	ld	TMP3, TMP2, r0
	st	TMP3, r31,  REG_OFF(EF_IPFSR)
	ld	TMP2, TMP,  CI_PFSR_D0
	ld	TMP3, TMP2, r0
	br.n	_ASM_LABEL(pfsr_done)
	 st	TMP3, r31,  REG_OFF(EF_DPFSR)
