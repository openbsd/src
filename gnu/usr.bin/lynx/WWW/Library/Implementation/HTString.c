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

#ifndef NO_LYNX_TRACE
PUBLIC BOOLEAN WWW_TraceFlag = 0;	/* Global trace flag for ALL W3 code */
PUBLIC int WWW_TraceMask = 0;		/* Global trace flag for ALL W3 code */
#endif

#ifndef VC
#define VC "unknown"
#endif /* !VC */

#ifdef _WINDOWS
CONST char * HTLibraryVersion = "2.14FM"; /* String for help screen etc */
#else
PUBLIC CONST char * HTLibraryVersion = VC; /* String for help screen etc */
#endif

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

#ifdef SH_EX	/* 1997/12/23 (Tue) 16:40:31 */

/*
 * This array is designed for mapping upper and lower case letter
 * together for a case independent comparison.  The mappings are
 * based upon ascii character sequences.
 */
static unsigned char charmap[] = {
	'\000', '\001', '\002', '\003', '\004', '\005', '\006', '\007',
	'\010', '\011', '\012', '\013', '\014', '\015', '\016', '\017',
	'\020', '\021', '\022', '\023', '\024', '\025', '\026', '\027',
	'\030', '\031', '\032', '\033', '\034', '\035', '\036', '\037',
	'\040', '\041', '\042', '\043', '\044', '\045', '\046', '\047',
	'\050', '\051', '\052', '\053', '\054', '\055', '\056', '\057',
	'\060', '\061', '\062', '\063', '\064', '\065', '\066', '\067',
	'\070', '\071', '\072', '\073', '\074', '\075', '\076', '\077',
	'\100', '\141', '\142', '\143', '\144', '\145', '\146', '\147',
	'\150', '\151', '\152', '\153', '\154', '\155', '\156', '\157',
	'\160', '\161', '\162', '\163', '\164', '\165', '\166', '\167',
	'\170', '\171', '\172', '\133', '\134', '\135', '\136', '\137',
	'\140', '\141', '\142', '\143', '\144', '\145', '\146', '\147',
	'\150', '\151', '\152', '\153', '\154', '\155', '\156', '\157',
	'\160', '\161', '\162', '\163', '\164', '\165', '\166', '\167',
	'\170', '\171', '\172', '\173', '\174', '\175', '\176', '\177',
	'\200', '\201', '\202', '\203', '\204', '\205', '\206', '\207',
	'\210', '\211', '\212', '\213', '\214', '\215', '\216', '\217',
	'\220', '\221', '\222', '\223', '\224', '\225', '\226', '\227',
	'\230', '\231', '\232', '\233', '\234', '\235', '\236', '\237',
	'\240', '\241', '\242', '\243', '\244', '\245', '\246', '\247',
	'\250', '\251', '\252', '\253', '\254', '\255', '\256', '\257',
	'\260', '\261', '\262', '\263', '\264', '\265', '\266', '\267',
	'\270', '\271', '\272', '\273', '\274', '\275', '\276', '\277',
	'\340', '\341', '\342', '\343', '\344', '\345', '\346', '\347',
	'\350', '\351', '\352', '\353', '\354', '\355', '\356', '\357',
	'\360', '\361', '\362', '\363', '\364', '\365', '\366', '\327',
	'\370', '\371', '\372', '\373', '\374', '\375', '\376', '\337',
	'\340', '\341', '\342', '\343', '\344', '\345', '\346', '\347',
	'\350', '\351', '\352', '\353', '\354', '\355', '\356', '\357',
	'\360', '\361', '\362', '\363', '\364', '\365', '\366', '\367',
	'\370', '\371', '\372', '\373', '\374', '\375', '\376', '\377',
};

PUBLIC int strcasecomp ARGS2(
	CONST char*,	s1,
	CONST char*,	s2)
{
    register unsigned char *cm = charmap;
    register unsigned char *us1 = (unsigned char *)s1;
    register unsigned char *us2 = (unsigned char *)s2;

    while (cm[*us1] == cm[*us2++])
	if (*us1++ == '\0')
	    return(0);
    return (cm[*us1] - cm[*--us2]);
}

