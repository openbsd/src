/*	$OpenBSD: vm_machdep.c,v 1.16 2000/04/27 01:10:14 bjc Exp $	*/
/*	$NetBSD: vm_machdep.c,v 1.56 2000/01/20 22:19:00 sommerfeld Exp $	     */

/*
 * Copyright (c) 1994 Ludd, University of Lule}, Sweden.
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
 *     This product includes software developed at Ludd, University of Lule}.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/exec.h>
#include <sys/vnode.h>
#include <sys/core.h>
#include <sys/mount.h>
#include <sys/device.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>

#include <machine/vmparam.h>
#include <machine/mtpr.h>
#include <machine/pmap.h>
#include <machine/pte.h>
#include <machine/macros.h>
#include <machine/trap.h>
#include <machine/pcb.h>
#include <machine/frame.h>
#include <machine/cpu.h>
#include <machine/sid.h>

#include <sys/syscallargs.h>

volatile int whichqs;

/*
 * pagemove - moves pages at virtual address from to virtual address to,
 * block moved of size size. Using fast insn bcopy for pte move.
 */
void
pagemove(from, to, size)
	caddr_t from, to;
	size_t size;
{
	pt_entry_t *fpte, *tpte;
	int	stor;

	fpte = kvtopte(from);
	tpte = kvtopte(to);

	stor = (size >> VAX_PGSHIFT) * sizeof(struct pte);
	bcopy(fpte, tpte, stor);
	bzero(fpte, stor);
	mtpr(0, PR_TBIA);
}

/*
 * Finish a fork operation, with process p2 nearly set up.
 * Copy and update the pcb and trap frame, making the child ready to run.
 * 
 * Rig the child's kernel stack so that it will start out in
 * proc_trampoline() and call child_return() with p2 as an
 * argument. This causes the newly-created child process to go
 * directly to user level with an apparent return value of 0 from
 * fork(), while the parent process returns normally.
 *
 * p1 is the process being forked; if p1 == &proc0, we are creating
 * a kernel thread, and the return path will later be changed in cpu_set_kpc.
 *
 * If an alternate user-level stack is requested (with non-zero values
 * in both the stack and stacksize args), set up the user stack pointer
 * accordingly.
 *
 * cpu_fork() copies parent process trapframe directly into child PCB
 * so that when we swtch() to the child process it will go directly
 * back to user mode without any need to jump back through kernel.
 * We also take away mapping for the second page after pcb, so that
 * we get something like a "red zone".
 * No need for either double-map kernel stack or relocate it when
 * forking.
 */
void
cpu_fork(p1, p2, stack, stacksize)
	struct proc *p1, *p2;
	void *stack;
	size_t stacksize;
{
	struct pte *pt;
	struct pcb *nyproc;
	struct trapframe *tf;
	struct pmap *pmap, *opmap;

#ifdef DIAGNOSTIC
	/*
	 * if p1 != curproc && p1 == &proc0, we're creating a kernel thread.
	 */
	if (p1 != curproc && p1 != &proc0)
		panic("cpu_fork: curproc");
#endif

	nyproc = &p2->p_addr->u_pcb;
	tf = p1->p_addr->u_pcb.framep;
	opmap = p1->p_vmspace->vm_map.pmap;
	pmap = p2->p_vmspace->vm_map.pmap;

	/* Mark page invalid */
	pt = kvtopte((u_int)p2->p_addr + REDZONEADDR);
	pt->pg_v = 0; 

	/*
	 * Activate address space for the new process.	The PTEs have
	 * already been allocated by way of pmap_create().
	 */
	pmap_activate(p2);

	/* Set up internal defs in PCB. */
	nyproc->iftrap = NULL;
	nyproc->KSP = (u_int)p2->p_addr + USPACE;

	/* General registers as taken from userspace */
	/* trapframe should be synced with pcb */
	bcopy(&tf->r2,&nyproc->R[2],10*sizeof(int));

	/*
	 * If specified, give the child a different stack.
	 */
	if (stack != NULL)
		tf->sp = (u_long)stack + stacksize;

	nyproc->AP = tf->ap;
	nyproc->FP = tf->fp;
	nyproc->USP = tf->sp;
	nyproc->PC = tf->pc;
	nyproc->PSL = tf->psl & ~PSL_C;
	nyproc->R[0] = p1->p_pid; /* parent pid. (shouldn't be needed) */
	nyproc->R[1] = 1;

	return; /* Child is ready. Parent, return! */
}

