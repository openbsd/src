/*	$OpenBSD: vectors.S,v 1.5 2010/06/06 21:18:47 jasper Exp $	*/
/*	$NetBSD: exception_vector.S,v 1.19 2006/08/22 21:47:57 uwe Exp $	*/

/*-
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
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

#include "assym.h"

#include <sh/param.h>
#include <sh/locore.h>
#include <sh/trap.h>
#include <sh/ubcreg.h>
#include <sh/mmu_sh3.h>
#include <sh/mmu_sh4.h>

/* 
 * Align vectors more strictly here (where we don't really care) so
 * that .align 5 (i.e. 32B cache line) before data block does the
 * right thing w.r.t. final destinations after vectors are copied.
 */
#define _ALIGN_TEXT	.align 5
#include <sh/asm.h>

/*
 * Exception vectors.
 * The following routines are copied to vector addreses.
 *	sh_vector_generic:	VBR + 0x100
 *	sh_vector_tlbmiss:	VBR + 0x400
 *	sh_vector_interrupt:	VBR + 0x600
 */

#define VECTOR_END_MARKER(sym)			\
		.globl	_C_LABEL(sym);		\
	_C_LABEL(sym):


/*
 * LINTSTUB: Var: char sh_vector_generic[1];
 *
 * void sh_vector_generic(void);
 *	Copied to VBR+0x100.  This code should be position independent
 *	and maximum 786 bytes long (== 0x400 - 0x100).
 */
NENTRY(sh_vector_generic)
	__EXCEPTION_ENTRY
	__INTR_MASK(r0, r1)
	/* Identify exception cause */
	MOV	(EXPEVT, r0)
	mov.l	@r0,	r0
	mov.l	r0, @(TF_EXPEVT, r14)	/* trapframe->tf_expevt = EXPEVT */
	/* Get curproc */
	mov.l	.Lg_curproc, r1
	mov.l	@r1,	r4	/* 1st arg */
	/* Get TEA */
	MOV	(TEA,	r1)
	mov.l	@r1,	r6	/* 3rd arg */
	/* Check TLB exception or not */
	mov.l	.Lg_TLB_PROT_ST, r1
	cmp/hi	r1,	r0
	bt	1f

	/* tlb_exception(curlwp, trapframe, TEA); */
	mov.l	.Lg_VPN_MASK, r1
	and	r1,	r6	/* va = trunc_page(va) */
	__EXCEPTION_UNBLOCK(r0, r1)
	mov.l	.Lg_tlb_exception, r0
	jsr	@r0
	 mov	r14,	r5	/* 2nd arg */
	bra	2f
	 nop

	/* general_exception(curproc, trapframe, TEA); */
1:	mov	r4,	r8
#ifdef DDB
	mov	#0,	r2
	MOV	(BBRA, r1)
	mov.w	r2,	@r1	/* disable UBC */
	mov.l	r2,	@(TF_UBC, r14)	/* clear trapframe->tf_ubc */
#endif /* DDB */
	__EXCEPTION_UNBLOCK(r0, r1)
	mov.l	.Lg_general_exception, r0
	jsr	@r0
	 mov	r14,	r5	/* 2nd arg */

	/* Check for ASTs on exit to user mode. */
	mov	r8,	r4
	mov.l	.Lg_ast,	r0
	jsr	@r0
	 mov	r14,	r5
#ifdef DDB	/* BBRA = trapframe->tf_ubc */
	__EXCEPTION_BLOCK(r0, r1)
	mov.l	@(TF_UBC, r14), r0
	MOV	(BBRA, r1)
	mov.w	r0,	@r1
#endif /* DDB */
2:	__EXCEPTION_RETURN
	/* NOTREACHED */
	.align	5
.Lg_curproc:			.long	_C_LABEL(cpu_info_store) + CI_CURPROC
REG_SYMBOL(EXPEVT)
REG_SYMBOL(BBRA)
REG_SYMBOL(TEA)
.Lg_tlb_exception:		.long	_C_LABEL(tlb_exception)
.Lg_general_exception:		.long	_C_LABEL(general_exception)
.Lg_ast:			.long	_C_LABEL(ast)
.Lg_TLB_PROT_ST:		.long	EXPEVT_TLB_PROT_ST
.Lg_VPN_MASK:			.long	0xfffff000

/* LINTSTUB: Var: char sh_vector_generic_end[1]; */
VECTOR_END_MARKER(sh_vector_generic_end)
	SET_ENTRY_SIZE(sh_vector_generic)


#ifdef SH3
/*
 * LINTSTUB: Var: char sh3_vector_tlbmiss[1];
 *
 * void sh3_vector_tlbmiss(void);
 *	Copied to VBR+0x400.  This code should be position independent
 *	and maximum 512 bytes long (== 0x600 - 0x400).
 */
NENTRY(sh3_vector_tlbmiss)
	__EXCEPTION_ENTRY
	mov	#(SH3_TEA & 0xff), r0
	mov.l	@r0,	r6	/* 3rd arg: va = TEA */

	/* if kernel stack is in P3, handle it here fast */
#if !defined(P1_STACK)
	/* Load kernel stack */
	mov.l	.L3_VPN_MASK, r0
	and	r6,	r0	/* VPN */
	tst	r0,	r0
	bt	6f		/* punt if VPN is 0 */

	mov.l	.L3_CURUPTE, r1
	mov.l	@r1, r1		/* upte = &l->l_md.md_upte[0] */
	mov	#UPAGES, r3	/* loop limit */
	mov	#1, r2		/* loop count */

	/* for each page of u-area */
