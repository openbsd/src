/*	$OpenBSD: vm_glue.c,v 1.31 1999/02/19 19:21:40 art Exp $    */
/*	$NetBSD: vm_glue.c,v 1.55.4.1 1996/06/13 17:25:45 cgd Exp $	*/

/* 
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * The Mach Operating System project at Carnegie-Mellon University.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 *	@(#)vm_glue.c	8.9 (Berkeley) 3/4/95
 *
 *
 * Copyright (c) 1987, 1990 Carnegie-Mellon University.
 * All rights reserved.
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/buf.h>
#include <sys/user.h>
#ifdef SYSVSHM
#include <sys/shm.h>
#endif

#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_page.h>
#include <vm/vm_kern.h>

#include <machine/cpu.h>

int	avefree = 0;		/* XXX */
unsigned maxdmap = MAXDSIZ;	/* XXX */ 
unsigned maxsmap = MAXSSIZ;	/* XXX */ 
int	readbuffers = 0;	/* XXX allow kgdb to read kernel buffer pool */

int
kernacc(addr, len, rw)
	caddr_t addr;
	int len, rw;
{
	boolean_t rv;
	vm_offset_t saddr, eaddr;
	vm_prot_t prot = rw == B_READ ? VM_PROT_READ : VM_PROT_WRITE;

	saddr = trunc_page(addr);
	eaddr = round_page(addr+len);
	rv = vm_map_check_protection(kernel_map, saddr, eaddr, prot);
	/*
	 * XXX there are still some things (e.g. the buffer cache) that
	 * are managed behind the VM system's back so even though an
	 * address is accessible in the mind of the VM system, there may
	 * not be physical pages where the VM thinks there is.  This can
	 * lead to bogus allocation of pages in the kernel address space
	 * or worse, inconsistencies at the pmap level.  We only worry
	 * about the buffer cache for now.
	 */
	if (!readbuffers && rv && (eaddr > (vm_offset_t)buffers &&
	    saddr < (vm_offset_t)buffers + MAXBSIZE * nbuf))
		rv = FALSE;
	return (rv == TRUE);
}

int
useracc(addr, len, rw)
	caddr_t addr;
	int len, rw;
{
	boolean_t rv;
	vm_prot_t prot = rw == B_READ ? VM_PROT_READ : VM_PROT_WRITE;

#if defined(i386) || defined(pc532)
	/*
	 * XXX - specially disallow access to user page tables - they are
	 * in the map.  This is here until i386 & pc532 pmaps are fixed...
	 */
	if ((vm_offset_t) addr >= VM_MAXUSER_ADDRESS
	    || (vm_offset_t) addr + len > VM_MAXUSER_ADDRESS
	    || (vm_offset_t) addr + len <= (vm_offset_t) addr)
		return (FALSE);
#endif

	rv = vm_map_check_protection(&curproc->p_vmspace->vm_map,
	    trunc_page(addr), round_page(addr+len), prot);
	return (rv == TRUE);
}

#ifdef KGDB
/*
 * Change protections on kernel pages from addr to addr+len
 * (presumably so debugger can plant a breakpoint).
 *
 * We force the protection change at the pmap level.  If we were
 * to use vm_map_protect a change to allow writing would be lazily-
 * applied meaning we would still take a protection fault, something
 * we really don't want to do.  It would also fragment the kernel
 * map unnecessarily.  We cannot use pmap_protect since it also won't
 * enforce a write-enable request.  Using pmap_enter is the only way
 * we can ensure the change takes place properly.
 */
void
chgkprot(addr, len, rw)
	register caddr_t addr;
	int len, rw;
{
	vm_prot_t prot;
	vm_offset_t pa, sva, eva;

	prot = rw == B_READ ? VM_PROT_READ : VM_PROT_READ|VM_PROT_WRITE;
	eva = round_page(addr + len);
	for (sva = trunc_page(addr); sva < eva; sva += PAGE_SIZE) {
		/*
		 * Extract physical address for the page.
		 * We use a cheezy hack to differentiate physical
		 * page 0 from an invalid mapping, not that it
		 * really matters...
		 */
		pa = pmap_extract(pmap_kernel(), sva|1);
		if (pa == 0)
			panic("chgkprot: invalid page");
		pmap_enter(pmap_kernel(), sva, pa&~1, prot, TRUE);
	}
}
#endif

