/*	$OpenBSD: gdt.c,v 1.20 2003/11/08 05:38:33 nordin Exp $	*/
/*	$NetBSD: gdt.c,v 1.8 1996/05/03 19:42:06 christos Exp $	*/

/*-
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/user.h>

#include <uvm/uvm_extern.h>

#include <machine/gdt.h>

#define	MINGDTSIZ	512
#define	MAXGDTSIZ	8192

union descriptor *dynamic_gdt = gdt;
int gdt_size = NGDT;		/* total number of GDT entries */
int gdt_count = NGDT;		/* number of GDT entries in use */
int gdt_next = NGDT;		/* next available slot for sweeping */
int gdt_free = GNULL_SEL;	/* next free slot; terminated with GNULL_SEL */

int gdt_flags;
#define	GDT_LOCKED	0x1
#define	GDT_WANTED	0x2

static __inline void gdt_lock(void);
static __inline void gdt_unlock(void);
void gdt_grow(void);
int gdt_get_slot(void);
void gdt_put_slot(int);

/*
 * Lock and unlock the GDT, to avoid races in case gdt_{ge,pu}t_slot() sleep
 * waiting for memory.
 */
static __inline void
gdt_lock()
{

	while ((gdt_flags & GDT_LOCKED) != 0) {
		gdt_flags |= GDT_WANTED;
		tsleep(&gdt_flags, PZERO, "gdtlck", 0);
	}
	gdt_flags |= GDT_LOCKED;
}

static __inline void
gdt_unlock()
{

	gdt_flags &= ~GDT_LOCKED;
	if ((gdt_flags & GDT_WANTED) != 0) {
		gdt_flags &= ~GDT_WANTED;
		wakeup(&gdt_flags);
	}
}

/*
 * Initialize the GDT subsystem.  Called from autoconf().
 */
void
gdt_init()
{
	size_t max_len, min_len;
	struct region_descriptor region;

	max_len = MAXGDTSIZ * sizeof(union descriptor);
	min_len = MINGDTSIZ * sizeof(union descriptor);
	gdt_size = MINGDTSIZ;

	dynamic_gdt = (union descriptor *)uvm_km_valloc(kernel_map, max_len);
	uvm_map_pageable(kernel_map, (vaddr_t)dynamic_gdt,
	    (vaddr_t)dynamic_gdt + min_len, FALSE, FALSE);
	bcopy(gdt, dynamic_gdt, NGDT * sizeof(union descriptor));

	setregion(&region, dynamic_gdt, max_len - 1);
	lgdt(&region);
}

/*
 * Grow the GDT.
 */
void
gdt_grow()
{
	size_t old_len, new_len;

	old_len = gdt_size * sizeof(union descriptor);
	gdt_size <<= 1;
	new_len = old_len << 1;

	uvm_map_pageable(kernel_map, (vaddr_t)dynamic_gdt + old_len,
	    (vaddr_t)dynamic_gdt + new_len, FALSE, FALSE);
}

/*
 * Allocate a GDT slot as follows:
 * 1) If there are entries on the free list, use those.
 * 2) If there are fewer than gdt_size entries in use, there are free slots
 *    near the end that we can sweep through.
 * 3) As a last resort, we increase the size of the GDT, and sweep through
 *    the new slots.
 */
int
gdt_get_slot()
{
	int slot;

	gdt_lock();

	if (gdt_free != GNULL_SEL) {
		slot = gdt_free;
		gdt_free = dynamic_gdt[slot].gd.gd_selector;
	} else {
		if (gdt_next != gdt_count)
			panic("gdt_get_slot: gdt_next != gdt_count");
		if (gdt_next >= gdt_size) {
			if (gdt_size >= MAXGDTSIZ)
				panic("gdt_get_slot: out of GDT descriptors");
			if (dynamic_gdt == gdt)
				panic("gdt_get_slot called before gdt_init");
			gdt_grow();
		}
		slot = gdt_next++;
	}

	gdt_count++;
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

	gdt_lock();
	gdt_count--;

	dynamic_gdt[slot].gd.gd_type = SDT_SYSNULL;
	dynamic_gdt[slot].gd.gd_selector = gdt_free;
	gdt_free = slot;

	gdt_unlock();
}

void
tss_alloc(pcb)
	struct pcb *pcb;
{
	int slot;

	slot = gdt_get_slot();
	setsegment(&dynamic_gdt[slot].sd, &pcb->pcb_tss, sizeof(struct pcb) - 1,
	    SDT_SYS386TSS, SEL_KPL, 0, 0);
	pcb->pcb_tss_sel = GSEL(slot, SEL_KPL);
}

void
tss_free(pcb)
	struct pcb *pcb;
{

	gdt_put_slot(IDXSEL(pcb->pcb_tss_sel));
}

void
ldt_alloc(pmap, ldt, len)
	struct pmap *pmap;
	union descriptor *ldt;
	size_t len;
{
	int slot;

	slot = gdt_get_slot();
	setsegment(&dynamic_gdt[slot].sd, ldt, len - 1, SDT_SYSLDT, SEL_KPL, 0,
	    0);
	simple_lock(&pmap->pm_lock);
	pmap->pm_ldt_sel = GSEL(slot, SEL_KPL);
	simple_unlock(&pmap->pm_lock);
}

void
ldt_free(pmap)
	struct pmap *pmap;
{
	int slot;

	simple_lock(&pmap->pm_lock);
	slot = IDXSEL(pmap->pm_ldt_sel);
	simple_unlock(&pmap->pm_lock);

	gdt_put_slot(slot);
}
