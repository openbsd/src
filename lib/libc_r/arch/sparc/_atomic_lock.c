/*	$OpenBSD	*/
/* atomic lock for sparc */
 
#include "spinlock.h"

register_t
_atomic_lock(volatile register_t * lock)
{
	return _thread_slow_atomic_lock(lock);
}
