/*	$OpenBSD: kern_subr.c,v 1.29 2004/11/28 02:11:33 deraadt Exp $	*/
/*	$NetBSD: kern_subr.c,v 1.15 1996/04/09 17:21:56 ragge Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 *	@(#)kern_subr.c	8.3 (Berkeley) 1/21/94
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/sched.h>
#include <sys/malloc.h>
#include <sys/queue.h>
#include <sys/kernel.h>
#include <sys/resourcevar.h>

int
uiomove(cp, n, uio)
	register void *cp;
	register int n;
	register struct uio *uio;
{
	register struct iovec *iov;
	u_int cnt;
	int error = 0;
	struct proc *p;

	p = uio->uio_procp;

#ifdef DIAGNOSTIC
	if (uio->uio_rw != UIO_READ && uio->uio_rw != UIO_WRITE)
		panic("uiomove: mode");
	if (uio->uio_segflg == UIO_USERSPACE && p != curproc)
		panic("uiomove: proc");
#endif
	while (n > 0 && uio->uio_resid) {
		iov = uio->uio_iov;
		cnt = iov->iov_len;
		if (cnt == 0) {
			uio->uio_iov++;
			uio->uio_iovcnt--;
			continue;
		}
		if (cnt > n)
			cnt = n;
		switch (uio->uio_segflg) {

		case UIO_USERSPACE:
#ifdef __HAVE_CPUINFO
			if (curcpu()->ci_schedstate.spc_schedflags &
			    SPCF_SHOULDYIELD)
#else
			if (p->p_schedflags & PSCHED_SHOULDYIELD)
#endif
				preempt(NULL);
			if (uio->uio_rw == UIO_READ)
				error = copyout(cp, iov->iov_base, cnt);
			else
				error = copyin(iov->iov_base, cp, cnt);
			if (error)
				return (error);
			break;

		case UIO_SYSSPACE:
			if (uio->uio_rw == UIO_READ)
				error = kcopy(cp, iov->iov_base, cnt);
			else
				error = kcopy(iov->iov_base, cp, cnt);
			if (error)
				return(error);
		}
		iov->iov_base = (caddr_t)iov->iov_base + cnt;
		iov->iov_len -= cnt;
		uio->uio_resid -= cnt;
		uio->uio_offset += cnt;
		cp = (caddr_t)cp + cnt;
		n -= cnt;
	}
	return (error);
}

/*
 * Give next character to user as result of read.
 */
int
ureadc(c, uio)
	register int c;
	register struct uio *uio;
{
	register struct iovec *iov;

	if (uio->uio_resid == 0)
#ifdef DIAGNOSTIC
		panic("ureadc: zero resid");
#else
		return (EINVAL);
#endif
again:
	if (uio->uio_iovcnt <= 0)
#ifdef DIAGNOSTIC
		panic("ureadc: non-positive iovcnt");
#else
		return (EINVAL);
#endif
	iov = uio->uio_iov;
	if (iov->iov_len <= 0) {
		uio->uio_iovcnt--;
		uio->uio_iov++;
		goto again;
	}
	switch (uio->uio_segflg) {

	case UIO_USERSPACE:
	{
		char tmp = c;

		if (copyout(&tmp, iov->iov_base, sizeof(char)) != 0)
			return (EFAULT);
	}
		break;

	case UIO_SYSSPACE:
		*(char *)iov->iov_base = c;
		break;
	}
	iov->iov_base = (caddr_t)iov->iov_base + 1;
	iov->iov_len--;
	uio->uio_resid--;
	uio->uio_offset++;
	return (0);
}

/*
 * General routine to allocate a hash table.
 */
void *
hashinit(elements, type, flags, hashmask)
	int elements, type, flags;
	u_long *hashmask;
{
	u_long hashsize, i;
	LIST_HEAD(generic, generic) *hashtbl;

	if (elements <= 0)
		panic("hashinit: bad cnt");
	for (hashsize = 1; hashsize < elements; hashsize <<= 1)
		continue;
	hashtbl = malloc(hashsize * sizeof(*hashtbl), type, flags);
	if (hashtbl == NULL)
		return NULL;
	for (i = 0; i < hashsize; i++)
		LIST_INIT(&hashtbl[i]);
	*hashmask = hashsize - 1;
	return (hashtbl);
}

/*
 * "Shutdown/startup hook" types, functions, and variables.
 */

struct hook_desc_head startuphook_list =
    TAILQ_HEAD_INITIALIZER(startuphook_list);
