/*	$OpenBSD: uthread_machdep.h,v 1.2 2004/02/25 03:48:36 deraadt Exp $	*/

#include <sys/types.h>
#include <machine/fpu.h>

struct _machdep_state {
	long	rsp;
	/* must be 128-bit aligned */
	struct savefpu   fpreg __attribute__ ((aligned (16)));
};
