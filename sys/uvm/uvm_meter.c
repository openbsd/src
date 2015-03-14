/*	$OpenBSD: uvm_meter.c,v 1.36 2015/03/14 03:38:53 jsg Exp $	*/
/*	$NetBSD: uvm_meter.c,v 1.21 2001/07/14 06:36:03 matt Exp $	*/

/*
 * Copyright (c) 1997 Charles D. Cranor and Washington University.
 * Copyright (c) 1982, 1986, 1989, 1993
 *      The Regents of the University of California.
 *
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
 *      @(#)vm_meter.c  8.4 (Berkeley) 1/4/94
 * from: Id: uvm_meter.c,v 1.1.2.1 1997/08/14 19:10:35 chuck Exp
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#include <sys/vmmeter.h>
#include <uvm/uvm.h>

#ifdef UVM_SWAP_ENCRYPT
#include <uvm/uvm_swap.h>
#include <uvm/uvm_swap_encrypt.h>
#endif

/*
 * The time for a process to be blocked before being very swappable.
 * This is a number of seconds which the system takes as being a non-trivial
 * amount of real time.  You probably shouldn't change this;
 * it is used in subtle ways (fractions and multiples of it are, that is, like
 * half of a ``long time'', almost a long time, etc.)
 * It is related to human patience and other factors which don't really
 * change over time.
 */
#define	MAXSLP	20

int maxslp = MAXSLP;	/* patchable ... */
struct loadavg averunnable;

/*
 * constants for averages over 1, 5, and 15 minutes when sampling at
 * 5 second intervals.
 */

static fixpt_t cexp[3] = {
	0.9200444146293232 * FSCALE,	/* exp(-1/12) */
	0.9834714538216174 * FSCALE,	/* exp(-1/60) */
	0.9944598480048967 * FSCALE,	/* exp(-1/180) */
};


static void uvm_loadav(struct loadavg *);
void uvm_total(struct vmtotal *);

/*
 * uvm_meter: calculate load average and wake up the swapper (if needed)
 */
void
uvm_meter(void)
{
	if ((time_second % 5) == 0)
		uvm_loadav(&averunnable);
	if (proc0.p_slptime > (maxslp / 2))
		wakeup(&proc0);
}

/*
 * uvm_loadav: compute a tenex style load average of a quantity on
 * 1, 5, and 15 minute intervals.
 */
static void
uvm_loadav(struct loadavg *avg)
{
	CPU_INFO_ITERATOR cii;
	struct cpu_info *ci;
	int i, nrun;
	struct proc *p;
	int nrun_cpu[MAXCPUS];

	nrun = 0;
	memset(nrun_cpu, 0, sizeof(nrun_cpu));

	LIST_FOREACH(p, &allproc, p_list) {
		switch (p->p_stat) {
		case SSLEEP:
			if (p->p_priority > PZERO || p->p_slptime > 1)
				continue;
		/* FALLTHROUGH */
		case SRUN:
		case SONPROC:
			if (p == p->p_cpu->ci_schedstate.spc_idleproc)
				continue;
		case SIDL:
			nrun++;
			if (p->p_cpu)
				nrun_cpu[CPU_INFO_UNIT(p->p_cpu)]++;
		}
	}

	for (i = 0; i < 3; i++) {
		avg->ldavg[i] = (cexp[i] * avg->ldavg[i] +
		    nrun * FSCALE * (FSCALE - cexp[i])) >> FSHIFT;
	}

	CPU_INFO_FOREACH(cii, ci) {
		struct schedstate_percpu *spc = &ci->ci_schedstate;

		if (nrun_cpu[CPU_INFO_UNIT(ci)] == 0)
			continue;
		spc->spc_ldavg = (cexp[0] * spc->spc_ldavg +
		    nrun_cpu[CPU_INFO_UNIT(ci)] * FSCALE *
		    (FSCALE - cexp[0])) >> FSHIFT;
	}		
}

/*
 * uvm_sysctl: sysctl hook into UVM system.
 */
