#include <sys/param.h>

#include <uvm/uvm_extern.h>

#include <machine/pmap.h>
#include <machine/pte.h>

void
pmap_virtual_space(vaddr_t *start, vaddr_t *end)
{
}

pmap_t
pmap_create(void)
{
	return NULL;
}

void
pmap_reference(pmap_t pm)
{
}

void
pmap_destroy(pmap_t pm)
{
}

void
pmap_init(void)
{
}

void
pmap_copy(pmap_t dst_pmap, pmap_t src_pmap, vaddr_t dst_addr,
    vsize_t len, vaddr_t src_addr)
{
}

int
pmap_enter(pmap_t pm, vaddr_t va, paddr_t pa, vm_prot_t prot, int flags)
{
	return ENOMEM;
}

void
pmap_remove(pmap_t pm, vaddr_t sva, vaddr_t eva)
{
}

void
pmap_protect(pmap_t pm, vaddr_t sva, vaddr_t eva, vm_prot_t prot)
{
}

void
pmap_page_protect(struct vm_page *pg, vm_prot_t prot)
{
}

void
pmap_kenter_pa(vaddr_t va, paddr_t pa, vm_prot_t prot)
{
}

void
pmap_kremove(vaddr_t va, vsize_t len)
{
}

int
pmap_is_referenced(struct vm_page *pg)
{
	return 0;
}

int
pmap_is_modified(struct vm_page *pg)
{
	return 0;
}

int
pmap_clear_reference(struct vm_page *pg)
{
	return 0;
}

int
pmap_clear_modify(struct vm_page *pg)
{
	return 0;
}

int
pmap_extract(pmap_t pm, vaddr_t va, paddr_t *pa)
{
	return 0;
}

void
pmap_activate(struct proc *p)
{
}

void
pmap_deactivate(struct proc *p)
{
}

void
pmap_unwire(pmap_t pm, vaddr_t va)
{
}

void
pmap_collect(pmap_t pm)
{
}

void
pmap_zero_page(struct vm_page *pg)
{
}

void
pmap_copy_page(struct vm_page *srcpg, struct vm_page *dstpg)
{
}

void
pmap_proc_iflush(struct process *pr, vaddr_t va, vsize_t len)
{
}
