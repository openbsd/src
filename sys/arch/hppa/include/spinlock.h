/*	$OpenBSD: spinlock.h,v 1.2 2005/12/19 21:30:10 marco Exp $	*/

#ifndef _MACHINE_SPINLOCK_H_
#define _MACHINE_SPINLOCK_H_

#define _SPINLOCK_UNLOCKED	(1)
#define _SPINLOCK_LOCKED	(0)
typedef int _spinlock_lock_t __attribute__((__aligned__(16)));

#endif
