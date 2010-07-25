/*	$Id: mandoc.c,v 1.16 2010/07/25 18:05:54 schwarze Exp $ */
/*
 * Copyright (c) 2008, 2009, 2010 Kristaps Dzonsons <kristaps@bsd.lv>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include <sys/types.h>

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "mandoc.h"
#include "libmandoc.h"

static	int	 a2time(time_t *, const char *, const char *);


int
mandoc_special(char *p)
{
	int		 len, i;
	char		 term;
	char		*sv;
	
	len = 0;
	term = '\0';
	sv = p;

	assert('\\' == *p);
	p++;

	switch (*p++) {
#if 0
	case ('Z'):
		/* FALLTHROUGH */
	case ('X'):
		/* FALLTHROUGH */
	case ('x'):
		/* FALLTHROUGH */
	case ('w'):
		/* FALLTHROUGH */
	case ('v'):
		/* FALLTHROUGH */
	case ('S'):
		/* FALLTHROUGH */
	case ('R'):
		/* FALLTHROUGH */
	case ('o'):
		/* FALLTHROUGH */
	case ('N'):
		/* FALLTHROUGH */
	case ('l'):
		/* FALLTHROUGH */
	case ('L'):
		/* FALLTHROUGH */
	case ('H'):
		/* FALLTHROUGH */
	case ('h'):
		/* FALLTHROUGH */
	case ('D'):
		/* FALLTHROUGH */
	case ('C'):
		/* FALLTHROUGH */
	case ('b'):
		/* FALLTHROUGH */
	case ('B'):
		/* FALLTHROUGH */
	case ('a'):
		/* FALLTHROUGH */
	case ('A'):
		if (*p++ != '\'')
			return(0);
		term = '\'';
		break;
#endif
	case ('s'):
		if (ASCII_HYPH == *p)
			*p = '-';
		if ('+' == *p || '-' == *p)
			p++;

		i = ('s' != *(p - 1));

		switch (*p++) {
		case ('('):
			len = 2;
			break;
		case ('['):
			term = ']';
			break;
		case ('\''):
			term = '\'';
			break;
		case ('0'):
			i++;
			/* FALLTHROUGH */
		default:
			len = 1;
			p--;
			break;
		}

		if (ASCII_HYPH == *p)
			*p = '-';
		if ('+' == *p || '-' == *p) {
			if (i++)
				return(0);
			p++;
		} 
		
		if (0 == i)
			return(0);
		break;
#if 0
	case ('Y'):
		/* FALLTHROUGH */
	case ('V'):
		/* FALLTHROUGH */
	case ('$'):
		/* FALLTHROUGH */
	case ('n'):
		/* FALLTHROUGH */
	case ('k'):
		/* FALLTHROUGH */
#endif
	case ('M'):
		/* FALLTHROUGH */
	case ('m'):
		/* FALLTHROUGH */
	case ('f'):
		/* FALLTHROUGH */
	case ('F'):
		/* FALLTHROUGH */
	case ('*'):
		switch (*p++) {
		case ('('):
			len = 2;
			break;
		case ('['):
			term = ']';
			break;
		default:
			len = 1;
			p--;
			break;
		}
		break;
	case ('('):
		len = 2;
		break;
	case ('['):
		term = ']';
		break;
	default:
		len = 1;
		p--;
		break;
	}

	if (term) {
		for ( ; *p && term != *p; p++)
			if (ASCII_HYPH == *p)
				*p = '-';
		return(*p ? (int)(p - sv) : 0);
	}

	for (i = 0; *p && i < len; i++, p++)
		if (ASCII_HYPH == *p)
			*p = '-';
	return(i == len ? (int)(p - sv) : 0);
}


void *
mandoc_calloc(size_t num, size_t size)
{
	void		*ptr;

	ptr = calloc(num, size);
	if (NULL == ptr) {
		perror(NULL);
		exit(EXIT_FAILURE);
	}

	return(ptr);
}


void *
mandoc_malloc(size_t size)
{
	void		*ptr;

	ptr = malloc(size);
	if (NULL == ptr) {
		perror(NULL);
		exit(EXIT_FAILURE);
	}

	return(ptr);
}


void *
mandoc_realloc(void *ptr, size_t size)
{

	ptr = realloc(ptr, size);
	if (NULL == ptr) {
		perror(NULL);
		exit(EXIT_FAILURE);
	}

	return(ptr);
}


char *
mandoc_strdup(const char *ptr)
{
	char		*p;

	p = strdup(ptr);
	if (NULL == p) {
		perror(NULL);
		exit(EXIT_FAILURE);
	}

	return(p);
}


static int
a2time(time_t *t, const char *fmt, const char *p)
{
	struct tm	 tm;
	char		*pp;

	memset(&tm, 0, sizeof(struct tm));

	pp = strptime(p, fmt, &tm);
	if (NULL != pp && '\0' == *pp) {
		*t = mktime(&tm);
		return(1);
	}

	return(0);
}


/*
 * Convert from a manual date string (see mdoc(7) and man(7)) into a
 * date according to the stipulated date type.
 */
time_t
mandoc_a2time(int flags, const char *p)
{
	time_t		 t;

	if (MTIME_MDOCDATE & flags) {
		if (0 == strcmp(p, "$" "Mdocdate$"))
			return(time(NULL));
		if (a2time(&t, "$" "Mdocdate: %b %d %Y $", p))
			return(t);
	}

	if (MTIME_CANONICAL & flags || MTIME_REDUCED & flags) 
		if (a2time(&t, "%b %d, %Y", p))
			return(t);

	if (MTIME_ISO_8601 & flags) 
		if (a2time(&t, "%Y-%m-%d", p))
			return(t);

	if (MTIME_REDUCED & flags) {
		if (a2time(&t, "%d, %Y", p))
			return(t);
		if (a2time(&t, "%Y", p))
			return(t);
	}

	return(0);
}


int
mandoc_eos(const char *p, size_t sz, int enclosed)
{
	const char *q;
	int found;

	if (0 == sz)
		return(0);

	/*
	 * End-of-sentence recognition must include situations where
	 * some symbols, such as `)', allow prior EOS punctuation to
	 * propogate outward.
	 */

	found = 0;
	for (q = p + (int)sz - 1; q >= p; q--) {
		switch (*q) {
		case ('\"'):
			/* FALLTHROUGH */
		case ('\''):
			/* FALLTHROUGH */
		case (']'):
			/* FALLTHROUGH */
		case (')'):
			if (0 == found)
				enclosed = 1;
			break;
		case ('.'):
			/* FALLTHROUGH */
		case ('!'):
			/* FALLTHROUGH */
		case ('?'):
			found = 1;
			break;
		default:
			return(found && (!enclosed || isalnum(*q)));
		}
	}

	return(found && !enclosed);
}


int
mandoc_hyph(const char *start, const char *c)
{

	/*
	 * Choose whether to break at a hyphenated character.  We only
	 * do this if it's free-standing within a word.
	 */

	/* Skip first/last character of buffer. */
	if (c == start || '\0' == *(c + 1))
		return(0);
	/* Skip first/last character of word. */
	if ('\t' == *(c + 1) || '\t' == *(c - 1))
		return(0);
	if (' ' == *(c + 1) || ' ' == *(c - 1))
		return(0);
	/* Skip double invocations. */
	if ('-' == *(c + 1) || '-' == *(c - 1))
		return(0);
	/* Skip escapes. */
	if ('\\' == *(c - 1))
		return(0);

	return(1);
}
