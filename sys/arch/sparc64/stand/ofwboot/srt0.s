/*	$OpenBSD: srt0.s,v 1.6 2006/07/09 19:36:57 miod Exp $	*/
/*	$NetBSD: srt0.s,v 1.1 2000/08/20 14:58:42 mrg Exp $	*/

/*
 * Copyright (C) 1995, 1996 Wolfgang Solfrank.
 * Copyright (C) 1995, 1996 TooLs GmbH.
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <machine/psl.h>
#include <machine/param.h>
#include <machine/frame.h>
#include <machine/asm.h>

/*
 * Globals
 */
	.globl	_esym
	.data
_esym:	.word	0			/* end of symbol table */
	.globl	_C_LABEL(romp)
	.align	8
	.register %g2,	#scratch
	.register %g3,	#scratch
_C_LABEL(romp):	.xword	0		/* openfirmware entry point */

/*
 * Startup entry
 */
	.text
	.globl	_start, _C_LABEL(kernel_text)
	_C_LABEL(kernel_text) = _start
_start:
	nop			! For some reason this is needed to fixup the text section
	
	/*
	 * Step 1: Save rom entry pointer  -- NOTE this probably needs to change
	 */
	
	mov	%o4, %g7	! save prom vector pointer
	set	_C_LABEL(romp), %g1
	stx	%o4, [%g1]	! It's initialized data, I hope

	/*
	 * Start by creating a stack for ourselves.
	 */
	/* 64-bit stack */
	btst	1, %sp
	set	CC64FSZ, %g1	! Frame Size (negative)
	bnz	1f
	 set	BIAS, %g2	! Bias (negative)
	andn	%sp, 0x0f, %sp	! 16 byte align, per ELF spec.
	add	%g1, %g2, %g1	! Frame + Bias
1:
	sub	%sp, %g1, %g1
	save	%g1, %g0, %sp
	
!	mov	%i0, %i4		! Apparently we get our CIF in i0
	
	/*
	 * Set the psr into a known state:
	 * Set supervisor mode, interrupt level >= 13, traps enabled
	 */
	wrpr	%g0, 0, %pil	! So I lied
	wrpr	%g0, PSTATE_PRIV+PSTATE_IE, %pstate

	clr	%g4		! Point %g4 to start of data segment
				! only problem is that apparently the
				! start of the data segment is 0
	
	/*
	 * XXXXXXXX Need to determine what params are passed
	 */
	call	_C_LABEL(setup)
	 nop
	mov	%i1, %o1		
	call	_C_LABEL(main)
	 mov	%i2, %o0
	call	_C_LABEL(exit)
	 nop
	call	_C_LABEL(_rtt)
	 nop

/*
 * void syncicache(void *start, int size)
 *
 * I$ flush.  Really simple.  Just flush over the whole range.
 */
	.align	8
	.globl	_C_LABEL(syncicache)
_C_LABEL(syncicache):
	dec	4, %o1
	flush	%o0
	brgz,a,pt	%o1, _C_LABEL(syncicache)
	 inc	4, %o0
	retl
	 nop
	
/*
 * openfirmware(cell* param);
 *
 * OpenFirmware entry point 
 * 
 * If we're running in 32-bit mode we need to convert to a 64-bit stack
 * and 64-bit cells.  The cells we'll allocate off the stack for simplicity.
 */
	.align 8
	.globl	_C_LABEL(openfirmware)
	.proc 1
	FTYPE(openfirmware)
_C_LABEL(openfirmware):
	andcc	%sp, 1, %g0
	bz,pt	%icc, 1f
	 sethi	%hi(_C_LABEL(romp)), %o1
	
	ldx	[%o1+%lo(_C_LABEL(romp))], %o4		! v9 stack, just load the addr and callit
	save	%sp, -CC64FSZ, %sp
	mov	%i0, %o0				! Copy over our parameter
	mov	%g1, %l1
	mov	%g2, %l2
	mov	%g3, %l3
	mov	%g4, %l4
	mov	%g5, %l5
	mov	%g6, %l6
	mov	%g7, %l7
	rdpr	%pstate, %l0
	jmpl	%i4, %o7
	 wrpr	%g0, PSTATE_PROM|PSTATE_IE, %pstate
	wrpr	%l0, %g0, %pstate
	mov	%l1, %g1
	mov	%l2, %g2
	mov	%l3, %g3
	mov	%l4, %g4
	mov	%l5, %g5
	mov	%l6, %g6
	mov	%l7, %g7
	ret
	 restore	%o0, %g0, %o0

1:						! v8 -- need to screw with stack & params
	save	%sp, -CC64FSZ, %sp			! Get a new 64-bit stack frame
	add	%sp, -BIAS, %sp
	sethi	%hi(_C_LABEL(romp)), %o1
	rdpr	%pstate, %l0
	ldx	[%o1+%lo(_C_LABEL(romp))], %o1		! Do the actual call
	srl	%sp, 0, %sp
	mov	%i0, %o0
	mov	%g1, %l1
	mov	%g2, %l2
	mov	%g3, %l3
	mov	%g4, %l4
	mov	%g5, %l5
	mov	%g6, %l6
	mov	%g7, %l7
	jmpl	%o1, %o7
	 wrpr	%g0, PSTATE_PROM|PSTATE_IE, %pstate	! Enable 64-bit addresses for the prom
	wrpr	%l0, 0, %pstate
	mov	%l1, %g1
	mov	%l2, %g2
	mov	%l3, %g3
	mov	%l4, %g4
	mov	%l5, %g5
	mov	%l6, %g6
	mov	%l7, %g7
	ret
	 restore	%o0, %g0, %o0

#if 0
	.data
	.align 8
bootstack:	
#define STACK_SIZE	0x14000
	.skip	STACK_SIZE
ebootstack:			! end (top) of boot stack
#endif
