/*    perl.h
 *
 *    Copyright (c) 1987-1997, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */
#ifndef H_PERL
#define H_PERL 1
#define OVERLOAD

#ifdef PERL_FOR_X2P
/*
 * This file is being used for x2p stuff. 
 * Above symbol is defined via -D in 'x2p/Makefile.SH'
 * Decouple x2p stuff from some of perls more extreme eccentricities. 
 */
#undef EMBED
#undef NO_EMBED
#define NO_EMBED
#undef MULTIPLICITY
#undef USE_STDIO
#define USE_STDIO
#endif /* PERL_FOR_X2P */

#define VOIDUSED 1
#include "config.h"

#include "embed.h"

/*
 * STMT_START { statements; } STMT_END;
 * can be used as a single statement, as in
 * if (x) STMT_START { ... } STMT_END; else ...
 *
 * Trying to select a version that gives no warnings...
 */
#if !(defined(STMT_START) && defined(STMT_END))
# if defined(__GNUC__) && !defined(__STRICT_ANSI__) && !defined(__cplusplus)
#   define STMT_START	(void)(	/* gcc supports ``({ STATEMENTS; })'' */
#   define STMT_END	)
# else
   /* Now which other defined()s do we need here ??? */
#  if (VOIDFLAGS) && (defined(sun) || defined(__sun__))
#   define STMT_START	if (1)
#   define STMT_END	else (void)0
#  else
#   define STMT_START	do
#   define STMT_END	while (0)
#  endif
# endif
#endif

/*
 * SOFT_CAST can be used for args to prototyped functions to retain some
 * type checking; it only casts if the compiler does not know prototypes.
 */
#if defined(CAN_PROTOTYPE) && defined(DEBUGGING_COMPILE)
#define SOFT_CAST(type)	
#else
#define SOFT_CAST(type)	(type)
#endif

#ifndef BYTEORDER
#   define BYTEORDER 0x1234
#endif

/* Overall memory policy? */
#ifndef CONSERVATIVE
#   define LIBERAL 1
#endif

/*
 * The following contortions are brought to you on behalf of all the
 * standards, semi-standards, de facto standards, not-so-de-facto standards
 * of the world, as well as all the other botches anyone ever thought of.
 * The basic theory is that if we work hard enough here, the rest of the
 * code can be a lot prettier.  Well, so much for theory.  Sorry, Henry...
 */

/* define this once if either system, instead of cluttering up the src */
#if defined(MSDOS) || defined(atarist) || defined(WIN32)
#define DOSISH 1
#endif

#if defined(__STDC__) || defined(vax11c) || defined(_AIX) || defined(__stdc__) || defined(__cplusplus)
# define STANDARD_C 1
#endif

#if defined(__cplusplus) || defined(WIN32)
# define DONT_DECLARE_STD 1
#endif

#if defined(HASVOLATILE) || defined(STANDARD_C)
#   ifdef __cplusplus
#	define VOL		// to temporarily suppress warnings
#   else
#	define VOL volatile
#   endif
#else
#   define VOL
#endif

#define TAINT		(tainted = TRUE)
#define TAINT_NOT	(tainted = FALSE)
#define TAINT_IF(c)	if (c) { tainted = TRUE; }
#define TAINT_ENV()	if (tainting) { taint_env(); }
#define TAINT_PROPER(s)	if (tainting) { taint_proper(no_security, s); }

/* XXX All process group stuff is handled in pp_sys.c.  Should these 
   defines move there?  If so, I could simplify this a lot. --AD  9/96.
*/
/* Process group stuff changed from traditional BSD to POSIX.
   perlfunc.pod documents the traditional BSD-style syntax, so we'll
   try to preserve that, if possible.
*/
#ifdef HAS_SETPGID
#  define BSD_SETPGRP(pid, pgrp)	setpgid((pid), (pgrp))
#else
#  if defined(HAS_SETPGRP) && defined(USE_BSD_SETPGRP)
#    define BSD_SETPGRP(pid, pgrp)	setpgrp((pid), (pgrp))
#  else
#    ifdef HAS_SETPGRP2  /* DG/UX */
#      define BSD_SETPGRP(pid, pgrp)	setpgrp2((pid), (pgrp))
#    endif
#  endif
#endif
#if defined(BSD_SETPGRP) && !defined(HAS_SETPGRP)
#  define HAS_SETPGRP  /* Well, effectively it does . . . */
#endif

/* getpgid isn't POSIX, but at least Solaris and Linux have it, and it makes
    our life easier :-) so we'll try it.
*/
#ifdef HAS_GETPGID
#  define BSD_GETPGRP(pid)		getpgid((pid))
#else
#  if defined(HAS_GETPGRP) && defined(USE_BSD_GETPGRP)
#    define BSD_GETPGRP(pid)		getpgrp((pid))
#  else
#    ifdef HAS_GETPGRP2  /* DG/UX */
#      define BSD_GETPGRP(pid)		getpgrp2((pid))
#    endif
#  endif
#endif
#if defined(BSD_GETPGRP) && !defined(HAS_GETPGRP)
#  define HAS_GETPGRP  /* Well, effectively it does . . . */
#endif

/* These are not exact synonyms, since setpgrp() and getpgrp() may 
   have different behaviors, but perl.h used to define USE_BSDPGRP
   (prior to 5.003_05) so some extension might depend on it.
*/
#if defined(USE_BSD_SETPGRP) || defined(USE_BSD_GETPGRP)
#  ifndef USE_BSDPGRP
#    define USE_BSDPGRP
#  endif
#endif

#ifndef _TYPES_		/* If types.h defines this it's easy. */
#   ifndef major		/* Does everyone's types.h define this? */
#	include <sys/types.h>
#   endif
#endif

#ifdef __cplusplus
#  ifndef I_STDARG
#    define I_STDARG 1
#  endif
#endif

#ifdef I_STDARG
#  include <stdarg.h>
#else
#  ifdef I_VARARGS
#    include <varargs.h>
#  endif
#endif

#include "perlio.h"

#ifdef USE_NEXT_CTYPE

#if NX_CURRENT_COMPILER_RELEASE >= 400
#include <objc/NXCType.h>
#else /*  NX_CURRENT_COMPILER_RELEASE < 400 */
#include <appkit/NXCType.h>
#endif /*  NX_CURRENT_COMPILER_RELEASE >= 400 */

#else /* !USE_NEXT_CTYPE */
#include <ctype.h>
#endif /* USE_NEXT_CTYPE */

#ifdef METHOD 	/* Defined by OSF/1 v3.0 by ctype.h */
#undef METHOD
#endif

#ifdef I_LOCALE
#   include <locale.h>
#endif

#if !defined(NO_LOCALE) && defined(HAS_SETLOCALE)
#   define USE_LOCALE
#   if !defined(NO_LOCALE_COLLATE) && defined(LC_COLLATE) \
       && defined(HAS_STRXFRM)
#	define USE_LOCALE_COLLATE
#   endif
#   if !defined(NO_LOCALE_CTYPE) && defined(LC_CTYPE)
#	define USE_LOCALE_CTYPE
#   endif
#   if !defined(NO_LOCALE_NUMERIC) && defined(LC_NUMERIC)
#	define USE_LOCALE_NUMERIC
#   endif
#endif /* !NO_LOCALE && HAS_SETLOCALE */

#include <setjmp.h>

#ifdef I_SYS_PARAM
#   ifdef PARAM_NEEDS_TYPES
#	include <sys/types.h>
#   endif
#   include <sys/param.h>
#endif


/* Use all the "standard" definitions? */
#if defined(STANDARD_C) && defined(I_STDLIB)
#   include <stdlib.h>
#endif

/* This comes after <stdlib.h> so we don't try to change the standard
 * library prototypes; we'll use our own in proto.h instead. */

#ifdef MYMALLOC

#   ifdef HIDEMYMALLOC
#	define malloc  Mymalloc
#	define calloc  Mycalloc
#	define realloc Myremalloc
#	define free    Myfree
#   endif
#   ifdef EMBEDMYMALLOC
#	define malloc  Perl_malloc
#	define calloc  Perl_calloc
#	define realloc Perl_realloc
#	define free    Perl_free
#   endif

#   undef safemalloc
#   undef safecalloc
#   undef saferealloc
#   undef safefree
#   define safemalloc  malloc
#   define safecalloc  calloc
#   define saferealloc realloc
#   define safefree    free

#endif /* MYMALLOC */

#define MEM_SIZE Size_t

#if defined(STANDARD_C) && defined(I_STDDEF)
#   include <stddef.h>
#   define STRUCT_OFFSET(s,m)  offsetof(s,m)
#else
#   define STRUCT_OFFSET(s,m)  (Size_t)(&(((s *)0)->m))
#endif

#if defined(I_STRING) || defined(__cplusplus)
#   include <string.h>
#else
#   include <strings.h>
#endif

#if !defined(HAS_STRCHR) && defined(HAS_INDEX) && !defined(strchr)
#define strchr index
#define strrchr rindex
#endif

#ifdef I_MEMORY
#  include <memory.h>
#endif

#ifdef HAS_MEMCPY
#  if !defined(STANDARD_C) && !defined(I_STRING) && !defined(I_MEMORY)
#    ifndef memcpy
        extern char * memcpy _((char*, char*, int));
#    endif
#  endif
#else
#   ifndef memcpy
#	ifdef HAS_BCOPY
#	    define memcpy(d,s,l) bcopy(s,d,l)
#	else
#	    define memcpy(d,s,l) my_bcopy(s,d,l)
#	endif
#   endif
#endif /* HAS_MEMCPY */

#ifdef HAS_MEMSET
#  if !defined(STANDARD_C) && !defined(I_STRING) && !defined(I_MEMORY)
#    ifndef memset
	extern char *memset _((char*, int, int));
#    endif
#  endif
#else
#  define memset(d,c,l) my_memset(d,c,l)
#endif /* HAS_MEMSET */

