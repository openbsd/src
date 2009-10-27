/*	$OpenBSD: look.c,v 1.4 2009/10/27 23:59:43 deraadt Exp $	*/

/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * David Hitz of Auspex Systems, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>

u_char	*binary_search(u_char *, u_char *, u_char *);
u_char	*linear_search(u_char *, u_char *, u_char *);
int	 compare(u_char *, u_char *, u_char *);
int	 look(u_char *, u_char *, u_char *);

int
look(u_char *string, u_char *front, u_char *back)
{
	u_char *s;

	/* Convert string to lower case before searching. */
	for (s = string; *s; s++) {
		if (isupper(*s))
			*s = _tolower(*s);
	}

	front = binary_search(string, front, back);
	front = linear_search(string, front, back);

	return (front != NULL);
}

/*
 * Binary search for "string" in memory between "front" and "back".
 * 
 * This routine is expected to return a pointer to the start of a line at
 * *or before* the first word matching "string".  Relaxing the constraint
 * this way simplifies the algorithm.
 * 
 * Invariants:
 * 	front points to the beginning of a line at or before the first 
 *	matching string.
 * 
 * 	back points to the beginning of a line at or after the first 
 *	matching line.
 * 
 * Base of the Invariants.
 * 	front = NULL; 
 *	back = EOF;
 * 
 * Advancing the Invariants:
 * 
 * 	p = first newline after halfway point from front to back.
 * 
 * 	If the string at "p" is not greater than the string to match, 
 *	p is the new front.  Otherwise it is the new back.
 * 
 * Termination:
 * 
 * 	The definition of the routine allows it return at any point, 
 *	since front is always at or before the line to print.
 * 
 * 	In fact, it returns when the chosen "p" equals "back".  This 
 *	implies that there exists a string is least half as long as 
 *	(back - front), which in turn implies that a linear search will 
 *	be no more expensive than the cost of simply printing a string or two.
 * 
 * 	Trying to continue with binary search at this point would be 
 *	more trouble than it's worth.
 */
#define	SKIP_PAST_NEWLINE(p, back) \
	while (p < back && *p++ != '\n');

u_char *
binary_search(u_char *string, u_char *front, u_char *back)
{
	u_char *p;

	p = front + (back - front) / 2;
	SKIP_PAST_NEWLINE(p, back);

	/*
	 * If the file changes underneath us, make sure we don't
	 * infinitely loop.
	 */
	while (p < back && back > front) {
		if (compare(string, p, back) > 0)
			front = p;
		else
			back = p;
		p = front + (back - front) / 2;
		SKIP_PAST_NEWLINE(p, back);
	}
	return (front);
}

/*
 * Find the first line that matches string, linearly searching from front
 * to back.
 * 
 * Return NULL for no such line.
 * 
 * This routine assumes:
 * 
 * 	o front points at the first character in a line. 
 *	o front is before or at the first line to be printed.
 */
u_char *
linear_search(u_char *string, u_char *front, u_char *back)
{
	int result;

	while (front < back) {
		result = compare(string, front, back);
		if (result == 0)
			return (front);	/* found it */
		if (result < 0)
			return (NULL);	/* not there */

		SKIP_PAST_NEWLINE(front, back);
	}
	return (NULL);
}

int
compare(u_char *s1, u_char *s2, u_char *back)
{
	int ch;

	/* Note that s1 is already upper case. */
	for (;; ++s1, ++s2) {
		if (*s2 == '\n' || s2 == back)
			ch = '\0';
		else if (isupper(*s2))
			ch = _tolower(*s2);
		else
			ch = *s2;
		if (*s1 != ch)
			return (*s1 - ch);
		if (ch == '\0')
			return (0);
	}
}
