/*    util.c
 *
 *    Copyright (c) 1991-1994, Larry Wall
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

/* Omit this -- it causes too much grief on mixed systems.
#ifdef I_UNISTD
#  include <unistd.h>
#endif
*/

#ifdef I_VFORK
#  include <vfork.h>
#endif

#ifdef I_LIMITS  /* Needed for cast_xxx() functions below. */
#  include <limits.h>
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

#define FLUSH

#ifdef LEAKTEST
static void xstat _((void));
#endif

#ifndef safemalloc

/* paranoid version of malloc */

/* NOTE:  Do not call the next three routines directly.  Use the macros
 * in handy.h, so that we can easily redefine everything to do tracking of
 * allocated hunks back to the original New to track down any memory leaks.
 */

char *
safemalloc(size)
#ifdef MSDOS
unsigned long size;
#else
MEM_SIZE size;
#endif /* MSDOS */
{
    char  *ptr;
#ifdef MSDOS
	if (size > 0xffff) {
		fprintf(stderr, "Allocation too large: %lx\n", size) FLUSH;
		my_exit(1);
	}
#endif /* MSDOS */
#ifdef DEBUGGING
    if ((long)size < 0)
	croak("panic: malloc");
#endif
    ptr = malloc(size?size:1);	/* malloc(0) is NASTY on our system */
#if !(defined(I286) || defined(atarist))
    DEBUG_m(fprintf(stderr,"0x%x: (%05d) malloc %ld bytes\n",ptr,an++,(long)size));
#else
    DEBUG_m(fprintf(stderr,"0x%lx: (%05d) malloc %ld bytes\n",ptr,an++,(long)size));
#endif
    if (ptr != Nullch)
	return ptr;
    else if (nomemok)
	return Nullch;
    else {
	fputs(no_mem,stderr) FLUSH;
	my_exit(1);
    }
    /*NOTREACHED*/
}

/* paranoid version of realloc */

char *
saferealloc(where,size)
char *where;
#ifndef MSDOS
MEM_SIZE size;
#else
unsigned long size;
#endif /* MSDOS */
{
    char *ptr;
#if !defined(STANDARD_C) && !defined(HAS_REALLOC_PROTOTYPE)
    char *realloc();
#endif /* !defined(STANDARD_C) && !defined(HAS_REALLOC_PROTOTYPE) */

#ifdef MSDOS
	if (size > 0xffff) {
		fprintf(stderr, "Reallocation too large: %lx\n", size) FLUSH;
		my_exit(1);
	}
#endif /* MSDOS */
    if (!where)
	croak("Null realloc");
#ifdef DEBUGGING
    if ((long)size < 0)
	croak("panic: realloc");
#endif
    ptr = realloc(where,size?size:1);	/* realloc(0) is NASTY on our system */

#if !(defined(I286) || defined(atarist))
    DEBUG_m( {
	fprintf(stderr,"0x%x: (%05d) rfree\n",where,an++);
	fprintf(stderr,"0x%x: (%05d) realloc %ld bytes\n",ptr,an++,(long)size);
    } )
#else
    DEBUG_m( {
	fprintf(stderr,"0x%lx: (%05d) rfree\n",where,an++);
	fprintf(stderr,"0x%lx: (%05d) realloc %ld bytes\n",ptr,an++,(long)size);
    } )
#endif

    if (ptr != Nullch)
	return ptr;
    else if (nomemok)
	return Nullch;
    else {
	fputs(no_mem,stderr) FLUSH;
	my_exit(1);
    }
    /*NOTREACHED*/
}

/* safe version of free */

void
safefree(where)
char *where;
{
#if !(defined(I286) || defined(atarist))
    DEBUG_m( fprintf(stderr,"0x%x: (%05d) free\n",where,an++));
#else
    DEBUG_m( fprintf(stderr,"0x%lx: (%05d) free\n",where,an++));
#endif
    if (where) {
	/*SUPPRESS 701*/
	free(where);
    }
}

#endif /* !safemalloc */

#ifdef LEAKTEST

#define ALIGN sizeof(long)

