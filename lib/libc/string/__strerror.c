/*
 * Copyright (c) 1988 Regents of the University of California.
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
static char *rcsid = "$OpenBSD: __strerror.c,v 1.11 2004/04/30 17:13:02 espie Exp $";
#endif /* LIBC_SCCS and not lint */

#ifdef NLS
#define catclose	_catclose
#define catgets		_catgets
#define catopen		_catopen
#include <nl_types.h>
#endif

#define sys_errlist	_sys_errlist
#define sys_nerr	_sys_nerr

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

static size_t
__digits10(unsigned int num)
{
	size_t i = 0;

	do {
		num /= 10;
		i++;
	} while (num != 0);

	return i;
}

static char *
__itoa(int num, char *buffer, size_t maxlen)
{
	char *p;
	size_t len;
	unsigned int a;
	int neg;

	if (num < 0) {
		a = -num;
		neg = 1;
	}
	else {
		a = num;
		neg = 0;
	}

	len = __digits10(a);
	if (neg)
	    len++;

	if (len >= maxlen)
		return NULL;

	buffer[len--] = '\0';
	do {
		buffer[len--] = (a % 10) + '0';
		a /= 10;
	} while (a != 0);
	if (neg)
	    *buffer = '-';

	return buffer;
}

/*
 * Since perror() is not allowed to change the contents of strerror()'s
 * static buffer, both functions supply their own buffers to the
 * internal function __strerror().
 */

char *
__strerror(int num, char *buf)
{
#define	UPREFIX	"Unknown error: "
	int len;
#ifdef NLS
	int save_errno;
	nl_catd catd;

	catd = catopen("libc", 0);
#endif

	if (num >= 0 && num < sys_nerr) {
#ifdef NLS
		strlcpy(buf, catgets(catd, 1, num,
		    (char *)sys_errlist[num]), NL_TEXTMAX);
#else
		return(sys_errlist[num]);
#endif
	} else {
#ifdef NLS
		len = strlcpy(buf, catgets(catd, 1, 0xffff, UPREFIX), NL_TEXTMAX);
#else
		len = strlcpy(buf, UPREFIX, NL_TEXTMAX);
#endif
		if (len < NL_TEXTMAX)
			__itoa(num, buf + len, NL_TEXTMAX - len);
		errno = EINVAL;
	}

#ifdef NLS
	save_errno = errno;
	catclose(catd);
	errno = save_errno;
#endif

	return buf;
}
