/*	$NetBSD: bsd_fdintr.s,v 1.4 1995/04/25 20:01:23 pk Exp $ */

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
#ifndef LOCORE
#define LOCORE
#endif
#include "assym.s"
#include <sparc/sparc/intreg.h>
#include <sparc/sparc/auxreg.h>
#include <sparc/sparc/vaddrs.h>
#include <sparc/dev/fdreg.h>
#include <sparc/dev/fdvar.h>
/* XXX this goes in a header file -- currently, it's hidden in locore.s */
#define INTREG_ADDR 0xf8002000

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
	.global _fdciop
/* A save haven for three precious registers */
save_l:
	.word	0
	.word	0
	.word	0
/* Pointer to a `struct fdcio', set in fd.c */
_fdciop:
	.word	0

	.seg	"text"
	.align	4
	.global _fdchwintr

_fdchwintr:
	set	save_l, %l7
	std	%l0, [%l7]
	st	%l2, [%l7 + 8]

	! tally interrupt
	sethi	%hi(_cnt+V_INTR), %l7
	ld	[%l7 + %lo(_cnt+V_INTR)], %l6
	inc	%l6
	st	%l6, [%l7 + %lo(_cnt+V_INTR)]

	! load fdc, if it's NULL there's nothing to do: schedule soft interrupt
	sethi	%hi(_fdciop), %l7
	ld	[%l7 + %lo(_fdciop)], R_fdc

	! tally interrupt
	ld	[R_fdc + FDC_EVCNT], %l6
	inc	%l6
	st	%l6, [R_fdc + FDC_EVCNT]

	! load chips register addresses
	ld	[R_fdc + FDC_REG_MSR], R_msr	! get chip MSR reg addr
	ld	[R_fdc + FDC_REG_FIFO], R_fifo	! get chip FIFO reg addr
	!!ld	[R_fdc + FDC_REG_DOR], R_dor	! get chip DOR reg addr

	! find out what we are supposed to do
	ld	[R_fdc + FDC_ISTATE], %l7	! examine flags 
	cmp	%l7, ISTATE_SENSEI
	be	sensei
	 nop
	cmp	%l7, ISTATE_DMA
	bne	spurious
	 nop

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

	! xfer done: update fdc->sc_buf & fdc->sc_tc, mark istate IDLE
	st	R_tc, [R_fdc + FDC_TC]
	st	R_buf, [R_fdc + FDC_DATA]

	! flip TC bit in auxreg
	sethi	%hi(_auxio_reg), %l6
	ld	[%l6 + %lo(_auxio_reg)], %l6
	ldub	[%l6], %l7
	or	%l7, AUXIO_MB1|AUXIO_FTC, %l7
	stb	%l7, [%l6]

	! we have some time to kill; anticipate on upcoming
	! result phase.
	add	R_fdc, FDC_STATUS, R_stat	! &fdc->sc_status[0]
	mov	-1, %l7
	st	%l7, [R_fdc + FDC_NSTAT]	! fdc->sc_nstat = -1;

	ldub	[%l6], %l7
	andn	%l7, AUXIO_FTC, %l7
	or	%l7, AUXIO_MB1, %l7
	stb	%l7, [%l6]
	b	resultphase1
	 nop

spurious:
	mov	ISTATE_SPURIOUS, %l7
	st	%l7, [R_fdc + FDC_ISTATE]
	b,a	ssi

sensei:
	ldub	[R_msr], %l7
	set	POLL_TIMO, %l6
1:	deccc	%l6				! timeout?
	be	ssi
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
	be	ssi
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
	! got status, update sc_nstat and mark istate IDLE
	st	R_stcnt, [R_fdc + FDC_NSTAT]
	mov	ISTATE_IDLE, %l7
	st	%l7, [R_fdc + FDC_ISTATE]

ssi:
	! set software interrupt
	sethi	%hi(INTREG_ADDR), %l7
	ldsb	[%l7 + %lo(INTREG_ADDR)], %l6
	or	%l6, IE_L4, %l6
	stb	%l6, [%l7 + %lo(INTREG_ADDR)]

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
