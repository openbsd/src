/*	$OpenBSD: citrus_ctype.c,v 1.2 2010/08/01 02:49:07 chl Exp $ */
/*	$NetBSD: citrus_ctype.c,v 1.5 2008/06/14 16:01:07 tnozaki Exp $	*/

/*-
 * Copyright (c)1999, 2000, 2001, 2002 Citrus Project,
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
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "citrus_ctype.h"
#include "citrus_none.h"
#include "citrus_utf8.h"

struct _citrus_ctype_rec _citrus_ctype_none = {
	&_citrus_none_ctype_ops,	/* cc_ops */
};

struct _citrus_ctype_rec _citrus_ctype_utf8 = {
	&_citrus_utf8_ctype_ops,	/* cc_ops */
};

int
_citrus_ctype_open(struct _citrus_ctype_rec **rcc, char const *encname)
{
	if (!strcmp(encname, "NONE")) {
		*rcc = &_citrus_ctype_none;
		__mb_cur_max = 1;
		return (0);
	} else if (!strcmp(encname, "UTF8")) {
		*rcc = &_citrus_ctype_utf8;
		__mb_cur_max = _CITRUS_UTF8_MB_CUR_MAX;
		return (0);
	}

	return (-1);
}
