/*		Case-independent string comparison		HTString.c
**
**	Original version came with listserv implementation.
**	Version TBL Oct 91 replaces one which modified the strings.
**	02-Dec-91 (JFG) Added stralloccopy and stralloccat
**	23 Jan 92 (TBL) Changed strallocc* to 8 char HTSAC* for VM and suchlike
**	 6 Oct 92 (TBL) Moved WWW_TraceFlag in here to be in library
*/
#include <ctype.h>
#include "HTUtils.h"
#include "tcp.h"

#include "LYLeaks.h"
#include "LYStrings.h"

#define FREE(x) if (x) {free(x); x = NULL;}

PUBLIC int WWW_TraceFlag = 0;	/* Global trace flag for ALL W3 code */

#ifndef VC
#define VC "unknown"
#endif /* !VC */

PUBLIC CONST char * HTLibraryVersion = VC; /* String for help screen etc */

/*
**     strcasecomp8 is a variant of strcasecomp (below)
**     ------------		    -----------
**     but uses 8bit upper/lower case information
**     from the current display charset.
**     It returns 0 if exact match.
*/
PUBLIC int strcasecomp8 ARGS2(
       CONST char*,    a,
       CONST char *,   b)
{
    CONST char *p = a;
    CONST char *q = b;

    for ( ; *p && *q; p++, q++) {
	int diff = UPPER8(*p, *q);
	if (diff) return diff;
    }
    if (*p)
	return 1;	/* p was longer than q */
    if (*q)
	return -1;	/* p was shorter than q */
    return 0;		/* Exact match */
}

/*
**     strncasecomp8 is a variant of strncasecomp (below)
**     -------------		     ------------
**     but uses 8bit upper/lower case information
**     from the current display charset.
**     It returns 0 if exact match.
*/
PUBLIC int strncasecomp8 ARGS3(
	CONST char*,	a,
	CONST char *,	b,
	int,		n)
{
    CONST char *p = a;
    CONST char *q = b;

    for ( ; ; p++, q++) {
	int diff;
	if (p == (a+n))
	    return 0;	/*   Match up to n characters */
	if (!(*p && *q))
	    return (*p - *q);
	diff = UPPER8(*p, *q);
	if (diff)
	    return diff;
    }
    /*NOTREACHED*/
}
#ifndef VM		/* VM has these already it seems */

/*	Strings of any length
**	---------------------
*/
PUBLIC int strcasecomp ARGS2(
	CONST char*,	a,
	CONST char *,	b)
{
    CONST char *p = a;
    CONST char *q = b;

    for ( ; *p && *q; p++, q++) {
	int diff = TOLOWER(*p) - TOLOWER(*q);
	if (diff) return diff;
    }
    if (*p)
	return 1;	/* p was longer than q */
    if (*q)
	return -1;	/* p was shorter than q */
    return 0;		/* Exact match */
}


/*	With count limit
**	----------------
*/
PUBLIC int strncasecomp ARGS3(
	CONST char*,	a,
	CONST char *,	b,
	int,		n)
{
    CONST char *p = a;
    CONST char *q = b;

    for ( ; ; p++, q++) {
	int diff;
	if (p == (a+n))
	    return 0;	/*   Match up to n characters */
	if (!(*p && *q))
	    return (*p - *q);
	diff = TOLOWER(*p) - TOLOWER(*q);
	if (diff)
	    return diff;
    }
    /*NOTREACHED*/
}
#endif /* VM */

/*	Allocate a new copy of a string, and returns it
*/
PUBLIC char * HTSACopy ARGS2(
	char **,	dest,
	CONST char *,	src)
{
    FREE(*dest);
    if (src) {
	*dest = (char *) malloc (strlen(src) + 1);
	if (*dest == NULL)
	    outofmem(__FILE__, "HTSACopy");
	strcpy (*dest, src);
    }
    return *dest;
}

/*	String Allocate and Concatenate
*/
PUBLIC char * HTSACat ARGS2(
	char **,	dest,
	CONST char *,	src)
{
    if (src && *src) {
	if (*dest) {
	    int length = strlen(*dest);
	    *dest = (char *)realloc(*dest, length + strlen(src) + 1);
	    if (*dest == NULL)
		outofmem(__FILE__, "HTSACat");
	    strcpy (*dest + length, src);
	} else {
	    *dest = (char *)malloc(strlen(src) + 1);
	    if (*dest == NULL)
		outofmem(__FILE__, "HTSACat");
	    strcpy (*dest, src);
	}
    }
    return *dest;
}


/*	Find next Field
**	---------------
**
** On entry,
**	*pstr	points to a string containig white space separated
**		field, optionlly quoted.
**
** On exit,
**	*pstr	has been moved to the first delimiter past the
**		field
**		THE STRING HAS BEEN MUTILATED by a 0 terminator
**
**	returns a pointer to the first field
*/
PUBLIC char * HTNextField ARGS1(
	char **,	pstr)
{
    char * p = *pstr;
    char * start;			/* start of field */

    while (*p && WHITE(*p))
	p++;				/* Strip white space */
    if (!*p) {
	*pstr = p;
	return NULL;		/* No first field */
    }
    if (*p == '"') {			/* quoted field */
	p++;
	start = p;
	for (; *p && *p!='"'; p++) {
	    if (*p == '\\' && p[1])
		p++;			/* Skip escaped chars */
	}
    } else {
	start = p;
	while (*p && !WHITE(*p))
	    p++;			/* Skip first field */
    }
    if (*p)
	*p++ = '\0';
    *pstr = p;
    return start;
}

