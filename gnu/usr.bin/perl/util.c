/*    util.c
 *
 *    Copyright (C) 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001,
 *    2002, 2003, 2004, 2005, 2006, 2007, 2008 by Larry Wall and others
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/*
 * 'Very useful, no doubt, that was to Saruman; yet it seems that he was
 *  not content.'                                    --Gandalf to Pippin
 *
 *     [p.598 of _The Lord of the Rings_, III/xi: "The Palantír"]
 */

/* This file contains assorted utility routines.
 * Which is a polite way of saying any stuff that people couldn't think of
 * a better place for. Amongst other things, it includes the warning and
 * dieing stuff, plus wrappers for malloc code.
 */

#include "EXTERN.h"
#define PERL_IN_UTIL_C
#include "perl.h"
#include "reentr.h"

#if defined(USE_PERLIO)
#include "perliol.h" /* For PerlIOUnix_refcnt */
#endif

#ifndef PERL_MICRO
#include <signal.h>
#ifndef SIG_ERR
# define SIG_ERR ((Sighandler_t) -1)
#endif
#endif

#include <math.h>
#include <stdlib.h>

#ifdef __Lynx__
/* Missing protos on LynxOS */
int putenv(char *);
#endif

#ifdef HAS_SELECT
# ifdef I_SYS_SELECT
#  include <sys/select.h>
# endif
#endif

#ifdef PERL_DEBUG_READONLY_COW
# include <sys/mman.h>
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

#if defined (DEBUGGING) || defined(PERL_IMPLICIT_SYS) || defined (PERL_TRACK_MEMPOOL)
#  define ALWAYS_NEED_THX
#endif

#if defined(PERL_TRACK_MEMPOOL) && defined(PERL_DEBUG_READONLY_COW)
static void
S_maybe_protect_rw(pTHX_ struct perl_memory_debug_header *header)
{
    if (header->readonly
     && mprotect(header, header->size, PROT_READ|PROT_WRITE))
	Perl_warn(aTHX_ "mprotect for COW string %p %lu failed with %d",
			 header, header->size, errno);
}

static void
S_maybe_protect_ro(pTHX_ struct perl_memory_debug_header *header)
{
    if (header->readonly
     && mprotect(header, header->size, PROT_READ))
	Perl_warn(aTHX_ "mprotect RW for COW string %p %lu failed with %d",
			 header, header->size, errno);
}
# define maybe_protect_rw(foo) S_maybe_protect_rw(aTHX_ foo)
# define maybe_protect_ro(foo) S_maybe_protect_ro(aTHX_ foo)
#else
# define maybe_protect_rw(foo) NOOP
# define maybe_protect_ro(foo) NOOP
#endif

#if defined(PERL_TRACK_MEMPOOL) || defined(PERL_DEBUG_READONLY_COW)
 /* Use memory_debug_header */
# define USE_MDH
# if (defined(PERL_POISON) && defined(PERL_TRACK_MEMPOOL)) \
   || defined(PERL_DEBUG_READONLY_COW)
#  define MDH_HAS_SIZE
# endif
#endif

/* paranoid version of system's malloc() */

Malloc_t
Perl_safesysmalloc(MEM_SIZE size)
{
#ifdef ALWAYS_NEED_THX
    dTHX;
#endif
    Malloc_t ptr;
    size += PERL_MEMORY_DEBUG_HEADER_SIZE;
#ifdef DEBUGGING
    if ((SSize_t)size < 0)
	Perl_croak_nocontext("panic: malloc, size=%"UVuf, (UV) size);
#endif
    if (!size) size = 1;	/* malloc(0) is NASTY on our system */
#ifdef PERL_DEBUG_READONLY_COW
    if ((ptr = mmap(0, size, PROT_READ|PROT_WRITE,
		    MAP_ANON|MAP_PRIVATE, -1, 0)) == MAP_FAILED) {
	perror("mmap failed");
	abort();
    }
#else
    ptr = (Malloc_t)PerlMem_malloc(size?size:1);
#endif
    PERL_ALLOC_CHECK(ptr);
    if (ptr != NULL) {
#ifdef USE_MDH
	struct perl_memory_debug_header *const header
	    = (struct perl_memory_debug_header *)ptr;
#endif

#ifdef PERL_POISON
	PoisonNew(((char *)ptr), size, char);
#endif

#ifdef PERL_TRACK_MEMPOOL
	header->interpreter = aTHX;
	/* Link us into the list.  */
	header->prev = &PL_memory_debug_header;
	header->next = PL_memory_debug_header.next;
	PL_memory_debug_header.next = header;
	maybe_protect_rw(header->next);
	header->next->prev = header;
	maybe_protect_ro(header->next);
#  ifdef PERL_DEBUG_READONLY_COW
	header->readonly = 0;
#  endif
#endif
#ifdef MDH_HAS_SIZE
	header->size = size;
#endif
        ptr = (Malloc_t)((char*)ptr+PERL_MEMORY_DEBUG_HEADER_SIZE);
	DEBUG_m(PerlIO_printf(Perl_debug_log, "0x%"UVxf": (%05ld) malloc %ld bytes\n",PTR2UV(ptr),(long)PL_an++,(long)size));
	return ptr;
}
    else {
#ifndef ALWAYS_NEED_THX
	dTHX;
#endif
	if (PL_nomemok)
	    return NULL;
	else {
	    croak_no_mem();
	}
    }
    /*NOTREACHED*/
}

/* paranoid version of system's realloc() */

Malloc_t
Perl_safesysrealloc(Malloc_t where,MEM_SIZE size)
{
#ifdef ALWAYS_NEED_THX
    dTHX;
#endif
    Malloc_t ptr;
#ifdef PERL_DEBUG_READONLY_COW
    const MEM_SIZE oldsize = where
	? ((struct perl_memory_debug_header *)((char *)where - PERL_MEMORY_DEBUG_HEADER_SIZE))->size
	: 0;
#endif
#if !defined(STANDARD_C) && !defined(HAS_REALLOC_PROTOTYPE) && !defined(PERL_MICRO)
    Malloc_t PerlMem_realloc();
#endif /* !defined(STANDARD_C) && !defined(HAS_REALLOC_PROTOTYPE) */

    if (!size) {
	safesysfree(where);
	return NULL;
    }

    if (!where)
	return safesysmalloc(size);
#ifdef USE_MDH
    where = (Malloc_t)((char*)where-PERL_MEMORY_DEBUG_HEADER_SIZE);
    size += PERL_MEMORY_DEBUG_HEADER_SIZE;
    {
	struct perl_memory_debug_header *const header
	    = (struct perl_memory_debug_header *)where;

# ifdef PERL_TRACK_MEMPOOL
	if (header->interpreter != aTHX) {
	    Perl_croak_nocontext("panic: realloc from wrong pool, %p!=%p",
				 header->interpreter, aTHX);
	}
	assert(header->next->prev == header);
	assert(header->prev->next == header);
#  ifdef PERL_POISON
	if (header->size > size) {
	    const MEM_SIZE freed_up = header->size - size;
	    char *start_of_freed = ((char *)where) + size;
	    PoisonFree(start_of_freed, freed_up, char);
	}
#  endif
# endif
# ifdef MDH_HAS_SIZE
	header->size = size;
# endif
    }
#endif
#ifdef DEBUGGING
    if ((SSize_t)size < 0)
	Perl_croak_nocontext("panic: realloc, size=%"UVuf, (UV)size);
#endif
#ifdef PERL_DEBUG_READONLY_COW
    if ((ptr = mmap(0, size, PROT_READ|PROT_WRITE,
		    MAP_ANON|MAP_PRIVATE, -1, 0)) == MAP_FAILED) {
	perror("mmap failed");
	abort();
    }
    Copy(where,ptr,oldsize < size ? oldsize : size,char);
    if (munmap(where, oldsize)) {
	perror("munmap failed");
	abort();
    }
#else
    ptr = (Malloc_t)PerlMem_realloc(where,size);
#endif
    PERL_ALLOC_CHECK(ptr);

    /* MUST do this fixup first, before doing ANYTHING else, as anything else
       might allocate memory/free/move memory, and until we do the fixup, it
       may well be chasing (and writing to) free memory.  */
    if (ptr != NULL) {
#ifdef PERL_TRACK_MEMPOOL
	struct perl_memory_debug_header *const header
	    = (struct perl_memory_debug_header *)ptr;

#  ifdef PERL_POISON
	if (header->size < size) {
	    const MEM_SIZE fresh = size - header->size;
	    char *start_of_fresh = ((char *)ptr) + size;
	    PoisonNew(start_of_fresh, fresh, char);
	}
#  endif

	maybe_protect_rw(header->next);
	header->next->prev = header;
	maybe_protect_ro(header->next);
	maybe_protect_rw(header->prev);
	header->prev->next = header;
	maybe_protect_ro(header->prev);
#endif
        ptr = (Malloc_t)((char*)ptr+PERL_MEMORY_DEBUG_HEADER_SIZE);
    }

    /* In particular, must do that fixup above before logging anything via
     *printf(), as it can reallocate memory, which can cause SEGVs.  */

    DEBUG_m(PerlIO_printf(Perl_debug_log, "0x%"UVxf": (%05ld) rfree\n",PTR2UV(where),(long)PL_an++));
    DEBUG_m(PerlIO_printf(Perl_debug_log, "0x%"UVxf": (%05ld) realloc %ld bytes\n",PTR2UV(ptr),(long)PL_an++,(long)size));


    if (ptr != NULL) {
	return ptr;
    }
    else {
#ifndef ALWAYS_NEED_THX
	dTHX;
#endif
	if (PL_nomemok)
	    return NULL;
	else {
	    croak_no_mem();
	}
    }
    /*NOTREACHED*/
}

/* safe version of system's free() */

Free_t
Perl_safesysfree(Malloc_t where)
{
#ifdef ALWAYS_NEED_THX
    dTHX;
#else
    dVAR;
#endif
    DEBUG_m( PerlIO_printf(Perl_debug_log, "0x%"UVxf": (%05ld) free\n",PTR2UV(where),(long)PL_an++));
    if (where) {
#ifdef USE_MDH
        where = (Malloc_t)((char*)where-PERL_MEMORY_DEBUG_HEADER_SIZE);
	{
	    struct perl_memory_debug_header *const header
		= (struct perl_memory_debug_header *)where;

# ifdef MDH_HAS_SIZE
	    const MEM_SIZE size = header->size;
# endif
# ifdef PERL_TRACK_MEMPOOL
	    if (header->interpreter != aTHX) {
		Perl_croak_nocontext("panic: free from wrong pool, %p!=%p",
				     header->interpreter, aTHX);
	    }
	    if (!header->prev) {
		Perl_croak_nocontext("panic: duplicate free");
	    }
	    if (!(header->next))
		Perl_croak_nocontext("panic: bad free, header->next==NULL");
	    if (header->next->prev != header || header->prev->next != header) {
		Perl_croak_nocontext("panic: bad free, ->next->prev=%p, "
				     "header=%p, ->prev->next=%p",
				     header->next->prev, header,
				     header->prev->next);
	    }
	    /* Unlink us from the chain.  */
	    maybe_protect_rw(header->next);
	    header->next->prev = header->prev;
	    maybe_protect_ro(header->next);
	    maybe_protect_rw(header->prev);
	    header->prev->next = header->next;
	    maybe_protect_ro(header->prev);
	    maybe_protect_rw(header);
#  ifdef PERL_POISON
	    PoisonNew(where, size, char);
#  endif
	    /* Trigger the duplicate free warning.  */
	    header->next = NULL;
# endif
# ifdef PERL_DEBUG_READONLY_COW
	    if (munmap(where, size)) {
		perror("munmap failed");
		abort();
	    }	
# endif
	}
#endif
#ifndef PERL_DEBUG_READONLY_COW
	PerlMem_free(where);
#endif
    }
}

/* safe version of system's calloc() */

Malloc_t
Perl_safesyscalloc(MEM_SIZE count, MEM_SIZE size)
{
#ifdef ALWAYS_NEED_THX
    dTHX;
#endif
    Malloc_t ptr;
#if defined(USE_MDH) || defined(DEBUGGING)
    MEM_SIZE total_size = 0;
#endif

    /* Even though calloc() for zero bytes is strange, be robust. */
    if (size && (count <= MEM_SIZE_MAX / size)) {
#if defined(USE_MDH) || defined(DEBUGGING)
	total_size = size * count;
#endif
    }
    else
	croak_memory_wrap();
#ifdef USE_MDH
    if (PERL_MEMORY_DEBUG_HEADER_SIZE <= MEM_SIZE_MAX - (MEM_SIZE)total_size)
	total_size += PERL_MEMORY_DEBUG_HEADER_SIZE;
    else
	croak_memory_wrap();
#endif
#ifdef DEBUGGING
    if ((SSize_t)size < 0 || (SSize_t)count < 0)
	Perl_croak_nocontext("panic: calloc, size=%"UVuf", count=%"UVuf,
			     (UV)size, (UV)count);
#endif
#ifdef PERL_DEBUG_READONLY_COW
    if ((ptr = mmap(0, total_size ? total_size : 1, PROT_READ|PROT_WRITE,
		    MAP_ANON|MAP_PRIVATE, -1, 0)) == MAP_FAILED) {
	perror("mmap failed");
	abort();
    }
#elif defined(PERL_TRACK_MEMPOOL)
    /* Have to use malloc() because we've added some space for our tracking
       header.  */
    /* malloc(0) is non-portable. */
    ptr = (Malloc_t)PerlMem_malloc(total_size ? total_size : 1);
#else
    /* Use calloc() because it might save a memset() if the memory is fresh
       and clean from the OS.  */
    if (count && size)
	ptr = (Malloc_t)PerlMem_calloc(count, size);
    else /* calloc(0) is non-portable. */
	ptr = (Malloc_t)PerlMem_calloc(count ? count : 1, size ? size : 1);
#endif
    PERL_ALLOC_CHECK(ptr);
    DEBUG_m(PerlIO_printf(Perl_debug_log, "0x%"UVxf": (%05ld) calloc %ld x %ld bytes\n",PTR2UV(ptr),(long)PL_an++,(long)count,(long)total_size));
    if (ptr != NULL) {
#ifdef USE_MDH
	{
	    struct perl_memory_debug_header *const header
		= (struct perl_memory_debug_header *)ptr;

#  ifndef PERL_DEBUG_READONLY_COW
	    memset((void*)ptr, 0, total_size);
#  endif
#  ifdef PERL_TRACK_MEMPOOL
	    header->interpreter = aTHX;
	    /* Link us into the list.  */
	    header->prev = &PL_memory_debug_header;
	    header->next = PL_memory_debug_header.next;
	    PL_memory_debug_header.next = header;
	    maybe_protect_rw(header->next);
	    header->next->prev = header;
	    maybe_protect_ro(header->next);
#    ifdef PERL_DEBUG_READONLY_COW
	    header->readonly = 0;
#    endif
#  endif
#  ifdef MDH_HAS_SIZE
	    header->size = total_size;
#  endif
	    ptr = (Malloc_t)((char*)ptr+PERL_MEMORY_DEBUG_HEADER_SIZE);
	}
#endif
	return ptr;
    }
    else {
#ifndef ALWAYS_NEED_THX
	dTHX;
#endif
	if (PL_nomemok)
	    return NULL;
	croak_no_mem();
    }
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
Perl_delimcpy(char *to, const char *toend, const char *from, const char *fromend, int delim, I32 *retlen)
{
    I32 tolen;

    PERL_ARGS_ASSERT_DELIMCPY;

    for (tolen = 0; from < fromend; from++, tolen++) {
	if (*from == '\\') {
	    if (from[1] != delim) {
		if (to < toend)
		    *to++ = *from;
		tolen++;
	    }
	    from++;
	}
	else if (*from == delim)
	    break;
	if (to < toend)
	    *to++ = *from;
    }
    if (to < toend)
	*to = '\0';
    *retlen = tolen;
    return (char *)from;
}

/* return ptr to little string in big string, NULL if not found */
/* This routine was donated by Corey Satten. */

char *
Perl_instr(const char *big, const char *little)
{

    PERL_ARGS_ASSERT_INSTR;

    /* libc prior to 4.6.27 (late 1994) did not work properly on a NULL
     * 'little' */
    if (!little)
	return (char*)big;
    return strstr((char*)big, (char*)little);
}

/* same as instr but allow embedded nulls.  The end pointers point to 1 beyond
 * the final character desired to be checked */

char *
Perl_ninstr(const char *big, const char *bigend, const char *little, const char *lend)
{
    PERL_ARGS_ASSERT_NINSTR;
    if (little >= lend)
        return (char*)big;
    {
        const char first = *little;
        const char *s, *x;
        bigend -= lend - little++;
    OUTER:
        while (big <= bigend) {
            if (*big++ == first) {
                for (x=big,s=little; s < lend; x++,s++) {
                    if (*s != *x)
                        goto OUTER;
                }
                return (char*)(big-1);
            }
        }
    }
    return NULL;
}

/* reverse of the above--find last substring */

char *
Perl_rninstr(const char *big, const char *bigend, const char *little, const char *lend)
{
    const char *bigbeg;
    const I32 first = *little;
    const char * const littleend = lend;

    PERL_ARGS_ASSERT_RNINSTR;

    if (little >= littleend)
	return (char*)bigend;
    bigbeg = big;
    big = bigend - (littleend - little++);
    while (big >= bigbeg) {
	const char *s, *x;
	if (*big-- != first)
	    continue;
	for (x=big+2,s=little; s < littleend; /**/ ) {
	    if (*s != *x)
		break;
	    else {
		x++;
		s++;
	    }
	}
	if (s >= littleend)
	    return (char*)(big+1);
    }
    return NULL;
}

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
    dVAR;
    const U8 *s;
    STRLEN i;
    STRLEN len;
    U32 frequency = 256;
    MAGIC *mg;
    PERL_DEB( STRLEN rarest = 0 );

    PERL_ARGS_ASSERT_FBM_COMPILE;

    if (isGV_with_GP(sv) || SvROK(sv))
	return;

    if (SvVALID(sv))
	return;

    if (flags & FBMcf_TAIL) {
	MAGIC * const mg = SvUTF8(sv) && SvMAGICAL(sv) ? mg_find(sv, PERL_MAGIC_utf8) : NULL;
	sv_catpvs(sv, "\n");		/* Taken into account in fbm_instr() */
	if (mg && mg->mg_len >= 0)
	    mg->mg_len++;
    }
    if (!SvPOK(sv) || SvNIOKp(sv))
	s = (U8*)SvPV_force_mutable(sv, len);
    else s = (U8 *)SvPV_mutable(sv, len);
    if (len == 0)		/* TAIL might be on a zero-length string. */
	return;
    SvUPGRADE(sv, SVt_PVMG);
    SvIOK_off(sv);
    SvNOK_off(sv);
    SvVALID_on(sv);

    /* "deep magic", the comment used to add. The use of MAGIC itself isn't
       really. MAGIC was originally added in 79072805bf63abe5 (perl 5.0 alpha 2)
       to call SvVALID_off() if the scalar was assigned to.

       The comment itself (and "deeper magic" below) date back to
       378cc40b38293ffc (perl 2.0). "deep magic" was an annotation on
       str->str_pok |= 2;
       where the magic (presumably) was that the scalar had a BM table hidden
       inside itself.

       As MAGIC is always present on BMs [in Perl 5 :-)], we can use it to store
       the table instead of the previous (somewhat hacky) approach of co-opting
       the string buffer and storing it after the string.  */

    assert(!mg_find(sv, PERL_MAGIC_bm));
    mg = sv_magicext(sv, NULL, PERL_MAGIC_bm, &PL_vtbl_bm, NULL, 0);
    assert(mg);

    if (len > 2) {
	/* Shorter strings are special-cased in Perl_fbm_instr(), and don't use
	   the BM table.  */
	const U8 mlen = (len>255) ? 255 : (U8)len;
	const unsigned char *const sb = s + len - mlen; /* first char (maybe) */
	U8 *table;

	Newx(table, 256, U8);
	memset((void*)table, mlen, 256);
	mg->mg_ptr = (char *)table;
	mg->mg_len = 256;

	s += len - 1; /* last char */
	i = 0;
	while (s >= sb) {
	    if (table[*s] == mlen)
		table[*s] = (U8)i;
	    s--, i++;
	}
    }

    s = (const unsigned char*)(SvPVX_const(sv));	/* deeper magic */
    for (i = 0; i < len; i++) {
	if (PL_freq[s[i]] < frequency) {
	    PERL_DEB( rarest = i );
	    frequency = PL_freq[s[i]];
	}
    }
    BmUSEFUL(sv) = 100;			/* Initial value */
    if (flags & FBMcf_TAIL)
	SvTAIL_on(sv);
    DEBUG_r(PerlIO_printf(Perl_debug_log, "rarest char %c at %"UVuf"\n",
			  s[rarest], (UV)rarest));
}

