/* 
 * Copyright (C) 1995 Advanced RISC Machines Limited. All rights reserved.
 * 
 * This software may be freely used, copied, modified, and distributed
 * provided that the above copyright notice is preserved in all copies of the
 * software.
 */


#ifndef _host_LOADED
#define _host_LOADED 1

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>

#ifndef SEEK_SET
#  define SEEK_SET  0
#endif
#ifndef SEEK_CUR
#  define SEEK_CUR  1
#endif
#ifndef SEEK_END
#  define SEEK_END  2
#endif

/* The following for the benefit of compiling on SunOS */
#ifndef offsetof
#  define offsetof(T, member)  ((char *)&(((T *)0)->member) - (char *)0)
#endif

/* A temporary sop to older compilers */
#if defined (__NetBSD__) || defined (unix)
#  ifndef __unix              /* (good for long-term portability?)  */
#    define __unix    1
#  endif
#endif

#if defined(__unix)
/* Generic unix -- hopefully a split into other variants will not be    */
/* needed.  However, beware the 'bsd' test above and safe_toupper etc.  */
/* which cope with backwards (pre-posix/X/open) unix compatility.       */
#  define COMPILING_ON_UNIX     1
#endif
#if defined(_WIN32)
#  define COMPILING_ON_WIN32    1
#  if !defined(__CYGWIN__)
#    define COMPILING_ON_WINDOWS  1
#  endif
#endif
#if defined(_CONSOLE)
#  define COMPILING_ON_WINDOWS_CONSOLE 1
#  define COMPILING_ON_WINDOWS 1
#endif
/* The '(defined(__sparc) && defined(P_tmpdir)                     */
/* && !defined(__svr4__))' is to detect gcc on SunOS.              */
/* C++ compilers don't have to define __STDC__                     */
#if (defined(__sparc) && defined(P_tmpdir))
#  if defined(__svr4__)
#    define COMPILING_ON_SOLARIS
#  else
#    define COMPILING_ON_SUNOS
#  endif
#endif
#ifdef __hppa
#  define COMPILING_ON_HPUX
#endif

/*
 * The following typedefs may need alteration for obscure host machines.
 */
#if defined(__alpha) && defined(__osf__) /* =========================== */
/* Under OSF the alpha has 64-bit pointers and 64-bit longs.            */
typedef                int   int32;
typedef unsigned       int   unsigned32;
/* IPtr and UPtr are 'ints of same size as pointer'.  Do not use in     */
/* new code.  Currently only used within 'ncc'.                         */
typedef          long  int   IPtr;
typedef unsigned long  int   UPtr;

#else /* all hosts except alpha under OSF ============================= */

typedef          long  int   int32;
typedef unsigned long  int   unsigned32;
/* IPtr and UPtr are 'ints of same size as pointer'.  Do not use in     */
/* new code.  Currently only used within 'ncc'.                         */
#define IPtr int32
#define UPtr unsigned32
#endif /* ============================================================= */

typedef          short int   int16;
typedef unsigned short int   unsigned16;
typedef   signed       char  int8;
typedef unsigned       char  unsigned8;

/* The following code defines the 'bool' type to be 'int' under C       */
/* and real 'bool' under C++.  It also avoids warnings such as          */
/* C++ keyword 'bool' used as identifier.  It can be overridden by      */
/* defining IMPLEMENT_BOOL_AS_ENUM or IMPLEMENT_BOOL_AS_INT.            */
#undef _bool

#ifdef IMPLEMENT_BOOL_AS_ENUM
   enum _bool { _false, _true };
#  define _bool enum _bool
#elif defined(IMPLEMENT_BOOL_AS_INT) || !defined(__cplusplus)
#  define _bool int
#  define _false 0
#  define _true 1
#endif

