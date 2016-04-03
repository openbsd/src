/*	$OpenBSD: lock.h,v 1.11 2016/04/03 11:05:26 jsg Exp $	*/

/* public domain */

#ifndef _MACHINE_LOCK_H_
#define	_MACHINE_LOCK_H_

#define SPINLOCK_SPIN_HOOK __asm volatile("pause": : :"memory");

#endif /* _MACHINE_LOCK_H_ */