#if !defined(HAS_MEMMOVE) && !defined(memmove)
#   if defined(HAS_BCOPY) && defined(HAS_SAFE_BCOPY)
#	define memmove(d,s,l) bcopy(s,d,l)
#   else
#	if defined(HAS_MEMCPY) && defined(HAS_SAFE_MEMCPY)
#	    define memmove(d,s,l) memcpy(d,s,l)
#	else
#	    define memmove(d,s,l) my_bcopy(s,d,l)
#	endif
#   endif
#endif

#if defined(mips) && defined(ultrix) && !defined(__STDC__)
#   undef HAS_MEMCMP
#endif

#if defined(HAS_MEMCMP) && defined(HAS_SANE_MEMCMP)
#  if !defined(STANDARD_C) && !defined(I_STRING) && !defined(I_MEMORY)
#    ifndef memcmp
	extern int memcmp _((char*, char*, int));
#    endif
#  endif
#  ifdef BUGGY_MSC
  #  pragma function(memcmp)
#  endif
#else
#   ifndef memcmp
#	define memcmp 	my_memcmp
#   endif
#endif /* HAS_MEMCMP && HAS_SANE_MEMCMP */

#ifndef memzero
#   ifdef HAS_MEMSET
#	define memzero(d,l) memset(d,0,l)
#   else
#	ifdef HAS_BZERO
#	    define memzero(d,l) bzero(d,l)
#	else
#	    define memzero(d,l) my_bzero(d,l)
#	endif
#   endif
#endif

#ifndef HAS_BCMP
#   ifndef bcmp
#	define bcmp(s1,s2,l) memcmp(s1,s2,l)
#   endif
#endif /* !HAS_BCMP */

#ifdef I_NETINET_IN
#   include <netinet/in.h>
#endif

#if defined(SF_APPEND) && defined(USE_SFIO) && defined(I_SFIO)
/* <sfio.h> defines SF_APPEND and <sys/stat.h> might define SF_APPEND
 * (the neo-BSD seem to do this).  */
#   undef SF_APPEND
#endif

#ifdef I_SYS_STAT
#   include <sys/stat.h>
#endif

/* The stat macros for Amdahl UTS, Unisoft System V/88 (and derivatives
   like UTekV) are broken, sometimes giving false positives.  Undefine
   them here and let the code below set them to proper values.

   The ghs macro stands for GreenHills Software C-1.8.5 which
   is the C compiler for sysV88 and the various derivatives.
   This header file bug is corrected in gcc-2.5.8 and later versions.
   --Kaveh Ghazi (ghazi@noc.rutgers.edu) 10/3/94.  */

#if defined(uts) || (defined(m88k) && defined(ghs))
#   undef S_ISDIR
#   undef S_ISCHR
#   undef S_ISBLK
#   undef S_ISREG
#   undef S_ISFIFO
#   undef S_ISLNK
#endif

#ifdef I_TIME
#   include <time.h>
#endif

#ifdef I_SYS_TIME
#   ifdef I_SYS_TIME_KERNEL
#	define KERNEL
#   endif
#   include <sys/time.h>
#   ifdef I_SYS_TIME_KERNEL
#	undef KERNEL
#   endif
#endif

#if defined(HAS_TIMES) && defined(I_SYS_TIMES)
#    include <sys/times.h>
#endif

#if defined(HAS_STRERROR) && (!defined(HAS_MKDIR) || !defined(HAS_RMDIR))
#   undef HAS_STRERROR
#endif

#ifndef HAS_MKFIFO
#  ifndef mkfifo
#    define mkfifo(path, mode) (mknod((path), (mode) | S_IFIFO, 0))
#  endif
#endif /* !HAS_MKFIFO */

#include <errno.h>
#ifdef HAS_SOCKET
#   ifdef I_NET_ERRNO
#     include <net/errno.h>
#   endif
#endif

#ifdef VMS
#   define SETERRNO(errcode,vmserrcode) \
	STMT_START {			\
	    set_errno(errcode);		\
	    set_vaxc_errno(vmserrcode);	\
	} STMT_END
#else
#   define SETERRNO(errcode,vmserrcode) errno = (errcode)
#endif

#ifndef errno
	extern int errno;     /* ANSI allows errno to be an lvalue expr */
#endif

#ifdef HAS_STRERROR
#       ifdef VMS
	char *strerror _((int,...));
#       else
#ifndef DONT_DECLARE_STD
	char *strerror _((int));
#endif
#       endif
#       ifndef Strerror
#           define Strerror strerror
#       endif
#else
#    ifdef HAS_SYS_ERRLIST
	extern int sys_nerr;
	extern char *sys_errlist[];
#       ifndef Strerror
#           define Strerror(e) \
		((e) < 0 || (e) >= sys_nerr ? "(unknown)" : sys_errlist[e])
#       endif
#   endif
#endif

#ifdef I_SYS_IOCTL
#   ifndef _IOCTL_
#	include <sys/ioctl.h>
#   endif
#endif

#if defined(mc300) || defined(mc500) || defined(mc700) || defined(mc6000)
#   ifdef HAS_SOCKETPAIR
#	undef HAS_SOCKETPAIR
#   endif
#   ifdef I_NDBM
#	undef I_NDBM
#   endif
#endif

#if INTSIZE == 2
#   define htoni htons
#   define ntohi ntohs
#else
#   define htoni htonl
#   define ntohi ntohl
#endif

/* Configure already sets Direntry_t */
#if defined(I_DIRENT)
#   include <dirent.h>
#   if defined(NeXT) && defined(I_SYS_DIR) /* NeXT needs dirent + sys/dir.h */
#	include <sys/dir.h>
#   endif
#else
#   ifdef I_SYS_NDIR
#	include <sys/ndir.h>
#   else
#	ifdef I_SYS_DIR
#	    ifdef hp9000s500
#		include <ndir.h>	/* may be wrong in the future */
#	    else
#		include <sys/dir.h>
#	    endif
#	endif
#   endif
#endif

#ifdef FPUTS_BOTCH
/* work around botch in SunOS 4.0.1 and 4.0.2 */
#   ifndef fputs
#	define fputs(sv,fp) fprintf(fp,"%s",sv)
#   endif
#endif

/*
 * The following gobbledygook brought to you on behalf of __STDC__.
 * (I could just use #ifndef __STDC__, but this is more bulletproof
 * in the face of half-implementations.)
 */

#ifndef S_IFMT
#   ifdef _S_IFMT
#	define S_IFMT _S_IFMT
#   else
#	define S_IFMT 0170000
#   endif
#endif

#ifndef S_ISDIR
#   define S_ISDIR(m) ((m & S_IFMT) == S_IFDIR)
#endif

#ifndef S_ISCHR
#   define S_ISCHR(m) ((m & S_IFMT) == S_IFCHR)
#endif

#ifndef S_ISBLK
#   ifdef S_IFBLK
#	define S_ISBLK(m) ((m & S_IFMT) == S_IFBLK)
#   else
#	define S_ISBLK(m) (0)
#   endif
#endif

#ifndef S_ISREG
#   define S_ISREG(m) ((m & S_IFMT) == S_IFREG)
#endif

#ifndef S_ISFIFO
#   ifdef S_IFIFO
#	define S_ISFIFO(m) ((m & S_IFMT) == S_IFIFO)
#   else
#	define S_ISFIFO(m) (0)
#   endif
#endif

#ifndef S_ISLNK
#   ifdef _S_ISLNK
#	define S_ISLNK(m) _S_ISLNK(m)
#   else
#	ifdef _S_IFLNK
#	    define S_ISLNK(m) ((m & S_IFMT) == _S_IFLNK)
#	else
#	    ifdef S_IFLNK
#		define S_ISLNK(m) ((m & S_IFMT) == S_IFLNK)
#	    else
#		define S_ISLNK(m) (0)
#	    endif
#	endif
#   endif
#endif

#ifndef S_ISSOCK
#   ifdef _S_ISSOCK
#	define S_ISSOCK(m) _S_ISSOCK(m)
#   else
#	ifdef _S_IFSOCK
#	    define S_ISSOCK(m) ((m & S_IFMT) == _S_IFSOCK)
#	else
#	    ifdef S_IFSOCK
#		define S_ISSOCK(m) ((m & S_IFMT) == S_IFSOCK)
#	    else
#		define S_ISSOCK(m) (0)
#	    endif
#	endif
#   endif
#endif

#ifndef S_IRUSR
#   ifdef S_IREAD
#	define S_IRUSR S_IREAD
#	define S_IWUSR S_IWRITE
#	define S_IXUSR S_IEXEC
#   else
#	define S_IRUSR 0400
#	define S_IWUSR 0200
#	define S_IXUSR 0100
#   endif
#   define S_IRGRP (S_IRUSR>>3)
#   define S_IWGRP (S_IWUSR>>3)
#   define S_IXGRP (S_IXUSR>>3)
#   define S_IROTH (S_IRUSR>>6)
#   define S_IWOTH (S_IWUSR>>6)
#   define S_IXOTH (S_IXUSR>>6)
#endif

#ifndef S_ISUID
#   define S_ISUID 04000
#endif

#ifndef S_ISGID
#   define S_ISGID 02000
#endif

#ifdef ff_next
#   undef ff_next
#endif

#if defined(cray) || defined(gould) || defined(i860) || defined(pyr)
#   define SLOPPYDIVIDE
#endif

#ifdef UV
#undef UV
#endif

/*  XXX QUAD stuff is not currently supported on most systems.
    Specifically, perl internals don't support long long.  Among
    the many problems is that some compilers support long long,
    but the underlying library functions (such as sprintf) don't.
    Some things do work (such as quad pack/unpack on convex);
    also some systems use long long for the fpos_t typedef.  That
    seems to work too.

    The IV type is supposed to be long enough to hold any integral
    value or a pointer.
    --Andy Dougherty	August 1996
*/

#ifdef cray
#   define Quad_t int
#else
#   ifdef convex
#	define Quad_t long long
#   else
#	if BYTEORDER > 0xFFFF
#	    define Quad_t long
#	endif
#   endif
#endif

#ifdef Quad_t
#   define HAS_QUAD
    typedef Quad_t IV;
    typedef unsigned Quad_t UV;
