/*	$NetBSD: uvm_glue.c,v 1.19 1999/04/30 21:23:50 thorpej Exp $	*/

/* 
 * Copyright (c) 1997 Charles D. Cranor and Washington University.
 * Copyright (c) 1991, 1993, The Regents of the University of California.  
 *
 * All rights reserved.
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
 *	This product includes software developed by Charles D. Cranor,
 *      Washington University, the University of California, Berkeley and 
 *      its contributors.
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
 *	@(#)vm_glue.c	8.6 (Berkeley) 1/5/94
 * from: Id: uvm_glue.c,v 1.1.2.8 1998/02/07 01:16:54 chs Exp
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

/*
 * uvm_glue.c: glue functions
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
#include <vm/vm_page.h>
#include <vm/vm_kern.h>

#include <uvm/uvm.h>

#include <machine/cpu.h>

/*
 * local prototypes
 */

static void uvm_swapout __P((struct proc *));

/*
 * XXXCDC: do these really belong here?
 */

unsigned maxdmap = MAXDSIZ;	/* kern_resource.c: RLIMIT_DATA max */
unsigned maxsmap = MAXSSIZ;	/* kern_resource.c: RLIMIT_STACK max */

int readbuffers = 0;		/* allow KGDB to read kern buffer pool */
				/* XXX: see uvm_kernacc */


/*
 * uvm_kernacc: can the kernel access a region of memory
 *
 * - called from malloc [DIAGNOSTIC], and /dev/kmem driver (mem.c)
 */

boolean_t
uvm_kernacc(addr, len, rw)
	caddr_t addr;
	size_t len;
	int rw;
{
	boolean_t rv;
	vaddr_t saddr, eaddr;
	vm_prot_t prot = rw == B_READ ? VM_PROT_READ : VM_PROT_WRITE;

	saddr = trunc_page(addr);
	eaddr = round_page(addr+len);
	vm_map_lock_read(kernel_map);
	rv = uvm_map_checkprot(kernel_map, saddr, eaddr, prot);
	vm_map_unlock_read(kernel_map);

	/*
	 * XXX there are still some things (e.g. the buffer cache) that
	 * are managed behind the VM system's back so even though an
	 * address is accessible in the mind of the VM system, there may
	 * not be physical pages where the VM thinks there is.  This can
	 * lead to bogus allocation of pages in the kernel address space
	 * or worse, inconsistencies at the pmap level.  We only worry
	 * about the buffer cache for now.
	 */
	if (!readbuffers && rv && (eaddr > (vaddr_t)buffers &&
			     saddr < (vaddr_t)buffers + MAXBSIZE * nbuf))
		rv = FALSE;
	return(rv);
}

/*
 * uvm_useracc: can the user access it?
 *
 * - called from physio() and sys___sysctl().
 */

boolean_t
uvm_useracc(addr, len, rw)
	caddr_t addr;
	size_t len;
	int rw;
{
	boolean_t rv;
	vm_prot_t prot = rw == B_READ ? VM_PROT_READ : VM_PROT_WRITE;

#if (defined(i386) || defined(pc532)) && !defined(PMAP_NEW)
	/*
	 * XXX - specially disallow access to user page tables - they are
	 * in the map.  This is here until i386 & pc532 pmaps are fixed...
	 */
	if ((vaddr_t) addr >= VM_MAXUSER_ADDRESS
	    || (vaddr_t) addr + len > VM_MAXUSER_ADDRESS
	    || (vaddr_t) addr + len <= (vaddr_t) addr)
		return (FALSE);
#endif

	rv = uvm_map_checkprot(&curproc->p_vmspace->vm_map,
			trunc_page(addr), round_page(addr+len), prot);
	return(rv);
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
uvm_chgkprot(addr, len, rw)
	register caddr_t addr;
	size_t len;
	int rw;
{
	vm_prot_t prot;
	paddr_t pa;
	vaddr_t sva, eva;

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
		pmap_enter(pmap_kernel(), sva, pa&~1, prot, TRUE, 0);
	}
}
#endif

/*
 * vslock: wire user memory for I/O
 *
 * - called from physio and sys___sysctl
 * - XXXCDC: consider nuking this (or making it a macro?)
 */

void
uvm_vslock(p, addr, len)
	struct proc *p;
	caddr_t	addr;
	size_t	len;
{
	uvm_fault_wire(&p->p_vmspace->vm_map, trunc_page(addr), 
	    round_page(addr+len));
}

/*
 * vslock: wire user memory for I/O
 *
 * - called from physio and sys___sysctl
 * - XXXCDC: consider nuking this (or making it a macro?)
 */

