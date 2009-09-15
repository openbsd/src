/*	$OpenBSD: lock.h,v 1.2 2009/09/15 04:54:31 syuu Exp $	*/

/* public domain */

#ifndef	_MIPS64_LOCK_H_
#define	_MIPS64_LOCK_H_

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

#endif	/* _MIPS64_LOCK_H_ */
