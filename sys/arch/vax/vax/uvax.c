/*	$OpenBSD: uvax.c,v 1.4 1999/01/11 05:12:08 millert Exp $ */
/*	$NetBSD: uvax.c,v 1.4 1997/02/19 10:04:27 ragge Exp $ */
/*
 * Copyright (c) 1996 Ludd, University of Lule}, Sweden.
 * Copyright (c) 1982, 1988, 1990, 1993
 * 	The Regents of the University of California.  All rights reserved.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * generic(?) MicroVAX and VAXstation support
 *
 * There are similarities to struct cpu_calls[] in autoconf.c
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/device.h>
#include <vm/vm.h>
#include <vm/vm_kern.h>

#include <machine/pte.h>
#include <machine/mtpr.h>
#include <machine/sid.h>
#include <machine/pmap.h>
#include <machine/nexus.h>
#include <machine/uvax.h>
#include <machine/cpu.h>

/*
 * NB: mapping should/must be done in chunks of PAGE_SIZE (ie. 1024), 
 *     while pmap_map() expects size to be in chunks of NBPG (ie. 512).
 * 
 * Thus we round down the start-address to be aligned wrt PAGE_SIZE and
 * the end-address up to be just beyond the next multiple of PAGE_SIZE.
 * size is the number of bytes between start and end expressed in NBPG.
 */
void 
uvax_fillmap()
{
	extern  vm_offset_t avail_start, virtual_avail, avail_end;
	register struct uc_map *p;
	register u_int base, end, off, size;

	for (p = dep_call->cpu_map; p->um_base != 0; p++) {
		base = TRUNC_PAGE(p->um_base);		/* round base down */
		off = p->um_base - base;
		size = ROUND_PAGE(off + p->um_size);
		if (size < PAGE_SIZE) {
			printf("invalid size %d in uVAX_fillmap\n", size);
			size = PAGE_SIZE;
		}
		end = base + size - 1;
		MAPVIRT(p->um_virt, size/NBPG);
		pmap_map((vm_offset_t)p->um_virt, base, end, 
		    VM_PROT_READ|VM_PROT_WRITE);

	}
}

u_long
uvax_phys2virt(phys)
	u_long phys;
{
	register struct uc_map *p;
	u_long virt = 0;

	for (p = dep_call->cpu_map; p->um_base != 0; p++) {
		if (p->um_base > phys || p->um_end < phys)
			continue;
		virt = p->um_virt + (phys - trunc_page(p->um_base));
		break;
	}

#ifdef DIAGNOSTIC
	if (virt == 0)
		panic("invalid argument %p to uvax_phys2virt()", phys);
#endif
	return (virt);
}
