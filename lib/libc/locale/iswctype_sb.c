/*	$OpenBSD: iswctype_sb.c,v 1.1 2005/04/30 09:14:23 espie Exp $	*/
/*	$NetBSD: iswctype_sb.c,v 1.3 2003/08/07 16:43:04 agc Exp $	*/

/*
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
static char rcsid[] = "$OpenBSD: iswctype_sb.c,v 1.1 2005/04/30 09:14:23 espie Exp $";
#endif /* LIBC_SCCS and not lint */

#include <wchar.h>
#include <wctype.h>
#include <ctype.h>

int
iswalnum(wint_t c)
{
	return isalnum((int)c);
}

int
iswalpha(wint_t c)
{
	return isalpha((int)c);
}

int
iswblank(wint_t c)
{
	return isblank((int)c);
}

int
iswcntrl(wint_t c)
{
	return iscntrl((int)c);
}

int
iswdigit(wint_t c)
{
	return isdigit((int)c);
}

int
iswgraph(wint_t c)
{
	return isgraph((int)c);
}

int
iswlower(wint_t c)
{
	return islower((int)c);
}

int
iswprint(wint_t c)
{
	return isprint((int)c);
}

int
iswpunct(wint_t c)
{
	return ispunct((int)c);
}

int
iswspace(wint_t c)
{
	return isspace((int)c);
}

int
iswupper(wint_t c)
{
	return isupper((int)c);
}

int
iswxdigit(wint_t c)
{
	return isxdigit((int)c);
}

wint_t
towupper(wint_t c)
{
	return toupper((int)c);
}

wint_t
towlower(wint_t c)
{
	return tolower((int)c);
}

int
wcwidth(wint_t c)
{
	return 1;
}

