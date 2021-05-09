/* $OpenBSD: setjmp.h,v 1.2 2021/05/09 21:26:06 drahn Exp $ */

/*
 * machine/setjmp.h: machine dependent setjmp-related information.
 */

#define	_JBLEN		256 /* sp, ra, [f]s0-11, fscr, magic val, sigmask */
#define	_JB_SIGMASK	28
