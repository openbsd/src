/*	$NetBSD: umprintf.c,v 1.3 1994/10/26 08:25:20 cgd Exp $	*/

/*-
 * Copyright (c) 1986, 1988, 1991 The Regents of the University of California.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *      hacked out from ..
 *	@(#)subr_prf.c	7.30 (Berkeley) 6/29/91
 */

#include <stdarg.h>
static char *ksprintn __P((u_long num, int base, int *len));

void
umprintf(char *fmt,...)
{
	va_list ap;
	char *p;
	int tmp;
	int base;
	unsigned long ul;
	char ch;

        va_start (ap,fmt);

	for (;;) {
		while ((ch = *fmt++) != '%') {
			if (ch == '\0')
				return;
			scncnputc(0, ch);
		}
		ch = *fmt++;
		switch (ch) {
		case 'd':
			ul = va_arg(ap, u_long);
			base = 10;
			goto number;
		case 'x':
			ul = va_arg(ap, u_long);
			base = 16;
number:			p = ksprintn(ul, base, &tmp);
			while (ch = *p--)
				scncnputc(0,ch);
			break;
		default:
			scncnputc(0,ch);
		}
	}
}

/*
 * Put a number (base <= 16) in a buffer in reverse order; return an
 * optional length and a pointer to the NULL terminated (preceded?)
 * buffer.
 */
static char *
ksprintn(ul, base, lenp)
	register u_long ul;
	register int base, *lenp;
{					/* A long in base 8, plus NULL. */
	static char buf[sizeof(long) * NBBY / 3 + 2];
	register char *p;
	int d;

	p = buf;
	*p='\0';
	do {
		d = ul % base;
		if (d < 10)
			*++p = '0' + d;
		else
			*++p = 'a' + d - 10;
	} while (ul /= base);
	if (lenp)
		*lenp = p - buf;
	return (p);
}
