/*	$OpenBSD: uthread_machdep.h,v 1.2 2002/01/02 19:11:13 art Exp $	*/
/* Arutr Grabowski <art@openbsd.org>. Public domain. */

struct _machdep_state {
	long	fp;		/* frame pointer */
	long	pc;		/* program counter */
};
