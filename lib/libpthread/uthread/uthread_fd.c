/*	$OpenBSD: uthread_fd.c,v 1.22 2004/06/07 21:11:23 marc Exp $	*/
/*
 * Copyright (c) 1995-1998 John Birrell <jb@cimlogic.com.au>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by John Birrell.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY JOHN BIRRELL AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: uthread_fd.c,v 1.13 1999/08/28 00:03:31 peter Exp $
 *
 */
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#ifdef _THREAD_SAFE
#include <pthread.h>
#include "pthread_private.h"

/* Static variables: */
static	spinlock_t	fd_table_lock	= _SPINLOCK_INITIALIZER;

/*
 * Build a new fd entry and return it.
 */
static struct fd_table_entry *
_thread_fd_entry(void)
{
	struct fd_table_entry *entry;

	entry = (struct fd_table_entry *) malloc(sizeof(struct fd_table_entry));
	if (entry != NULL) {
		memset(entry, 0, sizeof *entry);
		_SPINLOCK_INIT(&entry->lock);
		TAILQ_INIT(&entry->r_queue);
		TAILQ_INIT(&entry->w_queue);
	}
	return entry;
}

/*
 * Initialize the thread fd table for dup-ed fds, typically the stdio
 * fds.
 */

void
_thread_fd_init(void)
{
	int saved_errno;
	int fd;
	int fd2;
	int flag;
	int *flags;
	struct fd_table_entry *entry;

	saved_errno = errno;
	flags = calloc(_thread_dtablesize, sizeof *flags);
	if (flags == NULL)
		PANIC("Cannot allocate memory for flags table");


	/* read the current file flags */
	for (fd = 0; fd < _thread_dtablesize; fd += 1)
		flags[fd] = _thread_sys_fcntl(fd, F_GETFL, 0);

	/*
	 * Now toggle the sync flags and see what other fd's
	 * change.   Those are the dup-ed fd's.   Dup-ed fd's are
	 * added to the table, all others are NOT added to the
	 * table.  They MUST NOT be added as the fds may belong
	 * to dlopen.   As dlclose doesn't go through the thread code
	 * so the entries would never be cleaned.
	 */

	_SPINLOCK(&fd_table_lock);
	for (fd = 0; fd < _thread_dtablesize; fd += 1) {
		if (flags[fd] == -1)
			continue;
		entry = _thread_fd_entry();
		if (entry != NULL) {
			entry->flags = flags[fd];
			_thread_sys_fcntl(fd, F_SETFL,
					  entry->flags ^ O_SYNC);
			for (fd2 = fd + 1; fd2 < _thread_dtablesize; fd2 += 1) {
				if (flags[fd2] == -1)
					continue;
				flag = _thread_sys_fcntl(fd2, F_GETFL, 0);
				if (flag != flags[fd2]) {
					entry->refcnt += 1;
					_thread_fd_table[fd2] = entry;
					flags[fd2] = -1;
				}
			}
			if (entry->refcnt) {
				entry->refcnt += 1;
				_thread_fd_table[fd] = entry;
				flags[fd] |= O_NONBLOCK;
			} else
				free(entry);
		}
	}
	_SPINUNLOCK(&fd_table_lock);

	/* lastly, restore the file flags.   Flags for files that we
	   know to be duped have been modified so set the non-blocking'
	   flag.  Other files will be set to non-blocking when the
	   thread code is forced to take notice of the file. */
	for (fd = 0; fd < _thread_dtablesize; fd += 1)
		if (flags[fd] != -1)
			_thread_sys_fcntl(fd, F_SETFL, flags[fd]);

	free(flags);
	errno = saved_errno;
}

/*
 * Initialize the fd_table entry for the given fd.
 *
 * This function *must* return -1 and set the thread specific errno
 * as a system call. This is because the error return from this
 * function is propagated directly back from thread-wrapped system
 * calls.
 */
