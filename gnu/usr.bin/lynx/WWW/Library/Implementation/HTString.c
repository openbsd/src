/*
 * $LynxId: HTString.c,v 1.72 2013/11/28 11:14:49 tom Exp $
 *
 *	Case-independent string comparison		HTString.c
 *
 *	Original version came with listserv implementation.
 *	Version TBL Oct 91 replaces one which modified the strings.
 *	02-Dec-91 (JFG) Added stralloccopy and stralloccat
 *	23 Jan 92 (TBL) Changed strallocc* to 8 char HTSAC* for VM and suchlike
 *	 6 Oct 92 (TBL) Moved WWW_TraceFlag in here to be in library
 *	15 Nov 98 (TD)  Added HTSprintf.
 */

#include <HTUtils.h>

#include <LYLeaks.h>
#include <LYUtils.h>
#include <LYStrings.h>

#ifdef USE_IGNORE_RC
int ignore_unused;
#endif

#ifndef NO_LYNX_TRACE
BOOLEAN WWW_TraceFlag = 0;	/* Global trace flag for ALL W3 code */
int WWW_TraceMask = 0;		/* Global trace flag for ALL W3 code */
#endif

#ifdef _WINDOWS
#undef VC
#define VC "2.14FM"
#endif

#ifndef VC
#define VC "2.14"
#endif /* !VC */

const char *HTLibraryVersion = VC;	/* String for help screen etc */

/*
 *     strcasecomp8 is a variant of strcasecomp (below)
 *     ------------		    -----------
 *     but uses 8bit upper/lower case information
 *     from the current display charset.
 *     It returns 0 if exact match.
 */
int strcasecomp8(const char *a,
		 const char *b)
{
    const char *p = a;
    const char *q = b;

    for (; *p && *q; p++, q++) {
	int diff = UPPER8(*p, *q);

	if (diff)
	    return diff;
    }
    if (*p)
	return 1;		/* p was longer than q */
    if (*q)
	return -1;		/* p was shorter than q */
    return 0;			/* Exact match */
}

/*
 *     strncasecomp8 is a variant of strncasecomp (below)
 *     -------------		     ------------
 *     but uses 8bit upper/lower case information
 *     from the current display charset.
 *     It returns 0 if exact match.
 */
int strncasecomp8(const char *a,
		  const char *b,
		  int n)
{
    const char *p = a;
    const char *q = b;

    for (;; p++, q++) {
	int diff;

	if (p == (a + n))
	    return 0;		/*   Match up to n characters */
	if (!(*p && *q))
	    return (*p - *q);
	diff = UPPER8(*p, *q);
	if (diff)
	    return diff;
    }
    /*NOTREACHED */
}

#ifndef VM			/* VM has these already it seems */
/*	Strings of any length
 *	---------------------
 */
int strcasecomp(const char *a,
		const char *b)
{
    const char *p = a;
    const char *q = b;

    for (; *p && *q; p++, q++) {
	int diff = TOLOWER(*p) - TOLOWER(*q);

	if (diff)
	    return diff;
    }
    if (*p)
	return 1;		/* p was longer than q */
    if (*q)
	return -1;		/* p was shorter than q */
    return 0;			/* Exact match */
}

/*	With count limit
 *	----------------
 */
int strncasecomp(const char *a,
		 const char *b,
		 int n)
{
    const char *p = a;
    const char *q = b;

    for (;; p++, q++) {
	int diff;

	if (p == (a + n))
	    return 0;		/*   Match up to n characters */
	if (!(*p && *q))
	    return (*p - *q);
	diff = TOLOWER(*p) - TOLOWER(*q);
	if (diff)
	    return diff;
    }
    /*NOTREACHED */
}
#endif /* VM */

#define end_component(p) (*(p) == '.' || *(p) == '\0')

#ifdef DEBUG_ASTERISK
#define SHOW_ASTERISK CTRACE
#else
#define SHOW_ASTERISK(p)	/* nothing */
#endif

#define SHOW_ASTERISK_NUM(a,b,c)  \
	SHOW_ASTERISK((tfp, "test @%d, '%s' vs '%s' (%d)\n", __LINE__, a,b,c))

