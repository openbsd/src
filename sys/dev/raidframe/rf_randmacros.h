/*	$OpenBSD: rf_randmacros.h,v 1.1 1999/01/11 14:29:45 niklas Exp $	*/
/*	$NetBSD: rf_randmacros.h,v 1.1 1998/11/13 04:20:33 oster Exp $	*/
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

/* rf_randmacros.h
 * some macros to simplify using random in a multithreaded environment
 */

/* :  
 * Log: rf_randmacros.h,v 
 * Revision 1.17  1996/08/12 22:37:57  jimz
 * use regular random() stuff for AIX
 *
 * Revision 1.16  1996/08/11  00:41:03  jimz
 * fix up for aix4
 *
 * Revision 1.15  1996/07/29  05:22:34  jimz
 * use rand/srand on hpux
 *
 * Revision 1.14  1996/07/28  20:31:39  jimz
 * i386netbsd port
 * true/false fixup
 *
 * Revision 1.13  1996/07/27  23:36:08  jimz
 * Solaris port of simulator
 *
 * Revision 1.12  1996/07/22  19:52:16  jimz
 * switched node params to RF_DagParam_t, a union of
 * a 64-bit int and a void *, for better portability
 * attempted hpux port, but failed partway through for
 * lack of a single C compiler capable of compiling all
 * source files
 *
 * Revision 1.11  1996/07/18  22:57:14  jimz
 * port simulator to AIX
 *
 * Revision 1.10  1996/07/15  17:22:18  jimz
 * nit-pick code cleanup
 * resolve stdlib problems on DEC OSF
 *
 * Revision 1.9  1996/06/09  02:36:46  jimz
 * lots of little crufty cleanup- fixup whitespace
 * issues, comment #ifdefs, improve typing in some
 * places (esp size-related)
 *
 * Revision 1.8  1996/06/03  23:28:26  jimz
 * more bugfixes
 * check in tree to sync for IPDS runs with current bugfixes
 * there still may be a problem with threads in the script test
 * getting I/Os stuck- not trivially reproducible (runs ~50 times
 * in a row without getting stuck)
 *
 * Revision 1.7  1996/05/23  21:46:35  jimz
 * checkpoint in code cleanup (release prep)
 * lots of types, function names have been fixed
 *
 * Revision 1.6  1996/05/21  18:52:56  jimz
 * mask out highest bit from RANDOM (was causing angst)
 *
 * Revision 1.5  1996/05/18  19:51:34  jimz
 * major code cleanup- fix syntax, make some types consistent,
 * add prototypes, clean out dead code, et cetera
 *
 * Revision 1.4  1995/12/06  15:05:41  root
 * added copyright info
 *
 */

#ifndef _RF__RF_RANDMACROS_H_
#define _RF__RF_RANDMACROS_H_

#ifndef KERNEL

#ifdef __osf__
/*
 * Okay, here's the deal. The DEC man page for initstate_r() sez:
 *
 * int initstate_r(unsigned seed, char *state, int size, char **retval,
 *       struct random_data *rand_data);
 *
 * That wouldn't bug me so much, if /usr/include/random.h on the alpha
 * didn't say:
 *
 * int initstate_r(unsigned, char *, int, RANDMOD *);
 *
 * Most of the other random functions have similar problems (docs
 * don't match random.h). This is the case for random_r(), for
 * instance. Generally, I'm inclined to trust the code over the
 * documentation. Problem is, I have no clue what the arguments for
 * the prototyped versions are, since they don't have descriptive names
 * comma the bastards.
 *
 * Update: I looked at the DU sources to get this straightened out.
 * The docs are correct. and everything in random.h is wrong. Uh, that's
 * really cool or something. Not. I'm going to try slapping in prototypes
 * that match my view of the universe, here.
 *
 * Okay, now let's have some more fun. /usr/include/stdlib.h also defines
 * all this stuff, only differently. I mean differently from random.h,
 * _and_ differently from the source. How cool is _that_?
 *
 * --jimz
 */