char *
safexmalloc(x,size)
I32 x;
MEM_SIZE size;
{
    register char *where;

    where = safemalloc(size + ALIGN);
    xcount[x]++;
    where[0] = x % 100;
    where[1] = x / 100;
    return where + ALIGN;
}

char *
safexrealloc(where,size)
char *where;
MEM_SIZE size;
{
    register char *new = saferealloc(where - ALIGN, size + ALIGN);
    return new + ALIGN;
}

void
safexfree(where)
char *where;
{
    I32 x;

    if (!where)
	return;
    where -= ALIGN;
    x = where[0] + 100 * where[1];
    xcount[x]--;
    safefree(where);
}

static void
xstat()
{
    register I32 i;

    for (i = 0; i < MAXXCOUNT; i++) {
	if (xcount[i] > lastxcount[i]) {
	    fprintf(stderr,"%2d %2d\t%ld\n", i / 100, i % 100, xcount[i]);
	    lastxcount[i] = xcount[i];
	}
    }
}

#endif /* LEAKTEST */

/* copy a string up to some (non-backslashed) delimiter, if any */

char *
cpytill(to,from,fromend,delim,retlen)
register char *to;
register char *from;
register char *fromend;
register int delim;
I32 *retlen;
{
    char *origto = to;

    for (; from < fromend; from++,to++) {
	if (*from == '\\') {
	    if (from[1] == delim)
		from++;
	    else if (from[1] == '\\')
		*to++ = *from++;
	}
	else if (*from == delim)
	    break;
	*to = *from;
    }
    *to = '\0';
    *retlen = to - origto;
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

/* Initialize locale (and the fold[] array).*/
int
perl_init_i18nl14n(printwarn)	
    int printwarn;
{
    int ok = 1;
    /* returns
     *    1 = set ok or not applicable,
     *    0 = fallback to C locale,
     *   -1 = fallback to C locale failed
     */
#if defined(HAS_SETLOCALE) && defined(LC_CTYPE)
    char * lang     = getenv("LANG");
    char * lc_all   = getenv("LC_ALL");
    char * lc_ctype = getenv("LC_CTYPE");
    int i;

    if (setlocale(LC_CTYPE, "") == NULL && (lc_all || lc_ctype || lang)) {
	if (printwarn) {
	    fprintf(stderr, "warning: setlocale(LC_CTYPE, \"\") failed.\n");
	    fprintf(stderr,
	      "warning: LC_ALL = \"%s\", LC_CTYPE = \"%s\", LANG = \"%s\",\n",
	      lc_all   ? lc_all   : "(null)",
	      lc_ctype ? lc_ctype : "(null)",
	      lang     ? lang     : "(null)"
	      );
	    fprintf(stderr, "warning: falling back to the \"C\" locale.\n");
	}
	ok = 0;
	if (setlocale(LC_CTYPE, "C") == NULL)
	    ok = -1;
    }

    for (i = 0; i < 256; i++) {
	if (isUPPER(i)) fold[i] = toLOWER(i);
	else if (isLOWER(i)) fold[i] = toUPPER(i);
	else fold[i] = i;
    }
#endif
    return ok;
}

void
fbm_compile(sv, iflag)
SV *sv;
I32 iflag;
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
	if (table[*s] == len) {
#ifndef pdp11
	    if (iflag)
		table[*s] = table[fold[*s]] = i;
#else
	    if (iflag) {
		I32 j;
		j = fold[*s];
		table[j] = i;
		table[*s] = i;
	    }
#endif /* pdp11 */
	    else
		table[*s] = i;
	}
	s--,i++;
    }
    sv_upgrade(sv, SVt_PVBM);
    sv_magic(sv, Nullsv, 'B', Nullch, 0);			/* deep magic */
    SvVALID_on(sv);

    s = (unsigned char*)(SvPVX(sv));		/* deeper magic */
    if (iflag) {
	register U32 tmp, foldtmp;
	SvCASEFOLD_on(sv);
	for (i = 0; i < len; i++) {
	    tmp=freq[s[i]];
	    foldtmp=freq[fold[s[i]]];
	    if (tmp < frequency && foldtmp < frequency) {
		rarest = i;
		/* choose most frequent among the two */
		frequency = (tmp > foldtmp) ? tmp : foldtmp;
	    }
	}
    }
    else {
	for (i = 0; i < len; i++) {
	    if (freq[s[i]] < frequency) {
		rarest = i;
		frequency = freq[s[i]];
	    }
	}
    }
    BmRARE(sv) = s[rarest];
    BmPREVIOUS(sv) = rarest;
    DEBUG_r(fprintf(stderr,"rarest char %c at %d\n",BmRARE(sv),BmPREVIOUS(sv)));
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
	if (SvCASEFOLD(littlestr)) {	/* oops, fake it */
	    big = bigend - littlelen;		/* just start near end */
	    if (bigend[-1] == '\n' && little[littlelen-1] != '\n')
		big--;
	}
	else {
	    s = bigend - littlelen;
	    if (*s == *little && bcmp((char*)s,(char*)little,littlelen)==0)
		return (char*)s;		/* how sweet it is */
	    else if (bigend[-1] == '\n' && little[littlelen-1] != '\n'
	      && s > big) {
		    s--;
		if (*s == *little && bcmp((char*)s,(char*)little,littlelen)==0)
		    return (char*)s;
	    }
	    return Nullch;
	}
    }
    table = (unsigned char*)(SvPVX(littlestr) + littlelen + 1);
    if (--littlelen >= bigend - big)
	return Nullch;
    s = big + littlelen;
    oldlittle = little = table - 2;
    if (SvCASEFOLD(littlestr)) {	/* case insensitive? */
	if (s < bigend) {
	  top1:
	    /*SUPPRESS 560*/
	    if (tmp = table[*s]) {
#ifdef POINTERRIGOR
		if (bigend - s > tmp) {
		    s += tmp;
		    goto top1;
		}
#else
		if ((s += tmp) < bigend)
		    goto top1;
#endif
		return Nullch;
	    }
	    else {
		tmp = littlelen;	/* less expensive than calling strncmp() */
		olds = s;
		while (tmp--) {
		    if (*--s == *--little || fold[*s] == *little)
			continue;
		    s = olds + 1;	/* here we pay the price for failure */
		    little = oldlittle;
		    if (s < bigend)	/* fake up continue to outer loop */
			goto top1;
		    return Nullch;
		}
		return (char *)s;
	    }
	}
    }
    else {
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
    if (SvCASEFOLD(littlestr)) {	/* case insignificant? */
	do {
	    if (big[pos-previous] != first && big[pos-previous] != fold[first])
		continue;
	    for (x=big+pos+1-previous,s=little; s < littleend; /**/ ) {
		if (x >= bigend)
		    return Nullch;
		if (*s++ != *x++ && fold[*(s-1)] != *(x-1)) {
		    s--;
		    break;
		}
	    }
	    if (s == littleend)
		return (char *)(big+pos-previous);
	} while (
		pos += screamnext[pos]	/* does this goof up anywhere? */
	    );
    }
    else {
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
    }
#else /* !POINTERRIGOR */
    big -= previous;
    if (SvCASEFOLD(littlestr)) {	/* case insignificant? */
	do {
	    if (big[pos] != first && big[pos] != fold[first])
		continue;
	    for (x=big+pos+1,s=little; s < littleend; /**/ ) {
		if (x >= bigend)
		    return Nullch;
		if (*s++ != *x++ && fold[*(s-1)] != *(x-1)) {
		    s--;
		    break;
		}
	    }
	    if (s == littleend)
		return (char *)(big+pos);
	} while (
		pos += screamnext[pos]	/* does this goof up anywhere? */
	    );
    }
    else {
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
	} while (
		pos += screamnext[pos]
	    );
    }
