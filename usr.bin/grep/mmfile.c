/*	$OpenBSD: mmfile.c,v 1.9 2004/10/03 19:23:02 otto Exp $	*/

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

#include <sys/param.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <err.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#include "grep.h"

#define MAX_MAP_LEN 1048576

mmf_t *
mmopen(char *fn, char *mode)
{
	mmf_t *mmf;
	struct stat st;

	/* XXX ignore mode for now */
	mode = mode;

	mmf = grep_malloc(sizeof *mmf);
	if ((mmf->fd = open(fn, O_RDONLY)) == -1)
		goto ouch1;
	if (fstat(mmf->fd, &st) == -1)
		goto ouch2;
	if (st.st_size > SIZE_T_MAX) /* too big to mmap */
		goto ouch2;
	if ((st.st_mode & S_IFREG) == 0) /* only mmap regular files */
		goto ouch2;
	mmf->len = (size_t)st.st_size;
	mmf->base = mmap(NULL, mmf->len, PROT_READ, MAP_PRIVATE, mmf->fd, (off_t)0);
	if (mmf->base == MAP_FAILED)
		goto ouch2;
	mmf->ptr = mmf->base;
	mmf->end = mmf->base + mmf->len;
	madvise(mmf->base, mmf->len, MADV_SEQUENTIAL);
	return mmf;

ouch2:
	close(mmf->fd);
ouch1:
	free(mmf);
	return NULL;
}

void
mmclose(mmf_t *mmf)
{
	munmap(mmf->base, mmf->len);
	close(mmf->fd);
	free(mmf);
}

char *
mmfgetln(mmf_t *mmf, size_t *l)
{
	static char *p;

	if (mmf->ptr >= mmf->end)
		return NULL;
    for (p = mmf->ptr; mmf->ptr < mmf->end; ++mmf->ptr)
        if (*mmf->ptr == '\n')
            break;

	*l = mmf->ptr - p;
	++mmf->ptr;
	return p;
}

long
mmtell(mmf_t *mmf)
{
	return mmf->ptr - mmf->base;
}

void
mmrewind(mmf_t *mmf)
{
	mmf->ptr = mmf->base;
}
