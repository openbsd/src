/*    util.c
 *
 *    Copyright (c) 1991-1997, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/*
 * "Very useful, no doubt, that was to Saruman; yet it seems that he was
 * not content."  --Gandalf
 */

#include "EXTERN.h"
#include "perl.h"

#if !defined(NSIG) || defined(M_UNIX) || defined(M_XENIX)
#include <signal.h>
#endif

#ifndef SIG_ERR
# define SIG_ERR ((Sighandler_t) -1)
#endif

/* XXX If this causes problems, set i_unistd=undef in the hint file.  */
#ifdef I_UNISTD
#  include <unistd.h>
#endif

#ifdef I_VFORK
#  include <vfork.h>
#endif

/* Put this after #includes because fork and vfork prototypes may
   conflict.
*/
#ifndef HAS_VFORK
#   define vfork fork
#endif

#ifdef I_FCNTL
#  include <fcntl.h>
#endif
#ifdef I_SYS_FILE
#  include <sys/file.h>
#endif

#ifdef I_SYS_WAIT
#  include <sys/wait.h>
#endif

#define FLUSH

#ifdef LEAKTEST
static void xstat _((void));
#endif

#ifndef MYMALLOC

/* paranoid version of malloc */

/* NOTE:  Do not call the next three routines directly.  Use the macros
 * in handy.h, so that we can easily redefine everything to do tracking of
 * allocated hunks back to the original New to track down any memory leaks.
 * XXX This advice seems to be widely ignored :-(   --AD  August 1996.
 */

Malloc_t
safemalloc(size)
MEM_SIZE size;
{
    Malloc_t ptr;
#ifdef HAS_64K_LIMIT
	if (size > 0xffff) {
		PerlIO_printf(PerlIO_stderr(), "Allocation too large: %lx\n", size) FLUSH;
		my_exit(1);
	}
#endif /* HAS_64K_LIMIT */
#ifdef DEBUGGING
    if ((long)size < 0)
	croak("panic: malloc");
#endif
    ptr = malloc(size?size:1);	/* malloc(0) is NASTY on our system */
#if !(defined(I286) || defined(atarist))
    DEBUG_m(PerlIO_printf(Perl_debug_log, "0x%x: (%05d) malloc %ld bytes\n",ptr,an++,(long)size));
#else
    DEBUG_m(PerlIO_printf(Perl_debug_log, "0x%lx: (%05d) malloc %ld bytes\n",ptr,an++,(long)size));
#endif
    if (ptr != Nullch)
	return ptr;
    else if (nomemok)
	return Nullch;
    else {
	PerlIO_puts(PerlIO_stderr(),no_mem) FLUSH;
	my_exit(1);
    }
    /*NOTREACHED*/
}

/* paranoid version of realloc */

Malloc_t
saferealloc(where,size)
Malloc_t where;
MEM_SIZE size;
{
    Malloc_t ptr;
#if !defined(STANDARD_C) && !defined(HAS_REALLOC_PROTOTYPE)
    Malloc_t realloc();
#endif /* !defined(STANDARD_C) && !defined(HAS_REALLOC_PROTOTYPE) */

#ifdef HAS_64K_LIMIT 
    if (size > 0xffff) {
	PerlIO_printf(PerlIO_stderr(),
		      "Reallocation too large: %lx\n", size) FLUSH;
	my_exit(1);
    }
#endif /* HAS_64K_LIMIT */
    if (!where)
	croak("Null realloc");
#ifdef DEBUGGING
    if ((long)size < 0)
	croak("panic: realloc");
#endif
    ptr = realloc(where,size?size:1);	/* realloc(0) is NASTY on our system */

#if !(defined(I286) || defined(atarist))
    DEBUG_m( {
	PerlIO_printf(Perl_debug_log, "0x%x: (%05d) rfree\n",where,an++);
	PerlIO_printf(Perl_debug_log, "0x%x: (%05d) realloc %ld bytes\n",ptr,an++,(long)size);
    } )
#else
    DEBUG_m( {
	PerlIO_printf(Perl_debug_log, "0x%lx: (%05d) rfree\n",where,an++);
	PerlIO_printf(Perl_debug_log, "0x%lx: (%05d) realloc %ld bytes\n",ptr,an++,(long)size);
    } )
#endif

    if (ptr != Nullch)
	return ptr;
    else if (nomemok)
	return Nullch;
    else {
	PerlIO_puts(PerlIO_stderr(),no_mem) FLUSH;
	my_exit(1);
    }
    /*NOTREACHED*/
}

/* safe version of free */

Free_t
safefree(where)
Malloc_t where;
{
#if !(defined(I286) || defined(atarist))
    DEBUG_m( PerlIO_printf(Perl_debug_log, "0x%x: (%05d) free\n",where,an++));
#else
    DEBUG_m( PerlIO_printf(Perl_debug_log, "0x%lx: (%05d) free\n",where,an++));
#endif
    if (where) {
	/*SUPPRESS 701*/
	free(where);
    }
}

/* safe version of calloc */

Malloc_t
safecalloc(count, size)
MEM_SIZE count;
MEM_SIZE size;
{
    Malloc_t ptr;

#ifdef HAS_64K_LIMIT
    if (size * count > 0xffff) {
	PerlIO_printf(PerlIO_stderr(),
		      "Allocation too large: %lx\n", size * count) FLUSH;
	my_exit(1);
    }
#endif /* HAS_64K_LIMIT */
#ifdef DEBUGGING
    if ((long)size < 0 || (long)count < 0)
	croak("panic: calloc");
#endif
    size *= count;
    ptr = malloc(size?size:1);	/* malloc(0) is NASTY on our system */
#if !(defined(I286) || defined(atarist))
    DEBUG_m(PerlIO_printf(Perl_debug_log, "0x%x: (%05d) calloc %ld  x %ld bytes\n",ptr,an++,(long)count,(long)size));
#else
    DEBUG_m(PerlIO_printf(Perl_debug_log, "0x%lx: (%05d) calloc %ld x %ld bytes\n",ptr,an++,(long)count,(long)size));
#endif
    if (ptr != Nullch) {
	memset((void*)ptr, 0, size);
	return ptr;
    }
    else if (nomemok)
	return Nullch;
    else {
	PerlIO_puts(PerlIO_stderr(),no_mem) FLUSH;
	my_exit(1);
    }
    /*NOTREACHED*/
}

#endif /* !MYMALLOC */

#ifdef LEAKTEST

#define ALIGN sizeof(long)

Malloc_t
safexmalloc(x,size)
I32 x;
MEM_SIZE size;
{
    register Malloc_t where;

    where = safemalloc(size + ALIGN);
    xcount[x]++;
    where[0] = x % 100;
    where[1] = x / 100;
    return where + ALIGN;
}

Malloc_t
safexrealloc(where,size)
Malloc_t where;
MEM_SIZE size;
{
    register Malloc_t new = saferealloc(where - ALIGN, size + ALIGN);
    return new + ALIGN;
}

void
safexfree(where)
Malloc_t where;
{
    I32 x;

    if (!where)
	return;
    where -= ALIGN;
    x = where[0] + 100 * where[1];
    xcount[x]--;
    safefree(where);
}

Malloc_t
safexcalloc(x,count,size)
I32 x;
MEM_SIZE count;
MEM_SIZE size;
{
    register Malloc_t where;

    where = safexmalloc(x, size * count + ALIGN);
    xcount[x]++;
    memset((void*)where + ALIGN, 0, size * count);
    where[0] = x % 100;
    where[1] = x / 100;
    return where + ALIGN;
}

static void
xstat()
{
    register I32 i;

    for (i = 0; i < MAXXCOUNT; i++) {
	if (xcount[i] > lastxcount[i]) {
	    PerlIO_printf(PerlIO_stderr(),"%2d %2d\t%ld\n", i / 100, i % 100, xcount[i]);
	    lastxcount[i] = xcount[i];
	}
    }
}

#endif /* LEAKTEST */

/* copy a string up to some (non-backslashed) delimiter, if any */

