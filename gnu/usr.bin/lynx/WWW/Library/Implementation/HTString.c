/*		Case-independent string comparison		HTString.c
**
**	Original version came with listserv implementation.
**	Version TBL Oct 91 replaces one which modified the strings.
**	02-Dec-91 (JFG) Added stralloccopy and stralloccat
**	23 Jan 92 (TBL) Changed strallocc* to 8 char HTSAC* for VM and suchlike
**	 6 Oct 92 (TBL) Moved WWW_TraceFlag in here to be in library
**	15 Nov 98 (TD)  Added HTSprintf.
*/

#include <HTUtils.h>

#include <LYLeaks.h>
#include <LYStrings.h>

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

#ifdef NOT_ASCII

/*	Case-insensitive with ASCII collating sequence
**	----------------
*/
PUBLIC int AS_casecomp ARGS2(
	CONST char*,	p,
	CONST char*,	q)
{
    int diff;

    for ( ; ; p++, q++) {
	if (!(*p && *q))
	    return ((unsigned char) *p - (unsigned char) *q);
	diff = TOASCII(TOLOWER(*p))
	     - TOASCII(TOLOWER(*q));
	if (diff)
	    return diff;
    }
    /*NOTREACHED*/
}


/*	With count limit and ASCII collating sequence
**	----------------
**	AS_cmp uses n == -1 to compare indefinite length.
*/
PUBLIC int AS_ncmp ARGS3(
	CONST char *,	p,
	CONST char *,	q,
	unsigned int,	n)
{
    CONST char *a = p;
    int diff;

    for ( ; (p-a) < n; p++, q++) {
	if (!(*p && *q))
	    return ((unsigned char) *p - (unsigned char) *q);
	diff = TOASCII(*p)
	     - TOASCII(*q);
	if (diff)
	    return diff;
    }
    return 0;	/*   Match up to n characters */
}


/*	With ASCII collating sequence
**	----------------
*/
PUBLIC int AS_cmp ARGS2(
	CONST char *,	p,
	CONST char *,	q)
{
    return( AS_ncmp( p, q, -1 ) );
}
#endif /* NOT_ASCII */


/*	Allocate a new copy of a string, and returns it
*/
PUBLIC char * HTSACopy ARGS2(
	char **,	dest,
	CONST char *,	src)
{
    if (src != 0) {
	if (src != *dest) {
	    FREE(*dest);
	    *dest = (char *) malloc (strlen(src) + 1);
	    if (*dest == NULL)
		outofmem(__FILE__, "HTSACopy");
	    strcpy (*dest, src);
	}
    } else {
	FREE(*dest);
    }
    return *dest;
}