#   define IV_MAX PERL_QUAD_MAX
#   define IV_MIN PERL_QUAD_MIN
#   define UV_MAX PERL_UQUAD_MAX
#   define UV_MIN PERL_UQUAD_MIN
#else
    typedef long IV;
    typedef unsigned long UV;
#   define IV_MAX PERL_LONG_MAX
#   define IV_MIN PERL_LONG_MIN
#   define UV_MAX PERL_ULONG_MAX
#   define UV_MIN PERL_ULONG_MIN
#endif

/* Previously these definitions used hardcoded figures. 
 * It is hoped these formula are more portable, although
 * no data one way or another is presently known to me.
 * The "PERL_" names are used because these calculated constants
 * do not meet the ANSI requirements for LONG_MAX, etc., which
 * need to be constants acceptable to #if - kja
 *    define PERL_LONG_MAX        2147483647L
 *    define PERL_LONG_MIN        (-LONG_MAX - 1)
 *    define PERL ULONG_MAX       4294967295L
 */

#ifdef I_LIMITS  /* Needed for cast_xxx() functions below. */
#  include <limits.h>
#else
#ifdef I_VALUES
#  include <values.h>
#endif
#endif

/*
 * Try to figure out max and min values for the integral types.  THE CORRECT
 * SOLUTION TO THIS MESS: ADAPT enquire.c FROM GCC INTO CONFIGURE.  The
 * following hacks are used if neither limits.h or values.h provide them:
 * U<TYPE>_MAX: for types >= int: ~(unsigned TYPE)0
 *              for types <  int:  (unsigned TYPE)~(unsigned)0
 *	The argument to ~ must be unsigned so that later signed->unsigned
 *	conversion can't modify the value's bit pattern (e.g. -0 -> +0),
 *	and it must not be smaller than int because ~ does integral promotion.
 * <type>_MAX: (<type>) (U<type>_MAX >> 1)
 * <type>_MIN: -<type>_MAX - <is_twos_complement_architecture: (3 & -1) == 3>.
 *	The latter is a hack which happens to work on some machines but
 *	does *not* catch any random system, or things like integer types
 *	with NaN if that is possible.
 *
 * All of the types are explicitly cast to prevent accidental loss of
 * numeric range, and in the hope that they will be less likely to confuse
 * over-eager optimizers.
 *
 */

#define PERL_UCHAR_MIN ((unsigned char)0)

#ifdef UCHAR_MAX
#  define PERL_UCHAR_MAX ((unsigned char)UCHAR_MAX)
#else
#  ifdef MAXUCHAR
#    define PERL_UCHAR_MAX ((unsigned char)MAXUCHAR)
#  else
#    define PERL_UCHAR_MAX       ((unsigned char)~(unsigned)0)
#  endif
#endif
 
/*
 * CHAR_MIN and CHAR_MAX are not included here, as the (char) type may be
 * ambiguous. It may be equivalent to (signed char) or (unsigned char)
 * depending on local options. Until Configure detects this (or at least
 * detects whether the "signed" keyword is available) the CHAR ranges
 * will not be included. UCHAR functions normally.
 *                                                           - kja
 */

#define PERL_USHORT_MIN ((unsigned short)0)

#ifdef USHORT_MAX
#  define PERL_USHORT_MAX ((unsigned short)USHORT_MAX)
#else
#  ifdef MAXUSHORT
#    define PERL_USHORT_MAX ((unsigned short)MAXUSHORT)
#  else
#    define PERL_USHORT_MAX       ((unsigned short)~(unsigned)0)
#  endif
#endif

#ifdef SHORT_MAX
#  define PERL_SHORT_MAX ((short)SHORT_MAX)
#else
#  ifdef MAXSHORT    /* Often used in <values.h> */
#    define PERL_SHORT_MAX ((short)MAXSHORT)
#  else
#    define PERL_SHORT_MAX      ((short) (PERL_USHORT_MAX >> 1))
#  endif
#endif

#ifdef SHORT_MIN
#  define PERL_SHORT_MIN ((short)SHORT_MIN)
#else
#  ifdef MINSHORT
#    define PERL_SHORT_MIN ((short)MINSHORT)
#  else
#    define PERL_SHORT_MIN        (-PERL_SHORT_MAX - ((3 & -1) == 3))
#  endif
#endif

#ifdef UINT_MAX
#  define PERL_UINT_MAX ((unsigned int)UINT_MAX)
#else
#  ifdef MAXUINT
#    define PERL_UINT_MAX ((unsigned int)MAXUINT)
#  else
#    define PERL_UINT_MAX       (~(unsigned int)0)
#  endif
#endif

#define PERL_UINT_MIN ((unsigned int)0)

#ifdef INT_MAX
#  define PERL_INT_MAX ((int)INT_MAX)
#else
#  ifdef MAXINT    /* Often used in <values.h> */
#    define PERL_INT_MAX ((int)MAXINT)
#  else
#    define PERL_INT_MAX        ((int)(PERL_UINT_MAX >> 1))
#  endif
#endif

#ifdef INT_MIN
#  define PERL_INT_MIN ((int)INT_MIN)
#else
#  ifdef MININT
#    define PERL_INT_MIN ((int)MININT)
#  else
#    define PERL_INT_MIN        (-PERL_INT_MAX - ((3 & -1) == 3))
#  endif
#endif

#ifdef ULONG_MAX
#  define PERL_ULONG_MAX ((unsigned long)ULONG_MAX)
#else
#  ifdef MAXULONG
#    define PERL_ULONG_MAX ((unsigned long)MAXULONG)
#  else
#    define PERL_ULONG_MAX       (~(unsigned long)0)
#  endif
#endif

#define PERL_ULONG_MIN ((unsigned long)0L)

#ifdef LONG_MAX
#  define PERL_LONG_MAX ((long)LONG_MAX)
#else
#  ifdef MAXLONG    /* Often used in <values.h> */
#    define PERL_LONG_MAX ((long)MAXLONG)
#  else
#    define PERL_LONG_MAX        ((long) (PERL_ULONG_MAX >> 1))
#  endif
#endif

#ifdef LONG_MIN
#  define PERL_LONG_MIN ((long)LONG_MIN)
#else
#  ifdef MINLONG
#    define PERL_LONG_MIN ((long)MINLONG)
#  else
#    define PERL_LONG_MIN        (-PERL_LONG_MAX - ((3 & -1) == 3))
#  endif
#endif

#ifdef HAS_QUAD

#  ifdef UQUAD_MAX
#    define PERL_UQUAD_MAX ((UV)UQUAD_MAX)
#  else
#    define PERL_UQUAD_MAX	(~(UV)0)
#  endif

#  define PERL_UQUAD_MIN ((UV)0)

#  ifdef QUAD_MAX
#    define PERL_QUAD_MAX ((IV)QUAD_MAX)
#  else
#    define PERL_QUAD_MAX 	((IV) (PERL_UQUAD_MAX >> 1))
#  endif

#  ifdef QUAD_MIN
#    define PERL_QUAD_MIN ((IV)QUAD_MIN)
#  else
#    define PERL_QUAD_MIN 	(-PERL_QUAD_MAX - ((3 & -1) == 3))
#  endif

#endif

typedef MEM_SIZE STRLEN;

typedef struct op OP;
typedef struct cop COP;
typedef struct unop UNOP;
typedef struct binop BINOP;
typedef struct listop LISTOP;
typedef struct logop LOGOP;
typedef struct condop CONDOP;
typedef struct pmop PMOP;
typedef struct svop SVOP;
typedef struct gvop GVOP;
typedef struct pvop PVOP;
typedef struct loop LOOP;

typedef struct Outrec Outrec;
typedef struct interpreter PerlInterpreter;
#ifndef __BORLANDC__
typedef struct ff FF;		/* XXX not defined anywhere, should go? */
#endif
typedef struct sv SV;
typedef struct av AV;
typedef struct hv HV;
typedef struct cv CV;
typedef struct regexp REGEXP;
typedef struct gp GP;
typedef struct gv GV;
typedef struct io IO;
typedef struct context CONTEXT;
typedef struct block BLOCK;

typedef struct magic MAGIC;
typedef struct xrv XRV;
typedef struct xpv XPV;
typedef struct xpviv XPVIV;
typedef struct xpvuv XPVUV;
typedef struct xpvnv XPVNV;
typedef struct xpvmg XPVMG;
typedef struct xpvlv XPVLV;
typedef struct xpvav XPVAV;
typedef struct xpvhv XPVHV;
typedef struct xpvgv XPVGV;
typedef struct xpvcv XPVCV;
typedef struct xpvbm XPVBM;
typedef struct xpvfm XPVFM;
typedef struct xpvio XPVIO;
typedef struct mgvtbl MGVTBL;
typedef union any ANY;

#include "handy.h"

typedef I32 (*filter_t) _((int, SV *, int));
#define FILTER_READ(idx, sv, len)  filter_read(idx, sv, len)
#define FILTER_DATA(idx)	   (AvARRAY(rsfp_filters)[idx])
#define FILTER_ISREADER(idx)	   (idx >= AvFILL(rsfp_filters))

#ifdef DOSISH
# if defined(OS2)
#   include "os2ish.h"
# else
#   include "dosish.h"
# endif
#else
# if defined(VMS)
#   include "vmsish.h"
# else
#   if defined(PLAN9)
#     include "./plan9/plan9ish.h"
#   else
#     include "unixish.h"
#   endif
# endif
#endif
  
#ifdef VMS
#   define STATUS_NATIVE	statusvalue_vms
#   define STATUS_NATIVE_EXPORT \
	((I32)statusvalue_vms == -1 ? 44 : statusvalue_vms)
#   define STATUS_NATIVE_SET(n)						\
	STMT_START {							\
	    statusvalue_vms = (n);					\
	    if ((I32)statusvalue_vms == -1)				\
		statusvalue = -1;					\
	    else if (statusvalue_vms & STS$M_SUCCESS)			\
		statusvalue = 0;					\
	    else if ((statusvalue_vms & STS$M_SEVERITY) == 0)		\
		statusvalue = 1 << 8;					\
	    else							\
		statusvalue = (statusvalue_vms & STS$M_SEVERITY) << 8;	\
	} STMT_END