4:	mov.l	@r1+, r7	/* upte->addr: u-area VPN */
	cmp/eq	r7, r0		/* if (vpn == upte->addr) */
	bt	5f		/*     goto found; */
	add	#4, r1		/* skip, upte->data; point to next md_upte[i] */
	cmp/eq	r2,	r3
	bf/s	4b
	 add	#1,	r2
	/* not a page of u-area, proceed to handler */
	bra	7f		/* pull insn at 6f into delay slot */
	 mov	#(SH3_EXPEVT & 0xff), r0

	/* load entry for this uarea page into tlb */
5:	mov.l	@r1, r2		/* upte->data: u-area PTE */
	mov	#(SH3_PTEL & 0xff), r1
	mov.l	r2,	@r1

	mov	#(SH3_PTEH & 0xff), r1
	mov.l	@r1,	r2
	mov.l	.L3_VPN_MASK, r0
	and	r2,	r0
	mov.l	r0,	@r1	/* ASID 0 */

	ldtlb
	bra	99f		/* return */
	 mov.l	r2, @r1		/*  restore ASID */
#endif /* !P1_STACK */
	/* tlb_exception(curproc, trapframe, tea) */
6:	mov	#(SH3_EXPEVT & 0xff), r0
7:	mov.l	@r0,	r0
	mov.l	r0,	@(TF_EXPEVT, r14) /* trapframe->tf_expevt = EXPEVT */
	mov.l	.L3_curproc,	r0
	mov.l	@r0,	r4	/* 1st arg: curproc */
	__INTR_MASK(r0, r1)
	__EXCEPTION_UNBLOCK(r0, r1)
	mov.l	.L3_tlb_exception,	r0
	jsr	@r0
	 mov	r14,	r5	/* 2nd arg: trap frame */
99:	__EXCEPTION_RETURN

	.align	5
.L3_curproc:		.long	_C_LABEL(cpu_info_store) + CI_CURPROC
.L3_tlb_exception:	.long	_C_LABEL(tlb_exception)
.L3_VPN_MASK:		.long	0xfffff000
.L3_CURUPTE:		.long	_C_LABEL(curupte)

/* LINTSTUB: Var: char sh3_vector_tlbmiss_end[1]; */
VECTOR_END_MARKER(sh3_vector_tlbmiss_end)
	SET_ENTRY_SIZE(sh3_vector_tlbmiss)
#endif /* SH3 */


#ifdef SH4
/*
 * LINTSTUB: Var: char sh4_vector_tlbmiss[1];
 *
 * void sh4_vector_tlbmiss(void);
 *	Copied to VBR+0x400.  This code should be position independent
 *	and maximum 512 bytes long (== 0x600 - 0x400).
 */
NENTRY(sh4_vector_tlbmiss)
	__EXCEPTION_ENTRY
	mov.l	.L4_TEA4, r0
	mov.l	@r0,	r6
	mov.l	.L4_EXPEVT4, r0
	mov.l	@r0,	r0
	mov.l	r0,	@(TF_EXPEVT, r14) /* trapframe->tf_expevt = EXPEVT */
	mov.l	.L4_curproc,	r0
	mov.l	@r0,	r4	/* 1st arg */
	__INTR_MASK(r0, r1)
	__EXCEPTION_UNBLOCK(r0, r1)
	mov.l	.L4_tlb_exception,	r0
	jsr	@r0
	 mov	r14,	r5	/* 2nd arg */
	__EXCEPTION_RETURN

	.align	5
.L4_tlb_exception:	.long	_C_LABEL(tlb_exception)
.L4_curproc:		.long	_C_LABEL(cpu_info_store) + CI_CURPROC
.L4_EXPEVT4:		.long	SH4_EXPEVT
.L4_TEA4:			.long	SH4_TEA

/* LINTSTUB: Var: char sh4_vector_tlbmiss_end[1]; */
VECTOR_END_MARKER(sh4_vector_tlbmiss_end)
	SET_ENTRY_SIZE(sh4_vector_tlbmiss)
#endif /* SH4 */


/*
 * LINTSTUB: Var: char sh_vector_interrupt[1];
 *
 * void sh_vector_interrupt(void);
 *	Copied to VBR+0x600.  This code should be position independent.
 */
NENTRY(sh_vector_interrupt)
	__EXCEPTION_ENTRY
	xor	r0,	r0
	mov.l	r0,	@(TF_EXPEVT, r14) /* (for debug) */
	stc	r0_bank,r6	/* ssp */
	/* Enable exceptions for P3 access */
	__INTR_MASK(r0, r1)
	__EXCEPTION_UNBLOCK(r0, r1)
	/* uvmexp.intrs++ */
	mov.l	.Li_uvmexp_intrs, r0
	mov.l	@r0,	r1
	add	#1	r1
	mov.l	r1,	@r0
	/* Dispatch interrupt handler */
	mov.l	.Li_intc_intr, r0
	jsr	@r0		/* intc_intr(ssr, spc, ssp) */
	 nop
	/* Check for ASTs on exit to user mode. */
	mov.l	.Li_curproc,	r0
	mov.l	@r0,	r4	/* 1st arg */
	mov.l	.Li_ast, r0
	jsr	@r0
	 mov	r14,	r5	/* 2nd arg */
	__EXCEPTION_RETURN

	.align	5
.Li_curproc:		.long	_C_LABEL(cpu_info_store) + CI_CURPROC
.Li_intc_intr:		.long	_C_LABEL(intc_intr)
.Li_ast:		.long	_C_LABEL(ast)
.Li_uvmexp_intrs:	.long	_C_LABEL(uvmexp) + UVMEXP_INTRS

/* LINTSTUB: Var: char sh_vector_interrupt_end[1]; */
VECTOR_END_MARKER(sh_vector_interrupt_end)
	SET_ENTRY_SIZE(sh_vector_interrupt)
