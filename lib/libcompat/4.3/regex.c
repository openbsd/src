/*	$OpenBSD: regex.c,v 1.3 2000/03/02 00:29:48 todd Exp $	*/

/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * James da Silva at the University of Maryland at College Park.
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

/*
 * Compatibility routines that implement the old re_comp/re_exec interface in
 * terms of the regcomp/regexec interface.  It's possible that some programs
 * rely on dark corners of re_comp/re_exec and won't work with this version,
 * but most programs should be fine.
 */

#if defined(LIBC_SCCS) && !defined(lint)
/*static char sccsid[] = "from: @(#)regex.c	5.1 (Berkeley) 3/29/92";*/
static char rcsid[] = "$OpenBSD: regex.c,v 1.3 2000/03/02 00:29:48 todd Exp $";
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>
#include <stddef.h>
#include <regexp.h>
#include <string.h>
#include <stdlib.h>

static regexp *re_regexp;
static int re_goterr;
static char *re_errstr;

static void re_error __P((const char *));

char *
re_comp(s)
	const char *s;
{
	if (s == NULL)
		return (NULL);
	if (re_regexp) {
		free(re_regexp);
		re_regexp = NULL;
	}
	if (re_errstr) {
		free(re_errstr);
		re_errstr = NULL;
	}
	v8_setregerror(re_error);

	re_goterr = 0;
	re_regexp = v8_regcomp(s);
	return (re_goterr ? re_errstr : NULL);
}

int
re_exec(s)
	const char *s;
{
	int rc;

	re_goterr = 0;
	rc = v8_regexec(re_regexp, s);
	return (re_goterr ? -1 : rc);
}

static void
re_error(s)
	const char *s;
{
	re_goterr = 1;
	if (re_errstr)
		free(re_errstr);
	re_errstr = strdup(s);
}
