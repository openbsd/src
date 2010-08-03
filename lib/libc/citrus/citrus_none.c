/*	$OpenBSD: citrus_none.c,v 1.2 2010/08/03 11:23:37 stsp Exp $ */
/*	$NetBSD: citrus_none.c,v 1.18 2008/06/14 16:01:07 tnozaki Exp $	*/

/*-
 * Copyright (c)2002 Citrus Project,
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#include <sys/types.h>

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <wchar.h>

#include "citrus_ctype.h"
#include "citrus_none.h"

_CITRUS_CTYPE_DEF_OPS(none);

size_t
/*ARGSUSED*/
_citrus_none_ctype_mbrtowc(wchar_t * __restrict pwc,
			   const char * __restrict s, size_t n,
			   void * __restrict pspriv)
{
	/* pwc may be NULL */
	/* s may be NULL */
	/* pspriv appears to be unused */

	if (s == NULL)
		return 0;
	if (n == 0)
		return (size_t)-2;
	if (pwc)
		*pwc = (wchar_t)(unsigned char)*s;
	return (*s != '\0');
}

int
/*ARGSUSED*/
_citrus_none_ctype_mbsinit(const void * __restrict pspriv)
{
	return (1);  /* always initial state */
}

size_t
/*ARGSUSED*/
_citrus_none_ctype_mbsrtowcs(wchar_t * __restrict pwcs,
			     const char ** __restrict s, size_t n,
			     void * __restrict pspriv)
{
	int count = 0;

	/* pwcs may be NULL */
	/* s may be NULL */
	/* pspriv appears to be unused */

	if (!s || !*s)
		return 0;

	if (pwcs == NULL)
		return strlen(*s);

	while (n > 0) {
		if ((*pwcs++ = (wchar_t)(unsigned char)*(*s)++) == 0)
			break;
		count++;
		n--;
	}
	
	return count;
}

size_t
/*ARGSUSED*/
_citrus_none_ctype_wcrtomb(char * __restrict s,
			   wchar_t wc, void * __restrict pspriv)
{
	/* s may be NULL */
	/* ps appears to be unused */

	if (s == NULL)
		return 0;

	*s = (char) wc;
	return 1;
}

size_t
/*ARGSUSED*/
_citrus_none_ctype_wcsrtombs(char * __restrict s,
			     const wchar_t ** __restrict pwcs, size_t n,
			     void * __restrict pspriv)
{
	int count = 0;

	/* s may be NULL */
	/* pwcs may be NULL */
	/* pspriv appears to be unused */

	if (pwcs == NULL || *pwcs == NULL)
		return (0);

	if (s == NULL) {
		while (*(*pwcs)++ != 0)
			count++;
		return(count);
	}

	if (n != 0) {
		do {
			if ((*s++ = (char) *(*pwcs)++) == 0)
				break;
			count++;
		} while (--n != 0);
	}

	return count;
}