#define SHOW_ASTERISK_TXT(a,b,c)  \
	SHOW_ASTERISK((tfp, "test @%d, '%s' vs '%s' %s\n", __LINE__, a,b,c))

/*
 * Compare names as described in RFC 2818: ignore case, allow wildcards. 
 * Return zero on a match, nonzero on mismatch -TD
 *
 * From RFC 2818:
 * Names may contain the wildcard character * which is considered to match any
 * single domain name component or component fragment.  E.g., *.a.com matches
 * foo.a.com but not bar.foo.a.com.  f*.com matches foo.com but not bar.com.
 */
int strcasecomp_asterisk(const char *a, const char *b)
{
    const char *p;
    int result = 0;
    int done = FALSE;

    while (!result && !done) {
	SHOW_ASTERISK_TXT(a, b, "main");
	if (*a == '*') {
	    p = b;
	    for (;;) {
		SHOW_ASTERISK_TXT(a, p, "loop");
		if (end_component(p)) {
		    if (end_component(a + 1)) {
			b = p - 1;
			result = 0;
		    } else {
			result = 1;
		    }
		    break;
		} else if (strcasecomp_asterisk(a + 1, p)) {
		    ++p;
		} else {
		    b = p - 1;
		    result = 0;	/* found a match starting at 'p' */
		    done = TRUE;
		    break;
		}
	    }
	    SHOW_ASTERISK_NUM(a, b, result);
	} else if (*b == '*') {
	    result = strcasecomp_asterisk(b, a);
	    SHOW_ASTERISK_NUM(a, b, result);
	    done = (result == 0);
	} else if (*a == '\0' || *b == '\0') {
	    result = (*a != *b);
	    SHOW_ASTERISK_NUM(a, b, result);
	    break;
	} else if (TOLOWER(UCH(*a)) != TOLOWER(UCH(*b))) {
	    result = 1;
	    SHOW_ASTERISK_NUM(a, b, result);
	    break;
	}
	++a;
	++b;
    }
    return result;
}

#ifdef DEBUG_ASTERISK
void mismatch_asterisk(void)
{
    /* *INDENT-OFF* */
    static struct {
	const char *a;
	const char *b;
	int	    code;
    } table[] = {
	{ "foo.bar",	 "*.*",	      0 },
	{ "foo.bar",	 "*.b*",      0 },
	{ "foo.bar",	 "*.ba*",     0 },
	{ "foo.bar",	 "*.bar*",    0 },
	{ "foo.bar",	 "*.*bar*",   0 },
	{ "foo.bar",	 "*.*.",      1 },
	{ "foo.bar",	 "fo*.b*",    0 },
	{ "*oo.bar",	 "fo*.b*",    0 },
	{ "*oo.bar.com", "fo*.b*",    1 },
	{ "*oo.bar.com", "fo*.b*m",   1 },
	{ "*oo.bar.com", "fo*.b*.c*", 0 },
    };
    /* *INDENT-ON* */

    unsigned n;
    int code;

    CTRACE((tfp, "mismatch_asterisk testing\n"));
    for (n = 0; n < TABLESIZE(table); ++n) {
	CTRACE((tfp, "-------%d\n", n));
	code = strcasecomp_asterisk(table[n].a, table[n].b);
	if (code != table[n].code) {
	    CTRACE((tfp, "mismatch_asterisk '%s' '%s' got %d, want %d\n",
		    table[n].a, table[n].b, code, table[n].code));
	}
    }
}
#endif

#ifdef NOT_ASCII

/*	Case-insensitive with ASCII collating sequence
 *	----------------
 */
int AS_casecomp(const char *p,
		const char *q)
{
    int diff;

    for (;; p++, q++) {
	if (!(*p && *q))
	    return (UCH(*p) - UCH(*q));
	diff = TOASCII(TOLOWER(*p))
	    - TOASCII(TOLOWER(*q));
	if (diff)
	    return diff;
    }
    /*NOTREACHED */
}

/*	With count limit and ASCII collating sequence
 *	----------------
 *	AS_cmp uses n == -1 to compare indefinite length.
 */
