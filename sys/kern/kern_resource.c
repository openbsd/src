/*	$OpenBSD: kern_resource.c,v 1.26 2003/12/11 23:02:30 millert Exp $	*/
/*	$NetBSD: kern_resource.c,v 1.38 1996/10/23 07:19:38 matthias Exp $	*/

/*-
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
 *	@(#)kern_resource.c	8.5 (Berkeley) 1/21/94
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/file.h>
#include <sys/resourcevar.h>
#include <sys/pool.h>
#include <sys/proc.h>

#include <sys/mount.h>
#include <sys/syscallargs.h>

#include <uvm/uvm_extern.h>

/*
 * Patchable maximum data and stack limits.
 */
rlim_t maxdmap = MAXDSIZ;
rlim_t maxsmap = MAXSSIZ;

/*
 * Resource controls and accounting.
 */

int
sys_getpriority(curp, v, retval)
	struct proc *curp;
	void *v;
	register_t *retval;
{
	register struct sys_getpriority_args /* {
		syscallarg(int) which;
		syscallarg(id_t) who;
	} */ *uap = v;
	register struct proc *p;
	register int low = NZERO + PRIO_MAX + 1;

	switch (SCARG(uap, which)) {

	case PRIO_PROCESS:
		if (SCARG(uap, who) == 0)
			p = curp;
		else
			p = pfind(SCARG(uap, who));
		if (p == 0)
			break;
		low = p->p_nice;
		break;

	case PRIO_PGRP: {
		register struct pgrp *pg;

		if (SCARG(uap, who) == 0)
			pg = curp->p_pgrp;
		else if ((pg = pgfind(SCARG(uap, who))) == NULL)
			break;
		for (p = pg->pg_members.lh_first; p != 0; p = p->p_pglist.le_next) {
			if (p->p_nice < low)
				low = p->p_nice;
		}
		break;
	}

	case PRIO_USER:
		if (SCARG(uap, who) == 0)
			SCARG(uap, who) = curp->p_ucred->cr_uid;
		for (p = LIST_FIRST(&allproc); p; p = LIST_NEXT(p, p_list))
			if (p->p_ucred->cr_uid == SCARG(uap, who) &&
			    p->p_nice < low)
				low = p->p_nice;
		break;

	default:
		return (EINVAL);
	}
	if (low == NZERO + PRIO_MAX + 1)
		return (ESRCH);
	*retval = low - NZERO;
	return (0);
}

/* ARGSUSED */
int
sys_setpriority(curp, v, retval)
	struct proc *curp;
	void *v;
	register_t *retval;
{
	register struct sys_setpriority_args /* {
		syscallarg(int) which;
		syscallarg(id_t) who;
		syscallarg(int) prio;
	} */ *uap = v;
	register struct proc *p;
	int found = 0, error = 0;

	switch (SCARG(uap, which)) {

	case PRIO_PROCESS:
		if (SCARG(uap, who) == 0)
			p = curp;
		else
			p = pfind(SCARG(uap, who));
		if (p == 0)
			break;
		error = donice(curp, p, SCARG(uap, prio));
		found++;
		break;

	case PRIO_PGRP: {
		register struct pgrp *pg;
		 
		if (SCARG(uap, who) == 0)
			pg = curp->p_pgrp;
		else if ((pg = pgfind(SCARG(uap, who))) == NULL)
			break;
		for (p = pg->pg_members.lh_first; p != 0;
		    p = p->p_pglist.le_next) {
			error = donice(curp, p, SCARG(uap, prio));
			found++;
		}
		break;
	}

	case PRIO_USER:
		if (SCARG(uap, who) == 0)
			SCARG(uap, who) = curp->p_ucred->cr_uid;
		for (p = LIST_FIRST(&allproc); p; p = LIST_NEXT(p, p_list))
			if (p->p_ucred->cr_uid == SCARG(uap, who)) {
				error = donice(curp, p, SCARG(uap, prio));
				found++;
			}
		break;

	default:
		return (EINVAL);
	}
	if (found == 0)
		return (ESRCH);
	return (error);
}