PUBLIC int strncasecomp ARGS3(
	CONST char*,	a,
	CONST char*,	b,
	int,		n)
{
    register unsigned char *cm = charmap;
    register unsigned char *us1 = (unsigned char *)a;
    register unsigned char *us2 = (unsigned char *)b;

    while ((long)(--n) >= 0 && cm[*us1] == cm[*us2++])
	if (*us1++ == '\0')
	    return(0);
    return ((long)n < 0 ? 0 : cm[*us1] - cm[*--us2]);
}

#else	/* SH_EX */

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

#endif	/* SH_EX */
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
	    return (UCH(*p)  - UCH(*q));
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
	    return (UCH(*p) - UCH(*q));
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
	    size_t size = strlen(src) + 1;
	    FREE(*dest);
	    *dest = (char *) malloc(size);
	    if (*dest == NULL)
		outofmem(__FILE__, "HTSACopy");
	    memcpy(*dest, src, size);
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
	    size_t length = strlen(*dest);
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


/* optimized for heavily realloc'd strings, store length inside */

#define EXTRA_TYPE size_t		/* type we use for length */
#define EXTRA_SIZE sizeof(void *)	/* alignment >= sizeof(EXTRA_TYPE) */

PUBLIC void   HTSAFree_extra ARGS1(
	char *,		s)
{
    free(s - EXTRA_SIZE);
}

/* never shrink */
PUBLIC char * HTSACopy_extra ARGS2(
	char **,	dest,
	CONST char *,	src)
{
    if (src != 0) {
	size_t srcsize = strlen(src) + 1;
	EXTRA_TYPE size = 0;

	if (*dest != 0) {
	    size = *(EXTRA_TYPE *)((*dest) - EXTRA_SIZE);
	}
	if (size < srcsize) {
	    FREE_extra(*dest);
	    size = srcsize * 2;   /* x2 step */
	    *dest = (char *) malloc(size + EXTRA_SIZE);
	    if (*dest == NULL)
		outofmem(__FILE__, "HTSACopy_extra");
	    *(EXTRA_TYPE *)(*dest) = size;
	    *dest += EXTRA_SIZE;
	}
	memcpy(*dest, src, srcsize);
    } else {
	Clear_extra(*dest);
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
**		If NULL, default is white space "," ";" or "=".
**		The word can optionally be quoted or enclosed with
**		chars from bracks.
**		Comments surrrounded by '(' ')' are filtered out
**		unless they are specifically reqested by including
**		' ' or '(' in delims or bracks.
**	bracks	lists bracketing chars.  Some are recognized as
**		special, for those give the opening char.
**		If NULL, defaults to <"> and "<" ">".
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
	char *,		found)
{
    char * p = *pstr;
    char * start = NULL;
    BOOL get_blanks, skip_comments;
    BOOL get_comments;
    BOOL get_closing_char_too = FALSE;
    char closer;

    if (isEmpty(pstr)) return NULL;
    if (!delims) delims = " ;,=" ;
    if (!bracks) bracks = "<\"" ;

    get_blanks = (BOOL) (!strchr(delims,' ') && !strchr(bracks,' '));
    get_comments = (BOOL) (strchr(bracks,'(') != NULL);
    skip_comments = (BOOL) (!get_comments && !strchr(delims,'(') && !get_blanks);
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
		    get_closing_char_too = (BOOL) (strchr(bracks,*p) != NULL);
	    }
	} else if (strchr(bracks,*p)) {	       /* quoted or bracketed field */
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
		    get_closing_char_too = (BOOL) (strchr(bracks,*p) != NULL);
	    } else
	    break;			    /* kr95-10-9: needs to stop here */
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
 * If SAVE_TIME_NOT_SPACE is defined, StrAllocVsprintf will hang on to
 * its temporary string buffers instead of allocating and freeing them
 * in each invocation.  They only grow and never shrink, and won't be
 * cleaned up on exit. - kw
 */
#if defined(_REENTRANT) || defined(_THREAD_SAFE) || defined(LY_FIND_LEAKS)
#undef SAVE_TIME_NOT_SPACE
#endif