int
uvm_sysctl(int *name, u_int namelen, void *oldp, size_t *oldlenp, void *newp,
    size_t newlen, struct proc *p)
{
	struct process *pr = p->p_p;
	struct vmtotal vmtotals;
	int rv, t;

	switch (name[0]) {
	case VM_SWAPENCRYPT:
#ifdef UVM_SWAP_ENCRYPT
		return (swap_encrypt_ctl(name + 1, namelen - 1, oldp, oldlenp,
					 newp, newlen, p));
#else
		return (EOPNOTSUPP);
#endif
	default:
		/* all sysctl names at this level are terminal */
		if (namelen != 1)
			return (ENOTDIR);		/* overloaded */
		break;
	}

	switch (name[0]) {
	case VM_LOADAVG:
		return (sysctl_rdstruct(oldp, oldlenp, newp, &averunnable,
		    sizeof(averunnable)));

	case VM_METER:
		uvm_total(&vmtotals);
		return (sysctl_rdstruct(oldp, oldlenp, newp, &vmtotals,
		    sizeof(vmtotals)));

	case VM_UVMEXP:
		return (sysctl_rdstruct(oldp, oldlenp, newp, &uvmexp,
		    sizeof(uvmexp)));

	case VM_NKMEMPAGES:
		return (sysctl_rdint(oldp, oldlenp, newp, nkmempages));

	case VM_PSSTRINGS:
		return (sysctl_rdstruct(oldp, oldlenp, newp, &pr->ps_strings,
		    sizeof(pr->ps_strings)));

	case VM_ANONMIN:
		t = uvmexp.anonminpct;
		rv = sysctl_int(oldp, oldlenp, newp, newlen, &t);
		if (rv) {
			return rv;
		}
		if (t + uvmexp.vtextminpct + uvmexp.vnodeminpct > 95 || t < 0) {
			return EINVAL;
		}
		uvmexp.anonminpct = t;
		uvmexp.anonmin = t * 256 / 100;
		return rv;

	case VM_VTEXTMIN:
		t = uvmexp.vtextminpct;
		rv = sysctl_int(oldp, oldlenp, newp, newlen, &t);
		if (rv) {
			return rv;
		}
		if (uvmexp.anonminpct + t + uvmexp.vnodeminpct > 95 || t < 0) {
			return EINVAL;
		}
		uvmexp.vtextminpct = t;
		uvmexp.vtextmin = t * 256 / 100;
		return rv;

	case VM_VNODEMIN:
		t = uvmexp.vnodeminpct;
		rv = sysctl_int(oldp, oldlenp, newp, newlen, &t);
		if (rv) {
			return rv;
		}
		if (uvmexp.anonminpct + uvmexp.vtextminpct + t > 95 || t < 0) {
			return EINVAL;
		}
		uvmexp.vnodeminpct = t;
		uvmexp.vnodemin = t * 256 / 100;
		return rv;

	case VM_MAXSLP:
		return (sysctl_rdint(oldp, oldlenp, newp, maxslp));

	case VM_USPACE:
		return (sysctl_rdint(oldp, oldlenp, newp, USPACE));

	default:
		return (EOPNOTSUPP);
	}
	/* NOTREACHED */
}

/*
 * uvm_total: calculate the current state of the system.
 */
void
uvm_total(struct vmtotal *totalp)
{
	struct proc *p;
#if 0
	struct vm_map_entry *	entry;
	struct vm_map *map;
	int paging;
#endif

	memset(totalp, 0, sizeof *totalp);

	/* calculate process statistics */
	LIST_FOREACH(p, &allproc, p_list) {
		if (p->p_flag & P_SYSTEM)
			continue;
		switch (p->p_stat) {
		case 0:
			continue;

		case SSLEEP:
		case SSTOP:
			if (p->p_priority <= PZERO)
				totalp->t_dw++;
			else if (p->p_slptime < maxslp)
				totalp->t_sl++;
			if (p->p_slptime >= maxslp)
				continue;
			break;
		case SRUN:
		case SIDL:
		case SONPROC:
			totalp->t_rq++;
			if (p->p_stat == SIDL)
				continue;
			break;
		}
		/*
		 * note active objects
		 */
#if 0
		/*
		 * XXXCDC: BOGUS!  rethink this.   in the mean time
		 * don't do it.
		 */
		paging = 0;
		vm_map_lock(map);
		for (map = &p->p_vmspace->vm_map, entry = map->header.next;
		    entry != &map->header; entry = entry->next) {
			if (entry->is_a_map || entry->is_sub_map ||
			    entry->object.uvm_obj == NULL)
				continue;
			/* XXX how to do this with uvm */
		}
		vm_map_unlock(map);
		if (paging)
			totalp->t_pw++;
#endif
	}
	/*
	 * Calculate object memory usage statistics.
	 */
	totalp->t_free = uvmexp.free;
	totalp->t_vm = uvmexp.npages - uvmexp.free + uvmexp.swpginuse;
	totalp->t_avm = uvmexp.active + uvmexp.swpginuse;	/* XXX */
	totalp->t_rm = uvmexp.npages - uvmexp.free;
	totalp->t_arm = uvmexp.active;
	totalp->t_vmshr = 0;		/* XXX */
	totalp->t_avmshr = 0;		/* XXX */
	totalp->t_rmshr = 0;		/* XXX */
	totalp->t_armshr = 0;		/* XXX */
}