/* If SvTAIL(littlestr), it has a fake '\n' at end. */
/* If SvTAIL is actually due to \Z or \z, this gives false positives
   if multiline */

/*
=for apidoc fbm_instr

Returns the location of the SV in the string delimited by C<big> and
C<bigend>.  It returns C<NULL> if the string can't be found.  The C<sv>
does not have to be fbm_compiled, but the search will not be as fast
then.

=cut
*/

char *
Perl_fbm_instr(pTHX_ unsigned char *big, unsigned char *bigend, SV *littlestr, U32 flags)
{
    unsigned char *s;
    STRLEN l;
    const unsigned char *little = (const unsigned char *)SvPV_const(littlestr,l);
    STRLEN littlelen = l;
    const I32 multiline = flags & FBMrf_MULTILINE;

    PERL_ARGS_ASSERT_FBM_INSTR;

    if ((STRLEN)(bigend - big) < littlelen) {
	if ( SvTAIL(littlestr)
	     && ((STRLEN)(bigend - big) == littlelen - 1)
	     && (littlelen == 1
		 || (*big == *little &&
		     memEQ((char *)big, (char *)little, littlelen - 1))))
	    return (char*)big;
	return NULL;
    }

    switch (littlelen) { /* Special cases for 0, 1 and 2  */
    case 0:
	return (char*)big;		/* Cannot be SvTAIL! */
    case 1:
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
	    return NULL;
    case 2:
	if (SvTAIL(littlestr) && !multiline) {
	    if (bigend[-1] == '\n' && bigend[-2] == *little)
		return (char*)bigend - 2;
	    if (bigend[-1] == *little)
		return (char*)bigend - 1;
	    return NULL;
	}
	{
	    /* This should be better than FBM if c1 == c2, and almost
	       as good otherwise: maybe better since we do less indirection.
	       And we save a lot of memory by caching no table. */
	    const unsigned char c1 = little[0];
	    const unsigned char c2 = little[1];

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
	return NULL;
    default:
	break; /* Only lengths 0 1 and 2 have special-case code.  */
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
	return NULL;
    }
    if (!SvVALID(littlestr)) {
	char * const b = ninstr((char*)big,(char*)bigend,
			 (char*)little, (char*)little + littlelen);

	if (!b && SvTAIL(littlestr)) {	/* Automatically multiline!  */
	    /* Chop \n from littlestr: */
	    s = bigend - littlelen + 1;
	    if (*s == *little
		&& memEQ((char*)s + 1, (char*)little + 1, littlelen - 2))
	    {
		return (char*)s;
	    }
	    return NULL;
	}
	return b;
    }

    /* Do actual FBM.  */
    if (littlelen > (STRLEN)(bigend - big))
	return NULL;

    {
	const MAGIC *const mg = mg_find(littlestr, PERL_MAGIC_bm);
	const unsigned char * const table = (const unsigned char *) mg->mg_ptr;
	const unsigned char *oldlittle;

	--littlelen;			/* Last char found by table lookup */

	s = big + littlelen;
	little += littlelen;		/* last char */
	oldlittle = little;
	if (s < bigend) {
	    I32 tmp;

	  top2:
	    if ((tmp = table[*s])) {
		if ((s += tmp) < bigend)
		    goto top2;
		goto check_end;
	    }
	    else {		/* less expensive than calling strncmp() */
		unsigned char * const olds = s;

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
	if ( s == bigend
	     && SvTAIL(littlestr)
	     && memEQ((char *)(bigend - littlelen),
		      (char *)(oldlittle - littlelen), littlelen) )
	    return (char*)bigend - littlelen;
	return NULL;
    }
}

char *
Perl_screaminstr(pTHX_ SV *bigstr, SV *littlestr, I32 start_shift, I32 end_shift, I32 *old_posp, I32 last)
{
    dVAR;
    PERL_ARGS_ASSERT_SCREAMINSTR;
    PERL_UNUSED_ARG(bigstr);
    PERL_UNUSED_ARG(littlestr);
    PERL_UNUSED_ARG(start_shift);
    PERL_UNUSED_ARG(end_shift);
    PERL_UNUSED_ARG(old_posp);
    PERL_UNUSED_ARG(last);

    /* This function must only ever be called on a scalar with study magic,
       but those do not happen any more. */
    Perl_croak(aTHX_ "panic: screaminstr");
    return NULL;
}

/*
=for apidoc foldEQ

Returns true if the leading len bytes of the strings s1 and s2 are the same
case-insensitively; false otherwise.  Uppercase and lowercase ASCII range bytes
match themselves and their opposite case counterparts.  Non-cased and non-ASCII
range bytes match only themselves.

=cut
*/


I32
Perl_foldEQ(const char *s1, const char *s2, I32 len)
{
    const U8 *a = (const U8 *)s1;
    const U8 *b = (const U8 *)s2;

    PERL_ARGS_ASSERT_FOLDEQ;

    assert(len >= 0);

    while (len--) {
	if (*a != *b && *a != PL_fold[*b])
	    return 0;
	a++,b++;
    }
    return 1;
}
I32
Perl_foldEQ_latin1(const char *s1, const char *s2, I32 len)
{
    /* Compare non-utf8 using Unicode (Latin1) semantics.  Does not work on
     * MICRO_SIGN, LATIN_SMALL_LETTER_SHARP_S, nor
     * LATIN_SMALL_LETTER_Y_WITH_DIAERESIS, and does not check for these.  Nor
     * does it check that the strings each have at least 'len' characters */

    const U8 *a = (const U8 *)s1;
    const U8 *b = (const U8 *)s2;

    PERL_ARGS_ASSERT_FOLDEQ_LATIN1;

    assert(len >= 0);

    while (len--) {
	if (*a != *b && *a != PL_fold_latin1[*b]) {
	    return 0;
	}
	a++, b++;
    }
    return 1;
}

/*
=for apidoc foldEQ_locale

Returns true if the leading len bytes of the strings s1 and s2 are the same
case-insensitively in the current locale; false otherwise.

=cut
*/

I32
Perl_foldEQ_locale(const char *s1, const char *s2, I32 len)
{
    dVAR;
    const U8 *a = (const U8 *)s1;
    const U8 *b = (const U8 *)s2;

    PERL_ARGS_ASSERT_FOLDEQ_LOCALE;

    assert(len >= 0);

    while (len--) {
	if (*a != *b && *a != PL_fold_locale[*b])
	    return 0;
	a++,b++;
    }
    return 1;
}

/* copy a string to a safe spot */

/*
=head1 Memory Management

=for apidoc savepv

Perl's version of C<strdup()>.  Returns a pointer to a newly allocated
string which is a duplicate of C<pv>.  The size of the string is
determined by C<strlen()>, which means it may not contain embedded C<NUL>
characters and must have a trailing C<NUL>.  The memory allocated for the new
string can be freed with the C<Safefree()> function.

On some platforms, Windows for example, all allocated memory owned by a thread
is deallocated when that thread ends.  So if you need that not to happen, you
need to use the shared memory functions, such as C<L</savesharedpv>>.

=cut
*/

char *
Perl_savepv(pTHX_ const char *pv)
{
    PERL_UNUSED_CONTEXT;
    if (!pv)
	return NULL;
    else {
	char *newaddr;
	const STRLEN pvlen = strlen(pv)+1;
	Newx(newaddr, pvlen, char);
	return (char*)memcpy(newaddr, pv, pvlen);
    }
}

/* same thing but with a known length */

/*
=for apidoc savepvn

Perl's version of what C<strndup()> would be if it existed.  Returns a
pointer to a newly allocated string which is a duplicate of the first
C<len> bytes from C<pv>, plus a trailing
C<NUL> byte.  The memory allocated for
the new string can be freed with the C<Safefree()> function.

On some platforms, Windows for example, all allocated memory owned by a thread
is deallocated when that thread ends.  So if you need that not to happen, you
need to use the shared memory functions, such as C<L</savesharedpvn>>.

=cut
*/

char *
Perl_savepvn(pTHX_ const char *pv, I32 len)
{
    char *newaddr;
    PERL_UNUSED_CONTEXT;

    assert(len >= 0);

    Newx(newaddr,len+1,char);
    /* Give a meaning to NULL pointer mainly for the use in sv_magic() */
    if (pv) {
	/* might not be null terminated */
    	newaddr[len] = '\0';
    	return (char *) CopyD(pv,newaddr,len,char);
    }
    else {
	return (char *) ZeroD(newaddr,len+1,char);
    }
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
    char *newaddr;
    STRLEN pvlen;
    if (!pv)
	return NULL;

    pvlen = strlen(pv)+1;
    newaddr = (char*)PerlMemShared_malloc(pvlen);
    if (!newaddr) {
	croak_no_mem();
    }
    return (char*)memcpy(newaddr, pv, pvlen);
}

/*
=for apidoc savesharedpvn

A version of C<savepvn()> which allocates the duplicate string in memory
which is shared between threads.  (With the specific difference that a NULL
pointer is not acceptable)

=cut
*/
char *
Perl_savesharedpvn(pTHX_ const char *const pv, const STRLEN len)
{
    char *const newaddr = (char*)PerlMemShared_malloc(len + 1);

    /* PERL_ARGS_ASSERT_SAVESHAREDPVN; */

    if (!newaddr) {
	croak_no_mem();
    }
    newaddr[len] = '\0';
    return (char*)memcpy(newaddr, pv, len);
}

/*
=for apidoc savesvpv

A version of C<savepv()>/C<savepvn()> which gets the string to duplicate from
the passed in SV using C<SvPV()>

On some platforms, Windows for example, all allocated memory owned by a thread
is deallocated when that thread ends.  So if you need that not to happen, you
need to use the shared memory functions, such as C<L</savesharedsvpv>>.

=cut
*/

char *
Perl_savesvpv(pTHX_ SV *sv)
{
    STRLEN len;
    const char * const pv = SvPV_const(sv, len);
    char *newaddr;

    PERL_ARGS_ASSERT_SAVESVPV;

    ++len;
    Newx(newaddr,len,char);
    return (char *) CopyD(pv,newaddr,len,char);
}

/*
=for apidoc savesharedsvpv

A version of C<savesharedpv()> which allocates the duplicate string in
memory which is shared between threads.

=cut
*/

char *
Perl_savesharedsvpv(pTHX_ SV *sv)
{
    STRLEN len;
    const char * const pv = SvPV_const(sv, len);

    PERL_ARGS_ASSERT_SAVESHAREDSVPV;

    return savesharedpvn(pv, len);
}

/* the SV for Perl_form() and mess() is not kept in an arena */

STATIC SV *
S_mess_alloc(pTHX)
{
    dVAR;
    SV *sv;
    XPVMG *any;

    if (PL_phase != PERL_PHASE_DESTRUCT)
	return newSVpvs_flags("", SVs_TEMP);

    if (PL_mess_sv)
	return PL_mess_sv;

    /* Create as PVMG now, to avoid any upgrading later */
    Newx(sv, 1, SV);
    Newxz(any, 1, XPVMG);
    SvFLAGS(sv) = SVt_PVMG;
    SvANY(sv) = (void*)any;
    SvPV_set(sv, NULL);
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
    PERL_ARGS_ASSERT_FORM_NOCONTEXT;
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
    PERL_ARGS_ASSERT_FORM;
    va_start(args, pat);
    retval = vform(pat, &args);
    va_end(args);
    return retval;
}

char *
Perl_vform(pTHX_ const char *pat, va_list *args)
{
    SV * const sv = mess_alloc();
    PERL_ARGS_ASSERT_VFORM;
    sv_vsetpvfn(sv, pat, strlen(pat), args, NULL, 0, NULL);
    return SvPVX(sv);
}

/*
=for apidoc Am|SV *|mess|const char *pat|...

Take a sprintf-style format pattern and argument list.  These are used to
generate a string message.  If the message does not end with a newline,
then it will be extended with some indication of the current location
in the code, as described for L</mess_sv>.

Normally, the resulting message is returned in a new mortal SV.
During global destruction a single SV may be shared between uses of
this function.

=cut
*/

#if defined(PERL_IMPLICIT_CONTEXT)
SV *
Perl_mess_nocontext(const char *pat, ...)
{
    dTHX;
    SV *retval;
    va_list args;
    PERL_ARGS_ASSERT_MESS_NOCONTEXT;
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
    PERL_ARGS_ASSERT_MESS;
    va_start(args, pat);
    retval = vmess(pat, &args);
    va_end(args);
    return retval;
}

const COP*
Perl_closest_cop(pTHX_ const COP *cop, const OP *o, const OP *curop,
		       bool opnext)
{
    dVAR;
    /* Look for curop starting from o.  cop is the last COP we've seen. */
    /* opnext means that curop is actually the ->op_next of the op we are
       seeking. */

    PERL_ARGS_ASSERT_CLOSEST_COP;

    if (!o || !curop || (
	opnext ? o->op_next == curop && o->op_type != OP_SCOPE : o == curop
    ))
	return cop;

    if (o->op_flags & OPf_KIDS) {
	const OP *kid;
	for (kid = cUNOPo->op_first; kid; kid = kid->op_sibling) {
	    const COP *new_cop;

	    /* If the OP_NEXTSTATE has been optimised away we can still use it
	     * the get the file and line number. */

	    if (kid->op_type == OP_NULL && kid->op_targ == OP_NEXTSTATE)
		cop = (const COP *)kid;

	    /* Keep searching, and return when we've found something. */

	    new_cop = closest_cop(cop, kid, curop, opnext);
	    if (new_cop)
		return new_cop;
	}
    }

    /* Nothing found. */

    return NULL;
}

/*
=for apidoc Am|SV *|mess_sv|SV *basemsg|bool consume

Expands a message, intended for the user, to include an indication of
the current location in the code, if the message does not already appear
to be complete.

C<basemsg> is the initial message or object.  If it is a reference, it
will be used as-is and will be the result of this function.  Otherwise it
is used as a string, and if it already ends with a newline, it is taken
to be complete, and the result of this function will be the same string.
If the message does not end with a newline, then a segment such as C<at
foo.pl line 37> will be appended, and possibly other clauses indicating
the current state of execution.  The resulting message will end with a
dot and a newline.

Normally, the resulting message is returned in a new mortal SV.
During global destruction a single SV may be shared between uses of this
function.  If C<consume> is true, then the function is permitted (but not
required) to modify and return C<basemsg> instead of allocating a new SV.

=cut
*/

SV *
Perl_mess_sv(pTHX_ SV *basemsg, bool consume)
{
    dVAR;
    SV *sv;

    PERL_ARGS_ASSERT_MESS_SV;

    if (SvROK(basemsg)) {
	if (consume) {
	    sv = basemsg;
	}
	else {
	    sv = mess_alloc();
	    sv_setsv(sv, basemsg);
	}
	return sv;
    }

    if (SvPOK(basemsg) && consume) {
	sv = basemsg;
    }
    else {
	sv = mess_alloc();
	sv_copypv(sv, basemsg);
    }

    if (!SvCUR(sv) || *(SvEND(sv) - 1) != '\n') {
	/*
	 * Try and find the file and line for PL_op.  This will usually be
	 * PL_curcop, but it might be a cop that has been optimised away.  We
	 * can try to find such a cop by searching through the optree starting
	 * from the sibling of PL_curcop.
	 */

	const COP *cop =
	    closest_cop(PL_curcop, PL_curcop->op_sibling, PL_op, FALSE);
	if (!cop)
	    cop = PL_curcop;

	if (CopLINE(cop))
	    Perl_sv_catpvf(aTHX_ sv, " at %s line %"IVdf,
	    OutCopFILE(cop), (IV)CopLINE(cop));
	/* Seems that GvIO() can be untrustworthy during global destruction. */
	if (GvIO(PL_last_in_gv) && (SvTYPE(GvIOp(PL_last_in_gv)) == SVt_PVIO)
		&& IoLINES(GvIOp(PL_last_in_gv)))
	{
	    STRLEN l;
	    const bool line_mode = (RsSIMPLE(PL_rs) &&
				   *SvPV_const(PL_rs,l) == '\n' && l == 1);
	    Perl_sv_catpvf(aTHX_ sv, ", <%"SVf"> %s %"IVdf,
			   SVfARG(PL_last_in_gv == PL_argvgv
                                 ? &PL_sv_no
                                 : sv_2mortal(newSVhek(GvNAME_HEK(PL_last_in_gv)))),
			   line_mode ? "line" : "chunk",
			   (IV)IoLINES(GvIOp(PL_last_in_gv)));
	}
	if (PL_phase == PERL_PHASE_DESTRUCT)
	    sv_catpvs(sv, " during global destruction");
	sv_catpvs(sv, ".\n");
    }
    return sv;
}

/*
=for apidoc Am|SV *|vmess|const char *pat|va_list *args

C<pat> and C<args> are a sprintf-style format pattern and encapsulated
argument list.  These are used to generate a string message.  If the
message does not end with a newline, then it will be extended with
some indication of the current location in the code, as described for
L</mess_sv>.

Normally, the resulting message is returned in a new mortal SV.
During global destruction a single SV may be shared between uses of
this function.

=cut
*/

SV *
Perl_vmess(pTHX_ const char *pat, va_list *args)
{
    dVAR;
    SV * const sv = mess_alloc();

    PERL_ARGS_ASSERT_VMESS;

    sv_vsetpvfn(sv, pat, strlen(pat), args, NULL, 0, NULL);
    return mess_sv(sv, 1);
}

void
Perl_write_to_stderr(pTHX_ SV* msv)
{
    dVAR;
    IO *io;
    MAGIC *mg;

    PERL_ARGS_ASSERT_WRITE_TO_STDERR;

    if (PL_stderrgv && SvREFCNT(PL_stderrgv) 
	&& (io = GvIO(PL_stderrgv))
	&& (mg = SvTIED_mg((const SV *)io, PERL_MAGIC_tiedscalar))) 
	Perl_magic_methcall(aTHX_ MUTABLE_SV(io), mg, SV_CONST(PRINT),
			    G_SCALAR | G_DISCARD | G_WRITING_TO_STDERR, 1, msv);
    else {
	PerlIO * const serr = Perl_error_log;

	do_print(msv, serr);
	(void)PerlIO_flush(serr);
    }
}

/*
=head1 Warning and Dieing
*/

/* Common code used in dieing and warning */

STATIC SV *
S_with_queued_errors(pTHX_ SV *ex)
{
    PERL_ARGS_ASSERT_WITH_QUEUED_ERRORS;
    if (PL_errors && SvCUR(PL_errors) && !SvROK(ex)) {
	sv_catsv(PL_errors, ex);
	ex = sv_mortalcopy(PL_errors);
	SvCUR_set(PL_errors, 0);
    }
    return ex;
}

STATIC bool
S_invoke_exception_hook(pTHX_ SV *ex, bool warn)
{
    dVAR;
    HV *stash;
    GV *gv;
    CV *cv;
    SV **const hook = warn ? &PL_warnhook : &PL_diehook;
    /* sv_2cv might call Perl_croak() or Perl_warner() */
    SV * const oldhook = *hook;

    if (!oldhook)
	return FALSE;

    ENTER;
    SAVESPTR(*hook);
    *hook = NULL;
    cv = sv_2cv(oldhook, &stash, &gv, 0);
    LEAVE;
    if (cv && !CvDEPTH(cv) && (CvROOT(cv) || CvXSUB(cv))) {
	dSP;
	SV *exarg;

	ENTER;
	save_re_context();
	if (warn) {
	    SAVESPTR(*hook);
	    *hook = NULL;
	}
	exarg = newSVsv(ex);
	SvREADONLY_on(exarg);
	SAVEFREESV(exarg);

	PUSHSTACKi(warn ? PERLSI_WARNHOOK : PERLSI_DIEHOOK);
	PUSHMARK(SP);
	XPUSHs(exarg);
	PUTBACK;
	call_sv(MUTABLE_SV(cv), G_DISCARD);
	POPSTACK;
	LEAVE;
	return TRUE;
    }
    return FALSE;
}

/*
=for apidoc Am|OP *|die_sv|SV *baseex

Behaves the same as L</croak_sv>, except for the return type.
It should be used only where the C<OP *> return type is required.
The function never actually returns.

=cut
*/

OP *
Perl_die_sv(pTHX_ SV *baseex)
{
    PERL_ARGS_ASSERT_DIE_SV;
    croak_sv(baseex);
    assert(0); /* NOTREACHED */
    return NULL;
}

/*
=for apidoc Am|OP *|die|const char *pat|...

Behaves the same as L</croak>, except for the return type.
It should be used only where the C<OP *> return type is required.
The function never actually returns.

=cut
*/

#if defined(PERL_IMPLICIT_CONTEXT)
OP *
Perl_die_nocontext(const char* pat, ...)
{
    dTHX;
    va_list args;
    va_start(args, pat);
    vcroak(pat, &args);
    assert(0); /* NOTREACHED */
    va_end(args);
    return NULL;
}
#endif /* PERL_IMPLICIT_CONTEXT */

OP *
Perl_die(pTHX_ const char* pat, ...)
{
    va_list args;
    va_start(args, pat);
    vcroak(pat, &args);
    assert(0); /* NOTREACHED */
    va_end(args);
    return NULL;
}

/*
=for apidoc Am|void|croak_sv|SV *baseex

This is an XS interface to Perl's C<die> function.

C<baseex> is the error message or object.  If it is a reference, it
will be used as-is.  Otherwise it is used as a string, and if it does
not end with a newline then it will be extended with some indication of
the current location in the code, as described for L</mess_sv>.

The error message or object will be used as an exception, by default
returning control to the nearest enclosing C<eval>, but subject to
modification by a C<$SIG{__DIE__}> handler.  In any case, the C<croak_sv>
function never returns normally.

To die with a simple string message, the L</croak> function may be
more convenient.

=cut
*/

void
Perl_croak_sv(pTHX_ SV *baseex)
{
    SV *ex = with_queued_errors(mess_sv(baseex, 0));
    PERL_ARGS_ASSERT_CROAK_SV;
    invoke_exception_hook(ex, FALSE);
    die_unwind(ex);
}

/*
=for apidoc Am|void|vcroak|const char *pat|va_list *args

This is an XS interface to Perl's C<die> function.

C<pat> and C<args> are a sprintf-style format pattern and encapsulated
argument list.  These are used to generate a string message.  If the
message does not end with a newline, then it will be extended with
some indication of the current location in the code, as described for
L</mess_sv>.

The error message will be used as an exception, by default
returning control to the nearest enclosing C<eval>, but subject to
modification by a C<$SIG{__DIE__}> handler.  In any case, the C<croak>
function never returns normally.

For historical reasons, if C<pat> is null then the contents of C<ERRSV>
(C<$@>) will be used as an error message or object instead of building an
error message from arguments.  If you want to throw a non-string object,
or build an error message in an SV yourself, it is preferable to use
the L</croak_sv> function, which does not involve clobbering C<ERRSV>.

=cut
*/

void
Perl_vcroak(pTHX_ const char* pat, va_list *args)
{
    SV *ex = with_queued_errors(pat ? vmess(pat, args) : mess_sv(ERRSV, 0));
    invoke_exception_hook(ex, FALSE);
    die_unwind(ex);
}

/*
=for apidoc Am|void|croak|const char *pat|...

This is an XS interface to Perl's C<die> function.

Take a sprintf-style format pattern and argument list.  These are used to
generate a string message.  If the message does not end with a newline,
then it will be extended with some indication of the current location
in the code, as described for L</mess_sv>.

The error message will be used as an exception, by default
returning control to the nearest enclosing C<eval>, but subject to
modification by a C<$SIG{__DIE__}> handler.  In any case, the C<croak>
function never returns normally.

For historical reasons, if C<pat> is null then the contents of C<ERRSV>
(C<$@>) will be used as an error message or object instead of building an
error message from arguments.  If you want to throw a non-string object,
or build an error message in an SV yourself, it is preferable to use
the L</croak_sv> function, which does not involve clobbering C<ERRSV>.

=cut
*/

#if defined(PERL_IMPLICIT_CONTEXT)
void
Perl_croak_nocontext(const char *pat, ...)
{
    dTHX;
    va_list args;
    va_start(args, pat);
    vcroak(pat, &args);
    assert(0); /* NOTREACHED */
    va_end(args);
}
#endif /* PERL_IMPLICIT_CONTEXT */

void
Perl_croak(pTHX_ const char *pat, ...)
{
    va_list args;
    va_start(args, pat);
    vcroak(pat, &args);
    assert(0); /* NOTREACHED */
    va_end(args);
}

/*
=for apidoc Am|void|croak_no_modify

Exactly equivalent to C<Perl_croak(aTHX_ "%s", PL_no_modify)>, but generates
terser object code than using C<Perl_croak>.  Less code used on exception code
paths reduces CPU cache pressure.

=cut
*/

void
Perl_croak_no_modify(void)
{
    Perl_croak_nocontext( "%s", PL_no_modify);
}

/* does not return, used in util.c perlio.c and win32.c
   This is typically called when malloc returns NULL.
*/
void
Perl_croak_no_mem(void)
{
    dTHX;
    /* Can't use PerlIO to write as it allocates memory */
    PERL_UNUSED_RESULT(PerlLIO_write(PerlIO_fileno(Perl_error_log),
		  PL_no_mem, sizeof(PL_no_mem)-1));
    my_exit(1);
}

/* does not return, used only in POPSTACK */
void
Perl_croak_popstack(void)
{
    dTHX;
    PerlIO_printf(Perl_error_log, "panic: POPSTACK\n");
    my_exit(1);
}

/*
=for apidoc Am|void|warn_sv|SV *baseex

This is an XS interface to Perl's C<warn> function.

C<baseex> is the error message or object.  If it is a reference, it
will be used as-is.  Otherwise it is used as a string, and if it does
not end with a newline then it will be extended with some indication of
the current location in the code, as described for L</mess_sv>.

The error message or object will by default be written to standard error,
but this is subject to modification by a C<$SIG{__WARN__}> handler.

To warn with a simple string message, the L</warn> function may be
more convenient.

=cut
*/

void
Perl_warn_sv(pTHX_ SV *baseex)
{
    SV *ex = mess_sv(baseex, 0);
    PERL_ARGS_ASSERT_WARN_SV;
    if (!invoke_exception_hook(ex, TRUE))
	write_to_stderr(ex);
}

/*
=for apidoc Am|void|vwarn|const char *pat|va_list *args

This is an XS interface to Perl's C<warn> function.

C<pat> and C<args> are a sprintf-style format pattern and encapsulated
argument list.  These are used to generate a string message.  If the
message does not end with a newline, then it will be extended with
some indication of the current location in the code, as described for
L</mess_sv>.

The error message or object will by default be written to standard error,
but this is subject to modification by a C<$SIG{__WARN__}> handler.

Unlike with L</vcroak>, C<pat> is not permitted to be null.

=cut
*/

void
Perl_vwarn(pTHX_ const char* pat, va_list *args)
{
    SV *ex = vmess(pat, args);
    PERL_ARGS_ASSERT_VWARN;
    if (!invoke_exception_hook(ex, TRUE))
	write_to_stderr(ex);
}

/*
=for apidoc Am|void|warn|const char *pat|...

This is an XS interface to Perl's C<warn> function.

Take a sprintf-style format pattern and argument list.  These are used to
generate a string message.  If the message does not end with a newline,
then it will be extended with some indication of the current location
in the code, as described for L</mess_sv>.

The error message or object will by default be written to standard error,
but this is subject to modification by a C<$SIG{__WARN__}> handler.

Unlike with L</croak>, C<pat> is not permitted to be null.

=cut
*/

#if defined(PERL_IMPLICIT_CONTEXT)
void
Perl_warn_nocontext(const char *pat, ...)
{
    dTHX;
    va_list args;
    PERL_ARGS_ASSERT_WARN_NOCONTEXT;
    va_start(args, pat);
    vwarn(pat, &args);
    va_end(args);
}
#endif /* PERL_IMPLICIT_CONTEXT */

void
Perl_warn(pTHX_ const char *pat, ...)
{
    va_list args;
    PERL_ARGS_ASSERT_WARN;
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
    PERL_ARGS_ASSERT_WARNER_NOCONTEXT;
    va_start(args, pat);
    vwarner(err, pat, &args);
    va_end(args);
}
#endif /* PERL_IMPLICIT_CONTEXT */

void
Perl_ck_warner_d(pTHX_ U32 err, const char* pat, ...)
{
    PERL_ARGS_ASSERT_CK_WARNER_D;

    if (Perl_ckwarn_d(aTHX_ err)) {
	va_list args;
	va_start(args, pat);
	vwarner(err, pat, &args);
	va_end(args);
    }
}

void
Perl_ck_warner(pTHX_ U32 err, const char* pat, ...)
{
    PERL_ARGS_ASSERT_CK_WARNER;

    if (Perl_ckwarn(aTHX_ err)) {
	va_list args;
	va_start(args, pat);
	vwarner(err, pat, &args);
	va_end(args);
    }
}

void
Perl_warner(pTHX_ U32  err, const char* pat,...)
{
    va_list args;
    PERL_ARGS_ASSERT_WARNER;
    va_start(args, pat);
    vwarner(err, pat, &args);
    va_end(args);
}

void
Perl_vwarner(pTHX_ U32  err, const char* pat, va_list* args)
{
    dVAR;
    PERL_ARGS_ASSERT_VWARNER;
    if (PL_warnhook == PERL_WARNHOOK_FATAL || ckDEAD(err)) {
	SV * const msv = vmess(pat, args);

	invoke_exception_hook(msv, FALSE);
	die_unwind(msv);
    }
    else {
	Perl_vwarn(aTHX_ pat, args);
    }
}

/* implements the ckWARN? macros */

bool
Perl_ckwarn(pTHX_ U32 w)
{
    dVAR;
    /* If lexical warnings have not been set, use $^W.  */
    if (isLEXWARN_off)
	return PL_dowarn & G_WARN_ON;

    return ckwarn_common(w);
}

/* implements the ckWARN?_d macro */

bool
Perl_ckwarn_d(pTHX_ U32 w)
{
    dVAR;
    /* If lexical warnings have not been set then default classes warn.  */
    if (isLEXWARN_off)
	return TRUE;

    return ckwarn_common(w);
}

static bool
S_ckwarn_common(pTHX_ U32 w)
{
    if (PL_curcop->cop_warnings == pWARN_ALL)
	return TRUE;

    if (PL_curcop->cop_warnings == pWARN_NONE)
	return FALSE;

    /* Check the assumption that at least the first slot is non-zero.  */
    assert(unpackWARN1(w));

    /* Check the assumption that it is valid to stop as soon as a zero slot is
       seen.  */
    if (!unpackWARN2(w)) {
	assert(!unpackWARN3(w));
	assert(!unpackWARN4(w));
    } else if (!unpackWARN3(w)) {
	assert(!unpackWARN4(w));
    }
	
    /* Right, dealt with all the special cases, which are implemented as non-
       pointers, so there is a pointer to a real warnings mask.  */
    do {
	if (isWARN_on(PL_curcop->cop_warnings, unpackWARN1(w)))
	    return TRUE;
    } while (w >>= WARNshift);

    return FALSE;
}

/* Set buffer=NULL to get a new one.  */
STRLEN *
Perl_new_warnings_bitfield(pTHX_ STRLEN *buffer, const char *const bits,
			   STRLEN size) {
    const MEM_SIZE len_wanted =
	sizeof(STRLEN) + (size > WARNsize ? size : WARNsize);
    PERL_UNUSED_CONTEXT;
    PERL_ARGS_ASSERT_NEW_WARNINGS_BITFIELD;

    buffer = (STRLEN*)
	(specialWARN(buffer) ?
	 PerlMemShared_malloc(len_wanted) :
	 PerlMemShared_realloc(buffer, len_wanted));
    buffer[0] = size;
    Copy(bits, (buffer + 1), size, char);
    if (size < WARNsize)
	Zero((char *)(buffer + 1) + size, WARNsize - size, char);
    return buffer;
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
Perl_my_setenv(pTHX_ const char *nam, const char *val)
{
  dVAR;
#ifdef USE_ITHREADS
  /* only parent thread can modify process environment */
  if (PL_curinterp == aTHX)
#endif
  {
#ifndef PERL_USE_SAFE_PUTENV
    if (!PL_use_safe_putenv) {
    /* most putenv()s leak, so we manipulate environ directly */
    I32 i;
    const I32 len = strlen(nam);
    int nlen, vlen;

    /* where does it go? */
    for (i = 0; environ[i]; i++) {
        if (strnEQ(environ[i],nam,len) && environ[i][len] == '=')
            break;
    }

    if (environ == PL_origenviron) {   /* need we copy environment? */
       I32 j;
       I32 max;
       char **tmpenv;

       max = i;
       while (environ[max])
           max++;
       tmpenv = (char**)safesysmalloc((max+2) * sizeof(char*));
       for (j=0; j<max; j++) {         /* copy environment */
           const int len = strlen(environ[j]);
           tmpenv[j] = (char*)safesysmalloc((len+1)*sizeof(char));
           Copy(environ[j], tmpenv[j], len+1, char);
       }
       tmpenv[max] = NULL;
       environ = tmpenv;               /* tell exec where it is now */
    }
    if (!val) {
       safesysfree(environ[i]);
       while (environ[i]) {
           environ[i] = environ[i+1];
           i++;
	}
       return;
    }
    if (!environ[i]) {                 /* does not exist yet */
       environ = (char**)safesysrealloc(environ, (i+2) * sizeof(char*));
       environ[i+1] = NULL;    /* make sure it's null terminated */
    }
    else
       safesysfree(environ[i]);
       nlen = strlen(nam);
       vlen = strlen(val);

       environ[i] = (char*)safesysmalloc((nlen+vlen+2) * sizeof(char));
       /* all that work just for this */
       my_setenv_format(environ[i], nam, nlen, val, vlen);
    } else {
# endif
    /* This next branch should only be called #if defined(HAS_SETENV), but
       Configure doesn't test for that yet.  For Solaris, setenv() and unsetenv()
       were introduced in Solaris 9, so testing for HAS UNSETENV is sufficient.
    */
#   if defined(__CYGWIN__)|| defined(__SYMBIAN32__) || defined(__riscos__) || (defined(__sun) && defined(HAS_UNSETENV))
#       if defined(HAS_UNSETENV)
        if (val == NULL) {
            (void)unsetenv(nam);
        } else {
            (void)setenv(nam, val, 1);
        }
#       else /* ! HAS_UNSETENV */
        (void)setenv(nam, val, 1);
#       endif /* HAS_UNSETENV */
#   else
#       if defined(HAS_UNSETENV)
        if (val == NULL) {
            if (environ) /* old glibc can crash with null environ */
                (void)unsetenv(nam);
        } else {
	    const int nlen = strlen(nam);
	    const int vlen = strlen(val);
	    char * const new_env =
                (char*)safesysmalloc((nlen + vlen + 2) * sizeof(char));
            my_setenv_format(new_env, nam, nlen, val, vlen);
            (void)putenv(new_env);
        }
#       else /* ! HAS_UNSETENV */
        char *new_env;
	const int nlen = strlen(nam);
	int vlen;
        if (!val) {
	   val = "";
        }
        vlen = strlen(val);
        new_env = (char*)safesysmalloc((nlen + vlen + 2) * sizeof(char));
        /* all that work just for this */
        my_setenv_format(new_env, nam, nlen, val, vlen);
        (void)putenv(new_env);
#       endif /* HAS_UNSETENV */
#   endif /* __CYGWIN__ */
#ifndef PERL_USE_SAFE_PUTENV
    }
#endif
  }
}

#else /* WIN32 || NETWARE */

void
Perl_my_setenv(pTHX_ const char *nam, const char *val)
{
    dVAR;
    char *envstr;
    const int nlen = strlen(nam);
    int vlen;

    if (!val) {
       val = "";
    }
    vlen = strlen(val);
    Newx(envstr, nlen+vlen+2, char);
    my_setenv_format(envstr, nam, nlen, val, vlen);
    (void)PerlEnv_putenv(envstr);
    Safefree(envstr);
}

#endif /* WIN32 || NETWARE */

#endif /* !VMS */

#ifdef UNLINK_ALL_VERSIONS
I32
Perl_unlnk(pTHX_ const char *f)	/* unlink all versions of a file */
{
    I32 retries = 0;

    PERL_ARGS_ASSERT_UNLNK;

    while (PerlLIO_unlink(f) >= 0)
	retries++;
    return retries ? 0 : -1;
}
#endif

/* this is a drop-in replacement for bcopy() */
#if (!defined(HAS_MEMCPY) && !defined(HAS_BCOPY)) || (!defined(HAS_MEMMOVE) && !defined(HAS_SAFE_MEMCPY) && !defined(HAS_SAFE_BCOPY))
char *
Perl_my_bcopy(const char *from, char *to, I32 len)
{
    char * const retval = to;

    PERL_ARGS_ASSERT_MY_BCOPY;

    assert(len >= 0);

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
Perl_my_memset(char *loc, I32 ch, I32 len)
{
    char * const retval = loc;

    PERL_ARGS_ASSERT_MY_MEMSET;

    assert(len >= 0);

    while (len--)
	*loc++ = ch;
    return retval;
}
#endif

/* this is a drop-in replacement for bzero() */
#if !defined(HAS_BZERO) && !defined(HAS_MEMSET)
char *
Perl_my_bzero(char *loc, I32 len)
{
    char * const retval = loc;

    PERL_ARGS_ASSERT_MY_BZERO;

    assert(len >= 0);

    while (len--)
	*loc++ = 0;
    return retval;
}
#endif

/* this is a drop-in replacement for memcmp() */
#if !defined(HAS_MEMCMP) || !defined(HAS_SANE_MEMCMP)
I32
Perl_my_memcmp(const char *s1, const char *s2, I32 len)
{
    const U8 *a = (const U8 *)s1;
    const U8 *b = (const U8 *)s2;
    I32 tmp;

    PERL_ARGS_ASSERT_MY_MEMCMP;

    assert(len >= 0);

    while (len--) {
        if ((tmp = *a++ - *b++))
	    return tmp;
    }
    return 0;
}
#endif /* !HAS_MEMCMP || !HAS_SANE_MEMCMP */

#ifndef HAS_VPRINTF
/* This vsprintf replacement should generally never get used, since
   vsprintf was available in both System V and BSD 2.11.  (There may
   be some cross-compilation or embedded set-ups where it is needed,
   however.)

   If you encounter a problem in this function, it's probably a symptom
   that Configure failed to detect your system's vprintf() function.
   See the section on "item vsprintf" in the INSTALL file.

   This version may compile on systems with BSD-ish <stdio.h>,
   but probably won't on others.
*/

#ifdef USE_CHAR_VSPRINTF
char *
#else
int
#endif
vsprintf(char *dest, const char *pat, void *args)
{
    FILE fakebuf;

#if defined(STDIO_PTR_LVALUE) && defined(STDIO_CNT_LVALUE)
    FILE_ptr(&fakebuf) = (STDCHAR *) dest;
    FILE_cnt(&fakebuf) = 32767;
#else
    /* These probably won't compile -- If you really need
       this, you'll have to figure out some other method. */
    fakebuf._ptr = dest;
    fakebuf._cnt = 32767;
#endif
#ifndef _IOSTRG
#define _IOSTRG 0
#endif
    fakebuf._flag = _IOWRT|_IOSTRG;
    _doprnt(pat, args, &fakebuf);	/* what a kludge */
#if defined(STDIO_PTR_LVALUE)
    *(FILE_ptr(&fakebuf)++) = '\0';
#else
    /* PerlIO has probably #defined away fputc, but we want it here. */
#  ifdef fputc
#    undef fputc  /* XXX Should really restore it later */
#  endif
    (void)fputc('\0', &fakebuf);
#endif
#ifdef USE_CHAR_VSPRINTF
    return(dest);
#else
    return 0;		/* perl doesn't use return value */
#endif
}

#endif /* HAS_VPRINTF */

PerlIO *
Perl_my_popen_list(pTHX_ const char *mode, int n, SV **args)
{
#if (!defined(DOSISH) || defined(HAS_FORK) || defined(AMIGAOS)) && !defined(OS2) && !defined(VMS) && !defined(NETWARE) && !defined(__LIBCATAMOUNT__)
    dVAR;
    int p[2];
    I32 This, that;
    Pid_t pid;
    SV *sv;
    I32 did_pipes = 0;
    int pp[2];

    PERL_ARGS_ASSERT_MY_POPEN_LIST;

    PERL_FLUSHALL_FOR_CHILD;
    This = (*mode == 'w');
    that = !This;
    if (TAINTING_get) {
	taint_env();
	taint_proper("Insecure %s%s", "EXEC");
    }
    if (PerlProc_pipe(p) < 0)
	return NULL;
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
	    return NULL;
	}
	Perl_ck_warner(aTHX_ packWARN(WARN_PIPE), "Can't fork, trying again in 5 seconds");
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
	do_aexec5(NULL, args-1, args-1+n, pp[1], did_pipes);
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

    sv = *av_fetch(PL_fdpid,p[This],TRUE);
    SvUPGRADE(sv,SVt_IV);
    SvIV_set(sv, pid);
    PL_forkprocess = pid;
    /* If we managed to get status pipe check for exec fail */
    if (did_pipes && pid > 0) {
	int errkid;
	unsigned n = 0;
	SSize_t n1;

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
		Perl_croak(aTHX_ "panic: kid popen errno read, n=%u", n);
	    do {
		pid2 = wait4pid(pid, &status, 0);
	    } while (pid2 == -1 && errno == EINTR);
	    errno = errkid;		/* Propagate errno from kid */
	    return NULL;
	}
    }
    if (did_pipes)
	 PerlLIO_close(pp[0]);
    return PerlIO_fdopen(p[This], mode);
#else
#  ifdef OS2	/* Same, without fork()ing and all extra overhead... */
    return my_syspopen4(aTHX_ NULL, mode, n, args);
#  else
    Perl_croak(aTHX_ "List form of piped open not implemented");
    return (PerlIO *) NULL;
#  endif
#endif
}

    /* VMS' my_popen() is in VMS.c, same with OS/2. */
#if (!defined(DOSISH) || defined(HAS_FORK) || defined(AMIGAOS)) && !defined(VMS) && !defined(__LIBCATAMOUNT__)
PerlIO *
Perl_my_popen(pTHX_ const char *cmd, const char *mode)
{
    dVAR;
    int p[2];
    I32 This, that;
    Pid_t pid;
    SV *sv;
    const I32 doexec = !(*cmd == '-' && cmd[1] == '\0');
    I32 did_pipes = 0;
    int pp[2];

    PERL_ARGS_ASSERT_MY_POPEN;

    PERL_FLUSHALL_FOR_CHILD;
#ifdef OS2
    if (doexec) {
	return my_syspopen(aTHX_ cmd,mode);
    }
#endif
    This = (*mode == 'w');
    that = !This;
    if (doexec && TAINTING_get) {
	taint_env();
	taint_proper("Insecure %s%s", "EXEC");
    }
    if (PerlProc_pipe(p) < 0)
	return NULL;
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
		Perl_croak(aTHX_ "Can't fork: %s", Strerror(errno));
	    return NULL;
	}
	Perl_ck_warner(aTHX_ packWARN(WARN_PIPE), "Can't fork, trying again in 5 seconds");
	sleep(5);
    }
    if (pid == 0) {

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

#ifdef PERLIO_USING_CRLF
   /* Since we circumvent IO layers when we manipulate low-level
      filedescriptors directly, need to manually switch to the
      default, binary, low-level mode; see PerlIOBuf_open(). */
   PerlLIO_setmode((*mode == 'r'), O_BINARY);
#endif 
	PL_forkprocess = 0;
#ifdef PERL_USES_PL_PIDSTATUS
	hv_clear(PL_pidstatus);	/* we have no children */
#endif
	return NULL;
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

    sv = *av_fetch(PL_fdpid,p[This],TRUE);
    SvUPGRADE(sv,SVt_IV);
    SvIV_set(sv, pid);
    PL_forkprocess = pid;
    if (did_pipes && pid > 0) {
	int errkid;
	unsigned n = 0;
	SSize_t n1;

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
		Perl_croak(aTHX_ "panic: kid popen errno read, n=%u", n);
	    do {
		pid2 = wait4pid(pid, &status, 0);
	    } while (pid2 == -1 && errno == EINTR);
	    errno = errkid;		/* Propagate errno from kid */
	    return NULL;
	}
    }
    if (did_pipes)
	 PerlLIO_close(pp[0]);
    return PerlIO_fdopen(p[This], mode);
}
#else
#if defined(DJGPP)
FILE *djgpp_popen();
PerlIO *
Perl_my_popen(pTHX_ const char *cmd, const char *mode)
{
    PERL_FLUSHALL_FOR_CHILD;
    /* Call system's popen() to get a FILE *, then import it.
       used 0 for 2nd parameter to PerlIO_importFILE;
       apparently not used
    */
    return PerlIO_importFILE(djgpp_popen(cmd, mode), 0);
}
#else
#if defined(__LIBCATAMOUNT__)
PerlIO *
Perl_my_popen(pTHX_ const char *cmd, const char *mode)
{
    return NULL;
}
#endif
#endif

#endif /* !DOSISH */

/* this is called in parent before the fork() */
void
Perl_atfork_lock(void)
{
   dVAR;
#if defined(USE_ITHREADS)
    /* locks must be held in locking order (if any) */
#  ifdef USE_PERLIO
    MUTEX_LOCK(&PL_perlio_mutex);
#  endif
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
    dVAR;
#if defined(USE_ITHREADS)
    /* locks must be released in same order as in atfork_lock() */
#  ifdef USE_PERLIO
    MUTEX_UNLOCK(&PL_perlio_mutex);
#  endif
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
#if defined(USE_ITHREADS) && !defined(HAS_PTHREAD_ATFORK)
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

Sighandler_t
Perl_rsignal(pTHX_ int signo, Sighandler_t handler)
{
    dVAR;
    struct sigaction act, oact;

#ifdef USE_ITHREADS
    /* only "parent" interpreter can diddle signals */
    if (PL_curinterp != aTHX)
	return (Sighandler_t) SIG_ERR;
#endif

    act.sa_handler = (void(*)(int))handler;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
#ifdef SA_RESTART
    if (PL_signals & PERL_SIGNALS_UNSAFE_FLAG)
        act.sa_flags |= SA_RESTART;	/* SVR4, 4.3+BSD */
#endif
#if defined(SA_NOCLDWAIT) && !defined(BSDish) /* See [perl #18849] */
    if (signo == SIGCHLD && handler == (Sighandler_t) SIG_IGN)
	act.sa_flags |= SA_NOCLDWAIT;
#endif
    if (sigaction(signo, &act, &oact) == -1)
    	return (Sighandler_t) SIG_ERR;
    else
    	return (Sighandler_t) oact.sa_handler;
}

Sighandler_t
Perl_rsignal_state(pTHX_ int signo)
{
    struct sigaction oact;
    PERL_UNUSED_CONTEXT;

    if (sigaction(signo, (struct sigaction *)NULL, &oact) == -1)
	return (Sighandler_t) SIG_ERR;
    else
	return (Sighandler_t) oact.sa_handler;
}

int
Perl_rsignal_save(pTHX_ int signo, Sighandler_t handler, Sigsave_t *save)
{
    dVAR;
    struct sigaction act;

    PERL_ARGS_ASSERT_RSIGNAL_SAVE;

#ifdef USE_ITHREADS
    /* only "parent" interpreter can diddle signals */
    if (PL_curinterp != aTHX)
	return -1;
#endif

    act.sa_handler = (void(*)(int))handler;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
#ifdef SA_RESTART
    if (PL_signals & PERL_SIGNALS_UNSAFE_FLAG)
        act.sa_flags |= SA_RESTART;	/* SVR4, 4.3+BSD */
#endif
#if defined(SA_NOCLDWAIT) && !defined(BSDish) /* See [perl #18849] */
    if (signo == SIGCHLD && handler == (Sighandler_t) SIG_IGN)
	act.sa_flags |= SA_NOCLDWAIT;
#endif
    return sigaction(signo, &act, save);
}

int
Perl_rsignal_restore(pTHX_ int signo, Sigsave_t *save)
{
    dVAR;
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
	return (Sighandler_t) SIG_ERR;
#endif

    return PerlProc_signal(signo, handler);
}

static Signal_t
sig_trap(int signo)
{
    dVAR;
    PL_sig_trapped++;
}

Sighandler_t
Perl_rsignal_state(pTHX_ int signo)
{
    dVAR;
    Sighandler_t oldsig;

#if defined(USE_ITHREADS) && !defined(WIN32)
    /* only "parent" interpreter can diddle signals */
    if (PL_curinterp != aTHX)
	return (Sighandler_t) SIG_ERR;
#endif

    PL_sig_trapped = 0;
    oldsig = PerlProc_signal(signo, sig_trap);
    PerlProc_signal(signo, oldsig);
    if (PL_sig_trapped)
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
    return (*save == (Sighandler_t) SIG_ERR) ? -1 : 0;
}

int
Perl_rsignal_restore(pTHX_ int signo, Sigsave_t *save)
{
#if defined(USE_ITHREADS) && !defined(WIN32)
    /* only "parent" interpreter can diddle signals */
    if (PL_curinterp != aTHX)
	return -1;
#endif
    return (PerlProc_signal(signo, *save) == (Sighandler_t) SIG_ERR) ? -1 : 0;
}

#endif /* !HAS_SIGACTION */
#endif /* !PERL_MICRO */

    /* VMS' my_pclose() is in VMS.c; same with OS/2 */
#if (!defined(DOSISH) || defined(HAS_FORK) || defined(AMIGAOS)) && !defined(VMS) && !defined(__LIBCATAMOUNT__)
I32
Perl_my_pclose(pTHX_ PerlIO *ptr)
{
    dVAR;
    int status;
    SV **svp;
    Pid_t pid;
    Pid_t pid2 = 0;
    bool close_failed;
    dSAVEDERRNO;
    const int fd = PerlIO_fileno(ptr);
    bool should_wait;

    svp = av_fetch(PL_fdpid,fd,TRUE);
    pid = (SvTYPE(*svp) == SVt_IV) ? SvIVX(*svp) : -1;
    SvREFCNT_dec(*svp);
    *svp = NULL;

#if defined(USE_PERLIO)
    /* Find out whether the refcount is low enough for us to wait for the
       child proc without blocking. */
    should_wait = PerlIOUnix_refcnt(fd) == 1 && pid > 0;
#else
    should_wait = pid > 0;
#endif

#ifdef OS2
    if (pid == -1) {			/* Opened by popen. */
	return my_syspclose(ptr);
    }
#endif
    close_failed = (PerlIO_close(ptr) == EOF);
    SAVE_ERRNO;
    if (should_wait) do {
	pid2 = wait4pid(pid, &status, 0);
    } while (pid2 == -1 && errno == EINTR);
    if (close_failed) {
	RESTORE_ERRNO;
	return -1;
    }
    return(
      should_wait
       ? pid2 < 0 ? pid2 : status == 0 ? 0 : (errno = 0, status)
       : 0
    );
}
#else
#if defined(__LIBCATAMOUNT__)
I32
Perl_my_pclose(pTHX_ PerlIO *ptr)
{
    return -1;
}
#endif
#endif /* !DOSISH */

#if  (!defined(DOSISH) || defined(OS2) || defined(WIN32) || defined(NETWARE)) && !defined(__LIBCATAMOUNT__)
I32
Perl_wait4pid(pTHX_ Pid_t pid, int *statusp, int flags)
{
    dVAR;
    I32 result = 0;
    PERL_ARGS_ASSERT_WAIT4PID;
#ifdef PERL_USES_PL_PIDSTATUS
    if (!pid) {
        /* PERL_USES_PL_PIDSTATUS is only defined when neither
           waitpid() nor wait4() is available, or on OS/2, which
           doesn't appear to support waiting for a progress group
           member, so we can only treat a 0 pid as an unknown child.
        */
        errno = ECHILD;
        return -1;
    }
    {
	if (pid > 0) {
	    /* The keys in PL_pidstatus are now the raw 4 (or 8) bytes of the
	       pid, rather than a string form.  */
	    SV * const * const svp = hv_fetch(PL_pidstatus,(const char*) &pid,sizeof(Pid_t),FALSE);
	    if (svp && *svp != &PL_sv_undef) {
		*statusp = SvIVX(*svp);
		(void)hv_delete(PL_pidstatus,(const char*) &pid,sizeof(Pid_t),
				G_DISCARD);
		return pid;
	    }
	}
	else {
	    HE *entry;

	    hv_iterinit(PL_pidstatus);
	    if ((entry = hv_iternext(PL_pidstatus))) {
		SV * const sv = hv_iterval(PL_pidstatus,entry);
		I32 len;
		const char * const spid = hv_iterkey(entry,&len);

		assert (len == sizeof(Pid_t));
		memcpy((char *)&pid, spid, len);
		*statusp = SvIVX(sv);
		/* The hash iterator is currently on this entry, so simply
		   calling hv_delete would trigger the lazy delete, which on
		   aggregate does more work, beacuse next call to hv_iterinit()
		   would spot the flag, and have to call the delete routine,
		   while in the meantime any new entries can't re-use that
		   memory.  */
		hv_iterinit(PL_pidstatus);
		(void)hv_delete(PL_pidstatus,spid,len,G_DISCARD);
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
    result = wait4(pid,statusp,flags,NULL);
    goto finish;
#endif
#ifdef PERL_USES_PL_PIDSTATUS
#if defined(HAS_WAITPID) && defined(HAS_WAITPID_RUNTIME)
  hard_way:
#endif
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
#if defined(HAS_WAITPID) || defined(HAS_WAIT4)
  finish:
#endif
    if (result < 0 && errno == EINTR) {
	PERL_ASYNC_CHECK();
	errno = EINTR; /* reset in case a signal handler changed $! */
    }
    return result;
}
#endif /* !DOSISH || OS2 || WIN32 || NETWARE */

#ifdef PERL_USES_PL_PIDSTATUS
void
S_pidgone(pTHX_ Pid_t pid, int status)
{
    SV *sv;

    sv = *hv_fetch(PL_pidstatus,(const char*)&pid,sizeof(Pid_t),TRUE);
    SvUPGRADE(sv,SVt_IV);
    SvIV_set(sv, status);
    return;
}
#endif

#if defined(OS2)
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
    FILE * const f = PerlIO_findFILE(ptr);
    const I32 result = pclose(f);
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
    FILE * const f = PerlIO_findFILE(ptr);
    I32 result = djgpp_pclose(f);
    result = (result << 8) & 0xff00;
    PerlIO_releaseFILE(ptr,f);
    return result;
}
#endif

#define PERL_REPEATCPY_LINEAR 4
void
Perl_repeatcpy(char *to, const char *from, I32 len, IV count)
{
    PERL_ARGS_ASSERT_REPEATCPY;

    assert(len >= 0);

    if (count < 0)
	croak_memory_wrap();

    if (len == 1)
	memset(to, *from, count);
    else if (count) {
	char *p = to;
	IV items, linear, half;

	linear = count < PERL_REPEATCPY_LINEAR ? count : PERL_REPEATCPY_LINEAR;
	for (items = 0; items < linear; ++items) {
	    const char *q = from;
	    IV todo;
	    for (todo = len; todo > 0; todo--)
		*p++ = *q++;
        }

	half = count / 2;
	while (items <= half) {
	    IV size = items * len;
	    memcpy(p, to, size);
	    p     += size;
	    items *= 2;
	}

	if (count > items)
	    memcpy(p, to, (count - items) * len);
    }
}

#ifndef HAS_RENAME
I32
Perl_same_dirent(pTHX_ const char *a, const char *b)
{
    char *fa = strrchr(a,'/');
    char *fb = strrchr(b,'/');
    Stat_t tmpstatbuf1;
    Stat_t tmpstatbuf2;
    SV * const tmpsv = sv_newmortal();

    PERL_ARGS_ASSERT_SAME_DIRENT;

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
	sv_setpvs(tmpsv, ".");
    else
	sv_setpvn(tmpsv, a, fa - a);
    if (PerlLIO_stat(SvPVX_const(tmpsv), &tmpstatbuf1) < 0)
	return FALSE;
    if (fb == b)
	sv_setpvs(tmpsv, ".");
    else
	sv_setpvn(tmpsv, b, fb - b);
    if (PerlLIO_stat(SvPVX_const(tmpsv), &tmpstatbuf2) < 0)
	return FALSE;
    return tmpstatbuf1.st_dev == tmpstatbuf2.st_dev &&
	   tmpstatbuf1.st_ino == tmpstatbuf2.st_ino;
}
#endif /* !HAS_RENAME */

char*
Perl_find_script(pTHX_ const char *scriptname, bool dosearch,
		 const char *const *const search_ext, I32 flags)
{
    dVAR;
    const char *xfound = NULL;
    char *xfailed = NULL;
    char tmpbuf[MAXPATHLEN];
    char *s;
    I32 len = 0;
    int retval;
    char *bufend;
#if defined(DOSISH) && !defined(OS2)
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
    static const char *const exts[] = { SEARCH_EXTS };
    const char *const *const ext = search_ext ? search_ext : exts;
    int extidx = 0, i = 0;
    const char *curext = NULL;
#else
    PERL_UNUSED_ARG(search_ext);
#  define MAX_EXT_LEN 0
#endif

    PERL_ARGS_ASSERT_FIND_SCRIPT;

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
	int idx = 0, deftypes = 1;
	bool seen_dot = 1;

	const int hasdir = !dosearch || (strpbrk(scriptname,":[</") != NULL);
#  else
    if (dosearch) {
	int idx = 0, deftypes = 1;
	bool seen_dot = 1;

	const int hasdir = (strpbrk(scriptname,":[</") != NULL);
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
	    my_strlcat(tmpbuf, scriptname, sizeof(tmpbuf));
#else  /* !VMS */

#ifdef DOSISH
    if (strEQ(scriptname, "-"))
 	dosearch = 0;
    if (dosearch) {		/* Look in '.' first. */
	const char *cur = scriptname;
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
		my_strlcpy(tmpbuf, scriptname, sizeof(tmpbuf));
		cur = tmpbuf;
	    }
	} while (extidx >= 0 && ext[extidx]	/* try an extension? */
		 && my_strlcpy(tmpbuf+len, ext[extidx++], sizeof(tmpbuf) - len));
#endif
    }
#endif

    if (dosearch && !strchr(scriptname, '/')
#ifdef DOSISH
		 && !strchr(scriptname, '\\')
#endif
		 && (s = PerlEnv_getenv("PATH")))
    {
	bool seen_dot = 0;

	bufend = s + strlen(s);
	while (s < bufend) {
#  ifdef DOSISH
	    for (len = 0; *s
		    && *s != ';'; len++, s++) {
		if (len < sizeof tmpbuf)
		    tmpbuf[len] = *s;
	    }
	    if (len < sizeof tmpbuf)
		tmpbuf[len] = '\0';
#  else
	    s = delimcpy(tmpbuf, tmpbuf + sizeof tmpbuf, s, bufend,
			':',
			&len);
#  endif
	    if (s < bufend)
		s++;
	    if (len + 1 + strlen(scriptname) + MAX_EXT_LEN >= sizeof tmpbuf)
		continue;	/* don't search dir with too-long name */
	    if (len
#  ifdef DOSISH
		&& tmpbuf[len - 1] != '/'
		&& tmpbuf[len - 1] != '\\'
#  endif
	       )
		tmpbuf[len++] = '/';
	    if (len == 2 && tmpbuf[0] == '.')
		seen_dot = 1;
	    (void)my_strlcpy(tmpbuf + len, scriptname, sizeof(tmpbuf) - len);
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
		    && my_strlcpy(tmpbuf+len, ext[extidx++], sizeof(tmpbuf) - len)
		);
#endif
	    if (retval < 0)
		continue;
	    if (S_ISREG(PL_statbuf.st_mode)
		&& cando(S_IRUSR,TRUE,&PL_statbuf)
#if !defined(DOSISH)
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
		/* diag_listed_as: Can't execute %s */
		Perl_croak(aTHX_ "Can't %s %s%s%s",
		      (xfailed ? "execute" : "find"),
		      (xfailed ? xfailed : scriptname),
		      (xfailed ? "" : " on PATH"),
		      (xfailed || seen_dot) ? "" : ", '.' not in PATH");
	    }
	    scriptname = NULL;
	}
	Safefree(xfailed);
	scriptname = xfound;
    }
    return (scriptname ? savepv(scriptname) : NULL);
}

#ifndef PERL_GET_CONTEXT_DEFINED

void *
Perl_get_context(void)
{
    dVAR;
#if defined(USE_ITHREADS)
#  ifdef OLD_PTHREADS_API
    pthread_addr_t t;
    int error = pthread_getspecific(PL_thr_key, &t)
    if (error)
	Perl_croak_nocontext("panic: pthread_getspecific, error=%d", error);
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
    dVAR;
    PERL_ARGS_ASSERT_SET_CONTEXT;
#if defined(USE_ITHREADS)
#  ifdef I_MACH_CTHREADS
    cthread_set_data(cthread_self(), t);
#  else
    {
	const int error = pthread_setspecific(PL_thr_key, t);
	if (error)
	    Perl_croak_nocontext("panic: pthread_setspecific, error=%d", error);
    }
#  endif
#else
    PERL_UNUSED_ARG(t);
#endif
}

#endif /* !PERL_GET_CONTEXT_DEFINED */

#if defined(PERL_GLOBAL_STRUCT) && !defined(PERL_GLOBAL_STRUCT_PRIVATE)
struct perl_vars *
Perl_GetVars(pTHX)
{
 return &PL_Vars;
}
#endif

char **
Perl_get_op_names(pTHX)
{
    PERL_UNUSED_CONTEXT;
    return (char **)PL_op_name;
}

char **
Perl_get_op_descs(pTHX)
{
    PERL_UNUSED_CONTEXT;
    return (char **)PL_op_desc;
}

const char *
Perl_get_no_modify(pTHX)
{
    PERL_UNUSED_CONTEXT;
    return PL_no_modify;
}

U32 *
Perl_get_opargs(pTHX)
{
    PERL_UNUSED_CONTEXT;
    return (U32 *)PL_opargs;
}

PPADDR_t*
Perl_get_ppaddr(pTHX)
{
    dVAR;
    PERL_UNUSED_CONTEXT;
    return (PPADDR_t*)PL_ppaddr;
}

#ifndef HAS_GETENV_LEN
char *
Perl_getenv_len(pTHX_ const char *env_elem, unsigned long *len)
{
    char * const env_trans = PerlEnv_getenv(env_elem);
    PERL_UNUSED_CONTEXT;
    PERL_ARGS_ASSERT_GETENV_LEN;
    if (env_trans)
	*len = strlen(env_trans);
    return env_trans;
}
#endif


MGVTBL*
Perl_get_vtbl(pTHX_ int vtbl_id)
{
    PERL_UNUSED_CONTEXT;

    return (vtbl_id < 0 || vtbl_id >= magic_vtable_max)
	? NULL : (MGVTBL*)PL_magic_vtables + vtbl_id;
}

I32
Perl_my_fflush_all(pTHX)
{
#if defined(USE_PERLIO) || defined(FFLUSH_NULL)
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
Perl_report_wrongway_fh(pTHX_ const GV *gv, const char have)
{
    if (ckWARN(WARN_IO)) {
        HEK * const name
           = gv && (isGV_with_GP(gv))
                ? GvENAME_HEK((gv))
                : NULL;
	const char * const direction = have == '>' ? "out" : "in";

	if (name && HEK_LEN(name))
	    Perl_warner(aTHX_ packWARN(WARN_IO),
			"Filehandle %"HEKf" opened only for %sput",
			name, direction);
	else
	    Perl_warner(aTHX_ packWARN(WARN_IO),
			"Filehandle opened only for %sput", direction);
    }
}

void
Perl_report_evil_fh(pTHX_ const GV *gv)
{
    const IO *io = gv ? GvIO(gv) : NULL;
    const PERL_BITFIELD16 op = PL_op->op_type;
    const char *vile;
    I32 warn_type;

    if (io && IoTYPE(io) == IoTYPE_CLOSED) {
	vile = "closed";
	warn_type = WARN_CLOSED;
    }
    else {
	vile = "unopened";
	warn_type = WARN_UNOPENED;
    }

    if (ckWARN(warn_type)) {
        SV * const name
            = gv && isGV_with_GP(gv) && GvENAMELEN(gv) ?
                                     sv_2mortal(newSVhek(GvENAME_HEK(gv))) : NULL;
	const char * const pars =
	    (const char *)(OP_IS_FILETEST(op) ? "" : "()");
	const char * const func =
	    (const char *)
	    (op == OP_READLINE || op == OP_RCATLINE
				 ? "readline"  :	/* "<HANDLE>" not nice */
	     op == OP_LEAVEWRITE ? "write" :		/* "write exit" not nice */
	     PL_op_desc[op]);
	const char * const type =
	    (const char *)
	    (OP_IS_SOCKET(op) || (io && IoTYPE(io) == IoTYPE_SOCKET)
	     ? "socket" : "filehandle");
	const bool have_name = name && SvCUR(name);
	Perl_warner(aTHX_ packWARN(warn_type),
		   "%s%s on %s %s%s%"SVf, func, pars, vile, type,
		    have_name ? " " : "",
		    SVfARG(have_name ? name : &PL_sv_no));
	if (io && IoDIRP(io) && !(IoFLAGS(io) & IOf_FAKE_DIRP))
		Perl_warner(
			    aTHX_ packWARN(warn_type),
			"\t(Are you trying to call %s%s on dirhandle%s%"SVf"?)\n",
			func, pars, have_name ? " " : "",
			SVfARG(have_name ? name : &PL_sv_no)
			    );
    }
}

/* To workaround core dumps from the uninitialised tm_zone we get the
 * system to give us a reasonable struct to copy.  This fix means that
 * strftime uses the tm_zone and tm_gmtoff values returned by
 * localtime(time()). That should give the desired result most of the
 * time. But probably not always!
 *
 * This does not address tzname aspects of NETaa14816.
 *
 */

#ifdef __GLIBC__
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
    const struct tm* my_tm;
    PERL_ARGS_ASSERT_INIT_TM;
    (void)time(&now);
    my_tm = localtime(&now);
    if (my_tm)
        Copy(my_tm, ptm, 1, struct tm);
#else
    PERL_ARGS_ASSERT_INIT_TM;
    PERL_UNUSED_ARG(ptm);
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
    PERL_UNUSED_CONTEXT;

    PERL_ARGS_ASSERT_MINI_MKTIME;

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
 * were leap years in the Julian calendar but not in the Gregorian one.
 * I can live with that.
 *
 * This algorithm also fails to handle years before A.D. 1 gracefully, but
 * that's still outside the scope for POSIX time manipulation, so I don't
 * care.
 */

    year = 1900 + ptm->tm_year;
    month = ptm->tm_mon;
    mday = ptm->tm_mday;
    jday = 0;
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
    ptm->tm_wday = (jday + WEEKDAY_BIAS) % 7;
}

char *
Perl_my_strftime(pTHX_ const char *fmt, int sec, int min, int hour, int mday, int mon, int year, int wday, int yday, int isdst)
{
#ifdef HAS_STRFTIME
  char *buf;
  int buflen;
  struct tm mytm;
  int len;

  PERL_ARGS_ASSERT_MY_STRFTIME;

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
  Newx(buf, buflen, char);

  GCC_DIAG_IGNORE(-Wformat-nonliteral); /* fmt checked by caller */
  len = strftime(buf, buflen, fmt, &mytm);
  GCC_DIAG_RESTORE;

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
    const int fmtlen = strlen(fmt);
    int bufsize = fmtlen + buflen;

    Renew(buf, bufsize, char);
    while (buf) {

      GCC_DIAG_IGNORE(-Wformat-nonliteral); /* fmt checked by caller */
      buflen = strftime(buf, bufsize, fmt, &mytm);
      GCC_DIAG_RESTORE;

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
  return NULL;
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
Perl_getcwd_sv(pTHX_ SV *sv)
{
#ifndef PERL_MICRO
    dVAR;
    SvTAINTED_on(sv);

    PERL_ARGS_ASSERT_GETCWD_SV;

#ifdef HAS_GETCWD
    {
	char buf[MAXPATHLEN];

	/* Some getcwd()s automatically allocate a buffer of the given
	 * size from the heap if they are given a NULL buffer pointer.
	 * The problem is that this behaviour is not portable. */
	if (getcwd(buf, sizeof(buf) - 1)) {
	    sv_setpv(sv, buf);
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
    int pathlen=0;
    Direntry_t *dp;

    SvUPGRADE(sv, SVt_PV);

    if (PerlLIO_lstat(".", &statbuf) < 0) {
	SV_CWD_RETURN_UNDEF;
    }

    orig_cdev = statbuf.st_dev;
    orig_cino = statbuf.st_ino;
    cdev = orig_cdev;
    cino = orig_cino;

    for (;;) {
	DIR *dir;
	int namelen;
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
	    Move(SvPVX_const(sv), SvPVX(sv) + namelen + 1, pathlen, char);
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

	if (PerlDir_chdir(SvPVX_const(sv)) < 0) {
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

#include "vutil.c"

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
	FD_SET((unsigned int)sockets[0], &rset);
	FD_SET((unsigned int)sockets[1], &rset);

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
	dSAVE_ERRNO;
	if (sockets[0] != -1)
	    PerlLIO_close(sockets[0]);
	if (sockets[1] != -1)
	    PerlLIO_close(sockets[1]);
	RESTORE_ERRNO;
	return -1;
    }
}
#endif /*  EMULATE_SOCKETPAIR_UDP */

#if !defined(HAS_SOCKETPAIR) && defined(HAS_SOCKET) && defined(AF_INET) && defined(PF_INET)
int
Perl_my_socketpair (int family, int type, int protocol, int fd[2]) {
    /* Stevens says that family must be AF_LOCAL, protocol 0.
       I'm going to enforce that, then ignore it, and use TCP (or UDP).  */
    dTHXa(NULL);
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

    aTHXa(PERL_GET_THX);
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
#ifdef ECONNABORTED
  errno = ECONNABORTED;	/* This would be the standard thing to do. */
#else
#  ifdef ECONNREFUSED
  errno = ECONNREFUSED;	/* E.g. Symbian does not have ECONNABORTED. */
#  else
  errno = ETIMEDOUT;	/* Desperation time. */
#  endif
#endif
  tidy_up_and_fail:
    {
	dSAVE_ERRNO;
	if (listener != -1)
	    PerlLIO_close(listener);
	if (connector != -1)
	    PerlLIO_close(connector);
	if (acceptor != -1)
	    PerlLIO_close(acceptor);
	RESTORE_ERRNO;
	return -1;
    }
}
#else
/* In any case have a stub so that there's code corresponding
 * to the my_socketpair in embed.fnc. */
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
Or "locks" it.  Or "unlocks" it.  In other
words, ignores its single SV argument.
Exists to avoid test for a NULL function pointer and because it could
potentially warn under some level of strict-ness.

=cut
*/

void
Perl_sv_nosharing(pTHX_ SV *sv)
{
    PERL_UNUSED_CONTEXT;
    PERL_UNUSED_ARG(sv);
}

/*

=for apidoc sv_destroyable

Dummy routine which reports that object can be destroyed when there is no
sharing module present.  It ignores its single SV argument, and returns
'true'.  Exists to avoid test for a NULL function pointer and because it
could potentially warn under some level of strict-ness.

=cut
*/

bool
Perl_sv_destroyable(pTHX_ SV *sv)
{
    PERL_UNUSED_CONTEXT;
    PERL_UNUSED_ARG(sv);
    return TRUE;
}

U32
Perl_parse_unicode_opts(pTHX_ const char **popt)
{
  const char *p = *popt;
  U32 opt = 0;

  PERL_ARGS_ASSERT_PARSE_UNICODE_OPTS;

  if (*p) {
       if (isDIGIT(*p)) {
	    opt = (U32) atoi(p);
	    while (isDIGIT(*p))
		p++;
	    if (*p && *p != '\n' && *p != '\r') {
	     if(isSPACE(*p)) goto the_end_of_the_opts_parser;
	     else
		 Perl_croak(aTHX_ "Unknown Unicode option letter '%c'", *p);
	    }
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
		 case PERL_UNICODE_UTF8CACHEASSERT:
		      opt |= PERL_UNICODE_UTF8CACHEASSERT_FLAG; break;
		 default:
		      if (*p != '\n' && *p != '\r') {
			if(isSPACE(*p)) goto the_end_of_the_opts_parser;
			else
			  Perl_croak(aTHX_
				     "Unknown Unicode option letter '%c'", *p);
		      }
		 }
	    }
       }
  }
  else
       opt = PERL_UNICODE_DEFAULT_FLAGS;

  the_end_of_the_opts_parser:

  if (opt & ~PERL_UNICODE_ALL_FLAGS)
       Perl_croak(aTHX_ "Unknown Unicode option value %"UVuf,
		  (UV) (opt & ~PERL_UNICODE_ALL_FLAGS));

  *popt = p;

  return opt;
}

#ifdef VMS
#  include <starlet.h>
#endif

U32
Perl_seed(pTHX)
{
#if defined(__OpenBSD__)
	return arc4random();
#else
    dVAR;
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
    	if (PerlLIO_read(fd, (void*)&u, sizeof u) != sizeof u)
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
#endif
}

void
Perl_get_hash_seed(pTHX_ unsigned char * const seed_buffer)
{
    dVAR;
    const char *env_pv;
    unsigned long i;

    PERL_ARGS_ASSERT_GET_HASH_SEED;

    env_pv= PerlEnv_getenv("PERL_HASH_SEED");

    if ( env_pv )
#ifndef USE_HASH_SEED_EXPLICIT
    {
        /* ignore leading spaces */
        while (isSPACE(*env_pv))
            env_pv++;
#ifdef USE_PERL_PERTURB_KEYS
        /* if they set it to "0" we disable key traversal randomization completely */
        if (strEQ(env_pv,"0")) {
            PL_hash_rand_bits_enabled= 0;
        } else {
            /* otherwise switch to deterministic mode */
            PL_hash_rand_bits_enabled= 2;
        }
#endif
        /* ignore a leading 0x... if it is there */
        if (env_pv[0] == '0' && env_pv[1] == 'x')
            env_pv += 2;

        for( i = 0; isXDIGIT(*env_pv) && i < PERL_HASH_SEED_BYTES; i++ ) {
            seed_buffer[i] = READ_XDIGIT(env_pv) << 4;
            if ( isXDIGIT(*env_pv)) {
                seed_buffer[i] |= READ_XDIGIT(env_pv);
            }
        }
        while (isSPACE(*env_pv))
            env_pv++;

        if (*env_pv && !isXDIGIT(*env_pv)) {
            Perl_warn(aTHX_ "perl: warning: Non hex character in '$ENV{PERL_HASH_SEED}', seed only partially set\n");
        }
        /* should we check for unparsed crap? */
        /* should we warn about unused hex? */
        /* should we warn about insufficient hex? */
    }
    else
#endif
    {
        (void)seedDrand01((Rand_seed_t)seed());

        for( i = 0; i < PERL_HASH_SEED_BYTES; i++ ) {
            seed_buffer[i] = (unsigned char)(Drand01() * (U8_MAX+1));
        }
    }
#ifdef USE_PERL_PERTURB_KEYS
    {   /* initialize PL_hash_rand_bits from the hash seed.
         * This value is highly volatile, it is updated every
         * hash insert, and is used as part of hash bucket chain
         * randomization and hash iterator randomization. */
        PL_hash_rand_bits= 0xbe49d17f; /* I just picked a number */
        for( i = 0; i < sizeof(UV) ; i++ ) {
            PL_hash_rand_bits += seed_buffer[i % PERL_HASH_SEED_BYTES];
            PL_hash_rand_bits = ROTL_UV(PL_hash_rand_bits,8);
        }
    }
    env_pv= PerlEnv_getenv("PERL_PERTURB_KEYS");
    if (env_pv) {
        if (strEQ(env_pv,"0") || strEQ(env_pv,"NO")) {
            PL_hash_rand_bits_enabled= 0;
        } else if (strEQ(env_pv,"1") || strEQ(env_pv,"RANDOM")) {
            PL_hash_rand_bits_enabled= 1;
        } else if (strEQ(env_pv,"2") || strEQ(env_pv,"DETERMINISTIC")) {
            PL_hash_rand_bits_enabled= 2;
        } else {
            Perl_warn(aTHX_ "perl: warning: strange setting in '$ENV{PERL_PERTURB_KEYS}': '%s'\n", env_pv);
        }
    }
#endif
}

#ifdef PERL_GLOBAL_STRUCT

#define PERL_GLOBAL_STRUCT_INIT
#include "opcode.h" /* the ppaddr and check */

struct perl_vars *
Perl_init_global_struct(pTHX)
{
    struct perl_vars *plvarsp = NULL;
# ifdef PERL_GLOBAL_STRUCT
    const IV nppaddr = C_ARRAY_LENGTH(Gppaddr);
    const IV ncheck  = C_ARRAY_LENGTH(Gcheck);
#  ifdef PERL_GLOBAL_STRUCT_PRIVATE
    /* PerlMem_malloc() because can't use even safesysmalloc() this early. */
    plvarsp = (struct perl_vars*)PerlMem_malloc(sizeof(struct perl_vars));
    if (!plvarsp)
        exit(1);
#  else
    plvarsp = PL_VarsPtr;
#  endif /* PERL_GLOBAL_STRUCT_PRIVATE */
#  undef PERLVAR
#  undef PERLVARA
#  undef PERLVARI
#  undef PERLVARIC
#  define PERLVAR(prefix,var,type) /**/
#  define PERLVARA(prefix,var,n,type) /**/
#  define PERLVARI(prefix,var,type,init) plvarsp->prefix##var = init;
#  define PERLVARIC(prefix,var,type,init) plvarsp->prefix##var = init;
#  include "perlvars.h"
#  undef PERLVAR
#  undef PERLVARA
#  undef PERLVARI
#  undef PERLVARIC
#  ifdef PERL_GLOBAL_STRUCT
    plvarsp->Gppaddr =
	(Perl_ppaddr_t*)
	PerlMem_malloc(nppaddr * sizeof(Perl_ppaddr_t));
    if (!plvarsp->Gppaddr)
        exit(1);
    plvarsp->Gcheck  =
	(Perl_check_t*)
	PerlMem_malloc(ncheck  * sizeof(Perl_check_t));
    if (!plvarsp->Gcheck)
        exit(1);
    Copy(Gppaddr, plvarsp->Gppaddr, nppaddr, Perl_ppaddr_t); 
    Copy(Gcheck,  plvarsp->Gcheck,  ncheck,  Perl_check_t); 
#  endif
#  ifdef PERL_SET_VARS
    PERL_SET_VARS(plvarsp);
#  endif
#  ifdef PERL_GLOBAL_STRUCT_PRIVATE
    plvarsp->Gsv_placeholder.sv_flags = 0;
    memset(plvarsp->Ghash_seed, 0, sizeof(plvarsp->Ghash_seed));
#  endif
# undef PERL_GLOBAL_STRUCT_INIT
# endif
    return plvarsp;
}

#endif /* PERL_GLOBAL_STRUCT */

#ifdef PERL_GLOBAL_STRUCT

void
Perl_free_global_struct(pTHX_ struct perl_vars *plvarsp)
{
    int veto = plvarsp->Gveto_cleanup;

    PERL_ARGS_ASSERT_FREE_GLOBAL_STRUCT;
# ifdef PERL_GLOBAL_STRUCT
#  ifdef PERL_UNSET_VARS
    PERL_UNSET_VARS(plvarsp);
#  endif
    if (veto)
        return;
    free(plvarsp->Gppaddr);
    free(plvarsp->Gcheck);
#  ifdef PERL_GLOBAL_STRUCT_PRIVATE
    free(plvarsp);
#  endif
# endif
}

#endif /* PERL_GLOBAL_STRUCT */

#ifdef PERL_MEM_LOG

/* -DPERL_MEM_LOG: the Perl_mem_log_..() is compiled, including the
 * the default implementation, unless -DPERL_MEM_LOG_NOIMPL is also
 * given, and you supply your own implementation.
 *
 * The default implementation reads a single env var, PERL_MEM_LOG,
 * expecting one or more of the following:
 *
 *    \d+ - fd		fd to write to		: must be 1st (atoi)
 *    'm' - memlog	was PERL_MEM_LOG=1
 *    's' - svlog	was PERL_SV_LOG=1
 *    't' - timestamp	was PERL_MEM_LOG_TIMESTAMP=1
 *
 * This makes the logger controllable enough that it can reasonably be
 * added to the system perl.
 */

/* -DPERL_MEM_LOG_SPRINTF_BUF_SIZE=X: size of a (stack-allocated) buffer
 * the Perl_mem_log_...() will use (either via sprintf or snprintf).
 */
#define PERL_MEM_LOG_SPRINTF_BUF_SIZE 128

/* -DPERL_MEM_LOG_FD=N: the file descriptor the Perl_mem_log_...()
 * writes to.  In the default logger, this is settable at runtime.
 */
#ifndef PERL_MEM_LOG_FD
#  define PERL_MEM_LOG_FD 2 /* If STDERR is too boring for you. */
#endif

#ifndef PERL_MEM_LOG_NOIMPL

# ifdef DEBUG_LEAKING_SCALARS
#   define SV_LOG_SERIAL_FMT	    " [%lu]"
#   define _SV_LOG_SERIAL_ARG(sv)   , (unsigned long) (sv)->sv_debug_serial
# else
#   define SV_LOG_SERIAL_FMT
#   define _SV_LOG_SERIAL_ARG(sv)
# endif

static void
S_mem_log_common(enum mem_log_type mlt, const UV n, 
		 const UV typesize, const char *type_name, const SV *sv,
		 Malloc_t oldalloc, Malloc_t newalloc,
		 const char *filename, const int linenumber,
		 const char *funcname)
{
    const char *pmlenv;

    PERL_ARGS_ASSERT_MEM_LOG_COMMON;

    pmlenv = PerlEnv_getenv("PERL_MEM_LOG");
    if (!pmlenv)
	return;
    if (mlt < MLT_NEW_SV ? strchr(pmlenv,'m') : strchr(pmlenv,'s'))
    {
	/* We can't use SVs or PerlIO for obvious reasons,
	 * so we'll use stdio and low-level IO instead. */
	char buf[PERL_MEM_LOG_SPRINTF_BUF_SIZE];

#   ifdef HAS_GETTIMEOFDAY
#     define MEM_LOG_TIME_FMT	"%10d.%06d: "
#     define MEM_LOG_TIME_ARG	(int)tv.tv_sec, (int)tv.tv_usec
	struct timeval tv;
	gettimeofday(&tv, 0);
#   else
#     define MEM_LOG_TIME_FMT	"%10d: "
#     define MEM_LOG_TIME_ARG	(int)when
        Time_t when;
        (void)time(&when);
#   endif
	/* If there are other OS specific ways of hires time than
	 * gettimeofday() (see ext/Time-HiRes), the easiest way is
	 * probably that they would be used to fill in the struct
	 * timeval. */
	{
	    STRLEN len;
	    int fd = atoi(pmlenv);
	    if (!fd)
		fd = PERL_MEM_LOG_FD;

	    if (strchr(pmlenv, 't')) {
		len = my_snprintf(buf, sizeof(buf),
				MEM_LOG_TIME_FMT, MEM_LOG_TIME_ARG);
		PerlLIO_write(fd, buf, len);
	    }
	    switch (mlt) {
	    case MLT_ALLOC:
		len = my_snprintf(buf, sizeof(buf),
			"alloc: %s:%d:%s: %"IVdf" %"UVuf
			" %s = %"IVdf": %"UVxf"\n",
			filename, linenumber, funcname, n, typesize,
			type_name, n * typesize, PTR2UV(newalloc));
		break;
	    case MLT_REALLOC:
		len = my_snprintf(buf, sizeof(buf),
			"realloc: %s:%d:%s: %"IVdf" %"UVuf
			" %s = %"IVdf": %"UVxf" -> %"UVxf"\n",
			filename, linenumber, funcname, n, typesize,
			type_name, n * typesize, PTR2UV(oldalloc),
			PTR2UV(newalloc));
		break;
	    case MLT_FREE:
		len = my_snprintf(buf, sizeof(buf),
			"free: %s:%d:%s: %"UVxf"\n",
			filename, linenumber, funcname,
			PTR2UV(oldalloc));
		break;
	    case MLT_NEW_SV:
	    case MLT_DEL_SV:
		len = my_snprintf(buf, sizeof(buf),
			"%s_SV: %s:%d:%s: %"UVxf SV_LOG_SERIAL_FMT "\n",
			mlt == MLT_NEW_SV ? "new" : "del",
			filename, linenumber, funcname,
			PTR2UV(sv) _SV_LOG_SERIAL_ARG(sv));
		break;
	    default:
		len = 0;
	    }
	    PerlLIO_write(fd, buf, len);
	}
    }
}
#endif /* !PERL_MEM_LOG_NOIMPL */

#ifndef PERL_MEM_LOG_NOIMPL
# define \
    mem_log_common_if(alty, num, tysz, tynm, sv, oal, nal, flnm, ln, fnnm) \
    mem_log_common   (alty, num, tysz, tynm, sv, oal, nal, flnm, ln, fnnm)
#else
/* this is suboptimal, but bug compatible.  User is providing their
   own implementation, but is getting these functions anyway, and they
   do nothing. But _NOIMPL users should be able to cope or fix */
# define \
    mem_log_common_if(alty, num, tysz, tynm, u, oal, nal, flnm, ln, fnnm) \
    /* mem_log_common_if_PERL_MEM_LOG_NOIMPL */
#endif

Malloc_t
Perl_mem_log_alloc(const UV n, const UV typesize, const char *type_name,
		   Malloc_t newalloc, 
		   const char *filename, const int linenumber,
		   const char *funcname)
{
    mem_log_common_if(MLT_ALLOC, n, typesize, type_name,
		      NULL, NULL, newalloc,
		      filename, linenumber, funcname);
    return newalloc;
}

Malloc_t
Perl_mem_log_realloc(const UV n, const UV typesize, const char *type_name,
		     Malloc_t oldalloc, Malloc_t newalloc, 
		     const char *filename, const int linenumber, 
		     const char *funcname)
{
    mem_log_common_if(MLT_REALLOC, n, typesize, type_name,
		      NULL, oldalloc, newalloc, 
		      filename, linenumber, funcname);
    return newalloc;
}

Malloc_t
Perl_mem_log_free(Malloc_t oldalloc, 
		  const char *filename, const int linenumber, 
		  const char *funcname)
{
    mem_log_common_if(MLT_FREE, 0, 0, "", NULL, oldalloc, NULL, 
		      filename, linenumber, funcname);
    return oldalloc;
}

void
Perl_mem_log_new_sv(const SV *sv, 
		    const char *filename, const int linenumber,
		    const char *funcname)
{
    mem_log_common_if(MLT_NEW_SV, 0, 0, "", sv, NULL, NULL,
		      filename, linenumber, funcname);
}

void
Perl_mem_log_del_sv(const SV *sv,
		    const char *filename, const int linenumber, 
		    const char *funcname)
{
    mem_log_common_if(MLT_DEL_SV, 0, 0, "", sv, NULL, NULL, 
		      filename, linenumber, funcname);
}

#endif /* PERL_MEM_LOG */

/*
=for apidoc my_sprintf

The C library C<sprintf>, wrapped if necessary, to ensure that it will return
the length of the string written to the buffer.  Only rare pre-ANSI systems
need the wrapper function - usually this is a direct call to C<sprintf>.

=cut
*/
#ifndef SPRINTF_RETURNS_STRLEN
int
Perl_my_sprintf(char *buffer, const char* pat, ...)
{
    va_list args;
    PERL_ARGS_ASSERT_MY_SPRINTF;
    va_start(args, pat);
    vsprintf(buffer, pat, args);
    va_end(args);
    return strlen(buffer);
}
#endif

/*
=for apidoc my_snprintf

The C library C<snprintf> functionality, if available and
standards-compliant (uses C<vsnprintf>, actually).  However, if the
C<vsnprintf> is not available, will unfortunately use the unsafe
C<vsprintf> which can overrun the buffer (there is an overrun check,
but that may be too late).  Consider using C<sv_vcatpvf> instead, or
getting C<vsnprintf>.

=cut
*/
int
Perl_my_snprintf(char *buffer, const Size_t len, const char *format, ...)
{
    int retval;
    va_list ap;
    PERL_ARGS_ASSERT_MY_SNPRINTF;
    va_start(ap, format);
#ifdef HAS_VSNPRINTF
    retval = vsnprintf(buffer, len, format, ap);
#else
    retval = vsprintf(buffer, format, ap);
#endif
    va_end(ap);
    /* vsprintf() shows failure with < 0 */
    if (retval < 0
#ifdef HAS_VSNPRINTF
    /* vsnprintf() shows failure with >= len */
        ||
        (len > 0 && (Size_t)retval >= len) 
#endif
    )
	Perl_croak_nocontext("panic: my_snprintf buffer overflow");
    return retval;
}

/*
=for apidoc my_vsnprintf

The C library C<vsnprintf> if available and standards-compliant.
However, if if the C<vsnprintf> is not available, will unfortunately
use the unsafe C<vsprintf> which can overrun the buffer (there is an
overrun check, but that may be too late).  Consider using
C<sv_vcatpvf> instead, or getting C<vsnprintf>.

=cut
*/
int
Perl_my_vsnprintf(char *buffer, const Size_t len, const char *format, va_list ap)
{
    int retval;
#ifdef NEED_VA_COPY
    va_list apc;

    PERL_ARGS_ASSERT_MY_VSNPRINTF;

    Perl_va_copy(ap, apc);
# ifdef HAS_VSNPRINTF
    retval = vsnprintf(buffer, len, format, apc);
# else
    retval = vsprintf(buffer, format, apc);
# endif
    va_end(apc);
#else
# ifdef HAS_VSNPRINTF
    retval = vsnprintf(buffer, len, format, ap);
# else
    retval = vsprintf(buffer, format, ap);
# endif
#endif /* #ifdef NEED_VA_COPY */
    /* vsprintf() shows failure with < 0 */
    if (retval < 0
#ifdef HAS_VSNPRINTF
    /* vsnprintf() shows failure with >= len */
        ||
        (len > 0 && (Size_t)retval >= len) 
#endif
    )
	Perl_croak_nocontext("panic: my_vsnprintf buffer overflow");
    return retval;
}

void
Perl_my_clearenv(pTHX)
{
    dVAR;
#if ! defined(PERL_MICRO)
#  if defined(PERL_IMPLICIT_SYS) || defined(WIN32)
    PerlEnv_clearenv();
#  else /* ! (PERL_IMPLICIT_SYS || WIN32) */
#    if defined(USE_ENVIRON_ARRAY)
#      if defined(USE_ITHREADS)
    /* only the parent thread can clobber the process environment */
    if (PL_curinterp == aTHX)
#      endif /* USE_ITHREADS */
    {
#      if ! defined(PERL_USE_SAFE_PUTENV)
    if ( !PL_use_safe_putenv) {
      I32 i;
      if (environ == PL_origenviron)
        environ = (char**)safesysmalloc(sizeof(char*));
      else
        for (i = 0; environ[i]; i++)
          (void)safesysfree(environ[i]);
    }
    environ[0] = NULL;
#      else /* PERL_USE_SAFE_PUTENV */
#        if defined(HAS_CLEARENV)
    (void)clearenv();
#        elif defined(HAS_UNSETENV)
    int bsiz = 80; /* Most envvar names will be shorter than this. */
    char *buf = (char*)safesysmalloc(bsiz);
    while (*environ != NULL) {
      char *e = strchr(*environ, '=');
      int l = e ? e - *environ : (int)strlen(*environ);
      if (bsiz < l + 1) {
        (void)safesysfree(buf);
        bsiz = l + 1; /* + 1 for the \0. */
        buf = (char*)safesysmalloc(bsiz);
      } 
      memcpy(buf, *environ, l);
      buf[l] = '\0';
      (void)unsetenv(buf);
    }
    (void)safesysfree(buf);
#        else /* ! HAS_CLEARENV && ! HAS_UNSETENV */
    /* Just null environ and accept the leakage. */
    *environ = NULL;
#        endif /* HAS_CLEARENV || HAS_UNSETENV */
#      endif /* ! PERL_USE_SAFE_PUTENV */
    }
#    endif /* USE_ENVIRON_ARRAY */
#  endif /* PERL_IMPLICIT_SYS || WIN32 */
#endif /* PERL_MICRO */
}

#ifdef PERL_IMPLICIT_CONTEXT

/* Implements the MY_CXT_INIT macro. The first time a module is loaded,
the global PL_my_cxt_index is incremented, and that value is assigned to
that module's static my_cxt_index (who's address is passed as an arg).
Then, for each interpreter this function is called for, it makes sure a
void* slot is available to hang the static data off, by allocating or
extending the interpreter's PL_my_cxt_list array */

#ifndef PERL_GLOBAL_STRUCT_PRIVATE
void *
Perl_my_cxt_init(pTHX_ int *index, size_t size)
{
    dVAR;
    void *p;
    PERL_ARGS_ASSERT_MY_CXT_INIT;
    if (*index == -1) {
	/* this module hasn't been allocated an index yet */
#if defined(USE_ITHREADS)
	MUTEX_LOCK(&PL_my_ctx_mutex);
#endif
	*index = PL_my_cxt_index++;
#if defined(USE_ITHREADS)
	MUTEX_UNLOCK(&PL_my_ctx_mutex);
#endif
    }
    
    /* make sure the array is big enough */
    if (PL_my_cxt_size <= *index) {
	if (PL_my_cxt_size) {
	    while (PL_my_cxt_size <= *index)
		PL_my_cxt_size *= 2;
	    Renew(PL_my_cxt_list, PL_my_cxt_size, void *);
	}
	else {
	    PL_my_cxt_size = 16;
	    Newx(PL_my_cxt_list, PL_my_cxt_size, void *);
	}
    }
    /* newSV() allocates one more than needed */
    p = (void*)SvPVX(newSV(size-1));
    PL_my_cxt_list[*index] = p;
    Zero(p, size, char);
    return p;
}

#else /* #ifndef PERL_GLOBAL_STRUCT_PRIVATE */

int
Perl_my_cxt_index(pTHX_ const char *my_cxt_key)
{
    dVAR;
    int index;

    PERL_ARGS_ASSERT_MY_CXT_INDEX;

    for (index = 0; index < PL_my_cxt_index; index++) {
	const char *key = PL_my_cxt_keys[index];
	/* try direct pointer compare first - there are chances to success,
	 * and it's much faster.
	 */
	if ((key == my_cxt_key) || strEQ(key, my_cxt_key))
	    return index;
    }
    return -1;
}

void *
Perl_my_cxt_init(pTHX_ const char *my_cxt_key, size_t size)
{
    dVAR;
    void *p;
    int index;

    PERL_ARGS_ASSERT_MY_CXT_INIT;

    index = Perl_my_cxt_index(aTHX_ my_cxt_key);
    if (index == -1) {
	/* this module hasn't been allocated an index yet */
#if defined(USE_ITHREADS)
	MUTEX_LOCK(&PL_my_ctx_mutex);
#endif
	index = PL_my_cxt_index++;
#if defined(USE_ITHREADS)
	MUTEX_UNLOCK(&PL_my_ctx_mutex);
#endif
    }

    /* make sure the array is big enough */
    if (PL_my_cxt_size <= index) {
	int old_size = PL_my_cxt_size;
	int i;
	if (PL_my_cxt_size) {
	    while (PL_my_cxt_size <= index)
		PL_my_cxt_size *= 2;
	    Renew(PL_my_cxt_list, PL_my_cxt_size, void *);
	    Renew(PL_my_cxt_keys, PL_my_cxt_size, const char *);
	}
	else {
	    PL_my_cxt_size = 16;
	    Newx(PL_my_cxt_list, PL_my_cxt_size, void *);
	    Newx(PL_my_cxt_keys, PL_my_cxt_size, const char *);
	}
	for (i = old_size; i < PL_my_cxt_size; i++) {
	    PL_my_cxt_keys[i] = 0;
	    PL_my_cxt_list[i] = 0;
	}
    }
    PL_my_cxt_keys[index] = my_cxt_key;
    /* newSV() allocates one more than needed */
    p = (void*)SvPVX(newSV(size-1));
    PL_my_cxt_list[index] = p;
    Zero(p, size, char);
    return p;
}
#endif /* #ifndef PERL_GLOBAL_STRUCT_PRIVATE */
#endif /* PERL_IMPLICIT_CONTEXT */

void
Perl_xs_version_bootcheck(pTHX_ U32 items, U32 ax, const char *xs_p,
			  STRLEN xs_len)
{
    SV *sv;
    const char *vn = NULL;
    SV *const module = PL_stack_base[ax];

    PERL_ARGS_ASSERT_XS_VERSION_BOOTCHECK;

    if (items >= 2)	 /* version supplied as bootstrap arg */
	sv = PL_stack_base[ax + 1];
    else {
	/* XXX GV_ADDWARN */
	vn = "XS_VERSION";
	sv = get_sv(Perl_form(aTHX_ "%"SVf"::%s", module, vn), 0);
	if (!sv || !SvOK(sv)) {
	    vn = "VERSION";
	    sv = get_sv(Perl_form(aTHX_ "%"SVf"::%s", module, vn), 0);
	}
    }
    if (sv) {
	SV *xssv = Perl_newSVpvn_flags(aTHX_ xs_p, xs_len, SVs_TEMP);
	SV *pmsv = sv_isobject(sv) && sv_derived_from(sv, "version")
	    ? sv : sv_2mortal(new_version(sv));
	xssv = upg_version(xssv, 0);
	if ( vcmp(pmsv,xssv) ) {
	    SV *string = vstringify(xssv);
	    SV *xpt = Perl_newSVpvf(aTHX_ "%"SVf" object version %"SVf
				    " does not match ", module, string);

	    SvREFCNT_dec(string);
	    string = vstringify(pmsv);

	    if (vn) {
		Perl_sv_catpvf(aTHX_ xpt, "$%"SVf"::%s %"SVf, module, vn,
			       string);
	    } else {
		Perl_sv_catpvf(aTHX_ xpt, "bootstrap parameter %"SVf, string);
	    }
	    SvREFCNT_dec(string);

	    Perl_sv_2mortal(aTHX_ xpt);
	    Perl_croak_sv(aTHX_ xpt);
	}
    }
}

void
Perl_xs_apiversion_bootcheck(pTHX_ SV *module, const char *api_p,
			     STRLEN api_len)
{
    SV *xpt = NULL;
    SV *compver = Perl_newSVpvn_flags(aTHX_ api_p, api_len, SVs_TEMP);
    SV *runver;

    PERL_ARGS_ASSERT_XS_APIVERSION_BOOTCHECK;

    /* This might croak  */
    compver = upg_version(compver, 0);
    /* This should never croak */
    runver = new_version(PL_apiversion);
    if (vcmp(compver, runver)) {
	SV *compver_string = vstringify(compver);
	SV *runver_string = vstringify(runver);
	xpt = Perl_newSVpvf(aTHX_ "Perl API version %"SVf
			    " of %"SVf" does not match %"SVf,
			    compver_string, module, runver_string);
	Perl_sv_2mortal(aTHX_ xpt);

	SvREFCNT_dec(compver_string);
	SvREFCNT_dec(runver_string);
    }
    SvREFCNT_dec(runver);
    if (xpt)
	Perl_croak_sv(aTHX_ xpt);
}

/*
=for apidoc my_strlcat

The C library C<strlcat> if available, or a Perl implementation of it.
This operates on C C<NUL>-terminated strings.

C<my_strlcat()> appends string C<src> to the end of C<dst>.  It will append at
most S<C<size - strlen(dst) - 1>> characters.  It will then C<NUL>-terminate,
unless C<size> is 0 or the original C<dst> string was longer than C<size> (in
practice this should not happen as it means that either C<size> is incorrect or
that C<dst> is not a proper C<NUL>-terminated string).

Note that C<size> is the full size of the destination buffer and
the result is guaranteed to be C<NUL>-terminated if there is room.  Note that
room for the C<NUL> should be included in C<size>.

=cut

Description stolen from http://www.openbsd.org/cgi-bin/man.cgi?query=strlcat
*/
#ifndef HAS_STRLCAT
Size_t
Perl_my_strlcat(char *dst, const char *src, Size_t size)
{
    Size_t used, length, copy;

    used = strlen(dst);
    length = strlen(src);
    if (size > 0 && used < size - 1) {
        copy = (length >= size - used) ? size - used - 1 : length;
        memcpy(dst + used, src, copy);
        dst[used + copy] = '\0';
    }
    return used + length;
}
#endif


/*
=for apidoc my_strlcpy

The C library C<strlcpy> if available, or a Perl implementation of it.
This operates on C C<NUL>-terminated strings.

C<my_strlcpy()> copies up to S<C<size - 1>> characters from the string C<src>
to C<dst>, C<NUL>-terminating the result if C<size> is not 0.

=cut

Description stolen from http://www.openbsd.org/cgi-bin/man.cgi?query=strlcpy
*/
#ifndef HAS_STRLCPY
Size_t
Perl_my_strlcpy(char *dst, const char *src, Size_t size)
{
    Size_t length, copy;

    length = strlen(src);
    if (size > 0) {
        copy = (length >= size) ? size - 1 : length;
        memcpy(dst, src, copy);
        dst[copy] = '\0';
    }
    return length;
}
#endif

#if defined(_MSC_VER) && (_MSC_VER >= 1300) && (_MSC_VER < 1400) && (WINVER < 0x0500)
/* VC7 or 7.1, building with pre-VC7 runtime libraries. */
long _ftol( double ); /* Defined by VC6 C libs. */
long _ftol2( double dblSource ) { return _ftol( dblSource ); }
#endif

PERL_STATIC_INLINE bool
S_gv_has_usable_name(pTHX_ GV *gv)
{
    GV **gvp;
    return GvSTASH(gv)
	&& HvENAME(GvSTASH(gv))
	&& (gvp = (GV **)hv_fetchhek(
			GvSTASH(gv), GvNAME_HEK(gv), 0
	   ))
	&& *gvp == gv;
}

void
Perl_get_db_sub(pTHX_ SV **svp, CV *cv)
{
    dVAR;
    SV * const dbsv = GvSVn(PL_DBsub);
    const bool save_taint = TAINT_get;

    /* When we are called from pp_goto (svp is null),
     * we do not care about using dbsv to call CV;
     * it's for informational purposes only.
     */

    PERL_ARGS_ASSERT_GET_DB_SUB;

    TAINT_set(FALSE);
    save_item(dbsv);
    if (!PERLDB_SUB_NN) {
	GV *gv = CvGV(cv);

	if (!svp) {
	    gv_efullname3(dbsv, gv, NULL);
	}
	else if ( (CvFLAGS(cv) & (CVf_ANON | CVf_CLONED))
	     || strEQ(GvNAME(gv), "END")
	     || ( /* Could be imported, and old sub redefined. */
		 (GvCV(gv) != cv || !S_gv_has_usable_name(aTHX_ gv))
		 &&
		 !( (SvTYPE(*svp) == SVt_PVGV)
		    && (GvCV((const GV *)*svp) == cv)
		    /* Use GV from the stack as a fallback. */
		    && S_gv_has_usable_name(aTHX_ gv = (GV *)*svp) 
		  )
		)
	) {
	    /* GV is potentially non-unique, or contain different CV. */
	    SV * const tmp = newRV(MUTABLE_SV(cv));
	    sv_setsv(dbsv, tmp);
	    SvREFCNT_dec(tmp);
	}
	else {
	    sv_sethek(dbsv, HvENAME_HEK(GvSTASH(gv)));
	    sv_catpvs(dbsv, "::");
	    sv_catpvn_flags(
	      dbsv, GvNAME(gv), GvNAMELEN(gv),
	      GvNAMEUTF8(gv) ? SV_CATUTF8 : SV_CATBYTES
	    );
	}
    }
    else {
	const int type = SvTYPE(dbsv);
	if (type < SVt_PVIV && type != SVt_IV)
	    sv_upgrade(dbsv, SVt_PVIV);
	(void)SvIOK_on(dbsv);
	SvIV_set(dbsv, PTR2IV(cv));	/* Do it the quickest way  */
    }
    SvSETMAGIC(dbsv);
    TAINT_IF(save_taint);
#ifdef NO_TAINT_SUPPORT
    PERL_UNUSED_VAR(save_taint);
#endif
}

int
Perl_my_dirfd(pTHX_ DIR * dir) {

    /* Most dirfd implementations have problems when passed NULL. */
    if(!dir)
        return -1;
#ifdef HAS_DIRFD
    return dirfd(dir);
#elif defined(HAS_DIR_DD_FD)
    return dir->dd_fd;
#else
    Perl_die(aTHX_ PL_no_func, "dirfd");
    assert(0); /* NOT REACHED */
    return 0;
#endif 
}

REGEXP *
Perl_get_re_arg(pTHX_ SV *sv) {

    if (sv) {
        if (SvMAGICAL(sv))
            mg_get(sv);
        if (SvROK(sv))
	    sv = MUTABLE_SV(SvRV(sv));
        if (SvTYPE(sv) == SVt_REGEXP)
            return (REGEXP*) sv;
    }
 
    return NULL;
}

/*
 * This code is derived from drand48() implementation from FreeBSD,
 * found in lib/libc/gen/_rand48.c.
 *
 * The U64 implementation is original, based on the POSIX
 * specification for drand48().
 */

/*
* Copyright (c) 1993 Martin Birgmeier
* All rights reserved.
*
* You may redistribute unmodified or modified versions of this source
* code provided that the above copyright notice and this and the
* following conditions are retained.
*
* This software is provided ``as is'', and comes with no warranties
* of any kind. I shall in no event be liable for anything that happens
* to anyone/anything when using this software.
*/

#define FREEBSD_DRAND48_SEED_0   (0x330e)

#ifdef PERL_DRAND48_QUAD

#define DRAND48_MULT U64_CONST(0x5deece66d)
#define DRAND48_ADD  0xb
#define DRAND48_MASK U64_CONST(0xffffffffffff)

#else

#define FREEBSD_DRAND48_SEED_1   (0xabcd)
#define FREEBSD_DRAND48_SEED_2   (0x1234)
#define FREEBSD_DRAND48_MULT_0   (0xe66d)
#define FREEBSD_DRAND48_MULT_1   (0xdeec)
#define FREEBSD_DRAND48_MULT_2   (0x0005)
#define FREEBSD_DRAND48_ADD      (0x000b)

const unsigned short _rand48_mult[3] = {
                FREEBSD_DRAND48_MULT_0,
                FREEBSD_DRAND48_MULT_1,
                FREEBSD_DRAND48_MULT_2
};
const unsigned short _rand48_add = FREEBSD_DRAND48_ADD;

#endif

void
Perl_drand48_init_r(perl_drand48_t *random_state, U32 seed)
{
    PERL_ARGS_ASSERT_DRAND48_INIT_R;

#ifdef PERL_DRAND48_QUAD
    *random_state = FREEBSD_DRAND48_SEED_0 + ((U64TYPE)seed << 16);
#else
    random_state->seed[0] = FREEBSD_DRAND48_SEED_0;
    random_state->seed[1] = (U16) seed;
    random_state->seed[2] = (U16) (seed >> 16);
#endif
}

double
Perl_drand48_r(perl_drand48_t *random_state)
{
    PERL_ARGS_ASSERT_DRAND48_R;

#ifdef PERL_DRAND48_QUAD
    *random_state = (*random_state * DRAND48_MULT + DRAND48_ADD)
        & DRAND48_MASK;

    return ldexp((double)*random_state, -48);
#else
    {
    U32 accu;
    U16 temp[2];

    accu = (U32) _rand48_mult[0] * (U32) random_state->seed[0]
         + (U32) _rand48_add;
    temp[0] = (U16) accu;        /* lower 16 bits */
    accu >>= sizeof(U16) * 8;
    accu += (U32) _rand48_mult[0] * (U32) random_state->seed[1]
          + (U32) _rand48_mult[1] * (U32) random_state->seed[0];
    temp[1] = (U16) accu;        /* middle 16 bits */
    accu >>= sizeof(U16) * 8;
    accu += _rand48_mult[0] * random_state->seed[2]
          + _rand48_mult[1] * random_state->seed[1]
          + _rand48_mult[2] * random_state->seed[0];
    random_state->seed[0] = temp[0];
    random_state->seed[1] = temp[1];
    random_state->seed[2] = (U16) accu;

    return ldexp((double) random_state->seed[0], -48) +
           ldexp((double) random_state->seed[1], -32) +
           ldexp((double) random_state->seed[2], -16);
    }
#endif
}
 

/*
 * Local variables:
 * c-indentation-style: bsd
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 * ex: set ts=8 sts=4 sw=4 et:
 */
