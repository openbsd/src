/*	$OpenBSD: gdt.c,v 1.35 2015/02/11 05:54:48 dlg Exp $	*/
/*	$NetBSD: gdt.c,v 1.28 2002/12/14 09:38:50 junyoung Exp $	*/

/*-
 * Copyright (c) 1996, 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by John T. Kohl and Charles M. Hannum.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * The GDT handling has two phases.  During the early lifetime of the
 * kernel there is a static gdt which will be stored in bootstrap_gdt.
 * Later, when the virtual memory is initialized, this will be
 * replaced with a maximum sized GDT.
 *
 * The bootstrap GDT area will hold the initial requirement of NGDT
 * descriptors.  The normal GDT will have a statically sized virtual memory
 * area of size MAXGDTSIZ.
 *
 * Every CPU in a system has its own copy of the GDT.  The only real difference
 * between the two are currently that there is a cpu-specific segment holding
 * the struct cpu_info of the processor, for simplicity at getting cpu_info
 * fields from assembly.  The boot processor will actually refer to the global
 * copy of the GDT as pointed to by the gdt variable.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mutex.h>

#include <uvm/uvm_extern.h>

#include <machine/gdt.h>
#include <machine/pcb.h>

union descriptor bootstrap_gdt[NGDT];
union descriptor *gdt = bootstrap_gdt;

int gdt_next;		/* next available slot for sweeping */
int gdt_free;		/* next free slot; terminated with GNULL_SEL */

struct mutex gdt_lock_store = MUTEX_INITIALIZER(IPL_HIGH);

int gdt_get_slot(void);
void gdt_put_slot(int);

/*
 * Lock and unlock the GDT.
 */
#define gdt_lock()	(mtx_enter(&gdt_lock_store))
#define gdt_unlock()	(mtx_leave(&gdt_lock_store))

/* XXX needs spinlocking if we ever mean to go finegrained. */
void
setgdt(int sel, void *base, size_t limit, int type, int dpl, int def32,
    int gran)
{
	struct segment_descriptor *sd = &gdt[sel].sd;
	CPU_INFO_ITERATOR cii;
	struct cpu_info *ci;

	KASSERT(sel < MAXGDTSIZ);

	setsegment(sd, base, limit, type, dpl, def32, gran);
	CPU_INFO_FOREACH(cii, ci)
		if (ci->ci_gdt != NULL && ci->ci_gdt != gdt)
			ci->ci_gdt[sel].sd = *sd;
}

/*
 * Initialize the GDT subsystem.  Called from autoconf().
 */
void
gdt_init()
{
	struct vm_page *pg;
	vaddr_t va;
	struct cpu_info *ci = &cpu_info_primary;

	gdt_next = NGDT;
	gdt_free = GNULL_SEL;

	gdt = (union descriptor *)uvm_km_valloc(kernel_map, MAXGDTSIZ);
	for (va = (vaddr_t)gdt; va < (vaddr_t)gdt + MAXGDTSIZ;
	    va += PAGE_SIZE) {
		pg = uvm_pagealloc(NULL, 0, NULL, UVM_PGA_ZERO);
		if (pg == NULL)
			panic("gdt_init: no pages");
		pmap_kenter_pa(va, VM_PAGE_TO_PHYS(pg),
		    PROT_READ | PROT_WRITE);
	}
	bcopy(bootstrap_gdt, gdt, NGDT * sizeof(union descriptor));
	ci->ci_gdt = gdt;
	setsegment(&ci->ci_gdt[GCPU_SEL].sd, ci, sizeof(struct cpu_info)-1,
	    SDT_MEMRWA, SEL_KPL, 0, 0);

	gdt_init_cpu(ci);
}

#ifdef MULTIPROCESSOR
/*
 * Allocate shadow GDT for a slave cpu.
 */
void
gdt_alloc_cpu(struct cpu_info *ci)
{
	struct vm_page *pg;
	vaddr_t va;

	ci->ci_gdt = (union descriptor *)uvm_km_valloc(kernel_map, MAXGDTSIZ);
	uvm_map_pageable(kernel_map, (vaddr_t)ci->ci_gdt,
	    (vaddr_t)ci->ci_gdt + MAXGDTSIZ, FALSE, FALSE);
	for (va = (vaddr_t)ci->ci_gdt; va < (vaddr_t)ci->ci_gdt + MAXGDTSIZ;
	    va += PAGE_SIZE) {
		pg = uvm_pagealloc(NULL, 0, NULL, UVM_PGA_ZERO);
		if (pg == NULL)
			panic("gdt_init: no pages");
		pmap_kenter_pa(va, VM_PAGE_TO_PHYS(pg),
		    PROT_READ | PROT_WRITE);
	}
	bzero(ci->ci_gdt, MAXGDTSIZ);
	bcopy(gdt, ci->ci_gdt, MAXGDTSIZ);
	setsegment(&ci->ci_gdt[GCPU_SEL].sd, ci, sizeof(struct cpu_info)-1,
	    SDT_MEMRWA, SEL_KPL, 0, 0);
}
#endif	/* MULTIPROCESSOR */


/*
 * Load appropriate gdt descriptor; we better be running on *ci
 * (for the most part, this is how a cpu knows who it is).
 */
void
gdt_init_cpu(struct cpu_info *ci)
{
	struct region_descriptor region;

	setregion(&region, ci->ci_gdt, MAXGDTSIZ - 1);
	lgdt(&region);
}

/*
 * Allocate a GDT slot as follows:
 * 1) If there are entries on the free list, use those.
 * 2) If there are fewer than MAXGDTSIZ entries in use, there are free slots
 *    near the end that we can sweep through.
 */
int
gdt_get_slot()
{
	int slot;

	gdt_lock();

	if (gdt_free != GNULL_SEL) {
		slot = gdt_free;
		gdt_free = gdt[slot].gd.gd_selector;
	} else {
		if (gdt_next >= MAXGDTSIZ)
			panic("gdt_get_slot: out of GDT descriptors");
		slot = gdt_next++;
	}

	gdt_unlock();
	return (slot);
}

/*
 * Deallocate a GDT slot, putting it on the free list.
 */
void
gdt_put_slot(int slot)
{

	gdt_lock();

	gdt[slot].gd.gd_type = SDT_SYSNULL;
	gdt[slot].gd.gd_selector = gdt_free;
	gdt_free = slot;

	gdt_unlock();
}

int
tss_alloc(struct pcb *pcb)
{
	int slot;

	slot = gdt_get_slot();
	setgdt(slot, &pcb->pcb_tss, sizeof(struct pcb) - 1,
	    SDT_SYS386TSS, SEL_KPL, 0, 0);
	return GSEL(slot, SEL_KPL);
}

void
tss_free(int sel)
{

	gdt_put_slot(IDXSEL(sel));
}

#ifdef USER_LDT
/*
 * Caller must have pmap locked for both of these functions.
 */
void
ldt_alloc(struct pmap *pmap, union descriptor *ldt, size_t len)
{
	int slot;

	slot = gdt_get_slot();
	setgdt(slot, ldt, len - 1, SDT_SYSLDT, SEL_KPL, 0, 0);
	pmap->pm_ldt_sel = GSEL(slot, SEL_KPL);
}

void
ldt_free(struct pmap *pmap)
{
	int slot;

	slot = IDXSEL(pmap->pm_ldt_sel);

	gdt_put_slot(slot);
}
#endif /* USER_LDT */
