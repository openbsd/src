/* $OpenBSD: etc.c,v 1.5 2002/09/07 01:25:34 marc Exp $ */

/* Public Domain */

#include <sys/types.h>

#include <a.out.h>
#include <err.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "ld.h"

#define	OOM_MSG	"Out of memory"

char *
xstrdup(const char *s)
{
	char *ptr;

	if ((ptr = strdup(s)) == NULL)
		err(1, OOM_MSG);
	return (ptr);
}

void *
xmalloc(size_t size)
{
	void *ptr;

	if ((ptr = malloc(size)) == NULL)
		err(1, OOM_MSG);
	return (ptr);
}

void *
xrealloc(void *ptr, size_t size)
{
	void *nptr;

	if ((nptr = realloc(ptr, size)) == NULL)
		err(1, OOM_MSG);
	return (nptr);
}

char *
concat(const char *s1, const char *s2, const char *s3)
{
	char *str;
	size_t len;

	len = strlen(s1) + strlen(s2) + strlen(s3) + 1;
	str = xmalloc(len);

	strlcpy(str, s1, len);
	strlcat(str, s2, len);
	strlcat(str, s3, len);

	return (str);
}
