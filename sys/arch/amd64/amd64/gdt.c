/*	$OpenBSD: gdt.c,v 1.6 2005/09/25 20:48:18 miod Exp $	*/
/*	$NetBSD: gdt.c,v 1.1 2003/04/26 18:39:28 fvdl Exp $	*/

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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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
 * Modified to deal with variable-length entries for amd64 by
 * fvdl@wasabisystems.com, may 2001
 * XXX this file should be shared with the i386 code, the difference
 * can be hidden in macros.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/lock.h>
#include <sys/user.h>

#include <uvm/uvm.h>

#include <machine/gdt.h>

int gdt_size;		/* size of GDT in bytes */
int gdt_dyncount;	/* number of dyn. allocated GDT entries in use */
int gdt_dynavail;
int gdt_next;		/* next available slot for sweeping */
int gdt_free;		/* next free slot; terminated with GNULL_SEL */

struct lock gdt_lock_store;

static __inline void gdt_lock(void);
static __inline void gdt_unlock(void);
void gdt_init(void);
void gdt_grow(void);
int gdt_get_slot(void);
void gdt_put_slot(int);

/*
 * Lock and unlock the GDT, to avoid races in case gdt_{ge,pu}t_slot() sleep
 * waiting for memory.
 *
 * Note that the locking done here is not sufficient for multiprocessor
 * systems.  A freshly allocated slot will still be of type SDT_SYSNULL for
 * some time after the GDT is unlocked, so gdt_compact() could attempt to
 * reclaim it.
 */
static __inline void
gdt_lock()
{
	if (curproc != NULL)		/* XXX - ugh. needed for startup */
		(void) lockmgr(&gdt_lock_store, LK_EXCLUSIVE, NULL, curproc);
}

static __inline void
gdt_unlock()
{
	if (curproc != NULL)
		(void) lockmgr(&gdt_lock_store, LK_RELEASE, NULL, curproc);
}

void
set_mem_gdt(sd, base, limit, type, dpl, gran, def32, is64)
	struct mem_segment_descriptor *sd;
	void *base;
	size_t limit;
	int type, dpl, gran, def32, is64;
{
	CPU_INFO_ITERATOR cii;
	struct cpu_info *ci;
	int off;

        set_mem_segment(sd, base, limit, type, dpl, gran, def32, is64);
	off = (char *)sd - gdtstore;
        CPU_INFO_FOREACH(cii, ci) {
                if (ci->ci_gdt != NULL)
			*(struct mem_segment_descriptor *)(ci->ci_gdt + off) =
			    *sd;
        }
}

void
set_sys_gdt(sd, base, limit, type, dpl, gran)
	struct sys_segment_descriptor *sd;
	void *base;
	size_t limit;
	int type, dpl, gran;
{
	CPU_INFO_ITERATOR cii;
	struct cpu_info *ci;
	int off;

        set_sys_segment(sd, base, limit, type, dpl, gran);
	off = (char *)sd - gdtstore;
        CPU_INFO_FOREACH(cii, ci) {
                if (ci->ci_gdt != NULL)
			*(struct sys_segment_descriptor *)(ci->ci_gdt + off) =
			    *sd;
        }
}


/*
 * Initialize the GDT.
 */
void
gdt_init()
{
	char *old_gdt;
	struct vm_page *pg;
	vaddr_t va;
	struct cpu_info *ci = &cpu_info_primary;

	lockinit(&gdt_lock_store, PZERO, "gdtlck", 0, 0);

	gdt_size = MINGDTSIZ;
	gdt_dyncount = 0;
	gdt_next = 0;
	gdt_free = GNULL_SEL;
	gdt_dynavail =
	    (gdt_size - DYNSEL_START) / sizeof (struct sys_segment_descriptor);

	old_gdt = gdtstore;
	gdtstore = (char *)uvm_km_valloc(kernel_map, MAXGDTSIZ);
	for (va = (vaddr_t)gdtstore; va < (vaddr_t)gdtstore + gdt_size;
	    va += PAGE_SIZE) {
		pg = uvm_pagealloc(NULL, 0, NULL, UVM_PGA_ZERO);
		if (pg == NULL) {
			panic("gdt_init: no pages");
		}
		pmap_kenter_pa(va, VM_PAGE_TO_PHYS(pg),
		    VM_PROT_READ | VM_PROT_WRITE);
	}
	memcpy(gdtstore, old_gdt, DYNSEL_START);
	ci->ci_gdt = gdtstore;
	set_sys_segment(GDT_ADDR_SYS(gdtstore, GLDT_SEL), ldtstore,
	    LDT_SIZE - 1, SDT_SYSLDT, SEL_KPL, 0);

	gdt_init_cpu(ci);
}

#ifdef MULTIPROCESSOR
/*
 * Allocate shadow GDT for a slave cpu.
 */
void
gdt_alloc_cpu(struct cpu_info *ci)
{
        ci->ci_gdt = (char *)uvm_km_valloc(kernel_map, MAXGDTSIZ);
        uvm_map_pageable(kernel_map, (vaddr_t)ci->ci_gdt,
            (vaddr_t)ci->ci_gdt + MINGDTSIZ, FALSE, FALSE);
        memset(ci->ci_gdt, 0, MINGDTSIZ);
        memcpy(ci->ci_gdt, gdtstore,
	   DYNSEL_START + gdt_dyncount * sizeof(struct sys_segment_descriptor));
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

	setregion(&region, ci->ci_gdt, (u_int16_t)(MAXGDTSIZ - 1));
	lgdt(&region);
}

