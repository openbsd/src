/*	$OpenBSD: drm_linux.c,v 1.10 2016/04/05 20:44:03 kettenis Exp $	*/
/*
 * Copyright (c) 2013 Jonathan Gray <jsg@openbsd.org>
 * Copyright (c) 2015, 2016 Mark Kettenis <kettenis@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <dev/pci/drm/drmP.h>
#include <dev/pci/ppbreg.h>

struct timespec
ns_to_timespec(const int64_t nsec)
{
	struct timespec ts;
	int32_t rem;

	if (nsec == 0) {
		ts.tv_sec = 0;
		ts.tv_nsec = 0;
		return (ts);
	}

	ts.tv_sec = nsec / NSEC_PER_SEC;
	rem = nsec % NSEC_PER_SEC;
	if (rem < 0) {
		ts.tv_sec--;
		rem += NSEC_PER_SEC;
	}
	ts.tv_nsec = rem;
	return (ts);
}

int64_t
timeval_to_ns(const struct timeval *tv)
{
	return ((int64_t)tv->tv_sec * NSEC_PER_SEC) +
		tv->tv_usec * NSEC_PER_USEC;
}

struct timeval
ns_to_timeval(const int64_t nsec)
{
	struct timeval tv;
	int32_t rem;

	if (nsec == 0) {
		tv.tv_sec = 0;
		tv.tv_usec = 0;
		return (tv);
	}

	tv.tv_sec = nsec / NSEC_PER_SEC;
	rem = nsec % NSEC_PER_SEC;
	if (rem < 0) {
		tv.tv_sec--;
		rem += NSEC_PER_SEC;
	}
	tv.tv_usec = rem / 1000;
	return (tv);
}

extern char *hw_vendor, *hw_prod;

static bool
dmi_found(const struct dmi_system_id *dsi)
{
	int i, slot;

	for (i = 0; i < nitems(dsi->matches); i++) {
		slot = dsi->matches[i].slot;
		switch (slot) {
		case DMI_NONE:
			break;
		case DMI_SYS_VENDOR:
		case DMI_BOARD_VENDOR:
			if (hw_vendor != NULL &&
			    !strcmp(hw_vendor, dsi->matches[i].substr))
				break;
			else
				return false;
		case DMI_PRODUCT_NAME:
		case DMI_BOARD_NAME:
			if (hw_prod != NULL &&
			    !strcmp(hw_prod, dsi->matches[i].substr))
				break;
			else
				return false;
		default:
			return false;
		}
	}

	return true;
}

int
dmi_check_system(const struct dmi_system_id *sysid)
{
	const struct dmi_system_id *dsi;
	int num = 0;

	for (dsi = sysid; dsi->matches[0].slot != 0 ; dsi++) {
		if (dmi_found(dsi)) {
			num++;
			if (dsi->callback && dsi->callback(dsi))
				break;
		}
	}
	return (num);
}

struct vm_page *
alloc_pages(unsigned int gfp_mask, unsigned int order)
{
	int flags = (gfp_mask & M_NOWAIT) ? UVM_PLA_NOWAIT : UVM_PLA_WAITOK;
	struct pglist mlist;

	if (gfp_mask & M_CANFAIL)
		flags |= UVM_PLA_FAILOK;

	TAILQ_INIT(&mlist);
	if (uvm_pglistalloc(PAGE_SIZE << order, 0, -1, PAGE_SIZE, 0,
	    &mlist, 1, flags))
		return NULL;
	return TAILQ_FIRST(&mlist);
}

void
__free_pages(struct vm_page *page, unsigned int order)
{
	struct pglist mlist;
	int i;
	
	TAILQ_INIT(&mlist);
	for (i = 0; i < (1 << order); i++)
		TAILQ_INSERT_TAIL(&mlist, &page[i], pageq);
	uvm_pglistfree(&mlist);
}

void *
kmap(struct vm_page *pg)
{
	vaddr_t va;

#if defined (__HAVE_PMAP_DIRECT)
	va = pmap_map_direct(pg);
#else
	va = uvm_km_valloc_wait(phys_map, PAGE_SIZE);
	pmap_kenter_pa(va, VM_PAGE_TO_PHYS(pg), PROT_READ | PROT_WRITE);
	pmap_update(pmap_kernel());
#endif
	return (void *)va;
}

void
kunmap(void *addr)
{
	vaddr_t va = (vaddr_t)addr;

#if defined (__HAVE_PMAP_DIRECT)
	pmap_unmap_direct(va);
#else
	pmap_kremove(va, PAGE_SIZE);
	pmap_update(pmap_kernel());
	uvm_km_free_wakeup(phys_map, va, PAGE_SIZE);
#endif
}

void *
vmap(struct vm_page **pages, unsigned int npages, unsigned long flags,
     pgprot_t prot)
{
	vaddr_t va;
	paddr_t pa;
	int i;

	va = uvm_km_valloc(kernel_map, PAGE_SIZE * npages);
	if (va == 0)
		return NULL;
	for (i = 0; i < npages; i++) {
		pa = VM_PAGE_TO_PHYS(pages[i]) | prot;
		pmap_enter(pmap_kernel(), va + (i * PAGE_SIZE), pa,
		    PROT_READ | PROT_WRITE,
		    PROT_READ | PROT_WRITE | PMAP_WIRED);
		pmap_update(pmap_kernel());
	}

	return (void *)va;
}

void
vunmap(void *addr, size_t size)
{
	vaddr_t va = (vaddr_t)addr;

	pmap_remove(pmap_kernel(), va, va + size);
	pmap_update(pmap_kernel());
	uvm_km_free(kernel_map, va, size);
}

int
panic_cmp(struct rb_node *a, struct rb_node *b)
{
	panic(__func__);
}

#undef RB_ROOT
#define RB_ROOT(head)	(head)->rbh_root

RB_GENERATE(linux_root, rb_node, __entry, panic_cmp);

/*
 * This is a fairly minimal implementation of the Linux "idr" API.  It
 * probably isn't very efficient, and defenitely isn't RCU safe.  The
 * pre-load buffer is global instead of per-cpu; we rely on the kernel
 * lock to make this work.  We do randomize our IDs in order to make
 * them harder to guess.
 */

