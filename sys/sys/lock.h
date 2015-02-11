/*	$OpenBSD: lock.h,v 1.25 2015/02/11 07:22:15 dlg Exp $	*/

/* 
 * Copyright (c) 1995
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code contains ideas from software contributed to Berkeley by
 * Avadis Tevanian, Jr., Michael Wayne Young, and the Mach Operating
 * System project at Carnegie-Mellon University.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)lock.h	8.12 (Berkeley) 5/19/95
 */

#ifndef	_LOCK_H_
#define	_LOCK_H_

#include <sys/rwlock.h>

struct lock {
	struct rrwlock	lk_lck;
};

#define LK_SHARED	0x01	/* shared lock */
#define LK_EXCLUSIVE	0x02	/* exclusive lock */
#define LK_TYPE_MASK	0x03	/* type of lock sought */
#define LK_DRAIN	0x04	/* wait for all lock activity to end */
#define LK_RELEASE	0x08	/* release any type of lock */
#define LK_NOWAIT	0x10	/* do not sleep to await lock */
#define LK_CANRECURSE	0x20	/* allow recursive exclusive lock */
#define LK_RECURSEFAIL	0x40	/* fail if recursive exclusive lock */
#define LK_RETRY	0x80	/* vn_lock: retry until locked */

/* for lockstatus() only */
#define LK_EXCLOTHER	0x100	/* exclusive lock held by some other thread */

void	lockinit(struct lock *, int, char *, int, int);
int	lockmgr(struct lock *, u_int flags, void *);
int	lockstatus(struct lock *);

#define	lockmgr_printinfo(lkp)

#endif /* !_LOCK_H_ */