/*
 * Replacement for sprintf, allocates buffer on the fly according to what's
 * needed for its arguments.  Unlike sprintf, this always concatenates to the
 * destination buffer, so we do not have to provide both flavors.
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

PUBLIC_IF_FIND_LEAKS char * StrAllocVsprintf ARGS4(
	char **,	pstr,
	size_t,		dst_len,
	CONST char *,	fmt,
	va_list *,	ap)
{
#ifdef SAVE_TIME_NOT_SPACE
    static size_t tmp_len = 0;
    static size_t fmt_len = 0;
    static char *tmp_ptr = NULL;
    static char *fmt_ptr = NULL;
#else
    size_t tmp_len = GROW_SIZE;
    char *tmp_ptr = 0;
    char *fmt_ptr;
#endif /* SAVE_TIME_NOT_SPACE */
    size_t have, need;
    char *dst_ptr = *pstr;
    CONST char *format = fmt;

    if (fmt == 0 || *fmt == '\0')
	return 0;

#ifdef USE_VASPRINTF
    if (pstr && !dst_len) {
	if (*pstr)
	    FREE(*pstr);
	if (vasprintf(pstr, fmt, *ap) >= 0) {
	    mark_malloced(*pstr, strlen(*pstr)+1);
	    return(*pstr);
	}
    }
#endif /* USE_VASPRINTF */

    need = strlen(fmt) + 1;
#ifdef SAVE_TIME_NOT_SPACE
    if (!fmt_ptr || fmt_len < need*NUM_WIDTH) {
	fmt_ptr = HTAlloc(fmt_ptr, fmt_len = need*NUM_WIDTH);
    }
    if (!tmp_ptr || tmp_len < GROW_SIZE) {
	tmp_ptr = HTAlloc(tmp_ptr, tmp_len = GROW_SIZE);
    }
#else
    if ((fmt_ptr = malloc(need*NUM_WIDTH)) == 0
     || (tmp_ptr = malloc(tmp_len)) == 0) {
	outofmem(__FILE__, "StrAllocVsprintf");
    }
#endif /* SAVE_TIME_NOT_SPACE */

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

		if (isdigit(UCH(*fmt))) {
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
		} else if (isalpha(UCH(*fmt))) {
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
			CTRACE((tfp, "unknown format character '%c' in %s\n",
				    *fmt, format));
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
		    /* FALLTHRU */
		case 'i':
		    /* FALLTHRU */
		case 'p':
		    if (width < prec + 2)
			width = prec + 2; /* leading sign/space/zero, "0x" */
		    break;
		case 'c':
		    break;
		case '%':
		    break;
		default:
		    if (width < prec)
			width = prec;
		    break;
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

#ifndef SAVE_TIME_NOT_SPACE
    FREE(tmp_ptr);
    FREE(fmt_ptr);
#endif
    dst_ptr[dst_len] = '\0';
    if (pstr)
	*pstr = dst_ptr;
    return (dst_ptr);
}
#undef SAVE_TIME_NOT_SPACE

/*
 * Replacement for sprintf, allocates buffer on the fly according to what's
 * needed for its arguments.  Unlike sprintf, this always concatenates to the
 * destination buffer.
 */
/* Note: if making changes, also check the memory tracking version
 * LYLeakHTSprintf in LYLeaks.c. - kw */
#ifdef HTSprintf		/* if hidden by LYLeaks stuff */
#undef HTSprintf
#endif
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
/* Note: if making changes, also check the memory tracking version
 * LYLeakHTSprintf0 in LYLeaks.c. - kw */
#ifdef HTSprintf0		/* if hidden by LYLeaks stuff */
#undef HTSprintf0
#endif
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
#ifdef USE_VASPRINTF
	if (pstr) {
	    if (*pstr)
		FREE(*pstr);
	    if (vasprintf(pstr, fmt, ap) >= 0) /* else call outofmem?? */
		mark_malloced(*pstr, strlen(*pstr)+1);
	    result = *pstr;
	} else
