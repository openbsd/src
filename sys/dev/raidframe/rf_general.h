/*	$OpenBSD: rf_general.h,v 1.6 2003/04/27 11:22:54 ho Exp $	*/
/*	$NetBSD: rf_general.h,v 1.5 2000/03/03 02:04:48 oster Exp $	*/

/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Mark Holland
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

/*
 * rf_general.h -- Some general-use definitions.
 */

/*#define NOASSERT*/

#ifndef	_RF__RF_GENERAL_H_
#define	_RF__RF_GENERAL_H_

/* Error reporting and handling. */

#ifdef	_KERNEL
#include <sys/systm.h>		/* printf, snprintf, and friends. */
#endif

#define	RF_ERRORMSG(s)		printf((s))
#define	RF_ERRORMSG1(s,a)	printf((s), (a))
#define	RF_ERRORMSG2(s,a,b)	printf((s), (a), (b))
#define	RF_ERRORMSG3(s,a,b,c)	printf((s), (a), (b), (c))

extern char rf_panicbuf[2048];
#define	RF_PANIC()							\
do {									\
	snprintf(rf_panicbuf, sizeof rf_panicbuf,			\
	    "RAIDframe error at line %d file %s",			\
	    __LINE__, __FILE__);					\
	panic(rf_panicbuf);						\
} while (0)

#ifdef	_KERNEL
#ifdef	RF_ASSERT
#undef	RF_ASSERT
#endif	/* RF_ASSERT */
#ifndef	NOASSERT
#define	RF_ASSERT(_x_)							\
do {									\
	if (!(_x_)) {							\
		snprintf(rf_panicbuf, sizeof rf_panicbuf,		\
		    "RAIDframe error at line %d"			\
		    " file %s (failed asserting %s)\n", __LINE__,	\
		     __FILE__, #_x_);					\
		panic(rf_panicbuf);					\
	}								\
} while (0)
#else	/* !NOASSERT */
#define	RF_ASSERT(x)		{/*noop*/}
#endif	/* !NOASSERT */
#else	/* _KERNEL */
#define	RF_ASSERT(x)		{/*noop*/}
#endif	/* _KERNEL */

/* Random stuff. */
#define	RF_MAX(a,b)		(((a) > (b)) ? (a) : (b))
#define	RF_MIN(a,b)		(((a) < (b)) ? (a) : (b))

/* Divide-by-zero check. */
#define	RF_DB0_CHECK(a,b)	(((b)==0) ? 0 : (a)/(b))

/* Get time of day. */
#define	RF_GETTIME(_t)		microtime(&(_t))

/*
 * Zero memory - Not all bzero calls go through here, only
 * those which in the kernel may have a user address.
 */

#define	RF_BZERO(_bp,_b,_l)	bzero(_b, _l)	/*
						 * XXX This is likely
						 * incorrect. GO
						 */

#define	RF_UL(x)		((unsigned long)(x))
#define	RF_PGMASK		RF_UL(NBPG-1)
#define	RF_BLIP(x)		(NBPG - (RF_UL(x) & RF_PGMASK))	/*
								 * Bytes left
								 * in page.
								 */
#define	RF_PAGE_ALIGNED(x)	((RF_UL(x) & RF_PGMASK) == 0)

#ifdef	__STDC__
#define	RF_STRING(_str_)	#_str_
#else	/* __STDC__ */
#define	RF_STRING(_str_)	"_str_"
#endif	/* __STDC__ */

#endif	/* !_RF__RF_GENERAL_H_ */
