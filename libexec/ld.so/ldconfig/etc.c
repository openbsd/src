/* * $OpenBSD: etc.c,v 1.3 2001/12/07 18:45:32 mpech Exp $*/
/*
 */

#include <err.h>
#include <stdlib.h>
#include <string.h>

/*
 * Like strdup but get fatal error if memory is exhausted.
 */
char *
xstrdup(s)
	char*s;
{
	char *result = strdup(s);

	if (!result)
		errx(1, "virtual memory exhausted");

	return result;
}

/*
 * Like malloc but get fatal error if memory is exhausted.
 */
void *
xmalloc(size)
	size_t size;
{
	void	*result = (void *)malloc(size);

	if (!result)
		errx(1, "virtual memory exhausted");

	return result;
}

/*
 * Like realloc but get fatal error if memory is exhausted.
 */
void *
xrealloc(ptr, size)
	void *ptr;
	size_t size;
{
	void	*result;

	if (ptr == NULL)
		result = (void *)malloc(size);
	else
		result = (void *)realloc(ptr, size);

	if (!result)
		errx(1, "virtual memory exhausted");

	return result;
}

/*
 * Return a newly-allocated string whose contents concatenate
 * the strings S1, S2, S3.
 */
char *
concat(s1, s2, s3)
	const char *s1, *s2, *s3;
{
	int	len1 = strlen(s1),
			len2 = strlen(s2),
			len3 = strlen(s3);

	char *result = (char *)xmalloc(len1 + len2 + len3 + 1);

	strcpy(result, s1);
	strcpy(result + len1, s2);
	strcpy(result + len1 + len2, s3);
	result[len1 + len2 + len3] = 0;

	return result;
}