#endif /* POINTERRIGOR */
    return Nullch;
}

I32
ibcmp(a,b,len)
register U8 *a;
register U8 *b;
register I32 len;
{
    while (len--) {
	if (*a == *b) {
	    a++,b++;
	    continue;
	}
	if (fold[*a++] == *b++)
	    continue;
	return 1;
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

#if !defined(I_STDARG) && !defined(I_VARARGS)

/*
 * Fallback on the old hackers way of doing varargs
 */

/*VARARGS1*/
char *
mess(pat,a1,a2,a3,a4)
char *pat;
long a1, a2, a3, a4;
{
    char *s;
    char *s_start;
    I32 usermess = strEQ(pat,"%s");
    SV *tmpstr;

    s = s_start = buf;
    if (usermess) {
	tmpstr = sv_newmortal();
	sv_setpv(tmpstr, (char*)a1);
	*s++ = SvPVX(tmpstr)[SvCUR(tmpstr)-1];
    }
    else {
	(void)sprintf(s,pat,a1,a2,a3,a4);
	s += strlen(s);
    }

    if (s[-1] != '\n') {
	if (dirty)
	    strcpy(s, " during global destruction.\n");
	else {
	    if (curcop->cop_line) {
		(void)sprintf(s," at %s line %ld",
		  SvPVX(GvSV(curcop->cop_filegv)), (long)curcop->cop_line);
		s += strlen(s);
	    }
	    if (GvIO(last_in_gv) &&
		IoLINES(GvIOp(last_in_gv)) ) {
		(void)sprintf(s,", <%s> %s %ld",
		  last_in_gv == argvgv ? "" : GvENAME(last_in_gv),
		  strEQ(rs,"\n") ? "line" : "chunk", 
		  (long)IoLINES(GvIOp(last_in_gv)));
		s += strlen(s);
	    }
	    (void)strcpy(s,".\n");
	    s += 2;
	}
	if (usermess)
	    sv_catpv(tmpstr,buf+1);
    }

    if (s - s_start >= sizeof(buf)) {	/* Ooops! */
	if (usermess)
	    fputs(SvPVX(tmpstr), stderr);
	else
	    fputs(buf, stderr);
	fputs("panic: message overflow - memory corrupted!\n",stderr);
	my_exit(1);
    }
    if (usermess)
	return SvPVX(tmpstr);
    else
	return buf;
}

/*VARARGS1*/
void croak(pat,a1,a2,a3,a4)
char *pat;
long a1, a2, a3, a4;
{
    char *tmps;
    char *message;
    HV *stash;
    GV *gv;
    CV *cv;

    message = mess(pat,a1,a2,a3,a4);
    if (diehook && (cv = sv_2cv(diehook, &stash, &gv, 0)) && !CvDEPTH(cv)) {
	dSP;

	PUSHMARK(sp);
	EXTEND(sp, 1);
	PUSHs(sv_2mortal(newSVpv(message,0)));
	PUTBACK;
	perl_call_sv((SV*)cv, G_DISCARD);
    }
    if (in_eval) {
	restartop = die_where(message);
	Siglongjmp(top_env, 3);
    }
    fputs(message,stderr);
    (void)Fflush(stderr);
    if (e_tmpname) {
	if (e_fp) {
	    fclose(e_fp);
	    e_fp = Nullfp;
	}
	(void)UNLINK(e_tmpname);
	Safefree(e_tmpname);
	e_tmpname = Nullch;
    }
    statusvalue = SHIFTSTATUS(statusvalue);
#ifdef VMS
    my_exit((U32)vaxc$errno?vaxc$errno:errno?errno:statusvalue?statusvalue:SS$_ABORT);
#else
    my_exit((U32)((errno&255)?errno:((statusvalue&255)?statusvalue:255)));
#endif
}

/*VARARGS1*/
void warn(pat,a1,a2,a3,a4)
char *pat;
long a1, a2, a3, a4;
{
    char *message;
    SV *sv;
    HV *stash;
    GV *gv;
    CV *cv;

    message = mess(pat,a1,a2,a3,a4);
    if (warnhook && (cv = sv_2cv(warnhook, &stash, &gv, 0)) && !CvDEPTH(cv)) {
	dSP;

	PUSHMARK(sp);
	EXTEND(sp, 1);
	PUSHs(sv_2mortal(newSVpv(message,0)));
	PUTBACK;
	perl_call_sv((SV*)cv, G_DISCARD);
    }
    else {
	fputs(message,stderr);
#ifdef LEAKTEST
	DEBUG_L(xstat());
#endif
	(void)Fflush(stderr);
    }
}

#else /* !defined(I_STDARG) && !defined(I_VARARGS) */

#ifdef I_STDARG
char *
mess(char *pat, va_list *args)
#else
/*VARARGS0*/
char *
mess(pat, args)
    char *pat;
    va_list *args;
#endif
{
    char *s;
    char *s_start;
    SV *tmpstr;
    I32 usermess;
#ifndef HAS_VPRINTF
#ifdef USE_CHAR_VSPRINTF
    char *vsprintf();
#else
    I32 vsprintf();
#endif
#endif

    s = s_start = buf;
    usermess = strEQ(pat, "%s");
    if (usermess) {
	tmpstr = sv_newmortal();
	sv_setpv(tmpstr, va_arg(*args, char *));
	*s++ = SvPVX(tmpstr)[SvCUR(tmpstr)-1];
    }
    else {
	(void) vsprintf(s,pat,*args);
	s += strlen(s);
    }
    va_end(*args);

    if (s[-1] != '\n') {
	if (dirty)
	    strcpy(s, " during global destruction.\n");
	else {
	    if (curcop->cop_line) {
		(void)sprintf(s," at %s line %ld",
		  SvPVX(GvSV(curcop->cop_filegv)), (long)curcop->cop_line);
		s += strlen(s);
	    }
	    if (GvIO(last_in_gv) && IoLINES(GvIOp(last_in_gv))) {
		bool line_mode = (RsSIMPLE(rs) &&
				  SvLEN(rs) == 1 && *SvPVX(rs) == '\n');
		(void)sprintf(s,", <%s> %s %ld",
		  last_in_gv == argvgv ? "" : GvNAME(last_in_gv),
		  line_mode ? "line" : "chunk", 
		  (long)IoLINES(GvIOp(last_in_gv)));
		s += strlen(s);
	    }
	    (void)strcpy(s,".\n");
	    s += 2;
	}
	if (usermess)
	    sv_catpv(tmpstr,buf+1);
    }

    if (s - s_start >= sizeof(buf)) {	/* Ooops! */
	if (usermess)
	    fputs(SvPVX(tmpstr), stderr);
	else
	    fputs(buf, stderr);
	fputs("panic: message overflow - memory corrupted!\n",stderr);
	my_exit(1);
    }
    if (usermess)
	return SvPVX(tmpstr);
    else
	return buf;
}

#ifdef I_STDARG
void
croak(char* pat, ...)
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
    if (diehook && (cv = sv_2cv(diehook, &stash, &gv, 0)) && !CvDEPTH(cv)) {
	dSP;

	PUSHMARK(sp);
	EXTEND(sp, 1);
	PUSHs(sv_2mortal(newSVpv(message,0)));
	PUTBACK;
	perl_call_sv((SV*)cv, G_DISCARD);
    }
    if (in_eval) {
	restartop = die_where(message);
	Siglongjmp(top_env, 3);
    }
    fputs(message,stderr);
    (void)Fflush(stderr);
    if (e_tmpname) {
	if (e_fp) {
	    fclose(e_fp);
	    e_fp = Nullfp;
	}
	(void)UNLINK(e_tmpname);
	Safefree(e_tmpname);
	e_tmpname = Nullch;
    }
    statusvalue = SHIFTSTATUS(statusvalue);
#ifdef VMS
    my_exit((U32)(vaxc$errno?vaxc$errno:(statusvalue?statusvalue:44)));
#else
    my_exit((U32)((errno&255)?errno:((statusvalue&255)?statusvalue:255)));
#endif
}

void
#ifdef I_STDARG
warn(char* pat,...)
#else
/*VARARGS0*/
warn(pat,va_alist)
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

    if (warnhook && (cv = sv_2cv(warnhook, &stash, &gv, 0)) && !CvDEPTH(cv)) {
	dSP;

	PUSHMARK(sp);
	EXTEND(sp, 1);
	PUSHs(sv_2mortal(newSVpv(message,0)));
	PUTBACK;
	perl_call_sv((SV*)cv, G_DISCARD);
    }
    else {
	fputs(message,stderr);
#ifdef LEAKTEST
	DEBUG_L(xstat());
#endif
	(void)Fflush(stderr);
    }
}
#endif /* !defined(I_STDARG) && !defined(I_VARARGS) */

