/*	$OpenBSD: promcons.c,v 1.2 2000/03/03 00:54:51 todd Exp $ */

/*
 * Copyright (c) 1996 Nivas Madhur
 * Copyright (c) 1995 Theo de Raadt
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
 *	This product includes software developed by Theo de Raadt
 * 4. The name of the Author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <stdarg.h>
#include <sys/types.h>

int
getchar()
{
	char c;

	__asm volatile("or r9, r0, 0\n
			tb0 0, r0, 496\n
			st.b r2, %0" : "=m" (c));
	return (c);
}

peekchar()
{
	int have = 0;

	__asm volatile("or r9, r0, 1\n
			tb0 0, r0, 496\n
			bb1 2, r2, 1f\n
			or  r2,r0, 1\n
			st  r2, %0\n1:" : "=m" (have) :);
	return (have);
}

void
putchar(c)
	int c;
{
	if (c == '\n')
		putchar('\r');
	__asm volatile("or r9, r0, 0x20\n
			or r2, r0, %0\n
			tb0 0, r0, 496\n" : : "r" (c));
}

