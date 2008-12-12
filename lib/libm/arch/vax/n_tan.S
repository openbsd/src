/*	$OpenBSD: n_tan.S,v 1.7 2008/12/12 19:30:17 martynas Exp $	*/
/*	$NetBSD: n_tan.S,v 1.1 1995/10/10 23:40:31 ragge Exp $	*/
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
 *	@(#)tan.s	8.1 (Berkeley) 6/4/93
 */

#include <machine/asm.h>

/*  This is the implementation of Peter Tang's double precision  
 *  tangent for the VAX using Bob Corbett's argument reduction.
 *  
 *  Notes:
 *       under 1,024,000 random arguments testing on [0,2*pi] 
 *       tan() observed maximum error = 2.15 ulps
 *
 * double
 * tan(double arg)
 * method: true range reduction to [-pi/4,pi/4], P. Tang  &  B. Corbett
 * S. McDonald, April 4,  1985
 */

WEAK_ALIAS(tanl,tan)
ENTRY(tan, R2|R3|R4|R5|R6|R7|R8|R9|R10|R11)
	movq	4(ap),r0
	bicw3	$0x807f,r0,r2
	beql	1f		# if x is zero or reserved operand then return x
/*
 * Save the PSL's IV & FU bits on the stack.
 */
	movpsl	r2
	bicw3	$0xff9f,r2,-(sp)
/*
 *  Clear the IV & FU bits.
 */
	bicpsw	$0x0060
	jsb	__libm_argred
/*
 *  At this point,
 *	   r0  contains the quadrant number, 0, 1, 2, or 3;
 *	r2/r1  contains the reduced argument as a D-format number;
 *  	   r3  contains a F-format extension to the reduced argument;
 *
 *  Save  r3/r0  so that we can call cosine after calling sine.
 */
	movq	r2,-(sp)
	movq	r0,-(sp)
/*
 *  Call sine.  r4 = 0  implies sine.
 */
	movl	$0,r4
	jsb	__libm_sincos
/*
 *  Save  sin(x)  in  r11/r10 .
 */
	movd	r0,r10
/*
 *  Call cosine.  r4 = 1  implies cosine.
 */
	movq	(sp)+,r0
	movq	(sp)+,r2
	movl	$1,r4
	jsb	__libm_sincos
	divd3	r0,r10,r0
	bispsw	(sp)+
1:	ret