#ifndef VMS  /* VMS' my_setenv() is in VMS.c */
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

I32
setenv_getix(nam)
char *nam;
{
    register I32 i, len = strlen(nam);

    for (i = 0; environ[i]; i++) {
	if (strnEQ(environ[i],nam,len) && environ[i][len] == '=')
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

#ifndef HAS_MEMCMP
I32
my_memcmp(s1,s2,len)
register unsigned char *s1;
register unsigned char *s2;
register I32 len;
{
    register I32 tmp;

    while (len--) {
	if (tmp = *s1++ - *s2++)
	    return tmp;
    }
    return 0;
}
#endif /* HAS_MEMCMP */

#if defined(I_STDARG) || defined(I_VARARGS)
#ifndef HAS_VPRINTF

#ifdef USE_CHAR_VSPRINTF
char *
#else
int
#endif
vsprintf(dest, pat, args)
char *dest, *pat, *args;
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

int
vfprintf(fd, pat, args)
FILE *fd;
char *pat, *args;
{
    _doprnt(pat, args, fd);
    return 0;		/* wrong, but perl doesn't use the return value */
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

#if  !defined(DOSISH) && !defined(VMS)  /* VMS' my_popen() is in
					   VMS.c, same with OS/2. */
FILE *
my_popen(cmd,mode)
char	*cmd;
char	*mode;
{
    int p[2];
    register I32 this, that;
    register I32 pid;
    SV *sv;
    I32 doexec = strNE(cmd,"-");

    if (pipe(p) < 0)
	return Nullfp;
    this = (*mode == 'w');
    that = !this;
    if (tainting) {
	if (doexec) {
	    taint_env();
	    taint_proper("Insecure %s%s", "EXEC");
	}
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
	    sv_setiv(GvSV(tmpgv),(I32)getpid());
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
    return fdopen(p[this], mode);
}
#else
#if defined(atarist)
FILE *popen();
FILE *
my_popen(cmd,mode)
char	*cmd;
char	*mode;
{
    return popen(cmd, mode);
}
#endif

#endif /* !DOSISH */

#ifdef DUMP_FDS
dump_fds(s)
char *s;
{
    int fd;
    struct stat tmpstatbuf;

    fprintf(stderr,"%s", s);
    for (fd = 0; fd < 32; fd++) {
	if (Fstat(fd,&tmpstatbuf) >= 0)
	    fprintf(stderr," %d",fd);
    }
    fprintf(stderr,"\n");
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
    int fdtmp[256];
    I32 fdx = 0;
    int fd;

    if (oldfd == newfd)
	return oldfd;
    close(newfd);
    while ((fd = dup(oldfd)) != newfd && fd >= 0) /* good enough for low fd's */
	fdtmp[fdx++] = fd;
    while (fdx > 0)
	close(fdtmp[--fdx]);
    return fd;
#endif
}
#endif

#if  !defined(DOSISH) && !defined(VMS)  /* VMS' my_popen() is in VMS.c */
I32
my_pclose(ptr)
FILE *ptr;
{
    Signal_t (*hstat)(), (*istat)(), (*qstat)();
    int status;
    SV **svp;
    int pid;

    svp = av_fetch(fdpid,fileno(ptr),TRUE);
    pid = (int)SvIVX(*svp);
    SvREFCNT_dec(*svp);
    *svp = &sv_undef;
    fclose(ptr);
#ifdef UTS
    if(kill(pid, 0) < 0) { return(pid); }   /* HOM 12/23/91 */
#endif
    hstat = signal(SIGHUP, SIG_IGN);
    istat = signal(SIGINT, SIG_IGN);
    qstat = signal(SIGQUIT, SIG_IGN);
    do {
	pid = wait4pid(pid, &status, 0);
    } while (pid == -1 && errno == EINTR);
    signal(SIGHUP, hstat);
    signal(SIGINT, istat);
    signal(SIGQUIT, qstat);
    return(pid < 0 ? pid : status);
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
    char spid[16];

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
    return waitpid(pid,statusp,flags);
#else
#ifdef HAS_WAIT4
    return wait4((pid==-1)?0:pid,statusp,flags,Null(struct rusage *));
#else
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
    char spid[16];

    sprintf(spid, "%d", pid);
    sv = *hv_fetch(pidstatus,spid,strlen(spid),TRUE);
    (void)SvUPGRADE(sv,SVt_IV);
    SvIVX(sv) = status;
    return;
}

#if defined(atarist) || defined(OS2)
int pclose();
I32
my_pclose(ptr)
FILE *ptr;
{
    return pclose(ptr);
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

/* Look for MAX and MIN integral values.  If we can't find them,
   we'll use 32-bit two's complement defaults.
*/
#ifndef LONG_MAX
#  ifdef MAXLONG    /* Often used in <values.h> */
#    define LONG_MAX MAXLONG
#  else
#    define LONG_MAX        2147483647L
#  endif
#endif

#ifndef LONG_MIN
#    define LONG_MIN        (-LONG_MAX - 1)
#endif

#ifndef ULONG_MAX
#  ifdef MAXULONG 
#    define LONG_MAX MAXULONG
#  else
#    define ULONG_MAX       4294967295L
#  endif
#endif

/* Unfortunately, on some systems the cast_uv() function doesn't
   work with the system-supplied definition of ULONG_MAX.  The
   comparison  (f >= ULONG_MAX) always comes out true.  It must be a
   problem with the compiler constant folding.

   In any case, this workaround should be fine on any two's complement
   system.  If it's not, supply a '-DMY_ULONG_MAX=whatever' in your
   ccflags.
	       --Andy Dougherty      <doughera@lafcol.lafayette.edu>
*/
#ifndef MY_ULONG_MAX
#  define MY_ULONG_MAX ((UV)LONG_MAX * (UV)2 + (UV)1)
#endif

I32
cast_i32(f)
double f;
{
    if (f >= LONG_MAX)
	return (I32) LONG_MAX;
    if (f <= LONG_MIN)
	return (I32) LONG_MIN;
    return (I32) f;
}

IV
cast_iv(f)
double f;
{
    if (f >= LONG_MAX)
	return (IV) LONG_MAX;
    if (f <= LONG_MIN)
	return (IV) LONG_MIN;
    return (IV) f;
}

UV
cast_uv(f)
double f;
{
    if (f >= MY_ULONG_MAX)
	return (UV) MY_ULONG_MAX;
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
#ifndef MAXPATHLEN
#define MAXPATHLEN 1024
#endif
    char tmpbuf[MAXPATHLEN+1];

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
	strcpy(tmpbuf,".");
    else
	strncpy(tmpbuf, a, fa - a);
    if (Stat(tmpbuf, &tmpstatbuf1) < 0)
	return FALSE;
    if (fb == b)
	strcpy(tmpbuf,".");
    else
	strncpy(tmpbuf, b, fb - b);
    if (Stat(tmpbuf, &tmpstatbuf2) < 0)
	return FALSE;
    return tmpstatbuf1.st_dev == tmpstatbuf2.st_dev &&
	   tmpstatbuf1.st_ino == tmpstatbuf2.st_ino;
}
#endif /* !HAS_RENAME */

unsigned long
scan_oct(start, len, retlen)
char *start;
I32 len;
I32 *retlen;
{
    register char *s = start;
    register unsigned long retval = 0;

    while (len && *s >= '0' && *s <= '7') {
	retval <<= 3;
	retval |= *s++ - '0';
	len--;
    }
    if (dowarn && len && (*s == '8' || *s == '9'))
	warn("Illegal octal digit ignored");
    *retlen = s - start;
    return retval;
}

unsigned long
scan_hex(start, len, retlen)
char *start;
I32 len;
I32 *retlen;
{
    register char *s = start;
    register unsigned long retval = 0;
    char *tmp;

    while (len-- && *s && (tmp = strchr(hexdigit, *s))) {
	retval <<= 4;
	retval |= (tmp - hexdigit) & 15;
	s++;
    }
    *retlen = s - start;
    return retval;
}
