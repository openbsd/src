/*      $OpenBSD: vm_machdep.c,v 1.13 1998/07/28 00:13:56 millert Exp $       */
/*      $NetBSD: vm_machdep.c,v 1.33 1997/07/06 22:38:22 ragge Exp $       */

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

 /* All bugs are subject to removal without further notice */
		
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

	stor = (size >> PGSHIFT) * sizeof(struct pte);
	bcopy(fpte, tpte, stor);
	bzero(fpte, stor);
	mtpr(0, PR_TBIA);
}

#define VIRT2PHYS(x) \
	(((*(int *)((((((int)x) & 0x7fffffff) >> 9) * 4) + \
		(unsigned int)Sysmap)) & 0x1fffff) << 9)

/*
 * cpu_fork() copies parent process trapframe directly into child PCB
 * so that when we swtch() to the child process it will go directly
 * back to user mode without any need to jump back through kernel.
 * We also take away mapping for the second page after pcb, so that
 * we get something like a "red zone".
 * No need for either double-map kernel stack or relocate it when
 * forking.
 */
void
cpu_fork(p1, p2)
	struct proc *p1, *p2;
{
	struct pcb *nyproc;
	struct trapframe *tf;
	struct pmap *pmap, *opmap;

	nyproc = &p2->p_addr->u_pcb;
	tf = p1->p_addr->u_pcb.framep;
	opmap = &p1->p_vmspace->vm_pmap;
	pmap = &p2->p_vmspace->vm_pmap;
	pmap->pm_pcb = nyproc;

#ifdef notyet
	/* Mark page invalid */
	p2pte = kvtopte((u_int *)p2->p_addr + 2 * NBPG);
	*p2pte = 0; 
#endif

#ifdef notyet
	/* Set up internal defs in PCB, and alloc PTEs. */
	nyproc->P0BR = kmem_alloc_wait(pte_map,
	    (opmap->pm_pcb->P0LR & ~AST_MASK) * 4);
	nyproc->P1BR = kmem_alloc_wait(pte_map,
	    (0x800000 - (pmap->pm_pcb->P1LR * 4))) - 0x800000;
	nyproc->P0LR = opmap->pm_pcb->P0LR;
	nyproc->P1LR = opmap->pm_pcb->P1LR;
#else
	nyproc->P0BR = (void *)0x80000000;
	nyproc->P1BR = (void *)0x80000000;
	nyproc->P0LR = AST_PCB;
	nyproc->P1LR = 0x200000;
#endif
	nyproc->iftrap = NULL;
	nyproc->KSP = (u_int)p2->p_addr + USPACE;

	/* General registers as taken from userspace */
	/* trapframe should be synced with pcb */
	bcopy(&tf->r2,&nyproc->R[2],10*sizeof(int));
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
cpu_set_kpc(p, pc)
	struct proc *p;
	void (*pc) __P((struct proc *));
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
	kc->cf.ca_arg1 = (unsigned)p;
	kc->tf.r11 = boothowto;	/* If we have old init */
	kc->tf.psl = 0x3c00000;

	nyproc->framep = (void *)&kc->tf;
	nyproc->AP = (unsigned)&kc->cf.ca_argno;
	nyproc->FP = nyproc->KSP = (unsigned)kc;
	nyproc->PC = (unsigned)pc + 2;
}

/*
 * Put in a process on the correct run queue based on it's priority
 * and set the bit corresponding to the run queue.
 */
void 
setrunqueue(p)
	struct proc *p;
{
	struct	prochd *q;
	int	knummer;

	if (p->p_back) 
		panic("sket sig i setrunqueue");

	knummer = (p->p_priority >> 2);
	bitset(knummer, whichqs);
	q = &qs[knummer];

	_insque(p, q);

	return;
}

/*
 * Remove a process from the run queue. If this is the last process
 * on that queue, clear the queue bit in whichqs.
 */
void
remrunqueue(p)
	struct proc *p;
{
	struct	proc *qp;
	int	bitnr;

	bitnr = (p->p_priority >> 2);
	if (bitisclear(bitnr, whichqs))
		panic("remrunqueue: Process not in queue");

	_remque(p);

	qp = (struct proc *)&qs[bitnr];
	if (qp->p_forw == qp)
		bitclear(bitnr, whichqs);
}

volatile caddr_t curpcb, nypcb;

/*
 * Machine dependent part of switch function. Find the next process 
 * with the highest priority to run. If the process queues are empty,
 * sleep waiting for something to happen. The idle loop resides here.
 */
void
cpu_switch(pp)
	struct proc *pp;
{
	int	i,s;
	struct	proc *p, *q;
	extern	unsigned int scratch;

again:	
	/* First: Search for a queue. */
	s = splhigh();
	if ((i = ffs(whichqs) - 1) < 0)
		goto idle;

	/*
	 * A queue with runnable processes found.
	 * Get first process from queue. 
	 */
	asm(".data;savpsl:	.long	0;.text;movpsl savpsl");
	q = (struct proc *)&qs[i];
	if (q->p_forw == q)
		panic("swtch: no process queued");

