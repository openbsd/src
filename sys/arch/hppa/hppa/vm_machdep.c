/*	$OpenBSD: vm_machdep.c,v 1.3 1999/02/25 19:08:50 mickey Exp $	*/

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/vnode.h>
#include <sys/user.h>

#include <vm/vm_kern.h>
#include <machine/pmap.h>

#include <vm/vm.h>


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
	return EIO;
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
		pmap_enter(pmap_kernel(),
			   (vm_offset_t)to, pa, VM_PROT_READ|VM_PROT_WRITE, 1);
		from += PAGE_SIZE;
		to += PAGE_SIZE;
		size -= PAGE_SIZE;
	}
}

void
cpu_swapin(p)
	struct proc *p;
{

}

void
cpu_swapout(p)
	struct proc *p;
{

}

void
cpu_fork(p1, p2)
	struct proc *p1, *p2;
{
	p2->p_md.md_flags = p1->p_md.md_flags;

	p2->p_addr->u_pcb = p1->p_addr->u_pcb;

	/* TODO: create the child's kernel stack */
}

void
cpu_exit(p)
	struct proc *p;
{
	/* TODO: anything to be done about FPU */

	vmspace_free(p->p_vmspace);
	/* to the switch */
}


void
cpu_wait(p)
	struct proc *p;
{

}

void
cpu_set_kpc(p, pc, arg)
	struct proc *p;
	void (*pc) __P((void *));
	void *arg;
{

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
	taddr = kmem_alloc_wait(phys_map, len);
	bp->b_data = (caddr_t)(taddr + off);
	len = atop(len);
	while (len--) {
		pa = pmap_extract(vm_map_pmap(&p->p_vmspace->vm_map), faddr);
		if (pa == 0)
			panic("vmapbuf: null page frame");
		pmap_enter(vm_map_pmap(phys_map), taddr, trunc_page(pa),
			   VM_PROT_READ|VM_PROT_WRITE, TRUE);
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
	kmem_free_wakeup(phys_map, addr, len);
	bp->b_data = bp->b_saveaddr;
	bp->b_saveaddr = NULL;

}
