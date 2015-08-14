/*	$OpenBSD: setrunelocale.c,v 1.12 2015/08/14 14:29:45 stsp Exp $ */
/*	$NetBSD: setrunelocale.c,v 1.14 2003/08/07 16:43:07 agc Exp $	*/

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
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Paul Kranenburg.
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

/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Paul Borman at Krystal Technologies.
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

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>

#include "citrus_ctype.h"
#include "rune.h"
#include "rune_local.h"

struct localetable {
	char path[PATH_MAX];
	_RuneLocale *runelocale;
	struct localetable *next;
};
static struct localetable *localetable_head;

_RuneLocale *
_findrunelocale(const char *path)
{
	struct localetable *lt;

	/* ones which we have seen already */
	for (lt = localetable_head; lt; lt = lt->next)
		if (strcmp(path, lt->path) == 0)
			return lt->runelocale;

	return NULL;
}

int
_newrunelocale(const char *path)
{
	struct localetable *lt;
	FILE *fp;
	_RuneLocale *rl;

	if (strlen(path) + 1 > sizeof(lt->path))
		return EINVAL;

	rl = _findrunelocale(path);
	if (rl)
		return 0;

	if ((fp = fopen(path, "re")) == NULL)
		return ENOENT;

	if ((rl = _Read_RuneMagi(fp)) != NULL)
		goto found;

	fclose(fp);
	return EFTYPE;

found:
	fclose(fp);

	rl->rl_citrus_ctype = NULL;

	if (_citrus_ctype_open(&rl->rl_citrus_ctype, rl->rl_encoding)) {
		_NukeRune(rl);
		return EINVAL;
	}

	/* register it */
	lt = malloc(sizeof(struct localetable));
	if (lt == NULL) {
		_NukeRune(rl);
		return ENOMEM;
	}
	strlcpy(lt->path, path, sizeof(lt->path));
	lt->runelocale = rl;
	lt->next = localetable_head;
	localetable_head = lt;

	return 0;
}

int
_xpg4_setrunelocale(const char *locname)
{
	char path[PATH_MAX];
	_RuneLocale *rl;
	int error, len;
	const char *dot, *encoding;

	if (!strcmp(locname, "C") || !strcmp(locname, "POSIX")) {
		rl = &_DefaultRuneLocale;
		goto found;
	}

	/* Assumes "language[_territory][.codeset]" locale name. */
	dot = strstr(locname, ".UTF-8");
	if (dot == NULL) {
		/* No encoding specified. Fall back to ASCII. */
		rl = &_DefaultRuneLocale;
		goto found;
	}

	encoding = dot + 1;
	if (strcmp(encoding, "UTF-8") != 0)
		return ENOTSUP;

	len = snprintf(path, sizeof(path),
	    "%s/%s/LC_CTYPE", _PATH_LOCALE, encoding);
	if (len < 0 || len >= sizeof(path))
		return ENAMETOOLONG;

	error = _newrunelocale(path);
	if (error)
		return error;
	rl = _findrunelocale(path);
	if (!rl)
		return ENOENT;

found:
	_CurrentRuneLocale = rl;
	__mb_cur_max = rl->rl_citrus_ctype->cc_mb_cur_max;

	return 0;
}
