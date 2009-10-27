/*	$OpenBSD: strnsubst.c,v 1.5 2009/10/27 23:59:50 deraadt Exp $	*/
/*	$FreeBSD: strnsubst.c,v 1.6 2002/06/22 12:58:42 jmallett Exp $	*/

/*
 * Copyright (c) 2002 J. Mallett.  All rights reserved.
 * You may do whatever you want with this file as long as
 * the above copyright and this notice remain intact, along
 * with the following statement:
 * 	For the man who taught me vi, and who got too old, too young.
 */

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void	strnsubst(char **, const char *, const char *, size_t);

/*
 * Replaces str with a string consisting of str with match replaced with
 * replstr as many times as can be done before the constructed string is
 * maxsize bytes large.  It does not free the string pointed to by str, it
 * is up to the calling program to be sure that the original contents of
 * str as well as the new contents are handled in an appropriate manner.
 * If replstr is NULL, then that internally is changed to a nil-string, so
 * that we can still pretend to do somewhat meaningful substitution.
 * No value is returned.
 */
void
strnsubst(char **str, const char *match, const char *replstr, size_t maxsize)
{
	char *s1, *s2, *this;
	size_t matchlen, repllen, s2len;
	int n;

	if ((s1 = *str) == NULL)
		return;
	if ((s2 = malloc(maxsize)) == NULL)
		err(1, NULL);

	if (replstr == NULL)
		replstr = "";

	if (match == NULL || *match == '\0' || strlen(s1) >= maxsize) {
		strlcpy(s2, s1, maxsize);
		goto done;
	}

	*s2 = '\0';
	s2len = 0;
	matchlen = strlen(match);
	repllen = strlen(replstr);
	for (;;) {
		if ((this = strstr(s1, match)) == NULL)
			break;
		n = snprintf(s2 + s2len, maxsize - s2len, "%.*s%s",
		    (int)(this - s1), s1, replstr);
		if (n == -1 || n + s2len + strlen(this + matchlen) >= maxsize)
			break;			/* out of room */
		s2len += n;
		s1 = this + matchlen;
	}
	strlcpy(s2 + s2len, s1, maxsize - s2len);
done:
	*str = s2;
	return;
}

#ifdef TEST
#include <stdio.h>

int
main(void)
{
	char *x, *y, *z, *za;

	x = "{}%$";
	strnsubst(&x, "%$", "{} enpury!", 255);
	y = x;
	strnsubst(&y, "}{}", "ybir", 255);
	z = y;
	strnsubst(&z, "{", "v ", 255);
	za = z;
	strnsubst(&z, NULL, za, 255);
	if (strcmp(z, "v ybir enpury!") == 0)
		printf("strnsubst() seems to work!\n");
	else
		printf("strnsubst() is broken.\n");
	printf("%s\n", z);
	free(x);
	free(y);
	free(z);
	free(za);
	return 0;
}
#endif
