/*	$OpenBSD: spinlock.h,v 1.1 2001/09/10 20:00:14 jason Exp $	*/

#ifndef _MACHINE_SPINLOCK_H_
#define _MACHINE_SPINLOCK_H_

#define _SPINLOCK_UNLOCKED	(0x00)
#define _SPINLOCK_LOCKED	(0xFF)
typedef unsigned char _spinlock_lock_t;

#endif