#endif /* USE_VASPRINTF */
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
	 || isspace(UCH(parameter[i])))
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
	} else {
	    /* Note:  No special handling of other characters, including
	       backslash, since we are constructing a single-quoted string!
	       Backslash has no special escape meaning within those for sh
	       and compatible shells, so trying to escape a backslash by
	       doubling it is unnecessary and would be interpreted by the
	       shell as an additional data character. - kw 2000-05-02
	       */
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
		CTRACE((tfp, "PARAM-EXP:%s\n", *result));
		return;
	    }
	    next++;
	}
    }
}
#endif /* USE_QUOTED_PARAMETER */

/*
 * Append string to a system command that we are constructing, without quoting. 
 * We're given the index of the newest parameter we're processing.  Zero
 * indicates none, so a value of '1' indicates that we copy from the beginning
 * of the command string up to the first parameter, substitute the quoted
 * parameter and return the result.
 *
 * Parameters are substituted at "%s" tokens, like printf.  Other printf-style
 * tokens are not substituted; they are passed through without change.
 */
PUBLIC void HTAddToCmd ARGS4(
    char **,		result,
    CONST char *,	command,
    int,		number,
    CONST char *,	string)
{
    if (number > 0) {
	CONST char *last = HTAfterCommandArg(command, number - 1);
	CONST char *next = last;

	if (number <= 1) {
	    FREE(*result);
	}
	if (string == 0)
	    string = "";
	while (next[0] != 0) {
	    if (HTIsParam(next)) {
		if (next != last) {
		    size_t len = (next - last)
				+ ((*result != 0) ? strlen(*result) : 0);
		    HTSACat(result, last);
		    (*result)[len] = 0;
		}
		HTSACat(result, string);
		CTRACE((tfp, "PARAM-ADD:%s\n", *result));
		return;
	    }
	    next++;
	}
    }
}

/*
 * Append string-parameter to a system command that we are constructing.  The
 * string is a complete parameter (which is a necessary assumption so we can
 * quote it properly).
 */