int AS_ncmp(const char *p,
	    const char *q,
	    unsigned int n)
{
    const char *a = p;
    int diff;

    for (; (unsigned) (p - a) < n; p++, q++) {
	if (!(*p && *q))
	    return (UCH(*p) - UCH(*q));
	diff = TOASCII(*p)
	    - TOASCII(*q);
	if (diff)
	    return diff;
    }
    return 0;			/*   Match up to n characters */
}
#endif /* NOT_ASCII */

/*	Allocate a new copy of a string, and returns it
*/
char *HTSACopy(char **dest,
	       const char *src)
{
    if (src != 0) {
	if (src != *dest) {
	    size_t size = strlen(src) + 1;

	    FREE(*dest);
	    *dest = (char *) malloc(size);
	    if (*dest == NULL)
		outofmem(__FILE__, "HTSACopy");
	    assert(*dest != NULL);
	    MemCpy(*dest, src, size);
	}
    } else {
	FREE(*dest);
    }
    return *dest;
}

/*	String Allocate and Concatenate
*/
char *HTSACat(char **dest,
	      const char *src)
{
    if (src && *src && (src != *dest)) {
	if (*dest) {
	    size_t length = strlen(*dest);

	    *dest = (char *) realloc(*dest, length + strlen(src) + 1);
	    if (*dest == NULL)
		outofmem(__FILE__, "HTSACat");
	    assert(*dest != NULL);
	    strcpy(*dest + length, src);
	} else {
	    *dest = (char *) malloc(strlen(src) + 1);
	    if (*dest == NULL)
		outofmem(__FILE__, "HTSACat");
	    assert(*dest != NULL);
	    strcpy(*dest, src);
	}
    }
    return *dest;
}

/* optimized for heavily realloc'd strings, store length inside */

#define EXTRA_TYPE size_t	/* type we use for length */
#define EXTRA_SIZE sizeof(void *)	/* alignment >= sizeof(EXTRA_TYPE) */

void HTSAFree_extra(char *s)
{
    free(s - EXTRA_SIZE);
}

/* never shrink */
char *HTSACopy_extra(char **dest,
		     const char *src)
{
    if (src != 0) {
	size_t srcsize = strlen(src) + 1;
	EXTRA_TYPE size = 0;

	if (*dest != 0) {
	    size = *(EXTRA_TYPE *) (void *) ((*dest) - EXTRA_SIZE);
	}
	if ((*dest == 0) || (size < srcsize)) {
	    FREE_extra(*dest);
	    size = srcsize * 2;	/* x2 step */
	    *dest = (char *) malloc(size + EXTRA_SIZE);
	    if (*dest == NULL)
		outofmem(__FILE__, "HTSACopy_extra");
	    assert(*dest != NULL);
	    *(EXTRA_TYPE *) (void *) (*dest) = size;
	    *dest += EXTRA_SIZE;
	}
	MemCpy(*dest, src, srcsize);
    } else {
	Clear_extra(*dest);
    }
    return *dest;
}

/*	Find next Field
 *	---------------
 *
 * On entry,
 *	*pstr	points to a string containig white space separated
 *		field, optionlly quoted.
 *
 * On exit,
 *	*pstr	has been moved to the first delimiter past the
 *		field
 *		THE STRING HAS BEEN MUTILATED by a 0 terminator
 *
 *	returns a pointer to the first field
 */
char *HTNextField(char **pstr)
{
    char *p = *pstr;
    char *start = NULL;		/* start of field */

    if (p != NULL) {
	while (*p && WHITE(*p))
	    p++;		/* Strip white space */
	if (!*p) {
	    *pstr = p;
	} else {
	    if (*p == '"') {	/* quoted field */
		p++;
		start = p;
		for (; *p && *p != '"'; p++) {
		    if (*p == '\\' && p[1])
			p++;	/* Skip escaped chars */
		}
	    } else {
		start = p;
		while (*p && !WHITE(*p))
		    p++;	/* Skip first field */
	    }
	    if (*p)
		*p++ = '\0';
	    *pstr = p;
	}
    }
    return start;
}

