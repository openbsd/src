/*	$OpenBSD: spinlock.h,v 1.1 2005/04/01 10:40:48 mickey Exp $	*/

#ifndef _MACHINE_SPINLOCK_H_
#define _MACHINE_SPINLOCK_H_

#define _SPINLOCK_UNLOCKED	(1)
#define _SPINLOCK_LOCKED	(0)
typedef long _spinlock_lock_t;

#endif
