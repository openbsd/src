/* $OpenPackages$ */
/* $OpenBSD: memory.c,v 1.4 2007/09/16 10:43:53 espie Exp $ */

/*
 * Copyright (c) 1988, 1989, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1989 by Berkeley Softworks
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Adam de Boor.
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

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stddef.h>
#include <stdint.h>
#include <ohash.h>
#include "defines.h"
#include "memory.h"

static void enomem(size_t);

/*
 * emalloc --
 *	malloc, but die on error.
 */
void *
emalloc(size_t size)
{
	void *p;

	if ((p = malloc(size)) == NULL)
		enomem(size);
	return p;
}

/*
 * estrdup --
 *	strdup, but die on error.
 */
char *
estrdup(const char *str)
{
	char *p;
	size_t size;

	size = strlen(str) + 1;

	p = emalloc(size);
	memcpy(p, str, size);
	return p;
}

/*
 * erealloc --
 *	realloc, but die on error.
 */
void *
erealloc(void *ptr, size_t size)
{
	if ((ptr = realloc(ptr, size)) == NULL)
		enomem(size);
	return ptr;
}

void *
ecalloc(size_t s1, size_t s2)
{
	void *p;

	if ((p = calloc(s1, s2)) == NULL)
		enomem(s1 * s2);
	return p;
}

/* Support routines for hash tables.  */
void *
hash_alloc(size_t s, void *u UNUSED)
{
	return ecalloc(s, 1);
}

void
hash_free(void *p, size_t s UNUSED, void *u UNUSED)
{
	free(p);
}

void *
element_alloc(size_t s, void *u UNUSED)
{
	return emalloc(s);
}



/*
 * enomem --
 *	die when out of memory.
 */
void
enomem(size_t size)
{
	fprintf(stderr, "make: %s (%lu)\n", strerror(errno), (u_long)size);
	exit(2);
}

/*
 * esetenv --
 *	change environment, die on error.
 */
void
esetenv(const char *name, const char *value)
{
	if (setenv(name, value, 1) == 0)
	    return;

	fprintf(stderr, "make: setenv failed (%s)\n", strerror(errno));
	exit(2);
}


/*
 * enunlink --
 *	Remove a file carefully, avoiding directories.
 */
int
eunlink(const char *file)
{
	struct stat st;

	if (lstat(file, &st) == -1)
		return -1;

	if (S_ISDIR(st.st_mode)) {
		errno = EISDIR;
		return -1;
	}
	return unlink(file);
}

void
free_hash(struct ohash *h)
{
	void *e;
	unsigned int i;

	for (e = ohash_first(h, &i); e != NULL; e = ohash_next(h, &i))
		free(e);
	ohash_delete(h);
}

