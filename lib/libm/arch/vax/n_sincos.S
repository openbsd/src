/*	$OpenBSD: n_sincos.S,v 1.7 2008/12/12 19:30:17 martynas Exp $	*/
/*	$NetBSD: n_sincos.S,v 1.1 1995/10/10 23:40:28 ragge Exp $	*/
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
 *	@(#)sincos.s	8.1 (Berkeley) 6/4/93
 */

#include <machine/asm.h>

/*
 *  This is the implementation of Peter Tang's double precision  
 *  sine and cosine for the VAX using Bob Corbett's argument reduction.
 *  
 *  Notes:
 *       under 1,024,000 random arguments testing on [0,2*pi] 
 *       sin() observed maximum error = 0.814 ulps
 *       cos() observed maximum error = 0.792 ulps
 *
 * double
 * sin(double arg)
 * method: true range reduction to [-pi/4,pi/4], P. Tang  &  B. Corbett
 * S. McDonald, April 4,  1985
 */

WEAK_ALIAS(sinl,sin)
ENTRY(sin, R2|R3|R4|R5|R6|R7|R8|R9|R10|R11)
	movq	4(ap),r0
	bicw3	$0x807f,r0,r2
	beql	1f		# if x is zero or reserved operand then return x
/*
 * Save the PSL's IV & FU bits on the stack.
 */
	movpsl	r2
	bicw3	$0xff9f,r2,-(sp)
/*
 * Clear the IV & FU bits.
 */
	bicpsw	$0x0060
/*
 *  Entered by  sine    ; save  0  in  r4 .
 */
	jsb	__libm_argred
	movl	$0,r4
	jsb	__libm_sincos
	bispsw	(sp)+
1:	ret

/*
 * double
 * cos(double arg)
 * method: true range reduction to [-pi/4,pi/4], P. Tang  &  B. Corbett
 * S. McDonald, April 4,  1985
 */

WEAK_ALIAS(cosl,cos)
ENTRY(cos, R2|R3|R4|R5|R6|R7|R8|R9|R10|R11)
	movq	4(ap),r0
	bicw3	$0x7f,r0,r2
	cmpw	$0x8000,r2
	beql	1f		# if x is reserved operand then return x
/*
 * Save the PSL's IV & FU bits on the stack.
 */
	movpsl	r2
	bicw3	$0xff9f,r2,-(sp)
/*
 * Clear the IV & FU bits.
 */
	bicpsw	$0x0060
/*
 *  Entered by  cosine  ; save  1  in  r4 .
 */
	jsb	__libm_argred
	movl	$1,r4
	jsb	__libm_sincos
	bispsw	(sp)+
1:	ret