int
vslock(addr, len)
	caddr_t	addr;
	u_int	len;
{
#ifdef __i386__
	pmap_prefault(&curproc->p_vmspace->vm_map, (vm_offset_t)addr, len);
#endif
	return (vm_map_pageable(&curproc->p_vmspace->vm_map, trunc_page(addr),
	    round_page(addr+len), FALSE));
}

int
vsunlock(addr, len)
	caddr_t	addr;
	u_int	len;
{
	return (vm_map_pageable(&curproc->p_vmspace->vm_map, trunc_page(addr),
	    round_page(addr+len), TRUE));
}

/*
 * Implement fork's actions on an address space.
 * Here we arrange for the address space to be copied or referenced,
 * allocate a user struct (pcb and kernel stack), then call the
 * machine-dependent layer to fill those in and make the new process
 * ready to run.
 * NOTE: the kernel stack may be at a different location in the child
 * process, and thus addresses of automatic variables may be invalid
 * after cpu_fork returns in the child process.  We do nothing here
 * after cpu_fork returns.
 */
#ifdef __FORK_BRAINDAMAGE
int
#else
void
#endif
vm_fork(p1, p2)
	register struct proc *p1, *p2;
{
	register struct user *up = p2->p_addr;

#if defined(i386) || defined(pc532)
	/*
	 * avoid copying any of the parent's pagetables or other per-process
	 * objects that reside in the map by marking all of them non-inheritable
	 */
	(void)vm_map_inherit(&p1->p_vmspace->vm_map,
		VM_MAXUSER_ADDRESS, VM_MAX_ADDRESS, VM_INHERIT_NONE);
#endif
	p2->p_vmspace = vmspace_fork(p1->p_vmspace);

#ifdef SYSVSHM
	if (p1->p_vmspace->vm_shm)
		shmfork(p1, p2);
#endif

	vm_map_pageable(kernel_map, (vm_offset_t)up,
			(vm_offset_t)up + USPACE, FALSE);

	/*
	 * p_stats and p_sigacts currently point at fields
	 * in the user struct but not at &u, instead at p_addr.
	 * Copy p_sigacts and parts of p_stats; zero the rest
	 * of p_stats (statistics).
	 */
	p2->p_stats = &up->u_stats;
	p2->p_sigacts = &up->u_sigacts;
	up->u_sigacts = *p1->p_sigacts;
	bzero(&up->u_stats.pstat_startzero,
	    (unsigned) ((caddr_t)&up->u_stats.pstat_endzero -
	    (caddr_t)&up->u_stats.pstat_startzero));
	bcopy(&p1->p_stats->pstat_startcopy, &up->u_stats.pstat_startcopy,
	    ((caddr_t)&up->u_stats.pstat_endcopy -
	     (caddr_t)&up->u_stats.pstat_startcopy));

#if defined(i386) || defined(pc532)
	{
		 vm_offset_t addr = VM_MAXUSER_ADDRESS; struct vm_map *vp;

		 /* ream out old pagetables and kernel stack */
		 vp = &p2->p_vmspace->vm_map;
		 (void)vm_deallocate(vp, addr, VM_MAX_ADDRESS - addr);
		 (void)vm_allocate(vp, &addr, VM_MAX_ADDRESS - addr, FALSE);
		 (void)vm_map_inherit(vp, addr, VM_MAX_ADDRESS,
		     VM_INHERIT_NONE);
	}
#endif

#ifdef __FORK_BRAINDAMAGE
	/*
	 * cpu_fork will copy and update the kernel stack and pcb,
	 * and make the child ready to run.  It marks the child
	 * so that it can return differently than the parent.
	 * It returns twice, once in the parent process and
	 * once in the child.
	 */
	return (cpu_fork(p1, p2));
#else
	/*
	 * cpu_fork will copy and update the kernel stack and pcb,
	 * and make the child ready to run.  The child will exit
	 * directly to user mode on its first time slice, and will
	 * not return here.
	 */
	cpu_fork(p1, p2);
#endif
}

