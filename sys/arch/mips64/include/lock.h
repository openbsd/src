/*	$OpenBSD: lock.h,v 1.6 2014/09/30 06:51:58 jmatthew Exp $	*/

/* public domain */

#ifndef	_MIPS64_LOCK_H_
#define	_MIPS64_LOCK_H_

#include <sys/atomic.h>

#define rw_cas __cpu_cas
static __inline int
__cpu_cas(volatile unsigned long *addr, unsigned long old, unsigned long new)
{
	return (atomic_cas_ulong(addr, old, new) != old);
}

#endif	/* _MIPS64_LOCK_H_ */