	/* Remove process from queue */
	bitclear(i, whichqs);
	p = q->p_forw;
	_remque(p);

	if (q->p_forw != q)
		bitset(i, whichqs);
	if (curproc)
		(u_int)curpcb = VIRT2PHYS(&curproc->p_addr->u_pcb);
	else
		(u_int)curpcb = scratch & 0x7fffffff;
	(u_int)nypcb = VIRT2PHYS(&p->p_addr->u_pcb);

	if (p == 0)
		panic("switch: null proc pointer");
	want_resched = 0;
	curproc = p;

	/* Don't change process if it's the same that we'r already running */
	if (curpcb == nypcb)
		return;

	asm("pushl savpsl");
	asm("jsb _loswtch");

	return; /* New process! */

idle:	
	p = curproc;
	curproc = NULL;		/* This is nice. /BQT */
	spl0();
	while (whichqs == 0)
		;
	curproc = p;
	goto again;
}

/* Should check that values is in bounds XXX */
int
copyinstr(from, to, maxlen, lencopied)
	const void *from;
	void *to;
	size_t *lencopied;
	size_t maxlen;
{
	u_int i;
	void *addr=&curproc->p_addr->u_pcb.iftrap;
	const char *gfrom = from;
	char *gto = to;

	asm("movl $Lstr,(%0)":: "r"(addr));
	for(i=0;i<maxlen;i++){
		*(gto +i )=*(gfrom + i);
		if(!(*(gto+i))) goto ok;
	}

	return(ENAMETOOLONG);
ok:
	if(lencopied) *lencopied=i+1;
	return(0);
}

asm("Lstr:	ret");

/* Should check that values is in bounds XXX */
int
copyoutstr(from, to, maxlen, lencopied)
	const	void *from;
	void	*to;
	size_t	*lencopied;
	size_t	maxlen;
{
	u_int i;
	const char *gfrom=from;
	char *gto=to;
        void *addr=&curproc->p_addr->u_pcb.iftrap;

        asm("movl $Lstr,(%0)":: "r"(addr));
	for(i=0;i<maxlen;i++){
		*(gto+i)=*(gfrom+i);
		if(!(*(gto+i))) goto ok;
	}

	return(ENAMETOOLONG);
ok:
	if(lencopied) *lencopied=i+1;
	return 0;
}

int	reno_zmagic __P((struct proc *, struct exec_package *));


int
cpu_exec_aout_makecmds(p, epp)
	struct proc *p;
	struct exec_package *epp;
{
	int error;
	struct exec *ep;
	/*
	 * Compatibility with reno programs.
	 */
	ep=epp->ep_hdr;
	switch (ep->a_midmag) {
	case 0x10b: /* ZMAGIC in 4.3BSD Reno programs */
		error = reno_zmagic(p, epp);
		break;
	case 0x108:
printf("Warning: reno_nmagic\n");
		error = exec_aout_prep_nmagic(p, epp);
		break;
	case 0x107:
printf("Warning: reno_omagic\n");
		error = exec_aout_prep_omagic(p, epp);
		break;
	default:
		error = ENOEXEC;
	}
	return(error);
}

int
sys_sysarch(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{

	return (ENOSYS);
};

#ifdef COMPAT_ULTRIX
extern struct emul emul_ultrix;
#endif
/*
 * 4.3BSD Reno programs have an 1K header first in the executable
 * file, containing a.out header. Otherwise programs are identical.
 *
 *      from: exec_aout.c,v 1.9 1994/01/28 23:46:59 jtc Exp $
 */

int
reno_zmagic(p, epp)
	struct proc *p;
	struct exec_package *epp;
{
	struct exec *execp = epp->ep_hdr;

	epp->ep_taddr = 0;
	epp->ep_tsize = execp->a_text;
	epp->ep_daddr = epp->ep_taddr + execp->a_text;
	epp->ep_dsize = execp->a_data + execp->a_bss;
	epp->ep_entry = execp->a_entry;

#ifdef COMPAT_ULTRIX
	epp->ep_emul = &emul_ultrix;
#endif

	/*
	 * check if vnode is in open for writing, because we want to
	 * demand-page out of it.  if it is, don't do it, for various
	 * reasons
	 */
	if ((execp->a_text != 0 || execp->a_data != 0) &&
	    epp->ep_vp->v_writecount != 0) {
		return ETXTBSY;
	}
	epp->ep_vp->v_flag |= VTEXT;

	/* set up command for text segment */
	NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_pagedvn, execp->a_text,
	    epp->ep_taddr, epp->ep_vp, 0x400, VM_PROT_READ|VM_PROT_EXECUTE);

	/* set up command for data segment */
	NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_pagedvn, execp->a_data,
	    epp->ep_daddr, epp->ep_vp, execp->a_text+0x400,
	    VM_PROT_READ|VM_PROT_WRITE|VM_PROT_EXECUTE);

	/* set up command for bss segment */
	NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_zero, execp->a_bss,
	    epp->ep_daddr + execp->a_data, NULLVP, 0,
	    VM_PROT_READ|VM_PROT_WRITE|VM_PROT_EXECUTE);

	return exec_setup_stack(p, epp);
}