#ifndef _NO_PROTO
#define _NO_PROTO
#define _RF_SPANKME
#endif /* !_NO_PROTO */
#include <random.h>
#ifdef _RF_SPANKME
#undef _NO_PROTO
#undef _RF_SPANKME
#endif /* _RF_SPANKME */

extern int initstate_r(unsigned seed, char *arg_state, int n, char **retval,
  struct random_data *rand_data);
extern int random_r(int *retval, struct random_data *rand_data);
#endif /* __osf__ */
#ifdef SIMULATE
#if defined(DEC_OSF) || defined(hpux)
extern int random(void);
extern int srandom(unsigned);
#endif /* DEC_OSF || hpux */
#if defined(AIX) && RF_AIXVERS == 3
extern int random(void);
extern int srandom(unsigned);
#endif /* AIX && RF_AIXVERS == 3 */
#endif /* SIMULATE */

#define RF_FASTRANDOM 0 /* when >0 make RANDOM a macro instead of a function */

#ifdef __osf__
long rf_do_random(long *rval, struct random_data *rdata);  /* in utils.c */
#endif /* __osf__ */

#ifndef SIMULATE

#ifdef __osf__
/*
 * Mark's original comment about this rigamarole was, "What a pile of crap."
 */
#define RF_DECLARE_RANDOM                          \
  struct random_data randdata;                  \
  long randstate[64+1];            		\
  char *stptr = ((char *) randstate)+4;         \
  char *randst;                                 \
  long randval

#define RF_DECLARE_STATIC_RANDOM                         \
  static struct random_data randdata_st;              \
  static long randstate_st[64+1];            	      \
  static char *stptr_st = ((char *) randstate_st)+4;  \
  static char *randst_st;                             \
  long randval_st;

#define RF_INIT_RANDOM(_s_)                                         \
  randdata.state = NULL;                                         \
  initstate_r((unsigned) (_s_), stptr, 64, &randst, &randdata);

#define RF_INIT_STATIC_RANDOM(_s_)                                         \
  randdata_st.state = NULL;                                             \
  initstate_r((unsigned) (_s_), stptr_st, 64, &randst_st, &randdata_st);

#if RF_FASTRANDOM > 0
#define RF_RANDOM() (random_r(&randval, &randdata),randval)
#define RF_STATIC_RANDOM() (random_r(&randval_st, &randdata_st),randval_st)
#else /* RF_FASTRANDOM > 0 */
#define RF_RANDOM() (rf_do_random(&randval, &randdata)&0x7fffffffffffffff)
#define RF_STATIC_RANDOM() rf_do_random(&randval_st, &randdata_st)
#endif /* RF_FASTRANDOM > 0 */

#define RF_SRANDOM(_s_) srandom_r((_s_), &randdata)
#define RF_STATIC_SRANDOM(_s_) srandom_r((_s_), &randdata_st)
#endif /* __osf__ */

#ifdef AIX
#define RF_INIT_STATIC_RANDOM(_s_)
#define RF_DECLARE_STATIC_RANDOM    static int rf_rand_decl##__LINE__
#define RF_DECLARE_RANDOM           int rf_rand_decl##__LINE__
#define RF_RANDOM()                 random()
#define RF_STATIC_RANDOM()          random()
#define RF_INIT_RANDOM(_n_)         srandom(_n_)
#endif /* AIX */

#else /* !SIMULATE */

#define RF_INIT_STATIC_RANDOM(_s_)
#define RF_DECLARE_STATIC_RANDOM    static int rf_rand_decl##__LINE__
#define RF_DECLARE_RANDOM           int rf_rand_decl##__LINE__
#if defined(sun) || defined(hpux)
#define RF_RANDOM()                 rand()
#define RF_STATIC_RANDOM()          rand()
#define RF_INIT_RANDOM(_n_)         srand(_n_)
#else /* sun || hpux */
#define RF_RANDOM()                 random()
#define RF_STATIC_RANDOM()          random()
#define RF_INIT_RANDOM(_n_)         srandom(_n_)
#endif /* sun || hpux */

#endif /* !SIMULATE */

#endif /* !KERNEL */

#endif /* !_RF__RF_RANDMACROS_H_ */
