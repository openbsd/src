/*	$OpenBSD: spinlock.h,v 1.2 1999/02/07 23:50:59 d Exp $	*/

#ifndef _MACHINE_SPINLOCK_H_
#define _MACHINE_SPINLOCK_H_

#define _SPINLOCK_UNLOCKED	(0x00)
#define _SPINLOCK_LOCKED	(0xFF)
typedef unsigned char _spinlock_lock_t;

#endif
