/*    util.c
 *
 *    Copyright (C) 1993, 1994, 1995, 1996, 1997, 1998, 1999,
 *    2000, 2001, 2002, 2003, 2004, by Larry Wall and others
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
#define PERL_IN_UTIL_C
#include "perl.h"

#ifndef PERL_MICRO
#include <signal.h>
#ifndef SIG_ERR
# define SIG_ERR ((Sighandler_t) -1)
#endif
#endif

#ifdef I_SYS_WAIT
#  include <sys/wait.h>
#endif

#ifdef HAS_SELECT
# ifdef I_SYS_SELECT
#  include <sys/select.h>
# endif
#endif

#define FLUSH

#if defined(HAS_FCNTL) && defined(F_SETFD) && !defined(FD_CLOEXEC)
#  define FD_CLOEXEC 1			/* NeXT needs this */
#endif

/* NOTE:  Do not call the next three routines directly.  Use the macros
 * in handy.h, so that we can easily redefine everything to do tracking of
 * allocated hunks back to the original New to track down any memory leaks.
 * XXX This advice seems to be widely ignored :-(   --AD  August 1996.
 */

/* paranoid version of system's malloc() */

Malloc_t
Perl_safesysmalloc(MEM_SIZE size)
{
    dTHX;
    Malloc_t ptr;
#ifdef HAS_64K_LIMIT
	if (size > 0xffff) {
	    PerlIO_printf(Perl_error_log,
			  "Allocation too large: %lx\n", size) FLUSH;
	    my_exit(1);
	}
#endif /* HAS_64K_LIMIT */
#ifdef DEBUGGING
    if ((long)size < 0)
	Perl_croak_nocontext("panic: malloc");
#endif
    ptr = (Malloc_t)PerlMem_malloc(size?size:1);	/* malloc(0) is NASTY on our system */
    PERL_ALLOC_CHECK(ptr);
    DEBUG_m(PerlIO_printf(Perl_debug_log, "0x%"UVxf": (%05ld) malloc %ld bytes\n",PTR2UV(ptr),(long)PL_an++,(long)size));
    if (ptr != Nullch)
	return ptr;
    else if (PL_nomemok)
	return Nullch;
    else {
	/* Can't use PerlIO to write as it allocates memory */
	PerlLIO_write(PerlIO_fileno(Perl_error_log),
		      PL_no_mem, strlen(PL_no_mem));
	my_exit(1);
	return Nullch;
    }
    /*NOTREACHED*/
}

/* paranoid version of system's realloc() */

Malloc_t
Perl_safesysrealloc(Malloc_t where,MEM_SIZE size)
{
    dTHX;
    Malloc_t ptr;
#if !defined(STANDARD_C) && !defined(HAS_REALLOC_PROTOTYPE) && !defined(PERL_MICRO)
    Malloc_t PerlMem_realloc();
#endif /* !defined(STANDARD_C) && !defined(HAS_REALLOC_PROTOTYPE) */

#ifdef HAS_64K_LIMIT
    if (size > 0xffff) {
	PerlIO_printf(Perl_error_log,
		      "Reallocation too large: %lx\n", size) FLUSH;
	my_exit(1);
    }
#endif /* HAS_64K_LIMIT */
    if (!size) {
	safesysfree(where);
	return NULL;
    }

    if (!where)
	return safesysmalloc(size);
#ifdef DEBUGGING
    if ((long)size < 0)
	Perl_croak_nocontext("panic: realloc");
#endif
    ptr = (Malloc_t)PerlMem_realloc(where,size);
    PERL_ALLOC_CHECK(ptr);

    DEBUG_m(PerlIO_printf(Perl_debug_log, "0x%"UVxf": (%05ld) rfree\n",PTR2UV(where),(long)PL_an++));
    DEBUG_m(PerlIO_printf(Perl_debug_log, "0x%"UVxf": (%05ld) realloc %ld bytes\n",PTR2UV(ptr),(long)PL_an++,(long)size));

    if (ptr != Nullch)
	return ptr;
    else if (PL_nomemok)
	return Nullch;
    else {
	/* Can't use PerlIO to write as it allocates memory */
	PerlLIO_write(PerlIO_fileno(Perl_error_log),
		      PL_no_mem, strlen(PL_no_mem));
	my_exit(1);
	return Nullch;
    }
    /*NOTREACHED*/
}

/* safe version of system's free() */

Free_t
Perl_safesysfree(Malloc_t where)
{
#ifdef PERL_IMPLICIT_SYS
    dTHX;
#endif
    DEBUG_m( PerlIO_printf(Perl_debug_log, "0x%"UVxf": (%05ld) free\n",PTR2UV(where),(long)PL_an++));
    if (where) {
	/*SUPPRESS 701*/
	PerlMem_free(where);
    }
}

/* safe version of system's calloc() */

Malloc_t
Perl_safesyscalloc(MEM_SIZE count, MEM_SIZE size)
{
    dTHX;
    Malloc_t ptr;

#ifdef HAS_64K_LIMIT
    if (size * count > 0xffff) {
	PerlIO_printf(Perl_error_log,
		      "Allocation too large: %lx\n", size * count) FLUSH;
	my_exit(1);
    }
#endif /* HAS_64K_LIMIT */
#ifdef DEBUGGING
    if ((long)size < 0 || (long)count < 0)
	Perl_croak_nocontext("panic: calloc");
#endif
    size *= count;
    ptr = (Malloc_t)PerlMem_malloc(size?size:1);	/* malloc(0) is NASTY on our system */
    PERL_ALLOC_CHECK(ptr);
    DEBUG_m(PerlIO_printf(Perl_debug_log, "0x%"UVxf": (%05ld) calloc %ld x %ld bytes\n",PTR2UV(ptr),(long)PL_an++,(long)count,(long)size));
    if (ptr != Nullch) {
	memset((void*)ptr, 0, size);
	return ptr;
    }
    else if (PL_nomemok)
	return Nullch;
    else {
	/* Can't use PerlIO to write as it allocates memory */
	PerlLIO_write(PerlIO_fileno(Perl_error_log),
		      PL_no_mem, strlen(PL_no_mem));
	my_exit(1);
	return Nullch;
    }
    /*NOTREACHED*/
}

/* These must be defined when not using Perl's malloc for binary
 * compatibility */

#ifndef MYMALLOC

Malloc_t Perl_malloc (MEM_SIZE nbytes)
{
    dTHXs;
    return (Malloc_t)PerlMem_malloc(nbytes);
}

Malloc_t Perl_calloc (MEM_SIZE elements, MEM_SIZE size)
{
    dTHXs;
    return (Malloc_t)PerlMem_calloc(elements, size);
}

Malloc_t Perl_realloc (Malloc_t where, MEM_SIZE nbytes)
{
    dTHXs;
    return (Malloc_t)PerlMem_realloc(where, nbytes);
}

Free_t   Perl_mfree (Malloc_t where)
{
    dTHXs;
    PerlMem_free(where);
}

#endif

/* copy a string up to some (non-backslashed) delimiter, if any */

char *
Perl_delimcpy(pTHX_ register char *to, register char *toend, register char *from, register char *fromend, register int delim, I32 *retlen)
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
Perl_instr(pTHX_ register const char *big, register const char *little)
{
    register const char *s, *x;
    register I32 first;

    if (!little)
	return (char*)big;
    first = *little++;
    if (!first)
	return (char*)big;
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
	    return (char*)(big-1);
    }
    return Nullch;
}

/* same as instr but allow embedded nulls */

char *
Perl_ninstr(pTHX_ register const char *big, register const char *bigend, const char *little, const char *lend)
{
    register const char *s, *x;
    register I32 first = *little;
    register const char *littleend = lend;

    if (!first && little >= littleend)
	return (char*)big;
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
	    return (char*)(big-1);
    }
    return Nullch;
}

/* reverse of the above--find last substring */

char *
Perl_rninstr(pTHX_ register const char *big, const char *bigend, const char *little, const char *lend)
{
    register const char *bigbeg;
    register const char *s, *x;
    register I32 first = *little;
    register const char *littleend = lend;

    if (!first && little >= littleend)
	return (char*)bigend;
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
	    return (char*)(big+1);
    }
    return Nullch;
}

#define FBM_TABLE_OFFSET 2	/* Number of bytes between EOS and table*/

/* As a space optimization, we do not compile tables for strings of length
   0 and 1, and for strings of length 2 unless FBMcf_TAIL.  These are
   special-cased in fbm_instr().

   If FBMcf_TAIL, the table is created as if the string has a trailing \n. */

/*
=head1 Miscellaneous Functions

=for apidoc fbm_compile

Analyses the string in order to make fast searches on it using fbm_instr()
-- the Boyer-Moore algorithm.

=cut
*/

void
Perl_fbm_compile(pTHX_ SV *sv, U32 flags)
{
    register U8 *s;
    register U8 *table;
    register U32 i;
    STRLEN len;
    I32 rarest = 0;
    U32 frequency = 256;

    if (flags & FBMcf_TAIL) {
	MAGIC *mg = SvUTF8(sv) && SvMAGICAL(sv) ? mg_find(sv, PERL_MAGIC_utf8) : NULL;
	sv_catpvn(sv, "\n", 1);		/* Taken into account in fbm_instr() */
	if (mg && mg->mg_len >= 0)
	    mg->mg_len++;
    }
    s = (U8*)SvPV_force(sv, len);
    (void)SvUPGRADE(sv, SVt_PVBM);
    if (len == 0)		/* TAIL might be on a zero-length string. */
	return;
    if (len > 2) {
	U8 mlen;
	unsigned char *sb;

	if (len > 255)
	    mlen = 255;
	else
	    mlen = (U8)len;
	Sv_Grow(sv, len + 256 + FBM_TABLE_OFFSET);
	table = (unsigned char*)(SvPVX(sv) + len + FBM_TABLE_OFFSET);
	s = table - 1 - FBM_TABLE_OFFSET;	/* last char */
	memset((void*)table, mlen, 256);
	table[-1] = (U8)flags;
	i = 0;
	sb = s - mlen + 1;			/* first char (maybe) */
	while (s >= sb) {
	    if (table[*s] == mlen)
		table[*s] = (U8)i;
	    s--, i++;
	}
    }
    sv_magic(sv, Nullsv, PERL_MAGIC_bm, Nullch, 0);	/* deep magic */
    SvVALID_on(sv);

    s = (unsigned char*)(SvPVX(sv));		/* deeper magic */
    for (i = 0; i < len; i++) {
	if (PL_freq[s[i]] < frequency) {
	    rarest = i;
	    frequency = PL_freq[s[i]];
	}
    }
    BmRARE(sv) = s[rarest];
    BmPREVIOUS(sv) = (U16)rarest;
    BmUSEFUL(sv) = 100;			/* Initial value */
    if (flags & FBMcf_TAIL)
	SvTAIL_on(sv);
    DEBUG_r(PerlIO_printf(Perl_debug_log, "rarest char %c at %d\n",
			  BmRARE(sv),BmPREVIOUS(sv)));
}

/* If SvTAIL(littlestr), it has a fake '\n' at end. */
/* If SvTAIL is actually due to \Z or \z, this gives false positives
   if multiline */

/*
=for apidoc fbm_instr

Returns the location of the SV in the string delimited by C<str> and
C<strend>.  It returns C<Nullch> if the string can't be found.  The C<sv>
does not have to be fbm_compiled, but the search will not be as fast
then.

=cut
*/

char *
Perl_fbm_instr(pTHX_ unsigned char *big, register unsigned char *bigend, SV *littlestr, U32 flags)
{
    register unsigned char *s;
    STRLEN l;
    register unsigned char *little = (unsigned char *)SvPV(littlestr,l);
    register STRLEN littlelen = l;
    register I32 multiline = flags & FBMrf_MULTILINE;

    if ((STRLEN)(bigend - big) < littlelen) {
	if ( SvTAIL(littlestr)
	     && ((STRLEN)(bigend - big) == littlelen - 1)
	     && (littlelen == 1
		 || (*big == *little &&
		     memEQ((char *)big, (char *)little, littlelen - 1))))
	    return (char*)big;
	return Nullch;
    }

    if (littlelen <= 2) {		/* Special-cased */

	if (littlelen == 1) {
	    if (SvTAIL(littlestr) && !multiline) { /* Anchor only! */
		/* Know that bigend != big.  */
		if (bigend[-1] == '\n')
		    return (char *)(bigend - 1);
		return (char *) bigend;
	    }
	    s = big;
	    while (s < bigend) {
		if (*s == *little)
		    return (char *)s;
		s++;
	    }
	    if (SvTAIL(littlestr))
		return (char *) bigend;
	    return Nullch;
	}
	if (!littlelen)
	    return (char*)big;		/* Cannot be SvTAIL! */

	/* littlelen is 2 */
	if (SvTAIL(littlestr) && !multiline) {
	    if (bigend[-1] == '\n' && bigend[-2] == *little)
		return (char*)bigend - 2;
	    if (bigend[-1] == *little)
		return (char*)bigend - 1;
	    return Nullch;
	}
	{
	    /* This should be better than FBM if c1 == c2, and almost
	       as good otherwise: maybe better since we do less indirection.
	       And we save a lot of memory by caching no table. */
	    register unsigned char c1 = little[0];
	    register unsigned char c2 = little[1];

	    s = big + 1;
	    bigend--;
	    if (c1 != c2) {
		while (s <= bigend) {
		    if (s[0] == c2) {
			if (s[-1] == c1)
			    return (char*)s - 1;
			s += 2;
			continue;
		    }
		  next_chars:
		    if (s[0] == c1) {
			if (s == bigend)
			    goto check_1char_anchor;
			if (s[1] == c2)
			    return (char*)s;
			else {
			    s++;
			    goto next_chars;
			}
		    }
		    else
			s += 2;
		}
		goto check_1char_anchor;
	    }
	    /* Now c1 == c2 */
	    while (s <= bigend) {
		if (s[0] == c1) {
		    if (s[-1] == c1)
			return (char*)s - 1;
		    if (s == bigend)
			goto check_1char_anchor;
		    if (s[1] == c1)
			return (char*)s;
		    s += 3;
		}
		else
		    s += 2;
	    }
	}
      check_1char_anchor:		/* One char and anchor! */
	if (SvTAIL(littlestr) && (*bigend == *little))
	    return (char *)bigend;	/* bigend is already decremented. */
	return Nullch;
    }
    if (SvTAIL(littlestr) && !multiline) {	/* tail anchored? */
	s = bigend - littlelen;
	if (s >= big && bigend[-1] == '\n' && *s == *little
	    /* Automatically of length > 2 */
	    && memEQ((char*)s + 1, (char*)little + 1, littlelen - 2))
	{
	    return (char*)s;		/* how sweet it is */
	}
	if (s[1] == *little
	    && memEQ((char*)s + 2, (char*)little + 1, littlelen - 2))
	{
	    return (char*)s + 1;	/* how sweet it is */
	}
	return Nullch;
    }
    if (SvTYPE(littlestr) != SVt_PVBM || !SvVALID(littlestr)) {
	char *b = ninstr((char*)big,(char*)bigend,
			 (char*)little, (char*)little + littlelen);

	if (!b && SvTAIL(littlestr)) {	/* Automatically multiline!  */
	    /* Chop \n from littlestr: */
	    s = bigend - littlelen + 1;
	    if (*s == *little
		&& memEQ((char*)s + 1, (char*)little + 1, littlelen - 2))
	    {
		return (char*)s;
	    }
	    return Nullch;
	}
	return b;
    }

    {	/* Do actual FBM.  */
	register unsigned char *table = little + littlelen + FBM_TABLE_OFFSET;
	register unsigned char *oldlittle;

	if (littlelen > (STRLEN)(bigend - big))
	    return Nullch;
	--littlelen;			/* Last char found by table lookup */

	s = big + littlelen;
	little += littlelen;		/* last char */
	oldlittle = little;
	if (s < bigend) {
	    register I32 tmp;

	  top2:
	    /*SUPPRESS 560*/
	    if ((tmp = table[*s])) {
		if ((s += tmp) < bigend)
		    goto top2;
		goto check_end;
	    }
	    else {		/* less expensive than calling strncmp() */
		register unsigned char *olds = s;

		tmp = littlelen;

		while (tmp--) {
		    if (*--s == *--little)
			continue;
		    s = olds + 1;	/* here we pay the price for failure */
		    little = oldlittle;
		    if (s < bigend)	/* fake up continue to outer loop */
			goto top2;
		    goto check_end;
		}
		return (char *)s;
	    }
	}
      check_end:
	if ( s == bigend && (table[-1] & FBMcf_TAIL)
	     && memEQ((char *)(bigend - littlelen),
		      (char *)(oldlittle - littlelen), littlelen) )
	    return (char*)bigend - littlelen;
	return Nullch;
    }
}

