/*	$OpenBSD: printf.c,v 1.13 1998/06/12 12:09:12 d Exp $	*/
/*	$NetBSD: printf.c,v 1.10 1996/11/30 04:19:21 gwr Exp $	*/

/*-
 * Copyright (c) 1993
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
 *	@(#)printf.c	8.1 (Berkeley) 6/11/93
 */

/*
 * Scaled down version of printf(3).
 *
 * One additional format:
 *
 * The format %b is supported to decode error registers.
 * Its usage is:
 *
 *	printf("reg=%b\n", regval, "<base><arg>*");
 *
 * where <base> is the output base expressed as a control character, e.g.
 * \10 gives octal; \20 gives hex.  Each arg is a sequence of characters,
 * the first of which gives the bit number to be inspected (origin 1), and
 * the next characters (up to a control character, i.e. a character <= 32),
 * give the name of the register.  Thus:
 *
 *	printf("reg=%b\n", 3, "\10\2BITTWO\1BITONE\n");
 *
 * would produce output:
 *
 *	reg=3<BITTWO,BITONE>
 */

#include <sys/cdefs.h>
#include <sys/types.h>
#ifdef __STDC__
#include <machine/stdarg.h>
#else
#include <machine/varargs.h>
#endif

#include "stand.h"

static void kprintn __P((void (*)(int), u_long, int));
static void kdoprnt __P((void (*)(int), const char *, va_list));

#ifndef	STRIPPED
static void sputchar __P((int));
static char *sbuf;

static void
sputchar(c)
	int c;
{
	*sbuf++ = c;
}

void
#ifdef __STDC__
sprintf(char *buf, const char *fmt, ...)
#else
sprintf(buf, fmt, va_alist)
	char *buf, *fmt;
#endif
{
	va_list ap;

	sbuf = buf;
#ifdef __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	kdoprnt(sputchar, fmt, ap);
	va_end(ap);
	*sbuf = '\0';
}
#endif	/* NO_SPRINTF */

void
#ifdef __STDC__
printf(const char *fmt, ...)
#else
printf(fmt, va_alist)
	char *fmt;
#endif
{
	va_list ap;

#ifdef __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	kdoprnt(putchar, fmt, ap);
	va_end(ap);
}

void
vprintf(const char *fmt, va_list ap)
{
	kdoprnt(putchar, fmt, ap);
}

static void
kdoprnt(put, fmt, ap)
	void (*put)__P((int));
	const char *fmt;
	va_list ap;
{
	register char *p;
	register int ch;
	unsigned long ul;
	int lflag;

	for (;;) {
		while ((ch = *fmt++) != '%') {
			if (ch == '\0')
				return;
			put(ch);
		}
		lflag = 0;
reswitch:	switch (ch = *fmt++) {
		case 'l':
			lflag = 1;
			goto reswitch;
#ifndef	STRIPPED
		case 'b':
		{
			register int set, n;
			ul = va_arg(ap, int);
			p = va_arg(ap, char *);
			kprintn(put, ul, *p++);

			if (!ul)
				break;

			for (set = 0; (n = *p++);) {
				if (ul & (1 << (n - 1))) {
					put(set ? ',' : '<');
					for (; (n = *p) > ' '; ++p)
						put(n);
					set = 1;
				} else
					for (; *p > ' '; ++p);
			}
			if (set)
				put('>');
		}
			break;
#endif
		case 'c':
			ch = va_arg(ap, int);
				put(ch & 0x7f);
			break;
		case 's':
			p = va_arg(ap, char *);
			while ((ch = *p++))
				put(ch);
			break;
		case 'd':
			ul = lflag ?
			    va_arg(ap, long) : va_arg(ap, int);
			if ((long)ul < 0) {
				put('-');
				ul = -(long)ul;
			}
			kprintn(put, ul, 10);
			break;
		case 'o':
			ul = lflag ?
			    va_arg(ap, u_long) : va_arg(ap, u_int);
			kprintn(put, ul, 8);
			break;
		case 'u':
			ul = lflag ?
			    va_arg(ap, u_long) : va_arg(ap, u_int);
			kprintn(put, ul, 10);
			break;
		case 'p':
			put('0');
			put('x');
			lflag += sizeof(void *)==sizeof(u_long)? 1 : 0;
		case 'x':
			ul = lflag ?
			    va_arg(ap, u_long) : va_arg(ap, u_int);
			kprintn(put, ul, 16);
			break;
		default:
			put('%');
			if (lflag)
				put('l');
			put(ch);
		}
	}
	va_end(ap);
}

static void
kprintn(put, ul, base)
	void (*put)__P((int));
	unsigned long ul;
	int base;
{
					/* hold a long in base 8 */
	char *p, buf[(sizeof(long) * NBBY / 3) + 1];

	p = buf;
	do {
		*p++ = "0123456789abcdef"[ul % base];
	} while (ul /= base);
	do {
		put(*--p);
	} while (p > buf);
}

void
twiddle()
{
	static int pos;

	putchar("|/-\\"[pos++ & 3]);
	putchar('\b');
}