/*	Find next Token
**	---------------
**	Finds the next token in a string
**	On entry,
**	*pstr	points to a string to be parsed.
**	delims	lists characters to be recognized as delimiters.
**		If NULL default is white white space "," ";" or "=".
**		The word can optionally be quoted or enclosed with
**		chars from bracks.
**		Comments surrrounded by '(' ')' are filtered out
**		unless they are specifically reqested by including
**		' ' or '(' in delims or bracks.
**	bracks	lists bracketing chars.  Some are recognized as
**		special, for those give the opening char.
**		If NULL defaults to <"> and "<" ">".
**	found	points to location to fill with the ending delimiter
**		found, or is NULL.
**
**	On exit,
**	*pstr	has been moved to the first delimiter past the
**		field
**		THE STRING HAS BEEN MUTILATED by a 0 terminator
**	found	points to the delimiter found unless it was NULL.
**	Returns a pointer to the first word or NULL on error
*/
PUBLIC char * HTNextTok ARGS4(
	char **,	pstr,
	const char *,	delims,
	const char *,	bracks,
	char *, 	found)
{
    char * p = *pstr;
    char * start = NULL;
    BOOL get_blanks, skip_comments;
    BOOL get_comments;
    BOOL get_closing_char_too = FALSE;
    char closer;
    if (!pstr || !*pstr) return NULL;
    if (!delims) delims = " ;,=" ;
    if (!bracks) bracks = "<\"" ;

    get_blanks = (!strchr(delims,' ') && !strchr(bracks,' '));
    get_comments = (strchr(bracks,'(') != NULL);
    skip_comments = (!get_comments && !strchr(delims,'(') && !get_blanks);
#define skipWHITE(c) (!get_blanks && WHITE(c))

    while (*p && skipWHITE(*p))
	p++;				/* Strip white space */
    if (!*p) {
	*pstr = p;
	if (found) *found = '\0';
	return NULL;		/* No first field */
    }
    while (1) {
	/* Strip white space and other delimiters */
	while (*p && (skipWHITE(*p) || strchr(delims,*p))) p++;
	if (!*p) {
	    *pstr = p;
	    if (found) *found = *(p-1);
	    return NULL;					 /* No field */
	}

	if (*p == '(' && (skip_comments || get_comments)) {	  /* Comment */
	    int comment_level = 0;
	    if (get_comments && !start) start = p+1;
	    for(;*p && (*p!=')' || --comment_level>0); p++) {
		if (*p == '(') comment_level++;
		else if (*p == '"') {	      /* quoted field within Comment */
		    for(p++; *p && *p!='"'; p++)
			if (*p == '\\' && *(p+1)) p++; /* Skip escaped chars */
		    if (!*p) break; /* (invalid) end of string found, leave */
		}
		if (*p == '\\' && *(p+1)) p++;	       /* Skip escaped chars */
	    }
	    if (get_comments)
		break;
	    if (*p) p++;
	    if (get_closing_char_too) {
		if (!*p || (!strchr(bracks,*p) && strchr(delims,*p))) {
		    break;
		} else
		    get_closing_char_too = (strchr(bracks,*p) != NULL);
	    }
	} else if (strchr(bracks,*p)) {        /* quoted or bracketted field */
	    switch (*p) {
	       case '<': closer = '>'; break;
	       case '[': closer = ']'; break;
	       case '{': closer = '}'; break;
	       case ':': closer = ';'; break;
	    default:	 closer = *p;
	    }
	    if (!start) start = ++p;
	    for(;*p && *p!=closer; p++)
		if (*p == '\\' && *(p+1)) p++;	       /* Skip escaped chars */
	    if (get_closing_char_too) {
		p++;
		if (!*p || (!strchr(bracks,*p) && strchr(delims,*p))) {
		    break;
		} else
		    get_closing_char_too = (strchr(bracks,*p) != NULL);
	    } else
	    break;			    /* kr95-10-9: needs to stop here */
#if 0
	} else if (*p == '<') { 			     /* quoted field */
	    if (!start) start = ++p;
	    for(;*p && *p!='>'; p++)
		if (*p == '\\' && *(p+1)) p++;	       /* Skip escaped chars */
	    break;			    /* kr95-10-9: needs to stop here */
#endif
	} else {					      /* Spool field */
	    if (!start) start = p;
	    while(*p && !skipWHITE(*p) && !strchr(bracks,*p) &&
					  !strchr(delims,*p))
		p++;
	    if (*p && strchr(bracks,*p)) {
		get_closing_char_too = TRUE;
	    } else {
		if (*p=='(' && skip_comments) {
		    *pstr = p;
		    HTNextTok(pstr, NULL, "(", found);	/*	Advance pstr */
		    *p = '\0';
		    if (*pstr && **pstr) (*pstr)++;
		    return start;
		}
		    break;					   /* Got it */
	    }
	}
    }
    if (found) *found = *p;

    if (*p) *p++ = '\0';
    *pstr = p;
    return start;
}