/* start_shift, end_shift are positive quantities which give offsets
   of ends of some substring of bigstr.
   If `last' we want the last occurrence.
   old_posp is the way of communication between consequent calls if
   the next call needs to find the .
   The initial *old_posp should be -1.

   Note that we take into account SvTAIL, so one can get extra
   optimizations if _ALL flag is set.
 */

/* If SvTAIL is actually due to \Z or \z, this gives false positives
   if PL_multiline.  In fact if !PL_multiline the authoritative answer
   is not supported yet. */

char *
Perl_screaminstr(pTHX_ SV *bigstr, SV *littlestr, I32 start_shift, I32 end_shift, I32 *old_posp, I32 last)
{
    register unsigned char *s, *x;
    register unsigned char *big;
    register I32 pos;
    register I32 previous;
    register I32 first;
    register unsigned char *little;
    register I32 stop_pos;
    register unsigned char *littleend;
    I32 found = 0;

    if (*old_posp == -1
	? (pos = PL_screamfirst[BmRARE(littlestr)]) < 0
	: (((pos = *old_posp), pos += PL_screamnext[pos]) == 0)) {
      cant_find:
	if ( BmRARE(littlestr) == '\n'
	     && BmPREVIOUS(littlestr) == SvCUR(littlestr) - 1) {
	    little = (unsigned char *)(SvPVX(littlestr));
	    littleend = little + SvCUR(littlestr);
	    first = *little++;
	    goto check_tail;
	}
	return Nullch;
    }

    little = (unsigned char *)(SvPVX(littlestr));
    littleend = little + SvCUR(littlestr);
    first = *little++;
    /* The value of pos we can start at: */
    previous = BmPREVIOUS(littlestr);
    big = (unsigned char *)(SvPVX(bigstr));
    /* The value of pos we can stop at: */
    stop_pos = SvCUR(bigstr) - end_shift - (SvCUR(littlestr) - 1 - previous);
    if (previous + start_shift > stop_pos) {
/*
  stop_pos does not include SvTAIL in the count, so this check is incorrect
  (I think) - see [ID 20010618.006] and t/op/study.t. HVDS 2001/06/19
*/
#if 0
	if (previous + start_shift == stop_pos + 1) /* A fake '\n'? */
	    goto check_tail;
#endif
	return Nullch;
    }
    while (pos < previous + start_shift) {
	if (!(pos += PL_screamnext[pos]))
	    goto cant_find;
    }
    big -= previous;
    do {
	if (pos >= stop_pos) break;
	if (big[pos] != first)
	    continue;
	for (x=big+pos+1,s=little; s < littleend; /**/ ) {
	    if (*s++ != *x++) {
		s--;
		break;
	    }
	}
	if (s == littleend) {
	    *old_posp = pos;
	    if (!last) return (char *)(big+pos);
	    found = 1;
	}
    } while ( pos += PL_screamnext[pos] );
    if (last && found)
	return (char *)(big+(*old_posp));
  check_tail:
    if (!SvTAIL(littlestr) || (end_shift > 0))
	return Nullch;
    /* Ignore the trailing "\n".  This code is not microoptimized */
    big = (unsigned char *)(SvPVX(bigstr) + SvCUR(bigstr));
    stop_pos = littleend - little;	/* Actual littlestr len */
    if (stop_pos == 0)
	return (char*)big;
    big -= stop_pos;
    if (*big == first
	&& ((stop_pos == 1) ||
	    memEQ((char *)(big + 1), (char *)little, stop_pos - 1)))
	return (char*)big;
    return Nullch;
}

I32
Perl_ibcmp(pTHX_ const char *s1, const char *s2, register I32 len)
{
    register U8 *a = (U8 *)s1;
    register U8 *b = (U8 *)s2;
    while (len--) {
	if (*a != *b && *a != PL_fold[*b])
	    return 1;
	a++,b++;
    }
    return 0;
}

I32
Perl_ibcmp_locale(pTHX_ const char *s1, const char *s2, register I32 len)
{
    register U8 *a = (U8 *)s1;
    register U8 *b = (U8 *)s2;
    while (len--) {
	if (*a != *b && *a != PL_fold_locale[*b])
	    return 1;
	a++,b++;
    }
    return 0;
}

/* copy a string to a safe spot */

/*
=head1 Memory Management

=for apidoc savepv

Perl's version of C<strdup()>. Returns a pointer to a newly allocated
string which is a duplicate of C<pv>. The size of the string is
determined by C<strlen()>. The memory allocated for the new string can
be freed with the C<Safefree()> function.

=cut
*/

char *
Perl_savepv(pTHX_ const char *pv)
{
    register char *newaddr = Nullch;
    if (pv) {
	New(902,newaddr,strlen(pv)+1,char);
	(void)strcpy(newaddr,pv);
    }
    return newaddr;
}

/* same thing but with a known length */

/*
=for apidoc savepvn

Perl's version of what C<strndup()> would be if it existed. Returns a
pointer to a newly allocated string which is a duplicate of the first
C<len> bytes from C<pv>. The memory allocated for the new string can be
freed with the C<Safefree()> function.

=cut
*/

char *
Perl_savepvn(pTHX_ const char *pv, register I32 len)
{
    register char *newaddr;

    New(903,newaddr,len+1,char);
    /* Give a meaning to NULL pointer mainly for the use in sv_magic() */
    if (pv) {
    	Copy(pv,newaddr,len,char);	/* might not be null terminated */
    	newaddr[len] = '\0';		/* is now */
    }
    else {
	Zero(newaddr,len+1,char);
    }
    return newaddr;
}

/*
=for apidoc savesharedpv

A version of C<savepv()> which allocates the duplicate string in memory
which is shared between threads.

=cut
*/
char *
Perl_savesharedpv(pTHX_ const char *pv)
{
    register char *newaddr = Nullch;
    if (pv) {
	newaddr = (char*)PerlMemShared_malloc(strlen(pv)+1);
    	(void)strcpy(newaddr,pv);
    }
    return newaddr;
}



/* the SV for Perl_form() and mess() is not kept in an arena */

STATIC SV *
S_mess_alloc(pTHX)
{
    SV *sv;
    XPVMG *any;

    if (!PL_dirty)
	return sv_2mortal(newSVpvn("",0));

    if (PL_mess_sv)
	return PL_mess_sv;

    /* Create as PVMG now, to avoid any upgrading later */
    New(905, sv, 1, SV);
    Newz(905, any, 1, XPVMG);
    SvFLAGS(sv) = SVt_PVMG;
    SvANY(sv) = (void*)any;
    SvREFCNT(sv) = 1 << 30; /* practically infinite */
    PL_mess_sv = sv;
    return sv;
}

#if defined(PERL_IMPLICIT_CONTEXT)
char *
Perl_form_nocontext(const char* pat, ...)
{
    dTHX;
    char *retval;
    va_list args;
    va_start(args, pat);
    retval = vform(pat, &args);
    va_end(args);
    return retval;
}
#endif /* PERL_IMPLICIT_CONTEXT */

/*
=head1 Miscellaneous Functions
=for apidoc form

Takes a sprintf-style format pattern and conventional
(non-SV) arguments and returns the formatted string.

    (char *) Perl_form(pTHX_ const char* pat, ...)

can be used any place a string (char *) is required:

    char * s = Perl_form("%d.%d",major,minor);

Uses a single private buffer so if you want to format several strings you
must explicitly copy the earlier strings away (and free the copies when you
are done).

=cut
*/

char *
Perl_form(pTHX_ const char* pat, ...)
{
    char *retval;
    va_list args;
    va_start(args, pat);
    retval = vform(pat, &args);
    va_end(args);
    return retval;
}

char *
Perl_vform(pTHX_ const char *pat, va_list *args)
{
    SV *sv = mess_alloc();
    sv_vsetpvfn(sv, pat, strlen(pat), args, Null(SV**), 0, Null(bool*));
    return SvPVX(sv);
}

#if defined(PERL_IMPLICIT_CONTEXT)
SV *
Perl_mess_nocontext(const char *pat, ...)
{
    dTHX;
    SV *retval;
    va_list args;
    va_start(args, pat);
    retval = vmess(pat, &args);
    va_end(args);
    return retval;
}
#endif /* PERL_IMPLICIT_CONTEXT */

SV *
Perl_mess(pTHX_ const char *pat, ...)
{
    SV *retval;
    va_list args;
    va_start(args, pat);
    retval = vmess(pat, &args);
    va_end(args);
    return retval;
}

STATIC COP*
S_closest_cop(pTHX_ COP *cop, OP *o)
{
    /* Look for PL_op starting from o.  cop is the last COP we've seen. */

    if (!o || o == PL_op) return cop;

    if (o->op_flags & OPf_KIDS) {
	OP *kid;
	for (kid = cUNOPo->op_first; kid; kid = kid->op_sibling)
	{
	    COP *new_cop;

	    /* If the OP_NEXTSTATE has been optimised away we can still use it
	     * the get the file and line number. */

	    if (kid->op_type == OP_NULL && kid->op_targ == OP_NEXTSTATE)
		cop = (COP *)kid;

	    /* Keep searching, and return when we've found something. */

	    new_cop = closest_cop(cop, kid);
	    if (new_cop) return new_cop;
	}
    }

    /* Nothing found. */

    return 0;
}

SV *
Perl_vmess(pTHX_ const char *pat, va_list *args)
{
    SV *sv = mess_alloc();
    static char dgd[] = " during global destruction.\n";
    COP *cop;

    sv_vsetpvfn(sv, pat, strlen(pat), args, Null(SV**), 0, Null(bool*));
    if (!SvCUR(sv) || *(SvEND(sv) - 1) != '\n') {

	/*
	 * Try and find the file and line for PL_op.  This will usually be
	 * PL_curcop, but it might be a cop that has been optimised away.  We
	 * can try to find such a cop by searching through the optree starting
	 * from the sibling of PL_curcop.
	 */

	cop = closest_cop(PL_curcop, PL_curcop->op_sibling);
	if (!cop) cop = PL_curcop;

	if (CopLINE(cop))
	    Perl_sv_catpvf(aTHX_ sv, " at %s line %"IVdf,
	    OutCopFILE(cop), (IV)CopLINE(cop));
	if (GvIO(PL_last_in_gv) && IoLINES(GvIOp(PL_last_in_gv))) {
	    bool line_mode = (RsSIMPLE(PL_rs) &&
			      SvCUR(PL_rs) == 1 && *SvPVX(PL_rs) == '\n');
	    Perl_sv_catpvf(aTHX_ sv, ", <%s> %s %"IVdf,
			   PL_last_in_gv == PL_argvgv ?
			   "" : GvNAME(PL_last_in_gv),
			   line_mode ? "line" : "chunk",
			   (IV)IoLINES(GvIOp(PL_last_in_gv)));
	}
#ifdef USE_5005THREADS
	if (thr->tid)
	    Perl_sv_catpvf(aTHX_ sv, " thread %ld", thr->tid);
#endif
	sv_catpv(sv, PL_dirty ? dgd : ".\n");
    }
    return sv;
}

void
Perl_write_to_stderr(pTHX_ const char* message, int msglen)
{
    IO *io;
    MAGIC *mg;

    if (PL_stderrgv && SvREFCNT(PL_stderrgv) 
	&& (io = GvIO(PL_stderrgv))
	&& (mg = SvTIED_mg((SV*)io, PERL_MAGIC_tiedscalar))) 
    {
	dSP;
	ENTER;
	SAVETMPS;

	save_re_context();
	SAVESPTR(PL_stderrgv);
	PL_stderrgv = Nullgv;

	PUSHSTACKi(PERLSI_MAGIC);

	PUSHMARK(SP);
	EXTEND(SP,2);
	PUSHs(SvTIED_obj((SV*)io, mg));
	PUSHs(sv_2mortal(newSVpvn(message, msglen)));
	PUTBACK;
	call_method("PRINT", G_SCALAR);

	POPSTACK;
	FREETMPS;
	LEAVE;
    }
    else {
#ifdef USE_SFIO
	/* SFIO can really mess with your errno */
	int e = errno;
#endif
	PerlIO *serr = Perl_error_log;

	PERL_WRITE_MSG_TO_CONSOLE(serr, message, msglen);
	(void)PerlIO_flush(serr);
#ifdef USE_SFIO
	errno = e;
#endif
    }
}

OP *
Perl_vdie(pTHX_ const char* pat, va_list *args)
{
    char *message;
    int was_in_eval = PL_in_eval;
    HV *stash;
    GV *gv;
    CV *cv;
    SV *msv;
    STRLEN msglen;
    I32 utf8 = 0;

    DEBUG_S(PerlIO_printf(Perl_debug_log,
			  "%p: die: curstack = %p, mainstack = %p\n",
			  thr, PL_curstack, PL_mainstack));

    if (pat) {
	msv = vmess(pat, args);
	if (PL_errors && SvCUR(PL_errors)) {
	    sv_catsv(PL_errors, msv);
	    message = SvPV(PL_errors, msglen);
	    SvCUR_set(PL_errors, 0);
	}
	else
	    message = SvPV(msv,msglen);
	utf8 = SvUTF8(msv);
    }
    else {
	message = Nullch;
	msglen = 0;
    }

    DEBUG_S(PerlIO_printf(Perl_debug_log,
			  "%p: die: message = %s\ndiehook = %p\n",
			  thr, message, PL_diehook));
    if (PL_diehook) {
	/* sv_2cv might call Perl_croak() */
	SV *olddiehook = PL_diehook;
	ENTER;
	SAVESPTR(PL_diehook);
	PL_diehook = Nullsv;
	cv = sv_2cv(olddiehook, &stash, &gv, 0);
	LEAVE;
	if (cv && !CvDEPTH(cv) && (CvROOT(cv) || CvXSUB(cv))) {
	    dSP;
	    SV *msg;

	    ENTER;
	    save_re_context();
	    if (message) {
		msg = newSVpvn(message, msglen);
		SvFLAGS(msg) |= utf8;
		SvREADONLY_on(msg);
		SAVEFREESV(msg);
	    }
	    else {
		msg = ERRSV;
	    }

	    PUSHSTACKi(PERLSI_DIEHOOK);
	    PUSHMARK(SP);
	    XPUSHs(msg);
	    PUTBACK;
	    call_sv((SV*)cv, G_DISCARD);
	    POPSTACK;
	    LEAVE;
	}
    }

    PL_restartop = die_where(message, msglen);
    SvFLAGS(ERRSV) |= utf8;
    DEBUG_S(PerlIO_printf(Perl_debug_log,
	  "%p: die: restartop = %p, was_in_eval = %d, top_env = %p\n",
	  thr, PL_restartop, was_in_eval, PL_top_env));
    if ((!PL_restartop && was_in_eval) || PL_top_env->je_prev)
	JMPENV_JUMP(3);
    return PL_restartop;
}

#if defined(PERL_IMPLICIT_CONTEXT)
OP *
Perl_die_nocontext(const char* pat, ...)
{
    dTHX;
    OP *o;
    va_list args;
    va_start(args, pat);
    o = vdie(pat, &args);
    va_end(args);
    return o;
}
#endif /* PERL_IMPLICIT_CONTEXT */

OP *
Perl_die(pTHX_ const char* pat, ...)
{
    OP *o;
    va_list args;
    va_start(args, pat);
    o = vdie(pat, &args);
    va_end(args);
    return o;
}

void
Perl_vcroak(pTHX_ const char* pat, va_list *args)
{
    char *message;
    HV *stash;
    GV *gv;
    CV *cv;
    SV *msv;
    STRLEN msglen;
    I32 utf8 = 0;

    if (pat) {
	msv = vmess(pat, args);
	if (PL_errors && SvCUR(PL_errors)) {
	    sv_catsv(PL_errors, msv);
	    message = SvPV(PL_errors, msglen);
	    SvCUR_set(PL_errors, 0);
	}
	else
	    message = SvPV(msv,msglen);
	utf8 = SvUTF8(msv);
    }
    else {
	message = Nullch;
	msglen = 0;
    }

    DEBUG_S(PerlIO_printf(Perl_debug_log, "croak: 0x%"UVxf" %s",
			  PTR2UV(thr), message));

    if (PL_diehook) {
	/* sv_2cv might call Perl_croak() */
	SV *olddiehook = PL_diehook;
	ENTER;
	SAVESPTR(PL_diehook);
	PL_diehook = Nullsv;
	cv = sv_2cv(olddiehook, &stash, &gv, 0);
	LEAVE;
	if (cv && !CvDEPTH(cv) && (CvROOT(cv) || CvXSUB(cv))) {
	    dSP;
	    SV *msg;

	    ENTER;
	    save_re_context();
	    if (message) {
		msg = newSVpvn(message, msglen);
		SvFLAGS(msg) |= utf8;
		SvREADONLY_on(msg);
		SAVEFREESV(msg);
	    }
	    else {
		msg = ERRSV;
	    }

	    PUSHSTACKi(PERLSI_DIEHOOK);
	    PUSHMARK(SP);
	    XPUSHs(msg);
	    PUTBACK;
	    call_sv((SV*)cv, G_DISCARD);
	    POPSTACK;
	    LEAVE;
	}
    }
    if (PL_in_eval) {
	PL_restartop = die_where(message, msglen);
	SvFLAGS(ERRSV) |= utf8;
	JMPENV_JUMP(3);
    }
    else if (!message)
	message = SvPVx(ERRSV, msglen);

    write_to_stderr(message, msglen);
    my_failure_exit();
}

