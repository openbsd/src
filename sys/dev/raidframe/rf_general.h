/*	$OpenBSD: rf_general.h,v 1.1 1999/01/11 14:29:23 niklas Exp $	*/
/*	$NetBSD: rf_general.h,v 1.1 1998/11/13 04:20:30 oster Exp $	*/
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
 * rf_general.h -- some general-use definitions
 */

/*
 * :  
 * Log: rf_general.h,v 
 * Revision 1.26  1996/08/09 16:44:57  jimz
 * sunos port
 *
 * Revision 1.25  1996/08/07  21:08:57  jimz
 * get NBPG defined for IRIX
 *
 * Revision 1.24  1996/08/06  22:02:06  jimz
 * include linux/user.h for linux to get NBPG
 *
 * Revision 1.23  1996/07/27  23:36:08  jimz
 * Solaris port of simulator
 *
 * Revision 1.22  1996/06/09  02:36:46  jimz
 * lots of little crufty cleanup- fixup whitespace
 * issues, comment #ifdefs, improve typing in some
 * places (esp size-related)
 *
 * Revision 1.21  1996/05/30  23:22:16  jimz
 * bugfixes of serialization, timing problems
 * more cleanup
 *
 * Revision 1.20  1996/05/27  18:56:37  jimz
 * more code cleanup
 * better typing
 * compiles in all 3 environments
 *
 * Revision 1.19  1996/05/24  22:17:04  jimz
 * continue code + namespace cleanup
 * typed a bunch of flags
 *
 * Revision 1.18  1996/05/23  21:46:35  jimz
 * checkpoint in code cleanup (release prep)
 * lots of types, function names have been fixed
 *
 * Revision 1.17  1996/05/23  00:33:23  jimz
 * code cleanup: move all debug decls to rf_options.c, all extern
 * debug decls to rf_options.h, all debug vars preceded by rf_
 *
 * Revision 1.16  1996/05/21  18:53:13  jimz
 * be sure that noop macros don't confuse conditionals and loops
 *
 * Revision 1.15  1996/05/18  19:51:34  jimz
 * major code cleanup- fix syntax, make some types consistent,
 * add prototypes, clean out dead code, et cetera
 *
 * Revision 1.14  1996/05/08  21:01:24  jimz
 * fixed up enum type names that were conflicting with other
 * enums and function names (ie, "panic")
 * future naming trends will be towards RF_ and rf_ for
 * everything raidframe-related
 *
 * Revision 1.13  1995/12/12  18:10:06  jimz
 * MIN -> RF_MIN, MAX -> RF_MAX, ASSERT -> RF_ASSERT
 * fix 80-column brain damage in comments
 *
 * Revision 1.12  1995/12/01  18:29:08  root
 * added copyright info
 *
 * Revision 1.11  1995/09/19  22:59:52  jimz
 * Add kernel macro RF_DKU_END_IO(). When DKUSAGE is not defined,
 * this is a no-op. When it is defined, it calls dku_end_io()
 * correctly given a raidframe unit number and a buf pointer.
 *
 * Revision 1.10  1995/07/03  18:13:56  holland
 * changed kernel defn of GETTIME
 *
 * Revision 1.9  1995/07/02  15:07:42  holland
 * bug fixes related to getting distributed sparing numbers
 *
 * Revision 1.8  1995/06/12  15:54:40  rachad
 * Added garbege collection for log structured storage
 *
 * Revision 1.7  1995/06/03  19:18:16  holland
 * changes related to kernelization: access traces
 * changes related to distributed sparing: some bug fixes
 *
 * Revision 1.6  1995/05/01  13:28:00  holland
 * parity range locks, locking disk requests, recon+parityscan in kernel, etc.
 *
 * Revision 1.5  1995/04/06  14:47:56  rachad
 * merge completed
 *
 * Revision 1.4  1995/03/15  20:45:23  holland
 * distr sparing changes.
 *
 * Revision 1.3  1995/02/03  22:31:36  holland
 * many changes related to kernelization
 *
 * Revision 1.2  1994/11/29  21:37:10  danner
 * Added divide by zero check.
 *
 */

/*#define NOASSERT*/

#ifndef _RF__RF_GENERAL_H_
#define _RF__RF_GENERAL_H_

#ifdef _KERNEL
#define KERNEL
#endif

#if !defined(KERNEL) && !defined(NOASSERT)
#include <assert.h>
#endif /* !KERNEL && !NOASSERT */

/* error reporting and handling */

#ifndef KERNEL

#define RF_ERRORMSG(s)            fprintf(stderr,(s))
#define RF_ERRORMSG1(s,a)         fprintf(stderr,(s),(a))
#define RF_ERRORMSG2(s,a,b)       fprintf(stderr,(s),(a),(b))
#define RF_ERRORMSG3(s,a,b,c)     fprintf(stderr,(s),(a),(b),(c))
#define RF_ERRORMSG4(s,a,b,c,d)   fprintf(stderr,(s),(a),(b),(c),(d))
#define RF_ERRORMSG5(s,a,b,c,d,e) fprintf(stderr,(s),(a),(b),(c),(d),(e))
#ifndef NOASSERT
#define RF_ASSERT(x)   {assert(x);}
#else /* !NOASSERT */
#define RF_ASSERT(x)   {/*noop*/}
#endif /* !NOASSERT */
#define RF_PANIC()              {printf("YIKES!  Something terrible happened at line %d of file %s.  Use a debugger.\n",__LINE__,__FILE__); abort();}

