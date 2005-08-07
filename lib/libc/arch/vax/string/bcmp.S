/*	$OpenBSD: bcmp.S,v 1.4 2005/08/07 16:40:15 espie Exp $ */
/*
 * Copyright (c) 1983, 1993
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
 */

#include "DEFS.h"

/* Since "cmpc3" is not portable across VAXen, do it the hard way */
/*  still, this is four times faster than the generic C version on a uvax2 */

ENTRY(bcmp, 0)
	movl	12(ap),r0	# r0 = n
	jeql	9f
	movq	4(ap),r1	# r1 = s1, r2 = s2
	ashl	$-2,r0,r3	# convert len to # of long words
	jeql	2f
1:
	cmpl	(r1)+,(r2)+	# no "cmpq" alas, so four bytes at a time
	jneq	9f
	sobgtr	r3,1b
2:
	bicl3	$-4,r0,r3	# handle at most 3 extra bytes
	jeql	8f
3:
	cmpb	(r1)+,(r2)+
	jneq	9f
	sobgtr	r3,3b
8:
	clrl	r0		# we have a match!
9:
	ret
