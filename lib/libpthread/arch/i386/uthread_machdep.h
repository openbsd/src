/*	$OpenBSD: uthread_machdep.h,v 1.8 2003/01/24 20:58:23 marc Exp $	*/
/* David Leonard, <d@csee.uq.edu.au>. Public domain. */

#include <machine/npx.h>

struct _machdep_state {
	int		esp;
	struct save87	fpreg;
};