char *
delimcpy(to, toend, from, fromend, delim, retlen)
register char *to;
register char *toend;
register char *from;
register char *fromend;
register int delim;
I32 *retlen;
{
    register I32 tolen;
    for (tolen = 0; from < fromend; from++, tolen++) {
	if (*from == '\\') {
	    if (from[1] == delim)
		from++;
	    else {
		if (to < toend)
		    *to++ = *from;
		tolen++;
		from++;
	    }
	}
	else if (*from == delim)
	    break;
	if (to < toend)
	    *to++ = *from;
    }
    if (to < toend)
	*to = '\0';
    *retlen = tolen;
    return from;
}

/* return ptr to little string in big string, NULL if not found */
/* This routine was donated by Corey Satten. */

char *
instr(big, little)
register char *big;
register char *little;
{
    register char *s, *x;
    register I32 first;

    if (!little)
	return big;
    first = *little++;
    if (!first)
	return big;
    while (*big) {
	if (*big++ != first)
	    continue;
	for (x=big,s=little; *s; /**/ ) {
	    if (!*x)
		return Nullch;
	    if (*s++ != *x++) {
		s--;
		break;
	    }
	}
	if (!*s)
	    return big-1;
    }
    return Nullch;
}

/* same as instr but allow embedded nulls */

char *
ninstr(big, bigend, little, lend)
register char *big;
register char *bigend;
char *little;
char *lend;
{
    register char *s, *x;
    register I32 first = *little;
    register char *littleend = lend;

    if (!first && little >= littleend)
	return big;
    if (bigend - big < littleend - little)
	return Nullch;
    bigend -= littleend - little++;
    while (big <= bigend) {
	if (*big++ != first)
	    continue;
	for (x=big,s=little; s < littleend; /**/ ) {
	    if (*s++ != *x++) {
		s--;
		break;
	    }
	}
	if (s >= littleend)
	    return big-1;
    }
    return Nullch;
}

/* reverse of the above--find last substring */

char *
rninstr(big, bigend, little, lend)
register char *big;
char *bigend;
char *little;
char *lend;
{
    register char *bigbeg;
    register char *s, *x;
    register I32 first = *little;
    register char *littleend = lend;

    if (!first && little >= littleend)
	return bigend;
    bigbeg = big;
    big = bigend - (littleend - little++);
    while (big >= bigbeg) {
	if (*big-- != first)
	    continue;
	for (x=big+2,s=little; s < littleend; /**/ ) {
	    if (*s++ != *x++) {
		s--;
		break;
	    }
	}
	if (s >= littleend)
	    return big+1;
    }
    return Nullch;
}

/*
 * Set up for a new ctype locale.
 */
void
perl_new_ctype(newctype)
    char *newctype;
{
#ifdef USE_LOCALE_CTYPE

    int i;

    for (i = 0; i < 256; i++) {
	if (isUPPER_LC(i))
	    fold_locale[i] = toLOWER_LC(i);
	else if (isLOWER_LC(i))
	    fold_locale[i] = toUPPER_LC(i);
	else
	    fold_locale[i] = i;
    }

#endif /* USE_LOCALE_CTYPE */
}

/*
 * Set up for a new collation locale.
 */
void
perl_new_collate(newcoll)
    char *newcoll;
{
#ifdef USE_LOCALE_COLLATE

    if (! newcoll) {
	if (collation_name) {
	    ++collation_ix;
	    Safefree(collation_name);
	    collation_name = NULL;
	    collation_standard = TRUE;
	    collxfrm_base = 0;
	    collxfrm_mult = 2;
	}
	return;
    }

    if (! collation_name || strNE(collation_name, newcoll)) {
	++collation_ix;
	Safefree(collation_name);
	collation_name = savepv(newcoll);
	collation_standard = (strEQ(newcoll, "C") || strEQ(newcoll, "POSIX"));

	{
	  /*  2: at most so many chars ('a', 'b'). */
	  /* 50: surely no system expands a char more. */
#define XFRMBUFSIZE  (2 * 50)
	  char xbuf[XFRMBUFSIZE];
	  Size_t fa = strxfrm(xbuf, "a",  XFRMBUFSIZE);
	  Size_t fb = strxfrm(xbuf, "ab", XFRMBUFSIZE);
	  SSize_t mult = fb - fa;
	  if (mult < 1)
	      croak("strxfrm() gets absurd");
	  collxfrm_base = (fa > mult) ? (fa - mult) : 0;
	  collxfrm_mult = mult;
	}
    }

#endif /* USE_LOCALE_COLLATE */
}

/*
 * Set up for a new numeric locale.
 */
void
perl_new_numeric(newnum)
    char *newnum;
{
#ifdef USE_LOCALE_NUMERIC

    if (! newnum) {
	if (numeric_name) {
	    Safefree(numeric_name);
	    numeric_name = NULL;
	    numeric_standard = TRUE;
	    numeric_local = TRUE;
	}
	return;
    }

    if (! numeric_name || strNE(numeric_name, newnum)) {
	Safefree(numeric_name);
	numeric_name = savepv(newnum);
	numeric_standard = (strEQ(newnum, "C") || strEQ(newnum, "POSIX"));
	numeric_local = TRUE;
    }

#endif /* USE_LOCALE_NUMERIC */
}

void
perl_set_numeric_standard()
{
#ifdef USE_LOCALE_NUMERIC

    if (! numeric_standard) {
	setlocale(LC_NUMERIC, "C");
	numeric_standard = TRUE;
	numeric_local = FALSE;
    }

#endif /* USE_LOCALE_NUMERIC */
}

void
perl_set_numeric_local()
{
#ifdef USE_LOCALE_NUMERIC

    if (! numeric_local) {
	setlocale(LC_NUMERIC, numeric_name);
	numeric_standard = FALSE;
	numeric_local = TRUE;
    }

#endif /* USE_LOCALE_NUMERIC */
}


/*
 * Initialize locale awareness.
 */
