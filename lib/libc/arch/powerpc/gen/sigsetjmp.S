/*	$OpenBSD: sigsetjmp.S,v 1.5 2005/08/07 16:40:15 espie Exp $	*/

/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
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
 *	from: @(#)setjmp.s	5.1 (Berkeley) 4/23/90"
 */

#include "SYS.h"
#include <machine/asm.h>

ENTRY(sigsetjmp)
	mr	6,3
	or.	7,4,4
	beq	1f
	li	3,1			# SIG_BLOCK
	li	4,0
	li	0,SYS_sigprocmask
	sc				# assume no error	XXX
1:
	mflr	11
	mfcr	12
	mr	10,1
	mr	9,2
	mr	8,3
	stmw	7,0(6)
	li	3,0
	blr

ENTRY(siglongjmp)
	lmw	7,0(3)
	mr	6,4
	mtlr	11
	mtcr	12
	mr	2,9
	mr	1,10
	or.	7,7,7
	beq	1f
	mr	4,8
	li	3,3			# SIG_SETMASK
	li	0,SYS_sigprocmask
	sc				# assume no error	XXX
1:
	or.	3,6,6
	bnelr
	li	3,1
	blr
