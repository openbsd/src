/*	$OpenBSD: bsd_fdintr.s,v 1.12 2009/04/10 20:53:54 miod Exp $	*/
/*	$NetBSD: bsd_fdintr.s,v 1.11 1997/04/07 21:00:36 pk Exp $ */

/*
 * Copyright (c) 1995 Paul Kranenburg
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Paul Kranenburg.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef FDC_C_HANDLER
#include "assym.h"
#include <machine/param.h>
#include <machine/psl.h>
#include <machine/asm.h>
#include <sparc/sparc/intreg.h>
#include <sparc/sparc/auxioreg.h>
#include <sparc/sparc/vaddrs.h>
#include <sparc/dev/fdreg.h>
#include <sparc/dev/fdvar.h>

/*
 * Note the following code hardcodes soft interrupt level 4, instead of
 * picking the actual bits from the softintr cookie. We don't have enough
 * free registers to be able to pick it easily, anyway; it's just not
 * worth doing.
 */

#define FD_SET_SWINTR_4C				\
	sethi	%hi(INTRREG_VA), %l5;			\
	ldub	[%l5 + %lo(INTRREG_VA)], %l6;		\
	or	%l6, IE_L4, %l6;			\
	stb	%l6, [%l5 + %lo(INTRREG_VA)]

! raise(0,IPL_FDSOFT)	! NOTE: CPU#0 and IPL_FDSOFT=4
#define FD_SET_SWINTR_4M				\
	sethi	%hi(1 << (16 + 4)), %l5;		\
	set	ICR_PI_SET, %l6;			\
	st	%l5, [%l6]

/* set software interrupt */
#if (defined(SUN4) || defined(SUN4C)) && !defined(SUN4M)
#define FD_SET_SWINTR	FD_SET_SWINTR_4C
#elif !(defined(SUN4) || defined(SUN4C)) && defined(SUN4M)
#define FD_SET_SWINTR	FD_SET_SWINTR_4M
#else
#define FD_SET_SWINTR					\
	sethi	%hi(_C_LABEL(cputyp)), %l5;		\
	ld	[%l5 + %lo(_C_LABEL(cputyp))], %l5;	\
	cmp	%l5, CPU_SUN4M;				\
	be	8f;					\
	FD_SET_SWINTR_4C;				\
	ba,a	9f;					\
8:							\
	FD_SET_SWINTR_4M;				\
9:
#endif

! flip TC bit in auxreg
! assumes %l6 remains unchanged between ASSERT and DEASSERT
#define FD_ASSERT_TC_4C					\
	sethi	%hi(AUXREG_VA), %l6;			\
	ldub	[%l6 + %lo(AUXREG_VA) + 3], %l7;	\
	or	%l7, AUXIO4C_MB1|AUXIO4C_FTC, %l7;	\
	stb	%l7, [%l6 + %lo(AUXREG_VA) + 3];

#define FD_DEASSERT_TC_4C				\
	ldub	[%l6 + %lo(AUXREG_VA) + 3], %l7;	\
	andn	%l7, AUXIO4C_FTC, %l7;			\
	or	%l7, AUXIO4C_MB1, %l7;			\
	stb	%l7, [%l6 + %lo(AUXREG_VA) + 3];

! flip TC bit in auxreg
#define FD_ASSERT_TC_4M					\
	sethi	%hi(AUXREG_VA), %l6;			\
	ldub	[%l6 + %lo(AUXREG_VA) + 3], %l7;	\
	or	%l7, AUXIO4M_MB1|AUXIO4M_FTC, %l7;	\
	stb	%l7, [%l6 + %lo(AUXREG_VA) + 3];

#define FD_DEASSERT_TC_4M

/*
 * flip TC bit in auxreg
 * assumes %l5 remains unchanged between ASSERT and DEASSERT
 */