struct hook_desc_head shutdownhook_list =
    TAILQ_HEAD_INITIALIZER(shutdownhook_list);
struct hook_desc_head mountroothook_list =
    TAILQ_HEAD_INITIALIZER(mountroothook_list);

void *
hook_establish(head, tail, fn, arg)
	struct hook_desc_head *head;
	int tail;
	void (*fn)(void *);
	void *arg;
{
	struct hook_desc *hdp;

	hdp = (struct hook_desc *)malloc(sizeof (*hdp), M_DEVBUF, M_NOWAIT);
	if (hdp == NULL)
		return (NULL);

	hdp->hd_fn = fn;
	hdp->hd_arg = arg;
	if (tail)
		TAILQ_INSERT_TAIL(head, hdp, hd_list);
	else
		TAILQ_INSERT_HEAD(head, hdp, hd_list);

	return (hdp);
}

void
hook_disestablish(head, vhook)
	struct hook_desc_head *head;
	void *vhook;
{
	struct hook_desc *hdp;

#ifdef DIAGNOSTIC
	for (hdp = TAILQ_FIRST(head); hdp != NULL;
	    hdp = TAILQ_NEXT(hdp, hd_list))
                if (hdp == vhook)
			break;
	if (hdp == NULL)
		panic("hook_disestablish: hook not established");
#endif
	hdp = vhook;
	TAILQ_REMOVE(head, hdp, hd_list);
	free(hdp, M_DEVBUF);
}

/*
 * Run hooks.  Startup hooks are invoked right after scheduler_start but
 * before root is mounted.  Shutdown hooks are invoked immediately before the
 * system is halted or rebooted, i.e. after file systems unmounted,
 * after crash dump done, etc.
 */
void
dohooks(struct hook_desc_head *head, int flags)
{
	struct hook_desc *hdp;

	if ((flags & HOOK_REMOVE) == 0) {
		TAILQ_FOREACH(hdp, head, hd_list) {
			(*hdp->hd_fn)(hdp->hd_arg);
		}
	} else {
		while ((hdp = TAILQ_FIRST(head)) != NULL) {
			TAILQ_REMOVE(head, hdp, hd_list);
			(*hdp->hd_fn)(hdp->hd_arg);
			if ((flags & HOOK_FREE) != 0)
				free(hdp, M_DEVBUF);
		}
	}
}

/*
 * "Power hook" types, functions, and variables.
 */

struct powerhook_desc {
	CIRCLEQ_ENTRY(powerhook_desc) sfd_list;
	void	(*sfd_fn)(int, void *);
	void	*sfd_arg;
};

CIRCLEQ_HEAD(, powerhook_desc) powerhook_list =
	CIRCLEQ_HEAD_INITIALIZER(powerhook_list);

void *
powerhook_establish(fn, arg)
	void (*fn)(int, void *);
	void *arg;
{
	struct powerhook_desc *ndp;

	ndp = (struct powerhook_desc *)
	    malloc(sizeof(*ndp), M_DEVBUF, M_NOWAIT);
	if (ndp == NULL)
		return NULL;

	ndp->sfd_fn = fn;
	ndp->sfd_arg = arg;
	CIRCLEQ_INSERT_HEAD(&powerhook_list, ndp, sfd_list);

	return (ndp);
}

void
powerhook_disestablish(vhook)
	void *vhook;
{
#ifdef DIAGNOSTIC
	struct powerhook_desc *dp;

	CIRCLEQ_FOREACH(dp, &powerhook_list, sfd_list)
                if (dp == vhook)
			break;
	if (dp == NULL)
		panic("powerhook_disestablish: hook not established");
#endif

	CIRCLEQ_REMOVE(&powerhook_list, (struct powerhook_desc *)vhook,
		sfd_list);
	free(vhook, M_DEVBUF);
}

/*
 * Run power hooks.
 */
void
dopowerhooks(why)
	int why;
{
	struct powerhook_desc *dp;
	int s;

	s = splhigh();
	if (why == PWR_RESUME) {
		CIRCLEQ_FOREACH_REVERSE(dp, &powerhook_list, sfd_list) {
			(*dp->sfd_fn)(why, dp->sfd_arg);
		}
	} else {
		CIRCLEQ_FOREACH(dp, &powerhook_list, sfd_list) {
			(*dp->sfd_fn)(why, dp->sfd_arg);
		}
	}
	splx(s);
}
