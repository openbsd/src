/*	$NetBSD: uvm_meter.c,v 1.7 1998/08/09 22:36:39 perry Exp $	*/

/*
 * XXXCDC: "ROUGH DRAFT" QUALITY UVM PRE-RELEASE FILE!   
 *         >>>USE AT YOUR OWN RISK, WORK IS NOT FINISHED<<<
 */
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Charles D. Cranor,
 *      Washington University, and the University of California, Berkeley 
 *      and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <vm/vm.h>
#include <sys/sysctl.h>
#include <sys/exec.h>

/*
 * maxslp: ???? XXXCDC
 */

int maxslp = MAXSLP;	/* patchable ... */
struct loadavg averunnable; /* decl. */

/*
 * constants for averages over 1, 5, and 15 minutes when sampling at 
 * 5 second intervals.
 */

static fixpt_t cexp[3] = {
	0.9200444146293232 * FSCALE,	/* exp(-1/12) */
	0.9834714538216174 * FSCALE,	/* exp(-1/60) */
	0.9944598480048967 * FSCALE,	/* exp(-1/180) */
};

/*
 * prototypes
 */

static void uvm_loadav __P((struct loadavg *));

/*
 * uvm_meter: calculate load average and wake up the swapper (if needed)
 */
void
uvm_meter()
{
	if ((time.tv_sec % 5) == 0)
		uvm_loadav(&averunnable);
	if (proc0.p_slptime > (maxslp / 2))
		wakeup((caddr_t)&proc0);
}

/*
 * uvm_loadav: compute a tenex style load average of a quantity on 
 * 1, 5, and 15 minute internvals.
 */
static void
uvm_loadav(avg)
	struct loadavg *avg;
{
	int i, nrun;
	struct proc *p;

	for (nrun = 0, p = allproc.lh_first; p != 0; p = p->p_list.le_next) {
		switch (p->p_stat) {
		case SSLEEP:
			if (p->p_priority > PZERO || p->p_slptime > 1)
				continue;
		/* fall through */
		case SRUN:
		case SIDL:
			nrun++;
		}
	}
	for (i = 0; i < 3; i++)
		avg->ldavg[i] = (cexp[i] * avg->ldavg[i] +
		    nrun * FSCALE * (FSCALE - cexp[i])) >> FSHIFT;
}

/*
 * uvm_sysctl: sysctl hook into UVM system.
 */
int
uvm_sysctl(name, namelen, oldp, oldlenp, newp, newlen, p)
	int *name;
	u_int namelen;
	void *oldp;
	size_t *oldlenp;
	void *newp;
	size_t newlen;
	struct proc *p;
{
	struct vmtotal vmtotals;
	struct _ps_strings _ps = { PS_STRINGS };

	/* all sysctl names at this level are terminal */
	if (namelen != 1)
		return (ENOTDIR);		/* overloaded */

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

	case VM_PSSTRINGS:
		return (sysctl_rdstruct(oldp, oldlenp, newp, &_ps,
		    sizeof(_ps)));

	default:
		return (EOPNOTSUPP);
	}
	/* NOTREACHED */
}

/*
 * uvm_total: calculate the current state of the system.
 */
void
uvm_total(totalp)
	struct vmtotal *totalp;
{
	struct proc *p;
#if 0
	vm_map_entry_t	entry;
	vm_map_t map;
	int paging;
#endif

	bzero(totalp, sizeof *totalp);

	/*
	 * calculate process statistics
	 */

	for (p = allproc.lh_first; p != 0; p = p->p_list.le_next) {
		if (p->p_flag & P_SYSTEM)
			continue;
		switch (p->p_stat) {
		case 0:
			continue;

		case SSLEEP:
		case SSTOP:
			if (p->p_flag & P_INMEM) {
				if (p->p_priority <= PZERO)
					totalp->t_dw++;
				else if (p->p_slptime < maxslp)
					totalp->t_sl++;
			} else if (p->p_slptime < maxslp)
				totalp->t_sw++;
			if (p->p_slptime >= maxslp)
				continue;
			break;

		case SRUN:
		case SIDL:
			if (p->p_flag & P_INMEM)
				totalp->t_rq++;
			else
				totalp->t_sw++;
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