PUBLIC void HTAddParam ARGS4(
    char **,		result,
    CONST char *,	command,
    int,		number,
    CONST char *,	parameter)
{
    if (number > 0) {
#if USE_QUOTED_PARAMETER
	char *quoted = HTQuoteParameter(parameter);
	HTAddToCmd(result, command, number, quoted);
	FREE(quoted);
#else
	HTAddToCmd(result, command, number, parameter);
#endif
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
    CTRACE((tfp, "PARAM-END:%s\n", *result));
}


/*	Binary-strings (may have embedded nulls).
 *	Some modules (HTGopher) assume there is a null on the end, anyway.
 */

/*	Allocate a new bstring, and return it.
*/
PUBLIC void HTSABCopy ARGS3(
	bstring**,	dest,
	CONST char *,	src,
	int,		len)
{
    bstring *t;
    unsigned need = len + 1;

    CTRACE2(TRACE_BSTRING, (tfp, "HTSABCopy(%p, %p, %d)\n", dest, src, len));
    HTSABFree(dest);
    if (src) {
	if (TRACE_BSTRING) {
	    CTRACE((tfp, "===    %4d:", len));
	    trace_bstring2(src, len);
	    CTRACE((tfp, "\n"));
	}
	if ((t = (bstring*) malloc(sizeof(bstring))) == NULL)
	    outofmem(__FILE__, "HTSABCopy");
	if ((t->str = (char *) malloc (need)) == NULL)
	    outofmem(__FILE__, "HTSABCopy");
	memcpy (t->str, src, len);
	t->len = len;
	t->str[t->len] = '\0';
	*dest = t;
    }
    if (TRACE_BSTRING) {
	CTRACE((tfp, "=>     %4d:", BStrLen(*dest)));
	trace_bstring(*dest);
	CTRACE((tfp, "\n"));
    }
}

/*
 * Initialize with a null-terminated string (discards the null).
 */
PUBLIC void HTSABCopy0 ARGS2(
	bstring**,	dest,
	CONST char *,	src)
{
    HTSABCopy(dest, src, strlen(src));
}

/*
 * Append a block of memory to a bstring.
 */
PUBLIC void HTSABCat ARGS3(
	bstring **,	dest,
	CONST char *,	src,
	int,		len)
{
    bstring *t = *dest;

    CTRACE2(TRACE_BSTRING, (tfp, "HTSABCat(%p, %p, %d)\n", dest, src, len));
    if (src) {
	unsigned need = len + 1;

	if (TRACE_BSTRING) {
	    CTRACE((tfp, "===    %4d:", len));
	    trace_bstring2(src, len);
	    CTRACE((tfp, "\n"));
	}
	if (t) {
	    unsigned length = t->len + need;
	    if ((t->str = (char *)realloc(t->str, length)) == NULL)
		outofmem(__FILE__, "HTSACat");
	} else {
	    if ((t = typecalloc(bstring)) == NULL)
		outofmem(__FILE__, "HTSACat");
	    t->str = (char *)malloc(need);
	}
	if (t->str == NULL)
	    outofmem(__FILE__, "HTSACat");
	memcpy (t->str + t->len, src, len);
	t->len += len;
	t->str[t->len] = '\0';
	*dest = t;
    }
    if (TRACE_BSTRING) {
	CTRACE((tfp, "=>     %4d:", BStrLen(*dest)));
	trace_bstring(*dest);
	CTRACE((tfp, "\n"));
    }
}

/*
 * Append a null-terminated string (discards the null).
 */
PUBLIC void HTSABCat0 ARGS2(
	bstring**,	dest,
	CONST char *,	src)
{
    HTSABCat(dest, src, strlen(src));
}

/*
 * Compare two bstring's for equality
 */
PUBLIC BOOL HTSABEql   ARGS2(
	bstring *,	a,
	bstring *,	b)
{
    unsigned len_a = (a != 0) ? a->len : 0;
    unsigned len_b = (b != 0) ? b->len : 0;

    if (len_a == len_b) {
	if (len_a == 0
	 || memcmp(a->str, b->str, a->len) == 0)
	    return TRUE;
    }
    return FALSE;
}

/*
 * Deallocate a bstring.
 */
PUBLIC void HTSABFree ARGS1(
	bstring **,	ptr)
{
    if (*ptr != NULL) {
	FREE((*ptr)->str);
	FREE(*ptr);
	*ptr = NULL;
    }
}

/*
 * Use this function to perform formatted sprintf's onto the end of a bstring.
 * The bstring may contain embedded nulls; the formatted portions must not.
 */
#ifdef ANSI_VARARGS
PUBLIC bstring * HTBprintf (bstring ** pstr, CONST char * fmt, ...)
#else
PUBLIC bstring * HTBprintf (va_alist)
    va_dcl
#endif
{
    bstring *result = 0;
    char *temp = 0;
    va_list ap;

    LYva_start(ap,fmt);
    {
#if !ANSI_VARARGS
	bstring **	pstr = va_arg(ap, char **);
	CONST char *	fmt  = va_arg(ap, CONST char *);
#endif
	temp = StrAllocVsprintf(&temp, 0, fmt, &ap);
	if (!isEmpty(temp)) {
	    HTSABCat (pstr, temp, strlen(temp));
	}
	FREE(temp);
	result = *pstr;
    }
    va_end(ap);

    return (result);
}

/*
 * Write binary-data to the logfile, making it safe for most editors to view.
 * That is most, since we do not restrict line-length.  Nulls and other
 * non-printing characters are addressed.
 */
PUBLIC void trace_bstring2 ARGS2(
	CONST char *,	text,
	int,		size)
{
    int n;

    if (text != 0) {
	for (n = 0; n < size; ++n) {
	    int ch = UCH(text[n]);
	    switch (ch) {
	    case '\\':
		fputs("\\\\", tfp);
		break;
	    case '\r':
		fputs("\\r", tfp);
		break;
	    case '\t':
		fputs("\\t", tfp);
		break;
	    case '\f':
		fputs("\\f", tfp);
		break;
	    default:
		if (isprint(ch) || isspace(ch)) {
		    fputc(ch, tfp);
		} else {
		    fprintf(tfp, "\\%03o", ch);
		}
		break;
	    }
	}
    }
}

PUBLIC void trace_bstring ARGS1(
	bstring *,	data)
{
    trace_bstring2(BStrData(data), BStrLen(data));
}