#else /* !KERNEL */
#if (defined(__NetBSD__) || defined(__OpenBSD__)) && defined(_KERNEL)
#include<sys/systm.h> /* printf, sprintf, and friends */
#endif
#define RF_ERRORMSG(s)            printf((s))
#define RF_ERRORMSG1(s,a)         printf((s),(a))
#define RF_ERRORMSG2(s,a,b)       printf((s),(a),(b))
#define RF_ERRORMSG3(s,a,b,c)     printf((s),(a),(b),(c))
#define RF_ERRORMSG4(s,a,b,c,d)   printf((s),(a),(b),(c),(d))
#define RF_ERRORMSG5(s,a,b,c,d,e) printf((s),(a),(b),(c),(d),(e))
#define perror(x)
extern char rf_panicbuf[];
#define RF_PANIC() {sprintf(rf_panicbuf,"raidframe error at line %d file %s",__LINE__,__FILE__); panic(rf_panicbuf);}

#ifdef RF_ASSERT
#undef RF_ASSERT
#endif /* RF_ASSERT */
#ifndef NOASSERT
#define RF_ASSERT(_x_) { \
  if (!(_x_)) { \
    sprintf(rf_panicbuf, \
        "raidframe error at line %d file %s (failed asserting %s)\n", \
        __LINE__, __FILE__, #_x_); \
    panic(rf_panicbuf); \
  } \
}
#else /* !NOASSERT */
#define RF_ASSERT(x) {/*noop*/}
#endif /* !NOASSERT */

#endif  /* !KERNEL */

/* random stuff */
#define RF_MAX(a,b) (((a) > (b)) ? (a) : (b))
#define RF_MIN(a,b) (((a) < (b)) ? (a) : (b))

/* divide-by-zero check */
#define RF_DB0_CHECK(a,b) ( ((b)==0) ? 0 : (a)/(b) )

/* get time of day */
#ifdef KERNEL
#if !defined(__NetBSD__) && !defined(__OpenBSD__)
extern struct timeval time;
#endif /* !__NetBSD__ && !__OpenBSD__ */
#define RF_GETTIME(_t) microtime(&(_t))
#else /* KERNEL */
#define RF_GETTIME(_t) gettimeofday(&(_t), NULL);
#endif /* KERNEL */

/*
 * zero memory- not all bzero calls go through here, only
 * those which in the kernel may have a user address
 */
#ifdef KERNEL
#if !defined(__NetBSD__) && !defined(__OpenBSD__)
#define RF_BZERO(_bp,_b,_l) if (IS_SYS_VA(_b)) bzero(_b,_l); else rf_BzeroWithRemap(_bp,_b,_l)
#else 

#define RF_BZERO(_bp,_b,_l)  bzero(_b,_l)  /* XXX This is likely incorrect. GO*/
#endif /* __NetBSD__ || __OpenBSD__ */
#else /* KERNEL */
#define RF_BZERO(_bp,_b,_l) bzero(_b,_l)
#endif /* KERNEL */

#ifdef sun
#include <sys/param.h>
#ifndef NBPG
#define NBPG PAGESIZE
#endif /* !NBPG */
#endif /* sun */

#ifdef IRIX
#include <sys/tfp.h>
#define NBPG _PAGESZ
#endif /* IRIX */

#ifdef LINUX
#include <linux/user.h>
#endif /* LINUX */

#define RF_UL(x)           ((unsigned long) (x))
#define RF_PGMASK          RF_UL(NBPG-1)
#define RF_BLIP(x)         (NBPG - (RF_UL(x) & RF_PGMASK))    /* bytes left in page */
#define RF_PAGE_ALIGNED(x) ((RF_UL(x) & RF_PGMASK) == 0)

#ifdef KERNEL
#if !defined(__NetBSD__) && !defined(__OpenBSD__)
#include <dkusage.h>
#endif
#if DKUSAGE > 0
#define RF_DKU_END_IO(_unit_,_bp_) { \
	int s = splbio(); \
	dku_end_io(DKU_RAIDFRAME_BUS, _unit_, 0, \
			(((_bp_)->b_flags&(B_READ|B_WRITE) == B_READ) ? \
		    CAM_DIR_IN : CAM_DIR_OUT), \
			(_bp_)->b_bcount); \
	splx(s); \
}
#else /* DKUSAGE > 0 */
#define RF_DKU_END_IO(unit) { /* noop */ }
#endif /* DKUSAGE > 0 */
#endif /* KERNEL */

#ifdef __STDC__
#define RF_STRING(_str_) #_str_
#else /* __STDC__ */
#define RF_STRING(_str_) "_str_"
#endif /* __STDC__ */

#endif /* !_RF__RF_GENERAL_H_ */
