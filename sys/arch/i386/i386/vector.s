/*	$OpenBSD: vector.s,v 1.19 2015/04/25 21:31:24 guenther Exp $	*/
/*	$NetBSD: vector.s,v 1.32 1996/01/07 21:29:47 mycroft Exp $	*/

/*
 * Copyright (c) 1993, 1994, 1995 Charles M. Hannum.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *	notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *	notice, this list of conditions and the following disclaimer in the
 *	documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *	must display the following acknowledgement:
 *	This product includes software developed by Charles M. Hannum.
 * 4. The name of the author may not be used to endorse or promote products
 *	derived from this software without specific prior written permission.
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
 */

#include <machine/i8259.h>
#include <dev/isa/isareg.h>

/*
 * Macros for interrupt entry, call to handler, and exit.
 *
 * XXX
 * The interrupt frame is set up to look like a trap frame.  This may be a
 * waste.  The only handler which needs a frame is the clock handler, and it
 * only needs a few bits.  Xdoreti() needs a trap frame for handling ASTs, but
 * it could easily convert the frame on demand.
 *
 * The direct costs of setting up a trap frame are two pushl's (error code and
 * trap number), an addl to get rid of these, and pushing and popping the
 * callee-saved registers %esi, %edi, %ebx, and %ebp twice.
 *
 * If the interrupt frame is made more flexible,  INTR can push %eax first and
 * decide the ipending case with less overhead, e.g., by avoiding loading the
 * segment registers.
 */

	.globl	_C_LABEL(isa_strayintr)

#define voidop(num)

/*
 * Normal vectors.
 *
 * We cdr down the intrhand chain, calling each handler with its appropriate
 * argument (0 meaning a pointer to the frame, for clock interrupts).
 *
 * The handler returns one of three values:
 *   0 - This interrupt wasn't for me.
 *   1 - This interrupt was for me.
 *  -1 - This interrupt might have been for me, but I don't know.
 * If there are no handlers, or they all return 0, we flag it as a `stray'
 * interrupt.  On a system with level-triggered interrupts, we could terminate
 * immediately when one of them returns 1; but this is a PC.
 *
 * On exit, we jump to Xdoreti(), to process soft interrupts and ASTs.
 */