int
perl_init_i18nl10n(printwarn)	
    int printwarn;
{
    int ok = 1;
    /* returns
     *    1 = set ok or not applicable,
     *    0 = fallback to C locale,
     *   -1 = fallback to C locale failed
     */

#ifdef USE_LOCALE

#ifdef USE_LOCALE_CTYPE
    char *curctype   = NULL;
#endif /* USE_LOCALE_CTYPE */
#ifdef USE_LOCALE_COLLATE
    char *curcoll    = NULL;
#endif /* USE_LOCALE_COLLATE */
#ifdef USE_LOCALE_NUMERIC
    char *curnum     = NULL;
#endif /* USE_LOCALE_NUMERIC */
    char *lc_all     = getenv("LC_ALL");
    char *lang       = getenv("LANG");
    bool setlocale_failure = FALSE;

#ifdef LOCALE_ENVIRON_REQUIRED

    /*
     * Ultrix setlocale(..., "") fails if there are no environment
     * variables from which to get a locale name.
     */

    bool done = FALSE;

#ifdef LC_ALL
    if (lang) {
	if (setlocale(LC_ALL, ""))
	    done = TRUE;
	else
	    setlocale_failure = TRUE;
    }
    if (!setlocale_failure)
#endif /* LC_ALL */
    {
#ifdef USE_LOCALE_CTYPE
	if (! (curctype = setlocale(LC_CTYPE,
				    (!done && (lang || getenv("LC_CTYPE")))
				    ? "" : Nullch)))
	    setlocale_failure = TRUE;
#endif /* USE_LOCALE_CTYPE */
#ifdef USE_LOCALE_COLLATE
	if (! (curcoll = setlocale(LC_COLLATE,
				   (!done && (lang || getenv("LC_COLLATE")))
				   ? "" : Nullch)))
	    setlocale_failure = TRUE;
#endif /* USE_LOCALE_COLLATE */
#ifdef USE_LOCALE_NUMERIC
	if (! (curnum = setlocale(LC_NUMERIC,
				  (!done && (lang || getenv("LC_NUMERIC")))
				  ? "" : Nullch)))
	    setlocale_failure = TRUE;
#endif /* USE_LOCALE_NUMERIC */
    }

#else /* !LOCALE_ENVIRON_REQUIRED */

#ifdef LC_ALL

    if (! setlocale(LC_ALL, ""))
	setlocale_failure = TRUE;
    else {
#ifdef USE_LOCALE_CTYPE
	curctype = setlocale(LC_CTYPE, Nullch);
#endif /* USE_LOCALE_CTYPE */
#ifdef USE_LOCALE_COLLATE
	curcoll = setlocale(LC_COLLATE, Nullch);
#endif /* USE_LOCALE_COLLATE */
#ifdef USE_LOCALE_NUMERIC
	curnum = setlocale(LC_NUMERIC, Nullch);
#endif /* USE_LOCALE_NUMERIC */
    }

#else /* !LC_ALL */

#ifdef USE_LOCALE_CTYPE
    if (! (curctype = setlocale(LC_CTYPE, "")))
	setlocale_failure = TRUE;
#endif /* USE_LOCALE_CTYPE */
#ifdef USE_LOCALE_COLLATE
    if (! (curcoll = setlocale(LC_COLLATE, "")))
	setlocale_failure = TRUE;
#endif /* USE_LOCALE_COLLATE */
#ifdef USE_LOCALE_NUMERIC
    if (! (curnum = setlocale(LC_NUMERIC, "")))
	setlocale_failure = TRUE;
#endif /* USE_LOCALE_NUMERIC */

#endif /* LC_ALL */

#endif /* !LOCALE_ENVIRON_REQUIRED */

    if (setlocale_failure) {
	char *p;
	bool locwarn = (printwarn > 1 || 
			printwarn &&
			(!(p = getenv("PERL_BADLANG")) || atoi(p)));

	if (locwarn) {
#ifdef LC_ALL
  
	    PerlIO_printf(PerlIO_stderr(),
	       "perl: warning: Setting locale failed.\n");

#else /* !LC_ALL */
  
	    PerlIO_printf(PerlIO_stderr(),
	       "perl: warning: Setting locale failed for the categories:\n\t");
#ifdef USE_LOCALE_CTYPE
	    if (! curctype)
		PerlIO_printf(PerlIO_stderr(), "LC_CTYPE ");
#endif /* USE_LOCALE_CTYPE */
#ifdef USE_LOCALE_COLLATE
	    if (! curcoll)
		PerlIO_printf(PerlIO_stderr(), "LC_COLLATE ");
#endif /* USE_LOCALE_COLLATE */
#ifdef USE_LOCALE_NUMERIC
	    if (! curnum)
		PerlIO_printf(PerlIO_stderr(), "LC_NUMERIC ");
#endif /* USE_LOCALE_NUMERIC */
	    PerlIO_printf(PerlIO_stderr(), "\n");

#endif /* LC_ALL */

	    PerlIO_printf(PerlIO_stderr(),
		"perl: warning: Please check that your locale settings:\n");

	    PerlIO_printf(PerlIO_stderr(),
			  "\tLC_ALL = %c%s%c,\n",
			  lc_all ? '"' : '(',
			  lc_all ? lc_all : "unset",
			  lc_all ? '"' : ')');

	    {
	      char **e;
	      for (e = environ; *e; e++) {
		  if (strnEQ(*e, "LC_", 3)
			&& strnNE(*e, "LC_ALL=", 7)
			&& (p = strchr(*e, '=')))
		      PerlIO_printf(PerlIO_stderr(), "\t%.*s = \"%s\",\n",
				    (int)(p - *e), *e, p + 1);
	      }
	    }

	    PerlIO_printf(PerlIO_stderr(),
			  "\tLANG = %c%s%c\n",
			  lang ? '"' : '(',
			  lang ? lang : "unset",
			  lang ? '"' : ')');

	    PerlIO_printf(PerlIO_stderr(),
			  "    are supported and installed on your system.\n");
	}

#ifdef LC_ALL

	if (setlocale(LC_ALL, "C")) {
	    if (locwarn)
		PerlIO_printf(PerlIO_stderr(),
      "perl: warning: Falling back to the standard locale (\"C\").\n");
	    ok = 0;
	}
	else {
	    if (locwarn)
		PerlIO_printf(PerlIO_stderr(),
      "perl: warning: Failed to fall back to the standard locale (\"C\").\n");
	    ok = -1;
	}

#else /* ! LC_ALL */

	if (0
#ifdef USE_LOCALE_CTYPE
	    || !(curctype || setlocale(LC_CTYPE, "C"))
#endif /* USE_LOCALE_CTYPE */
#ifdef USE_LOCALE_COLLATE
	    || !(curcoll || setlocale(LC_COLLATE, "C"))
#endif /* USE_LOCALE_COLLATE */
#ifdef USE_LOCALE_NUMERIC
	    || !(curnum || setlocale(LC_NUMERIC, "C"))
#endif /* USE_LOCALE_NUMERIC */
	    )
	{
	    if (locwarn)
		PerlIO_printf(PerlIO_stderr(),
      "perl: warning: Cannot fall back to the standard locale (\"C\").\n");
	    ok = -1;
	}

#endif /* ! LC_ALL */

#ifdef USE_LOCALE_CTYPE
	curctype = setlocale(LC_CTYPE, Nullch);
#endif /* USE_LOCALE_CTYPE */
#ifdef USE_LOCALE_COLLATE
	curcoll = setlocale(LC_COLLATE, Nullch);
#endif /* USE_LOCALE_COLLATE */
#ifdef USE_LOCALE_NUMERIC
	curnum = setlocale(LC_NUMERIC, Nullch);
#endif /* USE_LOCALE_NUMERIC */
    }

#ifdef USE_LOCALE_CTYPE
    perl_new_ctype(curctype);
#endif /* USE_LOCALE_CTYPE */

#ifdef USE_LOCALE_COLLATE
    perl_new_collate(curcoll);
#endif /* USE_LOCALE_COLLATE */

#ifdef USE_LOCALE_NUMERIC
    perl_new_numeric(curnum);
#endif /* USE_LOCALE_NUMERIC */

#endif /* USE_LOCALE */

    return ok;
}

/* Backwards compatibility. */
int
perl_init_i18nl14n(printwarn)	
    int printwarn;
{
    return perl_init_i18nl10n(printwarn);
}

#ifdef USE_LOCALE_COLLATE

/*
 * mem_collxfrm() is a bit like strxfrm() but with two important
 * differences. First, it handles embedded NULs. Second, it allocates
 * a bit more memory than needed for the transformed data itself.
 * The real transformed data begins at offset sizeof(collationix).
 * Please see sv_collxfrm() to see how this is used.
 */
char *
mem_collxfrm(s, len, xlen)
     const char *s;
     STRLEN len;
     STRLEN *xlen;
{
    char *xbuf;
    STRLEN xalloc, xin, xout;

    /* the first sizeof(collationix) bytes are used by sv_collxfrm(). */
    /* the +1 is for the terminating NUL. */

    xalloc = sizeof(collation_ix) + collxfrm_base + (collxfrm_mult * len) + 1;
    New(171, xbuf, xalloc, char);
    if (! xbuf)
	goto bad;

    *(U32*)xbuf = collation_ix;
    xout = sizeof(collation_ix);
    for (xin = 0; xin < len; ) {
	SSize_t xused;

	for (;;) {
	    xused = strxfrm(xbuf + xout, s + xin, xalloc - xout);
	    if (xused == -1)
		goto bad;
	    if (xused < xalloc - xout)
		break;
	    xalloc = (2 * xalloc) + 1;
	    Renew(xbuf, xalloc, char);
	    if (! xbuf)
		goto bad;
	}

	xin += strlen(s + xin) + 1;
	xout += xused;

	/* Embedded NULs are understood but silently skipped
	 * because they make no sense in locale collation. */
    }

    xbuf[xout] = '\0';
    *xlen = xout - sizeof(collation_ix);
    return xbuf;

  bad:
    Safefree(xbuf);
    *xlen = 0;
    return NULL;
}

#endif /* USE_LOCALE_COLLATE */