#   define STATUS_POSIX	statusvalue
#   ifdef VMSISH_STATUS
#	define STATUS_CURRENT	(VMSISH_STATUS ? STATUS_NATIVE : STATUS_POSIX)
#   else
#	define STATUS_CURRENT	STATUS_POSIX
#   endif
#   define STATUS_POSIX_SET(n)				\
	STMT_START {					\
	    statusvalue = (n);				\
	    if (statusvalue != -1) {			\
		statusvalue &= 0xFFFF;			\
		statusvalue_vms = statusvalue ? 44 : 1;	\
	    }						\
	    else statusvalue_vms = -1;			\
	} STMT_END
#   define STATUS_ALL_SUCCESS	(statusvalue = 0, statusvalue_vms = 1)
#   define STATUS_ALL_FAILURE	(statusvalue = 1, statusvalue_vms = 44)
#else
#   define STATUS_NATIVE	STATUS_POSIX
#   define STATUS_NATIVE_EXPORT	STATUS_POSIX
#   define STATUS_NATIVE_SET	STATUS_POSIX_SET
#   define STATUS_POSIX		statusvalue
#   define STATUS_POSIX_SET(n)		\
	STMT_START {			\
	    statusvalue = (n);		\
	    if (statusvalue != -1)	\
		statusvalue &= 0xFFFF;	\
	} STMT_END
#   define STATUS_CURRENT STATUS_POSIX
#   define STATUS_ALL_SUCCESS	(statusvalue = 0)
#   define STATUS_ALL_FAILURE	(statusvalue = 1)
#endif

/* Some unistd.h's give a prototype for pause() even though
   HAS_PAUSE ends up undefined.  This causes the #define
   below to be rejected by the compmiler.  Sigh.
*/
#ifdef HAS_PAUSE
#define Pause	pause
#else
#define Pause() sleep((32767<<16)+32767)
#endif

#ifndef IOCPARM_LEN
#   ifdef IOCPARM_MASK
	/* on BSDish systes we're safe */
#	define IOCPARM_LEN(x)  (((x) >> 16) & IOCPARM_MASK)
#   else
	/* otherwise guess at what's safe */
#	define IOCPARM_LEN(x)	256
#   endif
#endif

union any {
    void*	any_ptr;
    I32		any_i32;
    IV		any_iv;
    long	any_long;
    void	(*any_dptr) _((void*));
};

/* Work around some cygwin32 problems with importing global symbols */
#if defined(CYGWIN32) && defined(DLLIMPORT) 
#   include "cw32imp.h"
#endif

#include "regexp.h"
#include "sv.h"
#include "util.h"
#include "form.h"
#include "gv.h"
#include "cv.h"
#include "opcode.h"
#include "op.h"
#include "cop.h"
#include "av.h"
#include "hv.h"
#include "mg.h"
#include "scope.h"

/* work around some libPW problems */
#ifdef DOINIT
EXT char Error[1];
#endif

#if defined(iAPX286) || defined(M_I286) || defined(I80286)
#   define I286
#endif

#if defined(htonl) && !defined(HAS_HTONL)
#define HAS_HTONL
#endif
#if defined(htons) && !defined(HAS_HTONS)
#define HAS_HTONS
#endif
#if defined(ntohl) && !defined(HAS_NTOHL)
#define HAS_NTOHL
#endif
#if defined(ntohs) && !defined(HAS_NTOHS)
#define HAS_NTOHS
#endif
#ifndef HAS_HTONL
#if (BYTEORDER & 0xffff) != 0x4321
#define HAS_HTONS
#define HAS_HTONL
#define HAS_NTOHS
#define HAS_NTOHL
#define MYSWAP
#define htons my_swap
#define htonl my_htonl
#define ntohs my_swap
#define ntohl my_ntohl
#endif
#else
#if (BYTEORDER & 0xffff) == 0x4321
#undef HAS_HTONS
#undef HAS_HTONL
#undef HAS_NTOHS
#undef HAS_NTOHL
#endif
#endif

/*
 * Little-endian byte order functions - 'v' for 'VAX', or 'reVerse'.
 * -DWS
 */
#if BYTEORDER != 0x1234
# define HAS_VTOHL
# define HAS_VTOHS
# define HAS_HTOVL
# define HAS_HTOVS
# if BYTEORDER == 0x4321
#  define vtohl(x)	((((x)&0xFF)<<24)	\
			+(((x)>>24)&0xFF)	\
			+(((x)&0x0000FF00)<<8)	\
			+(((x)&0x00FF0000)>>8)	)
#  define vtohs(x)	((((x)&0xFF)<<8) + (((x)>>8)&0xFF))
#  define htovl(x)	vtohl(x)
#  define htovs(x)	vtohs(x)
# endif
	/* otherwise default to functions in util.c */
#endif

#ifdef CASTNEGFLOAT
#define U_S(what) ((U16)(what))
#define U_I(what) ((unsigned int)(what))
#define U_L(what) ((U32)(what))
#else
#  ifdef __cplusplus
    extern "C" {
#  endif
U32 cast_ulong _((double));
#  ifdef __cplusplus
    }
#  endif
#define U_S(what) ((U16)cast_ulong((double)(what)))
#define U_I(what) ((unsigned int)cast_ulong((double)(what)))
#define U_L(what) (cast_ulong((double)(what)))
#endif

#ifdef CASTI32
#define I_32(what) ((I32)(what))
#define I_V(what) ((IV)(what))
#define U_V(what) ((UV)(what))
#else
#  ifdef __cplusplus
    extern "C" {
#  endif
I32 cast_i32 _((double));
IV cast_iv _((double));
UV cast_uv _((double));
#  ifdef __cplusplus
    }
#  endif
#define I_32(what) (cast_i32((double)(what)))
#define I_V(what) (cast_iv((double)(what)))
#define U_V(what) (cast_uv((double)(what)))
#endif

struct Outrec {
    I32		o_lines;
    char	*o_str;
    U32		o_len;
};

#ifndef MAXSYSFD
#   define MAXSYSFD 2
#endif

#ifndef TMPPATH
#  define TMPPATH "/tmp/perl-eXXXXXX"
#endif

#ifndef __cplusplus
Uid_t getuid _((void));
Uid_t geteuid _((void));
Gid_t getgid _((void));
Gid_t getegid _((void));
#endif

#ifdef DEBUGGING
#ifndef Perl_debug_log
#define Perl_debug_log	PerlIO_stderr()
#endif
#define YYDEBUG 1
#define DEB(a)     			a
#define DEBUG(a)   if (debug)		a
#define DEBUG_p(a) if (debug & 1)	a
#define DEBUG_s(a) if (debug & 2)	a
#define DEBUG_l(a) if (debug & 4)	a
#define DEBUG_t(a) if (debug & 8)	a
#define DEBUG_o(a) if (debug & 16)	a
#define DEBUG_c(a) if (debug & 32)	a
#define DEBUG_P(a) if (debug & 64)	a
#define DEBUG_m(a) if (curinterp && debug & 128)	a
#define DEBUG_f(a) if (debug & 256)	a
#define DEBUG_r(a) if (debug & 512)	a
#define DEBUG_x(a) if (debug & 1024)	a
#define DEBUG_u(a) if (debug & 2048)	a
#define DEBUG_L(a) if (debug & 4096)	a
#define DEBUG_H(a) if (debug & 8192)	a
#define DEBUG_X(a) if (debug & 16384)	a
#define DEBUG_D(a) if (debug & 32768)	a
#else
#define DEB(a)
#define DEBUG(a)
#define DEBUG_p(a)
#define DEBUG_s(a)
#define DEBUG_l(a)
#define DEBUG_t(a)
#define DEBUG_o(a)
#define DEBUG_c(a)
#define DEBUG_P(a)
#define DEBUG_m(a)
#define DEBUG_f(a)
#define DEBUG_r(a)
#define DEBUG_x(a)
#define DEBUG_u(a)
#define DEBUG_L(a)
#define DEBUG_H(a)
#define DEBUG_X(a)
#define DEBUG_D(a)
#endif
#define YYMAXDEPTH 300

#ifndef assert  /* <assert.h> might have been included somehow */
#define assert(what)	DEB( {						\
	if (!(what)) {							\
	    croak("Assertion failed: file \"%s\", line %d",		\
		__FILE__, __LINE__);					\
	    exit(1);							\
	}})
#endif

struct ufuncs {
    I32 (*uf_val)_((IV, SV*));
    I32 (*uf_set)_((IV, SV*));
    IV uf_index;
};

/* Fix these up for __STDC__ */
#ifndef DONT_DECLARE_STD
char *mktemp _((char*));
double atof _((const char*));
#endif

#ifndef STANDARD_C
/* All of these are in stdlib.h or time.h for ANSI C */
Time_t time();
struct tm *gmtime(), *localtime();
char *strchr(), *strrchr();
char *strcpy(), *strcat();
#endif /* ! STANDARD_C */


#ifdef I_MATH
#    include <math.h>
#else
#   ifdef __cplusplus
	extern "C" {
#   endif
	    double exp _((double));
	    double log _((double));
	    double log10 _((double));
	    double sqrt _((double));
	    double frexp _((double,int*));
	    double ldexp _((double,int));
	    double modf _((double,double*));
	    double sin _((double));
	    double cos _((double));
	    double atan2 _((double,double));
	    double pow _((double,double));
#   ifdef __cplusplus
	};
#   endif
#endif

#ifndef __cplusplus
#ifdef __NeXT__ /* or whatever catches all NeXTs */
char *crypt ();       /* Maybe more hosts will need the unprototyped version */
#else
char *crypt _((const char*, const char*));
#endif
#ifndef DONT_DECLARE_STD
#ifndef getenv
char *getenv _((const char*));
#endif
Off_t lseek _((int,Off_t,int));
#endif
char *getlogin _((void));
#endif