int idr_cmp(struct idr_entry *, struct idr_entry *);
SPLAY_PROTOTYPE(idr_tree, idr_entry, entry, idr_cmp);

struct pool idr_pool;
struct idr_entry *idr_entry_cache;

void
idr_init(struct idr *idr)
{
	static int initialized;

	if (!initialized) {
		pool_init(&idr_pool, sizeof(struct idr_entry), 0, 0, 0,
		    "idrpl", NULL);
		pool_setipl(&idr_pool, IPL_TTY);
		initialized = 1;
	}
	SPLAY_INIT(&idr->tree);
}

void
idr_destroy(struct idr *idr)
{
	struct idr_entry *id;

	while ((id = SPLAY_MIN(idr_tree, &idr->tree))) {
		SPLAY_REMOVE(idr_tree, &idr->tree, id);
		pool_put(&idr_pool, id);
	}
}

void
idr_preload(unsigned int gfp_mask)
{
	int flags = (gfp_mask & GFP_NOWAIT) ? PR_NOWAIT : PR_WAITOK;

	KERNEL_ASSERT_LOCKED();

	if (idr_entry_cache == NULL)
		idr_entry_cache = pool_get(&idr_pool, flags);
}

int
idr_alloc(struct idr *idr, void *ptr, int start, int end,
    unsigned int gfp_mask)
{
	int flags = (gfp_mask & GFP_NOWAIT) ? PR_NOWAIT : PR_WAITOK;
	struct idr_entry *id;

	KERNEL_ASSERT_LOCKED();

	if (idr_entry_cache) {
		id = idr_entry_cache;
		idr_entry_cache = NULL;
	} else {
		id = pool_get(&idr_pool, flags);
		if (id == NULL)
			return -ENOMEM;
	}

	if (end <= 0)
		end = INT_MAX;

	id->id = start + arc4random_uniform(end - start);
	while (SPLAY_INSERT(idr_tree, &idr->tree, id)) {
		if (++id->id == end)
			id->id = start;
	}
	id->ptr = ptr;
	return id->id;
}

