/*	$OpenBSD: util.c,v 1.13 2016/01/30 21:31:08 martijn Exp $	*/

/*-
 * Copyright (c) 1991, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1991, 1993, 1994, 1995, 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#include <sys/queue.h>

#include <bitstring.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "common.h"

#define MAXIMUM(a, b)	(((a) > (b)) ? (a) : (b))

/*
 * binc --
 *	Increase the size of a buffer.
 *
 * PUBLIC: void *binc(SCR *, void *, size_t *, size_t);
 */
void *
binc(SCR *sp, void *bp, size_t *bsizep, size_t min)
{
	size_t csize;

	/* If already larger than the minimum, just return. */
	if (min && *bsizep >= min)
		return (bp);

	csize = *bsizep + MAXIMUM(min, 256);
	REALLOC(sp, bp, csize);

	if (bp == NULL) {
		/*
		 * Theoretically, realloc is supposed to leave any already
		 * held memory alone if it can't get more.  Don't trust it.
		 */
		*bsizep = 0;
		return (NULL);
	}
	/*
	 * Memory is guaranteed to be zero-filled, various parts of
	 * nvi depend on this.
	 */
	memset((char *)bp + *bsizep, 0, csize - *bsizep);
	*bsizep = csize;
	return (bp);
}

/*
 * nonblank --
 *	Set the column number of the first non-blank character
 *	including or after the starting column.  On error, set
 *	the column to 0, it's safest.
 *
 * PUBLIC: int nonblank(SCR *, recno_t, size_t *);
 */
int
nonblank(SCR *sp, recno_t lno, size_t *cnop)
{
	char *p;
	size_t cnt, len, off;
	int isempty;

	/* Default. */
	off = *cnop;
	*cnop = 0;

	/* Get the line, succeeding in an empty file. */
	if (db_eget(sp, lno, &p, &len, &isempty))
		return (!isempty);

	/* Set the offset. */
	if (len == 0 || off >= len)
		return (0);

	for (cnt = off, p = &p[off],
	    len -= off; len && isblank(*p); ++cnt, ++p, --len);

	/* Set the return. */
	*cnop = len ? cnt : cnt - 1;
	return (0);
}

/*
 * v_strdup --
 *	Strdup for wide character strings with an associated length.
 *
 * PUBLIC: CHAR_T *v_strdup(SCR *, const CHAR_T *, size_t);
 */
CHAR_T *
v_strdup(SCR *sp, const CHAR_T *str, size_t len)
{
	CHAR_T *copy;

	MALLOC(sp, copy, len + 1);
	if (copy == NULL)
		return (NULL);
	memcpy(copy, str, len * sizeof(CHAR_T));
	copy[len] = '\0';
	return (copy);
}

/*
 * nget_uslong --
 *      Get an unsigned long, checking for overflow.
 *
 * PUBLIC: enum nresult nget_uslong(u_long *, const char *, char **, int);
 */
enum nresult
nget_uslong(u_long *valp, const char *p, char **endp, int base)
{
	errno = 0;
	*valp = strtoul(p, endp, base);
	if (errno == 0)
		return (NUM_OK);
	if (errno == ERANGE && *valp == ULONG_MAX)
		return (NUM_OVER);
	return (NUM_ERR);
}

/*
 * nget_slong --
 *      Convert a signed long, checking for overflow and underflow.
 *
 * PUBLIC: enum nresult nget_slong(long *, const char *, char **, int);
 */
enum nresult
nget_slong(long *valp, const char *p, char **endp, int base)
{
	errno = 0;
	*valp = strtol(p, endp, base);
	if (errno == 0)
		return (NUM_OK);
	if (errno == ERANGE) {
		if (*valp == LONG_MAX)
			return (NUM_OVER);
		if (*valp == LONG_MIN)
			return (NUM_UNDER);
	}
	return (NUM_ERR);
}

#ifdef DEBUG
#include <stdarg.h>

/*
 * TRACE --
 *	debugging trace routine.
 *
 * PUBLIC: void TRACE(SCR *, const char *, ...);
 */
void
TRACE(SCR *sp, const char *fmt, ...)
{
	FILE *tfp;
	va_list ap;

	if ((tfp = sp->gp->tracefp) == NULL)
		return;
	va_start(ap, fmt);
	(void)vfprintf(tfp, fmt, ap);
	va_end(ap);

	(void)fflush(tfp);
}
#endif