void
fbm_compile(sv)
SV *sv;
{
    register unsigned char *s;
    register unsigned char *table;
    register U32 i;
    register U32 len = SvCUR(sv);
    I32 rarest = 0;
    U32 frequency = 256;

    if (len > 255)
	return;			/* can't have offsets that big */
    Sv_Grow(sv,len+258);
    table = (unsigned char*)(SvPVX(sv) + len + 1);
    s = table - 2;
    for (i = 0; i < 256; i++) {
	table[i] = len;
    }
    i = 0;
    while (s >= (unsigned char*)(SvPVX(sv)))
    {
	if (table[*s] == len)
	    table[*s] = i;
	s--,i++;
    }
    sv_upgrade(sv, SVt_PVBM);
    sv_magic(sv, Nullsv, 'B', Nullch, 0);	/* deep magic */
    SvVALID_on(sv);

    s = (unsigned char*)(SvPVX(sv));		/* deeper magic */
    for (i = 0; i < len; i++) {
	if (freq[s[i]] < frequency) {
	    rarest = i;
	    frequency = freq[s[i]];
	}
    }
    BmRARE(sv) = s[rarest];
    BmPREVIOUS(sv) = rarest;
    DEBUG_r(PerlIO_printf(Perl_debug_log, "rarest char %c at %d\n",BmRARE(sv),BmPREVIOUS(sv)));
}

char *
fbm_instr(big, bigend, littlestr)
unsigned char *big;
register unsigned char *bigend;
SV *littlestr;
{
    register unsigned char *s;
    register I32 tmp;
    register I32 littlelen;
    register unsigned char *little;
    register unsigned char *table;
    register unsigned char *olds;
    register unsigned char *oldlittle;

    if (SvTYPE(littlestr) != SVt_PVBM || !SvVALID(littlestr)) {
	STRLEN len;
	char *l = SvPV(littlestr,len);
	if (!len)
	    return (char*)big;
	return ninstr((char*)big,(char*)bigend, l, l + len);
    }

    littlelen = SvCUR(littlestr);
    if (SvTAIL(littlestr) && !multiline) {	/* tail anchored? */
	if (littlelen > bigend - big)
	    return Nullch;
	little = (unsigned char*)SvPVX(littlestr);
	s = bigend - littlelen;
	if (*s == *little && memEQ((char*)s,(char*)little,littlelen))
	    return (char*)s;		/* how sweet it is */
	else if (bigend[-1] == '\n' && little[littlelen-1] != '\n'
		 && s > big) {
	    s--;
	    if (*s == *little && memEQ((char*)s,(char*)little,littlelen))
		return (char*)s;
	}
	return Nullch;
    }
    table = (unsigned char*)(SvPVX(littlestr) + littlelen + 1);
    if (--littlelen >= bigend - big)
	return Nullch;
    s = big + littlelen;
    oldlittle = little = table - 2;
    if (s < bigend) {
      top2:
	/*SUPPRESS 560*/
	if (tmp = table[*s]) {
#ifdef POINTERRIGOR
	    if (bigend - s > tmp) {
		s += tmp;
		goto top2;
	    }
#else
	    if ((s += tmp) < bigend)
		goto top2;
#endif
	    return Nullch;
	}
	else {
	    tmp = littlelen;	/* less expensive than calling strncmp() */
	    olds = s;
	    while (tmp--) {
		if (*--s == *--little)
		    continue;
		s = olds + 1;	/* here we pay the price for failure */
		little = oldlittle;
		if (s < bigend)	/* fake up continue to outer loop */
		    goto top2;
		return Nullch;
	    }
	    return (char *)s;
	}
    }
    return Nullch;
}

char *
screaminstr(bigstr, littlestr)
SV *bigstr;
SV *littlestr;
{
    register unsigned char *s, *x;
    register unsigned char *big;
    register I32 pos;
    register I32 previous;
    register I32 first;
    register unsigned char *little;
    register unsigned char *bigend;
    register unsigned char *littleend;

    if ((pos = screamfirst[BmRARE(littlestr)]) < 0) 
	return Nullch;
    little = (unsigned char *)(SvPVX(littlestr));
    littleend = little + SvCUR(littlestr);
    first = *little++;
    previous = BmPREVIOUS(littlestr);
    big = (unsigned char *)(SvPVX(bigstr));
    bigend = big + SvCUR(bigstr);
    while (pos < previous) {
	if (!(pos += screamnext[pos]))
	    return Nullch;
    }
#ifdef POINTERRIGOR
    do {
	if (big[pos-previous] != first)
	    continue;
	for (x=big+pos+1-previous,s=little; s < littleend; /**/ ) {
	    if (x >= bigend)
		return Nullch;
	    if (*s++ != *x++) {
		s--;
		break;
	    }
	}
	if (s == littleend)
	    return (char *)(big+pos-previous);
    } while ( pos += screamnext[pos] );
#else /* !POINTERRIGOR */
    big -= previous;
    do {
	if (big[pos] != first)
	    continue;
	for (x=big+pos+1,s=little; s < littleend; /**/ ) {
	    if (x >= bigend)
		return Nullch;
	    if (*s++ != *x++) {
		s--;
		break;
	    }
	}
	if (s == littleend)
	    return (char *)(big+pos);
    } while ( pos += screamnext[pos] );
#endif /* POINTERRIGOR */
    return Nullch;
}

I32
ibcmp(s1, s2, len)
char *s1, *s2;
register I32 len;
{
    register U8 *a = (U8 *)s1;
    register U8 *b = (U8 *)s2;
    while (len--) {
	if (*a != *b && *a != fold[*b])
	    return 1;
	a++,b++;
    }
    return 0;
}

I32
ibcmp_locale(s1, s2, len)
char *s1, *s2;
register I32 len;
{
    register U8 *a = (U8 *)s1;
    register U8 *b = (U8 *)s2;
    while (len--) {
	if (*a != *b && *a != fold_locale[*b])
	    return 1;
	a++,b++;
    }
    return 0;
}

/* copy a string to a safe spot */

char *
savepv(sv)
char *sv;
{
    register char *newaddr;

    New(902,newaddr,strlen(sv)+1,char);
    (void)strcpy(newaddr,sv);
    return newaddr;
}

/* same thing but with a known length */

char *
savepvn(sv, len)
char *sv;
register I32 len;
{
    register char *newaddr;

    New(903,newaddr,len+1,char);
    Copy(sv,newaddr,len,char);		/* might not be null terminated */
    newaddr[len] = '\0';		/* is now */
    return newaddr;
}

/* the SV for form() and mess() is not kept in an arena */

static SV *
mess_alloc()
{
    SV *sv;
    XPVMG *any;

    /* Create as PVMG now, to avoid any upgrading later */
    New(905, sv, 1, SV);
    Newz(905, any, 1, XPVMG);
    SvFLAGS(sv) = SVt_PVMG;
    SvANY(sv) = (void*)any;
    SvREFCNT(sv) = 1 << 30; /* practically infinite */
    return sv;
}

#ifdef I_STDARG
char *
form(const char* pat, ...)
#else
/*VARARGS0*/
char *
form(pat, va_alist)
    const char *pat;
    va_dcl
#endif
{
    va_list args;
#ifdef I_STDARG
    va_start(args, pat);
#else
    va_start(args);
#endif
    if (!mess_sv)
	mess_sv = mess_alloc();
    sv_vsetpvfn(mess_sv, pat, strlen(pat), &args, Null(SV**), 0, Null(bool*));
    va_end(args);
    return SvPVX(mess_sv);
}