#if defined(PERL_IMPLICIT_CONTEXT)
void
Perl_croak_nocontext(const char *pat, ...)
{
    dTHX;
    va_list args;
    va_start(args, pat);
    vcroak(pat, &args);
    /* NOTREACHED */
    va_end(args);
}
#endif /* PERL_IMPLICIT_CONTEXT */

/*
=head1 Warning and Dieing

=for apidoc croak

This is the XSUB-writer's interface to Perl's C<die> function.
Normally call this function the same way you call the C C<printf>
function.  Calling C<croak> returns control directly to Perl,
sidestepping the normal C order of execution. See C<warn>.

If you want to throw an exception object, assign the object to
C<$@> and then pass C<Nullch> to croak():

   errsv = get_sv("@", TRUE);
   sv_setsv(errsv, exception_object);
   croak(Nullch);

=cut
*/

void
Perl_croak(pTHX_ const char *pat, ...)
{
    va_list args;
    va_start(args, pat);
    vcroak(pat, &args);
    /* NOTREACHED */
    va_end(args);
}

void
Perl_vwarn(pTHX_ const char* pat, va_list *args)
{
    char *message;
    HV *stash;
    GV *gv;
    CV *cv;
    SV *msv;
    STRLEN msglen;
    I32 utf8 = 0;

    msv = vmess(pat, args);
    utf8 = SvUTF8(msv);
    message = SvPV(msv, msglen);

    if (PL_warnhook) {
	/* sv_2cv might call Perl_warn() */
	SV *oldwarnhook = PL_warnhook;
	ENTER;
	SAVESPTR(PL_warnhook);
	PL_warnhook = Nullsv;
	cv = sv_2cv(oldwarnhook, &stash, &gv, 0);
	LEAVE;
	if (cv && !CvDEPTH(cv) && (CvROOT(cv) || CvXSUB(cv))) {
	    dSP;
	    SV *msg;

	    ENTER;
	    save_re_context();
	    msg = newSVpvn(message, msglen);
	    SvFLAGS(msg) |= utf8;
	    SvREADONLY_on(msg);
	    SAVEFREESV(msg);

	    PUSHSTACKi(PERLSI_WARNHOOK);
	    PUSHMARK(SP);
	    XPUSHs(msg);
	    PUTBACK;
	    call_sv((SV*)cv, G_DISCARD);
	    POPSTACK;
	    LEAVE;
	    return;
	}
    }

    write_to_stderr(message, msglen);
}

#if defined(PERL_IMPLICIT_CONTEXT)
void
Perl_warn_nocontext(const char *pat, ...)
{
    dTHX;
    va_list args;
    va_start(args, pat);
    vwarn(pat, &args);
    va_end(args);
}
#endif /* PERL_IMPLICIT_CONTEXT */

/*
=for apidoc warn

This is the XSUB-writer's interface to Perl's C<warn> function.  Call this
function the same way you call the C C<printf> function.  See C<croak>.

=cut
*/

void
Perl_warn(pTHX_ const char *pat, ...)
{
    va_list args;
    va_start(args, pat);
    vwarn(pat, &args);
    va_end(args);
}

#if defined(PERL_IMPLICIT_CONTEXT)
void
Perl_warner_nocontext(U32 err, const char *pat, ...)
{
    dTHX;
    va_list args;
    va_start(args, pat);
    vwarner(err, pat, &args);
    va_end(args);
}
#endif /* PERL_IMPLICIT_CONTEXT */

void
Perl_warner(pTHX_ U32  err, const char* pat,...)
{
    va_list args;
    va_start(args, pat);
    vwarner(err, pat, &args);
    va_end(args);
}

void
Perl_vwarner(pTHX_ U32  err, const char* pat, va_list* args)
{
    char *message;
    HV *stash;
    GV *gv;
    CV *cv;
    SV *msv;
    STRLEN msglen;
    I32 utf8 = 0;

    msv = vmess(pat, args);
    message = SvPV(msv, msglen);
    utf8 = SvUTF8(msv);

    if (ckDEAD(err)) {
#ifdef USE_5005THREADS
	DEBUG_S(PerlIO_printf(Perl_debug_log, "croak: 0x%"UVxf" %s", PTR2UV(thr), message));
#endif /* USE_5005THREADS */
	if (PL_diehook) {
	    /* sv_2cv might call Perl_croak() */
	    SV *olddiehook = PL_diehook;
	    ENTER;
	    SAVESPTR(PL_diehook);
	    PL_diehook = Nullsv;
	    cv = sv_2cv(olddiehook, &stash, &gv, 0);
	    LEAVE;
	    if (cv && !CvDEPTH(cv) && (CvROOT(cv) || CvXSUB(cv))) {
		dSP;
		SV *msg;

		ENTER;
		save_re_context();
		msg = newSVpvn(message, msglen);
		SvFLAGS(msg) |= utf8;
		SvREADONLY_on(msg);
		SAVEFREESV(msg);

		PUSHSTACKi(PERLSI_DIEHOOK);
		PUSHMARK(sp);
		XPUSHs(msg);
		PUTBACK;
		call_sv((SV*)cv, G_DISCARD);
		POPSTACK;
		LEAVE;
	    }
	}
	if (PL_in_eval) {
	    PL_restartop = die_where(message, msglen);
	    SvFLAGS(ERRSV) |= utf8;
	    JMPENV_JUMP(3);
	}
	write_to_stderr(message, msglen);
	my_failure_exit();
    }
    else {
	if (PL_warnhook) {
	    /* sv_2cv might call Perl_warn() */
	    SV *oldwarnhook = PL_warnhook;
	    ENTER;
	    SAVESPTR(PL_warnhook);
	    PL_warnhook = Nullsv;
	    cv = sv_2cv(oldwarnhook, &stash, &gv, 0);
	    LEAVE;
	    if (cv && !CvDEPTH(cv) && (CvROOT(cv) || CvXSUB(cv))) {
		dSP;
		SV *msg;

		ENTER;
		save_re_context();
		msg = newSVpvn(message, msglen);
		SvFLAGS(msg) |= utf8;
		SvREADONLY_on(msg);
		SAVEFREESV(msg);

		PUSHSTACKi(PERLSI_WARNHOOK);
		PUSHMARK(sp);
		XPUSHs(msg);
		PUTBACK;
		call_sv((SV*)cv, G_DISCARD);
		POPSTACK;
		LEAVE;
		return;
	    }
	}
	write_to_stderr(message, msglen);
    }
}

/* since we've already done strlen() for both nam and val
 * we can use that info to make things faster than
 * sprintf(s, "%s=%s", nam, val)
 */
#define my_setenv_format(s, nam, nlen, val, vlen) \
   Copy(nam, s, nlen, char); \
   *(s+nlen) = '='; \
   Copy(val, s+(nlen+1), vlen, char); \
   *(s+(nlen+1+vlen)) = '\0'

#ifdef USE_ENVIRON_ARRAY
       /* VMS' my_setenv() is in vms.c */
#if !defined(WIN32) && !defined(NETWARE)
void
Perl_my_setenv(pTHX_ char *nam, char *val)
{
#ifdef USE_ITHREADS
  /* only parent thread can modify process environment */
  if (PL_curinterp == aTHX)
#endif
  {
#ifndef PERL_USE_SAFE_PUTENV
    /* most putenv()s leak, so we manipulate environ directly */
    register I32 i=setenv_getix(nam);		/* where does it go? */
    int nlen, vlen;

    if (environ == PL_origenviron) {	/* need we copy environment? */
	I32 j;
	I32 max;
	char **tmpenv;

	/*SUPPRESS 530*/
	for (max = i; environ[max]; max++) ;
	tmpenv = (char**)safesysmalloc((max+2) * sizeof(char*));
	for (j=0; j<max; j++) {		/* copy environment */
	    int len = strlen(environ[j]);
	    tmpenv[j] = (char*)safesysmalloc((len+1)*sizeof(char));
	    Copy(environ[j], tmpenv[j], len+1, char);
	}
	tmpenv[max] = Nullch;
	environ = tmpenv;		/* tell exec where it is now */
    }
    if (!val) {
	safesysfree(environ[i]);
	while (environ[i]) {
	    environ[i] = environ[i+1];
	    i++;
	}
	return;
    }
    if (!environ[i]) {			/* does not exist yet */
	environ = (char**)safesysrealloc(environ, (i+2) * sizeof(char*));
	environ[i+1] = Nullch;	/* make sure it's null terminated */
    }
    else
	safesysfree(environ[i]);
    nlen = strlen(nam);
    vlen = strlen(val);

    environ[i] = (char*)safesysmalloc((nlen+vlen+2) * sizeof(char));
    /* all that work just for this */
    my_setenv_format(environ[i], nam, nlen, val, vlen);

#else   /* PERL_USE_SAFE_PUTENV */
#   if defined(__CYGWIN__) || defined( EPOC)
    setenv(nam, val, 1);
#   else
    char *new_env;
    int nlen = strlen(nam), vlen;
    if (!val) {
	val = "";
    }
    vlen = strlen(val);
    new_env = (char*)safesysmalloc((nlen + vlen + 2) * sizeof(char));
    /* all that work just for this */
    my_setenv_format(new_env, nam, nlen, val, vlen);
    (void)putenv(new_env);
#   endif /* __CYGWIN__ */
#endif  /* PERL_USE_SAFE_PUTENV */
  }
}

#else /* WIN32 || NETWARE */

void
Perl_my_setenv(pTHX_ char *nam,char *val)
{
    register char *envstr;
    int nlen = strlen(nam), vlen;

    if (!val) {
	val = "";
    }
    vlen = strlen(val);
    New(904, envstr, nlen+vlen+2, char);
    my_setenv_format(envstr, nam, nlen, val, vlen);
    (void)PerlEnv_putenv(envstr);
    Safefree(envstr);
}

#endif /* WIN32 || NETWARE */

#ifndef PERL_MICRO
I32
Perl_setenv_getix(pTHX_ char *nam)
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
#endif /* !PERL_MICRO */

#endif /* !VMS && !EPOC*/

#ifdef UNLINK_ALL_VERSIONS
I32
Perl_unlnk(pTHX_ char *f)	/* unlink all versions of a file */
{
    I32 i;

    for (i = 0; PerlLIO_unlink(f) >= 0; i++) ;
    return i ? 0 : -1;
}
#endif

/* this is a drop-in replacement for bcopy() */
#if (!defined(HAS_MEMCPY) && !defined(HAS_BCOPY)) || (!defined(HAS_MEMMOVE) && !defined(HAS_SAFE_MEMCPY) && !defined(HAS_SAFE_BCOPY))
char *
Perl_my_bcopy(register const char *from,register char *to,register I32 len)
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

/* this is a drop-in replacement for memset() */
#ifndef HAS_MEMSET
void *
Perl_my_memset(register char *loc, register I32 ch, register I32 len)
{
    char *retval = loc;

    while (len--)
	*loc++ = ch;
    return retval;
}
#endif

/* this is a drop-in replacement for bzero() */
#if !defined(HAS_BZERO) && !defined(HAS_MEMSET)
char *
Perl_my_bzero(register char *loc, register I32 len)
{
    char *retval = loc;

    while (len--)
	*loc++ = 0;
    return retval;
}
#endif

/* this is a drop-in replacement for memcmp() */
#if !defined(HAS_MEMCMP) || !defined(HAS_SANE_MEMCMP)
I32
Perl_my_memcmp(const char *s1, const char *s2, register I32 len)
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

#ifndef HAS_VPRINTF

#ifdef USE_CHAR_VSPRINTF
char *
#else
int
#endif
vsprintf(char *dest, const char *pat, char *args)
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

#ifdef MYSWAP
#if BYTEORDER != 0x4321
short
Perl_my_swap(pTHX_ short s)
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
Perl_my_htonl(pTHX_ long l)
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
    Perl_croak(aTHX_ "Unknown BYTEORDER\n");
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
Perl_my_ntohl(pTHX_ long l)
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
    Perl_croak(aTHX_ "Unknown BYTEORDER\n");
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
	name (register type n)					\
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
	name (register type n)					\
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

PerlIO *
Perl_my_popen_list(pTHX_ char *mode, int n, SV **args)
{
#if (!defined(DOSISH) || defined(HAS_FORK) || defined(AMIGAOS)) && !defined(OS2) && !defined(VMS) && !defined(__OPEN_VM) && !defined(EPOC) && !defined(MACOS_TRADITIONAL) && !defined(NETWARE)
    int p[2];
    register I32 This, that;
    register Pid_t pid;
    SV *sv;
    I32 did_pipes = 0;
    int pp[2];

    PERL_FLUSHALL_FOR_CHILD;
    This = (*mode == 'w');
    that = !This;
    if (PL_tainting) {
	taint_env();
	taint_proper("Insecure %s%s", "EXEC");
    }
    if (PerlProc_pipe(p) < 0)
	return Nullfp;
    /* Try for another pipe pair for error return */
    if (PerlProc_pipe(pp) >= 0)
	did_pipes = 1;
    while ((pid = PerlProc_fork()) < 0) {
	if (errno != EAGAIN) {
	    PerlLIO_close(p[This]);
	    PerlLIO_close(p[that]);
	    if (did_pipes) {
		PerlLIO_close(pp[0]);
		PerlLIO_close(pp[1]);
	    }
	    return Nullfp;
	}
	sleep(5);
    }
    if (pid == 0) {
	/* Child */
#undef THIS
#undef THAT
#define THIS that
#define THAT This
	/* Close parent's end of error status pipe (if any) */
	if (did_pipes) {
	    PerlLIO_close(pp[0]);
#if defined(HAS_FCNTL) && defined(F_SETFD)
	    /* Close error pipe automatically if exec works */
	    fcntl(pp[1], F_SETFD, FD_CLOEXEC);
#endif
	}
	/* Now dup our end of _the_ pipe to right position */
	if (p[THIS] != (*mode == 'r')) {
	    PerlLIO_dup2(p[THIS], *mode == 'r');
	    PerlLIO_close(p[THIS]);
	    if (p[THAT] != (*mode == 'r'))	/* if dup2() didn't close it */
		PerlLIO_close(p[THAT]);	/* close parent's end of _the_ pipe */
	}
	else
	    PerlLIO_close(p[THAT]);	/* close parent's end of _the_ pipe */
#if !defined(HAS_FCNTL) || !defined(F_SETFD)
	/* No automatic close - do it by hand */
#  ifndef NOFILE
#  define NOFILE 20
#  endif
	{
	    int fd;

	    for (fd = PL_maxsysfd + 1; fd < NOFILE; fd++) {
		if (fd != pp[1])
		    PerlLIO_close(fd);
	    }
	}
#endif
	do_aexec5(Nullsv, args-1, args-1+n, pp[1], did_pipes);
	PerlProc__exit(1);
#undef THIS
#undef THAT
    }
    /* Parent */
    do_execfree();	/* free any memory malloced by child on fork */
    if (did_pipes)
	PerlLIO_close(pp[1]);
    /* Keep the lower of the two fd numbers */
    if (p[that] < p[This]) {
	PerlLIO_dup2(p[This], p[that]);
	PerlLIO_close(p[This]);
	p[This] = p[that];
    }
    else
	PerlLIO_close(p[that]);		/* close child's end of pipe */

    LOCK_FDPID_MUTEX;
    sv = *av_fetch(PL_fdpid,p[This],TRUE);
    UNLOCK_FDPID_MUTEX;
    (void)SvUPGRADE(sv,SVt_IV);
    SvIVX(sv) = pid;
    PL_forkprocess = pid;
    /* If we managed to get status pipe check for exec fail */
    if (did_pipes && pid > 0) {
	int errkid;
	int n = 0, n1;

	while (n < sizeof(int)) {
	    n1 = PerlLIO_read(pp[0],
			      (void*)(((char*)&errkid)+n),
			      (sizeof(int)) - n);
	    if (n1 <= 0)
		break;
	    n += n1;
	}
	PerlLIO_close(pp[0]);
	did_pipes = 0;
	if (n) {			/* Error */
	    int pid2, status;
	    PerlLIO_close(p[This]);
	    if (n != sizeof(int))
		Perl_croak(aTHX_ "panic: kid popen errno read");
	    do {
		pid2 = wait4pid(pid, &status, 0);
	    } while (pid2 == -1 && errno == EINTR);
	    errno = errkid;		/* Propagate errno from kid */
	    return Nullfp;
	}
    }
    if (did_pipes)
	 PerlLIO_close(pp[0]);
    return PerlIO_fdopen(p[This], mode);
#else
    Perl_croak(aTHX_ "List form of piped open not implemented");
    return (PerlIO *) NULL;
#endif
}

    /* VMS' my_popen() is in VMS.c, same with OS/2. */
