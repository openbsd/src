/*	$OpenBSD: lock.h,v 1.7 2015/02/11 00:14:11 dlg Exp $	*/

/* public domain */

#ifndef	_MIPS64_LOCK_H_
#define	_MIPS64_LOCK_H_

#include <sys/atomic.h>

static __inline int
__cpu_cas(volatile unsigned long *addr, unsigned long old, unsigned long new)
{
	return (atomic_cas_ulong(addr, old, new) != old);
}

#endif	/* _MIPS64_LOCK_H_ */
