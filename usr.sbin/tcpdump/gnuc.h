/*	$OpenBSD: gnuc.h,v 1.4 2002/02/17 19:42:39 millert Exp $	*/

/* @(#) $Header: /home/cvs/src/usr.sbin/tcpdump/Attic/gnuc.h,v 1.4 2002/02/17 19:42:39 millert Exp $ (LBL) */

/* inline foo */
#ifdef __GNUC__
#define inline __inline
#else
#define inline
#endif

/*
 * Handle new and old "dead" routine prototypes
 *
 * For example:
 *
 *	__dead void foo(void) __attribute__((volatile));
 *
 */
#ifdef __GNUC__
#ifndef __dead
#define __dead volatile
#endif
#if __GNUC__ < 2  || (__GNUC__ == 2 && __GNUC_MINOR__ < 5)
#ifndef __attribute__
#define __attribute__(args)
#endif
#endif
#else
#ifndef __dead
#define __dead
#endif
#ifndef __attribute__
#define __attribute__(args)
#endif
#endif
