/*
** File locking
**
** Snarfed from mod_rewrite.c. Munged up for our use.
*/

#include "ap_config.h"
#include "sdbm_tune.h"   /* include the prototypes */

    /* The locking support:
     * Try to determine whether we should use fcntl() or flock().
     * Would be better ap_config.h could provide this... :-(
     * Small monkey business to ensure that fcntl is preferred,
     * unless we specified USE_FLOCK_SERIALIZED_ACCEPT during compile.
     */
#define USE_FLOCK 1
#include <sys/file.h>

/* NOTE: this function blocks until it acquires the lock */
int sdbm_fd_lock(int fd, int readonly)
{
    int rc;

    while (   ((rc = flock(fd, readonly ? LOCK_SH : LOCK_EX)) < 0)
              && (errno == EINTR)               ) {
        continue;
    }
#ifdef USE_LOCKING
    /* ### this doesn't allow simultaneous reads! */
    /* ### this doesn't block forever */
    /* Lock the first byte */
    lseek(fd, 0, SEEK_SET);
    rc = _locking(fd, _LK_LOCK, 1);
#endif
#ifdef USE_SEM_LOCKING
	if ((locking_sem != 0) && (TimedWaitOnLocalSemaphore (locking_sem, 10000) != 0))
		rc = -1;
	else
		rc = 1;
#endif

    return rc;
}

int sdbm_fd_unlock(int fd)
{
    int rc;

    rc = flock(fd, LOCK_UN);
#ifdef USE_LOCKING
    lseek(fd, 0, SEEK_SET);
    rc = _locking(fd, _LK_UNLCK, 1);
#endif
#ifdef USE_SEM_LOCKING
	if (locking_sem)
		SignalLocalSemaphore (locking_sem);
	rc = 1;
#endif

    return rc;
}