void
uvm_vsunlock(p, addr, len)
	struct proc *p;
	caddr_t	addr;
	size_t	len;
{
	uvm_fault_unwire(p->p_vmspace->vm_map.pmap, trunc_page(addr), 
		round_page(addr+len));
}

/*
 * uvm_fork: fork a virtual address space
 *
 * - the address space is copied as per parent map's inherit values
 * - a new "user" structure is allocated for the child process
 *	[filled in by MD layer...]
 * - if specified, the child gets a new user stack described by
 *	stack and stacksize
 * - NOTE: the kernel stack may be at a different location in the child
 *	process, and thus addresses of automatic variables may be invalid
 *	after cpu_fork returns in the child process.  We do nothing here
 *	after cpu_fork returns.
 * - XXXCDC: we need a way for this to return a failure value rather
 *   than just hang
 */
void
uvm_fork(p1, p2, shared, stack, stacksize)
	struct proc *p1, *p2;
	boolean_t shared;
	void *stack;
	size_t stacksize;
{
	struct user *up = p2->p_addr;
	int rv;

	if (shared == TRUE)
		uvmspace_share(p1, p2);			/* share vmspace */
	else
		p2->p_vmspace = uvmspace_fork(p1->p_vmspace); /* fork vmspace */

	/*
	 * Wire down the U-area for the process, which contains the PCB
	 * and the kernel stack.  Wired state is stored in p->p_flag's
	 * P_INMEM bit rather than in the vm_map_entry's wired count
	 * to prevent kernel_map fragmentation.
	 */
	rv = uvm_fault_wire(kernel_map, (vaddr_t)up,
	    (vaddr_t)up + USPACE);
	if (rv != KERN_SUCCESS)
		panic("uvm_fork: uvm_fault_wire failed: %d", rv);

	/*
	 * p_stats and p_sigacts currently point at fields in the user
	 * struct but not at &u, instead at p_addr.  Copy p_sigacts and
	 * parts of p_stats; zero the rest of p_stats (statistics).
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
	
	/*
	 * cpu_fork will copy and update the kernel stack and pcb, and make
	 * the child ready to run.  The child will exit directly to user
	 * mode on its first time slice, and will not return here.
	 */
	cpu_fork(p1, p2, stack, stacksize);
}

/*
 * uvm_exit: exit a virtual address space
 *
 * - the process passed to us is a dead (pre-zombie) process; we
 *   are running on a different context now (the reaper).
 * - we must run in a separate thread because freeing the vmspace
 *   of the dead process may block.
 */
void
uvm_exit(p)
	struct proc *p;
{

	uvmspace_free(p->p_vmspace);
	uvm_km_free(kernel_map, (vaddr_t)p->p_addr, USPACE);
}

/*
 * uvm_init_limit: init per-process VM limits
 *
 * - called for process 0 and then inherited by all others.
 */
void
uvm_init_limits(p)
	struct proc *p;
{

	/*
	 * Set up the initial limits on process VM.  Set the maximum
	 * resident set size to be all of (reasonably) available memory.
	 * This causes any single, large process to start random page
	 * replacement once it fills memory.
	 */

	p->p_rlimit[RLIMIT_STACK].rlim_cur = DFLSSIZ;
	p->p_rlimit[RLIMIT_STACK].rlim_max = MAXSSIZ;
	p->p_rlimit[RLIMIT_DATA].rlim_cur = DFLDSIZ;
	p->p_rlimit[RLIMIT_DATA].rlim_max = MAXDSIZ;
	p->p_rlimit[RLIMIT_RSS].rlim_cur = ptoa(uvmexp.free);
}

#ifdef DEBUG
int	enableswap = 1;
int	swapdebug = 0;
#define	SDB_FOLLOW	1
#define SDB_SWAPIN	2
#define SDB_SWAPOUT	4
#endif

/*
 * uvm_swapin: swap in a process's u-area.
 */

void
uvm_swapin(p)
	struct proc *p;
{
	vaddr_t addr;
	int s;

	addr = (vaddr_t)p->p_addr;
	/* make P_INMEM true */
	uvm_fault_wire(kernel_map, addr, addr + USPACE);

	/*
	 * Some architectures need to be notified when the user area has
	 * moved to new physical page(s) (e.g.  see mips/mips/vm_machdep.c).
	 */
	cpu_swapin(p);
	s = splstatclock();
	if (p->p_stat == SRUN)
		setrunqueue(p);
	p->p_flag |= P_INMEM;
	splx(s);
	p->p_swtime = 0;
	++uvmexp.swapins;
}

/*
 * uvm_scheduler: process zero main loop
 *
 * - attempt to swapin every swaped-out, runnable process in order of
 *	priority.
 * - if not enough memory, wake the pagedaemon and let it clear space.
 */

