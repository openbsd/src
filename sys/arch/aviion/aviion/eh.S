/*	$OpenBSD: eh.S,v 1.6 2010/04/18 22:04:37 miod Exp $	*/
/*
 * Copyright (c) 2006, 2010 Miodrag Vallat
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
	NOP;	/* will be changed to	br.n pfsr_handler + 4 */ \
	NOP	/* will be changed to	 first instruction of pfsr_handler */

#include <m88k/m88k/eh_common.S>

ENTRY(pfsr_double)
	/*
	 * 4 CMMUs per CPU. Almost as simple as the 2 CMMUs per CPU
	 * situation...
	 *
	 * However, to be sure we don't get fooled with already-processed
	 * exceptions, we have to reset the fault status registers
	 * after reading them.
	 */
	ld	TMP2, TMP,  CI_PFSR_I0
	ld	TMP3, TMP2, r0
	extu	TMP3, TMP3, 3<16>
	bcnd	ne0,  TMP3, 1f
	st	r0,   TMP2, r0
	ld	TMP2, TMP,  CI_PFSR_I1
1:
	ld	TMP3, TMP2, r0
	st	r0,   TMP2, r0
	st	TMP3, r31,  EF_IPFSR

	ld	TMP2, TMP,  CI_PFSR_D0
	ld	TMP3, TMP2, r0
	extu	TMP3, TMP3, 3<16>
	bcnd	ne0,  TMP3, 2f
	st	r0,   TMP2, r0
	ld	TMP2, TMP,  CI_PFSR_D1
2:
	ld	TMP3, TMP2, r0
	st	r0,   TMP2, r0
	br.n	_ASM_LABEL(pfsr_done)
	 st	TMP3, r31,  EF_DPFSR

ENTRY(pfsr_straight)
	/*
	 * We have the simple 2 CMMUs per CPU mapping. Pick our couple;
	 * no need to reset fault status registers in this case.
	 */
	ld	TMP2, TMP,  CI_PFSR_I0
	ld	TMP3, TMP2, r0
	st	TMP3, r31,  EF_IPFSR
	ld	TMP2, TMP,  CI_PFSR_D0
	ld	TMP3, TMP2, r0
	br.n	_ASM_LABEL(pfsr_done)
	 st	TMP3, r31,  EF_DPFSR

ENTRY(pfsr_six)
	/*
	 * 6 CMMU (2D4I) per CPU.
	 * This is the worst situation. We handle the data CMMU first,
	 * using the cpu_info pointers; and then the instruction CMMU,
	 * relying upon them being set up every 0x1000 bytes in the
	 * address space.
	 */
	ld	TMP2, TMP,  CI_PFSR_D0
	ld	TMP3, TMP2, r0
	extu	TMP3, TMP3, 3<16>
	bcnd	ne0,  TMP3, 1f
	st	r0,   TMP2, r0
	ld	TMP2, TMP,  CI_PFSR_D1
1:
	ld	TMP3, TMP2, r0
	st	r0,   TMP2, r0
	st	TMP3, r31,  EF_DPFSR

	ld	TMP2, TMP,  CI_PFSR_I0
	ld	TMP3, TMP2, r0
	extu	TMP3, TMP3, 3<16>
	bcnd	ne0,  TMP3, 2f
	st	r0,   TMP2, r0

	addu	TMP2, TMP2, 0x1000	/* I1 */
	ld	TMP3, TMP2, r0
	extu	TMP3, TMP3, 3<16>
	bcnd	ne0,  TMP3, 2f
	st	r0,   TMP2, r0

	addu	TMP2, TMP2, 0x1000	/* I2 */
	ld	TMP3, TMP2, r0
	extu	TMP3, TMP3, 3<16>
	bcnd	ne0,  TMP3, 2f
	st	r0,   TMP2, r0

	addu	TMP2, TMP2, 0x1000	/* I3 */
2:
	ld	TMP3, TMP2, r0
	st	r0,   TMP2, r0
	br.n	_ASM_LABEL(pfsr_done)
	 st	TMP3, r31,  EF_IPFSR