#if (!defined(DOSISH) || defined(HAS_FORK) || defined(AMIGAOS)) && !defined(VMS) && !defined(__OPEN_VM) && !defined(EPOC) && !defined(MACOS_TRADITIONAL)
PerlIO *
Perl_my_popen(pTHX_ char *cmd, char *mode)
{
    int p[2];
    register I32 This, that;
    register Pid_t pid;
    SV *sv;
    I32 doexec = strNE(cmd,"-");
    I32 did_pipes = 0;
    int pp[2];

    PERL_FLUSHALL_FOR_CHILD;
#ifdef OS2
    if (doexec) {
	return my_syspopen(aTHX_ cmd,mode);
    }
#endif
    This = (*mode == 'w');
    that = !This;
    if (doexec && PL_tainting) {
	taint_env();
	taint_proper("Insecure %s%s", "EXEC");
    }
    if (PerlProc_pipe(p) < 0)
	return Nullfp;
    if (doexec && PerlProc_pipe(pp) >= 0)
	did_pipes = 1;
    while ((pid = PerlProc_fork()) < 0) {
	if (errno != EAGAIN) {
	    PerlLIO_close(p[This]);
	    PerlLIO_close(p[that]);
	    if (did_pipes) {
		PerlLIO_close(pp[0]);
		PerlLIO_close(pp[1]);
	    }
	    if (!doexec)
		Perl_croak(aTHX_ "Can't fork");
	    return Nullfp;
	}
	sleep(5);
    }
    if (pid == 0) {
	GV* tmpgv;

#undef THIS
#undef THAT
#define THIS that
#define THAT This
	if (did_pipes) {
	    PerlLIO_close(pp[0]);
#if defined(HAS_FCNTL) && defined(F_SETFD)
	    fcntl(pp[1], F_SETFD, FD_CLOEXEC);
#endif
	}
	if (p[THIS] != (*mode == 'r')) {
	    PerlLIO_dup2(p[THIS], *mode == 'r');
	    PerlLIO_close(p[THIS]);
	    if (p[THAT] != (*mode == 'r'))	/* if dup2() didn't close it */
		PerlLIO_close(p[THAT]);
	}
	else
	    PerlLIO_close(p[THAT]);
#ifndef OS2
	if (doexec) {
#if !defined(HAS_FCNTL) || !defined(F_SETFD)
	    int fd;

#ifndef NOFILE
#define NOFILE 20
#endif
	    {
		int fd;

		for (fd = PL_maxsysfd + 1; fd < NOFILE; fd++)
		    if (fd != pp[1])
			PerlLIO_close(fd);
	    }
#endif
	    /* may or may not use the shell */
	    do_exec3(cmd, pp[1], did_pipes);
	    PerlProc__exit(1);
	}
#endif	/* defined OS2 */
	/*SUPPRESS 560*/
	if ((tmpgv = gv_fetchpv("$",TRUE, SVt_PV))) {
	    SvREADONLY_off(GvSV(tmpgv));
	    sv_setiv(GvSV(tmpgv), PerlProc_getpid());
	    SvREADONLY_on(GvSV(tmpgv));
	}
#ifdef THREADS_HAVE_PIDS
	PL_ppid = (IV)getppid();
#endif
	PL_forkprocess = 0;
	hv_clear(PL_pidstatus);	/* we have no children */
	return Nullfp;
#undef THIS
#undef THAT
    }
    do_execfree();	/* free any memory malloced by child on vfork */
    if (did_pipes)
	PerlLIO_close(pp[1]);
    if (p[that] < p[This]) {
	PerlLIO_dup2(p[This], p[that]);
	PerlLIO_close(p[This]);
	p[This] = p[that];
    }
    else
	PerlLIO_close(p[that]);

    LOCK_FDPID_MUTEX;
    sv = *av_fetch(PL_fdpid,p[This],TRUE);
    UNLOCK_FDPID_MUTEX;
    (void)SvUPGRADE(sv,SVt_IV);
    SvIVX(sv) = pid;
    PL_forkprocess = pid;
    if (did_pipes && pid > 0) {
	int errkid;
	int n = 0, n1;

	while (n < sizeof(int)) {
	    n1 = PerlLIO_read(pp[0],
			      (void*)(((char*)&errkid)+n),
			      (sizeof(int)) - n);
	    if (n1 <= 0)
		break;
	    n += n1;
	}
	PerlLIO_close(pp[0]);
	did_pipes = 0;
	if (n) {			/* Error */
	    int pid2, status;
	    PerlLIO_close(p[This]);
	    if (n != sizeof(int))
		Perl_croak(aTHX_ "panic: kid popen errno read");
	    do {
		pid2 = wait4pid(pid, &status, 0);
	    } while (pid2 == -1 && errno == EINTR);
	    errno = errkid;		/* Propagate errno from kid */
	    return Nullfp;
	}
    }
    if (did_pipes)
	 PerlLIO_close(pp[0]);
    return PerlIO_fdopen(p[This], mode);
}
#else
#if defined(atarist) || defined(EPOC)
FILE *popen();
PerlIO *
Perl_my_popen(pTHX_ char *cmd, char *mode)
{
    PERL_FLUSHALL_FOR_CHILD;
    /* Call system's popen() to get a FILE *, then import it.
       used 0 for 2nd parameter to PerlIO_importFILE;
       apparently not used
    */
    return PerlIO_importFILE(popen(cmd, mode), 0);
}
#else
#if defined(DJGPP)
FILE *djgpp_popen();
PerlIO *
Perl_my_popen(pTHX_ char *cmd, char *mode)
{
    PERL_FLUSHALL_FOR_CHILD;
    /* Call system's popen() to get a FILE *, then import it.
       used 0 for 2nd parameter to PerlIO_importFILE;
       apparently not used
    */
    return PerlIO_importFILE(djgpp_popen(cmd, mode), 0);
}
#endif
#endif

#endif /* !DOSISH */

/* this is called in parent before the fork() */
void
Perl_atfork_lock(void)
{
#if defined(USE_5005THREADS) || defined(USE_ITHREADS)
    /* locks must be held in locking order (if any) */
#  ifdef MYMALLOC
    MUTEX_LOCK(&PL_malloc_mutex);
#  endif
    OP_REFCNT_LOCK;
#endif
}

/* this is called in both parent and child after the fork() */
void
Perl_atfork_unlock(void)
{
#if defined(USE_5005THREADS) || defined(USE_ITHREADS)
    /* locks must be released in same order as in atfork_lock() */
#  ifdef MYMALLOC
    MUTEX_UNLOCK(&PL_malloc_mutex);
#  endif
    OP_REFCNT_UNLOCK;
#endif
}

Pid_t
Perl_my_fork(void)
{
#if defined(HAS_FORK)
    Pid_t pid;
#if (defined(USE_5005THREADS) || defined(USE_ITHREADS)) && !defined(HAS_PTHREAD_ATFORK)
    atfork_lock();
    pid = fork();
    atfork_unlock();
#else
    /* atfork_lock() and atfork_unlock() are installed as pthread_atfork()
     * handlers elsewhere in the code */
    pid = fork();
#endif
    return pid;
#else
    /* this "canna happen" since nothing should be calling here if !HAS_FORK */
    Perl_croak_nocontext("fork() not available");
    return 0;
#endif /* HAS_FORK */
}

#ifdef DUMP_FDS
void
Perl_dump_fds(pTHX_ char *s)
{
    int fd;
    Stat_t tmpstatbuf;

    PerlIO_printf(Perl_debug_log,"%s", s);
    for (fd = 0; fd < 32; fd++) {
	if (PerlLIO_fstat(fd,&tmpstatbuf) >= 0)
	    PerlIO_printf(Perl_debug_log," %d",fd);
    }
    PerlIO_printf(Perl_debug_log,"\n");
}
#endif	/* DUMP_FDS */

#ifndef HAS_DUP2
int
dup2(int oldfd, int newfd)
{
#if defined(HAS_FCNTL) && defined(F_DUPFD)
    if (oldfd == newfd)
	return oldfd;
    PerlLIO_close(newfd);
    return fcntl(oldfd, F_DUPFD, newfd);
#else
#define DUP2_MAX_FDS 256
    int fdtmp[DUP2_MAX_FDS];
    I32 fdx = 0;
    int fd;

    if (oldfd == newfd)
	return oldfd;
    PerlLIO_close(newfd);
    /* good enough for low fd's... */
    while ((fd = PerlLIO_dup(oldfd)) != newfd && fd >= 0) {
	if (fdx >= DUP2_MAX_FDS) {
	    PerlLIO_close(fd);
	    fd = -1;
	    break;
	}
	fdtmp[fdx++] = fd;
    }
    while (fdx > 0)
	PerlLIO_close(fdtmp[--fdx]);
    return fd;
#endif
}
#endif

#ifndef PERL_MICRO
#ifdef HAS_SIGACTION

#ifdef MACOS_TRADITIONAL
/* We don't want restart behavior on MacOS */
#undef SA_RESTART
#endif

Sighandler_t
Perl_rsignal(pTHX_ int signo, Sighandler_t handler)
{
    struct sigaction act, oact;

#ifdef USE_ITHREADS
    /* only "parent" interpreter can diddle signals */
    if (PL_curinterp != aTHX)
	return SIG_ERR;
#endif

    act.sa_handler = handler;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
#ifdef SA_RESTART
    if (PL_signals & PERL_SIGNALS_UNSAFE_FLAG)
        act.sa_flags |= SA_RESTART;	/* SVR4, 4.3+BSD */
#endif
#if defined(SA_NOCLDWAIT) && !defined(BSDish) /* See [perl #18849] */
    if (signo == SIGCHLD && handler == (Sighandler_t)SIG_IGN)
	act.sa_flags |= SA_NOCLDWAIT;
#endif
    if (sigaction(signo, &act, &oact) == -1)
    	return SIG_ERR;
    else
    	return oact.sa_handler;
}

Sighandler_t
Perl_rsignal_state(pTHX_ int signo)
{
    struct sigaction oact;

    if (sigaction(signo, (struct sigaction *)NULL, &oact) == -1)
	return SIG_ERR;
    else
	return oact.sa_handler;
}

int
Perl_rsignal_save(pTHX_ int signo, Sighandler_t handler, Sigsave_t *save)
{
    struct sigaction act;

#ifdef USE_ITHREADS
    /* only "parent" interpreter can diddle signals */
    if (PL_curinterp != aTHX)
	return -1;
#endif

    act.sa_handler = handler;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
#ifdef SA_RESTART
    if (PL_signals & PERL_SIGNALS_UNSAFE_FLAG)
        act.sa_flags |= SA_RESTART;	/* SVR4, 4.3+BSD */
#endif
#if defined(SA_NOCLDWAIT) && !defined(BSDish) /* See [perl #18849] */
    if (signo == SIGCHLD && handler == (Sighandler_t)SIG_IGN)
	act.sa_flags |= SA_NOCLDWAIT;
#endif
    return sigaction(signo, &act, save);
}

int
Perl_rsignal_restore(pTHX_ int signo, Sigsave_t *save)
{
#ifdef USE_ITHREADS
    /* only "parent" interpreter can diddle signals */
    if (PL_curinterp != aTHX)
	return -1;
#endif

    return sigaction(signo, save, (struct sigaction *)NULL);
}

#else /* !HAS_SIGACTION */

Sighandler_t
Perl_rsignal(pTHX_ int signo, Sighandler_t handler)
{
#if defined(USE_ITHREADS) && !defined(WIN32)
    /* only "parent" interpreter can diddle signals */
    if (PL_curinterp != aTHX)
	return SIG_ERR;
#endif

    return PerlProc_signal(signo, handler);
}

static int sig_trapped;	/* XXX signals are process-wide anyway, so we
			   ignore the implications of this for threading */

static
Signal_t
sig_trap(int signo)
{
    sig_trapped++;
}

Sighandler_t
Perl_rsignal_state(pTHX_ int signo)
{
    Sighandler_t oldsig;

#if defined(USE_ITHREADS) && !defined(WIN32)
    /* only "parent" interpreter can diddle signals */
    if (PL_curinterp != aTHX)
	return SIG_ERR;
#endif

    sig_trapped = 0;
    oldsig = PerlProc_signal(signo, sig_trap);
    PerlProc_signal(signo, oldsig);
    if (sig_trapped)
	PerlProc_kill(PerlProc_getpid(), signo);
    return oldsig;
}

int
Perl_rsignal_save(pTHX_ int signo, Sighandler_t handler, Sigsave_t *save)
{
#if defined(USE_ITHREADS) && !defined(WIN32)
    /* only "parent" interpreter can diddle signals */
    if (PL_curinterp != aTHX)
	return -1;
#endif
    *save = PerlProc_signal(signo, handler);
    return (*save == SIG_ERR) ? -1 : 0;
}

int
Perl_rsignal_restore(pTHX_ int signo, Sigsave_t *save)
{
#if defined(USE_ITHREADS) && !defined(WIN32)
    /* only "parent" interpreter can diddle signals */
    if (PL_curinterp != aTHX)
	return -1;
#endif
    return (PerlProc_signal(signo, *save) == SIG_ERR) ? -1 : 0;
}

#endif /* !HAS_SIGACTION */
#endif /* !PERL_MICRO */

    /* VMS' my_pclose() is in VMS.c; same with OS/2 */
#if (!defined(DOSISH) || defined(HAS_FORK) || defined(AMIGAOS)) && !defined(VMS) && !defined(__OPEN_VM) && !defined(EPOC) && !defined(MACOS_TRADITIONAL)
I32
Perl_my_pclose(pTHX_ PerlIO *ptr)
{
    Sigsave_t hstat, istat, qstat;
    int status;
    SV **svp;
    Pid_t pid;
    Pid_t pid2;
    bool close_failed;
    int saved_errno = 0;
#ifdef VMS
    int saved_vaxc_errno;
#endif
#ifdef WIN32
    int saved_win32_errno;
#endif

    LOCK_FDPID_MUTEX;
    svp = av_fetch(PL_fdpid,PerlIO_fileno(ptr),TRUE);
    UNLOCK_FDPID_MUTEX;
    pid = (SvTYPE(*svp) == SVt_IV) ? SvIVX(*svp) : -1;
    SvREFCNT_dec(*svp);
    *svp = &PL_sv_undef;
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
#ifdef WIN32
	saved_win32_errno = GetLastError();
#endif
    }
#ifdef UTS
    if(PerlProc_kill(pid, 0) < 0) { return(pid); }   /* HOM 12/23/91 */
#endif
#ifndef PERL_MICRO
    rsignal_save(SIGHUP, SIG_IGN, &hstat);
    rsignal_save(SIGINT, SIG_IGN, &istat);
    rsignal_save(SIGQUIT, SIG_IGN, &qstat);
#endif
    do {
	pid2 = wait4pid(pid, &status, 0);
    } while (pid2 == -1 && errno == EINTR);
#ifndef PERL_MICRO
    rsignal_restore(SIGHUP, &hstat);
    rsignal_restore(SIGINT, &istat);
    rsignal_restore(SIGQUIT, &qstat);
#endif
    if (close_failed) {
	SETERRNO(saved_errno, saved_vaxc_errno);
	return -1;
    }
    return(pid2 < 0 ? pid2 : status == 0 ? 0 : (errno = 0, status));
}
#endif /* !DOSISH */