void
cpu_exit(p)
	struct	proc *p;
{
	extern	unsigned int scratch;

	if (p == 0)
		panic("cpu_exit from null process");
	vmspace_free(p->p_vmspace);

	(void) splimp();
	/* Must change kernel stack before freeing */
	mtpr(scratch + NBPG, PR_KSP);
	kmem_free(kernel_map, (vm_offset_t)p->p_addr, ctob(UPAGES));
	cpu_switch(0);
	/* NOTREACHED */
}

int
suword(ptr, val)
	void *ptr;
	long val;
{
        void *addr=&curproc->p_addr->u_pcb.iftrap;

        asm("movl $Lstr,(%0)":: "r"(addr));
	*(int *)ptr=val;
	return 0;
}

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
	CORE_SETMAGIC(*chdr, COREMAGIC, MID_VAX, 0);
	chdr->c_hdrsize = sizeof(struct core);
	chdr->c_seghdrsize = sizeof(struct coreseg);
	chdr->c_cpusize = sizeof(struct md_coredump);

	bcopy(tf, &state, sizeof(struct md_coredump));

	CORE_SETMAGIC(cseg, CORESEGMAGIC, MID_VAX, CORE_CPU);
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

int	locopyout __P((const void *, void *, size_t, void *));
int	locopyin __P((const void *, void *, size_t, void *));

int
copyout(from, to, len)
	const	void *from;
	void	*to;
	size_t	len;
{
	void *addr=&curproc->p_addr->u_pcb.iftrap;

	return locopyout(from, to, len, addr);
}

int
copyin(from, to, len)
	const	void *from;
	void	*to;
	size_t	len;
{
	void *addr = &curproc->p_addr->u_pcb.iftrap;

	return locopyin(from, to, len, addr);
}

/*
 * cpu_swapin() is called just before a process shall be swapped in.
 * Kernel stack and pcb must be mapped when we swtch() to this new
 * process, to guarantee that we frob all pages here to ensure that
 * they actually are in-core. Kernel stack red zone is also updated
 * here.
 */
void
cpu_swapin(p)
	struct proc *p;
{
	u_int uarea, i, *j, rv;

	uarea = (u_int)p->p_addr;

	for (i = uarea;i < uarea + USPACE;i += PAGE_SIZE) {
		j = (u_int *)kvtopte(i);
		if ((*j & PG_V) == 0) {
			rv = vm_fault(kernel_map, i,
			    VM_PROT_WRITE|VM_PROT_READ, FALSE);
			if (rv != KERN_SUCCESS)
				panic("cpu_swapin: rv %d",rv);
		}
	}
#ifdef notyet
	j = (u_int *)kvtopte(uarea + 2 * NBPG);
	*j = 0; /* Set kernel stack red zone */
#endif
}

#if VAX410 || VAX43
/*
 * vmapbuf()/vunmapbuf() only used on some vaxstations without
 * any busadapter with MMU.
 * XXX - This must be reworked to be effective.
 */
void
vmapbuf(bp, len)
        struct buf *bp;
        vm_size_t len;
{
        vm_offset_t faddr, taddr, off, pa;
        pmap_t fmap, tmap;

	if ((vax_boardtype != VAX_BTYP_43) && (vax_boardtype != VAX_BTYP_410))
		return;
        faddr = trunc_page(bp->b_saveaddr = bp->b_data);
        off = (vm_offset_t)bp->b_data - faddr;
        len = round_page(off + len);
        taddr = kmem_alloc_wait(phys_map, len);
        bp->b_data = (caddr_t)(taddr + off);
        fmap = vm_map_pmap(&bp->b_proc->p_vmspace->vm_map);
        tmap = vm_map_pmap(phys_map);
        len = len >> PGSHIFT;
        while (len--) {
		volatile int i = *(int *)faddr;

                pa = pmap_extract(fmap, faddr);
                if (pa == 0)
                       	panic("vmapbuf: null page frame for %x", faddr);

                pmap_enter(tmap, taddr, pa & ~(NBPG - 1),
                           VM_PROT_READ|VM_PROT_WRITE, TRUE);
                faddr += NBPG;
                taddr += NBPG;
        }
}

/*
 * Free the io map PTEs associated with this IO operation.
 * We also invalidate the TLB entries and restore the original b_addr.
 */
void
vunmapbuf(bp, len)
        struct buf *bp;
        vm_size_t len;
{
        vm_offset_t addr, off;

	if ((vax_boardtype != VAX_BTYP_43) && (vax_boardtype != VAX_BTYP_410))
		return;
        addr = trunc_page(bp->b_data);
        off = (vm_offset_t)bp->b_data - addr;
        len = round_page(off + len);
        kmem_free_wakeup(phys_map, addr, len);
        bp->b_data = bp->b_saveaddr;
        bp->b_saveaddr = 0;
}
#endif