char *
mess(pat, args)
    const char *pat;
    va_list *args;
{
    SV *sv;
    static char dgd[] = " during global destruction.\n";

    if (!mess_sv)
	mess_sv = mess_alloc();
    sv = mess_sv;
    sv_vsetpvfn(sv, pat, strlen(pat), args, Null(SV**), 0, Null(bool*));
    if (!SvCUR(sv) || *(SvEND(sv) - 1) != '\n') {
	if (dirty)
	    sv_catpv(sv, dgd);
	else {
	    if (curcop->cop_line)
		sv_catpvf(sv, " at %_ line %ld",
			  GvSV(curcop->cop_filegv), (long)curcop->cop_line);
	    if (GvIO(last_in_gv) && IoLINES(GvIOp(last_in_gv))) {
		bool line_mode = (RsSIMPLE(rs) &&
				  SvLEN(rs) == 1 && *SvPVX(rs) == '\n');
		sv_catpvf(sv, ", <%s> %s %ld",
			  last_in_gv == argvgv ? "" : GvNAME(last_in_gv),
			  line_mode ? "line" : "chunk", 
			  (long)IoLINES(GvIOp(last_in_gv)));
	    }
	    sv_catpv(sv, ".\n");
	}
    }
    return SvPVX(sv);
}

#ifdef I_STDARG
OP *
die(const char* pat, ...)
#else
/*VARARGS0*/
OP *
die(pat, va_alist)
    const char *pat;
    va_dcl
#endif
{
    va_list args;
    char *message;
    I32 oldrunlevel = runlevel;
    int was_in_eval = in_eval;
    HV *stash;
    GV *gv;
    CV *cv;

    /* We have to switch back to mainstack or die_where may try to pop
     * the eval block from the wrong stack if die is being called from a
     * signal handler.  - dkindred@cs.cmu.edu */
    if (curstack != mainstack) {
        dSP;
        SWITCHSTACK(curstack, mainstack);
    }

#ifdef I_STDARG
    va_start(args, pat);
#else
    va_start(args);
#endif
    message = mess(pat, &args);
    va_end(args);

    if (diehook) {
	/* sv_2cv might call croak() */
	SV *olddiehook = diehook;
	ENTER;
	SAVESPTR(diehook);
	diehook = Nullsv;
	cv = sv_2cv(olddiehook, &stash, &gv, 0);
	LEAVE;
	if (cv && !CvDEPTH(cv) && (CvROOT(cv) || CvXSUB(cv))) {
	    dSP;
	    SV *msg;

	    ENTER;
	    msg = newSVpv(message, 0);
	    SvREADONLY_on(msg);
	    SAVEFREESV(msg);

	    PUSHMARK(sp);
	    XPUSHs(msg);
	    PUTBACK;
	    perl_call_sv((SV*)cv, G_DISCARD);

	    LEAVE;
	}
    }

    restartop = die_where(message);
    if ((!restartop && was_in_eval) || oldrunlevel > 1)
	JMPENV_JUMP(3);
    return restartop;
}

#ifdef I_STDARG
void
croak(const char* pat, ...)
#else
/*VARARGS0*/
void
croak(pat, va_alist)
    char *pat;
    va_dcl
#endif
{
    va_list args;
    char *message;
    HV *stash;
    GV *gv;
    CV *cv;

#ifdef I_STDARG
    va_start(args, pat);
#else
    va_start(args);
#endif
    message = mess(pat, &args);
    va_end(args);
    if (diehook) {
	/* sv_2cv might call croak() */
	SV *olddiehook = diehook;
	ENTER;
	SAVESPTR(diehook);
	diehook = Nullsv;
	cv = sv_2cv(olddiehook, &stash, &gv, 0);
	LEAVE;
	if (cv && !CvDEPTH(cv) && (CvROOT(cv) || CvXSUB(cv))) {
	    dSP;
	    SV *msg;

	    ENTER;
	    msg = newSVpv(message, 0);
	    SvREADONLY_on(msg);
	    SAVEFREESV(msg);

	    PUSHMARK(sp);
	    XPUSHs(msg);
	    PUTBACK;
	    perl_call_sv((SV*)cv, G_DISCARD);

	    LEAVE;
	}
    }
    if (in_eval) {
	restartop = die_where(message);
	JMPENV_JUMP(3);
    }
    PerlIO_puts(PerlIO_stderr(),message);
    (void)PerlIO_flush(PerlIO_stderr());
    my_failure_exit();
}

void
#ifdef I_STDARG
warn(const char* pat,...)
#else
/*VARARGS0*/
warn(pat,va_alist)
    const char *pat;
    va_dcl
#endif
{
    va_list args;
    char *message;
    HV *stash;
    GV *gv;
    CV *cv;

#ifdef I_STDARG
    va_start(args, pat);
#else
    va_start(args);
#endif
    message = mess(pat, &args);
    va_end(args);

    if (warnhook) {
	/* sv_2cv might call warn() */
	SV *oldwarnhook = warnhook;
	ENTER;
	SAVESPTR(warnhook);
	warnhook = Nullsv;
	cv = sv_2cv(oldwarnhook, &stash, &gv, 0);
	LEAVE;
	if (cv && !CvDEPTH(cv) && (CvROOT(cv) || CvXSUB(cv))) {
	    dSP;
	    SV *msg;

	    ENTER;
	    msg = newSVpv(message, 0);
	    SvREADONLY_on(msg);
	    SAVEFREESV(msg);

	    PUSHMARK(sp);
	    XPUSHs(msg);
	    PUTBACK;
	    perl_call_sv((SV*)cv, G_DISCARD);

	    LEAVE;
	    return;
	}
    }
    PerlIO_puts(PerlIO_stderr(),message);
#ifdef LEAKTEST
    DEBUG_L(xstat());
#endif
    (void)PerlIO_flush(PerlIO_stderr());
}

#ifndef VMS  /* VMS' my_setenv() is in VMS.c */
#ifndef WIN32
void
my_setenv(nam,val)
char *nam, *val;
{
    register I32 i=setenv_getix(nam);		/* where does it go? */

    if (environ == origenviron) {	/* need we copy environment? */
	I32 j;
	I32 max;
	char **tmpenv;

	/*SUPPRESS 530*/
	for (max = i; environ[max]; max++) ;
	New(901,tmpenv, max+2, char*);
	for (j=0; j<max; j++)		/* copy environment */
	    tmpenv[j] = savepv(environ[j]);
	tmpenv[max] = Nullch;
	environ = tmpenv;		/* tell exec where it is now */
    }
    if (!val) {
	Safefree(environ[i]);
	while (environ[i]) {
	    environ[i] = environ[i+1];
	    i++;
	}
	return;
    }
    if (!environ[i]) {			/* does not exist yet */
	Renew(environ, i+2, char*);	/* just expand it a bit */
	environ[i+1] = Nullch;	/* make sure it's null terminated */
    }
    else
	Safefree(environ[i]);
    New(904, environ[i], strlen(nam) + strlen(val) + 2, char);
#ifndef MSDOS
    (void)sprintf(environ[i],"%s=%s",nam,val);/* all that work just for this */
#else
    /* MS-DOS requires environment variable names to be in uppercase */
    /* [Tom Dinger, 27 August 1990: Well, it doesn't _require_ it, but
     * some utilities and applications may break because they only look
     * for upper case strings. (Fixed strupr() bug here.)]
     */
    strcpy(environ[i],nam); strupr(environ[i]);
    (void)sprintf(environ[i] + strlen(nam),"=%s",val);
#endif /* MSDOS */
}

#else /* if WIN32 */

void
my_setenv(nam,val)
char *nam, *val;
{

#ifdef USE_WIN32_RTL_ENV

    register char *envstr;
    STRLEN namlen = strlen(nam);
    STRLEN vallen;
    char *oldstr = environ[setenv_getix(nam)];

    /* putenv() has totally broken semantics in both the Borland
     * and Microsoft CRTLs.  They either store the passed pointer in
     * the environment without making a copy, or make a copy and don't
     * free it. And on top of that, they dont free() old entries that
     * are being replaced/deleted.  This means the caller must
     * free any old entries somehow, or we end up with a memory
     * leak every time my_setenv() is called.  One might think
     * one could directly manipulate environ[], like the UNIX code
     * above, but direct changes to environ are not allowed when
     * calling putenv(), since the RTLs maintain an internal
     * *copy* of environ[]. Bad, bad, *bad* stink.
     * GSAR 97-06-07
     */

    if (!val) {
	if (!oldstr)
	    return;
	val = "";
	vallen = 0;
    }
    else
	vallen = strlen(val);
    New(904, envstr, namlen + vallen + 3, char);
    (void)sprintf(envstr,"%s=%s",nam,val);
    (void)putenv(envstr);
    if (oldstr)
	Safefree(oldstr);
#ifdef _MSC_VER
    Safefree(envstr);		/* MSVCRT leaks without this */
#endif

#else /* !USE_WIN32_RTL_ENV */

    /* The sane way to deal with the environment.
     * Has these advantages over putenv() & co.:
     *  * enables us to store a truly empty value in the
     *    environment (like in UNIX).
     *  * we don't have to deal with RTL globals, bugs and leaks.
     *  * Much faster.
     * Why you may want to enable USE_WIN32_RTL_ENV:
     *  * environ[] and RTL functions will not reflect changes,
     *    which might be an issue if extensions want to access
     *    the env. via RTL.  This cuts both ways, since RTL will
     *    not see changes made by extensions that call the Win32
     *    functions directly, either.
     * GSAR 97-06-07
     */
    SetEnvironmentVariable(nam,val);

#endif
}