#ifdef UNLINK_ALL_VERSIONS /* Currently only makes sense for VMS */
#define UNLINK unlnk
I32 unlnk _((char*));
#else
#define UNLINK unlink
#endif

#ifndef HAS_SETREUID
#  ifdef HAS_SETRESUID
#    define setreuid(r,e) setresuid(r,e,(Uid_t)-1)
#    define HAS_SETREUID
#  endif
#endif
#ifndef HAS_SETREGID
#  ifdef HAS_SETRESGID
#    define setregid(r,e) setresgid(r,e,(Gid_t)-1)
#    define HAS_SETREGID
#  endif
#endif

typedef Signal_t (*Sighandler_t) _((int));

#ifdef HAS_SIGACTION
typedef struct sigaction Sigsave_t;
#else
typedef Sighandler_t Sigsave_t;
#endif

#define SCAN_DEF 0
#define SCAN_TR 1
#define SCAN_REPL 2

#ifdef DEBUGGING
# ifndef register
#  define register
# endif
# define PAD_SV(po) pad_sv(po)
#else
# define PAD_SV(po) curpad[po]
#endif

/****************/
/* Truly global */
/****************/

/* global state */
EXT PerlInterpreter *	curinterp;	/* currently running interpreter */
/* VMS doesn't use environ array and NeXT has problems with crt0.o globals */
#if !defined(VMS) && !(defined(NeXT) && defined(__DYNAMIC__))
#ifndef DONT_DECLARE_STD
extern char **	environ;	/* environment variables supplied via exec */
#endif
#else
#  if defined(NeXT) && defined(__DYNAMIC__)

#  include <mach-o/dyld.h>
EXT char *** environ_pointer;
#  define environ (*environ_pointer)
#  endif
#endif /* environ processing */

EXT int		uid;		/* current real user id */
EXT int		euid;		/* current effective user id */
EXT int		gid;		/* current real group id */
EXT int		egid;		/* current effective group id */
EXT bool	nomemok;	/* let malloc context handle nomem */
EXT U32		an;		/* malloc sequence number */
EXT U32		cop_seqmax;	/* statement sequence number */
EXT U16		op_seqmax;	/* op sequence number */
EXT U32		evalseq;	/* eval sequence number */
EXT U32		sub_generation;	/* inc to force methods to be looked up again */
EXT char **	origenviron;
EXT U32		origalen;
EXT HV *	pidstatus;	/* pid-to-status mappings for waitpid */
EXT U32 *	profiledata;
EXT int		maxo INIT(MAXO);/* Number of ops */
EXT char *	osname;		/* operating system */
EXT char *	sh_path INIT(SH_PATH); /* full path of shell */

EXT XPV*	xiv_arenaroot;	/* list of allocated xiv areas */
EXT IV **	xiv_root;	/* free xiv list--shared by interpreters */
EXT double *	xnv_root;	/* free xnv list--shared by interpreters */
EXT XRV *	xrv_root;	/* free xrv list--shared by interpreters */
EXT XPV *	xpv_root;	/* free xpv list--shared by interpreters */
EXT HE *	he_root;	/* free he list--shared by interpreters */
EXT char *	nice_chunk;	/* a nice chunk of memory to reuse */
EXT U32		nice_chunk_size;/* how nice the chunk of memory is */

/* Stack for currently executing thread--context switch must handle this.     */
EXT SV **	stack_base;	/* stack->array_ary */
EXT SV **	stack_sp;	/* stack pointer now */
EXT SV **	stack_max;	/* stack->array_ary + stack->array_max */

/* likewise for these */

EXT OP *	op;		/* current op--oughta be in a global register */

EXT I32 *	scopestack;	/* blocks we've entered */
EXT I32		scopestack_ix;
EXT I32		scopestack_max;

EXT ANY*	savestack;	/* to save non-local values on */
EXT I32		savestack_ix;
EXT I32		savestack_max;

EXT OP **	retstack;	/* returns we've pushed */
EXT I32		retstack_ix;
EXT I32		retstack_max;

EXT I32 *	markstack;	/* stackmarks we're remembering */
EXT I32 *	markstack_ptr;	/* stackmarks we're remembering */
EXT I32 *	markstack_max;	/* stackmarks we're remembering */

EXT SV **	curpad;

/* temp space */
EXT SV *	Sv;
EXT XPV *	Xpv;
EXT char	tokenbuf[256];
EXT struct stat	statbuf;
#ifdef HAS_TIMES
EXT struct tms	timesbuf;
#endif
EXT STRLEN na;		/* for use in SvPV when length is Not Applicable */

/* for tmp use in stupid debuggers */
EXT int *	di;
EXT short *	ds;
EXT char *	dc;

/* handy constants */
EXTCONST char *	Yes INIT("1");
EXTCONST char *	No INIT("");
EXTCONST char *	hexdigit INIT("0123456789abcdef0123456789ABCDEFx");
EXTCONST char *	patleave INIT("\\.^$@dDwWsSbB+*?|()-nrtfeaxc0123456789[{]}");
EXTCONST char *	vert INIT("|");

EXTCONST char warn_uninit[]
  INIT("Use of uninitialized value");
EXTCONST char warn_nosemi[]
  INIT("Semicolon seems to be missing");
EXTCONST char warn_reserved[]
  INIT("Unquoted string \"%s\" may clash with future reserved word");
EXTCONST char warn_nl[]
  INIT("Unsuccessful %s on filename containing newline");
EXTCONST char no_wrongref[]
  INIT("Can't use %s ref as %s ref");
EXTCONST char no_symref[]
  INIT("Can't use string (\"%.32s\") as %s ref while \"strict refs\" in use");
EXTCONST char no_usym[]
  INIT("Can't use an undefined value as %s reference");
EXTCONST char no_aelem[]
  INIT("Modification of non-creatable array value attempted, subscript %d");
EXTCONST char no_helem[]
  INIT("Modification of non-creatable hash value attempted, subscript \"%s\"");
EXTCONST char no_modify[]
  INIT("Modification of a read-only value attempted");
EXTCONST char no_mem[]
  INIT("Out of memory!\n");
EXTCONST char no_security[]
  INIT("Insecure dependency in %s%s");
EXTCONST char no_sock_func[]
  INIT("Unsupported socket function \"%s\" called");
EXTCONST char no_dir_func[]
  INIT("Unsupported directory function \"%s\" called");
EXTCONST char no_func[]
  INIT("The %s function is unimplemented");
EXTCONST char no_myglob[]
  INIT("\"my\" variable %s can't be in a package");

EXT SV		sv_undef;
EXT SV		sv_no;
EXT SV		sv_yes;
#ifdef CSH
    EXT char *	cshname INIT(CSH);
    EXT I32	cshlen;
#endif

#ifdef DOINIT
EXT char *sig_name[] = { SIG_NAME };
EXT int   sig_num[]  = { SIG_NUM };
EXT SV	* psig_ptr[sizeof(sig_num)/sizeof(*sig_num)];
EXT SV  * psig_name[sizeof(sig_num)/sizeof(*sig_num)];
#else
EXT char *sig_name[];
EXT int   sig_num[];
EXT SV  * psig_ptr[];
EXT SV  * psig_name[];
#endif

/* fast case folding tables */

#ifdef DOINIT
EXTCONST  unsigned char fold[] = {
	0,	1,	2,	3,	4,	5,	6,	7,
	8,	9,	10,	11,	12,	13,	14,	15,
	16,	17,	18,	19,	20,	21,	22,	23,
	24,	25,	26,	27,	28,	29,	30,	31,
	32,	33,	34,	35,	36,	37,	38,	39,
	40,	41,	42,	43,	44,	45,	46,	47,
	48,	49,	50,	51,	52,	53,	54,	55,
	56,	57,	58,	59,	60,	61,	62,	63,
	64,	'a',	'b',	'c',	'd',	'e',	'f',	'g',
	'h',	'i',	'j',	'k',	'l',	'm',	'n',	'o',
	'p',	'q',	'r',	's',	't',	'u',	'v',	'w',
	'x',	'y',	'z',	91,	92,	93,	94,	95,
	96,	'A',	'B',	'C',	'D',	'E',	'F',	'G',
	'H',	'I',	'J',	'K',	'L',	'M',	'N',	'O',
	'P',	'Q',	'R',	'S',	'T',	'U',	'V',	'W',
	'X',	'Y',	'Z',	123,	124,	125,	126,	127,
	128,	129,	130,	131,	132,	133,	134,	135,
	136,	137,	138,	139,	140,	141,	142,	143,
	144,	145,	146,	147,	148,	149,	150,	151,
	152,	153,	154,	155,	156,	157,	158,	159,
	160,	161,	162,	163,	164,	165,	166,	167,
	168,	169,	170,	171,	172,	173,	174,	175,
	176,	177,	178,	179,	180,	181,	182,	183,
	184,	185,	186,	187,	188,	189,	190,	191,
	192,	193,	194,	195,	196,	197,	198,	199,
	200,	201,	202,	203,	204,	205,	206,	207,
	208,	209,	210,	211,	212,	213,	214,	215,
	216,	217,	218,	219,	220,	221,	222,	223,	
	224,	225,	226,	227,	228,	229,	230,	231,
	232,	233,	234,	235,	236,	237,	238,	239,
	240,	241,	242,	243,	244,	245,	246,	247,
	248,	249,	250,	251,	252,	253,	254,	255
};
#else
EXTCONST unsigned char fold[];
#endif

