/*
** This file is in the public domain, so clarified as of
** Feb 14, 2003 by Arthur David Olson (arthur_david_olson@nih.gov).
*/

#if defined(LIBC_SCCS) && !defined(lint) && !defined(NOID)
static char elsieid[] = "@(#)ialloc.c	8.29";
static char rcsid[] = "$OpenBSD: ialloc.c,v 1.8 2003/03/13 15:47:34 deraadt Exp $";
#endif /* LIBC_SCCS and not lint */

/*LINTLIBRARY*/

#include "private.h"

#define nonzero(n)	(((n) == 0) ? 1 : (n))

char *
imalloc(n)
const int	n;
{
	return malloc((size_t) nonzero(n));
}

char *
icalloc(nelem, elsize)
int	nelem;
int	elsize;
{
	if (nelem == 0 || elsize == 0)
		nelem = elsize = 1;
	return calloc((size_t) nelem, (size_t) elsize);
}

void *
irealloc(pointer, size)
void * const	pointer;
const int	size;
{
	void *p;

	if (pointer == NULL)
		return imalloc(size);
	p = realloc((void *) pointer, (size_t) nonzero(size));
	if (p == NULL && pointer)
		free(pointer);
	return p;
}

char *
icatalloc(old, new)
char * const		old;
const char * const	new;
{
	register char *	result;
	register int	oldsize, newsize;
	int size;

	newsize = (new == NULL) ? 0 : strlen(new);
	if (old == NULL)
		oldsize = 0;
	else if (newsize == 0)
		return old;
	else
		oldsize = strlen(old);
	size = oldsize + newsize + 1;
	if ((result = irealloc(old, size)) != NULL)
		if (new != NULL)
			(void) strlcpy(result + oldsize, new, newsize + 1);
	else
		free(old);
	return result;
}

char *
icpyalloc(string)
const char * const	string;
{
	return icatalloc((char *) NULL, string);
}

void
ifree(p)
char * const	p;
{
	if (p != NULL)
		(void) free(p);
}

void
icfree(p)
char * const	p;
{
	if (p != NULL)
		(void) free(p);
}