int
donice(curp, chgp, n)
	register struct proc *curp, *chgp;
	register int n;
{
	register struct pcred *pcred = curp->p_cred;

	if (pcred->pc_ucred->cr_uid && pcred->p_ruid &&
	    pcred->pc_ucred->cr_uid != chgp->p_ucred->cr_uid &&
	    pcred->p_ruid != chgp->p_ucred->cr_uid)
		return (EPERM);
	if (n > PRIO_MAX)
		n = PRIO_MAX;
	if (n < PRIO_MIN)
		n = PRIO_MIN;
	n += NZERO;
	if (n < chgp->p_nice && suser(curp, 0))
		return (EACCES);
	chgp->p_nice = n;
	(void)resetpriority(chgp);
	return (0);
}

/* ARGSUSED */
int
sys_setrlimit(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_setrlimit_args /* {
		syscallarg(int) which;
		syscallarg(const struct rlimit *) rlp;
	} */ *uap = v;
	struct rlimit alim;
	int error;

	error = copyin((caddr_t)SCARG(uap, rlp), (caddr_t)&alim,
		       sizeof (struct rlimit));
	if (error)
		return (error);
	return (dosetrlimit(p, SCARG(uap, which), &alim));
}

int
dosetrlimit(p, which, limp)
	struct proc *p;
	u_int which;
	struct rlimit *limp;
{
	struct rlimit *alimp;
	rlim_t maxlim;
	int error;

	if (which >= RLIM_NLIMITS)
		return (EINVAL);

	alimp = &p->p_rlimit[which];
	if (limp->rlim_cur > alimp->rlim_max ||
	    limp->rlim_max > alimp->rlim_max)
		if ((error = suser(p, 0)) != 0)
			return (error);
	if (p->p_limit->p_refcnt > 1 &&
	    (p->p_limit->p_lflags & PL_SHAREMOD) == 0) {
		p->p_limit->p_refcnt--;
		p->p_limit = limcopy(p->p_limit);
		alimp = &p->p_rlimit[which];
	}

	switch (which) {
	case RLIMIT_DATA:
		maxlim = maxdmap;
		break;
	case RLIMIT_STACK:
		maxlim = maxsmap;
		break;
	case RLIMIT_NOFILE:
		maxlim = maxfiles;
		break;
	case RLIMIT_NPROC:
		maxlim = maxproc;
		break;
	default:
		maxlim = RLIM_INFINITY;
		break;
	}

	if (limp->rlim_max > maxlim)
		limp->rlim_max = maxlim;
	if (limp->rlim_cur > limp->rlim_max)
		limp->rlim_cur = limp->rlim_max;

	if (which == RLIMIT_STACK) {
		/*
		 * Stack is allocated to the max at exec time with only
		 * "rlim_cur" bytes accessible.  If stack limit is going
		 * up make more accessible, if going down make inaccessible.
		 */
		if (limp->rlim_cur != alimp->rlim_cur) {
			vaddr_t addr;
			vsize_t size;
			vm_prot_t prot;

			if (limp->rlim_cur > alimp->rlim_cur) {
				prot = VM_PROT_READ|VM_PROT_WRITE;
				size = limp->rlim_cur - alimp->rlim_cur;
#ifdef MACHINE_STACK_GROWS_UP
				addr = USRSTACK + alimp->rlim_cur;
#else
				addr = USRSTACK - limp->rlim_cur;
#endif
			} else {
				prot = VM_PROT_NONE;
				size = alimp->rlim_cur - limp->rlim_cur;
#ifdef MACHINE_STACK_GROWS_UP
				addr = USRSTACK + limp->rlim_cur;
#else
				addr = USRSTACK - alimp->rlim_cur;
#endif
			}
			addr = trunc_page(addr);
			size = round_page(size);
			(void) uvm_map_protect(&p->p_vmspace->vm_map,
					      addr, addr+size, prot, FALSE);
		}
	}