#ifdef DOINIT
EXT unsigned char fold_locale[] = {
	0,	1,	2,	3,	4,	5,	6,	7,
	8,	9,	10,	11,	12,	13,	14,	15,
	16,	17,	18,	19,	20,	21,	22,	23,
	24,	25,	26,	27,	28,	29,	30,	31,
	32,	33,	34,	35,	36,	37,	38,	39,
	40,	41,	42,	43,	44,	45,	46,	47,
	48,	49,	50,	51,	52,	53,	54,	55,
	56,	57,	58,	59,	60,	61,	62,	63,
	64,	'a',	'b',	'c',	'd',	'e',	'f',	'g',
	'h',	'i',	'j',	'k',	'l',	'm',	'n',	'o',
	'p',	'q',	'r',	's',	't',	'u',	'v',	'w',
	'x',	'y',	'z',	91,	92,	93,	94,	95,
	96,	'A',	'B',	'C',	'D',	'E',	'F',	'G',
	'H',	'I',	'J',	'K',	'L',	'M',	'N',	'O',
	'P',	'Q',	'R',	'S',	'T',	'U',	'V',	'W',
	'X',	'Y',	'Z',	123,	124,	125,	126,	127,
	128,	129,	130,	131,	132,	133,	134,	135,
	136,	137,	138,	139,	140,	141,	142,	143,
	144,	145,	146,	147,	148,	149,	150,	151,
	152,	153,	154,	155,	156,	157,	158,	159,
	160,	161,	162,	163,	164,	165,	166,	167,
	168,	169,	170,	171,	172,	173,	174,	175,
	176,	177,	178,	179,	180,	181,	182,	183,
	184,	185,	186,	187,	188,	189,	190,	191,
	192,	193,	194,	195,	196,	197,	198,	199,
	200,	201,	202,	203,	204,	205,	206,	207,
	208,	209,	210,	211,	212,	213,	214,	215,
	216,	217,	218,	219,	220,	221,	222,	223,	
	224,	225,	226,	227,	228,	229,	230,	231,
	232,	233,	234,	235,	236,	237,	238,	239,
	240,	241,	242,	243,	244,	245,	246,	247,
	248,	249,	250,	251,	252,	253,	254,	255
};
#else
EXT unsigned char fold_locale[];
#endif

#ifdef DOINIT
EXTCONST unsigned char freq[] = {	/* letter frequencies for mixed English/C */
	1,	2,	84,	151,	154,	155,	156,	157,
	165,	246,	250,	3,	158,	7,	18,	29,
	40,	51,	62,	73,	85,	96,	107,	118,
	129,	140,	147,	148,	149,	150,	152,	153,
	255,	182,	224,	205,	174,	176,	180,	217,
	233,	232,	236,	187,	235,	228,	234,	226,
	222,	219,	211,	195,	188,	193,	185,	184,
	191,	183,	201,	229,	181,	220,	194,	162,
	163,	208,	186,	202,	200,	218,	198,	179,
	178,	214,	166,	170,	207,	199,	209,	206,
	204,	160,	212,	216,	215,	192,	175,	173,
	243,	172,	161,	190,	203,	189,	164,	230,
	167,	248,	227,	244,	242,	255,	241,	231,
	240,	253,	169,	210,	245,	237,	249,	247,
	239,	168,	252,	251,	254,	238,	223,	221,
	213,	225,	177,	197,	171,	196,	159,	4,
	5,	6,	8,	9,	10,	11,	12,	13,
	14,	15,	16,	17,	19,	20,	21,	22,
	23,	24,	25,	26,	27,	28,	30,	31,
	32,	33,	34,	35,	36,	37,	38,	39,
	41,	42,	43,	44,	45,	46,	47,	48,
	49,	50,	52,	53,	54,	55,	56,	57,
	58,	59,	60,	61,	63,	64,	65,	66,
	67,	68,	69,	70,	71,	72,	74,	75,
	76,	77,	78,	79,	80,	81,	82,	83,
	86,	87,	88,	89,	90,	91,	92,	93,
	94,	95,	97,	98,	99,	100,	101,	102,
	103,	104,	105,	106,	108,	109,	110,	111,
	112,	113,	114,	115,	116,	117,	119,	120,
	121,	122,	123,	124,	125,	126,	127,	128,
	130,	131,	132,	133,	134,	135,	136,	137,
	138,	139,	141,	142,	143,	144,	145,	146
};
#else
EXTCONST unsigned char freq[];
#endif

#ifdef DEBUGGING
#ifdef DOINIT
EXTCONST char* block_type[] = {
	"NULL",
	"SUB",
	"EVAL",
	"LOOP",
	"SUBST",
	"BLOCK",
};
#else
EXTCONST char* block_type[];
#endif
#endif

/*****************************************************************************/
/* This lexer/parser stuff is currently global since yacc is hard to reenter */
/*****************************************************************************/
/* XXX This needs to be revisited, since BEGIN makes yacc re-enter... */

#include "perly.h"

#define LEX_NOTPARSING		11	/* borrowed from toke.c */

typedef enum {
    XOPERATOR,
    XTERM,
    XREF,
    XSTATE,
    XBLOCK,
    XTERMBLOCK
} expectation;

EXT U32		lex_state;	/* next token is determined */
EXT U32		lex_defer;	/* state after determined token */
EXT expectation	lex_expect;	/* expect after determined token */
EXT I32		lex_brackets;	/* bracket count */
EXT I32		lex_formbrack;	/* bracket count at outer format level */
EXT I32		lex_fakebrack;	/* outer bracket is mere delimiter */
EXT I32		lex_casemods;	/* casemod count */
EXT I32		lex_dojoin;	/* doing an array interpolation */
EXT I32		lex_starts;	/* how many interps done on level */
EXT SV *	lex_stuff;	/* runtime pattern from m// or s/// */
EXT SV *	lex_repl;	/* runtime replacement from s/// */
EXT OP *	lex_op;		/* extra info to pass back on op */
EXT OP *	lex_inpat;	/* in pattern $) and $| are special */
EXT I32		lex_inwhat;	/* what kind of quoting are we in */
EXT char *	lex_brackstack;	/* what kind of brackets to pop */
EXT char *	lex_casestack;	/* what kind of case mods in effect */

/* What we know when we're in LEX_KNOWNEXT state. */
EXT YYSTYPE	nextval[5];	/* value of next token, if any */
EXT I32		nexttype[5];	/* type of next token */
EXT I32		nexttoke;

EXT PerlIO * VOL	rsfp INIT(Nullfp);
EXT SV *	linestr;
EXT char *	bufptr;
EXT char *	oldbufptr;
EXT char *	oldoldbufptr;
EXT char *	bufend;
EXT expectation expect INIT(XSTATE);	/* how to interpret ambiguous tokens */
EXT AV * 	rsfp_filters;

EXT I32		multi_start;	/* 1st line of multi-line string */
EXT I32		multi_end;	/* last line of multi-line string */
EXT I32		multi_open;	/* delimiter of said string */
EXT I32		multi_close;	/* delimiter of said string */

EXT GV *	scrgv;
EXT I32		error_count;	/* how many errors so far, max 10 */
EXT I32		subline;	/* line this subroutine began on */
EXT SV *	subname;	/* name of current subroutine */

EXT CV *	compcv;		/* currently compiling subroutine */
EXT AV *	comppad;	/* storage for lexically scoped temporaries */
EXT AV *	comppad_name;	/* variable names for "my" variables */
EXT I32		comppad_name_fill;/* last "introduced" variable offset */
EXT I32		comppad_name_floor;/* start of vars in innermost block */
EXT I32		min_intro_pending;/* start of vars to introduce */
EXT I32		max_intro_pending;/* end of vars to introduce */
EXT I32		padix;		/* max used index in current "register" pad */
EXT I32		padix_floor;	/* how low may inner block reset padix */
EXT I32		pad_reset_pending; /* reset pad on next attempted alloc */
EXT COP		compiling;

EXT I32		thisexpr;	/* name id for nothing_in_common() */
EXT char *	last_uni;	/* position of last named-unary operator */
EXT char *	last_lop;	/* position of last list operator */
EXT OPCODE	last_lop_op;	/* last list operator */
EXT bool	in_my;		/* we're compiling a "my" declaration */
#ifdef FCRYPT
EXT I32		cryptseen;	/* has fast crypt() been initialized? */
#endif

EXT U32		hints;		/* various compilation flags */

				/* Note: the lowest 8 bits are reserved for
				   stuffing into op->op_private */
#define HINT_INTEGER		0x00000001
#define HINT_STRICT_REFS	0x00000002

#define HINT_BLOCK_SCOPE	0x00000100
#define HINT_STRICT_SUBS	0x00000200
#define HINT_STRICT_VARS	0x00000400
#define HINT_LOCALE		0x00000800

/**************************************************************************/
/* This regexp stuff is global since it always happens within 1 expr eval */
/**************************************************************************/

EXT char *	regprecomp;	/* uncompiled string. */
EXT char *	regparse;	/* Input-scan pointer. */
EXT char *	regxend;	/* End of input for compile */
EXT I32		regnpar;	/* () count. */
EXT char *	regcode;	/* Code-emit pointer; &regdummy = don't. */
EXT I32		regsize;	/* Code size. */
EXT I32		regnaughty;	/* How bad is this pattern? */
EXT I32		regsawback;	/* Did we see \1, ...? */

EXT char *	reginput;	/* String-input pointer. */
EXT char *	regbol;		/* Beginning of input, for ^ check. */
EXT char *	regeol;		/* End of input, for $ check. */
EXT char **	regstartp;	/* Pointer to startp array. */
EXT char **	regendp;	/* Ditto for endp. */
EXT U32 *	reglastparen;	/* Similarly for lastparen. */
EXT char *	regtill;	/* How far we are required to go. */
EXT U16		regflags;	/* are we folding, multilining? */
EXT char	regprev;	/* char before regbol, \n if none */

EXT bool	do_undump;	/* -u or dump seen? */
EXT VOL U32	debug;

/***********************************************/
/* Global only to current interpreter instance */
/***********************************************/

