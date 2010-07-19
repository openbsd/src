/*	$OpenBSD: str.c,v 1.27 2010/07/19 19:46:44 espie Exp $	*/
/*	$NetBSD: str.c,v 1.13 1996/11/06 17:59:23 christos Exp $	*/

/*-
 * Copyright (c) 1988, 1989, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1989 by Berkeley Softworks
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Adam de Boor.
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

#include <ctype.h>
#include <string.h>
#include "config.h"
#include "defines.h"
#include "str.h"
#include "memory.h"
#include "buf.h"

/* helpers for Str_Matchi */
static bool range_match(char, const char **, const char *);
static bool star_match(const char *, const char *, const char *, const char *);

char *
Str_concati(const char *s1, const char *e1, const char *s2, const char *e2,
    int sep)
{
	size_t len1, len2;
	char *result;

	/* get the length of both strings */
	len1 = e1 - s1;
	len2 = e2 - s2;

	/* space for separator */
	if (sep)
		len1++;
	result = emalloc(len1 + len2 + 1);

	/* copy first string into place */
	memcpy(result, s1, len1);

	/* add separator character */
	if (sep)
		result[len1-1] = sep;

	/* copy second string plus EOS into place */
	memcpy(result + len1, s2, len2);
	result[len1+len2] = '\0';
	return result;
}

/*-
 * brk_string --
 *	Fracture a string into an array of words (as delineated by tabs or
 *	spaces) taking quotation marks into account.  Leading tabs/spaces
 *	are ignored.
 *
 * returns --
 *	Pointer to the array of pointers to the words.	To make life easier,
 *	the first word is always the value of the .MAKE variable.
 */
char **
brk_string(const char *str, int *store_argc, char **buffer)
{
	int argc;
	char ch;
	char inquote;
	const char *p;
	char *start, *t;
	size_t len;
	int argmax = 50;
	size_t curlen = 0;
	char **argv = emalloc((argmax + 1) * sizeof(char *));

	/* skip leading space chars. */
	for (; *str == ' ' || *str == '\t'; ++str)
		continue;

	/* allocate room for a copy of the string */
	if ((len = strlen(str) + 1) > curlen)
		*buffer = emalloc(curlen = len);

	/*
	 * copy the string; at the same time, parse backslashes,
	 * quotes and build the argument list.
	 */
	argc = 0;
	inquote = '\0';
	for (p = str, start = t = *buffer;; ++p) {
		switch (ch = *p) {
		case '"':
		case '\'':
			if (inquote) {
				if (inquote == ch)
					inquote = '\0';
				else
					break;
			} else {
				inquote = ch;
				/* Don't miss "" or '' */
				if (start == NULL && p[1] == inquote) {
					start = t + 1;
					break;
				}
			}
			continue;
		case ' ':
		case '\t':
		case '\n':
			if (inquote)
				break;
			if (!start)
				continue;
			/* FALLTHROUGH */
		case '\0':
			/*
			 * end of a token -- make sure there's enough argv
			 * space and save off a pointer.
			 */
			if (!start)
				goto done;

			*t++ = '\0';
			if (argc == argmax) {
				argmax *= 2;	/* ramp up fast */
				argv = erealloc(argv,
				    (argmax + 1) * sizeof(char *));
			}
			argv[argc++] = start;
			start = NULL;
			if (ch == '\n' || ch == '\0')
				goto done;
			continue;
		case '\\':
			switch (ch = *++p) {
			case '\0':
			case '\n':
				/* hmmm; fix it up as best we can */
				ch = '\\';
				--p;
				break;
			case 'b':
				ch = '\b';
				break;
			case 'f':
				ch = '\f';
				break;
			case 'n':
				ch = '\n';
				break;
			case 'r':
				ch = '\r';
				break;
			case 't':
				ch = '\t';
				break;
			}
			    break;
		}
		if (!start)
			start = t;
		*t++ = ch;
	}
    done:
	    argv[argc] = NULL;
	    *store_argc = argc;
	    return argv;
}


const char *
iterate_words(const char **end)
{
	const char	*start, *p;
	char	state = 0;
	start = *end;

	while (isspace(*start))
		start++;
	if (*start == '\0')
		return NULL;

	for (p = start;; p++)
	    switch(*p) {
	    case '\\':
		    if (p[1] != '\0')
			    p++;
		    break;
	    case '\'':
	    case '"':
		    if (state == *p)
			    state = 0;
		    else if (state == 0)
			    state = *p;
		    break;
	    case ' ':
	    case '\t':
		    if (state != 0)
			    break;
		    /* FALLTHROUGH */
	    case '\0':
		    *end = p;
		    return start;
	    default:
		    break;
	    }
}

static bool
star_match(const char *string, const char *estring,
    const char *pattern, const char *epattern)
{
	/* '*' matches any substring.  We handle this by calling ourselves
	 * recursively for each postfix of string, until either we match or
	 * we reach the end of the string.  */
	pattern++;
	/* Skip over contiguous  sequences of `?*', so that
	 * recursive calls only occur on `real' characters.  */
	while (pattern != epattern &&
		(*pattern == '?' || *pattern == '*')) {
		if (*pattern == '?') {
			if (string == estring)
				return false;
			else
				string++;
		}
		pattern++;
	}
	if (pattern == epattern)
		return true;
	for (; string != estring; string++)
		if (Str_Matchi(string, estring, pattern,
		    epattern))
			return true;
	return false;
}

