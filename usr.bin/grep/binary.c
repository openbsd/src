/*	$OpenBSD: binary.c,v 1.4 2003/06/22 22:38:50 deraadt Exp $	*/

/*-
 * Copyright (c) 1999 James Howard and Dag-Erling Coïdan Smørgrav
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

#include <ctype.h>
#include <stdio.h>
#include <zlib.h>

#include "grep.h"

#define BUFFER_SIZE 32

int
bin_file(FILE *f)
{
	char		buf[BUFFER_SIZE];
	int		i, m;

	if (fseek(f, 0L, SEEK_SET) == -1)
		return 0;

	if ((m = (int)fread(buf, 1, BUFFER_SIZE, f)) == 0)
		return 0;

	for (i = 0; i < m; i++)
		if (!isprint(buf[i]) && !isspace(buf[i]))
			return 1;

	rewind(f);
	return 0;
}

#ifndef NOZ
int
gzbin_file(gzFile *f)
{
	char		buf[BUFFER_SIZE];
	int		i, m;

	if (gzseek(f, SEEK_SET, 0) == -1)
		return 0;

	if ((m = (int)gzread(f, buf, BUFFER_SIZE)) == 0)
		return 0;

	for (i = 0; i < m; i++)
		if (!isprint(buf[i]))
			return 1;

	gzrewind(f);
	return 0;
}
#endif

int
mmbin_file(mmf_t *f)
{
	int i;

	/* XXX knows too much about mmf internals */
	for (i = 0; i < BUFFER_SIZE && i < f->len; i++)
		if (!isprint(f->base[i]))
			return 1;
	mmrewind(f);
	return 0;
}
