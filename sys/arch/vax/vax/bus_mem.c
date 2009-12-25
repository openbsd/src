/*	$OpenBSD: bus_mem.c,v 1.6 2009/12/25 20:52:57 miod Exp $	*/
/*	$NetBSD: bus_mem.c,v 1.8 2000/06/29 07:14:23 mrg Exp $ */
/*
 * Copyright (c) 1998 Matt Thomas
 * All rights reserved.
 *
 * This code is derived from software contributed to Ludd by Bertram Barth.
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
 *	This product includes software developed at Ludd, University of 
 *	Lule}, Sweden and its contributors.
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

#include <sys/param.h>
#include <sys/types.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/systm.h>

#include <uvm/uvm_extern.h>

#include <machine/cpu.h>
#include <machine/pmap.h>
#include <machine/bus.h>
#include <machine/intr.h>

int	 vax_mem_bus_space_map(void *, bus_addr_t, bus_size_t, int,
	    bus_space_handle_t *, int);
void	 vax_mem_bus_space_unmap(void *, bus_space_handle_t, bus_size_t, int);
int	 vax_mem_bus_space_subregion(void *, bus_space_handle_t, bus_size_t,
	    bus_size_t, bus_space_handle_t *);
int	 vax_mem_bus_space_alloc(void *, bus_addr_t, bus_addr_t, bus_size_t,
	    bus_size_t, bus_size_t, int, bus_addr_t *, bus_space_handle_t *);
void	 vax_mem_bus_space_free(void *, bus_space_handle_t, bus_size_t);
void	*vax_mem_bus_space_vaddr(void *, bus_space_handle_t);

int
vax_mem_bus_space_map(void *t, bus_addr_t pa, bus_size_t size, int flags,
    bus_space_handle_t *bshp, int f2)
{
	vaddr_t va;

	size += (pa & VAX_PGOFSET);	/* have to include the byte offset */
	va = uvm_km_valloc(kernel_map, size);
	if (va == 0)
		return (ENOMEM);

	*bshp = (bus_space_handle_t)(va + (pa & VAX_PGOFSET));

	ioaccess(va, pa, (size + VAX_NBPG - 1) >> VAX_PGSHIFT);

	return 0;   
} 

int
vax_mem_bus_space_subregion(void *t, bus_space_handle_t h, bus_size_t o,
    bus_size_t s, bus_space_handle_t *hp)
{
	*hp = h + o;
	return (0);             
}

void
vax_mem_bus_space_unmap(void *t, bus_space_handle_t h, bus_size_t size, int f)
{
	u_long va = trunc_page(h);
	u_long endva = round_page(h + size);

        /* 
         * Free the kernel virtual mapping.
         */
	iounaccess(va, size >> VAX_PGSHIFT);
	uvm_km_free(kernel_map, va, endva - va);
}

int
vax_mem_bus_space_alloc(void *t, bus_addr_t rs, bus_addr_t re, bus_size_t s,
    bus_size_t a, bus_size_t b, int f, bus_addr_t *ap, bus_space_handle_t *hp)
{
	panic("vax_mem_bus_alloc not implemented");
}

void
vax_mem_bus_space_free(void *t, bus_space_handle_t h, bus_size_t s)
{    
	panic("vax_mem_bus_free not implemented");
}

void *
vax_mem_bus_space_vaddr(void *t, bus_space_handle_t h)
{
	return ((void *)h);
}
	
struct vax_bus_space vax_mem_bus_space = {
	NULL,
	vax_mem_bus_space_map,
	vax_mem_bus_space_unmap,
	vax_mem_bus_space_subregion,
	vax_mem_bus_space_alloc,
	vax_mem_bus_space_free,
	vax_mem_bus_space_vaddr
};
