/* $OpenBSD: apicvec.s,v 1.5 2004/12/24 21:22:00 pvalchev Exp $ */	
/* $NetBSD: apicvec.s,v 1.1.2.2 2000/02/21 21:54:01 sommerfeld Exp $ */	

/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by RedBack Networks Inc.
 *
 * Author: Bill Sommerfeld
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
	
#include <machine/i82093reg.h>
#include <machine/i82489reg.h>	

#ifdef __ELF__
#define XINTR(vec) Xintr/**/vec
#else
#define XINTR(vec) _Xintr/**/vec
#endif

#ifdef MULTIPROCESSOR
	.globl	XINTR(ipi)
XINTR(ipi):
	pushl	$0		
	pushl	$T_ASTFLT
	INTRENTRY		
	MAKE_FRAME		
	pushl	CPL
	movl	_C_LABEL(lapic_ppr),%eax
	movl	%eax,CPL
	ioapic_asm_ack()
        sti			/* safe to take interrupts.. */
	call	_C_LABEL(i386_ipi_handler)
	jmp	_C_LABEL(Xdoreti)
#endif
	
	/*
	 * Interrupt from the local APIC timer.
	 */
	.globl	XINTR(ltimer)
XINTR(ltimer):			
	pushl	$0		
	pushl	$T_ASTFLT
	INTRENTRY		
	MAKE_FRAME		
	pushl	CPL
	movl	_C_LABEL(lapic_ppr),%eax
	movl	%eax,CPL
	ioapic_asm_ack()
	sti
#ifdef MULTIPROCESSOR
	call	_C_LABEL(i386_softintlock)
#endif
	movl	%esp,%eax
	pushl	%eax
	call	_C_LABEL(lapic_clockintr)
	addl	$4,%esp		
#ifdef MULTIPROCESSOR
	call	_C_LABEL(i386_softintunlock)
#endif
	jmp	_C_LABEL(Xdoreti)

	.globl	XINTR(softclock), XINTR(softnet), XINTR(softtty)
XINTR(softclock):
	pushl	$0		
	pushl	$T_ASTFLT
	INTRENTRY		
	MAKE_FRAME		
	pushl	CPL
	movl	$IPL_SOFTCLOCK,CPL
	andl	$~(1<<SIR_CLOCK),_C_LABEL(ipending)
	ioapic_asm_ack()
	sti
#ifdef MULTIPROCESSOR
	call	_C_LABEL(i386_softintlock)
#endif
	call	_C_LABEL(softclock)
#ifdef MULTIPROCESSOR
	call	_C_LABEL(i386_softintunlock)
#endif
	jmp	_C_LABEL(Xdoreti)
	
#define DONETISR(s, c) \
	.globl  _C_LABEL(c)	;\
	testl	$(1 << s),%edi	;\
	jz	1f		;\
	call	_C_LABEL(c)	;\
1:

XINTR(softnet):
	pushl	$0		
	pushl	$T_ASTFLT
	INTRENTRY		
	MAKE_FRAME		
	pushl	CPL
	movl	$IPL_SOFTNET,CPL
	andl	$~(1<<SIR_NET),_C_LABEL(ipending)
	ioapic_asm_ack()
	sti
#ifdef MULTIPROCESSOR
	call	_C_LABEL(i386_softintlock)
#endif
	xorl	%edi,%edi
	xchgl	_C_LABEL(netisr),%edi
#include <net/netisr_dispatch.h>
#ifdef MULTIPROCESSOR
	call	_C_LABEL(i386_softintunlock)
#endif
	jmp	_C_LABEL(Xdoreti)
#undef DONETISR

XINTR(softtty):	
	pushl	$0		
	pushl	$T_ASTFLT
	INTRENTRY		
	MAKE_FRAME		
	pushl	CPL
	movl	$IPL_SOFTTTY,CPL
	andl	$~(1<<SIR_TTY),_C_LABEL(ipending)
	ioapic_asm_ack()
	sti
#ifdef MULTIPROCESSOR
	call	_C_LABEL(i386_softintlock)
#endif
	call	_C_LABEL(comsoft)
#ifdef MULTIPROCESSOR
	call	_C_LABEL(i386_softintunlock)
#endif
	jmp	_C_LABEL(Xdoreti)

#if NIOAPIC > 0

#define voidop(num)

	/*
	 * I/O APIC interrupt.
	 * We sort out which one is which based on the value of 
	 * the processor priority register.
	 *
	 * XXX no stray interrupt mangling stuff..
	 * XXX use cmove when appropriate.
	 */
	
#define APICINTR(name, num, early_ack, late_ack, mask, unmask, level_mask) \
_C_LABEL(Xintr_/**/name/**/num):					\
	pushl	$0							;\
	pushl	$T_ASTFLT						;\
	INTRENTRY							;\
	MAKE_FRAME							;\
	pushl	CPL							;\
	movl	_C_LABEL(lapic_ppr),%eax				;\
	orl	$num,%eax						;\
	movl	_C_LABEL(apic_maxlevel)(,%eax,4),%ebx			;\
	movl	%ebx,CPL						;\
	mask(num)			/* mask it in hardware */	;\
	early_ack(num)			/* and allow other intrs */	;\
	incl	MY_COUNT+V_INTR		/* statistical info */		;\
	sti								;\
	incl	_C_LABEL(apic_intrcount)(,%eax,4)			;\
	movl	_C_LABEL(apic_intrhand)(,%eax,4),%ebx /* chain head */	;\
	testl	%ebx,%ebx						;\
	jz	8f			/* oops, no handlers.. */	;\
