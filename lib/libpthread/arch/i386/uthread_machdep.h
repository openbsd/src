/*	$OpenBSD: uthread_machdep.h,v 1.10 2010/06/30 19:04:51 kettenis Exp $	*/
/* David Leonard, <d@csee.uq.edu.au>. Public domain. */

#include <machine/npx.h>

struct _machdep_state {
	int		esp;
	/* must be 128-bit aligned */
	union savefpu	fpreg __attribute__ ((aligned (16)));
};