/*
 * Set default limits for VM system.
 * Called for proc 0, and then inherited by all others.
 */
void
vm_init_limits(p)
	register struct proc *p;
{

	/*
	 * Set up the initial limits on process VM.
	 * Set the maximum resident set size to be all
	 * of (reasonably) available memory.  This causes
	 * any single, large process to start random page
	 * replacement once it fills memory.
	 */
	p->p_rlimit[RLIMIT_STACK].rlim_cur = DFLSSIZ;
	p->p_rlimit[RLIMIT_STACK].rlim_max = MAXSSIZ;
	p->p_rlimit[RLIMIT_DATA].rlim_cur = DFLDSIZ;
	p->p_rlimit[RLIMIT_DATA].rlim_max = MAXDSIZ;
	p->p_rlimit[RLIMIT_RSS].rlim_cur = ptoa(cnt.v_free_count);
}

#include <vm/vm_pageout.h>

#ifdef DEBUG
int	enableswap = 1;
int	swapdebug = 0;
#define	SDB_FOLLOW	1
#define SDB_SWAPIN	2
#define SDB_SWAPOUT	4
#endif

/*
 * Swap in a process's u-area.
 */
void
swapin(p)
	struct proc *p;
{
	vm_offset_t addr;
	int s;

	addr = (vm_offset_t)p->p_addr;
	vm_map_pageable(kernel_map, addr, addr + USPACE, FALSE);
	/*
	 * Some architectures need to be notified when the
	 * user area has moved to new physical page(s) (e.g.
	 * see pmax/pmax/vm_machdep.c).
	 */
	cpu_swapin(p);
	s = splstatclock();
	if (p->p_stat == SRUN)
		setrunqueue(p);
	p->p_flag |= P_INMEM;
	splx(s);
	p->p_swtime = 0;
	++cnt.v_swpin;
}

/*
 * Brutally simple:
 *	1. Attempt to swapin every swaped-out, runnable process in
 *	   order of priority.
 *	2. If not enough memory, wake the pageout daemon and let it
 *	   clear some space.
 */
void
scheduler()
{
	register struct proc *p;
	register int pri;
	struct proc *pp;
	int ppri;

loop:
#ifdef DEBUG
	while (!enableswap) {
		panic ("swap disabled??");
		tsleep((caddr_t)&proc0, PVM, "noswap", 0);
	}
#endif
	pp = NULL;
	ppri = INT_MIN;
	for (p = allproc.lh_first; p != 0; p = p->p_list.le_next) {
		if (p->p_stat == SRUN && (p->p_flag & P_INMEM) == 0) {
			pri = p->p_swtime + p->p_slptime - p->p_nice * 8;
			if (pri > ppri) {
				pp = p;
				ppri = pri;
			}
		}
	}
#ifdef DEBUG
	if (swapdebug & SDB_FOLLOW)
		printf("scheduler: running, procp %p pri %d\n", pp, ppri);
#endif
	/*
	 * Nothing to do, back to sleep
	 */
	if ((p = pp) == NULL) {
		tsleep((caddr_t)&proc0, PVM, "scheduler", 0);
		goto loop;
	}

	/*
	 * We would like to bring someone in.
	 * This part is really bogus cuz we could deadlock on memory
	 * despite our feeble check.
	 * XXX should require at least vm_swrss / 2
	 */
	if (cnt.v_free_count > atop(USPACE)) {
#ifdef DEBUG
		if (swapdebug & SDB_SWAPIN)
			printf("swapin: pid %d(%s)@%p, pri %d free %d\n",
			    p->p_pid, p->p_comm, p->p_addr, ppri,
			    cnt.v_free_count);
#endif
#if defined(arc) || defined(pica)
			vm_map_pageable(kernel_map, (vm_offset_t)p->p_addr,
			    (vm_offset_t)p->p_addr + atop(USPACE), FALSE);
#endif
		swapin(p);
		goto loop;
	}
	/*
	 * Not enough memory, jab the pageout daemon and wait til the
	 * coast is clear.
	 */
#ifdef DEBUG
	if (swapdebug & SDB_FOLLOW)
		printf("scheduler: no room for pid %d(%s), free %d\n",
		    p->p_pid, p->p_comm, cnt.v_free_count);
#endif
	(void)splhigh();
	vm_wait("fLowmem");
	(void)spl0();
#ifdef DEBUG
	if (swapdebug & SDB_FOLLOW)
		printf("scheduler: room again, free %d\n", cnt.v_free_count);
#endif
	goto loop;
}

