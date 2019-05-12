/* $OpenBSD: xmalloc.c,v 1.1 2019/05/12 20:44:39 kmos Exp $ */

/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * Versions of malloc and friends that check their results, and never return
 * failure (they call fatalx if they encounter an error).
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 */

#include <err.h>
#include <errno.h>
#include <limits.h>
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
		err(1, "xmalloc: allocating %zu bytes", size);
	return ptr;
}

void *
xcalloc(size_t nmemb, size_t size)
{
	void *ptr;

	if (size == 0 || nmemb == 0)
		errx(1, "xcalloc: zero size");
	ptr = calloc(nmemb, size);
	if (ptr == NULL)
		err(1, "xcalloc: allocating %zu * %zu bytes", nmemb, size);
	return ptr;
}

void *
xrealloc(void *ptr, size_t size)
{
	return xreallocarray(ptr, 1, size);
}

void *
xreallocarray(void *ptr, size_t nmemb, size_t size)
{
	void *new_ptr;

	if (nmemb == 0 || size == 0)
		errx(1, "xreallocarray: zero size");
	new_ptr = reallocarray(ptr, nmemb, size);
	if (new_ptr == NULL)
		err(1, "xreallocarray: allocating %zu * %zu bytes",
		    nmemb, size);
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

char *
xstrndup(const char *str, size_t maxlen)
{
	char *cp;

	if ((cp = strndup(str, maxlen)) == NULL)
		err(1, "xstrndup");
	return cp;
}

int
xasprintf(char **ret, const char *fmt, ...)
{
	va_list ap;
	int i;

	va_start(ap, fmt);
	i = xvasprintf(ret, fmt, ap);
	va_end(ap);

	return i;
}

int
xvasprintf(char **ret, const char *fmt, va_list ap)
{
	int i;

	i = vasprintf(ret, fmt, ap);

	if (i < 0 || *ret == NULL)
		err(1, "xasprintf");

	return i;
}

int
xsnprintf(char *str, size_t len, const char *fmt, ...)
{
	va_list ap;
	int i;

	va_start(ap, fmt);
	i = xvsnprintf(str, len, fmt, ap);
	va_end(ap);

	return i;
}

int
xvsnprintf(char *str, size_t len, const char *fmt, va_list ap)
{
	int i;

	if (len > INT_MAX)
		errx(1, "xsnprintf: len > INT_MAX");

	i = vsnprintf(str, len, fmt, ap);

	if (i < 0 || i >= (int)len)
		errx(1, "xsnprintf: overflow");

	return i;
}
