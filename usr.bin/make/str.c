/*	$OpenBSD: str.c,v 1.14 2000/07/17 23:01:20 espie Exp $	*/
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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

#ifndef lint
#if 0
static char     sccsid[] = "@(#)str.c	5.8 (Berkeley) 6/1/90";
#else
static char rcsid[] = "$OpenBSD: str.c,v 1.14 2000/07/17 23:01:20 espie Exp $";
#endif
#endif				/* not lint */

#include "make.h"

/*-
 * str_concat --
 *	concatenate the two strings, possibly inserting a separator
 *
 * returns --
 *	the resulting string in allocated space.
 */
char *
str_concat(s1, s2, sep)
    const char *s1, *s2;
    char sep;
{
    size_t len1, len2;
    char *result;

    /* get the length of both strings */
    len1 = strlen(s1);
    len2 = strlen(s2);

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
    memcpy(result + len1, s2, len2 + 1);
    return result;
}

/*-
 * brk_string --
 *	Fracture a string into an array of words (as delineated by tabs or
 *	spaces) taking quotation marks into account.  Leading tabs/spaces
 *	are ignored.
 *
 * returns --
 *	Pointer to the array of pointers to the words.  To make life easier,
 *	the first word is always the value of the .MAKE variable.
 */
char **
brk_string(str, store_argc, expand, buffer)
	const char *str;
	int *store_argc;
	Boolean expand;
	char **buffer;
{
	register int argc, ch;
	char inquote;
	const char *p;
	char *start, *t;
	size_t len;
	int argmax = 50;
	size_t curlen = 0;
    	char **argv = (char **)emalloc((argmax + 1) * sizeof(char *));

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
		switch(ch = *p) {
		case '"':
		case '\'':
			if (inquote) {
				if (inquote == ch)
					inquote = '\0';
				else
					break;
			} else {
				inquote = (char) ch;
				/* Don't miss "" or '' */
				if (start == NULL && p[1] == inquote) {
					start = t + 1;
					break;
				}
			}
			if (!expand) {
				if (!start)
					start = t;
				*t++ = ch;
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
				argmax *= 2;		/* ramp up fast */
				argv = (char **)erealloc(argv,
				    (argmax + 1) * sizeof(char *));
			}
			argv[argc++] = start;
			start = (char *)NULL;
			if (ch == '\n' || ch == '\0')
				goto done;
			continue;
		case '\\':
			if (!expand) {
				if (!start)
					start = t;
				*t++ = '\\';
				ch = *++p;
				break;
			}

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
		*t++ = (char) ch;
	}
done:	argv[argc] = (char *)NULL;
	*store_argc = argc;
	return(argv);
}

/*
 * Str_Match --
 *
 * See if a particular string matches a particular pattern.
 *
 * Results: TRUE is returned if string matches pattern, FALSE otherwise. The
 * matching operation permits the following special characters in the
 * pattern: *?\[] (see the man page for details on what these mean).
 */