#define	swappable(p)							\
	(((p)->p_flag & (P_SYSTEM | P_INMEM | P_WEXIT)) == P_INMEM &&	\
	    (p)->p_holdcnt == 0)

/*
 * Swapout is driven by the pageout daemon.  Very simple, we find eligible
 * procs and unwire their u-areas.  We try to always "swap" at least one
 * process in case we need the room for a swapin.
 * If any procs have been sleeping/stopped for at least maxslp seconds,
 * they are swapped.  Else, we swap the longest-sleeping or stopped process,
 * if any, otherwise the longest-resident process.
 */
void
swapout_threads()
{
	register struct proc *p;
	struct proc *outp, *outp2;
	int outpri, outpri2;
	int didswap = 0;
	extern int maxslp;

#ifdef DEBUG
	if (!enableswap)
		return;
#endif
	outp = outp2 = NULL;
	outpri = outpri2 = 0;
	for (p = allproc.lh_first; p != 0; p = p->p_list.le_next) {
		if (!swappable(p))
			continue;
		switch (p->p_stat) {
		case SRUN:
			if (p->p_swtime > outpri2) {
				outp2 = p;
				outpri2 = p->p_swtime;
			}
			continue;
			
		case SSLEEP:
		case SSTOP:
			if (p->p_slptime >= maxslp) {
				swapout(p);
				didswap++;
			} else if (p->p_slptime > outpri) {
				outp = p;
				outpri = p->p_slptime;
			}
			continue;
		}
	}
	/*
	 * If we didn't get rid of any real duds, toss out the next most
	 * likely sleeping/stopped or running candidate.  We only do this
	 * if we are real low on memory since we don't gain much by doing
	 * it (USPACE bytes).
	 */
	if (didswap == 0 &&
	    cnt.v_free_count <= atop(round_page(USPACE))) {
		if ((p = outp) == 0)
			p = outp2;
#ifdef DEBUG
		if (swapdebug & SDB_SWAPOUT)
			printf("swapout_threads: no duds, try procp %p\n", p);
#endif
		if (p)
			swapout(p);
	}
}

void
swapout(p)
	register struct proc *p;
{
	vm_offset_t addr;
	int s;

#ifdef DEBUG
	if (swapdebug & SDB_SWAPOUT)
		printf("swapout: pid %d(%s)@%p, stat %x pri %d free %d\n",
		    p->p_pid, p->p_comm, p->p_addr, p->p_stat, p->p_slptime,
		    cnt.v_free_count);
#endif

	/*
	 * Do any machine-specific actions necessary before swapout.
	 * This can include saving floating point state, etc.
	 */
	cpu_swapout(p);

	/*
	 * Unwire the to-be-swapped process's user struct and kernel stack.
	 */
	addr = (vm_offset_t)p->p_addr;
	vm_map_pageable(kernel_map, addr, addr + USPACE, TRUE);
	pmap_collect(vm_map_pmap(&p->p_vmspace->vm_map));

	/*
	 * Mark it as (potentially) swapped out.
	 */
	s = splstatclock();
	p->p_flag &= ~P_INMEM;
	if (p->p_stat == SRUN)
		remrunqueue(p);
	splx(s);
	p->p_swtime = 0;
	++cnt.v_swpout;
}
