/*	$OpennBSD$	*/

/*
 * Copyright (c) 1994 Mats O Jansson <moj@stacken.kth.se>
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Mats O Jansson
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static char rcsid[] = "$OpenBSD: read_line.c,v 1.1 2001/05/02 22:56:52 millert Exp $";
#endif /* not lint */

#include <sys/param.h>
#include <err.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "supcdefs.h"
#include "supextern.h"

/* read_line():
 *	Read a line from a file, parsing continuations ending in \
 *	and eliminating trailing newlines.
 *	Returns a pointer to an internal buffer that is reused upon
 *	next invocation.
 */
char *
read_line(fp, size, lineno, delim, flags)
	FILE		*fp;
	size_t		*size;
	size_t		*lineno;
	const char	delim[3];	/* unused */
	int		flags;		/* unused */
{
	static char	*buf;
#ifdef HAS_FPARSELN

	if (buf != NULL)
		free(buf);
	return (buf = fparseln(fp, size, lineno, delim, flags));
#else
	static int	 buflen;

	size_t	 s, len;
	char	*ptr;
	int	 cnt;

	len = 0;
	cnt = 1;
	while (cnt) {
		if (lineno != NULL)
			(*lineno)++;
		if ((ptr = fgetln(fp, &s)) == NULL) {
			if (size != NULL)
				*size = len;
			if (len == 0)
				return NULL;
			else
				return buf;
		}
		if (ptr[s - 1] == '\n')	/* the newline may be missing at EOF */
			s--;		/* forget newline */
		if (!s)
			cnt = 0;
		else {
			if ((cnt = (ptr[s - 1] == '\\')) != 0)
				s--;		/* forget \\ */
		}

		if (len + s + 1 > buflen) {
			buflen = len + s + 1;
			buf = realloc(buf, buflen);
		}
		if (buf == NULL)
			err(1, "can't realloc");
		memcpy(buf + len, ptr, s);
		len += s;
		buf[len] = '\0';
	}
	if (size != NULL)
		*size = len;
	return buf;
#endif /* HAS_FPARSELN */
}