/*	Find next Token
 *	---------------
 *	Finds the next token in a string
 *	On entry,
 *	*pstr	points to a string to be parsed.
 *	delims	lists characters to be recognized as delimiters.
 *		If NULL, default is white space "," ";" or "=".
 *		The word can optionally be quoted or enclosed with
 *		chars from bracks.
 *		Comments surrrounded by '(' ')' are filtered out
 *		unless they are specifically reqested by including
 *		' ' or '(' in delims or bracks.
 *	bracks	lists bracketing chars.  Some are recognized as
 *		special, for those give the opening char.
 *		If NULL, defaults to <"> and "<" ">".
 *	found	points to location to fill with the ending delimiter
 *		found, or is NULL.
 *
 *	On exit,
 *	*pstr	has been moved to the first delimiter past the
 *		field
 *		THE STRING HAS BEEN MUTILATED by a 0 terminator
 *	found	points to the delimiter found unless it was NULL.
 *	Returns a pointer to the first word or NULL on error
 */
char *HTNextTok(char **pstr,
		const char *delims,
		const char *bracks,
		char *found)
{
    char *p = *pstr;
    char *start = NULL;
    BOOL get_blanks, skip_comments;
    BOOL get_comments;
    BOOL get_closing_char_too = FALSE;
    char closer;

    if (isEmpty(pstr))
	return NULL;
    if (!delims)
	delims = " ;,=";
    if (!bracks)
	bracks = "<\"";

    get_blanks = (BOOL) (!StrChr(delims, ' ') && !StrChr(bracks, ' '));
    get_comments = (BOOL) (StrChr(bracks, '(') != NULL);
    skip_comments = (BOOL) (!get_comments && !StrChr(delims, '(') && !get_blanks);
#define skipWHITE(c) (!get_blanks && WHITE(c))

    while (*p && skipWHITE(*p))
	p++;			/* Strip white space */
    if (!*p) {
	*pstr = p;
	if (found)
	    *found = '\0';
	return NULL;		/* No first field */
    }
    while (1) {
	/* Strip white space and other delimiters */
	while (*p && (skipWHITE(*p) || StrChr(delims, *p)))
	    p++;
	if (!*p) {
	    *pstr = p;
	    if (found)
		*found = *(p - 1);
	    return NULL;	/* No field */
	}

	if (*p == '(' && (skip_comments || get_comments)) {	/* Comment */
	    int comment_level = 0;

	    if (get_comments && !start)
		start = p + 1;
	    for (; *p && (*p != ')' || --comment_level > 0); p++) {
		if (*p == '(')
		    comment_level++;
		else if (*p == '"') {	/* quoted field within Comment */
		    for (p++; *p && *p != '"'; p++)
			if (*p == '\\' && *(p + 1))
			    p++;	/* Skip escaped chars */
		    if (!*p)
			break;	/* (invalid) end of string found, leave */
		}
		if (*p == '\\' && *(p + 1))
		    p++;	/* Skip escaped chars */
	    }
	    if (get_comments)
		break;
	    if (*p)
		p++;
	    if (get_closing_char_too) {
		if (!*p || (!StrChr(bracks, *p) && StrChr(delims, *p))) {
		    break;
		} else
		    get_closing_char_too = (BOOL) (StrChr(bracks, *p) != NULL);
	    }
	} else if (StrChr(bracks, *p)) {	/* quoted or bracketed field */
	    switch (*p) {
	    case '<':
		closer = '>';
		break;
	    case '[':
		closer = ']';
		break;
	    case '{':
		closer = '}';
		break;
	    case ':':
		closer = ';';
		break;
	    default:
		closer = *p;
	    }
	    if (!start)
		start = ++p;
	    for (; *p && *p != closer; p++)
		if (*p == '\\' && *(p + 1))
		    p++;	/* Skip escaped chars */
	    if (get_closing_char_too) {
		p++;
		if (!*p || (!StrChr(bracks, *p) && StrChr(delims, *p))) {
		    break;
		} else
		    get_closing_char_too = (BOOL) (StrChr(bracks, *p) != NULL);
	    } else
		break;		/* kr95-10-9: needs to stop here */
	} else {		/* Spool field */
	    if (!start)
		start = p;
	    while (*p && !skipWHITE(*p) && !StrChr(bracks, *p) &&
		   !StrChr(delims, *p))
		p++;
	    if (*p && StrChr(bracks, *p)) {
		get_closing_char_too = TRUE;
	    } else {
		if (*p == '(' && skip_comments) {
		    *pstr = p;
		    HTNextTok(pstr, NULL, "(", found);	/*      Advance pstr */
		    *p = '\0';
		    if (*pstr && **pstr)
			(*pstr)++;
		    return start;
		}
		break;		/* Got it */
	    }
	}
    }
    if (found)
	*found = *p;

    if (*p)
	*p++ = '\0';
    *pstr = p;
    return start;
}

