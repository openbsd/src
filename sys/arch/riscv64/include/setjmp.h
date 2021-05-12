/*	$OpenBSD: setjmp.h,v 1.3 2021/05/12 01:20:52 jsg Exp $	*/

/*
 * machine/setjmp.h: machine dependent setjmp-related information.
 */

#define	_JBLEN		256 /* sp, ra, [f]s0-11, fscr, magic val, sigmask */
#define	_JB_SIGMASK	28
