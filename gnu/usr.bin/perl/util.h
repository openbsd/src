/*    util.h
 *
 *    Copyright (C) 1991, 1992, 1993, 1999, 2001, 2002, 2003, 2004, 2005,
 *    2007, by Larry Wall and others
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

#ifdef VMS
#  define PERL_FILE_IS_ABSOLUTE(f) \
	(*(f) == '/'							\
	 || (strchr(f,':')						\
	     || ((*(f) == '[' || *(f) == '<')				\
		 && (isWORDCHAR((f)[1]) || strchr("$-_]>",(f)[1])))))

#else		/* !VMS */
#  if defined(WIN32) || defined(__CYGWIN__)
#    define PERL_FILE_IS_ABSOLUTE(f) \
	(*(f) == '/' || *(f) == '\\'		/* UNC/rooted path */	\
	 || ((f)[0] && (f)[1] == ':'))		/* drive name */
#  else		/* !WIN32 */
#  ifdef NETWARE
#    define PERL_FILE_IS_ABSOLUTE(f) \
	(((f)[0] && (f)[1] == ':')		/* drive name */	\
	 || ((f)[0] == '\\' && (f)[1] == '\\')	/* UNC path */	\
	 ||	((f)[3] == ':'))				/* volume name, currently only sys */
#  else		/* !NETWARE */
#    if defined(DOSISH) || defined(__SYMBIAN32__)
#      define PERL_FILE_IS_ABSOLUTE(f) \
	(*(f) == '/'							\
	 || ((f)[0] && (f)[1] == ':'))		/* drive name */
#    else	/* NEITHER DOSISH NOR SYMBIANISH */
#      define PERL_FILE_IS_ABSOLUTE(f)	(*(f) == '/')
#    endif	/* DOSISH */
#   endif	/* NETWARE */
#  endif	/* WIN32 */
#endif		/* VMS */

/*
=for apidoc ibcmp

This is a synonym for (! foldEQ())

=for apidoc ibcmp_locale

This is a synonym for (! foldEQ_locale())

=cut
*/
#define ibcmp(s1, s2, len)         cBOOL(! foldEQ(s1, s2, len))
#define ibcmp_locale(s1, s2, len)  cBOOL(! foldEQ_locale(s1, s2, len))

/* outside the core, perl.h undefs HAS_QUAD if IV isn't 64-bit
   We can't swap this to HAS_QUAD, because the logic here affects the type of
   perl_drand48_t below, and that is visible outside of the core.  */
#if defined(U64TYPE) && !defined(USING_MSVC6)
/* use a faster implementation when quads are available,
 * but not with VC6 on Windows */
#    define PERL_DRAND48_QUAD
#endif

#ifdef PERL_DRAND48_QUAD

/* U64 is only defined under PERL_CORE, but this needs to be visible
 * elsewhere so the definition of PerlInterpreter is complete.
 */
typedef U64TYPE perl_drand48_t;

#else

struct PERL_DRAND48_T {
    U16 seed[3];
};

typedef struct PERL_DRAND48_T perl_drand48_t;

#endif

#define PL_RANDOM_STATE_TYPE perl_drand48_t

#define Perl_drand48_init(seed) (Perl_drand48_init_r(&PL_random_state, (seed)))
#define Perl_drand48() (Perl_drand48_r(&PL_random_state))

/*
 * Local variables:
 * c-indentation-style: bsd
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 * ex: set ts=8 sts=4 sw=4 et:
 */
