/*	$OpenBSD: atomic.h,v 1.8 2011/03/23 16:54:34 pirofti Exp $	*/

/* Public Domain */

#ifndef _ARM_ATOMIC_H_
#define _ARM_ATOMIC_H_

#if defined(_KERNEL)

/*
 * on pre-v6 arm processors, it is necessary to disable interrupts if
 * in the kernel and atomic updates are necessary without full mutexes
 */

void atomic_setbits_int(__volatile unsigned int *, unsigned int);
void atomic_clearbits_int(__volatile unsigned int *, unsigned int);

#endif /* defined(_KERNEL) */
#endif /* _ARM_ATOMIC_H_ */
