/*	$OpenBSD: uthread_machdep.h,v 1.5 2000/10/04 05:55:35 d Exp $	*/
/* David Leonard, <d@csee.uq.edu.au>. Public domain. */

struct _machdep_state {
	int	fp;		/* frame pointer */
	int	pc;		/* program counter */
};