#ifdef _bool
#  if defined(_MFC_VER) || defined(__CC_NORCROFT) /* When using MS Visual C/C++ v4.2 */
#    define bool _bool /* avoids "'bool' is reserved word" warning      */
#  else
#    ifndef bool
       typedef _bool bool;
#    endif
#  endif
#  define true _true
#  define false _false
#endif

#define YES   true
#define NO    false
#undef TRUE             /* some OSF headers define as 1                 */
#define TRUE  true
#undef FALSE            /* some OSF headers define as 1                 */
#define FALSE false

/* 'uint' conflicts with some Unixen sys/types.h, so we now don't define it */
typedef unsigned8  uint8;
typedef unsigned16 uint16;
typedef unsigned32 uint32;

typedef unsigned   Uint;
typedef unsigned8  Uint8;
typedef unsigned16 Uint16;
typedef unsigned32 Uint32;

#ifdef ALPHA_TASO_SHORT_ON_OSF /* was __alpha && __osf.                 */
/* The following sets ArgvType for 64-bit pointers so that              */
/* DEC Unix (OSF) cc can be used with the -xtaso_short compiler option  */
/* to force pointers to be 32-bit.  Not currently used since most/all   */
/* ARM tools accept 32 or 64 bit pointers transparently.  See IPtr.     */
#pragma pointer_size (save)
#pragma pointer_size (long)
typedef char *ArgvType;
#pragma pointer_size (restore)
#else
typedef char *ArgvType;
#endif

/*
 * Rotate macros
 */
#define ROL_32(val, n) \
((((unsigned32)(val) << (n)) | ((unsigned32)(val) >> (32-(n)))) & 0xFFFFFFFFL)
#define ROR_32(val, n) \
((((unsigned32)(val) >> (n)) | ((unsigned32)(val) << (32-(n)))) & 0xFFFFFFFFL)

#ifdef COMPILING_ON_UNIX
#  define FOPEN_WB     "w"
#  define FOPEN_RB     "r"
#  define FOPEN_RWB    "r+"
#  ifndef __STDC__                     /* caveat RISCiX... */
#    define remove(file) unlink(file)  /* a horrid hack, but probably best? */
#  endif
#else
#  define FOPEN_WB     "wb"
#  define FOPEN_RB     "rb"
#  define FOPEN_RWB    "rb+"
#endif

#ifndef FILENAME_MAX
#  define FILENAME_MAX 256
#endif

#if (!defined(__STDC__) && !defined(__cplusplus)) || defined(COMPILING_ON_SUNOS)
/* Use bcopy rather than memmove, as memmove is not available.     */
/* There does not seem to be a header for bcopy.                   */
void bcopy(const char *src, char *dst, int length);
#  define memmove(d,s,l) bcopy(s,d,l)

/* BSD/SUN don't have strtoul(), but then strtol() doesn't barf on */
/* overflow as required by ANSI... This bodge is horrid.           */
#  define strtoul(s, ptr, base) strtol(s, ptr, base)

/* strtod is present in the C-library but is not in stdlib.h       */
extern double strtod(const char *str, char **ptr);
#endif

/* For systems that do not define EXIT_SUCCESS and EXIT_FAILURE */
#ifndef EXIT_SUCCESS
#  define EXIT_SUCCESS           0
#endif
#ifndef EXIT_FAILURE
#  define EXIT_FAILURE           1
#endif

#ifndef IGNORE
#define IGNORE(x) (x = x)  /* Silence compiler warnings for unused arguments */
#endif

/* Define endian-ness of host */

#if defined(__acorn) || defined(__mvs) || defined(_WIN32) || \
  (defined(__alpha) && defined(__osf__))
#  define HOST_ENDIAN_LITTLE
#elif defined(__sparc) || defined(macintosh)
#  define HOST_ENDIAN_BIG
#else
#  define HOST_ENDIAN_UNKNOWN
#endif

#endif

/* Needs to be supplied by the host.  */
extern void Fail (const char *, ...);

/* end of host.h */
