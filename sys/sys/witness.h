/*	$OpenBSD: witness.h,v 1.1 2017/04/20 12:59:36 visa Exp $	*/

/*-
 * Copyright (c) 1997 Berkeley Software Design, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Berkeley Software Design Inc's name may not be used to endorse or
 *    promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BERKELEY SOFTWARE DESIGN INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL BERKELEY SOFTWARE DESIGN INC BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from BSDI Id: mutex.h,v 2.7.2.35 2000/04/27 03:10:26 cp
 * $FreeBSD: head/sys/sys/lock.h 313908 2017-02-18 01:52:10Z mjg $
 */

#ifndef _SYS_WITNESS_H_
#define _SYS_WITNESS_H_

#include <sys/_lock.h>

/*
 * Lock classes are statically assigned an index into the global lock_classes
 * array.  Debugging code looks up the lock class for a given lock object
 * by indexing the array.
 */
#define	LO_CLASSINDEX(lock) \
	((((lock)->lo_flags) & LO_CLASSMASK) >> LO_CLASSSHIFT)
#define	LOCK_CLASS(lock) \
	(lock_classes[LO_CLASSINDEX((lock))])
#define	LOCK_CLASS_MAX \
	(LO_CLASSMASK >> LO_CLASSSHIFT)

/*
 * Option flags passed to lock operations that witness also needs to know
 * about or that are generic across all locks.
 */
#define	LOP_NEWORDER	0x00000001	/* Define a new lock order. */
#define	LOP_QUIET	0x00000002	/* Don't log locking operations. */
#define	LOP_TRYLOCK	0x00000004	/* Don't check lock order. */
#define	LOP_EXCLUSIVE	0x00000008	/* Exclusive lock. */
#define	LOP_DUPOK	0x00000010	/* Don't check for duplicate acquires */

/* Flags passed to witness_assert. */
#define	LA_MASKASSERT	0x000000ff	/* Mask for witness defined asserts. */
#define	LA_UNLOCKED	0x00000000	/* Lock is unlocked. */
#define	LA_LOCKED	0x00000001	/* Lock is at least share locked. */
#define	LA_SLOCKED	0x00000002	/* Lock is exactly share locked. */
#define	LA_XLOCKED	0x00000004	/* Lock is exclusively locked. */
#define	LA_RECURSED	0x00000008	/* Lock is recursed. */
#define	LA_NOTRECURSED	0x00000010	/* Lock is not recursed. */

#ifdef _KERNEL

void	witness_initialize(void);
void	witness_init(struct lock_object *, struct lock_type *);
int	witness_defineorder(struct lock_object *, struct lock_object *);
void	witness_checkorder(struct lock_object *, int, const char *, int,
	    struct lock_object *);
void	witness_lock(struct lock_object *, int, const char *, int);
void	witness_upgrade(struct lock_object *, int, const char *, int);
void	witness_downgrade(struct lock_object *, int, const char *, int);
void	witness_unlock(struct lock_object *, int, const char *, int);
void	witness_save(struct lock_object *, const char **, int *);
void	witness_restore(struct lock_object *, const char *, int);
int	witness_warn(int, struct lock_object *, const char *, ...);
void	witness_assert(const struct lock_object *, int, const char *, int);
void	witness_display_spinlock(struct lock_object *, struct proc *,
	    int (*)(const char *, ...));
int	witness_line(struct lock_object *);
void	witness_norelease(struct lock_object *);
void	witness_releaseok(struct lock_object *);
const char *witness_file(struct lock_object *);
void	witness_thread_exit(struct proc *);

#ifdef	WITNESS

/* Flags for witness_warn(). */
#define	WARN_KERNELOK	0x01	/* Kernel lock is exempt from this check. */
#define	WARN_PANIC	0x02	/* Panic if check fails. */
#define	WARN_SLEEPOK	0x04	/* Sleepable locks are exempt from check. */

#define	WITNESS_INITIALIZE()						\
	witness_initialize()

#define	WITNESS_INIT(lock, type)					\
	witness_init((lock), (type))

#define	WITNESS_CHECKORDER(lock, flags, file, line, interlock)		\
	witness_checkorder((lock), (flags), (file), (line), (interlock))

#define	WITNESS_DEFINEORDER(lock1, lock2)				\
	witness_defineorder((struct lock_object *)(lock1),		\
	    (struct lock_object *)(lock2))

#define	WITNESS_LOCK(lock, flags, file, line)				\
	witness_lock((lock), (flags), (file), (line))

#define	WITNESS_UPGRADE(lock, flags, file, line)			\
	witness_upgrade((lock), (flags), (file), (line))

#define	WITNESS_DOWNGRADE(lock, flags, file, line)			\
	witness_downgrade((lock), (flags), (file), (line))

#define	WITNESS_UNLOCK(lock, flags, file, line)				\
	witness_unlock((lock), (flags), (file), (line))

#define	WITNESS_CHECK(flags, lock, fmt, ...)				\
	witness_warn((flags), (lock), (fmt), ## __VA_ARGS__)

#define	WITNESS_WARN(flags, lock, fmt, ...)				\
	witness_warn((flags), (lock), (fmt), ## __VA_ARGS__)

#define	WITNESS_SAVE_DECL(n)						\
	const char * __CONCAT(n, __wf);					\
	int __CONCAT(n, __wl)

#define	WITNESS_SAVE(lock, n) 						\
	witness_save((lock), &__CONCAT(n, __wf), &__CONCAT(n, __wl))

#define	WITNESS_RESTORE(lock, n) 					\
	witness_restore((lock), __CONCAT(n, __wf), __CONCAT(n, __wl))

#define	WITNESS_NORELEASE(lock)						\
	witness_norelease(&(lock)->lock_object)

#define	WITNESS_RELEASEOK(lock)						\
	witness_releaseok(&(lock)->lock_object)

#define	WITNESS_FILE(lock) 						\
	witness_file(lock)

#define	WITNESS_LINE(lock) 						\
	witness_line(lock)

#define	WITNESS_THREAD_EXIT(p)						\
	witness_thread_exit((p))

#else	/* WITNESS */
#define	WITNESS_INITIALIZE()					(void)0
#define	WITNESS_INIT(lock, type)				(void)0
#define	WITNESS_DEFINEORDER(lock1, lock2)	0
#define	WITNESS_CHECKORDER(lock, flags, file, line, interlock)	(void)0
#define	WITNESS_LOCK(lock, flags, file, line)			(void)0
#define	WITNESS_UPGRADE(lock, flags, file, line)		(void)0
#define	WITNESS_DOWNGRADE(lock, flags, file, line)		(void)0
#define	WITNESS_UNLOCK(lock, flags, file, line)			(void)0
#define	WITNESS_CHECK(flags, lock, fmt, ...)	0
#define	WITNESS_WARN(flags, lock, fmt, ...)			(void)0
#define	WITNESS_SAVE_DECL(n)					(void)0
#define	WITNESS_SAVE(lock, n)					(void)0
#define	WITNESS_RESTORE(lock, n)				(void)0
#define	WITNESS_NORELEASE(lock)					(void)0
#define	WITNESS_RELEASEOK(lock)					(void)0
#define	WITNESS_THREAD_EXIT(p)					(void)0
#define	WITNESS_FILE(lock) ("?")
#define	WITNESS_LINE(lock) (0)
#endif	/* WITNESS */

#endif	/* _KERNEL */
#endif	/* _SYS_WITNESS_H_ */
