/*	$NetBSD: oc_cksum.s,v 1.2 1994/10/26 08:25:13 cgd Exp $	*/

/* 
 * Copyright (c) 1993 Philip A. Nelson.
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
 *	This product includes software developed by Philip A. Nelson.
 * 4. The name of Philip A. Nelson may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PHILIP NELSON ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL PHILIP NELSON BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 * oc_cksum: ones complement 16 bit checksum for NS32532
 *
 * oc_cksum (buffer, count, strtval)
 *
 * Do a 16 bit one's complement sum of 'count' bytes from 'buffer'.
 * 'strtval' is the starting value of the sum (usually zero).
 */


#include <machine/asm.h>

 /* This could use some tuning for better speed. */

	.globl _oc_cksum
_oc_cksum:
	movd	S_ARG0,r2	/* buffer */
	movd	S_ARG1,r1	/* count */
	movd	S_ARG2,r0	/* strtval */

loop:	
	cmpqd	1, r1
	ble	oneleft
	addw	0(r2), r0
	addqd	2,r2
	addqd	-2,r1
	br	loop

oneleft:
	bls	done
	movqd	0, r1
	movb	0(r2), r1
	addw	r1, r0
done:
	ret 0	
