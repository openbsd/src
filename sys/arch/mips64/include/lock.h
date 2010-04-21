/*	$OpenBSD: lock.h,v 1.4 2010/04/21 03:03:26 deraadt Exp $	*/

/* public domain */

#ifndef	_MIPS64_LOCK_H_
#define	_MIPS64_LOCK_H_

#include <mips64/atomic.h>

typedef volatile u_int __cpu_simple_lock_t;

#define	__SIMPLELOCK_LOCKED	1
#define	__SIMPLELOCK_UNLOCKED	0

static __inline__ void
__cpu_simple_lock_init(__cpu_simple_lock_t *l)
{
	*l = __SIMPLELOCK_UNLOCKED;
}

static __inline__ void
__cpu_simple_lock(__cpu_simple_lock_t *l)
{
	__cpu_simple_lock_t old, new;

	do {
		new = __SIMPLELOCK_LOCKED;
		__asm__ __volatile__
		   ("1:\tll\t%0, %1\n" 
		    "\tsc\t%2, %1\n"
		    "\tbeqz\t%2, 1b\n"
		    "\t nop" : "=&r" (old) : "m" (*l), "r" (new));
	} while (old != __SIMPLELOCK_UNLOCKED);
}

static __inline__ int
__cpu_simple_lock_try(__cpu_simple_lock_t *l)
{
	__cpu_simple_lock_t old, new = __SIMPLELOCK_LOCKED;

	__asm__ __volatile__
	   ("1:\tll\t%0, %1\n" 
	    "\tsc\t%2, %1\n"
	    "\tbeqz\t%2, 1b\n"
	    "\t nop" : "=&r" (old) : "m" (*l), "r" (new));

	return (old == __SIMPLELOCK_UNLOCKED);
}

static __inline__ void
__cpu_simple_unlock(__cpu_simple_lock_t *l)
{
	*l = __SIMPLELOCK_UNLOCKED;
}

#define rw_cas __cpu_cas
static __inline int
__cpu_cas(volatile unsigned long *addr, unsigned long old, unsigned long new)
{
	int success, scratch0, scratch1;

        __asm volatile(
		".set noreorder\n"
		"1:\n"
		"lld	%0, (%5)\n"
		"bne	%0, %3, 2f\n"
		"move	%1, %4\n"
		"scd	%1, (%5)\n"
		"beqz	%1, 1b\n"
		"move   %2, $0\n"
		"j	3f\n"
		"nop\n"
		"2:\n"
		"daddi   %2, $0, 1\n"
		"3:\n"
		".set reorder\n"
		: "=&r"(scratch0), "=&r"(scratch1), "=&r"(success)
		: "r"(old), "r"(new), "r"(addr)
		: "memory");

	return success;
}

#endif	/* _MIPS64_LOCK_H_ */
