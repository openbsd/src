/*	$Id: mandoc.c,v 1.22 2011/03/07 01:35:33 schwarze Exp $ */
/*
 * Copyright (c) 2008, 2009, 2010 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2011 Ingo Schwarze <schwarze@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHORS DISCLAIM ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR
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

#define DATESIZE 32

static	int	 a2time(time_t *, const char *, const char *);
static	char	*time2a(time_t);

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
	case ('S'):
		/* FALLTHROUGH */
	case ('R'):
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
	case ('h'):
		/* FALLTHROUGH */
	case ('v'):
		/* FALLTHROUGH */
	case ('s'):
		if (ASCII_HYPH == *p)
			*p = '-';

		i = 0;
		if ('+' == *p || '-' == *p) {
			p++;
			i = 1;
		}

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
			i = 1;
			/* FALLTHROUGH */
		default:
			len = 1;
			p--;
			break;
		}

		if (ASCII_HYPH == *p)
			*p = '-';
		if ('+' == *p || '-' == *p) {
			if (i)
				return(0);
			p++;
		} 
		
		/* Handle embedded numerical subexp or escape. */

		if ('(' == *p) {
			while (*p && ')' != *p)
				if ('\\' == *p++) {
					i = mandoc_special(--p);
					if (0 == i)
						return(0);
					p += i;
				}

			if (')' == *p++)
				break;

			return(0);
		} else if ('\\' == *p) {
			if (0 == (i = mandoc_special(p)))
				return(0);
			p += i;
		}

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
#endif
	case ('k'):
		/* FALLTHROUGH */
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
	case ('z'):
		len = 1;
		if ('\\' == *p) {
			if (0 == (i = mandoc_special(p)))
				return(0);
			p += i;
			return(*p ? (int)(p - sv) : 0);
		}
		break;
	case ('o'):
		/* FALLTHROUGH */
	case ('w'):
		if ('\'' == *p++) {
			term = '\'';
			break;
		}
		/* FALLTHROUGH */
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
		exit((int)MANDOCLEVEL_SYSERR);
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
		exit((int)MANDOCLEVEL_SYSERR);
	}

	return(ptr);
}


void *
mandoc_realloc(void *ptr, size_t size)
{

	ptr = realloc(ptr, size);
	if (NULL == ptr) {
		perror(NULL);
		exit((int)MANDOCLEVEL_SYSERR);
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
		exit((int)MANDOCLEVEL_SYSERR);
	}

	return(p);
}

/*
 * Parse a quoted or unquoted roff-style request or macro argument.
 * Return a pointer to the parsed argument, which is either the original
 * pointer or advanced by one byte in case the argument is quoted.
 * Null-terminate the argument in place.
 * Collapse pairs of quotes inside quoted arguments.
 * Advance the argument pointer to the next argument,
 * or to the null byte terminating the argument line.
 */
char *
mandoc_getarg(char **cpp, mandocmsg msg, void *data, int ln, int *pos)
{
	char	 *start, *cp;
	int	  quoted, pairs, white;

	/* Quoting can only start with a new word. */
	start = *cpp;
	if ('"' == *start) {
		quoted = 1;
		start++;
	} else
		quoted = 0;

	pairs = 0;
	white = 0;
	for (cp = start; '\0' != *cp; cp++) {
		/* Move left after quoted quotes and escaped backslashes. */
		if (pairs)
			cp[-pairs] = cp[0];
		if ('\\' == cp[0]) {
			if ('\\' == cp[1]) {
				/* Poor man's copy mode. */
				pairs++;
				cp++;
			} else if (0 == quoted && ' ' == cp[1])
				/* Skip escaped blanks. */
				cp++;
		} else if (0 == quoted) {
			if (' ' == cp[0]) {
				/* Unescaped blanks end unquoted args. */
				white = 1;
				break;
			}
		} else if ('"' == cp[0]) {
			if ('"' == cp[1]) {
				/* Quoted quotes collapse. */
				pairs++;
				cp++;
			} else {
				/* Unquoted quotes end quoted args. */
				quoted = 2;
				break;
			}
		}
	}

	/* Quoted argument without a closing quote. */
	if (1 == quoted && msg)
		(*msg)(MANDOCERR_BADQUOTE, data, ln, *pos, NULL);

	/* Null-terminate this argument and move to the next one. */
	if (pairs)
		cp[-pairs] = '\0';
	if ('\0' != *cp) {
		*cp++ = '\0';
		while (' ' == *cp)
			cp++;
	}
	*pos += (cp - start) + (quoted ? 1 : 0);
	*cpp = cp;

	if ('\0' == *cp && msg && (white || ' ' == cp[-1]))
		(*msg)(MANDOCERR_EOLNSPACE, data, ln, *pos, NULL);

	return(start);
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


static char *
time2a(time_t t)
{
	struct tm	 tm;
	char		 buf[DATESIZE];
	char		*p;
	size_t		 nsz, rsz;
	int		 isz;

	localtime_r(&t, &tm);

	p = buf;
	rsz = DATESIZE;

	if (0 == (nsz = strftime(p, rsz, "%B ", &tm)))
		return(NULL);

	p += (int)nsz;
	rsz -= nsz;

	if (-1 == (isz = snprintf(p, rsz, "%d, ", tm.tm_mday)))
		return(NULL);

	p += isz;
	rsz -= isz;

	return(strftime(p, rsz, "%Y", &tm) ? buf : NULL);
}


char *
mandoc_normdate(char *in, mandocmsg msg, void *data, int ln, int pos)
{
	char		*out;
	time_t		 t;

	if (NULL == in || '\0' == *in ||
	    0 == strcmp(in, "$" "Mdocdate$")) {
		(*msg)(MANDOCERR_NODATE, data, ln, pos, NULL);
		time(&t);
	}
	else if (!a2time(&t, "$" "Mdocdate: %b %d %Y $", in) &&
	    !a2time(&t, "%b %d, %Y", in) &&
	    !a2time(&t, "%Y-%m-%d", in)) {
		(*msg)(MANDOCERR_BADDATE, data, ln, pos, NULL);
		t = 0;
	}
	out = t ? time2a(t) : NULL;
	return(mandoc_strdup(out ? out : in));
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
			return(found && (!enclosed || isalnum((unsigned char)*q)));
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
