/* $NetBSD: irongate_bus_mem.c,v 1.6 2000/11/29 06:29:10 thorpej Exp $ */

/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/syslog.h>
#include <sys/device.h>

#include <vm/vm.h>
#include <uvm/uvm_extern.h>

#include <machine/bus.h>

#include <alpha/pci/irongatereg.h>
#include <alpha/pci/irongatevar.h>

#define	CHIP		irongate

#define	CHIP_EX_MALLOC_SAFE(v)	(((struct irongate_config *)(v))->ic_mallocsafe)
#define	CHIP_MEM_EXTENT(v)	(((struct irongate_config *)(v))->ic_mem_ex)

#define	CHIP_MEM_SYS_START(v)	IRONGATE_MEM_BASE

/* 
 * AMD 751 core logic appears on EV6.  We require at least EV56 
 * support for the assembler to emit BWX opcodes. 
 */
__asm(".arch ev6");

#include <alpha/pci/pci_bwx_bus_mem_chipdep.c>

#include <sys/kcore.h>

extern phys_ram_seg_t mem_clusters[];
extern int mem_cluster_cnt;

void
irongate_bus_mem_init2(bus_space_tag_t t, void *v)
{
	int i, error;

	/*
	 * Since the AMD 751 doesn't have DMA windows, we need to
	 * allocate RAM out of the extent map.
	 */
	for (i = 0; i < mem_cluster_cnt; i++) {
		error = extent_alloc_region(CHIP_MEM_EXTENT(v),
		    mem_clusters[i].start, mem_clusters[i].size,
		    EX_NOWAIT | (CHIP_EX_MALLOC_SAFE(v) ? EX_MALLOCOK : 0));
		if (error) {
			printf("WARNING: unable reserve RAM at 0x%lx - 0x%lx\n",
			    mem_clusters[i].start,
			    mem_clusters[i].start + (mem_clusters[i].size - 1));
		}
	}
}
