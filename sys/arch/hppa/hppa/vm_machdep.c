/*	$OpenBSD: vm_machdep.c,v 1.9 1999/09/20 21:14:22 mickey Exp $	*/

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/vnode.h>
#include <sys/user.h>
#include <sys/ptrace.h>
#include <sys/exec.h>
#include <sys/core.h>

#include <machine/pmap.h>
#include <machine/pcb.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <uvm/uvm.h>


/*
 * Dump the machine specific header information at the start of a core dump.
 */
int
cpu_coredump(p, vp, cred, core)
	struct proc *p;
	struct vnode *vp;
	struct ucred *cred;
	struct core *core;
{
	struct md_coredump md_core;
	struct coreseg cseg;
	off_t off;
	int error;

	CORE_SETMAGIC(*core, COREMAGIC, MID_ZERO, 0);
	core->c_hdrsize = ALIGN(sizeof(*core));
	core->c_seghdrsize = ALIGN(sizeof(cseg));
	core->c_cpusize = sizeof(md_core);

	process_read_regs(p, &md_core.md_reg);

	CORE_SETMAGIC(cseg, CORESEGMAGIC, MID_ZERO, CORE_CPU);
	cseg.c_addr = 0;
	cseg.c_size = core->c_cpusize;

#define	write(vp, addr, n) vn_rdwr(UIO_WRITE, (vp), (caddr_t)(addr), (n), off, \
			     UIO_SYSSPACE, IO_NODELOCKED|IO_UNIT, cred, NULL, p)
	
	off = core->c_hdrsize;
	if ((error = write(vp, &cseg, core->c_seghdrsize)))
		return error;
	off += core->c_seghdrsize;
	if ((error = write(vp, (caddr_t)&md_core, sizeof md_core)))
		return error;

#undef write
	core->c_nseg++;

	return error;
}

/*
 * Move pages from one kernel virtual address to another.
 * Both addresses are assumed to reside in the Sysmap,
 * and size must be a multiple of CLSIZE.
 */
void
pagemove(from, to, size)
	register caddr_t from, to;
	size_t size;
{
	register vm_offset_t pa;

	while (size > 0) {
		pa = pmap_extract(pmap_kernel(), (vm_offset_t)from);
		pmap_remove(pmap_kernel(),
			    (vm_offset_t)from, (vm_offset_t)from + PAGE_SIZE);
		pmap_enter(pmap_kernel(), (vm_offset_t)to, pa,
			   VM_PROT_READ|VM_PROT_WRITE, 1,
			   VM_PROT_READ|VM_PROT_WRITE);
		from += PAGE_SIZE;
		to += PAGE_SIZE;
		size -= PAGE_SIZE;
	}
}

void
cpu_swapin(p)
	struct proc *p;
{
	/* nothing yet, move to macros later */
}

void
cpu_swapout(p)
	struct proc *p;
{
	/*
	 * explicit FPU save state, since user area might get
	 * swapped out as well, and won't be able to save it no more
	 */
}

void
cpu_fork(p1, p2, stack, stacksize)
	struct proc *p1, *p2;
	void *stack;
	size_t stacksize;
{
	register struct pcb *pcbp;
	register struct trapframe *tf;
	register_t sp;

	pcbp = &p2->p_addr->u_pcb;
	*pcbp = p1->p_addr->u_pcb;
	/* space is cached for the copy{in,out}'s pleasure */
	pcbp->pcb_space = p2->p_vmspace->vm_map.pmap->pmap_space;

#ifdef DIAGNOSTIC
	if (round_page(sizeof(struct user)) >= USPACE)
		panic("USPACE too small for user");
#endif

	p2->p_md.md_regs = tf = &pcbp->pcb_tf;
	sp = (register_t)p2->p_addr + round_page(sizeof(struct user));

	/* setup initial stack frame */
	bzero((caddr_t)sp, HPPA_FRAME_SIZE);
	tf->tf_sp = sp + HPPA_FRAME_SIZE;

	/*
	 * If specified, give the child a different stack.
	 */
	if (stack != NULL)
		tf->tf_sp = (register_t)stack;

	/*
	 * everybody recomends to note here the inline version of
	 * the cpu_set_kpc(), so note it !
	 */
	tf->tf_arg0 = KERNMODE(child_return);
	tf->tf_arg1 = (register_t)p2;
	tf->tf_iioq_tail = tf->tf_iioq_head = (register_t)switch_trampoline;
}

void
cpu_set_kpc(p, pc, arg)
	struct proc *p;
	void (*pc) __P((void *));
	void *arg;
{
	register struct pcb *pcbp = &p->p_addr->u_pcb;

	pcbp->pcb_tf.tf_arg0 = (register_t)pc;
	pcbp->pcb_tf.tf_arg1 = (register_t)arg;
	pcbp->pcb_tf.tf_iioq_tail = pcbp->pcb_tf.tf_iioq_head = 
		(register_t)switch_trampoline;

}

void
cpu_exit(p)
	struct proc *p;
{
	extern struct proc *fpu_curproc;	/* from machdep.c */
	uvmexp.swtch++;

	curproc = NULL;
	fpu_curproc = NULL;
	uvmspace_free(p->p_vmspace);

	/* XXX should be in the locore? */
	uvm_km_free(kernel_map, (vaddr_t)p->p_addr, USPACE);

	splhigh();
	switch_exit(p);
}

/*
 * Map an IO request into kernel virtual address space.
 */
void
vmapbuf(bp, len)
	struct buf *bp;
	vm_size_t len;
{
	vm_offset_t faddr, taddr, off, pa;
	struct proc *p;

	if ((bp->b_flags & B_PHYS) == 0)
		panic("vmapbuf");
	p = bp->b_proc;
	faddr = trunc_page(bp->b_saveaddr = bp->b_data);
	off = (vm_offset_t)bp->b_data - faddr;
	len = round_page(off + len);
	taddr = uvm_km_valloc_wait(phys_map, len);
	bp->b_data = (caddr_t)(taddr + off);
	len = atop(len);
	while (len--) {
		pa = pmap_extract(vm_map_pmap(&p->p_vmspace->vm_map), faddr);
		if (pa == 0)
			panic("vmapbuf: null page frame");
		pmap_enter(vm_map_pmap(phys_map), taddr, trunc_page(pa),
			   VM_PROT_READ|VM_PROT_WRITE, TRUE, 0);
		faddr += PAGE_SIZE;
		taddr += PAGE_SIZE;
	}
}

/*
 * Unmap IO request from the kernel virtual address space.
 */
void
vunmapbuf(bp, len)
	struct buf *bp;
	vm_size_t len;
{
	vm_offset_t addr, off;

	if ((bp->b_flags & B_PHYS) == 0)
		panic("vunmapbuf");
	addr = trunc_page(bp->b_data);
	off = (vm_offset_t)bp->b_data - addr;
	len = round_page(off + len);
	uvm_km_free_wakeup(phys_map, addr, len);
	bp->b_data = bp->b_saveaddr;
	bp->b_saveaddr = NULL;

}