static bool
range_match(char c, const char **ppat, const char *epattern)
{
	if (*ppat == epattern) {
		if (c == '[')
			return true;
		else
			return false;
	}
	if (**ppat == '!' || **ppat == '^') {
		(*ppat)++;
		return !range_match(c, ppat, epattern);
	}
	for (;;) {
		if (**ppat == '\\') {
			if (++(*ppat) == epattern)
				return false;
		}
		if (**ppat == c)
			break;
		if ((*ppat)[1] == '-') {
			if (*ppat + 2 == epattern)
				return false;
			if (**ppat < c && c <= (*ppat)[2])
				break;
			if ((*ppat)[2] <= c && c < **ppat)
				break;
			*ppat += 3;
		} else
			(*ppat)++;
		/* The test for ']' is done at the end
		 * so that ']' can be used at the
		 * start of the range without '\' */
		if (*ppat == epattern || **ppat == ']')
			return false;
	}
	/* Found matching character, skip over rest
	 * of class.  */
	while (**ppat != ']') {
		if (**ppat == '\\')
			(*ppat)++;
		/* A non-terminated character class
		 * is ok. */
		if (*ppat == epattern)
			break;
		(*ppat)++;
	}
	return true;
}

bool
Str_Matchi(const char *string, const char *estring,
    const char *pattern, const char *epattern)
{
	while (pattern != epattern) {
		/* Check for a "*" as the next pattern character.  */
		if (*pattern == '*')
			return star_match(string, estring, pattern, epattern);
		else if (string == estring)
			return false;
		/* Check for a "[" as the next pattern character.  It is
		 * followed by a list of characters that are acceptable, or
		 * by a range (two characters separated by "-").  */
		else if (*pattern == '[') {
			pattern++;
			if (!range_match(*string, &pattern, epattern))
				return false;

		}
		/* '?' matches any single character, so shunt test.  */
		else if (*pattern != '?') {
			/* If the next pattern character is '\', just strip
			 * off the '\' so we do exact matching on the
			 * character that follows.  */
			if (*pattern == '\\') {
				if (++pattern == epattern)
					return false;
			}
			/* There's no special character.  Just make sure that
			 * the next characters of each string match.  */
			if (*pattern != *string)
				return false;
		}
		pattern++;
		string++;
	}
	if (string == estring)
		return true;
	else
		return false;
}


/*-
 *-----------------------------------------------------------------------
 * Str_SYSVMatch --
 *	Check word against pattern for a match (% is wild),
 *
 * Results:
 *	Returns the beginning position of a match or null. The number
 *	of characters matched is returned in len.
 *-----------------------------------------------------------------------
 */
const char *
Str_SYSVMatch(const char *word, const char *pattern, size_t *len)
{
	const char *p = pattern;
	const char *w = word;
	const char *m;

	if (*p == '\0') {
		/* Null pattern is the whole string.  */
		*len = strlen(w);
		return w;
	}

	if ((m = strchr(p, '%')) != NULL) {
		/* Check that the prefix matches.  */
		for (; p != m && *w && *w == *p; w++, p++)
			 continue;

		if (p != m)
			return NULL;	/* No match.  */

		if (*++p == '\0') {
			/* No more pattern, return the rest of the string. */
			*len = strlen(w);
			return w;
		}
	}

	m = w;

	/* Find a matching tail.  */
	do {
		if (strcmp(p, w) == 0) {
			*len = w - m;
			return m;
		}
	} while (*w++ != '\0');

	return NULL;
}


/*-
 *-----------------------------------------------------------------------
 * Str_SYSVSubst --
 *	Substitute '%' in the pattern with len characters from src.
 *	If the pattern does not contain a '%' prepend len characters
 *	from src.
 *
 * Side Effects:
 *	Adds result to buf
 *-----------------------------------------------------------------------
 */
void
Str_SYSVSubst(Buffer buf, const char *pat, const char *src, size_t len)
{
	const char *m;

	if ((m = strchr(pat, '%')) != NULL) {
		/* Copy the prefix.  */
		Buf_Addi(buf, pat, m);
		/* Skip the %.	*/
		pat = m + 1;
	}

	/* Copy the pattern.  */
	Buf_AddChars(buf, len, src);

	/* Append the rest.  */
	Buf_AddString(buf, pat);
}

char *
Str_dupi(const char *begin, const char *end)
{
	char *s;

	s = emalloc(end - begin + 1);
	memcpy(s, begin, end - begin);
	s[end-begin] = '\0';
	return s;
}

char *
escape_dupi(const char *begin, const char *end, const char *set)
{
	char *s, *t;

	t = s = emalloc(end - begin + 1);
	while (begin != end) {
		if (*begin == '\\') {
			begin++;
			if (begin == end) {
				*t++ = '\\';
				break;
			}
			if (strchr(set, *begin) == NULL)
				*t++ = '\\';
		}
		*t++ = *begin++;
	}
	*t++ = '\0';
	return s;
}

char *
Str_rchri(const char *begin, const char *end, int c)
{
	if (begin != end)
		do {
			if (*--end == c)
				return (char *)end;
		} while (end != begin);
	return NULL;
}
