/*	$OpenBSD: spinlock.h,v 1.3 2011/11/14 14:29:53 deraadt Exp $	*/

#ifndef _MACHINE_SPINLOCK_H_
#define _MACHINE_SPINLOCK_H_

#define _SPINLOCK_UNLOCKED	(1)
#define _SPINLOCK_LOCKED	(0)
typedef long _spinlock_lock_t __attribute__((__aligned__(16)));

#endif
