/*	$OpenBSD: vm_machdep.c,v 1.2 1999/01/10 13:34:18 niklas Exp $	*/

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/vnode.h>
#include <sys/user.h>

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
}

void
cpu_exit(p)
	struct proc *p;
{

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

void
vmapbuf(bp, len)
	struct buf *bp;
	vm_size_t len;
{

}

void
vunmapbuf(bp, len)
	struct buf *bp;
	vm_size_t len;
{

}