7:									 \
	LOCK_KERNEL(IF_PPL(%esp))					;\
	movl	IH_ARG(%ebx),%eax	/* get handler arg */		;\
	testl	%eax,%eax						;\
	jnz	6f							;\
	movl	%esp,%eax		/* 0 means frame pointer */	;\
6:									 \
	pushl	%eax							;\
	call	*IH_FUN(%ebx)		/* call it */			;\
	addl	$4,%esp			/* toss the arg */		;\
	orl	%eax,%eax		/* should it be counted? */	;\
	jz	4f							;\
	addl	$1,IH_COUNT(%ebx)	/* count the intrs */		;\
	adcl	$0,IH_COUNT+4(%ebx)					;\
4:									 \
	UNLOCK_KERNEL(IF_PPL(%esp))					;\
	movl	IH_NEXT(%ebx),%ebx	/* next handler in chain */	;\
	testl	%ebx,%ebx						;\
	jnz	7b							;\
8:									 \
	unmask(num)			/* unmask it in hardware */	;\
	late_ack(num)							;\
	jmp	_C_LABEL(Xdoreti)
	
APICINTR(ioapic,0, voidop, ioapic_asm_ack, voidop, voidop, voidop)
APICINTR(ioapic,1, voidop, ioapic_asm_ack, voidop, voidop, voidop)
APICINTR(ioapic,2, voidop, ioapic_asm_ack, voidop, voidop, voidop)
APICINTR(ioapic,3, voidop, ioapic_asm_ack, voidop, voidop, voidop)
APICINTR(ioapic,4, voidop, ioapic_asm_ack, voidop, voidop, voidop)
APICINTR(ioapic,5, voidop, ioapic_asm_ack, voidop, voidop, voidop)
APICINTR(ioapic,6, voidop, ioapic_asm_ack, voidop, voidop, voidop)
APICINTR(ioapic,7, voidop, ioapic_asm_ack, voidop, voidop, voidop)
APICINTR(ioapic,8, voidop, ioapic_asm_ack, voidop, voidop, voidop)
APICINTR(ioapic,9, voidop, ioapic_asm_ack, voidop, voidop, voidop)
APICINTR(ioapic,10, voidop, ioapic_asm_ack, voidop, voidop, voidop)
APICINTR(ioapic,11, voidop, ioapic_asm_ack, voidop, voidop, voidop)
APICINTR(ioapic,12, voidop, ioapic_asm_ack, voidop, voidop, voidop)
APICINTR(ioapic,13, voidop, ioapic_asm_ack, voidop, voidop, voidop)
APICINTR(ioapic,14, voidop, ioapic_asm_ack, voidop, voidop, voidop)
APICINTR(ioapic,15, voidop, ioapic_asm_ack, voidop, voidop, voidop)

	.globl	_C_LABEL(Xintr_ioapic0),_C_LABEL(Xintr_ioapic1)
	.globl	_C_LABEL(Xintr_ioapic2),_C_LABEL(Xintr_ioapic3)
	.globl	_C_LABEL(Xintr_ioapic4),_C_LABEL(Xintr_ioapic5)
	.globl	_C_LABEL(Xintr_ioapic6),_C_LABEL(Xintr_ioapic7)
	.globl	_C_LABEL(Xintr_ioapic8),_C_LABEL(Xintr_ioapic9)
	.globl	_C_LABEL(Xintr_ioapic10),_C_LABEL(Xintr_ioapic11)
	.globl	_C_LABEL(Xintr_ioapic12),_C_LABEL(Xintr_ioapic13)
	.globl	_C_LABEL(Xintr_ioapic14),_C_LABEL(Xintr_ioapic15)
	.globl _C_LABEL(apichandler)

_C_LABEL(apichandler):	
	.long	_C_LABEL(Xintr_ioapic0),_C_LABEL(Xintr_ioapic1)
	.long	_C_LABEL(Xintr_ioapic2),_C_LABEL(Xintr_ioapic3)
	.long	_C_LABEL(Xintr_ioapic4),_C_LABEL(Xintr_ioapic5)
	.long	_C_LABEL(Xintr_ioapic6),_C_LABEL(Xintr_ioapic7)
	.long	_C_LABEL(Xintr_ioapic8),_C_LABEL(Xintr_ioapic9)
	.long	_C_LABEL(Xintr_ioapic10),_C_LABEL(Xintr_ioapic11)
	.long	_C_LABEL(Xintr_ioapic12),_C_LABEL(Xintr_ioapic13)
	.long	_C_LABEL(Xintr_ioapic14),_C_LABEL(Xintr_ioapic15)

#endif
	
