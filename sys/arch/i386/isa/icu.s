/*	$OpenBSD: icu.s,v 1.14 2001/09/21 20:54:32 mickey Exp $	*/
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
	.globl	_C_LABEL(imen), _C_LABEL(cpl), _C_LABEL(ipending)
	.globl	_C_LABEL(astpending), _C_LABEL(netisr)
_C_LABEL(imen):
	.long	0xffff		# interrupt mask enable (all off)

	.text

#if defined(PROF) || defined(GPROF)
	.globl	_C_LABEL(splhigh), _C_LABEL(splx)

	ALIGN_TEXT
_C_LABEL(splhigh):
	movl	$-1,%eax
	xchgl	%eax,_C_LABEL(cpl)
	ret

	ALIGN_TEXT
_C_LABEL(splx):
	movl	4(%esp),%eax
	movl	%eax,_C_LABEL(cpl)
	testl	%eax,%eax
	jnz	_C_LBALE(Xspllower)
	ret
#endif /* PROF || GPROF */
	
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
	movl	_C_LABEL(cpl),%ebx	# save priority
	movl	$1f,%esi		# address to resume loop at
1:	movl	%ebx,%eax
	notl	%eax
	andl	_C_LABEL(ipending),%eax
	jz	2f
	bsfl	%eax,%eax
	btrl	%eax,_C_LABEL(ipending)
	jnc	1b
	jmp	*_C_LABEL(Xrecurse)(,%eax,4)
2:	popl	%edi
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
	movl	%ebx,_C_LABEL(cpl)
	movl	$1f,%esi		# address to resume loop at
1:	movl	%ebx,%eax
	notl	%eax
	andl	_C_LABEL(ipending),%eax
	jz	2f
	bsfl    %eax,%eax               # slow, but not worth optimizing
	btrl    %eax,_C_LABEL(ipending)
	jnc     1b			# some intr cleared the in-memory bit
	cli
	jmp	*_C_LABEL(Xresume)(,%eax,4)
2:	/* Check for ASTs on exit to user mode. */
	cli
	cmpb	$0,_C_LABEL(astpending)
	je	3f
	testb   $SEL_RPL,TF_CS(%esp)
#ifdef VM86
	jnz	4f
	testl	$PSL_VM,TF_EFLAGS(%esp)
#endif
	jz	3f
4:	movb	$0,_C_LABEL(astpending)
	sti
	/* Pushed T_ASTFLT into tf_trapno on entry. */
	call	_C_LABEL(trap)
	jmp	2b
3:	INTRFASTEXIT


/*
 * Soft interrupt handlers
 */

#include "pccom.h"

IDTVEC(softtty)
#if NPCCOM > 0
	leal	SIR_TTYMASK(%ebx),%eax
	movl	%eax,_C_LABEL(cpl)
	call	_C_LABEL(comsoft)
	movl	%ebx,_C_LABEL(cpl)
#endif
	jmp	%esi

#define DONETISR(s, c) \
	.globl  _C_LABEL(c)	;\
	testl	$(1 << s),%edi	;\
	jz	1f		;\
	call	_C_LABEL(c)	;\
1:

IDTVEC(softnet)
	leal	SIR_NETMASK(%ebx),%eax
	movl	%eax,_C_LABEL(cpl)
	xorl	%edi,%edi
	xchgl	_C_LABEL(netisr),%edi
#include <net/netisr_dispatch.h>
	movl	%ebx,_C_LABEL(cpl)
	jmp	%esi
#undef DONETISR

IDTVEC(softclock)
	leal	SIR_CLOCKMASK(%ebx),%eax
	movl	%eax,_C_LABEL(cpl)
	call	_C_LABEL(softclock)
	movl	%ebx,_C_LABEL(cpl)
	jmp	%esi