#ifdef MULTIPROCESSOR

void
gdt_reload_cpu(struct cpu_info *ci)
{
	struct region_descriptor region;

	setregion(&region, ci->ci_gdt, MAXGDTSIZ - 1);
	lgdt(&region);
}
#endif


/*
 * Grow or shrink the GDT.
 */
void
gdt_grow()
{
	size_t old_len;
	struct vm_page *pg;
	vaddr_t va;

	old_len = gdt_size;
	gdt_size = 2 * gdt_size;
	gdt_dynavail =
	    (gdt_size - DYNSEL_START) / sizeof (struct sys_segment_descriptor);

	for (va = (vaddr_t)gdtstore + old_len;
	    va < (vaddr_t)gdtstore + gdt_size;
	    va += PAGE_SIZE) {
		while ((pg = uvm_pagealloc(NULL, 0, NULL, UVM_PGA_ZERO)) ==
		       NULL) {
			uvm_wait("gdt_grow");
		}
		pmap_kenter_pa(va, VM_PAGE_TO_PHYS(pg),
		    VM_PROT_READ | VM_PROT_WRITE);
	}
}

/*
 * Allocate a GDT slot as follows:
 * 1) If there are entries on the free list, use those.
 * 2) If there are fewer than gdt_dynavail entries in use, there are free slots
 *    near the end that we can sweep through.
 * 3) As a last resort, we increase the size of the GDT, and sweep through
 *    the new slots.
 */
int
gdt_get_slot()
{
	int slot;
	struct sys_segment_descriptor *gdt;

	gdt = (struct sys_segment_descriptor *)&gdtstore[DYNSEL_START];

	gdt_lock();

	if (gdt_free != GNULL_SEL) {
		slot = gdt_free;
		gdt_free = gdt[slot].sd_xx3;	/* XXXfvdl res. field abuse */
	} else {
#ifdef DIAGNOSTIC
		if (gdt_next != gdt_dyncount)
			panic("gdt_get_slot botch 1");
#endif
		if (gdt_next >= gdt_dynavail) {
#ifdef DIAGNOSTIC
			if (gdt_size >= MAXGDTSIZ)
				panic("gdt_get_slot botch 2");
#endif
			gdt_grow();
		}
		slot = gdt_next++;
	}

	gdt_dyncount++;
	gdt_unlock();
	return (slot);
}

/*
 * Deallocate a GDT slot, putting it on the free list.
 */
void
gdt_put_slot(slot)
	int slot;
{
	struct sys_segment_descriptor *gdt;

	gdt = (struct sys_segment_descriptor *)&gdtstore[DYNSEL_START];

	gdt_lock();
	gdt_dyncount--;

	gdt[slot].sd_type = SDT_SYSNULL;
	gdt[slot].sd_xx3 = gdt_free;
	gdt_free = slot;

	gdt_unlock();
}

int
tss_alloc(pcb)
	struct pcb *pcb;
{
	int slot;
	struct sys_segment_descriptor *gdt;

	gdt = (struct sys_segment_descriptor *)&gdtstore[DYNSEL_START];

	slot = gdt_get_slot();
#if 0
	printf("tss_alloc: slot %d addr %p\n", slot, &gdt[slot]);
#endif
	set_sys_gdt(&gdt[slot], &pcb->pcb_tss, sizeof (struct x86_64_tss)-1,
	    SDT_SYS386TSS, SEL_KPL, 0);
#if 0
	printf("lolimit %lx lobase %lx type %lx dpl %lx p %lx hilimit %lx\n"
	       "xx1 %lx gran %lx hibase %lx xx2 %lx zero %lx xx3 %lx pad %lx\n",
		(unsigned long)gdt[slot].sd_lolimit,
		(unsigned long)gdt[slot].sd_lobase,
		(unsigned long)gdt[slot].sd_type,
		(unsigned long)gdt[slot].sd_dpl,
		(unsigned long)gdt[slot].sd_p,
		(unsigned long)gdt[slot].sd_hilimit,
		(unsigned long)gdt[slot].sd_xx1,
		(unsigned long)gdt[slot].sd_gran,
		(unsigned long)gdt[slot].sd_hibase,
		(unsigned long)gdt[slot].sd_xx2,
		(unsigned long)gdt[slot].sd_zero,
		(unsigned long)gdt[slot].sd_xx3);
#endif
	return GDYNSEL(slot, SEL_KPL);
}

void
tss_free(int sel)
{

	gdt_put_slot(IDXDYNSEL(sel));
}

void
ldt_alloc(pmap, ldt, len)
	struct pmap *pmap;
	char *ldt;
	size_t len;
{
	int slot;
	struct sys_segment_descriptor *gdt;

	gdt = (struct sys_segment_descriptor *)&gdtstore[DYNSEL_START];

	slot = gdt_get_slot();
	set_sys_gdt(&gdt[slot], ldt, len - 1, SDT_SYSLDT, SEL_KPL, 0);
	pmap->pm_ldt_sel = GSEL(slot, SEL_KPL);
}

void
ldt_free(pmap)
	struct pmap *pmap;
{
	int slot;

	slot = IDXDYNSEL(pmap->pm_ldt_sel);

	gdt_put_slot(slot);
}
