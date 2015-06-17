/* $OpenBSD: xmalloc.c,v 1.10 2015/06/17 20:50:10 nicm Exp $ */
/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * Versions of malloc and friends that check their results, and never return
 * failure (they call fatal if they encounter an error).
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 */

#include <err.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xmalloc.h"

void *
xmalloc(size_t size)
{
	void *ptr;

	if (size == 0)
		errx(1, "xmalloc: zero size");
	ptr = malloc(size);
	if (ptr == NULL)
		errx(1,
		    "xmalloc: out of memory (allocating %zu bytes)",
		    size);
	return ptr;
}

void *
xcalloc(size_t nmemb, size_t size)
{
	void *ptr;

	if (size == 0 || nmemb == 0)
		errx(1, "xcalloc: zero size");
	if (SIZE_MAX / nmemb < size)
		errx(1, "xcalloc: nmemb * size > SIZE_MAX");
	ptr = calloc(nmemb, size);
	if (ptr == NULL)
		errx(1, "xcalloc: out of memory (allocating %zu bytes)",
		    (size * nmemb));
	return ptr;
}

void *
xreallocarray(void *ptr, size_t nmemb, size_t size)
{
	void *new_ptr;

	new_ptr = reallocarray(ptr, nmemb, size);
	if (new_ptr == NULL)
		errx(1, "xreallocarray: out of memory (new_size %zu bytes)",
		    nmemb * size);
	return new_ptr;
}

char *
xstrdup(const char *str)
{
	char *cp;

	if ((cp = strdup(str)) == NULL)
		err(1, "xstrdup");
	return cp;
}

int
xasprintf(char **ret, const char *fmt, ...)
{
	va_list ap;
	int i;

	va_start(ap, fmt);
	i = vasprintf(ret, fmt, ap);
	va_end(ap);

	if (i < 0 || *ret == NULL)
		errx(1, "xasprintf: could not allocate memory");

	return (i);
}