#if (defined(SUN4) || defined(SUN4C)) && !defined(SUN4M)
#define FD_ASSERT_TC		FD_ASSERT_TC_4C
#define FD_DEASSERT_TC		FD_DEASSERT_TC_4C
#elif !(defined(SUN4) || defined(SUN4C)) && defined(SUN4M)
#define FD_ASSERT_TC		FD_ASSERT_TC_4M
#define FD_DEASSERT_TC		FD_DEASSERT_TC_4M
#else
#define FD_ASSERT_TC					\
	sethi	%hi(_C_LABEL(cputyp)), %l5;		\
	ld	[%l5 + %lo(_C_LABEL(cputyp))], %l5;	\
	cmp	%l5, CPU_SUN4M;				\
	be	8f;					\
	 nop;						\
	FD_ASSERT_TC_4C;				\
	ba,a	9f;					\
8:							\
	FD_ASSERT_TC_4M;				\
9:
#define FD_DEASSERT_TC					\
	cmp	%l5, CPU_SUN4M;				\
	be	8f;					\
	 nop;						\
	FD_DEASSERT_TC_4C;				\
	ba,a	9f;					\
8:							\
	FD_DEASSERT_TC_4M;				\
9:
#endif


/* Timeout waiting for chip ready */
#define POLL_TIMO	100000

/*
 * register mnemonics. note overlapping assignments.
 */
#define R_fdc	%l0
#define R_msr	%l1
#define R_fifo	%l2
#define R_buf	%l3
#define R_tc	%l4
#define R_stat	%l3
#define R_nstat	%l4
#define R_stcnt	%l5
/* use %l6 and %l7 as short term temporaries */


	.seg	"data"
	.align	8
	.global _C_LABEL(fdciop)
/* A save haven for three precious registers */
save_l:
	.word	0
	.word	0
	.word	0
/* Pointer to a `struct fdcio', set in fd.c */
_C_LABEL(fdciop):
	.word	0

	.seg	"text"
	.align	4
	.global _C_LABEL(fdchwintr)

_C_LABEL(fdchwintr):
	set	save_l, %l7
	std	%l0, [%l7]
	st	%l2, [%l7 + 8]

	! tally interrupt
	sethi	%hi(_C_LABEL(uvmexp)+V_INTR), %l7
	ld	[%l7 + %lo(_C_LABEL(uvmexp)+V_INTR)], %l6
	inc	%l6
	st	%l6, [%l7 + %lo(_C_LABEL(uvmexp)+V_INTR)]

	! load fdc, if it's NULL there's nothing to do: schedule soft interrupt
	sethi	%hi(_C_LABEL(fdciop)), %l7
	ld	[%l7 + %lo(_C_LABEL(fdciop))], R_fdc

	! tally interrupt
	ldd	[R_fdc + FDC_COUNT], %l4
	inccc	%l4
	addx	%l4, 0, %l4
	std	%l4, [R_fdc + FDC_COUNT]

	! load chips register addresses
	ld	[R_fdc + FDC_REG_MSR], R_msr	! get chip MSR reg addr
	ld	[R_fdc + FDC_REG_FIFO], R_fifo	! get chip FIFO reg addr
	!!ld	[R_fdc + FDC_REG_DOR], R_dor	! get chip DOR reg addr

	! find out what we are supposed to do
	ld	[R_fdc + FDC_ITASK], %l7	! get task from fdc
	cmp	%l7, FDC_ITASK_SENSEI
	be	sensei
	 !nop
	cmp	%l7, FDC_ITASK_RESULT
	be	resultphase
	 !nop
	cmp	%l7, FDC_ITASK_DMA
	bne,a	ssi				! a spurious interrupt
	 mov	FDC_ISTATUS_SPURIOUS, %l7	! set status and post sw intr

	! pseudo DMA
	ld	[R_fdc + FDC_TC], R_tc		! residual count
	ld	[R_fdc + FDC_DATA], R_buf	! IO buffer

	ldub	[R_msr], %l7			! get MSR value