static char *HTAlloc(char *ptr, size_t length)
{
    if (ptr != 0)
	ptr = (char *) realloc(ptr, length);
    else
	ptr = (char *) malloc(length);
    if (ptr == 0)
	outofmem(__FILE__, "HTAlloc");
    assert(ptr != NULL);
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
typedef enum {
    Flags,
    Width,
    Prec,
    Type,
    Format
} PRINTF;

#define VA_INTGR(type) ival = (int)    va_arg((*ap), type)
#define VA_FLOAT(type) fval = (double) va_arg((*ap), type)
#define VA_POINT(type) pval = (char *) va_arg((*ap), type)

#define NUM_WIDTH 10		/* allow for width substituted for "*" in "%*s" */
		/* also number of chars assumed to be needed in addition
		   to a given precision in floating point formats */

#define GROW_EXPR(n) (((n) * 3) / 2)
#define GROW_SIZE 256

PUBLIC_IF_FIND_LEAKS char *StrAllocVsprintf(char **pstr,
					    size_t dst_len,
					    const char *fmt,
					    va_list * ap)
{
#ifdef HAVE_VASPRINTF
    /*
     * Use vasprintf() if we have it, since it is simplest.
     */
    char *result = 0;
    char *temp = 0;

    /* discard old destination if no length was given */
    if (pstr && !dst_len) {
	if (*pstr)
	    FREE(*pstr);
    }

    if (vasprintf(&temp, fmt, *ap) >= 0) {
	if (dst_len != 0) {
	    size_t src_len = strlen(temp);
	    size_t new_len = dst_len + src_len + 1;

	    result = HTAlloc(pstr ? *pstr : 0, new_len);
	    if (result != 0) {
		strcpy(result + dst_len, temp);
	    }
	    (free) (temp);
	} else {
	    result = temp;
	    mark_malloced(temp, strlen(temp));
	}
    }

    if (pstr != 0)
	*pstr = result;

    return result;
#else /* !HAVE_VASPRINTF */
    /*
     * If vasprintf() is not available, this works - but does not implement
     * the POSIX '$' formatting character which may be used in some of the
     * ".po" files.
     */
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
    const char *format = fmt;

    if (isEmpty(fmt))
	return 0;

    need = strlen(fmt) + 1;
#ifdef SAVE_TIME_NOT_SPACE
    if (!fmt_ptr || fmt_len < need * NUM_WIDTH) {
	fmt_ptr = HTAlloc(fmt_ptr, fmt_len = need * NUM_WIDTH);
    }
    if (!tmp_ptr || tmp_len < GROW_SIZE) {
	tmp_ptr = HTAlloc(tmp_ptr, tmp_len = GROW_SIZE);
    }
#else
    if ((fmt_ptr = malloc(need * NUM_WIDTH)) == 0
	|| (tmp_ptr = malloc(tmp_len)) == 0) {
	outofmem(__FILE__, "StrAllocVsprintf");
	assert(fmt_ptr != NULL);
	assert(tmp_ptr != NULL);
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
	    char *pval = dummy;	/* avoid const-cast */
	    double fval = 0.0;
	    int done = FALSE;
	    int ival = 0;
	    int prec = -1;
	    int type = 0;
	    int used = 0;
	    int width = -1;
	    size_t f = 0;

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
		    case 'Z':	/* FALLTHRU */
		    case 'h':	/* FALLTHRU */
		    case 'l':	/* FALLTHRU */
		    case 'L':	/* FALLTHRU */
			done = FALSE;
			type = *fmt;
			break;
		    case 'o':	/* FALLTHRU */
		    case 'i':	/* FALLTHRU */
		    case 'd':	/* FALLTHRU */
		    case 'u':	/* FALLTHRU */
		    case 'x':	/* FALLTHRU */
		    case 'X':	/* FALLTHRU */
			if (type == 'l')
			    VA_INTGR(long);

			else if (type == 'Z')
			    VA_INTGR(size_t);

			else
			    VA_INTGR(int);

			used = 'i';
			break;
		    case 'f':	/* FALLTHRU */
		    case 'e':	/* FALLTHRU */
		    case 'E':	/* FALLTHRU */
		    case 'g':	/* FALLTHRU */
		    case 'G':	/* FALLTHRU */
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
			width = prec + 2;	/* leading sign/space/zero, "0x" */
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
	    if (width >= (int) tmp_len) {
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
#endif /* HAVE_VASPRINTF */
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
char *HTSprintf(char **pstr, const char *fmt,...)
{
    char *result = 0;
    size_t inuse = 0;
    va_list ap;

    LYva_start(ap, fmt);
    {
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
char *HTSprintf0(char **pstr, const char *fmt,...)
{
    char *result = 0;
    va_list ap;

    LYva_start(ap, fmt);
    {
	result = StrAllocVsprintf(pstr, (size_t) 0, fmt, &ap);
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
char *HTQuoteParameter(const char *parameter)
{
    size_t i;
    size_t last;
    size_t n = 0;
    size_t quoted = 0;
    char *result;

    if (parameter == 0)
	parameter = "";

    last = strlen(parameter);
    for (i = 0; i < last; ++i)
	if (StrChr("\\&#$^*?(){}<>\"';`|", parameter[i]) != 0
	    || isspace(UCH(parameter[i])))
	    ++quoted;

    result = (char *) malloc(last + 5 * quoted + 3);
    if (result == NULL)
	outofmem(__FILE__, "HTQuoteParameter");

    assert(result != NULL);

    n = 0;
#if (USE_QUOTED_PARAMETER == 1)
    /*
     * Only double-quotes are used in Win32/DOS -TD
     */
    if (quoted)
	result[n++] = D_QUOTE;
    for (i = 0; i < last; i++) {
	result[n++] = parameter[i];
    }
    if (quoted)
	result[n++] = D_QUOTE;
#else
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
#endif
    result[n] = '\0';
    return result;
}
#endif

#define HTIsParam(string) ((string[0] == '%' && string[1] == 's'))

/*
 * Returns the number of "%s" tokens in a system command-template.
 */
int HTCountCommandArgs(const char *command)
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
static const char *HTAfterCommandArg(const char *command,
				     int number)
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
void HTAddXpand(char **result,
		const char *command,
		int number,
		const char *parameter)
{
    if (number > 0) {
	const char *last = HTAfterCommandArg(command, number - 1);
	const char *next = last;

	if (number <= 1) {
	    FREE(*result);
	}

	while (next[0] != 0) {
	    if (HTIsParam(next)) {
		if (next != last) {
		    size_t len = ((size_t) (next - last)
				  + ((*result != 0)
				     ? strlen(*result)
				     : 0));

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
void HTAddToCmd(char **result,
		const char *command,
		int number,
		const char *string)
{
    if (number > 0) {
	const char *last = HTAfterCommandArg(command, number - 1);
	const char *next = last;

	if (number <= 1) {
	    FREE(*result);
	}
	if (string == 0)
	    string = "";
	while (next[0] != 0) {
	    if (HTIsParam(next)) {
		if (next != last) {
		    size_t len = ((size_t) (next - last)
				  + ((*result != 0)
				     ? strlen(*result)
				     : 0));

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
void HTAddParam(char **result,
		const char *command,
		int number,
		const char *parameter)
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
void HTEndParam(char **result,
		const char *command,
		int number)
{
    const char *last;
    int count;

    count = HTCountCommandArgs(command);
    if (count < number)
	number = count;
    last = HTAfterCommandArg(command, number);
    if (last[0] != 0) {
	HTSACat(result, last);
    }
    CTRACE((tfp, "PARAM-END:%s\n", *result));
}

/* Binary-strings (may have embedded nulls).  Some modules (HTGopher) assume
 * there is a null on the end, anyway.
 */

/* (Re)allocate a bstring, e.g., to increase its buffer size for ad hoc
 * operations.
 */
void HTSABAlloc(bstring **dest, int len)
{
    if (*dest == 0) {
	*dest = typecalloc(bstring);

	if (*dest == 0)
	    outofmem(__FILE__, "HTSABAlloc");
    }

    if ((*dest)->len != len) {
	(*dest)->str = typeRealloc(char, (*dest)->str, len);

	if ((*dest)->str == 0)
	    outofmem(__FILE__, "HTSABAlloc");

	(*dest)->len = len;
    }
}

/* Allocate a new bstring, and return it.
*/
void HTSABCopy(bstring **dest, const char *src,
	       int len)
{
    bstring *t;
    unsigned need = (unsigned) (len + 1);

    CTRACE2(TRACE_BSTRING,
	    (tfp, "HTSABCopy(%p, %p, %d)\n",
	     (void *) dest, (const void *) src, len));
    HTSABFree(dest);
    if (src) {
	if (TRACE_BSTRING) {
	    CTRACE((tfp, "===    %4d:", len));
	    trace_bstring2(src, len);
	    CTRACE((tfp, "\n"));
	}
	if ((t = (bstring *) malloc(sizeof(bstring))) == NULL)
	      outofmem(__FILE__, "HTSABCopy");

	assert(t != NULL);

	if ((t->str = typeMallocn(char, need)) == NULL)
	      outofmem(__FILE__, "HTSABCopy");

	assert(t->str != NULL);

	MemCpy(t->str, src, len);
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
void HTSABCopy0(bstring **dest, const char *src)
{
    HTSABCopy(dest, src, (int) strlen(src));
}

/*
 * Append a block of memory to a bstring.
 */
void HTSABCat(bstring **dest, const char *src,
	      int len)
{
    bstring *t = *dest;

    CTRACE2(TRACE_BSTRING,
	    (tfp, "HTSABCat(%p, %p, %d)\n",
	     (void *) dest, (const void *) src, len));
    if (src) {
	unsigned need = (unsigned) (len + 1);

	if (TRACE_BSTRING) {
	    CTRACE((tfp, "===    %4d:", len));
	    trace_bstring2(src, len);
	    CTRACE((tfp, "\n"));
	}
	if (t) {
	    unsigned length = (unsigned) t->len + need;

	    t->str = typeRealloc(char, t->str, length);
	} else {
	    if ((t = typecalloc(bstring)) == NULL)
		  outofmem(__FILE__, "HTSACat");

	    assert(t != NULL);

	    t->str = typeMallocn(char, need);
	}
	if (t->str == NULL)
	    outofmem(__FILE__, "HTSACat");

	assert(t->str != NULL);

	MemCpy(t->str + t->len, src, len);
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
void HTSABCat0(bstring **dest, const char *src)
{
    HTSABCat(dest, src, (int) strlen(src));
}

/*
 * Compare two bstring's for equality
 */
BOOL HTSABEql(bstring *a, bstring *b)
{
    unsigned len_a = (unsigned) ((a != 0) ? a->len : 0);
    unsigned len_b = (unsigned) ((b != 0) ? b->len : 0);

    if (len_a == len_b) {
	if (len_a == 0
	    || MemCmp(a->str, b->str, a->len) == 0)
	    return TRUE;
    }
    return FALSE;
}

/*
 * Deallocate a bstring.
 */
void HTSABFree(bstring **ptr)
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
bstring *HTBprintf(bstring **pstr, const char *fmt,...)
{
    bstring *result = 0;
    char *temp = 0;
    va_list ap;

    LYva_start(ap, fmt);
    {
	temp = StrAllocVsprintf(&temp, (size_t) 0, fmt, &ap);
	if (non_empty(temp)) {
	    HTSABCat(pstr, temp, (int) strlen(temp));
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
void trace_bstring2(const char *text,
		    int size)
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

void trace_bstring(bstring *data)
{
    trace_bstring2(BStrData(data), BStrLen(data));
}