#endif /* WIN32 */

I32
setenv_getix(nam)
char *nam;
{
    register I32 i, len = strlen(nam);

    for (i = 0; environ[i]; i++) {
	if (
#ifdef WIN32
	    strnicmp(environ[i],nam,len) == 0
#else
	    strnEQ(environ[i],nam,len)
#endif
	    && environ[i][len] == '=')
	    break;			/* strnEQ must come first to avoid */
    }					/* potential SEGV's */
    return i;
}

#endif /* !VMS */

#ifdef UNLINK_ALL_VERSIONS
I32
unlnk(f)	/* unlink all versions of a file */
char *f;
{
    I32 i;

    for (i = 0; unlink(f) >= 0; i++) ;
    return i ? 0 : -1;
}
#endif

#if !defined(HAS_BCOPY) || !defined(HAS_SAFE_BCOPY)
char *
my_bcopy(from,to,len)
register char *from;
register char *to;
register I32 len;
{
    char *retval = to;

    if (from - to >= 0) {
	while (len--)
	    *to++ = *from++;
    }
    else {
	to += len;
	from += len;
	while (len--)
	    *(--to) = *(--from);
    }
    return retval;
}
#endif

#ifndef HAS_MEMSET
void *
my_memset(loc,ch,len)
register char *loc;
register I32 ch;
register I32 len;
{
    char *retval = loc;

    while (len--)
	*loc++ = ch;
    return retval;
}
#endif

#if !defined(HAS_BZERO) && !defined(HAS_MEMSET)
char *
my_bzero(loc,len)
register char *loc;
register I32 len;
{
    char *retval = loc;

    while (len--)
	*loc++ = 0;
    return retval;
}
#endif

#if !defined(HAS_MEMCMP) || !defined(HAS_SANE_MEMCMP)
I32
my_memcmp(s1,s2,len)
char *s1;
char *s2;
register I32 len;
{
    register U8 *a = (U8 *)s1;
    register U8 *b = (U8 *)s2;
    register I32 tmp;

    while (len--) {
	if (tmp = *a++ - *b++)
	    return tmp;
    }
    return 0;
}
#endif /* !HAS_MEMCMP || !HAS_SANE_MEMCMP */

#if defined(I_STDARG) || defined(I_VARARGS)
#ifndef HAS_VPRINTF

#ifdef USE_CHAR_VSPRINTF
char *
#else
int
#endif
vsprintf(dest, pat, args)
char *dest;
const char *pat;
char *args;
{
    FILE fakebuf;

    fakebuf._ptr = dest;
    fakebuf._cnt = 32767;
#ifndef _IOSTRG
#define _IOSTRG 0
#endif
    fakebuf._flag = _IOWRT|_IOSTRG;
    _doprnt(pat, args, &fakebuf);	/* what a kludge */
    (void)putc('\0', &fakebuf);
#ifdef USE_CHAR_VSPRINTF
    return(dest);
#else
    return 0;		/* perl doesn't use return value */
#endif
}

#endif /* HAS_VPRINTF */
#endif /* I_VARARGS || I_STDARGS */

#ifdef MYSWAP
#if BYTEORDER != 0x4321
short
#ifndef CAN_PROTOTYPE
my_swap(s)
short s;
#else
my_swap(short s)
#endif
{
#if (BYTEORDER & 1) == 0
    short result;

    result = ((s & 255) << 8) + ((s >> 8) & 255);
    return result;
#else
    return s;
#endif
}

long
#ifndef CAN_PROTOTYPE
my_htonl(l)
register long l;
#else
my_htonl(long l)
#endif
{
    union {
	long result;
	char c[sizeof(long)];
    } u;

#if BYTEORDER == 0x1234
    u.c[0] = (l >> 24) & 255;
    u.c[1] = (l >> 16) & 255;
    u.c[2] = (l >> 8) & 255;
    u.c[3] = l & 255;
    return u.result;
#else
#if ((BYTEORDER - 0x1111) & 0x444) || !(BYTEORDER & 0xf)
    croak("Unknown BYTEORDER\n");
#else
    register I32 o;
    register I32 s;

    for (o = BYTEORDER - 0x1111, s = 0; s < (sizeof(long)*8); o >>= 4, s += 8) {
	u.c[o & 0xf] = (l >> s) & 255;
    }
    return u.result;
#endif
#endif
}

long
#ifndef CAN_PROTOTYPE
my_ntohl(l)
register long l;
#else
my_ntohl(long l)
#endif
{
    union {
	long l;
	char c[sizeof(long)];
    } u;

#if BYTEORDER == 0x1234
    u.c[0] = (l >> 24) & 255;
    u.c[1] = (l >> 16) & 255;
    u.c[2] = (l >> 8) & 255;
    u.c[3] = l & 255;
    return u.l;
#else
#if ((BYTEORDER - 0x1111) & 0x444) || !(BYTEORDER & 0xf)
    croak("Unknown BYTEORDER\n");
#else
    register I32 o;
    register I32 s;

    u.l = l;
    l = 0;
    for (o = BYTEORDER - 0x1111, s = 0; s < (sizeof(long)*8); o >>= 4, s += 8) {
	l |= (u.c[o & 0xf] & 255) << s;
    }
    return l;
#endif
#endif
}

#endif /* BYTEORDER != 0x4321 */
#endif /* MYSWAP */

/*
 * Little-endian byte order functions - 'v' for 'VAX', or 'reVerse'.
 * If these functions are defined,
 * the BYTEORDER is neither 0x1234 nor 0x4321.
 * However, this is not assumed.
 * -DWS
 */

#define HTOV(name,type)						\
	type							\
	name (n)						\
	register type n;					\
	{							\
	    union {						\
		type value;					\
		char c[sizeof(type)];				\
	    } u;						\
	    register I32 i;					\
	    register I32 s;					\
	    for (i = 0, s = 0; i < sizeof(u.c); i++, s += 8) {	\
		u.c[i] = (n >> s) & 0xFF;			\
	    }							\
	    return u.value;					\
	}

#define VTOH(name,type)						\
	type							\
	name (n)						\
	register type n;					\
	{							\
	    union {						\
		type value;					\
		char c[sizeof(type)];				\
	    } u;						\
	    register I32 i;					\
	    register I32 s;					\
	    u.value = n;					\
	    n = 0;						\
	    for (i = 0, s = 0; i < sizeof(u.c); i++, s += 8) {	\
		n += (u.c[i] & 0xFF) << s;			\
	    }							\
	    return n;						\
	}

#if defined(HAS_HTOVS) && !defined(htovs)
HTOV(htovs,short)
#endif
#if defined(HAS_HTOVL) && !defined(htovl)
HTOV(htovl,long)
#endif
#if defined(HAS_VTOHS) && !defined(vtohs)
VTOH(vtohs,short)
#endif
#if defined(HAS_VTOHL) && !defined(vtohl)
VTOH(vtohl,long)
#endif

    /* VMS' my_popen() is in VMS.c, same with OS/2. */