int
_thread_fd_table_init(int fd)
{
	int	ret = 0;
	struct fd_table_entry *entry;
	int	saved_errno;

	if (fd < 0 || fd >= _thread_dtablesize) {
		/*
		 * file descriptor is out of range, Return a bad file
		 * descriptor error:
		 */ 
		errno = EBADF;
		ret = -1;
	} else if (_thread_fd_table[fd] == NULL) {
		/* First time for this fd, build an entry */
		entry = _thread_fd_entry();
		if (entry == NULL) {
			errno = ENOMEM;
			ret = -1;
		} else {
			entry->flags = _thread_sys_fcntl(fd, F_GETFL, 0);
			if (entry->flags == -1)
				/* use the errno fcntl returned */
				ret = -1;
			else {
				/*
				 * Make the file descriptor non-blocking.
				 * This might fail if the device driver does
				 * not support non-blocking calls, or if the
				 * driver is naturally non-blocking.
				 */
				if ((entry->flags & O_NONBLOCK) == 0) {
					saved_errno = errno;
					_thread_sys_fcntl(fd, F_SETFL,
						  entry->flags | O_NONBLOCK);
					errno = saved_errno;
				}

				/* Lock the file descriptor table: */
				_SPINLOCK(&fd_table_lock);

				/*
				 * Check if another thread allocated the
				 * file descriptor entry while this thread
				 * was doing the same thing. The table wasn't
				 * kept locked during this operation because
				 * it has the potential to recurse.
				 */
				if (_thread_fd_table[fd] == NULL) {
					/* This thread wins: */
					entry->refcnt += 1;
					_thread_fd_table[fd] = entry;
				}

				/* Unlock the file descriptor table: */
				_SPINUNLOCK(&fd_table_lock);
			}

			/*
			 * If there was an error in getting the flags for
			 * the file or if another thread initialized the
			 * table entry throw this entry away.
			 */
			if (entry->refcnt == 0)
				free(entry);
		}
	}

	/* Return the completion status: */
	return (ret);
}

/*
 * Dup from_fd -> to_fd.  from_fd is assumed to be locked (which
 * guarantees that _thread_fd_table[from_fd] exists).
 */
int
_thread_fd_table_dup(int from_fd, int to_fd)
{
	struct fd_table_entry	*entry;
	int ret;

	if (from_fd != to_fd) {
		/* release any existing to_fd table entry */
		entry = _thread_fd_table[to_fd];
		if (entry != NULL) {
			ret = _FD_LOCK(to_fd, FD_RDWR, NULL);
			if (ret != -1)
				_thread_fd_table_remove(to_fd);
		} else
			ret = 0;

		/* to_fd is a copy of from_fd */
		if (ret != -1) {
			_SPINLOCK(&fd_table_lock);
			_thread_fd_table[to_fd] = _thread_fd_table[from_fd];
			_thread_fd_table[to_fd]->refcnt += 1;
			_SPINUNLOCK(&fd_table_lock);
		}
	} else
		ret = 0;

	return (ret);
}

/*
 * Remove an fd entry from the table and free it if it's reference count
 * goes to zero.   The entry is assumed to be locked with a RDWR lock!  It
 * will be unlocked if it is not freed.
 */
void
_thread_fd_table_remove(int fd)
{
	struct fd_table_entry	*entry;

	_SPINLOCK(&fd_table_lock);
	entry = _thread_fd_table[fd];
	if (--entry->refcnt == 0)
		free(entry);
	else
		_FD_UNLOCK(fd, FD_RDWR);
	_thread_fd_table[fd] = NULL;
	_SPINUNLOCK(&fd_table_lock);
}

/*
 * Unlock the fd table entry for a given thread, fd, and lock type.
 */
