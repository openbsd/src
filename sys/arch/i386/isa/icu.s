/*	$OpenBSD: icu.s,v 1.30 2010/05/09 12:03:16 kettenis Exp $	*/
/*	$NetBSD: icu.s,v 1.45 1996/01/07 03:59:34 mycroft Exp $	*/

/*-
 * Copyright (c) 1993, 1994, 1995 Charles M. Hannum.  All rights reserved.
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
 *	This product includes software developed by Charles M. Hannum.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

#include <net/netisr.h>

	.data
	.globl	_C_LABEL(imen),_C_LABEL(netisr)
_C_LABEL(imen):
	.long	0xffff		# interrupt mask enable (all off)
_C_LABEL(netisr):
	.long	0		# scheduling bits for network

	.text
/*
 * Process pending interrupts.
 *
 * Important registers:
 *   ebx - cpl
 *   esi - address to resume loop at
 *   edi - scratch for Xsoftnet
 */
IDTVEC(spllower)
	pushl	%ebx
	pushl	%esi
	pushl	%edi
	movl	CPL,%ebx		# save priority
	movl	$1f,%esi		# address to resume loop at
1:	movl	%ebx,%eax		# get cpl
	shrl	$4,%eax			# find its mask.
	movl	_C_LABEL(iunmask)(,%eax,4),%eax
	cli
	andl	CPUVAR(IPENDING),%eax	# any non-masked bits left?
	jz	2f
	sti
	bsfl	%eax,%eax
	btrl	%eax,CPUVAR(IPENDING)
	jnc	1b
	jmp	*_C_LABEL(Xrecurse)(,%eax,4)
2:	movl	%ebx,CPL
	sti
	popl	%edi
	popl	%esi
	popl	%ebx
	ret

/*
 * Handle return from interrupt after device handler finishes.
 *
 * Important registers:
 *   ebx - cpl to restore
 *   esi - address to resume loop at
 *   edi - scratch for Xsoftnet
 */
IDTVEC(doreti)
	popl	%ebx			# get previous priority
	movl	$1f,%esi		# address to resume loop at
1:	movl	%ebx,%eax
	shrl	$4,%eax
	movl	_C_LABEL(iunmask)(,%eax,4),%eax
	cli
	andl	CPUVAR(IPENDING),%eax
	jz	2f
	sti
	bsfl    %eax,%eax               # slow, but not worth optimizing
	btrl    %eax,CPUVAR(IPENDING)
	jnc     1b			# some intr cleared the in-memory bit
	cli
	jmp	*_C_LABEL(Xresume)(,%eax,4)
2:	/* Check for ASTs on exit to user mode. */
	CHECK_ASTPENDING(%ecx)
	movl	%ebx,CPL
	je	3f
	testb   $SEL_RPL,TF_CS(%esp)
#ifdef VM86
	jnz	4f
	testl	$PSL_VM,TF_EFLAGS(%esp)
#endif
	jz	3f
4:	CLEAR_ASTPENDING(%ecx)
	sti
	movl	$T_ASTFLT,TF_TRAPNO(%esp)	/* XXX undo later. */
	/* Pushed T_ASTFLT into tf_trapno on entry. */
	pushl	%esp
	call	_C_LABEL(trap)
	addl	$4,%esp
	cli
	jmp	2b
3:	INTRFASTEXIT


/*
 * Soft interrupt handlers
 */

IDTVEC(softtty)
	movl	$IPL_SOFTTTY,%eax
	movl	%eax,CPL
	sti
#ifdef MULTIPROCESSOR
	call	_C_LABEL(i386_softintlock)
#endif
	pushl	$I386_SOFTINTR_SOFTTTY
	call	_C_LABEL(softintr_dispatch)
	addl	$4,%esp
#ifdef MULTIPROCESSOR	
	call	_C_LABEL(i386_softintunlock)
#endif
	jmp	*%esi

#define DONETISR(s, c) \
	.globl  _C_LABEL(c)	;\
	testl	$(1 << s),%edi	;\
	jz	1f		;\
	call	_C_LABEL(c)	;\
1:

IDTVEC(softnet)
	movl	$IPL_SOFTNET,%eax
	movl	%eax,CPL
	sti
#ifdef MULTIPROCESSOR
	call	_C_LABEL(i386_softintlock)
#endif
	xorl	%edi,%edi
	xchgl	_C_LABEL(netisr),%edi

#include <net/netisr_dispatch.h>

	pushl	$I386_SOFTINTR_SOFTNET
	call	_C_LABEL(softintr_dispatch)
	addl	$4,%esp
#ifdef MULTIPROCESSOR	
	call	_C_LABEL(i386_softintunlock)
#endif
	jmp	*%esi
#undef DONETISR

IDTVEC(softclock)
	movl	$IPL_SOFTCLOCK,%eax
	movl	%eax,CPL
	sti
#ifdef MULTIPROCESSOR
	call	_C_LABEL(i386_softintlock)
#endif
	pushl	$I386_SOFTINTR_SOFTCLOCK
	call	_C_LABEL(softintr_dispatch)
	addl	$4,%esp
#ifdef MULTIPROCESSOR	
	call	_C_LABEL(i386_softintunlock)
#endif
	jmp	*%esi