#ifdef MULTIPLICITY
#define IEXT
#define IINIT(x)
struct interpreter {
#else
#define IEXT EXT
#define IINIT(x) INIT(x)
#endif

/* pseudo environmental stuff */
IEXT int	Iorigargc;
IEXT char **	Iorigargv;
IEXT GV *	Ienvgv;
IEXT GV *	Isiggv;
IEXT GV *	Iincgv;
IEXT char *	Iorigfilename;
IEXT SV *	Idiehook;
IEXT SV *	Iwarnhook;
IEXT SV *	Iparsehook;

/* Various states of an input record separator SV (rs, nrs) */
#define RsSNARF(sv)   (! SvOK(sv))
#define RsSIMPLE(sv)  (SvOK(sv) && SvCUR(sv))
#define RsPARA(sv)    (SvOK(sv) && ! SvCUR(sv))

/* switches */
IEXT char *	Icddir;
IEXT bool	Iminus_c;
IEXT char	Ipatchlevel[10];
IEXT char **	Ilocalpatches;
IEXT SV *	Inrs;
IEXT char *	Isplitstr IINIT(" ");
IEXT bool	Ipreprocess;
IEXT bool	Iminus_n;
IEXT bool	Iminus_p;
IEXT bool	Iminus_l;
IEXT bool	Iminus_a;
IEXT bool	Iminus_F;
IEXT bool	Idoswitches;
IEXT bool	Idowarn;
IEXT bool	Idoextract;
IEXT bool	Isawampersand;	/* must save all match strings */
IEXT bool	Isawstudy;	/* do fbm_instr on all strings */
IEXT bool	Isawvec;
IEXT bool	Iunsafe;
IEXT char *	Iinplace;
IEXT char *	Ie_tmpname;
IEXT PerlIO *	Ie_fp;
IEXT U32	Iperldb;
	/* This value may be raised by extensions for testing purposes */
IEXT int	Iperl_destruct_level IINIT(0);	/* 0=none, 1=full, 2=full with checks */

/* magical thingies */
IEXT Time_t	Ibasetime;		/* $^T */
IEXT SV *	Iformfeed;		/* $^L */
IEXT char *	Ichopset IINIT(" \n-");	/* $: */
IEXT SV *	Irs;			/* $/ */
IEXT char *	Iofs;			/* $, */
IEXT STRLEN	Iofslen;
IEXT char *	Iors;			/* $\ */
IEXT STRLEN	Iorslen;
IEXT char *	Iofmt;			/* $# */
IEXT I32	Imaxsysfd IINIT(MAXSYSFD); /* top fd to pass to subprocesses */
IEXT int	Imultiline;		/* $*--do strings hold >1 line? */
IEXT I32	Istatusvalue;		/* $? */
#ifdef VMS
IEXT U32	Istatusvalue_vms;
#endif

IEXT struct stat Istatcache;		/* _ */
IEXT GV *	Istatgv;
IEXT SV *	Istatname IINIT(Nullsv);

/* shortcuts to various I/O objects */
IEXT GV *	Istdingv;
IEXT GV *	Ilast_in_gv;
IEXT GV *	Idefgv;
IEXT GV *	Iargvgv;
IEXT GV *	Idefoutgv;
IEXT GV *	Iargvoutgv;

/* shortcuts to regexp stuff */
IEXT GV *	Ileftgv;
IEXT GV *	Iampergv;
IEXT GV *	Irightgv;
IEXT PMOP *	Icurpm;		/* what to do \ interps from */
IEXT I32 *	Iscreamfirst;
IEXT I32 *	Iscreamnext;
IEXT I32	Imaxscream IINIT(-1);
IEXT SV *	Ilastscream;

/* shortcuts to misc objects */
IEXT GV *	Ierrgv;

/* shortcuts to debugging objects */
IEXT GV *	IDBgv;
IEXT GV *	IDBline;
IEXT GV *	IDBsub;
IEXT SV *	IDBsingle;
IEXT SV *	IDBtrace;
IEXT SV *	IDBsignal;
IEXT AV *	Ilineary;	/* lines of script for debugger */
IEXT AV *	Idbargs;	/* args to call listed by caller function */

/* symbol tables */
IEXT HV *	Idefstash;	/* main symbol table */
IEXT HV *	Icurstash;	/* symbol table for current package */
IEXT HV *	Idebstash;	/* symbol table for perldb package */
IEXT SV *	Icurstname;	/* name of current package */
IEXT AV *	Ibeginav;	/* names of BEGIN subroutines */
IEXT AV *	Iendav;		/* names of END subroutines */
IEXT HV *	Istrtab;	/* shared string table */

/* memory management */
IEXT SV **	Itmps_stack;
IEXT I32	Itmps_ix IINIT(-1);
IEXT I32	Itmps_floor IINIT(-1);
IEXT I32	Itmps_max;
IEXT I32	Isv_count;	/* how many SV* are currently allocated */
IEXT I32	Isv_objcount;	/* how many objects are currently allocated */
IEXT SV*	Isv_root;	/* storage for SVs belonging to interp */
IEXT SV*	Isv_arenaroot;	/* list of areas for garbage collection */

/* funky return mechanisms */
IEXT I32	Ilastspbase;
IEXT I32	Ilastsize;
IEXT int	Iforkprocess;	/* so do_open |- can return proc# */

/* subprocess state */
IEXT AV *	Ifdpid;		/* keep fd-to-pid mappings for my_popen */

/* internal state */
IEXT VOL int	Iin_eval;	/* trap "fatal" errors? */
IEXT OP *	Irestartop;	/* Are we propagating an error from croak? */
IEXT int	Idelaymagic;	/* ($<,$>) = ... */
IEXT bool	Idirty;		/* In the middle of tearing things down? */
IEXT U8		Ilocalizing;	/* are we processing a local() list? */
IEXT bool	Itainted;	/* using variables controlled by $< */
IEXT bool	Itainting;	/* doing taint checks */
IEXT char *	Iop_mask IINIT(NULL);	/* masked operations for safe evals */

/* trace state */
IEXT I32	Idlevel;
IEXT I32	Idlmax IINIT(128);
IEXT char *	Idebname;
IEXT char *	Idebdelim;

/* current interpreter roots */
IEXT CV *	Imain_cv;
IEXT OP *	Imain_root;
IEXT OP *	Imain_start;
IEXT OP *	Ieval_root;
IEXT OP *	Ieval_start;

/* runtime control stuff */
IEXT COP * VOL	Icurcop IINIT(&compiling);
IEXT COP *	Icurcopdb IINIT(NULL);
IEXT line_t	Icopline IINIT(NOLINE);
IEXT CONTEXT *	Icxstack;
IEXT I32	Icxstack_ix IINIT(-1);
IEXT I32	Icxstack_max IINIT(128);
IEXT JMPENV 	Istart_env;	/* empty startup sigjmp() environment */
IEXT JMPENV *	Itop_env;	/* ptr. to current sigjmp() environment */
IEXT I32	Irunlevel;

/* stack stuff */
IEXT AV *	Icurstack;		/* THE STACK */
IEXT AV *	Imainstack;	/* the stack when nothing funny is happening */
IEXT SV **	Imystack_base;	/* stack->array_ary */
IEXT SV **	Imystack_sp;	/* stack pointer now */
IEXT SV **	Imystack_max;	/* stack->array_ary + stack->array_max */

/* format accumulators */
IEXT SV *	Iformtarget;
IEXT SV *	Ibodytarget;
IEXT SV *	Itoptarget;

/* statics moved here for shared library purposes */
IEXT SV		Istrchop;	/* return value from chop */
IEXT int	Ifilemode;	/* so nextargv() can preserve mode */
IEXT int	Ilastfd;	/* what to preserve mode on */
IEXT char *	Ioldname;	/* what to preserve mode on */
IEXT char **	IArgv;		/* stuff to free from do_aexec, vfork safe */
IEXT char *	ICmd;		/* stuff to free from do_aexec, vfork safe */
IEXT OP *	Isortcop;	/* user defined sort routine */
IEXT HV *	Isortstash;	/* which is in some package or other */
IEXT GV *	Ifirstgv;	/* $a */
IEXT GV *	Isecondgv;	/* $b */
IEXT AV *	Isortstack;	/* temp stack during pp_sort() */
IEXT AV *	Isignalstack;	/* temp stack during sighandler() */
IEXT SV *	Imystrk;	/* temp key string for do_each() */
IEXT I32	Idumplvl;	/* indentation level on syntax tree dump */
IEXT PMOP *	Ioldlastpm;	/* for saving regexp context during debugger */
IEXT I32	Igensym;	/* next symbol for getsym() to define */
IEXT bool	Ipreambled;
IEXT AV *	Ipreambleav;
IEXT int	Ilaststatval IINIT(-1);
IEXT I32	Ilaststype IINIT(OP_STAT);
IEXT SV *	Imess_sv;

#undef IEXT
#undef IINIT

#ifdef MULTIPLICITY
};
#else
struct interpreter {
    char broiled;
};
#endif

#include "pp.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "proto.h"

#ifdef EMBED
#define Perl_sv_setptrobj(rv,ptr,name) Perl_sv_setref_iv(rv,name,(IV)ptr)
#define Perl_sv_setptrref(rv,ptr) Perl_sv_setref_iv(rv,Nullch,(IV)ptr)
#else
#define sv_setptrobj(rv,ptr,name) sv_setref_iv(rv,name,(IV)ptr)
#define sv_setptrref(rv,ptr) sv_setref_iv(rv,Nullch,(IV)ptr)
#endif

#ifdef __cplusplus
};
#endif

/* The following must follow proto.h */

#ifdef DOINIT

EXT MGVTBL vtbl_sv =	{magic_get,
				magic_set,
					magic_len,
						0,	0};
EXT MGVTBL vtbl_env =	{0,	magic_set_all_env,
				0,	magic_clear_all_env,
							0};
EXT MGVTBL vtbl_envelem =	{0,	magic_setenv,
					0,	magic_clearenv,
							0};
EXT MGVTBL vtbl_sig =	{0,	0,		 0, 0, 0};
EXT MGVTBL vtbl_sigelem =	{magic_getsig,
					magic_setsig,
					0,	magic_clearsig,
							0};
EXT MGVTBL vtbl_pack =	{0,	0,	0,	magic_wipepack,
							0};
EXT MGVTBL vtbl_packelem =	{magic_getpack,
				magic_setpack,
					0,	magic_clearpack,
							0};
