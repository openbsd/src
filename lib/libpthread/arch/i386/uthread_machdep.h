/*	$OpenBSD: uthread_machdep.h,v 1.9 2004/02/21 22:55:20 deraadt Exp $	*/
/* David Leonard, <d@csee.uq.edu.au>. Public domain. */

#include <machine/npx.h>

struct _machdep_state {
	int		esp;
	int		pad[3];
	union savefpu	fpreg;	/* must be 128-bit aligned */
};
