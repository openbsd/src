/*	$OpenBSD: lock.h,v 1.9 2014/01/30 00:51:13 dlg Exp $	*/

/* public domain */

#ifndef	_MACHINE_LOCK_H_
#define	_MACHINE_LOCK_H_

#include <sys/atomic.h>

#define	rw_cas(p, o, n)		(atomic_cas_ulong(p, o, n) != o)

#endif	/* _MACHINE_LOCK_H_ */
