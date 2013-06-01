/*	$OpenBSD: spinlock.h,v 1.3 2013/06/01 20:47:40 tedu Exp $	*/

#ifndef _MACHINE_SPINLOCK_H_
#define _MACHINE_SPINLOCK_H_

#define _ATOMIC_LOCK_UNLOCKED	(0x00)
#define _ATOMIC_LOCK_LOCKED	(0xFF)
typedef unsigned char _atomic_lock_t;

#ifndef _KERNEL
int _atomic_lock(volatile _atomic_lock_t *);
#endif

#endif
