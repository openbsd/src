/*	$NetBSD: str.s,v 1.2 1996/08/02 16:18:40 ragge Exp $ */
/*
 * Copyright (c) 1996 Ludd, University of Lule}, Sweden.
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
 *      This product includes software developed at Ludd, University of 
 *      Lule}, Sweden and its contributors.
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
 */

/*
 * Small versions of the most common string functions not using any
 * emulated instructions.
 */

#include "../include/asm.h"

ENTRY(strlen, 0);
	movl	4(ap), r0
1:	tstb	(r0)+
	bneq	1b
	decl	r0
	subl2	4(ap), r0
	ret

ENTRY(strncmp, 0)
	movl	12(ap), r3
	brb	5f

ENTRY(strcmp, 0)
	movl	$250, r3	# max string len to compare
5:	movl	4(ap), r2
	movl	8(ap), r1
	movl	$1, r0

2:	cmpb	(r2),(r1)+
	bneq	1f		# something differ
	tstb	(r2)+
	beql	4f		# continue, strings unequal
	decl	r3		# max string len encountered?
	bneq	2b

4:	clrl	r0		# We are done, strings equal.
	ret

1:	bgtr	3f
	mnegl	r0, r0
3:	ret
