/*
 * Copyright (c) 1991 The Regents of the University of California.
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

#if defined(LIBC_SCCS) && !defined(lint)
static char *rcsid = "$OpenBSD: multibyte.c,v 1.4 2003/06/02 20:18:38 millert Exp $";
#endif /* LIBC_SCCS and not lint */

#include <stdlib.h>

/*
 * Stub multibyte character functions.
 * This cheezy implementation is fixed to the native single-byte
 * character set.
 */

int
mblen(s, n)
	const char *s;
	size_t n;
{
	if (s == NULL || *s == '\0')
		return 0;
	if (n == 0)
		return -1;
	return 1;
}

/*ARGSUSED*/
int
mbtowc(pwc, s, n)
	wchar_t *pwc;
	const char *s;
	size_t n;
{
	if (s == NULL)
		return 0;
	if (n == 0)
		return -1;
	if (pwc)
		*pwc = (wchar_t) *s;
	return (*s != '\0');
}

/*ARGSUSED*/
int
wctomb(char *s, wchar_t wchar)
{
	if (s == NULL)
		return 0;

	*s = (char) wchar;
	return 1;
}

/*ARGSUSED*/
size_t
mbstowcs(pwcs, s, n)
	wchar_t *pwcs;
	const char *s;
	size_t n;
{
	int count = 0;

	if (n != 0) {
		do {
			if ((*pwcs++ = (wchar_t) *s++) == 0)
				break;
			count++;
		} while (--n != 0);
	}
	
	return count;
}

/*ARGSUSED*/
size_t
wcstombs(s, pwcs, n)
	char *s;
	const wchar_t *pwcs;
	size_t n;
{
	int count = 0;

	if (n != 0) {
		do {
			if ((*s++ = (char) *pwcs++) == 0)
				break;
			count++;
		} while (--n != 0);
	}

	return count;
}

