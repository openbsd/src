/*	$OpenBSD: ecvt.c,v 1.1 2002/12/02 15:38:54 millert Exp $	*/

/*
 * Copyright (c) 2002 Todd C. Miller <Todd.Miller@courtesan.com>
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: ecvt.c,v 1.1 2002/12/02 15:38:54 millert Exp $";
#endif /* LIBC_SCCS and not lint */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern char *__dtoa(double, int, int, int *, int *, char **);
static char *__cvt(double, int, int *, int *, int, int);

static char *
__cvt(double value, int ndigit, int *decpt, int *sign, int fmode, int pad)
{
	static char *s;
	char *p, *rve;
	size_t siz;

	if (ndigit == 0) {
		*sign = value < 0.0;
		*decpt = 0;
		return ("");
	}

	if (s) {
		free(s);
		s = NULL;
	}

	if (ndigit < 0)
		siz = -ndigit + 1;
	else
		siz = ndigit + 1;


	/* __dtoa() doesn't allocate space for 0 so we do it by hand */
	if (value == 0.0) {
		*decpt = 1 - fmode;	/* 1 for 'e', 0 for 'f' */
		*sign = 0;
		if ((rve = s = (char *)malloc(siz)) == NULL)
			return(NULL);
		*rve++ = '0';
		*rve = '\0';
	} else {
		p = __dtoa(value, fmode + 2, ndigit, decpt, sign, &rve);
		if (*decpt == 9999) {
			/* Nan or Infinity */
			*decpt = 0;
			return(p);
		}
		/* make a local copy and adjust rve to be in terms of s */
		if (pad && fmode)
			siz += *decpt;
		if ((s = (char *)malloc(siz)) == NULL)
			return(NULL);
		(void) strlcpy(s, p, siz);
		rve = s + (rve - p);
	}

	/* Add trailing zeros (unless we got NaN or Inf) */
	if (pad && *decpt != 9999) {
		siz -= rve - s;
		while (--siz)
			*rve++ = '0';
		*rve = '\0';
	}

	return(s);
}

char *
ecvt(double value, int ndigit, int *decpt, int *sign)
{
	return(__cvt(value, ndigit, decpt, sign, 0, 1));
}

char *
fcvt(double value, int ndigit, int *decpt, int *sign)
{
	return(__cvt(value, ndigit, decpt, sign, 1, 1));
}
