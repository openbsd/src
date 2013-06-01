/*	$OpenBSD: spinlock.h,v 1.2 2013/06/01 20:47:40 tedu Exp $	*/

#ifndef _ARM_SPINLOCK_H_
#define _ARM_SPINLOCK_H_

#define _ATOMIC_LOCK_UNLOCKED	(0)
#define _ATOMIC_LOCK_LOCKED	(1)
typedef int _atomic_lock_t;

#ifndef _KERNEL
int _atomic_lock(volatile _atomic_lock_t *);
#endif

#endif
