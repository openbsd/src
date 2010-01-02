/*	$OpenBSD: dl_printf.c,v 1.16 2010/01/02 00:59:01 deraadt Exp $	*/

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
#include <stdarg.h>
#include "syscall.h"
#include "util.h"

int lastfd = -1;
#define OUTBUFSIZE 128
static char outbuf[OUTBUFSIZE];
static char *outptr = outbuf;

static void kprintn(int, u_long, int);
static void kdoprnt(int, const char *, va_list);
static void _dl_flushbuf(void);

static void putcharfd(int, int );

static void
putcharfd(int c, int fd)
{
	char b = c;
	int len;

	if (fd != lastfd) {
		_dl_flushbuf();
		lastfd = fd;
	}
	*outptr++ = b;
	len = outptr - outbuf;
	if ((len >= OUTBUFSIZE) || (b == '\n') || (b == '\r')) {
		_dl_flushbuf();
	}
}

static void
_dl_flushbuf()
{
	int len = outptr - outbuf;
	if (len != 0) {
		_dl_write(lastfd, outbuf, len);
		outptr = outbuf;
	}
}

void
_dl_printf(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	kdoprnt(2, fmt, ap);
	va_end(ap);
}

void
_dl_fdprintf(int fd, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	kdoprnt(fd, fmt, ap);
	va_end(ap);
}

void
_dl_vprintf(const char *fmt, va_list ap)
{
	kdoprnt(2, fmt, ap);
}

static void
kdoprnt(int fd, const char *fmt, va_list ap)
{
	unsigned long ul;
	int lflag, ch;
	char *p;

	for (;;) {
		while ((ch = *fmt++) != '%') {
			if (ch == '\0')
				return;
			putcharfd(ch, fd);
		}
		lflag = 0;
reswitch:
		switch (ch = *fmt++) {
		case 'l':
			lflag = 1;
			goto reswitch;
		case 'b':
		{
			int set, n;

			ul = va_arg(ap, int);
			p = va_arg(ap, char *);
			kprintn(fd, ul, *p++);

			if (!ul)
				break;

			for (set = 0; (n = *p++);) {
				if (ul & (1 << (n - 1))) {
					putcharfd(set ? ',' : '<', fd);
					for (; (n = *p) > ' '; ++p)
						putcharfd(n, fd);
					set = 1;
				} else
					for (; *p > ' '; ++p);
			}
			if (set)
				putcharfd('>', fd);
		}
			break;
		case 'c':
			ch = va_arg(ap, int);
			putcharfd(ch & 0x7f, fd);
			break;
		case 's':
			p = va_arg(ap, char *);
			while ((ch = *p++))
				putcharfd(ch, fd);
			break;
		case 'd':
			ul = lflag ? va_arg(ap, long) : va_arg(ap, int);
			if ((long)ul < 0) {
				putcharfd('-', fd);
				ul = -(long)ul;
			}
			kprintn(fd, ul, 10);
			break;
		case 'o':
			ul = lflag ? va_arg(ap, u_long) : va_arg(ap, u_int);
			kprintn(fd, ul, 8);
			break;
		case 'u':
			ul = lflag ? va_arg(ap, u_long) : va_arg(ap, u_int);
			kprintn(fd, ul, 10);
			break;
		case 'p':
			putcharfd('0', fd);
			putcharfd('x', fd);
			lflag += sizeof(void *)==sizeof(u_long)? 1 : 0;
		case 'x':
			ul = lflag ? va_arg(ap, u_long) : va_arg(ap, u_int);
			kprintn(fd, ul, 16);
			break;
		case 'X':
		{
			int l;

			ul = lflag ? va_arg(ap, u_long) : va_arg(ap, u_int);
			if (lflag)
				l = (sizeof(ulong) * 8) - 4;
			else
				l = (sizeof(u_int) * 8) - 4;
			while (l >= 0) {
				putcharfd("0123456789abcdef"[(ul >> l) & 0xf], fd);
				l -= 4;
			}
			break;
		}
		default:
			putcharfd('%', fd);
			if (lflag)
				putcharfd('l', fd);
			putcharfd(ch, fd);
		}
	}
	_dl_flushbuf();
}

static void
kprintn(int fd, unsigned long ul, int base)
{
	/* hold a long in base 8 */
	char *p, buf[(sizeof(long) * NBBY / 3) + 1];

	p = buf;
	do {
		*p++ = "0123456789abcdef"[ul % base];
	} while (ul /= base);
	do {
		putcharfd(*--p, fd);
	} while (p > buf);
}