#if  (!defined(DOSISH) || defined(OS2) || defined(WIN32) || defined(NETWARE)) && !defined(MACOS_TRADITIONAL)
I32
Perl_wait4pid(pTHX_ Pid_t pid, int *statusp, int flags)
{
    I32 result;
    if (!pid)
	return -1;
#if !defined(HAS_WAITPID) && !defined(HAS_WAIT4) || defined(HAS_WAITPID_RUNTIME)
    {
	SV *sv;
	SV** svp;
	char spid[TYPE_CHARS(int)];

	if (pid > 0) {
	    sprintf(spid, "%"IVdf, (IV)pid);
	    svp = hv_fetch(PL_pidstatus,spid,strlen(spid),FALSE);
	    if (svp && *svp != &PL_sv_undef) {
		*statusp = SvIVX(*svp);
		(void)hv_delete(PL_pidstatus,spid,strlen(spid),G_DISCARD);
		return pid;
	    }
	}
	else {
	    HE *entry;

	    hv_iterinit(PL_pidstatus);
	    if ((entry = hv_iternext(PL_pidstatus))) {
		SV *sv;
		char spid[TYPE_CHARS(int)];

		pid = atoi(hv_iterkey(entry,(I32*)statusp));
		sv = hv_iterval(PL_pidstatus,entry);
		*statusp = SvIVX(sv);
		sprintf(spid, "%"IVdf, (IV)pid);
		(void)hv_delete(PL_pidstatus,spid,strlen(spid),G_DISCARD);
		return pid;
	    }
	}
    }
#endif
#ifdef HAS_WAITPID
#  ifdef HAS_WAITPID_RUNTIME
    if (!HAS_WAITPID_RUNTIME)
	goto hard_way;
#  endif
    result = PerlProc_waitpid(pid,statusp,flags);
    goto finish;
#endif
#if !defined(HAS_WAITPID) && defined(HAS_WAIT4)
    result = wait4((pid==-1)?0:pid,statusp,flags,Null(struct rusage *));
    goto finish;
#endif
#if !defined(HAS_WAITPID) && !defined(HAS_WAIT4) || defined(HAS_WAITPID_RUNTIME)
  hard_way:
    {
	if (flags)
	    Perl_croak(aTHX_ "Can't do waitpid with flags");
	else {
	    while ((result = PerlProc_wait(statusp)) != pid && pid > 0 && result >= 0)
		pidgone(result,*statusp);
	    if (result < 0)
		*statusp = -1;
	}
    }
#endif
  finish:
    if (result < 0 && errno == EINTR) {
	PERL_ASYNC_CHECK();
    }
    return result;
}
#endif /* !DOSISH || OS2 || WIN32 || NETWARE */

void
/*SUPPRESS 590*/
Perl_pidgone(pTHX_ Pid_t pid, int status)
{
    register SV *sv;
    char spid[TYPE_CHARS(int)];

    sprintf(spid, "%"IVdf, (IV)pid);
    sv = *hv_fetch(PL_pidstatus,spid,strlen(spid),TRUE);
    (void)SvUPGRADE(sv,SVt_IV);
    SvIVX(sv) = status;
    return;
}

#if defined(atarist) || defined(OS2) || defined(EPOC)
int pclose();
#ifdef HAS_FORK
int					/* Cannot prototype with I32
					   in os2ish.h. */
my_syspclose(PerlIO *ptr)
#else
I32
Perl_my_pclose(pTHX_ PerlIO *ptr)
#endif
{
    /* Needs work for PerlIO ! */
    FILE *f = PerlIO_findFILE(ptr);
    I32 result = pclose(f);
    PerlIO_releaseFILE(ptr,f);
    return result;
}
#endif

#if defined(DJGPP)
int djgpp_pclose();
I32
Perl_my_pclose(pTHX_ PerlIO *ptr)
{
    /* Needs work for PerlIO ! */
    FILE *f = PerlIO_findFILE(ptr);
    I32 result = djgpp_pclose(f);
    result = (result << 8) & 0xff00;
    PerlIO_releaseFILE(ptr,f);
    return result;
}
#endif

void
Perl_repeatcpy(pTHX_ register char *to, register const char *from, I32 len, register I32 count)
{
    register I32 todo;
    register const char *frombase = from;

    if (len == 1) {
	register const char c = *from;
	while (count-- > 0)
	    *to++ = c;
	return;
    }
    while (count-- > 0) {
	for (todo = len; todo > 0; todo--) {
	    *to++ = *from++;
	}
	from = frombase;
    }
}

#ifndef HAS_RENAME
I32
Perl_same_dirent(pTHX_ char *a, char *b)
{
    char *fa = strrchr(a,'/');
    char *fb = strrchr(b,'/');
    Stat_t tmpstatbuf1;
    Stat_t tmpstatbuf2;
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
    if (PerlLIO_stat(SvPVX(tmpsv), &tmpstatbuf1) < 0)
	return FALSE;
    if (fb == b)
	sv_setpv(tmpsv, ".");
    else
	sv_setpvn(tmpsv, b, fb - b);
    if (PerlLIO_stat(SvPVX(tmpsv), &tmpstatbuf2) < 0)
	return FALSE;
    return tmpstatbuf1.st_dev == tmpstatbuf2.st_dev &&
	   tmpstatbuf1.st_ino == tmpstatbuf2.st_ino;
}
#endif /* !HAS_RENAME */

