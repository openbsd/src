/*	$OpenBSD: uthread_machdep.h,v 1.1 2001/09/10 20:00:14 jason Exp $	*/
/* David Leonard, <d@csee.uq.edu.au>. Public domain. */

struct _machdep_state {
	int	fp;		/* frame pointer */
	int	pc;		/* program counter */
};