	*alimp = *limp;
	return (0);
}

/* ARGSUSED */
int
sys_getrlimit(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_getrlimit_args /* {
		syscallarg(int) which;
		syscallarg(struct rlimit *) rlp;
	} */ *uap = v;

	if (SCARG(uap, which) < 0 || SCARG(uap, which) >= RLIM_NLIMITS)
		return (EINVAL);
	return (copyout((caddr_t)&p->p_rlimit[SCARG(uap, which)],
	    (caddr_t)SCARG(uap, rlp), sizeof (struct rlimit)));
}

/*
 * Transform the running time and tick information in proc p into user,
 * system, and interrupt time usage.
 */
void
calcru(p, up, sp, ip)
	struct proc *p;
	struct timeval *up;
	struct timeval *sp;
	struct timeval *ip;
{
	u_quad_t st, ut, it;
	int freq;
	int s;

	s = splstatclock();
	st = p->p_sticks;
	ut = p->p_uticks;
	it = p->p_iticks;
	splx(s);

	if (st + ut + it == 0) {
		timerclear(up);
		timerclear(sp);
		if (ip != NULL)
			timerclear(ip);
		return;
	}

	freq = stathz ? stathz : hz;

	st = st * 1000000 / freq;
	sp->tv_sec = st / 1000000;
	sp->tv_usec = st % 1000000;
	ut = ut * 1000000 / freq;
	up->tv_sec = ut / 1000000;
	up->tv_usec = ut % 1000000;
	if (ip != NULL) {
		it = it * 1000000 / freq;
		ip->tv_sec = it / 1000000;
		ip->tv_usec = it % 1000000;
	}
}

/* ARGSUSED */
int
sys_getrusage(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_getrusage_args /* {
		syscallarg(int) who;
		syscallarg(struct rusage *) rusage;
	} */ *uap = v;
	register struct rusage *rup;

	switch (SCARG(uap, who)) {

	case RUSAGE_SELF:
		rup = &p->p_stats->p_ru;
		calcru(p, &rup->ru_utime, &rup->ru_stime, NULL);
		break;

	case RUSAGE_CHILDREN:
		rup = &p->p_stats->p_cru;
		break;

	default:
		return (EINVAL);
	}
	return (copyout((caddr_t)rup, (caddr_t)SCARG(uap, rusage),
	    sizeof (struct rusage)));
}

void
ruadd(ru, ru2)
	register struct rusage *ru, *ru2;
{
	register long *ip, *ip2;
	register int i;

	timeradd(&ru->ru_utime, &ru2->ru_utime, &ru->ru_utime);
	timeradd(&ru->ru_stime, &ru2->ru_stime, &ru->ru_stime);
	if (ru->ru_maxrss < ru2->ru_maxrss)
		ru->ru_maxrss = ru2->ru_maxrss;
	ip = &ru->ru_first; ip2 = &ru2->ru_first;
	for (i = &ru->ru_last - &ru->ru_first; i >= 0; i--)
		*ip++ += *ip2++;
}

struct pool plimit_pool;

/*
 * Make a copy of the plimit structure.
 * We share these structures copy-on-write after fork,
 * and copy when a limit is changed.
 */
struct plimit *
limcopy(struct plimit *lim)
{
	struct plimit *newlim;
	static int initialized;

	if (!initialized) {
		pool_init(&plimit_pool, sizeof(struct plimit), 0, 0, 0,
		    "plimitpl", &pool_allocator_nointr);
		initialized = 1;
	}

	newlim = pool_get(&plimit_pool, PR_WAITOK);
	bcopy(lim->pl_rlimit, newlim->pl_rlimit,
	    sizeof(struct rlimit) * RLIM_NLIMITS);
	newlim->p_lflags = 0;
	newlim->p_refcnt = 1;
	return (newlim);
}

void
limfree(struct plimit *lim)
{
	if (--lim->p_refcnt > 0)
		return;
	pool_put(&plimit_pool, lim);
}