char*
Perl_find_script(pTHX_ char *scriptname, bool dosearch, char **search_ext, I32 flags)
{
    char *xfound = Nullch;
    char *xfailed = Nullch;
    char tmpbuf[MAXPATHLEN];
    register char *s;
    I32 len = 0;
    int retval;
#if defined(DOSISH) && !defined(OS2) && !defined(atarist)
#  define SEARCH_EXTS ".bat", ".cmd", NULL
#  define MAX_EXT_LEN 4
#endif
#ifdef OS2
#  define SEARCH_EXTS ".cmd", ".btm", ".bat", ".pl", NULL
#  define MAX_EXT_LEN 4
#endif
#ifdef VMS
#  define SEARCH_EXTS ".pl", ".com", NULL
#  define MAX_EXT_LEN 4
#endif
    /* additional extensions to try in each dir if scriptname not found */
#ifdef SEARCH_EXTS
    char *exts[] = { SEARCH_EXTS };
    char **ext = search_ext ? search_ext : exts;
    int extidx = 0, i = 0;
    char *curext = Nullch;
#else
#  define MAX_EXT_LEN 0
#endif

    /*
     * If dosearch is true and if scriptname does not contain path
     * delimiters, search the PATH for scriptname.
     *
     * If SEARCH_EXTS is also defined, will look for each
     * scriptname{SEARCH_EXTS} whenever scriptname is not found
     * while searching the PATH.
     *
     * Assuming SEARCH_EXTS is C<".foo",".bar",NULL>, PATH search
     * proceeds as follows:
     *   If DOSISH or VMSISH:
     *     + look for ./scriptname{,.foo,.bar}
     *     + search the PATH for scriptname{,.foo,.bar}
     *
     *   If !DOSISH:
     *     + look *only* in the PATH for scriptname{,.foo,.bar} (note
     *       this will not look in '.' if it's not in the PATH)
     */
    tmpbuf[0] = '\0';

#ifdef VMS
#  ifdef ALWAYS_DEFTYPES
    len = strlen(scriptname);
    if (!(len == 1 && *scriptname == '-') && scriptname[len-1] != ':') {
	int hasdir, idx = 0, deftypes = 1;
	bool seen_dot = 1;

	hasdir = !dosearch || (strpbrk(scriptname,":[</") != Nullch) ;
#  else
    if (dosearch) {
	int hasdir, idx = 0, deftypes = 1;
	bool seen_dot = 1;

	hasdir = (strpbrk(scriptname,":[</") != Nullch) ;
#  endif
	/* The first time through, just add SEARCH_EXTS to whatever we
	 * already have, so we can check for default file types. */
	while (deftypes ||
	       (!hasdir && my_trnlnm("DCL$PATH",tmpbuf,idx++)) )
	{
	    if (deftypes) {
		deftypes = 0;
		*tmpbuf = '\0';
	    }
	    if ((strlen(tmpbuf) + strlen(scriptname)
		 + MAX_EXT_LEN) >= sizeof tmpbuf)
		continue;	/* don't search dir with too-long name */
	    strcat(tmpbuf, scriptname);
#else  /* !VMS */

#ifdef DOSISH
    if (strEQ(scriptname, "-"))
 	dosearch = 0;
    if (dosearch) {		/* Look in '.' first. */
	char *cur = scriptname;
#ifdef SEARCH_EXTS
	if ((curext = strrchr(scriptname,'.')))	/* possible current ext */
	    while (ext[i])
		if (strEQ(ext[i++],curext)) {
		    extidx = -1;		/* already has an ext */
		    break;
		}
	do {
#endif
	    DEBUG_p(PerlIO_printf(Perl_debug_log,
				  "Looking for %s\n",cur));
	    if (PerlLIO_stat(cur,&PL_statbuf) >= 0
		&& !S_ISDIR(PL_statbuf.st_mode)) {
		dosearch = 0;
		scriptname = cur;
#ifdef SEARCH_EXTS
		break;
#endif
	    }
#ifdef SEARCH_EXTS
	    if (cur == scriptname) {
		len = strlen(scriptname);
		if (len+MAX_EXT_LEN+1 >= sizeof(tmpbuf))
		    break;
		cur = strcpy(tmpbuf, scriptname);
	    }
	} while (extidx >= 0 && ext[extidx]	/* try an extension? */
		 && strcpy(tmpbuf+len, ext[extidx++]));
#endif
    }
#endif

#ifdef MACOS_TRADITIONAL
    if (dosearch && !strchr(scriptname, ':') &&
	(s = PerlEnv_getenv("Commands")))
#else
    if (dosearch && !strchr(scriptname, '/')
#ifdef DOSISH
		 && !strchr(scriptname, '\\')
#endif
		 && (s = PerlEnv_getenv("PATH")))
#endif
    {
	bool seen_dot = 0;
	
	PL_bufend = s + strlen(s);
	while (s < PL_bufend) {
#ifdef MACOS_TRADITIONAL
	    s = delimcpy(tmpbuf, tmpbuf + sizeof tmpbuf, s, PL_bufend,
			',',
			&len);
#else
#if defined(atarist) || defined(DOSISH)
	    for (len = 0; *s
#  ifdef atarist
		    && *s != ','
#  endif
		    && *s != ';'; len++, s++) {
		if (len < sizeof tmpbuf)
		    tmpbuf[len] = *s;
	    }
	    if (len < sizeof tmpbuf)
		tmpbuf[len] = '\0';
#else  /* ! (atarist || DOSISH) */
	    s = delimcpy(tmpbuf, tmpbuf + sizeof tmpbuf, s, PL_bufend,
			':',
			&len);
#endif /* ! (atarist || DOSISH) */
#endif /* MACOS_TRADITIONAL */
	    if (s < PL_bufend)
		s++;
	    if (len + 1 + strlen(scriptname) + MAX_EXT_LEN >= sizeof tmpbuf)
		continue;	/* don't search dir with too-long name */
#ifdef MACOS_TRADITIONAL
	    if (len && tmpbuf[len - 1] != ':')
	    	tmpbuf[len++] = ':';
#else
	    if (len
#if defined(atarist) || defined(__MINT__) || defined(DOSISH)
		&& tmpbuf[len - 1] != '/'
		&& tmpbuf[len - 1] != '\\'
#endif
	       )
		tmpbuf[len++] = '/';
	    if (len == 2 && tmpbuf[0] == '.')
		seen_dot = 1;
#endif
	    (void)strcpy(tmpbuf + len, scriptname);
#endif  /* !VMS */

#ifdef SEARCH_EXTS
	    len = strlen(tmpbuf);
	    if (extidx > 0)	/* reset after previous loop */
		extidx = 0;
	    do {
#endif
	    	DEBUG_p(PerlIO_printf(Perl_debug_log, "Looking for %s\n",tmpbuf));
		retval = PerlLIO_stat(tmpbuf,&PL_statbuf);
		if (S_ISDIR(PL_statbuf.st_mode)) {
		    retval = -1;
		}
#ifdef SEARCH_EXTS
	    } while (  retval < 0		/* not there */
		    && extidx>=0 && ext[extidx]	/* try an extension? */
		    && strcpy(tmpbuf+len, ext[extidx++])
		);
#endif
	    if (retval < 0)
		continue;
	    if (S_ISREG(PL_statbuf.st_mode)
		&& cando(S_IRUSR,TRUE,&PL_statbuf)
#if !defined(DOSISH) && !defined(MACOS_TRADITIONAL)
		&& cando(S_IXUSR,TRUE,&PL_statbuf)
#endif
		)
	    {
		xfound = tmpbuf;		/* bingo! */
		break;
	    }
	    if (!xfailed)
		xfailed = savepv(tmpbuf);
	}
#ifndef DOSISH
	if (!xfound && !seen_dot && !xfailed &&
	    (PerlLIO_stat(scriptname,&PL_statbuf) < 0
	     || S_ISDIR(PL_statbuf.st_mode)))
#endif
	    seen_dot = 1;			/* Disable message. */
	if (!xfound) {
	    if (flags & 1) {			/* do or die? */
		Perl_croak(aTHX_ "Can't %s %s%s%s",
		      (xfailed ? "execute" : "find"),
		      (xfailed ? xfailed : scriptname),
		      (xfailed ? "" : " on PATH"),
		      (xfailed || seen_dot) ? "" : ", '.' not in PATH");
	    }
	    scriptname = Nullch;
	}
	if (xfailed)
	    Safefree(xfailed);
	scriptname = xfound;
    }
    return (scriptname ? savepv(scriptname) : Nullch);
}

#ifndef PERL_GET_CONTEXT_DEFINED

void *
Perl_get_context(void)
{
#if defined(USE_5005THREADS) || defined(USE_ITHREADS)
#  ifdef OLD_PTHREADS_API
    pthread_addr_t t;
    if (pthread_getspecific(PL_thr_key, &t))
	Perl_croak_nocontext("panic: pthread_getspecific");
    return (void*)t;
#  else
#    ifdef I_MACH_CTHREADS
    return (void*)cthread_data(cthread_self());
#    else
    return (void*)PTHREAD_GETSPECIFIC(PL_thr_key);
#    endif
#  endif
#else
    return (void*)NULL;
#endif
}

void
Perl_set_context(void *t)
{
#if defined(USE_5005THREADS) || defined(USE_ITHREADS)
#  ifdef I_MACH_CTHREADS
    cthread_set_data(cthread_self(), t);
#  else
    if (pthread_setspecific(PL_thr_key, t))
	Perl_croak_nocontext("panic: pthread_setspecific");
#  endif
#endif
}

#endif /* !PERL_GET_CONTEXT_DEFINED */

#ifdef USE_5005THREADS

#ifdef FAKE_THREADS
/* Very simplistic scheduler for now */
void
schedule(void)
{
    thr = thr->i.next_run;
}

void
Perl_cond_init(pTHX_ perl_cond *cp)
{
    *cp = 0;
}

void
Perl_cond_signal(pTHX_ perl_cond *cp)
{
    perl_os_thread t;
    perl_cond cond = *cp;

    if (!cond)
	return;
    t = cond->thread;
    /* Insert t in the runnable queue just ahead of us */
    t->i.next_run = thr->i.next_run;
    thr->i.next_run->i.prev_run = t;
    t->i.prev_run = thr;
    thr->i.next_run = t;
    thr->i.wait_queue = 0;
    /* Remove from the wait queue */
    *cp = cond->next;
    Safefree(cond);
}

void
Perl_cond_broadcast(pTHX_ perl_cond *cp)
{
    perl_os_thread t;
    perl_cond cond, cond_next;

    for (cond = *cp; cond; cond = cond_next) {
	t = cond->thread;
	/* Insert t in the runnable queue just ahead of us */
	t->i.next_run = thr->i.next_run;
	thr->i.next_run->i.prev_run = t;
	t->i.prev_run = thr;
	thr->i.next_run = t;
	thr->i.wait_queue = 0;
	/* Remove from the wait queue */
	cond_next = cond->next;
	Safefree(cond);
    }
    *cp = 0;
}

void
Perl_cond_wait(pTHX_ perl_cond *cp)
{
    perl_cond cond;

    if (thr->i.next_run == thr)
	Perl_croak(aTHX_ "panic: perl_cond_wait called by last runnable thread");

    New(666, cond, 1, struct perl_wait_queue);
    cond->thread = thr;
    cond->next = *cp;
    *cp = cond;
    thr->i.wait_queue = cond;
    /* Remove ourselves from runnable queue */
    thr->i.next_run->i.prev_run = thr->i.prev_run;
    thr->i.prev_run->i.next_run = thr->i.next_run;
}
#endif /* FAKE_THREADS */

MAGIC *
Perl_condpair_magic(pTHX_ SV *sv)
{
    MAGIC *mg;

    (void)SvUPGRADE(sv, SVt_PVMG);
    mg = mg_find(sv, PERL_MAGIC_mutex);
    if (!mg) {
	condpair_t *cp;

	New(53, cp, 1, condpair_t);
	MUTEX_INIT(&cp->mutex);
	COND_INIT(&cp->owner_cond);
	COND_INIT(&cp->cond);
	cp->owner = 0;
	LOCK_CRED_MUTEX;		/* XXX need separate mutex? */
	mg = mg_find(sv, PERL_MAGIC_mutex);
	if (mg) {
	    /* someone else beat us to initialising it */
	    UNLOCK_CRED_MUTEX;		/* XXX need separate mutex? */
	    MUTEX_DESTROY(&cp->mutex);
	    COND_DESTROY(&cp->owner_cond);
	    COND_DESTROY(&cp->cond);
	    Safefree(cp);
	}
	else {
	    sv_magic(sv, Nullsv, PERL_MAGIC_mutex, 0, 0);
	    mg = SvMAGIC(sv);
	    mg->mg_ptr = (char *)cp;
	    mg->mg_len = sizeof(cp);
	    UNLOCK_CRED_MUTEX;		/* XXX need separate mutex? */
	    DEBUG_S(WITH_THR(PerlIO_printf(Perl_debug_log,
					   "%p: condpair_magic %p\n", thr, sv)));
	}
    }
    return mg;
}

SV *
Perl_sv_lock(pTHX_ SV *osv)
{
    MAGIC *mg;
    SV *sv = osv;

    LOCK_SV_LOCK_MUTEX;
    if (SvROK(sv)) {
	sv = SvRV(sv);
    }

    mg = condpair_magic(sv);
    MUTEX_LOCK(MgMUTEXP(mg));
    if (MgOWNER(mg) == thr)
	MUTEX_UNLOCK(MgMUTEXP(mg));
    else {
	while (MgOWNER(mg))
	    COND_WAIT(MgOWNERCONDP(mg), MgMUTEXP(mg));
	MgOWNER(mg) = thr;
	DEBUG_S(PerlIO_printf(Perl_debug_log,
			      "0x%"UVxf": Perl_lock lock 0x%"UVxf"\n",
			      PTR2UV(thr), PTR2UV(sv)));
	MUTEX_UNLOCK(MgMUTEXP(mg));
	SAVEDESTRUCTOR_X(Perl_unlock_condpair, sv);
    }
    UNLOCK_SV_LOCK_MUTEX;
    return sv;
}

/*
 * Make a new perl thread structure using t as a prototype. Some of the
 * fields for the new thread are copied from the prototype thread, t,
 * so t should not be running in perl at the time this function is
 * called. The use by ext/Thread/Thread.xs in core perl (where t is the
 * thread calling new_struct_thread) clearly satisfies this constraint.
 */
struct perl_thread *
Perl_new_struct_thread(pTHX_ struct perl_thread *t)
{
#if !defined(PERL_IMPLICIT_CONTEXT)
    struct perl_thread *thr;
#endif
    SV *sv;
    SV **svp;
    I32 i;

    sv = newSVpvn("", 0);
    SvGROW(sv, sizeof(struct perl_thread) + 1);
    SvCUR_set(sv, sizeof(struct perl_thread));
    thr = (Thread) SvPVX(sv);
#ifdef DEBUGGING
    Poison(thr, 1, struct perl_thread);
    PL_markstack = 0;
    PL_scopestack = 0;
    PL_savestack = 0;
    PL_retstack = 0;
    PL_dirty = 0;
    PL_localizing = 0;
    Zero(&PL_hv_fetch_ent_mh, 1, HE);
    PL_efloatbuf = (char*)NULL;
    PL_efloatsize = 0;
#else
    Zero(thr, 1, struct perl_thread);
#endif

    thr->oursv = sv;
    init_stacks();

    PL_curcop = &PL_compiling;
    thr->interp = t->interp;
    thr->cvcache = newHV();
    thr->threadsv = newAV();
    thr->specific = newAV();
    thr->errsv = newSVpvn("", 0);
    thr->flags = THRf_R_JOINABLE;
    thr->thr_done = 0;
    MUTEX_INIT(&thr->mutex);

    JMPENV_BOOTSTRAP;

    PL_in_eval = EVAL_NULL;	/* ~(EVAL_INEVAL|EVAL_WARNONLY|EVAL_KEEPERR|EVAL_INREQUIRE) */
    PL_restartop = 0;

    PL_statname = NEWSV(66,0);
    PL_errors = newSVpvn("", 0);
    PL_maxscream = -1;
    PL_regcompp = MEMBER_TO_FPTR(Perl_pregcomp);
    PL_regexecp = MEMBER_TO_FPTR(Perl_regexec_flags);
    PL_regint_start = MEMBER_TO_FPTR(Perl_re_intuit_start);
    PL_regint_string = MEMBER_TO_FPTR(Perl_re_intuit_string);
    PL_regfree = MEMBER_TO_FPTR(Perl_pregfree);
    PL_regindent = 0;
    PL_reginterp_cnt = 0;
    PL_lastscream = Nullsv;
    PL_screamfirst = 0;
    PL_screamnext = 0;
    PL_reg_start_tmp = 0;
    PL_reg_start_tmpl = 0;
    PL_reg_poscache = Nullch;

    PL_peepp = MEMBER_TO_FPTR(Perl_peep);

    /* parent thread's data needs to be locked while we make copy */
    MUTEX_LOCK(&t->mutex);

#ifdef PERL_FLEXIBLE_EXCEPTIONS
    PL_protect = t->Tprotect;
#endif

    PL_curcop = t->Tcurcop;       /* XXX As good a guess as any? */
    PL_defstash = t->Tdefstash;   /* XXX maybe these should */
    PL_curstash = t->Tcurstash;   /* always be set to main? */

    PL_tainted = t->Ttainted;
    PL_curpm = t->Tcurpm;	/* XXX No PMOP ref count */
    PL_rs = newSVsv(t->Trs);
    PL_last_in_gv = Nullgv;
    PL_ofs_sv = t->Tofs_sv ? SvREFCNT_inc(PL_ofs_sv) : Nullsv;
    PL_defoutgv = (GV*)SvREFCNT_inc(t->Tdefoutgv);
    PL_chopset = t->Tchopset;
    PL_bodytarget = newSVsv(t->Tbodytarget);
    PL_toptarget = newSVsv(t->Ttoptarget);
    if (t->Tformtarget == t->Ttoptarget)
	PL_formtarget = PL_toptarget;
    else
	PL_formtarget = PL_bodytarget;
    PL_watchaddr = 0; /* XXX */
    PL_watchok = 0; /* XXX */
    PL_comppad = 0;
    PL_curpad = 0;

    /* Initialise all per-thread SVs that the template thread used */
    svp = AvARRAY(t->threadsv);
    for (i = 0; i <= AvFILLp(t->threadsv); i++, svp++) {
	if (*svp && *svp != &PL_sv_undef) {
	    SV *sv = newSVsv(*svp);
	    av_store(thr->threadsv, i, sv);
	    sv_magic(sv, 0, PERL_MAGIC_sv, &PL_threadsv_names[i], 1);
	    DEBUG_S(PerlIO_printf(Perl_debug_log,
		"new_struct_thread: copied threadsv %"IVdf" %p->%p\n",
				  (IV)i, t, thr));
	}
    }
    thr->threadsvp = AvARRAY(thr->threadsv);

    MUTEX_LOCK(&PL_threads_mutex);
    PL_nthreads++;
    thr->tid = ++PL_threadnum;
    thr->next = t->next;
    thr->prev = t;
    t->next = thr;
    thr->next->prev = thr;
    MUTEX_UNLOCK(&PL_threads_mutex);

    /* done copying parent's state */
    MUTEX_UNLOCK(&t->mutex);

#ifdef HAVE_THREAD_INTERN
    Perl_init_thread_intern(thr);
#endif /* HAVE_THREAD_INTERN */
    return thr;
}
#endif /* USE_5005THREADS */

#ifdef PERL_GLOBAL_STRUCT
struct perl_vars *
Perl_GetVars(pTHX)
{
 return &PL_Vars;
}
#endif

char **
Perl_get_op_names(pTHX)
{
 return PL_op_name;
}

char **
Perl_get_op_descs(pTHX)
{
 return PL_op_desc;
}

char *
Perl_get_no_modify(pTHX)
{
 return (char*)PL_no_modify;
}

U32 *
Perl_get_opargs(pTHX)
{
 return PL_opargs;
}

PPADDR_t*
Perl_get_ppaddr(pTHX)
{
 return (PPADDR_t*)PL_ppaddr;
}

#ifndef HAS_GETENV_LEN
char *
Perl_getenv_len(pTHX_ const char *env_elem, unsigned long *len)
{
    char *env_trans = PerlEnv_getenv(env_elem);
    if (env_trans)
	*len = strlen(env_trans);
    return env_trans;
}
#endif


MGVTBL*
Perl_get_vtbl(pTHX_ int vtbl_id)
{
    MGVTBL* result = Null(MGVTBL*);

    switch(vtbl_id) {
    case want_vtbl_sv:
	result = &PL_vtbl_sv;
	break;
    case want_vtbl_env:
	result = &PL_vtbl_env;
	break;
    case want_vtbl_envelem:
	result = &PL_vtbl_envelem;
	break;
    case want_vtbl_sig:
	result = &PL_vtbl_sig;
	break;
    case want_vtbl_sigelem:
	result = &PL_vtbl_sigelem;
	break;
    case want_vtbl_pack:
	result = &PL_vtbl_pack;
	break;
    case want_vtbl_packelem:
	result = &PL_vtbl_packelem;
	break;
    case want_vtbl_dbline:
	result = &PL_vtbl_dbline;
	break;
    case want_vtbl_isa:
	result = &PL_vtbl_isa;
	break;
    case want_vtbl_isaelem:
	result = &PL_vtbl_isaelem;
	break;
    case want_vtbl_arylen:
	result = &PL_vtbl_arylen;
	break;
    case want_vtbl_glob:
	result = &PL_vtbl_glob;
	break;
    case want_vtbl_mglob:
	result = &PL_vtbl_mglob;
	break;
    case want_vtbl_nkeys:
	result = &PL_vtbl_nkeys;
	break;
    case want_vtbl_taint:
	result = &PL_vtbl_taint;
	break;
    case want_vtbl_substr:
	result = &PL_vtbl_substr;
	break;
    case want_vtbl_vec:
	result = &PL_vtbl_vec;
	break;
    case want_vtbl_pos:
	result = &PL_vtbl_pos;
	break;
    case want_vtbl_bm:
	result = &PL_vtbl_bm;
	break;
    case want_vtbl_fm:
	result = &PL_vtbl_fm;
	break;
    case want_vtbl_uvar:
	result = &PL_vtbl_uvar;
	break;
#ifdef USE_5005THREADS
    case want_vtbl_mutex:
	result = &PL_vtbl_mutex;
	break;
#endif
    case want_vtbl_defelem:
	result = &PL_vtbl_defelem;
	break;
    case want_vtbl_regexp:
	result = &PL_vtbl_regexp;
	break;
    case want_vtbl_regdata:
	result = &PL_vtbl_regdata;
	break;
    case want_vtbl_regdatum:
	result = &PL_vtbl_regdatum;
	break;
#ifdef USE_LOCALE_COLLATE
    case want_vtbl_collxfrm:
	result = &PL_vtbl_collxfrm;
	break;
#endif
    case want_vtbl_amagic:
	result = &PL_vtbl_amagic;
	break;
    case want_vtbl_amagicelem:
	result = &PL_vtbl_amagicelem;
	break;
    case want_vtbl_backref:
	result = &PL_vtbl_backref;
	break;
    case want_vtbl_utf8:
	result = &PL_vtbl_utf8;
	break;
    }
    return result;
}

I32
Perl_my_fflush_all(pTHX)
{
#if defined(USE_PERLIO) || defined(FFLUSH_NULL) || defined(USE_SFIO)
    return PerlIO_flush(NULL);
#else
# if defined(HAS__FWALK)
    extern int fflush(FILE *);
    /* undocumented, unprototyped, but very useful BSDism */
    extern void _fwalk(int (*)(FILE *));
    _fwalk(&fflush);
    return 0;
# else
#  if defined(FFLUSH_ALL) && defined(HAS_STDIO_STREAM_ARRAY)
    long open_max = -1;
#   ifdef PERL_FFLUSH_ALL_FOPEN_MAX
    open_max = PERL_FFLUSH_ALL_FOPEN_MAX;
#   else
#    if defined(HAS_SYSCONF) && defined(_SC_OPEN_MAX)
    open_max = sysconf(_SC_OPEN_MAX);
#     else
#      ifdef FOPEN_MAX
    open_max = FOPEN_MAX;
#      else
#       ifdef OPEN_MAX
    open_max = OPEN_MAX;
#       else
#        ifdef _NFILE
    open_max = _NFILE;
#        endif
#       endif
#      endif
#     endif
#    endif
    if (open_max > 0) {
      long i;
      for (i = 0; i < open_max; i++)
	    if (STDIO_STREAM_ARRAY[i]._file >= 0 &&
		STDIO_STREAM_ARRAY[i]._file < open_max &&
		STDIO_STREAM_ARRAY[i]._flag)
		PerlIO_flush(&STDIO_STREAM_ARRAY[i]);
      return 0;
    }
#  endif
    SETERRNO(EBADF,RMS_IFI);
    return EOF;
# endif
#endif
}

void
Perl_report_evil_fh(pTHX_ GV *gv, IO *io, I32 op)
{
    char *func =
	op == OP_READLINE   ? "readline"  :	/* "<HANDLE>" not nice */
	op == OP_LEAVEWRITE ? "write" :		/* "write exit" not nice */
	PL_op_desc[op];
    char *pars = OP_IS_FILETEST(op) ? "" : "()";
    char *type = OP_IS_SOCKET(op)
	    || (gv && io && IoTYPE(io) == IoTYPE_SOCKET)
		?  "socket" : "filehandle";
    char *name = NULL;

    if (gv && isGV(gv)) {
	name = GvENAME(gv);
    }

    if (op == OP_phoney_OUTPUT_ONLY || op == OP_phoney_INPUT_ONLY) {
	if (ckWARN(WARN_IO)) {
	    const char *direction = (op == OP_phoney_INPUT_ONLY) ? "in" : "out";
	    if (name && *name)
		Perl_warner(aTHX_ packWARN(WARN_IO),
			    "Filehandle %s opened only for %sput",
			    name, direction);
	    else
		Perl_warner(aTHX_ packWARN(WARN_IO),
			    "Filehandle opened only for %sput", direction);
	}
    }
    else {
	char *vile;
	I32   warn_type;

	if (gv && io && IoTYPE(io) == IoTYPE_CLOSED) {
	    vile = "closed";
	    warn_type = WARN_CLOSED;
	}
	else {
	    vile = "unopened";
	    warn_type = WARN_UNOPENED;
	}

	if (ckWARN(warn_type)) {
	    if (name && *name) {
		Perl_warner(aTHX_ packWARN(warn_type),
			    "%s%s on %s %s %s", func, pars, vile, type, name);
		if (io && IoDIRP(io) && !(IoFLAGS(io) & IOf_FAKE_DIRP))
		    Perl_warner(
			aTHX_ packWARN(warn_type),
			"\t(Are you trying to call %s%s on dirhandle %s?)\n",
			func, pars, name
		    );
	    }
	    else {
		Perl_warner(aTHX_ packWARN(warn_type),
			    "%s%s on %s %s", func, pars, vile, type);
		if (gv && io && IoDIRP(io) && !(IoFLAGS(io) & IOf_FAKE_DIRP))
		    Perl_warner(
			aTHX_ packWARN(warn_type),
			"\t(Are you trying to call %s%s on dirhandle?)\n",
			func, pars
		    );
	    }
	}
    }
}

#ifdef EBCDIC
/* in ASCII order, not that it matters */
static const char controllablechars[] = "?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_";

int
Perl_ebcdic_control(pTHX_ int ch)
{
    if (ch > 'a') {
	char *ctlp;

	if (islower(ch))
	    ch = toupper(ch);

	if ((ctlp = strchr(controllablechars, ch)) == 0) {
	    Perl_die(aTHX_ "unrecognised control character '%c'\n", ch);
	}

	if (ctlp == controllablechars)
	    return('\177'); /* DEL */
	else
	    return((unsigned char)(ctlp - controllablechars - 1));
    } else { /* Want uncontrol */
	if (ch == '\177' || ch == -1)
	    return('?');
	else if (ch == '\157')
	    return('\177');
	else if (ch == '\174')
	    return('\000');
	else if (ch == '^')    /* '\137' in 1047, '\260' in 819 */
	    return('\036');
	else if (ch == '\155')
	    return('\037');
	else if (0 < ch && ch < (sizeof(controllablechars) - 1))
	    return(controllablechars[ch+1]);
	else
	    Perl_die(aTHX_ "invalid control request: '\\%03o'\n", ch & 0xFF);
    }
}
#endif

/* To workaround core dumps from the uninitialised tm_zone we get the
 * system to give us a reasonable struct to copy.  This fix means that
 * strftime uses the tm_zone and tm_gmtoff values returned by
 * localtime(time()). That should give the desired result most of the
 * time. But probably not always!
 *
 * This does not address tzname aspects of NETaa14816.
 *
 */

#ifdef HAS_GNULIBC
# ifndef STRUCT_TM_HASZONE
#    define STRUCT_TM_HASZONE
# endif
#endif

#ifdef STRUCT_TM_HASZONE /* Backward compat */
# ifndef HAS_TM_TM_ZONE
#    define HAS_TM_TM_ZONE
# endif
#endif

void
Perl_init_tm(pTHX_ struct tm *ptm)	/* see mktime, strftime and asctime */
{
#ifdef HAS_TM_TM_ZONE
    Time_t now;
    (void)time(&now);
    Copy(localtime(&now), ptm, 1, struct tm);
#endif
}

/*
 * mini_mktime - normalise struct tm values without the localtime()
 * semantics (and overhead) of mktime().
 */
void
Perl_mini_mktime(pTHX_ struct tm *ptm)
{
    int yearday;
    int secs;
    int month, mday, year, jday;
    int odd_cent, odd_year;

#define	DAYS_PER_YEAR	365
#define	DAYS_PER_QYEAR	(4*DAYS_PER_YEAR+1)
#define	DAYS_PER_CENT	(25*DAYS_PER_QYEAR-1)
#define	DAYS_PER_QCENT	(4*DAYS_PER_CENT+1)
#define	SECS_PER_HOUR	(60*60)
#define	SECS_PER_DAY	(24*SECS_PER_HOUR)
/* parentheses deliberately absent on these two, otherwise they don't work */
#define	MONTH_TO_DAYS	153/5
#define	DAYS_TO_MONTH	5/153
/* offset to bias by March (month 4) 1st between month/mday & year finding */
#define	YEAR_ADJUST	(4*MONTH_TO_DAYS+1)
/* as used here, the algorithm leaves Sunday as day 1 unless we adjust it */
#define	WEEKDAY_BIAS	6	/* (1+6)%7 makes Sunday 0 again */

/*
 * Year/day algorithm notes:
 *
 * With a suitable offset for numeric value of the month, one can find
 * an offset into the year by considering months to have 30.6 (153/5) days,
 * using integer arithmetic (i.e., with truncation).  To avoid too much
 * messing about with leap days, we consider January and February to be
 * the 13th and 14th month of the previous year.  After that transformation,
 * we need the month index we use to be high by 1 from 'normal human' usage,
 * so the month index values we use run from 4 through 15.
 *
 * Given that, and the rules for the Gregorian calendar (leap years are those
 * divisible by 4 unless also divisible by 100, when they must be divisible
 * by 400 instead), we can simply calculate the number of days since some
 * arbitrary 'beginning of time' by futzing with the (adjusted) year number,
 * the days we derive from our month index, and adding in the day of the
 * month.  The value used here is not adjusted for the actual origin which
 * it normally would use (1 January A.D. 1), since we're not exposing it.
 * We're only building the value so we can turn around and get the
 * normalised values for the year, month, day-of-month, and day-of-year.
 *
 * For going backward, we need to bias the value we're using so that we find
 * the right year value.  (Basically, we don't want the contribution of
 * March 1st to the number to apply while deriving the year).  Having done
 * that, we 'count up' the contribution to the year number by accounting for
 * full quadracenturies (400-year periods) with their extra leap days, plus
 * the contribution from full centuries (to avoid counting in the lost leap
 * days), plus the contribution from full quad-years (to count in the normal
 * leap days), plus the leftover contribution from any non-leap years.
 * At this point, if we were working with an actual leap day, we'll have 0
 * days left over.  This is also true for March 1st, however.  So, we have
 * to special-case that result, and (earlier) keep track of the 'odd'
 * century and year contributions.  If we got 4 extra centuries in a qcent,
 * or 4 extra years in a qyear, then it's a leap day and we call it 29 Feb.
 * Otherwise, we add back in the earlier bias we removed (the 123 from
 * figuring in March 1st), find the month index (integer division by 30.6),
 * and the remainder is the day-of-month.  We then have to convert back to
 * 'real' months (including fixing January and February from being 14/15 in
 * the previous year to being in the proper year).  After that, to get
 * tm_yday, we work with the normalised year and get a new yearday value for
 * January 1st, which we subtract from the yearday value we had earlier,
 * representing the date we've re-built.  This is done from January 1
 * because tm_yday is 0-origin.
 *
 * Since POSIX time routines are only guaranteed to work for times since the
 * UNIX epoch (00:00:00 1 Jan 1970 UTC), the fact that this algorithm
 * applies Gregorian calendar rules even to dates before the 16th century
 * doesn't bother me.  Besides, you'd need cultural context for a given
 * date to know whether it was Julian or Gregorian calendar, and that's
 * outside the scope for this routine.  Since we convert back based on the
 * same rules we used to build the yearday, you'll only get strange results
 * for input which needed normalising, or for the 'odd' century years which
 * were leap years in the Julian calander but not in the Gregorian one.
 * I can live with that.
 *
 * This algorithm also fails to handle years before A.D. 1 gracefully, but
 * that's still outside the scope for POSIX time manipulation, so I don't
 * care.
 */

    year = 1900 + ptm->tm_year;
    month = ptm->tm_mon;
    mday = ptm->tm_mday;
    /* allow given yday with no month & mday to dominate the result */
    if (ptm->tm_yday >= 0 && mday <= 0 && month <= 0) {
	month = 0;
	mday = 0;
	jday = 1 + ptm->tm_yday;
    }
    else {
	jday = 0;
    }
    if (month >= 2)
	month+=2;
    else
	month+=14, year--;
    yearday = DAYS_PER_YEAR * year + year/4 - year/100 + year/400;
    yearday += month*MONTH_TO_DAYS + mday + jday;
    /*
     * Note that we don't know when leap-seconds were or will be,
     * so we have to trust the user if we get something which looks
     * like a sensible leap-second.  Wild values for seconds will
     * be rationalised, however.
     */
    if ((unsigned) ptm->tm_sec <= 60) {
	secs = 0;
    }
    else {
	secs = ptm->tm_sec;
	ptm->tm_sec = 0;
    }
    secs += 60 * ptm->tm_min;
    secs += SECS_PER_HOUR * ptm->tm_hour;
    if (secs < 0) {
	if (secs-(secs/SECS_PER_DAY*SECS_PER_DAY) < 0) {
	    /* got negative remainder, but need positive time */
	    /* back off an extra day to compensate */
	    yearday += (secs/SECS_PER_DAY)-1;
	    secs -= SECS_PER_DAY * (secs/SECS_PER_DAY - 1);
	}
	else {
	    yearday += (secs/SECS_PER_DAY);
	    secs -= SECS_PER_DAY * (secs/SECS_PER_DAY);
	}
    }
    else if (secs >= SECS_PER_DAY) {
	yearday += (secs/SECS_PER_DAY);
	secs %= SECS_PER_DAY;
    }
    ptm->tm_hour = secs/SECS_PER_HOUR;
    secs %= SECS_PER_HOUR;
    ptm->tm_min = secs/60;
    secs %= 60;
    ptm->tm_sec += secs;
    /* done with time of day effects */
    /*
     * The algorithm for yearday has (so far) left it high by 428.
     * To avoid mistaking a legitimate Feb 29 as Mar 1, we need to
     * bias it by 123 while trying to figure out what year it
     * really represents.  Even with this tweak, the reverse
     * translation fails for years before A.D. 0001.
     * It would still fail for Feb 29, but we catch that one below.
     */
    jday = yearday;	/* save for later fixup vis-a-vis Jan 1 */
    yearday -= YEAR_ADJUST;
    year = (yearday / DAYS_PER_QCENT) * 400;
    yearday %= DAYS_PER_QCENT;
    odd_cent = yearday / DAYS_PER_CENT;
    year += odd_cent * 100;
    yearday %= DAYS_PER_CENT;
    year += (yearday / DAYS_PER_QYEAR) * 4;
    yearday %= DAYS_PER_QYEAR;
    odd_year = yearday / DAYS_PER_YEAR;
    year += odd_year;
    yearday %= DAYS_PER_YEAR;
    if (!yearday && (odd_cent==4 || odd_year==4)) { /* catch Feb 29 */
	month = 1;
	yearday = 29;
    }
    else {
	yearday += YEAR_ADJUST;	/* recover March 1st crock */
	month = yearday*DAYS_TO_MONTH;
	yearday -= month*MONTH_TO_DAYS;
	/* recover other leap-year adjustment */
	if (month > 13) {
	    month-=14;
	    year++;
	}
	else {
	    month-=2;
	}
    }
    ptm->tm_year = year - 1900;
    if (yearday) {
      ptm->tm_mday = yearday;
      ptm->tm_mon = month;
    }
    else {
      ptm->tm_mday = 31;
      ptm->tm_mon = month - 1;
    }
    /* re-build yearday based on Jan 1 to get tm_yday */
    year--;
    yearday = year*DAYS_PER_YEAR + year/4 - year/100 + year/400;
    yearday += 14*MONTH_TO_DAYS + 1;
    ptm->tm_yday = jday - yearday;
    /* fix tm_wday if not overridden by caller */
    if ((unsigned)ptm->tm_wday > 6)
	ptm->tm_wday = (jday + WEEKDAY_BIAS) % 7;
}

char *
Perl_my_strftime(pTHX_ char *fmt, int sec, int min, int hour, int mday, int mon, int year, int wday, int yday, int isdst)
{
#ifdef HAS_STRFTIME
  char *buf;
  int buflen;
  struct tm mytm;
  int len;

  init_tm(&mytm);	/* XXX workaround - see init_tm() above */
  mytm.tm_sec = sec;
  mytm.tm_min = min;
  mytm.tm_hour = hour;
  mytm.tm_mday = mday;
  mytm.tm_mon = mon;
  mytm.tm_year = year;
  mytm.tm_wday = wday;
  mytm.tm_yday = yday;
  mytm.tm_isdst = isdst;
  mini_mktime(&mytm);
  /* use libc to get the values for tm_gmtoff and tm_zone [perl #18238] */
#if defined(HAS_MKTIME) && (defined(HAS_TM_TM_GMTOFF) || defined(HAS_TM_TM_ZONE))
  STMT_START {
    struct tm mytm2;
    mytm2 = mytm;
    mktime(&mytm2);
#ifdef HAS_TM_TM_GMTOFF
    mytm.tm_gmtoff = mytm2.tm_gmtoff;
#endif
#ifdef HAS_TM_TM_ZONE
    mytm.tm_zone = mytm2.tm_zone;
#endif
  } STMT_END;
#endif
  buflen = 64;
  New(0, buf, buflen, char);
  len = strftime(buf, buflen, fmt, &mytm);
  /*
  ** The following is needed to handle to the situation where
  ** tmpbuf overflows.  Basically we want to allocate a buffer
  ** and try repeatedly.  The reason why it is so complicated
  ** is that getting a return value of 0 from strftime can indicate
  ** one of the following:
  ** 1. buffer overflowed,
  ** 2. illegal conversion specifier, or
  ** 3. the format string specifies nothing to be returned(not
  **	  an error).  This could be because format is an empty string
  **    or it specifies %p that yields an empty string in some locale.
  ** If there is a better way to make it portable, go ahead by
  ** all means.
  */
  if ((len > 0 && len < buflen) || (len == 0 && *fmt == '\0'))
    return buf;
  else {
    /* Possibly buf overflowed - try again with a bigger buf */
    int     fmtlen = strlen(fmt);
    int	    bufsize = fmtlen + buflen;

    New(0, buf, bufsize, char);
    while (buf) {
      buflen = strftime(buf, bufsize, fmt, &mytm);
      if (buflen > 0 && buflen < bufsize)
	break;
      /* heuristic to prevent out-of-memory errors */
      if (bufsize > 100*fmtlen) {
	Safefree(buf);
	buf = NULL;
	break;
      }
      bufsize *= 2;
      Renew(buf, bufsize, char);
    }
    return buf;
  }
#else
  Perl_croak(aTHX_ "panic: no strftime");
#endif
}


#define SV_CWD_RETURN_UNDEF \
sv_setsv(sv, &PL_sv_undef); \
return FALSE

#define SV_CWD_ISDOT(dp) \
    (dp->d_name[0] == '.' && (dp->d_name[1] == '\0' || \
	(dp->d_name[1] == '.' && dp->d_name[2] == '\0')))

/*
=head1 Miscellaneous Functions

=for apidoc getcwd_sv

Fill the sv with current working directory

=cut
*/

/* Originally written in Perl by John Bazik; rewritten in C by Ben Sugars.
 * rewritten again by dougm, optimized for use with xs TARG, and to prefer
 * getcwd(3) if available
 * Comments from the orignal:
 *     This is a faster version of getcwd.  It's also more dangerous
 *     because you might chdir out of a directory that you can't chdir
 *     back into. */

int
Perl_getcwd_sv(pTHX_ register SV *sv)
{
#ifndef PERL_MICRO

#ifndef INCOMPLETE_TAINTS
    SvTAINTED_on(sv);
#endif

#ifdef HAS_GETCWD
    {
	char buf[MAXPATHLEN];

	/* Some getcwd()s automatically allocate a buffer of the given
	 * size from the heap if they are given a NULL buffer pointer.
	 * The problem is that this behaviour is not portable. */
	if (getcwd(buf, sizeof(buf) - 1)) {
	    STRLEN len = strlen(buf);
	    sv_setpvn(sv, buf, len);
	    return TRUE;
	}
	else {
	    sv_setsv(sv, &PL_sv_undef);
	    return FALSE;
	}
    }

#else

    Stat_t statbuf;
    int orig_cdev, orig_cino, cdev, cino, odev, oino, tdev, tino;
    int namelen, pathlen=0;
    DIR *dir;
    Direntry_t *dp;

    (void)SvUPGRADE(sv, SVt_PV);

    if (PerlLIO_lstat(".", &statbuf) < 0) {
	SV_CWD_RETURN_UNDEF;
    }

    orig_cdev = statbuf.st_dev;
    orig_cino = statbuf.st_ino;
    cdev = orig_cdev;
    cino = orig_cino;

    for (;;) {
	odev = cdev;
	oino = cino;

	if (PerlDir_chdir("..") < 0) {
	    SV_CWD_RETURN_UNDEF;
	}
	if (PerlLIO_stat(".", &statbuf) < 0) {
	    SV_CWD_RETURN_UNDEF;
	}

	cdev = statbuf.st_dev;
	cino = statbuf.st_ino;

	if (odev == cdev && oino == cino) {
	    break;
	}
	if (!(dir = PerlDir_open("."))) {
	    SV_CWD_RETURN_UNDEF;
	}

	while ((dp = PerlDir_read(dir)) != NULL) {
#ifdef DIRNAMLEN
	    namelen = dp->d_namlen;
#else
	    namelen = strlen(dp->d_name);
#endif
	    /* skip . and .. */
	    if (SV_CWD_ISDOT(dp)) {
		continue;
	    }

	    if (PerlLIO_lstat(dp->d_name, &statbuf) < 0) {
		SV_CWD_RETURN_UNDEF;
	    }

	    tdev = statbuf.st_dev;
	    tino = statbuf.st_ino;
	    if (tino == oino && tdev == odev) {
		break;
	    }
	}

	if (!dp) {
	    SV_CWD_RETURN_UNDEF;
	}

	if (pathlen + namelen + 1 >= MAXPATHLEN) {
	    SV_CWD_RETURN_UNDEF;
	}

	SvGROW(sv, pathlen + namelen + 1);

	if (pathlen) {
	    /* shift down */
	    Move(SvPVX(sv), SvPVX(sv) + namelen + 1, pathlen, char);
	}

	/* prepend current directory to the front */
	*SvPVX(sv) = '/';
	Move(dp->d_name, SvPVX(sv)+1, namelen, char);
	pathlen += (namelen + 1);

#ifdef VOID_CLOSEDIR
	PerlDir_close(dir);
#else
	if (PerlDir_close(dir) < 0) {
	    SV_CWD_RETURN_UNDEF;
	}
#endif
    }

    if (pathlen) {
	SvCUR_set(sv, pathlen);
	*SvEND(sv) = '\0';
	SvPOK_only(sv);

	if (PerlDir_chdir(SvPVX(sv)) < 0) {
	    SV_CWD_RETURN_UNDEF;
	}
    }
    if (PerlLIO_stat(".", &statbuf) < 0) {
	SV_CWD_RETURN_UNDEF;
    }

    cdev = statbuf.st_dev;
    cino = statbuf.st_ino;

    if (cdev != orig_cdev || cino != orig_cino) {
	Perl_croak(aTHX_ "Unstable directory path, "
		   "current directory changed unexpectedly");
    }

    return TRUE;
#endif

#else
    return FALSE;
#endif
}

#if !defined(HAS_SOCKETPAIR) && defined(HAS_SOCKET) && defined(AF_INET) && defined(PF_INET) && defined(SOCK_DGRAM) && defined(HAS_SELECT)
#   define EMULATE_SOCKETPAIR_UDP
#endif

#ifdef EMULATE_SOCKETPAIR_UDP
static int
S_socketpair_udp (int fd[2]) {
    dTHX;
    /* Fake a datagram socketpair using UDP to localhost.  */
    int sockets[2] = {-1, -1};
    struct sockaddr_in addresses[2];
    int i;
    Sock_size_t size = sizeof(struct sockaddr_in);
    unsigned short port;
    int got;

    memset(&addresses, 0, sizeof(addresses));
    i = 1;
    do {
	sockets[i] = PerlSock_socket(AF_INET, SOCK_DGRAM, PF_INET);
	if (sockets[i] == -1)
	    goto tidy_up_and_fail;

	addresses[i].sin_family = AF_INET;
	addresses[i].sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	addresses[i].sin_port = 0;	/* kernel choses port.  */
	if (PerlSock_bind(sockets[i], (struct sockaddr *) &addresses[i],
		sizeof(struct sockaddr_in)) == -1)
	    goto tidy_up_and_fail;
    } while (i--);

    /* Now have 2 UDP sockets. Find out which port each is connected to, and
       for each connect the other socket to it.  */
    i = 1;
    do {
	if (PerlSock_getsockname(sockets[i], (struct sockaddr *) &addresses[i],
		&size) == -1)
	    goto tidy_up_and_fail;
	if (size != sizeof(struct sockaddr_in))
	    goto abort_tidy_up_and_fail;
	/* !1 is 0, !0 is 1 */
	if (PerlSock_connect(sockets[!i], (struct sockaddr *) &addresses[i],
		sizeof(struct sockaddr_in)) == -1)
	    goto tidy_up_and_fail;
    } while (i--);

    /* Now we have 2 sockets connected to each other. I don't trust some other
       process not to have already sent a packet to us (by random) so send
       a packet from each to the other.  */
    i = 1;
    do {
	/* I'm going to send my own port number.  As a short.
	   (Who knows if someone somewhere has sin_port as a bitfield and needs
	   this routine. (I'm assuming crays have socketpair)) */
	port = addresses[i].sin_port;
	got = PerlLIO_write(sockets[i], &port, sizeof(port));
	if (got != sizeof(port)) {
	    if (got == -1)
		goto tidy_up_and_fail;
	    goto abort_tidy_up_and_fail;
	}
    } while (i--);

    /* Packets sent. I don't trust them to have arrived though.
       (As I understand it Solaris TCP stack is multithreaded. Non-blocking
       connect to localhost will use a second kernel thread. In 2.6 the
       first thread running the connect() returns before the second completes,
       so EINPROGRESS> In 2.7 the improved stack is faster and connect()
       returns 0. Poor programs have tripped up. One poor program's authors'
       had a 50-1 reverse stock split. Not sure how connected these were.)
       So I don't trust someone not to have an unpredictable UDP stack.
    */

    {
	struct timeval waitfor = {0, 100000}; /* You have 0.1 seconds */
	int max = sockets[1] > sockets[0] ? sockets[1] : sockets[0];
	fd_set rset;

	FD_ZERO(&rset);
	FD_SET(sockets[0], &rset);
	FD_SET(sockets[1], &rset);

	got = PerlSock_select(max + 1, &rset, NULL, NULL, &waitfor);
	if (got != 2 || !FD_ISSET(sockets[0], &rset)
		|| !FD_ISSET(sockets[1], &rset)) {
	    /* I hope this is portable and appropriate.  */
	    if (got == -1)
		goto tidy_up_and_fail;
	    goto abort_tidy_up_and_fail;
	}
    }

    /* And the paranoia department even now doesn't trust it to have arrive
       (hence MSG_DONTWAIT). Or that what arrives was sent by us.  */
    {
	struct sockaddr_in readfrom;
	unsigned short buffer[2];

	i = 1;
	do {
#ifdef MSG_DONTWAIT
	    got = PerlSock_recvfrom(sockets[i], (char *) &buffer,
		    sizeof(buffer), MSG_DONTWAIT,
		    (struct sockaddr *) &readfrom, &size);
#else
	    got = PerlSock_recvfrom(sockets[i], (char *) &buffer,
		    sizeof(buffer), 0,
		    (struct sockaddr *) &readfrom, &size);
#endif

	    if (got == -1)
		goto tidy_up_and_fail;
	    if (got != sizeof(port)
		    || size != sizeof(struct sockaddr_in)
		    /* Check other socket sent us its port.  */
		    || buffer[0] != (unsigned short) addresses[!i].sin_port
		    /* Check kernel says we got the datagram from that socket */
		    || readfrom.sin_family != addresses[!i].sin_family
		    || readfrom.sin_addr.s_addr != addresses[!i].sin_addr.s_addr
		    || readfrom.sin_port != addresses[!i].sin_port)
		goto abort_tidy_up_and_fail;
	} while (i--);
    }
    /* My caller (my_socketpair) has validated that this is non-NULL  */
    fd[0] = sockets[0];
    fd[1] = sockets[1];
    /* I hereby declare this connection open.  May God bless all who cross
       her.  */
    return 0;

  abort_tidy_up_and_fail:
    errno = ECONNABORTED;
  tidy_up_and_fail:
    {
	int save_errno = errno;
	if (sockets[0] != -1)
	    PerlLIO_close(sockets[0]);
	if (sockets[1] != -1)
	    PerlLIO_close(sockets[1]);
	errno = save_errno;
	return -1;
    }
}
#endif /*  EMULATE_SOCKETPAIR_UDP */

#if !defined(HAS_SOCKETPAIR) && defined(HAS_SOCKET) && defined(AF_INET) && defined(PF_INET)
int
Perl_my_socketpair (int family, int type, int protocol, int fd[2]) {
    /* Stevens says that family must be AF_LOCAL, protocol 0.
       I'm going to enforce that, then ignore it, and use TCP (or UDP).  */
    dTHX;
    int listener = -1;
    int connector = -1;
    int acceptor = -1;
    struct sockaddr_in listen_addr;
    struct sockaddr_in connect_addr;
    Sock_size_t size;

    if (protocol
#ifdef AF_UNIX
	|| family != AF_UNIX
#endif
    ) {
	errno = EAFNOSUPPORT;
	return -1;
    }
    if (!fd) {
	errno = EINVAL;
	return -1;
    }

#ifdef EMULATE_SOCKETPAIR_UDP
    if (type == SOCK_DGRAM)
	return S_socketpair_udp(fd);
#endif

    listener = PerlSock_socket(AF_INET, type, 0);
    if (listener == -1)
	return -1;
    memset(&listen_addr, 0, sizeof(listen_addr));
    listen_addr.sin_family = AF_INET;
    listen_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    listen_addr.sin_port = 0;	/* kernel choses port.  */
    if (PerlSock_bind(listener, (struct sockaddr *) &listen_addr,
	    sizeof(listen_addr)) == -1)
	goto tidy_up_and_fail;
    if (PerlSock_listen(listener, 1) == -1)
	goto tidy_up_and_fail;

    connector = PerlSock_socket(AF_INET, type, 0);
    if (connector == -1)
	goto tidy_up_and_fail;
    /* We want to find out the port number to connect to.  */
    size = sizeof(connect_addr);
    if (PerlSock_getsockname(listener, (struct sockaddr *) &connect_addr,
	    &size) == -1)
	goto tidy_up_and_fail;
    if (size != sizeof(connect_addr))
	goto abort_tidy_up_and_fail;
    if (PerlSock_connect(connector, (struct sockaddr *) &connect_addr,
	    sizeof(connect_addr)) == -1)
	goto tidy_up_and_fail;

    size = sizeof(listen_addr);
    acceptor = PerlSock_accept(listener, (struct sockaddr *) &listen_addr,
	    &size);
    if (acceptor == -1)
	goto tidy_up_and_fail;
    if (size != sizeof(listen_addr))
	goto abort_tidy_up_and_fail;
    PerlLIO_close(listener);
    /* Now check we are talking to ourself by matching port and host on the
       two sockets.  */
    if (PerlSock_getsockname(connector, (struct sockaddr *) &connect_addr,
	    &size) == -1)
	goto tidy_up_and_fail;
    if (size != sizeof(connect_addr)
	    || listen_addr.sin_family != connect_addr.sin_family
	    || listen_addr.sin_addr.s_addr != connect_addr.sin_addr.s_addr
	    || listen_addr.sin_port != connect_addr.sin_port) {
	goto abort_tidy_up_and_fail;
    }
    fd[0] = connector;
    fd[1] = acceptor;
    return 0;

  abort_tidy_up_and_fail:
  errno = ECONNABORTED; /* I hope this is portable and appropriate.  */
  tidy_up_and_fail:
    {
	int save_errno = errno;
	if (listener != -1)
	    PerlLIO_close(listener);
	if (connector != -1)
	    PerlLIO_close(connector);
	if (acceptor != -1)
	    PerlLIO_close(acceptor);
	errno = save_errno;
	return -1;
    }
}
#else
/* In any case have a stub so that there's code corresponding
 * to the my_socketpair in global.sym. */
int
Perl_my_socketpair (int family, int type, int protocol, int fd[2]) {
#ifdef HAS_SOCKETPAIR
    return socketpair(family, type, protocol, fd);
#else
    return -1;
#endif
}
#endif

/*

=for apidoc sv_nosharing

Dummy routine which "shares" an SV when there is no sharing module present.
Exists to avoid test for a NULL function pointer and because it could potentially warn under
some level of strict-ness.

=cut
*/

void
Perl_sv_nosharing(pTHX_ SV *sv)
{
}

/*
=for apidoc sv_nolocking

Dummy routine which "locks" an SV when there is no locking module present.
Exists to avoid test for a NULL function pointer and because it could potentially warn under
some level of strict-ness.

=cut
*/

void
Perl_sv_nolocking(pTHX_ SV *sv)
{
}


/*
=for apidoc sv_nounlocking

Dummy routine which "unlocks" an SV when there is no locking module present.
Exists to avoid test for a NULL function pointer and because it could potentially warn under
some level of strict-ness.

=cut
*/

void
Perl_sv_nounlocking(pTHX_ SV *sv)
{
}

U32
Perl_parse_unicode_opts(pTHX_ char **popt)
{
  char *p = *popt;
  U32 opt = 0;

  if (*p) {
       if (isDIGIT(*p)) {
	    opt = (U32) atoi(p);
	    while (isDIGIT(*p)) p++;
	    if (*p && *p != '\n' && *p != '\r')
		 Perl_croak(aTHX_ "Unknown Unicode option letter '%c'", *p);
       }
       else {
	    for (; *p; p++) {
		 switch (*p) {
		 case PERL_UNICODE_STDIN:
		      opt |= PERL_UNICODE_STDIN_FLAG;	break;
		 case PERL_UNICODE_STDOUT:
		      opt |= PERL_UNICODE_STDOUT_FLAG;	break;
		 case PERL_UNICODE_STDERR:
		      opt |= PERL_UNICODE_STDERR_FLAG;	break;
		 case PERL_UNICODE_STD:
		      opt |= PERL_UNICODE_STD_FLAG;    	break;
		 case PERL_UNICODE_IN:
		      opt |= PERL_UNICODE_IN_FLAG;	break;
		 case PERL_UNICODE_OUT:
		      opt |= PERL_UNICODE_OUT_FLAG;	break;
		 case PERL_UNICODE_INOUT:
		      opt |= PERL_UNICODE_INOUT_FLAG;	break;
		 case PERL_UNICODE_LOCALE:
		      opt |= PERL_UNICODE_LOCALE_FLAG;	break;
		 case PERL_UNICODE_ARGV:
		      opt |= PERL_UNICODE_ARGV_FLAG;	break;
		 default:
		      if (*p != '\n' && *p != '\r')
			  Perl_croak(aTHX_
				     "Unknown Unicode option letter '%c'", *p);
		 }
	    }
       }
  }
  else
       opt = PERL_UNICODE_DEFAULT_FLAGS;

  if (opt & ~PERL_UNICODE_ALL_FLAGS)
       Perl_croak(aTHX_ "Unknown Unicode option value %"UVuf,
		  (UV) (opt & ~PERL_UNICODE_ALL_FLAGS));

  *popt = p;

  return opt;
}

U32
Perl_seed(pTHX)
{
    /*
     * This is really just a quick hack which grabs various garbage
     * values.  It really should be a real hash algorithm which
     * spreads the effect of every input bit onto every output bit,
     * if someone who knows about such things would bother to write it.
     * Might be a good idea to add that function to CORE as well.
     * No numbers below come from careful analysis or anything here,
     * except they are primes and SEED_C1 > 1E6 to get a full-width
     * value from (tv_sec * SEED_C1 + tv_usec).  The multipliers should
     * probably be bigger too.
     */
#if RANDBITS > 16
#  define SEED_C1	1000003
#define   SEED_C4	73819
#else
#  define SEED_C1	25747
#define   SEED_C4	20639
#endif
#define   SEED_C2	3
#define   SEED_C3	269
#define   SEED_C5	26107

#ifndef PERL_NO_DEV_RANDOM
    int fd;
#endif
    U32 u;
#ifdef VMS
#  include <starlet.h>
    /* when[] = (low 32 bits, high 32 bits) of time since epoch
     * in 100-ns units, typically incremented ever 10 ms.        */
    unsigned int when[2];
#else
#  ifdef HAS_GETTIMEOFDAY
    struct timeval when;
#  else
    Time_t when;
#  endif
#endif

/* This test is an escape hatch, this symbol isn't set by Configure. */
#ifndef PERL_NO_DEV_RANDOM
#ifndef PERL_RANDOM_DEVICE
   /* /dev/random isn't used by default because reads from it will block
    * if there isn't enough entropy available.  You can compile with
    * PERL_RANDOM_DEVICE to it if you'd prefer Perl to block until there
    * is enough real entropy to fill the seed. */
#  define PERL_RANDOM_DEVICE "/dev/urandom"
#endif
    fd = PerlLIO_open(PERL_RANDOM_DEVICE, 0);
    if (fd != -1) {
    	if (PerlLIO_read(fd, &u, sizeof u) != sizeof u)
	    u = 0;
	PerlLIO_close(fd);
	if (u)
	    return u;
    }
#endif

#ifdef VMS
    _ckvmssts(sys$gettim(when));
    u = (U32)SEED_C1 * when[0] + (U32)SEED_C2 * when[1];
#else
#  ifdef HAS_GETTIMEOFDAY
    PerlProc_gettimeofday(&when,NULL);
    u = (U32)SEED_C1 * when.tv_sec + (U32)SEED_C2 * when.tv_usec;
#  else
    (void)time(&when);
    u = (U32)SEED_C1 * when;
#  endif
#endif
    u += SEED_C3 * (U32)PerlProc_getpid();
    u += SEED_C4 * (U32)PTR2UV(PL_stack_sp);
#ifndef PLAN9           /* XXX Plan9 assembler chokes on this; fix needed  */
    u += SEED_C5 * (U32)PTR2UV(&when);
#endif
    return u;
}

UV
Perl_get_hash_seed(pTHX)
{
     char *s = PerlEnv_getenv("PERL_HASH_SEED");
     UV myseed = 0;

     if (s)
	  while (isSPACE(*s)) s++;
     if (s && isDIGIT(*s))
	  myseed = (UV)Atoul(s);
     else
#ifdef USE_HASH_SEED_EXPLICIT
     if (s)
#endif
     {
	  /* Compute a random seed */
	  (void)seedDrand01((Rand_seed_t)seed());
	  myseed = (UV)(Drand01() * (NV)UV_MAX);
#if RANDBITS < (UVSIZE * 8)
	  /* Since there are not enough randbits to to reach all
	   * the bits of a UV, the low bits might need extra
	   * help.  Sum in another random number that will
	   * fill in the low bits. */
	  myseed +=
	       (UV)(Drand01() * (NV)((1 << ((UVSIZE * 8 - RANDBITS))) - 1));
#endif /* RANDBITS < (UVSIZE * 8) */
	  if (myseed == 0) { /* Superparanoia. */
	      myseed = (UV)(Drand01() * (NV)UV_MAX); /* One more chance. */
	      if (myseed == 0)
		  Perl_croak(aTHX_ "Your random numbers are not that random");
	  }
     }
     PL_rehash_seed_set = TRUE;

     return myseed;
}