#if (!defined(DOSISH) || defined(HAS_FORK) || defined(AMIGAOS)) && !defined(VMS)
PerlIO *
my_popen(cmd,mode)
char	*cmd;
char	*mode;
{
    int p[2];
    register I32 this, that;
    register I32 pid;
    SV *sv;
    I32 doexec = strNE(cmd,"-");

#ifdef OS2
    if (doexec) {
	return my_syspopen(cmd,mode);
    }
#endif 
    if (pipe(p) < 0)
	return Nullfp;
    this = (*mode == 'w');
    that = !this;
    if (doexec && tainting) {
	taint_env();
	taint_proper("Insecure %s%s", "EXEC");
    }
    while ((pid = (doexec?vfork():fork())) < 0) {
	if (errno != EAGAIN) {
	    close(p[this]);
	    if (!doexec)
		croak("Can't fork");
	    return Nullfp;
	}
	sleep(5);
    }
    if (pid == 0) {
	GV* tmpgv;

#define THIS that
#define THAT this
	close(p[THAT]);
	if (p[THIS] != (*mode == 'r')) {
	    dup2(p[THIS], *mode == 'r');
	    close(p[THIS]);
	}
	if (doexec) {
#if !defined(HAS_FCNTL) || !defined(F_SETFD)
	    int fd;

#ifndef NOFILE
#define NOFILE 20
#endif
	    for (fd = maxsysfd + 1; fd < NOFILE; fd++)
		close(fd);
#endif
	    do_exec(cmd);	/* may or may not use the shell */
	    _exit(1);
	}
	/*SUPPRESS 560*/
	if (tmpgv = gv_fetchpv("$",TRUE, SVt_PV))
	    sv_setiv(GvSV(tmpgv), (IV)getpid());
	forkprocess = 0;
	hv_clear(pidstatus);	/* we have no children */
	return Nullfp;
#undef THIS
#undef THAT
    }
    do_execfree();	/* free any memory malloced by child on vfork */
    close(p[that]);
    if (p[that] < p[this]) {
	dup2(p[this], p[that]);
	close(p[this]);
	p[this] = p[that];
    }
    sv = *av_fetch(fdpid,p[this],TRUE);
    (void)SvUPGRADE(sv,SVt_IV);
    SvIVX(sv) = pid;
    forkprocess = pid;
    return PerlIO_fdopen(p[this], mode);
}
#else
#if defined(atarist) || defined(DJGPP)
FILE *popen();
PerlIO *
my_popen(cmd,mode)
char	*cmd;
char	*mode;
{
    /* Needs work for PerlIO ! */
    /* used 0 for 2nd parameter to PerlIO-exportFILE; apparently not used */
    return popen(PerlIO_exportFILE(cmd, 0), mode);
}
#endif

#endif /* !DOSISH */

#ifdef DUMP_FDS
dump_fds(s)
char *s;
{
    int fd;
    struct stat tmpstatbuf;

    PerlIO_printf(PerlIO_stderr(),"%s", s);
    for (fd = 0; fd < 32; fd++) {
	if (Fstat(fd,&tmpstatbuf) >= 0)
	    PerlIO_printf(PerlIO_stderr()," %d",fd);
    }
    PerlIO_printf(PerlIO_stderr(),"\n");
}
#endif

#ifndef HAS_DUP2
int
dup2(oldfd,newfd)
int oldfd;
int newfd;
{
#if defined(HAS_FCNTL) && defined(F_DUPFD)
    if (oldfd == newfd)
	return oldfd;
    close(newfd);
    return fcntl(oldfd, F_DUPFD, newfd);
#else
#define DUP2_MAX_FDS 256
    int fdtmp[DUP2_MAX_FDS];
    I32 fdx = 0;
    int fd;

    if (oldfd == newfd)
	return oldfd;
    close(newfd);
    /* good enough for low fd's... */
    while ((fd = dup(oldfd)) != newfd && fd >= 0) {
	if (fdx >= DUP2_MAX_FDS) {
	    close(fd);
	    fd = -1;
	    break;
	}
	fdtmp[fdx++] = fd;
    }
    while (fdx > 0)
	close(fdtmp[--fdx]);
    return fd;
#endif
}
#endif


#ifdef HAS_SIGACTION

Sighandler_t
rsignal(signo, handler)
int signo;
Sighandler_t handler;
{
    struct sigaction act, oact;

    act.sa_handler = handler;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
#ifdef SA_RESTART
    act.sa_flags |= SA_RESTART;	/* SVR4, 4.3+BSD */
#endif
    if (sigaction(signo, &act, &oact) == -1)
    	return SIG_ERR;
    else
    	return oact.sa_handler;
}

Sighandler_t
rsignal_state(signo)
int signo;
{
    struct sigaction oact;

    if (sigaction(signo, (struct sigaction *)NULL, &oact) == -1)
        return SIG_ERR;
    else
        return oact.sa_handler;
}

int
rsignal_save(signo, handler, save)
int signo;
Sighandler_t handler;
Sigsave_t *save;
{
    struct sigaction act;

    act.sa_handler = handler;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
#ifdef SA_RESTART
    act.sa_flags |= SA_RESTART;	/* SVR4, 4.3+BSD */
#endif
    return sigaction(signo, &act, save);
}

int
rsignal_restore(signo, save)
int signo;
Sigsave_t *save;
{
    return sigaction(signo, save, (struct sigaction *)NULL);
}

#else /* !HAS_SIGACTION */

Sighandler_t
rsignal(signo, handler)
int signo;
Sighandler_t handler;
{
    return signal(signo, handler);
}

static int sig_trapped;

static
Signal_t
sig_trap(signo)
int signo;
{
    sig_trapped++;
}

Sighandler_t
rsignal_state(signo)
int signo;
{
    Sighandler_t oldsig;

    sig_trapped = 0;
    oldsig = signal(signo, sig_trap);
    signal(signo, oldsig);
    if (sig_trapped)
        kill(getpid(), signo);
    return oldsig;
}

int
rsignal_save(signo, handler, save)
int signo;
Sighandler_t handler;
Sigsave_t *save;
{
    *save = signal(signo, handler);
    return (*save == SIG_ERR) ? -1 : 0;
}

int
rsignal_restore(signo, save)
int signo;
Sigsave_t *save;
{
    return (signal(signo, *save) == SIG_ERR) ? -1 : 0;
}

#endif /* !HAS_SIGACTION */

    /* VMS' my_pclose() is in VMS.c; same with OS/2 */
#if (!defined(DOSISH) || defined(HAS_FORK) || defined(AMIGAOS)) && !defined(VMS)
I32
my_pclose(ptr)
PerlIO *ptr;
{
    Sigsave_t hstat, istat, qstat;
    int status;
    SV **svp;
    int pid;
    bool close_failed;
    int saved_errno;
#ifdef VMS
    int saved_vaxc_errno;
#endif

    svp = av_fetch(fdpid,PerlIO_fileno(ptr),TRUE);
    pid = (int)SvIVX(*svp);
    SvREFCNT_dec(*svp);
    *svp = &sv_undef;
#ifdef OS2
    if (pid == -1) {			/* Opened by popen. */
	return my_syspclose(ptr);
    }
#endif 
    if ((close_failed = (PerlIO_close(ptr) == EOF))) {
	saved_errno = errno;
#ifdef VMS
	saved_vaxc_errno = vaxc$errno;
#endif
    }
#ifdef UTS
    if(kill(pid, 0) < 0) { return(pid); }   /* HOM 12/23/91 */
#endif
    rsignal_save(SIGHUP, SIG_IGN, &hstat);
    rsignal_save(SIGINT, SIG_IGN, &istat);
    rsignal_save(SIGQUIT, SIG_IGN, &qstat);
    do {
	pid = wait4pid(pid, &status, 0);
    } while (pid == -1 && errno == EINTR);
    rsignal_restore(SIGHUP, &hstat);
    rsignal_restore(SIGINT, &istat);
    rsignal_restore(SIGQUIT, &qstat);
    if (close_failed) {
	SETERRNO(saved_errno, saved_vaxc_errno);
	return -1;
    }
    return(pid < 0 ? pid : status == 0 ? 0 : (errno = 0, status));
}
#endif /* !DOSISH */