#define	INTRSTUB(name, num, early_ack, late_ack, mask, unmask, level_mask) \
IDTVEC(resume_##name##num)						;\
	push	%ebx							;\
	cli								;\
	jmp	1f							;\
IDTVEC(recurse_##name##num)						;\
	pushfl								;\
	pushl	%cs							;\
	pushl	%esi							;\
	pushl	$0			/* dummy error code */		;\
	pushl	$T_ASTFLT		/* trap # for doing ASTs */	;\
	movl	%ebx,%esi						;\
	INTRENTRY							;\
	MAKE_FRAME							;\
	push	%esi							;\
	cli								;\
	jmp	1f							;\
_C_LABEL(Xintr_##name##num):						;\
	pushl	$0			/* dummy error code */		;\
	pushl	$T_ASTFLT		/* trap # for doing ASTs */	;\
	INTRENTRY							;\
	MAKE_FRAME							;\
	mask(num)			/* mask it in hardware */	;\
	early_ack(num)			/* and allow other intrs */	;\
	incl	_C_LABEL(uvmexp)+V_INTR	/* statistical info */		;\
	movl	_C_LABEL(iminlevel) + (num) * 4, %eax			;\
	movl	CPL,%ebx						;\
	cmpl	%eax,%ebx						;\
	jae	_C_LABEL(Xhold_##name##num)/* currently masked; hold it */;\
	pushl	%ebx			/* cpl to restore on exit */	;\
1:									;\
	movl	_C_LABEL(imaxlevel) + (num) * 4,%eax			;\
	movl	%eax,CPL		/* block enough for this irq */	;\
	sti				/* safe to take intrs now */	;\
	movl	_C_LABEL(intrhand) + (num) * 4,%ebx	/* head of chain */ ;\
	testl	%ebx,%ebx						;\
	jz	_C_LABEL(Xstray_##name##num)	/* no handlers; we're stray */	;\
	STRAY_INITIALIZE		/* nobody claimed it yet */	;\
	incl	CPUVAR(IDEPTH)						;\
7:	movl	%esp, %eax		/* save frame pointer in eax */	;\
	pushl	%ebx			/* arg 2: ih structure */	;\
	pushl	%eax			/* arg 1: frame pointer */	;\
	call	_C_LABEL(intr_handler)	/* call it */			;\
	addl	$8, %esp		/* toss args */			;\
	STRAY_INTEGRATE			/* maybe he claimed it */	;\
	orl	%eax,%eax		/* should it be counted? */	;\
	jz	5f			/* no, skip it */		;\
	addl	$1,IH_COUNT(%ebx)	/* count the intrs */		;\
	adcl	$0,IH_COUNT+4(%ebx)					;\
	cmpl	$0,_C_LABEL(intr_shared_edge)				;\
	jne	5f			 /* if no shared edges ... */	;\
	orl	%eax,%eax		/* ... 1 means stop trying */	;\
	jns	8f							;\
5:	movl	IH_NEXT(%ebx),%ebx	/* next handler in chain */	;\
	testl	%ebx,%ebx						;\
	jnz	7b							;\
8:	decl	CPUVAR(IDEPTH)						;\
	STRAY_TEST(name,num)		/* see if it's a stray */	;\
6:	unmask(num)			/* unmask it in hardware */	;\
	late_ack(num)							;\
	jmp	_C_LABEL(Xdoreti)	/* lower spl and do ASTs */	;\
IDTVEC(stray_##name##num)						;\
	pushl	$num							;\
	call	_C_LABEL(isa_strayintr)					;\
	addl	$4,%esp							;\
	jmp	6b							;\
IDTVEC(hold_##name##num)						;\
	orb	$IRQ_BIT(num),CPUVAR(IPENDING) + IRQ_BYTE(num)	;\
	INTRFASTEXIT

#if defined(DEBUG)
#define	STRAY_INITIALIZE \
	xorl	%esi,%esi
#define	STRAY_INTEGRATE \
	orl	%eax,%esi
#define	STRAY_TEST(name,num) \
	testl	%esi,%esi						;\
	jz	_C_LABEL(Xstray_##name##num)
#else /* !DEBUG */
#define	STRAY_INITIALIZE
#define	STRAY_INTEGRATE
#define	STRAY_TEST(name,num)
#endif /* DEBUG */

#ifdef DDB
#define	MAKE_FRAME \
	leal	-8(%esp),%ebp
#else /* !DDB */
#define	MAKE_FRAME
#endif /* DDB */

#define ICUADDR IO_ICU1

INTRSTUB(legacy,0, i8259_asm_ack1, voidop, i8259_asm_mask, i8259_asm_unmask,
    voidop)
INTRSTUB(legacy,1, i8259_asm_ack1, voidop, i8259_asm_mask, i8259_asm_unmask,
    voidop)
INTRSTUB(legacy,2, i8259_asm_ack1, voidop, i8259_asm_mask, i8259_asm_unmask,
    voidop)
INTRSTUB(legacy,3, i8259_asm_ack1, voidop, i8259_asm_mask, i8259_asm_unmask,
    voidop)
INTRSTUB(legacy,4, i8259_asm_ack1, voidop, i8259_asm_mask, i8259_asm_unmask,
    voidop)
INTRSTUB(legacy,5, i8259_asm_ack1, voidop, i8259_asm_mask, i8259_asm_unmask,
    voidop)
INTRSTUB(legacy,6, i8259_asm_ack1, voidop, i8259_asm_mask, i8259_asm_unmask,
    voidop)
INTRSTUB(legacy,7, i8259_asm_ack1, voidop, i8259_asm_mask, i8259_asm_unmask,
    voidop)

#undef ICUADDR
#define ICUADDR IO_ICU2

INTRSTUB(legacy,8, i8259_asm_ack2, voidop, i8259_asm_mask, i8259_asm_unmask,
    voidop)
INTRSTUB(legacy,9, i8259_asm_ack2, voidop, i8259_asm_mask, i8259_asm_unmask,
    voidop)
INTRSTUB(legacy,10, i8259_asm_ack2, voidop, i8259_asm_mask, i8259_asm_unmask,
    voidop)
INTRSTUB(legacy,11, i8259_asm_ack2, voidop, i8259_asm_mask, i8259_asm_unmask,
    voidop)
INTRSTUB(legacy,12, i8259_asm_ack2, voidop, i8259_asm_mask, i8259_asm_unmask,
    voidop)
INTRSTUB(legacy,13, i8259_asm_ack2, voidop, i8259_asm_mask, i8259_asm_unmask,
    voidop)
INTRSTUB(legacy,14, i8259_asm_ack2, voidop, i8259_asm_mask, i8259_asm_unmask,
    voidop)
INTRSTUB(legacy,15, i8259_asm_ack2, voidop, i8259_asm_mask, i8259_asm_unmask,
    voidop)

/*
 * These tables are used by the ISA configuration code.
 */
/* interrupt service routine entry points */
IDTVEC(intr)
	.long   _C_LABEL(Xintr_legacy0), _C_LABEL(Xintr_legacy1)
	.long	_C_LABEL(Xintr_legacy2), _C_LABEL(Xintr_legacy3)
	.long	_C_LABEL(Xintr_legacy4), _C_LABEL(Xintr_legacy5)
	.long	_C_LABEL(Xintr_legacy6), _C_LABEL(Xintr_legacy7)
	.long	_C_LABEL(Xintr_legacy8), _C_LABEL(Xintr_legacy9)
	.long	_C_LABEL(Xintr_legacy10), _C_LABEL(Xintr_legacy11)
	.long	_C_LABEL(Xintr_legacy12), _C_LABEL(Xintr_legacy13)
	.long	_C_LABEL(Xintr_legacy14), _C_LABEL(Xintr_legacy15)

/*
 * These tables are used by Xdoreti() and Xspllower().
 */
/* resume points for suspended interrupts */
IDTVEC(resume)
	.long	_C_LABEL(Xresume_legacy0), _C_LABEL(Xresume_legacy1)
	.long	_C_LABEL(Xresume_legacy2), _C_LABEL(Xresume_legacy3)
	.long	_C_LABEL(Xresume_legacy4), _C_LABEL(Xresume_legacy5)
	.long	_C_LABEL(Xresume_legacy6), _C_LABEL(Xresume_legacy7)
	.long	_C_LABEL(Xresume_legacy8), _C_LABEL(Xresume_legacy9)
	.long	_C_LABEL(Xresume_legacy10), _C_LABEL(Xresume_legacy11)
	.long	_C_LABEL(Xresume_legacy12), _C_LABEL(Xresume_legacy13)
	.long	_C_LABEL(Xresume_legacy14), _C_LABEL(Xresume_legacy15)
	/* for soft interrupts */
	.long	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	.long	_C_LABEL(Xsofttty), _C_LABEL(Xsoftnet), _C_LABEL(Xsoftclock)
	.long	0, 0
/* fake interrupts to resume from splx() */
IDTVEC(recurse)
	.long	_C_LABEL(Xrecurse_legacy0), _C_LABEL(Xrecurse_legacy1)
	.long	_C_LABEL(Xrecurse_legacy2), _C_LABEL(Xrecurse_legacy3)
	.long	_C_LABEL(Xrecurse_legacy4), _C_LABEL(Xrecurse_legacy5)
	.long	_C_LABEL(Xrecurse_legacy6), _C_LABEL(Xrecurse_legacy7)
	.long	_C_LABEL(Xrecurse_legacy8), _C_LABEL(Xrecurse_legacy9)
	.long	_C_LABEL(Xrecurse_legacy10), _C_LABEL(Xrecurse_legacy11)
	.long	_C_LABEL(Xrecurse_legacy12), _C_LABEL(Xrecurse_legacy13)
	.long	_C_LABEL(Xrecurse_legacy14), _C_LABEL(Xrecurse_legacy15)
	/* for soft interrupts */
	.long	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	.long	_C_LABEL(Xsofttty), _C_LABEL(Xsoftnet), _C_LABEL(Xsoftclock)
	.long	0, 0
