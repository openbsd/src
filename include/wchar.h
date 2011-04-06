/*	$OpenBSD: wchar.h,v 1.12 2011/04/06 11:39:42 miod Exp $	*/
/*	$NetBSD: wchar.h,v 1.16 2003/03/07 07:11:35 tshiozak Exp $	*/

/*-
 * Copyright (c)1999 Citrus Project,
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

/*-
 * Copyright (c) 1999, 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Julian Coleman.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _WCHAR_H_
#define _WCHAR_H_

#include <sys/cdefs.h>
#include <sys/_types.h>

#ifndef	NULL
#ifdef	__GNUG__
#define	NULL	__null
#else
#define	NULL	((void *)0)
#endif
#endif

#include <stdio.h> /* for FILE* */

#if !defined(_WCHAR_T_DEFINED_) && !defined(__cplusplus)
#define _WCHAR_T_DEFINED_
typedef	__wchar_t	wchar_t;
#endif

#ifndef	_MBSTATE_T_DEFINED_
#define	_MBSTATE_T_DEFINED_
typedef	__mbstate_t	mbstate_t;
#endif

#ifndef	_WINT_T_DEFINED_
#define	_WINT_T_DEFINED_
typedef	__wint_t	wint_t;
#endif

#ifndef	_SIZE_T_DEFINED_
#define	_SIZE_T_DEFINED_
typedef	__size_t	size_t;
#endif

#ifndef WEOF
#define	WEOF 	((wint_t)-1)
#endif

__BEGIN_DECLS
wint_t	btowc(int);
size_t	mbrlen(const char * __restrict, size_t, mbstate_t * __restrict);
size_t	mbrtowc(wchar_t * __restrict, const char * __restrict, size_t,
	    mbstate_t * __restrict);
int	mbsinit(const mbstate_t *);
size_t	mbsrtowcs(wchar_t * __restrict, const char ** __restrict, size_t,
	    mbstate_t * __restrict);
size_t	wcrtomb(char * __restrict, wchar_t, mbstate_t * __restrict);
wchar_t	*wcscat(wchar_t * __restrict, const wchar_t * __restrict);
wchar_t	*wcschr(const wchar_t *, wchar_t);
int	wcscmp(const wchar_t *, const wchar_t *);
int	wcscoll(const wchar_t *, const wchar_t *);
wchar_t	*wcscpy(wchar_t * __restrict, const wchar_t * __restrict);
size_t	wcscspn(const wchar_t *, const wchar_t *);
size_t	wcslen(const wchar_t *);
wchar_t	*wcsncat(wchar_t * __restrict, const wchar_t * __restrict,
	    size_t);
int	wcsncmp(const wchar_t *, const wchar_t *, size_t);
wchar_t	*wcsncpy(wchar_t * __restrict , const wchar_t * __restrict,
	    size_t);
wchar_t	*wcspbrk(const wchar_t *, const wchar_t *);
wchar_t	*wcsrchr(const wchar_t *, wchar_t);
size_t	wcsrtombs(char * __restrict, const wchar_t ** __restrict, size_t,
	    mbstate_t * __restrict);
size_t	wcsspn(const wchar_t *, const wchar_t *);
wchar_t	*wcsstr(const wchar_t *, const wchar_t *);
wchar_t *wcstok(wchar_t * __restrict, const wchar_t * __restrict,
		     wchar_t ** __restrict);
size_t	wcsxfrm(wchar_t *, const wchar_t *, size_t);
wchar_t	*wcswcs(const wchar_t *, const wchar_t *);
wchar_t	*wmemchr(const wchar_t *, wchar_t, size_t);
int	wmemcmp(const wchar_t *, const wchar_t *, size_t);
wchar_t	*wmemcpy(wchar_t * __restrict, const wchar_t * __restrict,
	    size_t);
wchar_t	*wmemmove(wchar_t *, const wchar_t *, size_t);
wchar_t	*wmemset(wchar_t *, wchar_t, size_t);

size_t	wcslcat(wchar_t *, const wchar_t *, size_t);
size_t	wcslcpy(wchar_t *, const wchar_t *, size_t);
int	wcswidth(const wchar_t *, size_t);
int	wctob(wint_t);
int	wcwidth(wchar_t);

double wcstod(const wchar_t * __restrict, wchar_t ** __restrict);
long int wcstol(const wchar_t * __restrict, wchar_t ** __restrict, int base);
unsigned long int wcstoul(const wchar_t * __restrict, wchar_t ** __restrict,
		int base);

#if __ISO_C_VISIBLE >= 1999
float	wcstof(const wchar_t * __restrict, wchar_t ** __restrict);
long double wcstold(const wchar_t * __restrict, wchar_t ** __restrict);
#endif

#if (defined(__GNUC__) && __GNUC__ >= 2 && !defined(__STRICT_ANSI__)) || \
    __ISO_C_VISIBLE >= 1999
/* LONGLONG */
long long int wcstoll(const wchar_t * __restrict,
	wchar_t ** __restrict, int base);
/* LONGLONG */
unsigned long long int wcstoull(const wchar_t * __restrict,
	wchar_t ** __restrict, int base);
#endif

wint_t ungetwc(wint_t, FILE *);
wint_t fgetwc(FILE *);
wchar_t *fgetws(wchar_t * __restrict, int, FILE * __restrict);
wint_t getwc(FILE *);
wint_t getwchar(void);
wint_t fputwc(wchar_t, FILE *);
int fputws(const wchar_t * __restrict, FILE * __restrict);
wint_t putwc(wchar_t, FILE *);
wint_t putwchar(wchar_t);

int fwide(FILE *, int);

#define getwc(f) fgetwc(f)
#define getwchar() getwc(stdin)
#define putwc(wc, f) fputwc((wc), (f))
#define putwchar(wc) putwc((wc), stdout)
__END_DECLS

#endif /* !_WCHAR_H_ */
