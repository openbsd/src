/*	$OpenBSD: _spinlock.h,v 1.1 1998/12/21 07:22:26 d Exp $	*/

#define _SPINLOCK_UNLOCKED	(0)
#define _SPINLOCK_LOCKED	(1)
typedef register_t _spinlock_lock_t;

