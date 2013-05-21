/*	$OpenBSD: lock.h,v 1.8 2013/05/21 20:05:30 tedu Exp $	*/

/* public domain */

#ifndef	_MACHINE_LOCK_H_
#define	_MACHINE_LOCK_H_

#include <machine/atomic.h>
#include <machine/ctlreg.h>

#define	rw_cas(p, o, n)		(sparc64_casx(p, o, n) != o)

#endif	/* _MACHINE_LOCK_H_ */
