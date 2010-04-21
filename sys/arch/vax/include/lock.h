/*	$OpenBSD: lock.h,v 1.2 2010/04/21 03:03:26 deraadt Exp $	*/

/* public domain */

#ifndef	_VAX_LOCK_H_
#define	_VAX_LOCK_H_

#include <machine/atomic.h>

typedef volatile u_int	__cpu_simple_lock_t;

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
	__cpu_simple_lock_t old;

	do {
		old = __SIMPLELOCK_LOCKED;
		__asm__ __volatile__
		   ("\tmovl\t$1, %1\n"		/* _SPLINLOCK_LOCKED */
		    "\tbbssi\t$0, %0, 1f\n"
		    "\tmovl\t$0, %1\n"		/* _SPLINLOCK_UNLOCKED */
		    "1:\n" : "=m" (*l), "=r" (old) : "0" (*l));
	} while (old != __SIMPLELOCK_UNLOCKED);
}

static __inline__ int
__cpu_simple_lock_try(__cpu_simple_lock_t *l)
{
	__cpu_simple_lock_t old = __SIMPLELOCK_LOCKED;

	__asm__ __volatile__
	   ("\tmovl\t$1, %1\n"		/* _SPLINLOCK_LOCKED */
	    "\tbbssi\t$0, %0, 1f\n"
	    "\tmovl\t$0, %1\n"		/* _SPLINLOCK_UNLOCKED */
	    "1:\n" : "=m" (*l), "=r" (old) : "0" (*l));

	return (old == __SIMPLELOCK_UNLOCKED);
}

static __inline__ void
__cpu_simple_unlock(__cpu_simple_lock_t *l)
{
	*l = __SIMPLELOCK_UNLOCKED;
}

#endif	/* _VAX_LOCK_H_ */