#if  !defined(DOSISH) || defined(OS2)
I32
wait4pid(pid,statusp,flags)
int pid;
int *statusp;
int flags;
{
    SV *sv;
    SV** svp;
    char spid[TYPE_CHARS(int)];

    if (!pid)
	return -1;
    if (pid > 0) {
	sprintf(spid, "%d", pid);
	svp = hv_fetch(pidstatus,spid,strlen(spid),FALSE);
	if (svp && *svp != &sv_undef) {
	    *statusp = SvIVX(*svp);
	    (void)hv_delete(pidstatus,spid,strlen(spid),G_DISCARD);
	    return pid;
	}
    }
    else {
	HE *entry;

	hv_iterinit(pidstatus);
	if (entry = hv_iternext(pidstatus)) {
	    pid = atoi(hv_iterkey(entry,(I32*)statusp));
	    sv = hv_iterval(pidstatus,entry);
	    *statusp = SvIVX(sv);
	    sprintf(spid, "%d", pid);
	    (void)hv_delete(pidstatus,spid,strlen(spid),G_DISCARD);
	    return pid;
	}
    }
#ifdef HAS_WAITPID
#  ifdef HAS_WAITPID_RUNTIME
    if (!HAS_WAITPID_RUNTIME)
	goto hard_way;
#  endif
    return waitpid(pid,statusp,flags);
#endif
#if !defined(HAS_WAITPID) && defined(HAS_WAIT4)
    return wait4((pid==-1)?0:pid,statusp,flags,Null(struct rusage *));
#endif
#if !defined(HAS_WAITPID) && !defined(HAS_WAIT4) || defined(HAS_WAITPID_RUNTIME)
  hard_way:
    {
	I32 result;
	if (flags)
	    croak("Can't do waitpid with flags");
	else {
	    while ((result = wait(statusp)) != pid && pid > 0 && result >= 0)
		pidgone(result,*statusp);
	    if (result < 0)
		*statusp = -1;
	}
	return result;
    }
#endif
}
#endif /* !DOSISH */

void
/*SUPPRESS 590*/
pidgone(pid,status)
int pid;
int status;
{
    register SV *sv;
    char spid[TYPE_CHARS(int)];

    sprintf(spid, "%d", pid);
    sv = *hv_fetch(pidstatus,spid,strlen(spid),TRUE);
    (void)SvUPGRADE(sv,SVt_IV);
    SvIVX(sv) = status;
    return;
}

#if defined(atarist) || defined(OS2) || defined(DJGPP)
int pclose();
#ifdef HAS_FORK
int					/* Cannot prototype with I32
					   in os2ish.h. */
my_syspclose(ptr)
#else
I32
my_pclose(ptr)
#endif 
PerlIO *ptr;
{
    /* Needs work for PerlIO ! */
    FILE *f = PerlIO_findFILE(ptr);
    I32 result = pclose(f);
    PerlIO_releaseFILE(ptr,f);
    return result;
}
#endif

void
repeatcpy(to,from,len,count)
register char *to;
register char *from;
I32 len;
register I32 count;
{
    register I32 todo;
    register char *frombase = from;

    if (len == 1) {
	todo = *from;
	while (count-- > 0)
	    *to++ = todo;
	return;
    }
    while (count-- > 0) {
	for (todo = len; todo > 0; todo--) {
	    *to++ = *from++;
	}
	from = frombase;
    }
}

#ifndef CASTNEGFLOAT
U32
cast_ulong(f)
double f;
{
    long along;

#if CASTFLAGS & 2
#   define BIGDOUBLE 2147483648.0
    if (f >= BIGDOUBLE)
	return (unsigned long)(f-(long)(f/BIGDOUBLE)*BIGDOUBLE)|0x80000000;
#endif
    if (f >= 0.0)
	return (unsigned long)f;
    along = (long)f;
    return (unsigned long)along;
}
# undef BIGDOUBLE
#endif

#ifndef CASTI32

/* Unfortunately, on some systems the cast_uv() function doesn't
   work with the system-supplied definition of ULONG_MAX.  The
   comparison  (f >= ULONG_MAX) always comes out true.  It must be a
   problem with the compiler constant folding.

   In any case, this workaround should be fine on any two's complement
   system.  If it's not, supply a '-DMY_ULONG_MAX=whatever' in your
   ccflags.
	       --Andy Dougherty      <doughera@lafcol.lafayette.edu>
*/

/* Code modified to prefer proper named type ranges, I32, IV, or UV, instead
   of LONG_(MIN/MAX).
                           -- Kenneth Albanowski <kjahds@kjahds.com>
*/                                      

#ifndef MY_UV_MAX
#  define MY_UV_MAX ((UV)IV_MAX * (UV)2 + (UV)1)
#endif

I32
cast_i32(f)
double f;
{
    if (f >= I32_MAX)
	return (I32) I32_MAX;
    if (f <= I32_MIN)
	return (I32) I32_MIN;
    return (I32) f;
}

IV
cast_iv(f)
double f;
{
    if (f >= IV_MAX)
	return (IV) IV_MAX;
    if (f <= IV_MIN)
	return (IV) IV_MIN;
    return (IV) f;
}

UV
cast_uv(f)
double f;
{
    if (f >= MY_UV_MAX)
	return (UV) MY_UV_MAX;
    return (UV) f;
}

#endif

#ifndef HAS_RENAME
I32
same_dirent(a,b)
char *a;
char *b;
{
    char *fa = strrchr(a,'/');
    char *fb = strrchr(b,'/');
    struct stat tmpstatbuf1;
    struct stat tmpstatbuf2;
    SV *tmpsv = sv_newmortal();

    if (fa)
	fa++;
    else
	fa = a;
    if (fb)
	fb++;
    else
	fb = b;
    if (strNE(a,b))
	return FALSE;
    if (fa == a)
	sv_setpv(tmpsv, ".");
    else
	sv_setpvn(tmpsv, a, fa - a);
    if (Stat(SvPVX(tmpsv), &tmpstatbuf1) < 0)
	return FALSE;
    if (fb == b)
	sv_setpv(tmpsv, ".");
    else
	sv_setpvn(tmpsv, b, fb - b);
    if (Stat(SvPVX(tmpsv), &tmpstatbuf2) < 0)
	return FALSE;
    return tmpstatbuf1.st_dev == tmpstatbuf2.st_dev &&
	   tmpstatbuf1.st_ino == tmpstatbuf2.st_ino;
}
#endif /* !HAS_RENAME */

UV
scan_oct(start, len, retlen)
char *start;
I32 len;
I32 *retlen;
{
    register char *s = start;
    register UV retval = 0;
    bool overflowed = FALSE;

    while (len && *s >= '0' && *s <= '7') {
	register UV n = retval << 3;
	if (!overflowed && (n >> 3) != retval) {
	    warn("Integer overflow in octal number");
	    overflowed = TRUE;
	}
	retval = n | (*s++ - '0');
	len--;
    }
    if (dowarn && len && (*s == '8' || *s == '9'))
	warn("Illegal octal digit ignored");
    *retlen = s - start;
    return retval;
}

UV
scan_hex(start, len, retlen)
char *start;
I32 len;
I32 *retlen;
{
    register char *s = start;
    register UV retval = 0;
    bool overflowed = FALSE;
    char *tmp;

    while (len-- && *s && (tmp = strchr(hexdigit, *s))) {
	register UV n = retval << 4;
	if (!overflowed && (n >> 4) != retval) {
	    warn("Integer overflow in hex number");
	    overflowed = TRUE;
	}
	retval = n | (tmp - hexdigit) & 15;
	s++;
    }
    *retlen = s - start;
    return retval;
}


#ifdef HUGE_VAL
/*
 * This hack is to force load of "huge" support from libm.a
 * So it is in perl for (say) POSIX to use. 
 * Needed for SunOS with Sun's 'acc' for example.
 */
double 
Perl_huge()
{
 return HUGE_VAL;
}
#endif