EXT MGVTBL vtbl_dbline =	{0,	magic_setdbline,
					0,	0,	0};
EXT MGVTBL vtbl_isa =	{0,	magic_setisa,
					0,	magic_setisa,
							0};
EXT MGVTBL vtbl_isaelem =	{0,	magic_setisa,
					0,	0,	0};
EXT MGVTBL vtbl_arylen =	{magic_getarylen,
				magic_setarylen,
					0,	0,	0};
EXT MGVTBL vtbl_glob =	{magic_getglob,
				magic_setglob,
					0,	0,	0};
EXT MGVTBL vtbl_mglob =	{0,	magic_setmglob,
					0,	0,	0};
EXT MGVTBL vtbl_nkeys =	{0,	magic_setnkeys,
					0,	0,	0};
EXT MGVTBL vtbl_taint =	{magic_gettaint,magic_settaint,
					0,	0,	0};
EXT MGVTBL vtbl_substr =	{0,	magic_setsubstr,
					0,	0,	0};
EXT MGVTBL vtbl_vec =	{0,	magic_setvec,
					0,	0,	0};
EXT MGVTBL vtbl_pos =	{magic_getpos,
				magic_setpos,
					0,	0,	0};
EXT MGVTBL vtbl_bm =	{0,	magic_setbm,
					0,	0,	0};
EXT MGVTBL vtbl_fm =	{0,	magic_setfm,
					0,	0,	0};
EXT MGVTBL vtbl_uvar =	{magic_getuvar,
				magic_setuvar,
					0,	0,	0};
EXT MGVTBL vtbl_defelem = {magic_getdefelem,magic_setdefelem,
					0,	0,	magic_freedefelem};

#ifdef USE_LOCALE_COLLATE
EXT MGVTBL vtbl_collxfrm = {0,
				magic_setcollxfrm,
					0,	0,	0};
#endif

#ifdef OVERLOAD
EXT MGVTBL vtbl_amagic =       {0,     magic_setamagic,
                                        0,      0,      magic_setamagic};
EXT MGVTBL vtbl_amagicelem =   {0,     magic_setamagic,
                                        0,      0,      magic_setamagic};
#endif /* OVERLOAD */

#else /* !DOINIT */

EXT MGVTBL vtbl_sv;
EXT MGVTBL vtbl_env;
EXT MGVTBL vtbl_envelem;
EXT MGVTBL vtbl_sig;
EXT MGVTBL vtbl_sigelem;
EXT MGVTBL vtbl_pack;
EXT MGVTBL vtbl_packelem;
EXT MGVTBL vtbl_dbline;
EXT MGVTBL vtbl_isa;
EXT MGVTBL vtbl_isaelem;
EXT MGVTBL vtbl_arylen;
EXT MGVTBL vtbl_glob;
EXT MGVTBL vtbl_mglob;
EXT MGVTBL vtbl_nkeys;
EXT MGVTBL vtbl_taint;
EXT MGVTBL vtbl_substr;
EXT MGVTBL vtbl_vec;
EXT MGVTBL vtbl_pos;
EXT MGVTBL vtbl_bm;
EXT MGVTBL vtbl_fm;
EXT MGVTBL vtbl_uvar;
EXT MGVTBL vtbl_defelem;

#ifdef USE_LOCALE_COLLATE
EXT MGVTBL vtbl_collxfrm;
#endif

#ifdef OVERLOAD
EXT MGVTBL vtbl_amagic;
EXT MGVTBL vtbl_amagicelem;
#endif /* OVERLOAD */

#endif /* !DOINIT */

#ifdef OVERLOAD

EXT long amagic_generation;

#define NofAMmeth 58
#ifdef DOINIT
EXTCONST char * AMG_names[NofAMmeth] = {
  "fallback",	"abs",			/* "fallback" should be the first. */
  "bool",	"nomethod",
  "\"\"",	"0+",
  "+",		"+=",
  "-",		"-=",
  "*",		"*=",
  "/",		"/=",
  "%",		"%=",
  "**",		"**=",
  "<<",		"<<=",
  ">>",		">>=",
  "&",		"&=",
  "|",		"|=",
  "^",		"^=",
  "<",		"<=",
  ">",		">=",
  "==",		"!=",
  "<=>",	"cmp",
  "lt",		"le",
  "gt",		"ge",
  "eq",		"ne",
  "!",		"~",
  "++",		"--",
  "atan2",	"cos",
  "sin",	"exp",
  "log",	"sqrt",
  "x",		"x=",
  ".",		".=",
  "=",		"neg"
};
#else
EXTCONST char * AMG_names[NofAMmeth];
#endif /* def INITAMAGIC */

struct am_table {
  long was_ok_sub;
  long was_ok_am;
  U32 flags;
  CV* table[NofAMmeth];
  long fallback;
};
struct am_table_short {
  long was_ok_sub;
  long was_ok_am;
  U32 flags;
};
typedef struct am_table AMT;
typedef struct am_table_short AMTS;

#define AMGfallNEVER	1
#define AMGfallNO	2
#define AMGfallYES	3

#define AMTf_AMAGIC		1
#define AMT_AMAGIC(amt)		((amt)->flags & AMTf_AMAGIC)
#define AMT_AMAGIC_on(amt)	((amt)->flags |= AMTf_AMAGIC)
#define AMT_AMAGIC_off(amt)	((amt)->flags &= ~AMTf_AMAGIC)

enum {
  fallback_amg,	abs_amg,
  bool__amg,	nomethod_amg,
  string_amg,	numer_amg,
  add_amg,	add_ass_amg,
  subtr_amg,	subtr_ass_amg,
  mult_amg,	mult_ass_amg,
  div_amg,	div_ass_amg,
  mod_amg,	mod_ass_amg,
  pow_amg,	pow_ass_amg,
  lshift_amg,	lshift_ass_amg,
  rshift_amg,	rshift_ass_amg,
  band_amg,	band_ass_amg,
  bor_amg,	bor_ass_amg,
  bxor_amg,	bxor_ass_amg,
  lt_amg,	le_amg,
  gt_amg,	ge_amg,
  eq_amg,	ne_amg,
  ncmp_amg,	scmp_amg,
  slt_amg,	sle_amg,
  sgt_amg,	sge_amg,
  seq_amg,	sne_amg,
  not_amg,	compl_amg,
  inc_amg,	dec_amg,
  atan2_amg,	cos_amg,
  sin_amg,	exp_amg,
  log_amg,	sqrt_amg,
  repeat_amg,   repeat_ass_amg,
  concat_amg,	concat_ass_amg,
  copy_amg,	neg_amg
};

/*
 * some compilers like to redefine cos et alia as faster
 * (and less accurate?) versions called F_cos et cetera (Quidquid
 * latine dictum sit, altum viditur.)  This trick collides with
 * the Perl overloading (amg).  The following #defines fool both.
 */

#ifdef _FASTMATH
#   ifdef atan2
#       define F_atan2_amg  atan2_amg
#   endif
#   ifdef cos
#       define F_cos_amg    cos_amg
#   endif
#   ifdef exp
#       define F_exp_amg    exp_amg
#   endif
#   ifdef log
#       define F_log_amg    log_amg
#   endif
#   ifdef pow
#       define F_pow_amg    pow_amg
#   endif
#   ifdef sin
#       define F_sin_amg    sin_amg
#   endif
#   ifdef sqrt
#       define F_sqrt_amg   sqrt_amg
#   endif
#endif /* _FASTMATH */

#endif /* OVERLOAD */

#define PERLDB_ALL	0xff
#define PERLDBf_SUB	0x01		/* Debug sub enter/exit. */
#define PERLDBf_LINE	0x02		/* Keep line #. */
#define PERLDBf_NOOPT	0x04		/* Switch off optimizations. */
#define PERLDBf_INTER	0x08		/* Preserve more data for
					   later inspections.  */
#define PERLDBf_SUBLINE	0x10		/* Keep subr source lines. */
#define PERLDBf_SINGLE	0x20		/* Start with single-step on. */

#define PERLDB_SUB	(perldb && (perldb & PERLDBf_SUB))
#define PERLDB_LINE	(perldb && (perldb & PERLDBf_LINE))
#define PERLDB_NOOPT	(perldb && (perldb & PERLDBf_NOOPT))
#define PERLDB_INTER	(perldb && (perldb & PERLDBf_INTER))
#define PERLDB_SUBLINE	(perldb && (perldb & PERLDBf_SUBLINE))
#define PERLDB_SINGLE	(perldb && (perldb & PERLDBf_SINGLE))

#ifdef USE_LOCALE_COLLATE
EXT U32		collation_ix;		/* Collation generation index */
EXT char *	collation_name;		/* Name of current collation */
EXT bool	collation_standard INIT(TRUE); /* Assume simple collation */
EXT Size_t	collxfrm_base;		/* Basic overhead in *xfrm() */
EXT Size_t	collxfrm_mult INIT(2);	/* Expansion factor in *xfrm() */
#endif /* USE_LOCALE_COLLATE */

#ifdef USE_LOCALE_NUMERIC

EXT char *	numeric_name;		/* Name of current numeric locale */
EXT bool	numeric_standard INIT(TRUE); /* Assume simple numerics */
EXT bool	numeric_local INIT(TRUE);    /* Assume local numerics */

#define SET_NUMERIC_STANDARD() \
    STMT_START {				\
	if (! numeric_standard)			\
	    perl_set_numeric_standard();	\
    } STMT_END

#define SET_NUMERIC_LOCAL() \
    STMT_START {				\
	if (! numeric_local)			\
	    perl_set_numeric_local();		\
    } STMT_END

#else /* !USE_LOCALE_NUMERIC */

#define SET_NUMERIC_STANDARD()  /**/
#define SET_NUMERIC_LOCAL()     /**/

#endif /* !USE_LOCALE_NUMERIC */

#if !defined(PERLIO_IS_STDIO) && defined(HAS_ATTRIBUTE)
/* 
 * Now we have __attribute__ out of the way 
 * Remap printf 
 */
#define printf PerlIO_stdoutf
#endif

#endif /* Include guard */

