/*	$OpenBSD: n_infnan.S,v 1.3 2008/05/21 20:37:10 miod Exp $	*/
/*      $NetBSD: n_infnan.S,v 1.1 1995/10/10 23:40:27 ragge Exp $ */
/*
 * Copyright (c) 1985, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)infnan.s	8.1 (Berkeley) 6/4/93
 */

#include <machine/asm.h>
#include <sys/errno.h>

/*
 * infnan(arg) int arg;
 * where arg :=    EDOM	if result is  NaN
 *	     :=  ERANGE	if result is +INF
 *	     := -ERANGE if result is -INF
 *
 * The Reserved Operand Fault is generated inside of this routine.
 */	
ENTRY(infnan,0)
	cmpl	4(ap),$ERANGE
	bneq	1f
	movl	$ERANGE,_C_LABEL(errno)
	brb	2f
1:	movl	$EDOM,_C_LABEL(errno)
2:	emodd	$0,$0,$0x8000,r0,r0	# generates the reserved operand fault
	ret