void
_thread_fd_unlock_thread(struct pthread	*thread, int fd, int lock_type)
{
	struct fd_table_entry *entry;
	int	ret;

	/*
	 * Check that the file descriptor table is initialised for this
	 * entry: 
	 */
	ret = _thread_fd_table_init(fd);
	if (ret == 0) {
		entry = _thread_fd_table[fd];

		/*
		 * Defer signals to protect the scheduling queues from
		 * access by the signal handler:
		 */
		_thread_kern_sig_defer();

		/*
		 * Lock the file descriptor table entry to prevent
		 * other threads for clashing with the current
		 * thread's accesses:
		 */
		_SPINLOCK(&entry->lock);

		/* Check if the running thread owns the read lock: */
		if (entry->r_owner == thread &&
		    (lock_type == FD_READ || lock_type == FD_RDWR)) {
			/*
			 * Decrement the read lock count for the
			 * running thread: 
			 */
			entry->r_lockcount--;
			if (entry->r_lockcount == 0) {
				/*
				 * no read locks, dequeue any threads
				 * waiting for a read lock
				 */
				entry->r_owner = TAILQ_FIRST(&entry->r_queue);
				if (entry->r_owner != NULL) {
					TAILQ_REMOVE(&entry->r_queue,
						     entry->r_owner, qe);

					/*
					 * Set the state of the new owner of
					 * the thread to running:  
					 */
					PTHREAD_NEW_STATE(entry->r_owner,
							  PS_RUNNING);

					/*
					 * Reset the number of read locks.
					 * This will be incremented by the new
					 * owner of the lock when it sees that
					 *it has the lock.
					 */
					entry->r_lockcount = 0;
				}
			}

		}
		/* Check if the running thread owns the write lock: */
		if (entry->w_owner == thread &&
		    (lock_type == FD_WRITE || lock_type == FD_RDWR)) {
			/*
			 * Decrement the write lock count for the
			 * running thread: 
			 */
			entry->w_lockcount--;
			if (entry->w_lockcount == 0) {
				/*
				 * no write locks, dequeue any threads
				 * waiting on a write lock.
				 */
				entry->w_owner = TAILQ_FIRST(&entry->w_queue);
				if (entry->w_owner != NULL) {
					/* Remove this thread from the queue: */
					TAILQ_REMOVE(&entry->w_queue,
						     entry->w_owner, qe);

					/*
					 * Set the state of the new owner of
					 * the thread to running: 
					 */
					PTHREAD_NEW_STATE(entry->w_owner,
							  PS_RUNNING);

					/*
					 * Reset the number of write locks.
					 * This will be incremented by the
					 * new owner of the lock when it  
					 * sees that it has the lock.
					 */
					entry->w_lockcount = 0;
				}
			}
		}

		/* Unlock the file descriptor table entry: */
		_SPINUNLOCK(&entry->lock);

		/*
		 * Undefer and handle pending signals, yielding if
		 * necessary:
		 */
		_thread_kern_sig_undefer();
	}

	/* Nothing to return. */
	return;
}

/*
 * Unlock an fd table entry for the given fd and lock type.
 */
void
_thread_fd_unlock(int fd, int lock_type)
{
	struct pthread	*curthread = _get_curthread();
	_thread_fd_unlock_thread(curthread, fd, lock_type);
}

/*
 * Unlock all fd table entries owned by the given thread
 */
void
_thread_fd_unlock_owned(pthread_t pthread)
{
	struct fd_table_entry *entry;
	int do_unlock;
	int fd;

	for (fd = 0; fd < _thread_dtablesize; fd++) {
		entry = _thread_fd_table[fd];
		if (entry) {
			_SPINLOCK(&entry->lock);
			do_unlock = 0;
			/* force an unlock regardless of the recursion level */
			if (entry->r_owner == pthread) {
				entry->r_lockcount = 1;
				do_unlock++;
			}
			if (entry->w_owner == pthread) {
				entry->w_lockcount = 1;
				do_unlock++;
			}
			_SPINUNLOCK(&entry->lock);
			if (do_unlock)
				_thread_fd_unlock_thread(pthread, fd, FD_RDWR);
		}
	}
}

/*
 * Lock an fd table entry for the given fd and lock type.
 */