nextc:
	btst	NE7_RQM, %l7			! room in fifo?
	bnz,a	0f
	 btst	NE7_NDM, %l7			! overrun?

	! we filled/emptied the FIFO; update fdc->sc_buf & fdc->sc_tc
	st	R_tc, [R_fdc + FDC_TC]
	b	x
	st	R_buf, [R_fdc + FDC_DATA]

0:
	bz	resultphase			! overrun/underrun
	btst	NE7_DIO, %l7			! IO direction
	bz	1f
	 deccc	R_tc
	ldub	[R_fifo], %l7			! reading:
	b	2f
	stb	%l7, [R_buf]			!    *fdc->sc_bufp = *reg_fifo

1:
	ldub	[R_buf], %l7			! writing:
	stb	%l7, [R_fifo]			!    *reg_fifo = *fdc->sc_bufp
2:
	inc	R_buf				! fdc->sc_bufp++
	bne,a	nextc				! if (--fdc->sc_tc) goto ...
	 ldub	[R_msr], %l7			! get MSR value

	! xfer done: update fdc->sc_buf & fdc->sc_tc, mark istate DONE
	st	R_tc, [R_fdc + FDC_TC]
	st	R_buf, [R_fdc + FDC_DATA]

	! flip TC bit in auxreg
	FD_ASSERT_TC

	! we have some time to kill; anticipate on upcoming
	! result phase.
	add	R_fdc, FDC_STATUS, R_stat	! &fdc->sc_status[0]
	mov	-1, %l7
	st	%l7, [R_fdc + FDC_NSTAT]	! fdc->sc_nstat = -1;

	FD_DEASSERT_TC
	b,a	resultphase1

sensei:
	ldub	[R_msr], %l7
	set	POLL_TIMO, %l6
1:	deccc	%l6				! timeout?
	be,a	ssi				! if so, set status
	 mov	FDC_ISTATUS_ERROR, %l7		! and post sw interrupt
	and	%l7, (NE7_RQM | NE7_DIO | NE7_CB), %l7
	cmp	%l7, NE7_RQM
	bne,a	1b				! loop till chip ready
	 ldub	[R_msr], %l7
	mov	NE7CMD_SENSEI, %l7
	stb	%l7, [R_fifo]

resultphase:
	! prepare for result phase
	add	R_fdc, FDC_STATUS, R_stat	! &fdc->sc_status[0]
	mov	-1, %l7
	st	%l7, [R_fdc + FDC_NSTAT]	! fdc->sc_nstat = -1;

resultphase1:
	clr	R_stcnt
	ldub	[R_msr], %l7
	set	POLL_TIMO, %l6
1:	deccc	%l6				! timeout?
	be,a	ssi				! if so, set status
	 mov	FDC_ISTATUS_ERROR, %l7		! and post sw interrupt
	and	%l7, (NE7_RQM | NE7_DIO | NE7_CB), %l7
	cmp	%l7, NE7_RQM
	be	3f				! done
	cmp	%l7, (NE7_RQM | NE7_DIO | NE7_CB)
	bne,a	1b				! loop till chip ready
	 ldub	[R_msr], %l7

	cmp	R_stcnt, FDC_NSTATUS		! status overrun?
	bge	2f				! if so, load but dont store
	ldub	[R_fifo], %l7			! load the status byte
	stb	%l7, [R_stat]
	inc	R_stat
	inc	R_stcnt
2:	b	1b
	 ldub	[R_msr], %l7

3:
	! got status, update sc_nstat and mark istate DONE
	st	R_stcnt, [R_fdc + FDC_NSTAT]
	mov	FDC_ISTATUS_DONE, %l7

ssi:
	! set software interrupt
	! enter here with status in %l7
	st	%l7, [R_fdc + FDC_ISTATUS]
	FD_SET_SWINTR

x:
	/*
	 * Restore psr -- note: psr delay honored by pc restore loads.
	 */
	set	save_l, %l7
	ldd	[%l7], %l0
	mov	%l0, %psr
	 nop
	ld	[%l7 + 8], %l2
	jmp	%l1
	rett	%l2
#endif
