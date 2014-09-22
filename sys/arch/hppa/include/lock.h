/*	$OpenBSD: lock.h,v 1.8 2014/09/22 12:12:23 dlg Exp $	*/

/* public domain */

#ifndef	_MACHINE_LOCK_H_
#define	_MACHINE_LOCK_H_

#include <machine/atomic.h>

#define rw_cas(p, o, n) (atomic_cas_ulong(p, o, n) != o)

#endif	/* _MACHINE_LOCK_H_ */
