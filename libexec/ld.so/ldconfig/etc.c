/* $OpenBSD: etc.c,v 1.7 2006/05/12 23:35:16 deraadt Exp $ */

/* Public Domain */

#include <sys/types.h>

#include <err.h>
#include <stdlib.h>
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

void *
xcalloc(size_t nmemb, size_t size)
{
	void *ptr;

	ptr = calloc(nmemb, size);
	if (ptr == NULL)
		err(1, OOM_MSG);
	return ptr;
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
