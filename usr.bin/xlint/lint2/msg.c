/*	$OpenBSD: msg.c,v 1.9 2011/09/21 18:08:07 jsg Exp $	*/
/*	$NetBSD: msg.c,v 1.2 1995/07/03 21:24:56 cgd Exp $	*/

/*
 * Copyright (c) 1994, 1995 Jochen Pohl
 * All Rights Reserved.
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
 *      This product includes software developed by Jochen Pohl for
 *	The NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <string.h>

#include <stdio.h>
#include <stdarg.h>

#include "lint2.h"


static	const	char *msgs[] = {
	"%s: %s used, but not defined",				      /* 0 */
	"%s: %s defined, but never used",			      /* 1 */
	"%s: %s declared, but never used or defined",		      /* 2 */
	"%s: %s multiply defined (%s)",				      /* 3 */
	"%s: %s used inconsistently (%s)",			      /* 4 */
	"%s: %s declared inconsistently (%s)",			      /* 5 */
	"%s: %s arg %d used inconsistently (%s)",		      /* 6 */
	"%s: %s called with varying # of args (%s)",		      /* 7 */
	"%s returns value which is always ignored",		      /* 8 */
	"%s returns value which is sometimes ignored",		      /* 9 */
	"%s: return value of %s is used, but none returned",	      /* 10 */
	"%s: %s arg %d declared inconsistently (%s)",		      /* 11 */
	"%s: %s declared with varying # of args (%s)",		      /* 12 */
	"%s: malformed format string to %s",			      /* 13 */
	"%s, arg %d to %s inconsistent with format",		      /* 14 */
	"%s: too few format args to %s",			      /* 15 */
	"%s: too many format args to %s",			      /* 16 */
	"%s: %s must be declared before use (%s)",		      /* 17 */
};

static	const	char *lbasename(const char *);

void
msg(int n, ...)
{
	va_list	ap;

	va_start(ap, n);

	(void)vprintf(msgs[n], ap);
	(void)printf("\n");

	va_end(ap);
}

/*
 * Return a pointer to the last component of a path.
 */
static const char *
lbasename(const char *path)
{
	const	char *cp, *cp1, *cp2;

	if (Fflag)
		return (path);

	cp = cp1 = cp2 = path;
	while (*cp != '\0') {
		if (*cp++ == '/') {
			cp2 = cp1;
			cp1 = cp;
		}
	}
	return (*cp1 == '\0' ? cp2 : cp1);
}

/*
 * Create a string which describes a position in a source file.
 */
const char *
mkpos(pos_t *posp)
{
	size_t	len;
	const	char *fn;
	static	char	*buf;
	static	size_t	blen = 0;
	int	qm, src, line;

	if (Hflag && posp->p_src != posp->p_isrc) {
		src = posp->p_isrc;
		line = posp->p_iline;
	} else {
		src = posp->p_src;
		line = posp->p_line;
	}
	qm = !Hflag && posp->p_src != posp->p_isrc;

	len = strlen(fn = lbasename(fnames[src]));
	len += 3 * sizeof (u_short) + 4;

	if (len > blen)
		buf = xrealloc(buf, blen = len);
	if (line != 0) {
		(void)snprintf(buf, blen, "%s%s:%hu",
			      fn, qm ? "?" : "", line);
	} else {
		(void)snprintf(buf, blen, "%s", fn);
	}

	return (buf);
}

