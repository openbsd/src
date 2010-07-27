/*	$OpenBSD: multibyte_citrus.c,v 1.1 2010/07/27 16:59:04 stsp Exp $ */
/*	$NetBSD: multibyte_amd1.c,v 1.7 2009/01/11 02:46:28 christos Exp $ */

/*-
 * Copyright (c)2002, 2008 Citrus Project,
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
#include <wchar.h>

#include "citrus_ctype.h"
#include "rune.h"
#include "multibyte.h"

int
mbsinit(const mbstate_t *ps)
{
	struct _citrus_ctype_rec *cc;
	_RuneLocale *rl;

	if (ps == NULL)
		return 1;

	rl = _ps_to_runelocale(ps);
	if (rl == NULL)
		rl = _CurrentRuneLocale;
	cc = rl->rl_citrus_ctype;
	return (*cc->cc_ops->co_mbsinit)(ps);
}

size_t
mbrtowc(wchar_t *pwc, const char *s, size_t n, mbstate_t *ps)
{
	static mbstate_t mbs;
	struct _citrus_ctype_rec *cc;

	if (ps == NULL)
		ps = &mbs;
	cc = _CurrentRuneLocale->rl_citrus_ctype;
	return (*cc->cc_ops->co_mbrtowc)(pwc, s, n, _ps_to_private(ps));
}

size_t
mbsrtowcs(wchar_t *pwcs, const char **s, size_t n, mbstate_t *ps)
{
	static mbstate_t mbs;
	struct _citrus_ctype_rec *cc;

	if (ps == NULL)
		ps = &mbs;
	cc = _CurrentRuneLocale->rl_citrus_ctype;
	return (*cc->cc_ops->co_mbsrtowcs)(pwcs, s, n, _ps_to_private(ps));
}

size_t
wcrtomb(char *s, wchar_t wc, mbstate_t *ps)
{
	static mbstate_t mbs;
	struct _citrus_ctype_rec *cc;

	if (ps == NULL)
		ps = &mbs;
	cc = _CurrentRuneLocale->rl_citrus_ctype;
	return (*cc->cc_ops->co_wcrtomb)(s, wc, _ps_to_private(ps));
}

size_t
wcsrtombs(char *s, const wchar_t **ppwcs, size_t n, mbstate_t *ps)
{
	static mbstate_t mbs;
	struct _citrus_ctype_rec *cc;

	if (ps == NULL)
		ps = &mbs;
	cc = _CurrentRuneLocale->rl_citrus_ctype;
	return (*cc->cc_ops->co_wcsrtombs)(s, ppwcs, n, _ps_to_private(ps));
}
