/*	$OpenBSD: runeglue.c,v 1.2 2011/04/03 21:07:34 stsp Exp $ */
/*	$NetBSD: runeglue.c,v 1.10 2003/03/10 21:18:49 tshiozak Exp $	*/

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
 *
 *	Id: runeglue.c,v 1.7 2000/12/22 22:52:29 itojun Exp
 */

/*
 * Glue code to hide "rune" facility from user programs.
 * This is important to keep backward/future compatibility.
 */

#include <sys/cdefs.h>

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <sys/types.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "rune.h"
#include "rune_local.h"
#include "ctype_private.h"

#if EOF != -1
#error "EOF != -1"
#endif
#if _CACHED_RUNES != 256
#error "_CACHED_RUNES != 256"
#endif

int
__make_ctype_tabs(_RuneLocale *rl)
{
	int i, limit;
	struct old_tabs *p;

	p = malloc(sizeof *p);
	if (!p)
		return -1;

	/* By default, fill the ctype tab completely. */
	limit = CTYPE_NUM_CHARS;

	/* In UTF-8-encoded locales, the single-byte ctype functions
	 * must only return non-zero values for ASCII characters.
	 * Any non-ASCII single-byte character is not a valid UTF-8 sequence.
	 */
	if (strcmp(rl->rl_encoding, "UTF8") == 0)
		limit = 128;

	rl->rl_tabs = p;
	p->ctype_tab[0] = 0;
	p->toupper_tab[0] = EOF;
	p->tolower_tab[0] = EOF;
	for (i = 0; i < limit; i++) {
		p->ctype_tab[i + 1] = 0;
		if (rl->rl_runetype[i] & _CTYPE_U)
			p->ctype_tab[i + 1] |= _U;
		if (rl->rl_runetype[i] & _CTYPE_L)
			p->ctype_tab[i + 1] |= _L;
		if (rl->rl_runetype[i] & _CTYPE_D)
			p->ctype_tab[i + 1] |= _N;
		if (rl->rl_runetype[i] & _CTYPE_S)
			p->ctype_tab[i + 1] |= _S;
		if (rl->rl_runetype[i] & _CTYPE_P)
			p->ctype_tab[i + 1] |= _P;
		if (rl->rl_runetype[i] & _CTYPE_C)
			p->ctype_tab[i + 1] |= _C;
		if (rl->rl_runetype[i] & _CTYPE_X)
			p->ctype_tab[i + 1] |= _X;
		/*
		 * _B has been used incorrectly (or with older declaration)
		 * in ctype.h isprint() macro.
		 * _B does not mean isblank, it means "isprint && !isgraph".
		 * the following is okay since isblank() was hardcoded in
		 * function (i.e. isblank() is inherently locale unfriendly).
		 */
		if ((rl->rl_runetype[i] & (_CTYPE_R | _CTYPE_G))
		    == _CTYPE_R)
			p->ctype_tab[i + 1] |= _B;

		p->toupper_tab[i + 1] = (short)rl->rl_mapupper[i];
		p->tolower_tab[i + 1] = (short)rl->rl_maplower[i];
	}
	for (i = limit; i < CTYPE_NUM_CHARS; i++)
		p->ctype_tab[i + 1] = 0;

	return 0;
}

void
__install_currentrunelocale_ctype()
{
	if (_CurrentRuneLocale->rl_tabs != NULL) {
		/* LINTED const cast */
		_ctype_ = (const unsigned char *)
		    &(_CurrentRuneLocale->rl_tabs->ctype_tab);
		/* LINTED const cast */
		_toupper_tab_ = (const short *)
		    &(_CurrentRuneLocale->rl_tabs->toupper_tab);
		/* LINTED const cast */
		_tolower_tab_ = (const short *)
		    &(_CurrentRuneLocale->rl_tabs->tolower_tab);
	} else {
		_ctype_ = _C_ctype_;
		_toupper_tab_ = _C_toupper_;
		_tolower_tab_ = _C_tolower_;
	}
}
