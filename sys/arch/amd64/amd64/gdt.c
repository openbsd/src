/*	$OpenBSD: gdt.c,v 1.23 2015/03/14 03:38:46 jsg Exp $	*/
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
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mutex.h>

#include <uvm/uvm_extern.h>

#include <machine/tss.h>
#include <machine/pcb.h>

/*
 * Allocate shadow GDT for a slave cpu.
 */
void
gdt_alloc_cpu(struct cpu_info *ci)
{
	struct vm_page *pg;
	vaddr_t va;

	ci->ci_gdt = (char *)uvm_km_valloc(kernel_map,
	    GDT_SIZE + sizeof(*ci->ci_tss));
	ci->ci_tss = (void *)(ci->ci_gdt + GDT_SIZE);
	uvm_map_pageable(kernel_map, (vaddr_t)ci->ci_gdt,
            (vaddr_t)ci->ci_gdt + GDT_SIZE, FALSE, FALSE);
	for (va = (vaddr_t)ci->ci_gdt;
	    va < (vaddr_t)ci->ci_gdt + GDT_SIZE + sizeof(*ci->ci_tss);
	    va += PAGE_SIZE) {
		pg = uvm_pagealloc(NULL, 0, NULL, UVM_PGA_ZERO);
		if (pg == NULL)
			panic("gdt_init: no pages");
		pmap_kenter_pa(va, VM_PAGE_TO_PHYS(pg), PROT_READ | PROT_WRITE);
	}
	memcpy(ci->ci_gdt, gdtstore, GDT_SIZE);
	bzero(ci->ci_tss, sizeof(*ci->ci_tss));
}


/*
 * Load appropriate gdt descriptor; we better be running on *ci
 * (for the most part, this is how a cpu knows who it is).
 */
void
gdt_init_cpu(struct cpu_info *ci)
{
	struct region_descriptor region;

	set_sys_segment(GDT_ADDR_SYS(ci->ci_gdt, GPROC0_SEL), ci->ci_tss,
	    sizeof (struct x86_64_tss)-1, SDT_SYS386TSS, SEL_KPL, 0);

	setregion(&region, ci->ci_gdt, GDT_SIZE - 1);
	lgdt(&region);
	ltr(GSYSSEL(GPROC0_SEL, SEL_KPL));
}
