/*	$OpenBSD: strlcpy.S,v 1.2 2000/01/12 00:08:41 mickey Exp $	*/

/*
 * Copyright (c) 1999 Michael Shalayeff
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
 *      This product includes software developed by Michael Shalayeff.
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

#include "DEFS.h"

#if defined(LIBC_SCCS)
	.text
	.asciz "$OpenBSD: strlcpy.S,v 1.2 2000/01/12 00:08:41 mickey Exp $"
	.align	4
#endif

/* size_t strlcpy(char *dst, const char *src, size_t siz)
 *
 * it will never be a string of 2^32, so we assume 2^31 as a max size
 */
ENTRY(strlcpy)

	addi	arg1, 1, ret0
	subi	arg2, 1, t2

$strlcpy_loop
	ldbs,ma	1(arg1), t1
	comb,=	r0, t1, $strlcpy_exit

	addib,>,n -1, t2, $strlcpy_loop
	stbs,ma	t1, 1(arg0)

$strlcpy_exit

	comb,=,n r0, arg2, $strlcpy_nzero
	stbs	r0, -1(arg0)
$strlcpy_nzero

	bv	0(rp)
	sub	arg1, ret0, ret0

EXIT(strlcpy)
