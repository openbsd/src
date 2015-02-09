/*	$OpenBSD: private.h,v 1.31 2015/02/09 13:39:16 tedu Exp $	*/
#ifndef PRIVATE_H

#define PRIVATE_H

/*
** This file is in the public domain, so clarified as of
** 1996-06-05 by Arthur David Olson.
*/

/* OpenBSD defaults */
#define TM_GMTOFF		tm_gmtoff
#define TM_ZONE			tm_zone
#define PCTS			1
#define ALL_STATE		1
#define STD_INSPIRED		1
#define HAVE_STRERROR		1
#define HAVE_STDINT_H		1

/*
** This header is for use ONLY with the time conversion code.
** There is no guarantee that it will remain unchanged,
** or that it will remain at all.
** Do NOT copy it to any system include directory.
** Thank you!
*/

#define GRANDPARENTED	"Local time zone must be set--see zic manual page"

#define HAVE_ADJTIME		1

#define HAVE_SETTIMEOFDAY	3

#define HAVE_SYMLINK		1

#define HAVE_SYS_STAT_H		1

#define HAVE_SYS_WAIT_H		1

#define HAVE_UNISTD_H		1

#define HAVE_UTMPX_H		0

/*
** Nested includes
*/

#include <sys/types.h>	/* for time_t */
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <limits.h>	/* for CHAR_BIT et al. */
#include <time.h>
#include <stdlib.h>


#include <sys/wait.h>	/* for WIFEXITED and WEXITSTATUS */

#include <unistd.h>	/* for F_OK, R_OK, and other POSIX goodness */

#include <stdint.h>

#ifndef INT_FAST64_MAX
/* Pre-C99 GCC compilers define __LONG_LONG_MAX__ instead of LLONG_MAX.  */
#if defined LLONG_MAX || defined __LONG_LONG_MAX__
typedef long long	int_fast64_t;
#else /* ! (defined LLONG_MAX || defined __LONG_LONG_MAX__) */
#if (LONG_MAX >> 31) < 0xffffffff
Please use a compiler that supports a 64-bit integer type (or wider);
you may need to compile with "-DHAVE_STDINT_H".
#endif /* (LONG_MAX >> 31) < 0xffffffff */
typedef long		int_fast64_t;
#endif /* ! (defined LLONG_MAX || defined __LONG_LONG_MAX__) */
#endif /* !defined INT_FAST64_MAX */

/*
** Private function declarations.
*/

const char *	scheck(const char * string, const char * format);

/*
** Finally, some convenience items.
*/

#ifndef TRUE
#define TRUE	1
#endif /* !defined TRUE */

#ifndef FALSE
#define FALSE	0
#endif /* !defined FALSE */

#ifndef TYPE_BIT
#define TYPE_BIT(type)	(sizeof (type) * CHAR_BIT)
#endif /* !defined TYPE_BIT */

#ifndef TYPE_SIGNED
#define TYPE_SIGNED(type) (((type) -1) < 0)
#endif /* !defined TYPE_SIGNED */

/*
** Since the definition of TYPE_INTEGRAL contains floating point numbers,
** it cannot be used in preprocessor directives.
*/

#ifndef TYPE_INTEGRAL
#define TYPE_INTEGRAL(type) (((type) 0.5) != 0.5)
#endif /* !defined TYPE_INTEGRAL */

#ifndef INT_STRLEN_MAXIMUM
/*
** 302 / 1000 is log10(2.0) rounded up.
** Subtract one for the sign bit if the type is signed;
** add one for integer division truncation;
** add one more for a minus sign if the type is signed.
*/
#define INT_STRLEN_MAXIMUM(type) \
	((TYPE_BIT(type) - TYPE_SIGNED(type)) * 302 / 1000 + \
	1 + TYPE_SIGNED(type))
#endif /* !defined INT_STRLEN_MAXIMUM */

/*
** INITIALIZE(x)
*/

#ifndef GNUC_or_lint
#ifdef lint
#define GNUC_or_lint
#endif /* defined lint */
#ifndef lint
#ifdef __GNUC__
#define GNUC_or_lint
#endif /* defined __GNUC__ */
#endif /* !defined lint */
#endif /* !defined GNUC_or_lint */

#ifndef INITIALIZE
#ifdef GNUC_or_lint
#define INITIALIZE(x)	((x) = 0)
#endif /* defined GNUC_or_lint */
#ifndef GNUC_or_lint
#define INITIALIZE(x)
#endif /* !defined GNUC_or_lint */
#endif /* !defined INITIALIZE */

/*
** For the benefit of GNU folk...
** `_(MSGID)' uses the current locale's message library string for MSGID.
** The default is to use gettext if available, and use MSGID otherwise.
*/

#ifndef _
#define _(msgid) msgid
#endif /* !defined _ */

#ifndef TZ_DOMAIN
#define TZ_DOMAIN "tz"
#endif /* !defined TZ_DOMAIN */

#ifndef YEARSPERREPEAT
#define YEARSPERREPEAT		400	/* years before a Gregorian repeat */
#endif /* !defined YEARSPERREPEAT */

/*
** The Gregorian year averages 365.2425 days, which is 31556952 seconds.
*/

#ifndef AVGSECSPERYEAR
#define AVGSECSPERYEAR		31556952L
#endif /* !defined AVGSECSPERYEAR */

#ifndef SECSPERREPEAT
#define SECSPERREPEAT		((int_fast64_t) YEARSPERREPEAT * (int_fast64_t) AVGSECSPERYEAR)
#endif /* !defined SECSPERREPEAT */

#ifndef SECSPERREPEAT_BITS
#define SECSPERREPEAT_BITS	34	/* ceil(log2(SECSPERREPEAT)) */
#endif /* !defined SECSPERREPEAT_BITS */

/*
** UNIX was a registered trademark of The Open Group in 2003.
*/

#endif /* !defined PRIVATE_H */
