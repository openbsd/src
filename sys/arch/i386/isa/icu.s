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
	.globl	_imen,_cpl,_ipending,_astpending,_netisr
_imen:
	.long	0xffff		# interrupt mask enable (all off)

	.text

#if defined(PROF) || defined(GPROF)
	.globl	_splhigh, _splx

	ALIGN_TEXT
_splhigh:
	movl	$-1,%eax
	xchgl	%eax,_cpl
	ret

	ALIGN_TEXT
_splx:
	movl	4(%esp),%eax
	movl	%eax,_cpl
	testl	%eax,%eax
	jnz	_Xspllower
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
	movl	_cpl,%ebx		# save priority
	movl	$1f,%esi		# address to resume loop at
1:	movl	%ebx,%eax
	notl	%eax
	andl	_ipending,%eax
	jz	2f
	bsfl	%eax,%eax
	btrl	%eax,_ipending
	jnc	1b
	jmp	*_Xrecurse(,%eax,4)
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
	movl	%ebx,_cpl
	movl	$1f,%esi		# address to resume loop at
1:	movl	%ebx,%eax
	notl	%eax
	andl	_ipending,%eax
	jz	2f
	bsfl    %eax,%eax               # slow, but not worth optimizing
	btrl    %eax,_ipending
	jnc     1b			# some intr cleared the in-memory bit
	cli
	jmp	*_Xresume(,%eax,4)
2:	/* Check for ASTs on exit to user mode. */
	cli
	cmpb	$0,_astpending
	je	3f
	testb   $SEL_RPL,TF_CS(%esp)
#ifdef VM86
	jnz	4f
	testl	$PSL_VM,TF_EFLAGS(%esp)
#endif
	jz	3f
4:	movb	$0,_astpending
	sti
	/* Pushed T_ASTFLT into tf_trapno on entry. */
	call	_trap
3:	INTRFASTEXIT


/*
 * Soft interrupt handlers
 */

IDTVEC(softtty)
	/* XXXX nothing for now */
	jmp	%esi

#define DONET(s, c) \
	.globl  c		;\
	testl	$(1 << s),%edi	;\
	jz	1f		;\
	call	c		;\
1:

IDTVEC(softnet)
	leal	SIR_NETMASK(%ebx),%eax
	movl	%eax,_cpl
	xorl	%edi,%edi
	xchgl	_netisr,%edi
#ifdef INET
#include "ether.h"
#if NETHER > 0
	DONET(NETISR_ARP, _arpintr)
#endif
	DONET(NETISR_IP, _ipintr)
#endif
#ifdef IMP
	DONET(NETISR_IMP, _impintr)
#endif
#ifdef NS
	DONET(NETISR_NS, _nsintr)
#endif
#ifdef ISO
	DONET(NETISR_ISO, _clnlintr)
#endif
#ifdef CCITT
	DONET(NETISR_CCITT, _ccittintr)
#endif
#include "ppp.h"
#if NPPP > 0
	DONET(NETISR_PPP, _pppintr)
#endif
	movl	%ebx,_cpl
	jmp	%esi

IDTVEC(softclock)
	leal	SIR_CLOCKMASK(%ebx),%eax
	movl	%eax,_cpl
	call	_softclock
	movl	%ebx,_cpl
	jmp	%esi
