/*	$OpenBSD: uthread_machdep.h,v 1.7 2000/10/04 05:55:34 d Exp $	*/
/* David Leonard, <d@csee.uq.edu.au>. Public domain. */

#include <machine/reg.h>

struct _machdep_state {
	int		esp;
	struct fpreg	fpreg;
};