/*	String Allocate and Concatenate
*/
PUBLIC char * HTSACat ARGS2(
	char **,	dest,
	CONST char *,	src)
{
    if (src && *src && (src != *dest)) {
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
	CONST char *,	delims,
	CONST char *,	bracks,
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

PRIVATE char *HTAlloc ARGS2(char *, ptr, size_t, length)
{
    if (ptr != 0)
	ptr = (char *)realloc(ptr, length);
    else
	ptr = (char *)malloc(length);
    if (ptr == 0)
	outofmem(__FILE__, "HTAlloc");
    return ptr;
}

/*
 * Replacement for sprintf, allocates buffer on the fly according to what's needed
 * for its arguments.  Unlike sprintf, this always concatenates to the destination
 * buffer, so we do not have to provide both flavors.
 */
typedef enum { Flags, Width, Prec, Type, Format } PRINTF;

#define VA_INTGR(type) ival = va_arg((*ap), type)
#define VA_FLOAT(type) fval = va_arg((*ap), type)
#define VA_POINT(type) pval = (void *)va_arg((*ap), type)

#define NUM_WIDTH 10	/* allow for width substituted for "*" in "%*s" */
		/* also number of chars assumed to be needed in addition
		   to a given precision in floating point formats */

#define GROW_EXPR(n) (((n) * 3) / 2)
#define GROW_SIZE 256

PRIVATE char * StrAllocVsprintf ARGS4(
	char **,	pstr,
	size_t,		dst_len,
	CONST char *,	fmt,
	va_list *,	ap)
{
    size_t tmp_len = GROW_SIZE;
    size_t have, need;
    char *tmp_ptr = 0;
    char *fmt_ptr;
    char *dst_ptr = *pstr;
    CONST char *format = fmt;

    if (fmt == 0 || *fmt == '\0')
	return 0;

    need = strlen(fmt) + 1;
    if ((fmt_ptr = malloc(need*NUM_WIDTH)) == 0
     || (tmp_ptr = malloc(tmp_len)) == 0) {
	outofmem(__FILE__, "StrAllocVsprintf");
    }

    if (dst_ptr == 0) {
	dst_ptr = HTAlloc(dst_ptr, have = GROW_SIZE + need);
    } else {
	have = strlen(dst_ptr) + 1;
	need += dst_len;
	if (have < need)
	    dst_ptr = HTAlloc(dst_ptr, have = GROW_SIZE + need);
    }

    while (*fmt != '\0') {
	if (*fmt == '%') {
	    static char dummy[] = "";
	    PRINTF state = Flags;
	    char *pval   = dummy;	/* avoid const-cast */
	    double fval  = 0.0;
	    int done     = FALSE;
	    int ival     = 0;
	    int prec     = -1;
	    int type     = 0;
	    int used     = 0;
	    int width    = -1;
	    size_t f     = 0;

	    fmt_ptr[f++] = *fmt;
	    while (*++fmt != '\0' && !done) {
		fmt_ptr[f++] = *fmt;

		if (isdigit(*fmt)) {
		    int num = *fmt - '0';
		    if (state == Flags && num != 0)
			state = Width;
		    if (state == Width) {
			if (width < 0)
			    width = 0;
			width = (width * 10) + num;
		    } else if (state == Prec) {
			if (prec < 0)
			    prec = 0;
			prec = (prec * 10) + num;
		    }
		} else if (*fmt == '*') {
		    VA_INTGR(int);
		    if (state == Flags)
			state = Width;
		    if (state == Width) {
			width = ival;
		    } else if (state == Prec) {
			prec = ival;
		    }
		    sprintf(&fmt_ptr[--f], "%d", ival);
		    f = strlen(fmt_ptr);
		} else if (isalpha(*fmt)) {
		    done = TRUE;
		    switch (*fmt) {
		    case 'Z': /* FALLTHRU */
		    case 'h': /* FALLTHRU */
		    case 'l': /* FALLTHRU */
		    case 'L': /* FALLTHRU */
			done = FALSE;
			type = *fmt;
			break;
		    case 'o': /* FALLTHRU */
		    case 'i': /* FALLTHRU */
		    case 'd': /* FALLTHRU */
		    case 'u': /* FALLTHRU */
		    case 'x': /* FALLTHRU */
		    case 'X': /* FALLTHRU */
			if (type == 'l')
			    VA_INTGR(long);
			else if (type == 'Z')
			    VA_INTGR(size_t);
			else
			    VA_INTGR(int);
			used = 'i';
			break;
		    case 'f': /* FALLTHRU */
		    case 'e': /* FALLTHRU */
		    case 'E': /* FALLTHRU */
		    case 'g': /* FALLTHRU */
		    case 'G': /* FALLTHRU */
#if 0	/* we don't need this, it doesn't work on SunOS 4.x */
			if (type == 'L')
			    VA_FLOAT(long double);
			else
#endif
			    VA_FLOAT(double);
			used = 'f';
			break;
		    case 'c':
			VA_INTGR(int);
			used = 'c';
			break;
		    case 's':
			VA_POINT(char *);
			if (prec < 0)
			    prec = strlen(pval);
			used = 's';
			break;
		    case 'p':
			VA_POINT(void *);
			used = 'p';
			break;
		    case 'n':
			VA_POINT(int *);
			used = 0;
			break;
		    default:
			CTRACE(tfp, "unknown format character '%c' in %s\n",
			            *fmt, format);
			break;
		    }
		} else if (*fmt == '.') {
		    state = Prec;
		} else if (*fmt == '%') {
		    done = TRUE;
		    used = '%';
		}
	    }
	    fmt_ptr[f] = '\0';

	    if (prec > 0) {
		switch (used) {
		case 'f':
		    if (width < prec + NUM_WIDTH)
			width = prec + NUM_WIDTH;
		case 'i':
		case 'p':
		    if (width < prec + 2)
			width = prec + 2; /* leading sign/space/zero, "0x" */
		case 'c':
		case '%':
		    break;
		default:
		    if (width < prec)
			width = prec;
		}
	    }
	    if (width >= (int)tmp_len) {
		tmp_len = GROW_EXPR(tmp_len + width);
		tmp_ptr = HTAlloc(tmp_ptr, tmp_len);
	    }

	    switch (used) {
	    case 'i':
	    case 'c':
		sprintf(tmp_ptr, fmt_ptr, ival);
		break;
	    case 'f':
		sprintf(tmp_ptr, fmt_ptr, fval);
		break;
	    default:
		sprintf(tmp_ptr, fmt_ptr, pval);
		break;
	    }
	    need = dst_len + strlen(tmp_ptr) + 1;
	    if (need >= have) {
		dst_ptr = HTAlloc(dst_ptr, have = GROW_EXPR(need));
	    }
	    strcpy(dst_ptr + dst_len, tmp_ptr);
	    dst_len += strlen(tmp_ptr);
	} else {
	    if ((dst_len + 2) >= have) {
		dst_ptr = HTAlloc(dst_ptr, (have += GROW_SIZE));
	    }
	    dst_ptr[dst_len++] = *fmt++;
	}
    }

    FREE(tmp_ptr);
    FREE(fmt_ptr);
    dst_ptr[dst_len] = '\0';
    if (pstr)
    	*pstr = dst_ptr;
    return (dst_ptr);
}

/*
 * Replacement for sprintf, allocates buffer on the fly according to what's needed
 * for its arguments.  Unlike sprintf, this always concatenates to the destination
 * buffer.
 */
#if ANSI_VARARGS
PUBLIC char * HTSprintf (char ** pstr, CONST char * fmt, ...)
#else
PUBLIC char * HTSprintf (va_alist)
    va_dcl
#endif
{
    char *result = 0;
    size_t inuse = 0;
    va_list ap;

    LYva_start(ap,fmt);
    {
#if !ANSI_VARARGS
	char **		pstr = va_arg(ap, char **);
	CONST char *	fmt  = va_arg(ap, CONST char *);
#endif
	if (pstr != 0 && *pstr != 0)
	    inuse = strlen(*pstr);
	result = StrAllocVsprintf(pstr, inuse, fmt, &ap);
    }
    va_end(ap);

    return (result);
}

/*
 * Replacement for sprintf, allocates buffer on the fly according to what's
 * needed for its arguments.  Like sprintf, this always resets the destination
 * buffer.
 */
#if ANSI_VARARGS
PUBLIC char * HTSprintf0 (char ** pstr, CONST char * fmt, ...)
#else
PUBLIC char * HTSprintf0 (va_alist)
    va_dcl
#endif
{
    char *result = 0;
    va_list ap;

    LYva_start(ap,fmt);
    {
#if !ANSI_VARARGS
	char **		pstr = va_arg(ap, char **);
	CONST char *	fmt  = va_arg(ap, CONST char *);
#endif
	result = StrAllocVsprintf(pstr, 0, fmt, &ap);
    }
    va_end(ap);

    return (result);
}

/*
 * Returns a quoted or escaped form of the given parameter, suitable for use in
 * a command string.
 */
#if USE_QUOTED_PARAMETER
#define S_QUOTE '\''
#define D_QUOTE '"'
PUBLIC char *HTQuoteParameter ARGS1(
    CONST char *,	parameter)
{
    size_t i;
    size_t last = strlen(parameter);
    size_t n = 0;
    size_t quoted = 0;
    char * result;

    for (i=0; i < last; ++i)
	if (strchr("\\&#$^*?(){}<>\"';`|", parameter[i]) != 0
	 || isspace(parameter[i]))
	    ++quoted;

    result = (char *)malloc(last + 5*quoted + 3);
    if (result == NULL)
	outofmem(__FILE__, "HTQuoteParameter");

    n = 0;
    if (quoted)
	result[n++] = S_QUOTE;
    for (i = 0; i < last; i++) {
	if (parameter[i] == S_QUOTE) {
	    result[n++] = S_QUOTE;
	    result[n++] = D_QUOTE;
	    result[n++] = parameter[i];
	    result[n++] = D_QUOTE;
	    result[n++] = S_QUOTE;
	} else if (parameter[i] == '\\') {
	    result[n++] = parameter[i];
	    result[n++] = parameter[i];
	} else {
	    result[n++] = parameter[i];
	}
    }
    if (quoted)
	result[n++] = S_QUOTE;
    result[n] = '\0';
    return result;
}
#endif

#define HTIsParam(string) ((string[0] == '%' && string[1] == 's'))

/*
 * Returns the number of "%s" tokens in a system command-template.
 */
PUBLIC int HTCountCommandArgs ARGS1(
    CONST char *,	command)
{
    int number = 0;
    while (command[0] != 0) {
	if (HTIsParam(command))
	    number++;
	command++;
    }
    return number;
}

/*
 * Returns a pointer into the given string after the given parameter number
 */
PRIVATE CONST char *HTAfterCommandArg ARGS2(
    CONST char *,	command,
    int,		number)
{
    while (number > 0) {
	if (command[0] != 0) {
	    if (HTIsParam(command)) {
		number--;
		command++;
	    }
	    command++;
	} else {
	    break;
	}
    }
    return command;
}

/*
 * Like HTAddParam, but the parameter may be an environment variable, which we
 * will expand and append.  Do this only for things like the command-verb,
 * where we obtain the parameter from the user's configuration.  Any quoting
 * required for the environment variable has to be done within its value, e.g.,
 *
 *	setenv EDITOR 'xvile -name "No such class"'
 *
 * This is useful only when we quote parameters, of course.
 */
#if USE_QUOTED_PARAMETER
PUBLIC void HTAddXpand ARGS4(
    char **,		result,
    CONST char *,	command,
    int,		number,
    CONST char *,	parameter)
{
    if (number > 0) {
	CONST char *last = HTAfterCommandArg(command, number - 1);
	CONST char *next = last;

	if (number <= 1) {
	    FREE(*result);
	}

	while (next[0] != 0) {
	    if (HTIsParam(next)) {
		if (next != last) {
		    size_t len = (next - last)
		    		+ ((*result != 0) ? strlen(*result) : 0);
		    HTSACat(result, last);
		    (*result)[len] = 0;
		}
		HTSACat(result, parameter);
		CTRACE(tfp, "PARAM-EXP:%s\n", *result);
		return;
	    }
	    next++;
	}
    }
}
#endif /* USE_QUOTED_PARAMETER */

/*
 * Append string-parameter to a system command that we are constructing.  The
 * string is a complete parameter (which is a necessary assumption so we can
 * quote it properly).  We're given the index of the newest parameter we're
 * processing.  Zero indicates none, so a value of '1' indicates that we copy
 * from the beginning of the command string up to the first parameter,
 * substitute the quoted parameter and return the result.
 *
 * Parameters are substituted at "%s" tokens, like printf.  Other printf-style
 * tokens are not substituted; they are passed through without change.
 */
PUBLIC void HTAddParam ARGS4(
    char **,		result,
    CONST char *,	command,
    int,		number,
    CONST char *,	parameter)
{
    if (number > 0) {
	CONST char *last = HTAfterCommandArg(command, number - 1);
	CONST char *next = last;
	char *quoted;

	if (number <= 1) {
	    FREE(*result);
	}
	if (parameter == 0)
	    parameter = "";
	while (next[0] != 0) {
	    if (HTIsParam(next)) {
		if (next != last) {
		    size_t len = (next - last)
		    		+ ((*result != 0) ? strlen(*result) : 0);
		    HTSACat(result, last);
		    (*result)[len] = 0;
		}
#if USE_QUOTED_PARAMETER
		quoted = HTQuoteParameter(parameter);
		HTSACat(result, quoted);
		FREE(quoted);
#else
		HTSACat(result, parameter);
#endif
		CTRACE(tfp, "PARAM-ADD:%s\n", *result);
		return;
	    }
	    next++;
	}
    }
}

/*
 * Append the remaining command-string to a system command (compare with
 * HTAddParam).  Any remaining "%s" tokens are copied as empty strings.
 */
PUBLIC void HTEndParam ARGS3(
    char **,		result,
    CONST char *,	command,
    int,		number)
{
    CONST char *last;
    int count;
    count = HTCountCommandArgs (command);
    if (count < number)
	number = count;
    last = HTAfterCommandArg(command, number);
    if (last[0] != 0) {
	HTSACat(result, last);
    }
    CTRACE(tfp, "PARAM-END:%s\n", *result);
}