void
uvm_scheduler()
{
	register struct proc *p;
	register int pri;
	struct proc *pp;
	int ppri;
	UVMHIST_FUNC("uvm_scheduler"); UVMHIST_CALLED(maphist);

loop:
#ifdef DEBUG
	while (!enableswap)
		tsleep((caddr_t)&proc0, PVM, "noswap", 0);
#endif
	pp = NULL;		/* process to choose */
	ppri = INT_MIN;	/* its priority */
	for (p = allproc.lh_first; p != 0; p = p->p_list.le_next) {

		/* is it a runnable swapped out process? */
		if (p->p_stat == SRUN && (p->p_flag & P_INMEM) == 0) {
			pri = p->p_swtime + p->p_slptime -
			    (p->p_nice - NZERO) * 8;
			if (pri > ppri) {   /* higher priority?  remember it. */
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
	 * we have found swapped out process which we would like to bring
	 * back in.
	 *
	 * XXX: this part is really bogus cuz we could deadlock on memory
	 * despite our feeble check
	 */
	if (uvmexp.free > atop(USPACE)) {
#ifdef DEBUG
		if (swapdebug & SDB_SWAPIN)
			printf("swapin: pid %d(%s)@%p, pri %d free %d\n",
	     p->p_pid, p->p_comm, p->p_addr, ppri, uvmexp.free);
#endif
		uvm_swapin(p);
		goto loop;
	}
	/*
	 * not enough memory, jab the pageout daemon and wait til the coast
	 * is clear
	 */
#ifdef DEBUG
	if (swapdebug & SDB_FOLLOW)
		printf("scheduler: no room for pid %d(%s), free %d\n",
	   p->p_pid, p->p_comm, uvmexp.free);
#endif
	(void) splhigh();
	uvm_wait("schedpwait");
	(void) spl0();
#ifdef DEBUG
	if (swapdebug & SDB_FOLLOW)
		printf("scheduler: room again, free %d\n", uvmexp.free);
#endif
	goto loop;
}

/*
 * swappable: is process "p" swappable?
 */

#define	swappable(p)							\
	(((p)->p_flag & (P_SYSTEM | P_INMEM | P_WEXIT)) == P_INMEM &&	\
	 (p)->p_holdcnt == 0)

/*
 * swapout_threads: find threads that can be swapped and unwire their
 *	u-areas.
 *
 * - called by the pagedaemon
 * - try and swap at least one processs
 * - processes that are sleeping or stopped for maxslp or more seconds
 *   are swapped... otherwise the longest-sleeping or stopped process
 *   is swapped, otherwise the longest resident process...
 */
void
uvm_swapout_threads()
{
	register struct proc *p;
	struct proc *outp, *outp2;
	int outpri, outpri2;
	int didswap = 0;
	extern int maxslp; 
	/* XXXCDC: should move off to uvmexp. or uvm., also in uvm_meter */

#ifdef DEBUG
	if (!enableswap)
		return;
#endif

	/*
	 * outp/outpri  : stop/sleep process with largest sleeptime < maxslp
	 * outp2/outpri2: the longest resident process (its swap time)
	 */
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
				uvm_swapout(p);			/* zap! */
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
	if (didswap == 0 && uvmexp.free <= atop(round_page(USPACE))) {
		if ((p = outp) == NULL)
			p = outp2;
#ifdef DEBUG
		if (swapdebug & SDB_SWAPOUT)
			printf("swapout_threads: no duds, try procp %p\n", p);
#endif
		if (p)
			uvm_swapout(p);
	}
}

/*
 * uvm_swapout: swap out process "p"
 *
 * - currently "swapout" means "unwire U-area" and "pmap_collect()" 
 *   the pmap.
 * - XXXCDC: should deactivate all process' private anonymous memory
 */

static void
uvm_swapout(p)
	register struct proc *p;
{
	vaddr_t addr;
	int s;

#ifdef DEBUG
	if (swapdebug & SDB_SWAPOUT)
		printf("swapout: pid %d(%s)@%p, stat %x pri %d free %d\n",
	   p->p_pid, p->p_comm, p->p_addr, p->p_stat,
	   p->p_slptime, uvmexp.free);
#endif

	/*
	 * Do any machine-specific actions necessary before swapout.
	 * This can include saving floating point state, etc.
	 */
	cpu_swapout(p);

	/*
	 * Unwire the to-be-swapped process's user struct and kernel stack.
	 */
	addr = (vaddr_t)p->p_addr;
	uvm_fault_unwire(kernel_map->pmap, addr, addr + USPACE); /* !P_INMEM */
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
	++uvmexp.swapouts;
}

