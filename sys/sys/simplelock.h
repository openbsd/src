/*	$OpenBSD: simplelock.h,v 1.7 1999/01/19 20:44:42 art Exp $	*/

#ifndef _SIMPLELOCK_H_
#define _SIMPLELOCK_H_
/*
 * A simple spin lock.
 *
 * This structure only sets one bit of data, but is sized based on the
 * minimum word size that can be operated on by the hardware test-and-set
 * instruction. It is only needed for multiprocessors, as uniprocessors
 * will always run to completion or a sleep. It is an error to hold one
 * of these locks while a process is sleeping.
 */
struct simplelock {
	int	lock_data;
};

#ifdef _KERNEL

#ifndef NCPUS
#define NCPUS 1
#endif

#if NCPUS == 1

#if !defined(SIMPLELOCK_DEBUG)
#define	simple_lock(alp)
#define	simple_lock_try(alp)	(1)	/* always succeeds */
#define	simple_unlock(alp)

static __inline void simple_lock_init __P((struct simplelock *));

static __inline void
simple_lock_init(lkp)
	struct simplelock *lkp;
{

	lkp->lock_data = 0;
}

#else

void _simple_unlock __P((__volatile struct simplelock *, const char *, int));
#define simple_unlock(alp) _simple_unlock(alp, __FILE__, __LINE__)
int _simple_lock_try __P((__volatile struct simplelock *, const char *, int));
#define simple_lock_try(alp) _simple_lock_try(alp, __FILE__, __LINE__)
void _simple_lock __P((__volatile struct simplelock *, const char *, int));
#define simple_lock(alp) _simple_lock(alp, __FILE__, __LINE__)
void simple_lock_init __P((struct simplelock *alp));

#endif /* !defined(SIMPLELOCK_DEBUG) */

#else  /* NCPUS >  1 */

/*
 * The simple-lock routines are the primitives out of which the lock
 * package is built. The machine-dependent code must implement an
 * atomic test_and_set operation that indivisibly sets the simple lock
 * to non-zero and returns its old value. It also assumes that the
 * setting of the lock to zero below is indivisible. Simple locks may
 * only be used for exclusive locks.
 */

static __inline void
simple_lock(lkp)
	__volatile struct simplelock *lkp;
{

	while (test_and_set(&lkp->lock_data))
		continue;
}

static __inline int
simple_lock_try(lkp)
	__volatile struct simplelock *lkp;
{

	return (!test_and_set(&lkp->lock_data))
}

static __inline void
simple_unlock(lkp)
	__volatile struct simplelock *lkp;
{

	lkp->lock_data = 0;
}
#endif /* NCPUS > 1 */

#endif /* _KERNEL */

#endif /* !_SIMPLELOCK_H_ */
