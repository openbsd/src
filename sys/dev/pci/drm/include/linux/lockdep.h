/* Public domain. */

#ifndef _LINUX_LOCKDEP_H
#define _LINUX_LOCKDEP_H

struct lock_class_key {
};

#define might_lock(lock)
#define lockdep_assert_held(lock)	do { (void)(lock); } while(0)
#define lock_acquire(lock, a, b, c, d, e, f)
#define lock_release(lock, a, b)
#define lock_acquire_shared_recursive(lock, a, b, c, d)
#define lockdep_set_subclass(a, b)

#endif
