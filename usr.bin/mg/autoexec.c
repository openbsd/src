/* $OpenBSD: autoexec.c,v 1.2 2002/08/22 23:28:19 deraadt Exp $ */
/* this file is in the public domain */
/* Author: Vincent Labrecque <vincent@openbsd.org>	April 2002 */

#include "def.h"
#include "funmap.h"

#include <sys/queue.h>

#include <fnmatch.h>

struct autoexec {
	SLIST_ENTRY(autoexec) next;	/* link in the linked list */
	const char	*pattern;	/* Pattern to match to filenames */
	PF		 fp;
};

static SLIST_HEAD(, autoexec)	 autos;
static int			 ready;

/*
 * Return a NULL terminated array of function pointers to be called
 * when we open a file that matches <fname>.  The list must be free(ed)
 * after use.
 */
PF *
find_autoexec(const char *fname)
{
	PF *pfl;
	int have, used;
	struct autoexec *ae;

	if (!ready)
		return (NULL);

	pfl = NULL;
	have = 0;
	used = 0;
	SLIST_FOREACH(ae, &autos, next) {
		if (fnmatch(ae->pattern, fname, 0) == 0) {
			if (used >= have) {
				have += 8;
				/*
				 * XXX - realloc(NULL, ...) is not really
				 *       portable
				 */
				pfl = realloc(pfl, (have + 1) * sizeof(PF));
				if (pfl == NULL)
					panic("out of memory");
			}
			pfl[used++] = ae->fp;
		}
	}
	if (used) {
		pfl[used] = NULL;
		pfl = realloc(pfl, (used + 1) * sizeof(PF));
	}
	return (pfl);
}

int
add_autoexec(const char *pattern, const char *func)
{
	PF fp;
	struct autoexec *ae;

	if (!ready) {
		SLIST_INIT(&autos);
		ready = 1;
	}
	fp = name_function(func);
	if (fp == NULL)
		return (FALSE);
	ae = malloc(sizeof *ae);
	if (ae == NULL)
		return (FALSE);
	ae->fp = fp;
	ae->pattern = strdup(pattern);
	if (ae->pattern == NULL) {
		free(ae);
		return (FALSE);
	}
	SLIST_INSERT_HEAD(&autos, ae, next);

	return (TRUE);
}

int
auto_execute(int f, int n)
{
	char patbuf[128], funcbuf[128];
	int s;

	if ((s = ereply("Filename pattern: ", patbuf, sizeof patbuf)) != TRUE)
		return (s);
	if ((s = ereply("Execute: ", funcbuf, sizeof funcbuf)) != TRUE)
		return (s);
	if ((s = add_autoexec(patbuf, funcbuf)) != TRUE)
		return (s);
	return (TRUE);
}