Boolean
Str_Match(string, pattern)
    const char *string;			/* String */
    const char *pattern;		/* Pattern */
{
    while (*pattern != '\0') {
	/* Check for a "*" as the next pattern character.  It matches
	 * any substring.  We handle this by calling ourselves
	 * recursively for each postfix of string, until either we
	 * match or we reach the end of the string.  */
	if (*pattern == '*') {
	    pattern++;
	    /* Skip over contiguous  sequences of `?*', so that recursive
	     * calls only occur on `real' characters.  */
	    while (*pattern == '?' || *pattern == '*') {
		if (*pattern == '?') {
		    if (*string == '\0')
			return FALSE;
		    else
			string++;
		}
		pattern++;
	    }
	    if (*pattern == '\0')
		return TRUE;
	    for (; *string != '\0'; string++)
		if (Str_Match(string, pattern))
		    return TRUE;
	    return FALSE;
	} else if (*string == '\0') 
	    return FALSE;
	/* Check for a "[" as the next pattern character.  It is
	 * followed by a list of characters that are acceptable, or
	 * by a range (two characters separated by "-").  */
	else if (*pattern == '[') {
	    pattern++;
	    if (*pattern == '\0')
	    	return FALSE;
	    if (*pattern == '!' || *pattern == '^') {
		pattern++;
		if (*pattern == '\0')
			return FALSE;
		/* Negative match */
		for (;;) {
		    if (*pattern == '\\') {
			if (*++pattern == '\0')
			    return FALSE;
		    }
		    if (*pattern == *string)
			return FALSE;
		    if (pattern[1] == '-') {
			if (pattern[2] == '\0')
			    return FALSE;
			if (*pattern < *string && *string <= pattern[2])
			    return FALSE;
			if (pattern[2] <= *string && *string < *pattern)
			    return FALSE;
			pattern += 3;
		    } else
			pattern++;
		    if (*pattern == '\0')
		    	return FALSE;
		    /* The test for ']' is done at the end so that ']'
		     * can be used at the start of the range without '\' */
		    if (*pattern == ']')
		    	break;
		}
	    } else {
		for (;;) {
		    if (*pattern == '\\') {
			if (*++pattern == '\0')
			    return FALSE;
		    }
		    if (*pattern == *string)
			break;
		    if (pattern[1] == '-') {
			if (pattern[2] == '\0')
			    return FALSE;
			if (*pattern < *string && *string <= pattern[2])
			    break;
			if (pattern[2] <= *string && *string < *pattern)
			    break;
			pattern += 3;
		    } else
			pattern++;
		    /* The test for ']' is done at the end so that ']'
		     * can be used at the start of the range without '\' */
		    if (*pattern == '\0' || *pattern == ']')
		    	return FALSE;
		}
		/* Found matching character, skip over rest of class.  */
		while (*pattern != ']') {
		    if (*pattern == '\\')
			pattern++;
		    /* A non-terminated character class is ok.  */
		    if (*pattern == '\0')
			break;
		    pattern++;
		}
	    }
	}
	/* '?' matches any single character, so shunt test.  */
	else if (*pattern != '?') {
	    /* If the next pattern character is '\', just strip off the
	     * '\' so we do exact matching on the character that follows.  */
	    if (*pattern == '\\') {
		if (*++pattern == '\0')
		    return FALSE;
	    }
	    /* There's no special character.  Just make sure that 
	     * the next characters of each string match.  */
	    if (*pattern != *string)
		return FALSE;
	}
	pattern++;
	string++;
    }
    if (*string == '\0')
	return TRUE;
    else
	return FALSE;
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
Str_SYSVMatch(word, pattern, len)
    const char	*word;		/* Word to examine */
    const char	*pattern;	/* Pattern to examine against */
    size_t	*len;		/* Number of characters to substitute */
{
    const char *p = pattern;
    const char *w = word;
    const char *m;

    if (*p == '\0') {
	/* Null pattern is the whole string */
	*len = strlen(w);
	return w;
    }

    if ((m = strchr(p, '%')) != NULL) {
	/* check that the prefix matches */
	for (; p != m && *w && *w == *p; w++, p++)
	     continue;

	if (p != m)
	    return NULL;	/* No match */

	if (*++p == '\0') {
	    /* No more pattern, return the rest of the string */
	    *len = strlen(w);
	    return w;
	}
    }

    m = w;

    /* Find a matching tail */
    do
	if (strcmp(p, w) == 0) {
	    *len = w - m;
	    return m;
	}
    while (*w++ != '\0');

    return NULL;
}


/*-
 *-----------------------------------------------------------------------
 * Str_SYSVSubst --
 *	Substitute '%' on the pattern with len characters from src.
 *	If the pattern does not contain a '%' prepend len characters
 *	from src.
 *
 * Side Effects:
 *	Places result on buf
 *-----------------------------------------------------------------------
 */
void
Str_SYSVSubst(buf, pat, src, len)
    Buffer buf;
    const char *pat;
    const char *src;
    size_t   len;
{
    const char *m;

    if ((m = strchr(pat, '%')) != NULL) {
	/* Copy the prefix */
	Buf_AddInterval(buf, pat, m);
	/* skip the % */
	pat = m + 1;
    }

    /* Copy the pattern */
    Buf_AddChars(buf, len, src);

    /* append the rest */
    Buf_AddString(buf, pat);
}

char *
interval_dup(begin, end)
    const char *begin;
    const char *end;
{
    char *s;

    s = emalloc(end - begin + 1);
    memcpy(s, begin, end - begin);
    s[end-begin] = '\0';
    return s;
}