/*
 * cpu_set_kpc() sets up pcb for the new kernel process so that it will
 * start at the procedure pointed to by pc next time swtch() is called.
 * When that procedure returns, it will pop off everything from the
 * faked calls frame on the kernel stack, do an REI and go down to
 * user mode.
 */
void
cpu_set_kpc(p, pc, arg)
	struct proc *p;
	void (*pc) __P((void *));
	void *arg;
{
	struct pcb *nyproc;
	struct {
		struct	callsframe cf;
		struct	trapframe tf;
	} *kc;
	extern int sret, boothowto;

	nyproc = &p->p_addr->u_pcb;
	(unsigned)kc = nyproc->FP = nyproc->KSP =
	    (unsigned)p->p_addr + USPACE - sizeof(*kc);
	kc->cf.ca_cond = 0;
	kc->cf.ca_maskpsw = 0x20000000;
	kc->cf.ca_pc = (unsigned)&sret;
	kc->cf.ca_argno = 1;
	kc->cf.ca_arg1 = (unsigned)arg;
	kc->tf.r11 = boothowto; /* If we have old init */
	kc->tf.psl = 0x3c00000;

	nyproc->framep = (void *)&kc->tf;
	nyproc->AP = (unsigned)&kc->cf.ca_argno;
	nyproc->FP = nyproc->KSP = (unsigned)kc;
	nyproc->PC = (unsigned)pc + 2;
}

int
cpu_exec_aout_makecmds(p, epp)
	struct proc *p;
	struct exec_package *epp;
{
	return ENOEXEC;
}

int
sys_sysarch(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{

	return (ENOSYS);
};

/*
 * Dump the machine specific header information at the start of a core dump.
 * First put all regs in PCB for debugging purposes. This is not an good
 * way to do this, but good for my purposes so far.
 */
int
cpu_coredump(p, vp, cred, chdr)
	struct proc *p;
	struct vnode *vp;
	struct ucred *cred;
	struct core *chdr;
{
	struct trapframe *tf;
	struct md_coredump state;
	struct coreseg cseg;
	int error;

	tf = p->p_addr->u_pcb.framep;
	CORE_SETMAGIC(*chdr, COREMAGIC, MID_MACHINE, 0);
	chdr->c_hdrsize = sizeof(struct core);
	chdr->c_seghdrsize = sizeof(struct coreseg);
	chdr->c_cpusize = sizeof(struct md_coredump);

	bcopy(tf, &state, sizeof(struct md_coredump));

	CORE_SETMAGIC(cseg, CORESEGMAGIC, MID_MACHINE, CORE_CPU);
	cseg.c_addr = 0;
	cseg.c_size = chdr->c_cpusize;

	error = vn_rdwr(UIO_WRITE, vp, (caddr_t)&cseg, chdr->c_seghdrsize,
	    (off_t)chdr->c_hdrsize, UIO_SYSSPACE,
	    IO_NODELOCKED|IO_UNIT, cred, NULL, p);
	if (error)
		return error;

	error = vn_rdwr(UIO_WRITE, vp, (caddr_t)&state, sizeof(state),
	    (off_t)(chdr->c_hdrsize + chdr->c_seghdrsize), UIO_SYSSPACE,
	    IO_NODELOCKED|IO_UNIT, cred, NULL, p);

	if (!error)
		chdr->c_nseg++;

	return error;
}

/*
 * Kernel stack red zone need to be set when a process is swapped in.
 */
void
cpu_swapin(p)
	struct proc *p;
{
	kvtopte((vaddr_t)p->p_addr + REDZONEADDR)->pg_v = 0;
}

/*
 * Map in a bunch of pages read/writeable for the kernel.
 */
void
ioaccess(vaddr, paddr, npgs)
	vaddr_t vaddr;
	paddr_t paddr;
	int npgs;
{
	u_int *pte = (u_int *)kvtopte(vaddr);
	int i;

	for (i = 0; i < npgs; i++)
		pte[i] = PG_V | PG_KW | (PG_PFNUM(paddr) + i);
}

/*
 * Opposite to the above: just forget their mapping.
 */
void
iounaccess(vaddr, npgs)
	vaddr_t vaddr;
	int npgs;
{
	u_int *pte = (u_int *)kvtopte(vaddr);
	int i;

	for (i = 0; i < npgs; i++)
		pte[i] = 0;
	mtpr(0, PR_TBIA);
}
