#if defined(LIBC_SCCS) && !defined(lint) && !defined(NOID)
static char elsieid[] = "@(#)ialloc.c	8.29";
static char rcsid[] = "$OpenBSD: ialloc.c,v 1.5 1998/08/14 21:39:43 deraadt Exp $";
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
	if (pointer == NULL)
		return imalloc(size);
	return realloc((void *) pointer, (size_t) nonzero(size));
}

char *
icatalloc(old, new)
char * const		old;
const char * const	new;
{
	register char *	result;
	register int	oldsize, newsize;

	newsize = (new == NULL) ? 0 : strlen(new);
	if (old == NULL)
		oldsize = 0;
	else if (newsize == 0)
		return old;
	else	oldsize = strlen(old);
	if ((result = irealloc(old, oldsize + newsize + 1)) != NULL)
		if (new != NULL)
			(void) strcpy(result + oldsize, new);
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