int
_thread_fd_lock(int fd, int lock_type, struct timespec * timeout)
{
	struct pthread	*curthread = _get_curthread();
	struct fd_table_entry *entry;
	int	ret;

	/*
	 * Check that the file descriptor table is initialised for this
	 * entry: 
	 */
	ret = _thread_fd_table_init(fd);
	if (ret == 0) {
		entry = _thread_fd_table[fd];

		/*
		 * Lock the file descriptor table entry to prevent
		 * other threads for clashing with the current
		 * thread's accesses:
		 */
		_SPINLOCK(&entry->lock);
		/* Handle read locks */
		if (lock_type == FD_READ || lock_type == FD_RDWR) {
			/*
			 * Enter a loop to wait for the file descriptor to be
			 * locked    for read for the current thread: 
			 */
			while (entry->r_owner != curthread) {
				/*
				 * Check if the file descriptor is locked by
				 * another thread: 
				 */
				if (entry->r_owner != NULL) {
					/*
					 * Another thread has locked the file
					 * descriptor for read, so join the
					 * queue of threads waiting for a  
					 * read lock on this file descriptor: 
					 */
					TAILQ_INSERT_TAIL(&entry->r_queue,
							  curthread, qe);

					/*
					 * Save the file descriptor details
					 * in the thread structure for the
					 * running thread: 
					 */
					curthread->data.fd.fd = fd;

					/* Set the timeout: */
					_thread_kern_set_timeout(timeout);

					/*
					 * Unlock the file descriptor
					 * table entry:
					 */
					_SPINUNLOCK(&entry->lock);

					/*
					 * Schedule this thread to wait on
					 * the read lock. It will only be
					 * woken when it becomes the next in
					 * the   queue and is granted access
					 * to the lock by the thread that is
					 * unlocking the file descriptor.
					 */
					_thread_kern_sched_state(PS_FDLR_WAIT,
								 __FILE__,
								 __LINE__);

					/*
					 * Lock the file descriptor
					 * table entry again:
					 */
					_SPINLOCK(&entry->lock);

				} else {
					/*
					 * The running thread now owns the
					 * read lock on this file descriptor: 
					 */
					entry->r_owner = curthread;

					/*
					 * Reset the number of read locks for
					 * this file descriptor: 
					 */
					entry->r_lockcount = 0;
				}
			}

			/* Increment the read lock count: */
			entry->r_lockcount++;
		}

		/* Handle write locks */
		if (lock_type == FD_WRITE || lock_type == FD_RDWR) {
			/*
			 * Enter a loop to wait for the file descriptor to be
			 * locked for write for the current thread: 
			 */
			while (entry->w_owner != curthread) {
				/*
				 * Check if the file descriptor is locked by
				 * another thread: 
				 */
				if (entry->w_owner != NULL) {
					/*
					 * Another thread has locked the file
					 * descriptor for write, so join the
					 * queue of threads waiting for a 
					 * write lock on this file
					 * descriptor: 
					 */
					TAILQ_INSERT_TAIL(&entry->w_queue,
							  curthread, qe);

					/*
					 * Save the file descriptor details
					 * in the thread structure for the
					 * running thread: 
					 */
					curthread->data.fd.fd = fd;

					/* Set the timeout: */
					_thread_kern_set_timeout(timeout);

					/*
					 * Unlock the file descriptor
					 * table entry:
					 */
					_SPINUNLOCK(&entry->lock);

					/*
					 * Schedule this thread to wait on
					 * the write lock. It will only be
					 * woken when it becomes the next in
					 * the queue and is granted access to
					 * the lock by the thread that is
					 * unlocking the file descriptor.
					 */
					_thread_kern_sched_state(PS_FDLW_WAIT,
								 __FILE__,
								 __LINE__);

					/*
					 * Lock the file descriptor
					 * table entry again:
					 */
					_SPINLOCK(&entry->lock);
				} else {
					/*
					 * The running thread now owns the
					 * write lock on this file descriptor: 
					 */
					entry->w_owner = curthread;

					/*
					 * Reset the number of write locks
					 * for this file descriptor: 
					 */
					entry->w_lockcount = 0;
				}
			}

			/* Increment the write lock count: */
			entry->w_lockcount++;
		}

		/* Unlock the file descriptor table entry: */
		_SPINUNLOCK(&entry->lock);
	}

	/* Return the completion status: */
	return (ret);
}

#endif
