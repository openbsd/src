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
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char *rcsid = "$OpenBSD: __strerror.c,v 1.8 2001/12/08 20:37:32 deraadt Exp $";
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

static char *
itoa(num)
	int num;
{
	static char buffer[11];
	char *p;

	p = buffer + 4;
	while (num >= 10) {
		*--p = (num % 10) + '0';
		num /= 10;
	}
	*p = (num % 10) + '0';
	return p;
}

/*
 * Since perror() is not allowed to change the contents of strerror()'s
 * static buffer, both functions supply their own buffers to the
 * internal function __strerror().
 */

char *
__strerror(num, buf)
	int num;
	char *buf;
{
#define	UPREFIX	"Unknown error: "
	register unsigned int errnum;
#ifdef NLS
	int save_errno;
	nl_catd catd;

	catd = catopen("libc", 0);
#endif

	errnum = num;				/* convert to unsigned */
	if (errnum < sys_nerr) {
#ifdef NLS
		strlcpy(buf, catgets(catd, 1, errnum,
		    (char *)sys_errlist[errnum]), NL_TEXTMAX);
#else
		return(sys_errlist[errnum]);
#endif
	} else {
#ifdef NLS
		strlcpy(buf, catgets(catd, 1, 0xffff, UPREFIX), NL_TEXTMAX);
#else
		strcpy(buf, UPREFIX);
#endif
		strncat(buf, itoa(errnum), NL_TEXTMAX-strlen(buf)-1);
		errno = EINVAL;
	}

#ifdef NLS
	save_errno = errno;
	catclose(catd);
	errno = save_errno;
#endif

	return buf;
}