void
idr_remove(struct idr *idr, int id)
{
	struct idr_entry find, *res;

	find.id = id;
	res = SPLAY_FIND(idr_tree, &idr->tree, &find);
	if (res) {
		SPLAY_REMOVE(idr_tree, &idr->tree, res);
		pool_put(&idr_pool, res);
	}
}

void *
idr_find(struct idr *idr, int id)
{
	struct idr_entry find, *res;

	find.id = id;
	res = SPLAY_FIND(idr_tree, &idr->tree, &find);
	if (res == NULL)
		return NULL;
	return res->ptr;
}

int
idr_for_each(struct idr *idr, int (*func)(int, void *, void *), void *data)
{
	struct idr_entry *id;
	int ret;

	SPLAY_FOREACH(id, idr_tree, &idr->tree) {
		ret = func(id->id, id->ptr, data);
		if (ret)
			return ret;
	}

	return 0;
}

int
idr_cmp(struct idr_entry *a, struct idr_entry *b)
{
	return (a->id < b->id ? -1 : a->id > b->id);
}

SPLAY_GENERATE(idr_tree, idr_entry, entry, idr_cmp);

#if defined(__amd64__) || defined(__i386__)

/*
 * This is a minimal implementation of the Linux vga_get/vga_put
 * interface.  In all likelyhood, it will only work for inteldrm(4) as
 * it assumes that if there is another active VGA device in the
 * system, it is sitting behind a PCI bridge.
 */

extern int pci_enumerate_bus(struct pci_softc *,
    int (*)(struct pci_attach_args *), struct pci_attach_args *);

pcitag_t vga_bridge_tag;
int vga_bridge_disabled;

int
vga_disable_bridge(struct pci_attach_args *pa)
{
	pcireg_t bhlc, bc;

	if (pa->pa_domain != 0)
		return 0;

	bhlc = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_BHLC_REG);
	if (PCI_HDRTYPE_TYPE(bhlc) != 1)
		return 0;

	bc = pci_conf_read(pa->pa_pc, pa->pa_tag, PPB_REG_BRIDGECONTROL);
	if ((bc & PPB_BC_VGA_ENABLE) == 0)
		return 0;
	bc &= ~PPB_BC_VGA_ENABLE;
	pci_conf_write(pa->pa_pc, pa->pa_tag, PPB_REG_BRIDGECONTROL, bc);

	vga_bridge_tag = pa->pa_tag;
	vga_bridge_disabled = 1;

	return 1;
}

void
vga_get_uninterruptible(struct pci_dev *pdev, int rsrc)
{
	KASSERT(pdev->pci->sc_bridgetag == NULL);
	pci_enumerate_bus(pdev->pci, vga_disable_bridge, NULL);
}

void
vga_put(struct pci_dev *pdev, int rsrc)
{
	pcireg_t bc;

	if (!vga_bridge_disabled)
		return;

	bc = pci_conf_read(pdev->pc, vga_bridge_tag, PPB_REG_BRIDGECONTROL);
	bc |= PPB_BC_VGA_ENABLE;
	pci_conf_write(pdev->pc, vga_bridge_tag, PPB_REG_BRIDGECONTROL, bc);

	vga_bridge_disabled = 0;
}

#endif

/*
 * ACPI types and interfaces.
 */

#if defined(__amd64__) || defined(__i386__)
#include "acpi.h"
#endif

#if NACPI > 0

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>

acpi_status
acpi_get_table_with_size(const char *sig, int instance,
    struct acpi_table_header **hdr, acpi_size *size)
{
	struct acpi_softc *sc = acpi_softc;
	struct acpi_q *entry;

	KASSERT(instance == 1);

	SIMPLEQ_FOREACH(entry, &sc->sc_tables, q_next) {
		if (memcmp(entry->q_table, sig, strlen(sig)) == 0) {
			*hdr = entry->q_table;
			*size = (*hdr)->length;
			return 0;
		}
	}

	return AE_NOT_FOUND;
}

#endif
