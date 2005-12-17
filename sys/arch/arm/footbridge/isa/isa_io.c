/*	$OpenBSD: isa_io.c,v 1.2 2005/12/17 07:31:25 miod Exp $	*/
/*	$NetBSD: isa_io.c,v 1.2 2002/09/27 15:35:44 provos Exp $	*/

/*
 * Copyright 1997
 * Digital Equipment Corporation. All rights reserved.
 *
 * This software is furnished under license and may be used and
 * copied only in accordance with the following terms and conditions.
 * Subject to these conditions, you may download, copy, install,
 * use, modify and distribute this software in source and/or binary
 * form. No title or ownership is transferred hereby.
 *
 * 1) Any source code used, modified or distributed must reproduce
 *    and retain this copyright notice and list of conditions as
 *    they appear in the source file.
 *
 * 2) No right is granted to use any trade name, trademark, or logo of
 *    Digital Equipment Corporation. Neither the "Digital Equipment
 *    Corporation" name nor any trademark or logo of Digital Equipment
 *    Corporation may be used to endorse or promote products derived
 *    from this software without the prior written permission of
 *    Digital Equipment Corporation.
 *
 * 3) This software is provided "AS-IS" and any express or implied
 *    warranties, including but not limited to, any implied warranties
 *    of merchantability, fitness for a particular purpose, or
 *    non-infringement are disclaimed. In no event shall DIGITAL be
 *    liable for any damages whatsoever, and in particular, DIGITAL
 *    shall not be liable for special, indirect, consequential, or
 *    incidental damages or damages for lost profits, loss of
 *    revenue or loss of use, whether such damages arise in contract,
 *    negligence, tort, under statute, in equity, at law or otherwise,
 *    even if advised of the possibility of such damage.
 */

/*
 * bus_space I/O functions for isa
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <machine/bus.h>
#include <machine/pio.h>
#include <machine/isa_machdep.h>

/* Proto types for all the bus_space structure functions */

bs_protos(isa);
bs_protos(bs_notimpl);

/*
 * Declare the isa bus space tags
 * The IO and MEM structs are identical, except for the cookies,
 * which contain the address space bases.
 */

/*
 * NOTE: ASSEMBLY LANGUAGE RELIES ON THE COOKIE -- THE FIRST MEMBER OF 
 *       THIS STRUCTURE -- TO BE THE VIRTUAL ADDRESS OF ISA/IO!
 */
struct bus_space isa_io_bs_tag = {
	/* cookie */
	NULL, /* initialized below */

	/* mapping/unmapping */
	isa_bs_map,
	isa_bs_unmap,
	isa_bs_subregion,

	/* allocation/deallocation */
	isa_bs_alloc,
	isa_bs_free,

	/* get kernel virtual address */
	isa_bs_vaddr,

	/* mmap bus space for userland */
	bs_notimpl_bs_mmap,		/* XXX possible even? XXX */

	/* barrier */
	isa_bs_barrier,

	/* read (single) */
	isa_bs_r_1,
	isa_bs_r_2,
	isa_bs_r_4,
	bs_notimpl_bs_r_8,

	/* read multiple */
	isa_bs_rm_1,
	isa_bs_rm_2,
	isa_bs_rm_4,
	bs_notimpl_bs_rm_8,

	/* read region */
	isa_bs_rr_1,
	isa_bs_rr_2,
	isa_bs_rr_4,
	bs_notimpl_bs_rr_8,

	/* write (single) */
	isa_bs_w_1,
	isa_bs_w_2,
	isa_bs_w_4,
	bs_notimpl_bs_w_8,

	/* write multiple */
	isa_bs_wm_1,
	isa_bs_wm_2,
	isa_bs_wm_4,
	bs_notimpl_bs_wm_8,

	/* write region */
	isa_bs_wr_1,
	isa_bs_wr_2,
	isa_bs_wr_4,
	bs_notimpl_bs_wr_8,

	/* set multiple */
	bs_notimpl_bs_sm_1,
	bs_notimpl_bs_sm_2,
	bs_notimpl_bs_sm_4,
	bs_notimpl_bs_sm_8,

	/* set region */
	bs_notimpl_bs_sr_1,
	isa_bs_sr_2,
	bs_notimpl_bs_sr_4,
	bs_notimpl_bs_sr_8,

	/* copy */
	bs_notimpl_bs_c_1,
	bs_notimpl_bs_c_2,
	bs_notimpl_bs_c_4,
	bs_notimpl_bs_c_8,
};

/*
 * NOTE: ASSEMBLY LANGUAGE RELIES ON THE COOKIE -- THE FIRST MEMBER OF 
 *       THIS STRUCTURE -- TO BE THE VIRTUAL ADDRESS OF ISA/MEMORY!
 */
struct bus_space isa_mem_bs_tag = {
	/* cookie */
        NULL, /* initialized below */

	/* mapping/unmapping */
	isa_bs_map,
	isa_bs_unmap,
	isa_bs_subregion,

	/* allocation/deallocation */
	isa_bs_alloc,
	isa_bs_free,

	/* get kernel virtual address */
	isa_bs_vaddr,

	/* mmap bus space for userland */
	bs_notimpl_bs_mmap,		/* XXX open for now ... XXX */

	/* barrier */
	isa_bs_barrier,

	/* read (single) */
	isa_bs_r_1,
	isa_bs_r_2,
	isa_bs_r_4,
	bs_notimpl_bs_r_8,

	/* read multiple */
	isa_bs_rm_1,
	isa_bs_rm_2,
	isa_bs_rm_4,
	bs_notimpl_bs_rm_8,

	/* read region */
	isa_bs_rr_1,
	isa_bs_rr_2,
	isa_bs_rr_4,
	bs_notimpl_bs_rr_8,

	/* write (single) */
	isa_bs_w_1,
	isa_bs_w_2,
	isa_bs_w_4,
	bs_notimpl_bs_w_8,

	/* write multiple */
	isa_bs_wm_1,
	isa_bs_wm_2,
	isa_bs_wm_4,
	bs_notimpl_bs_wm_8,

	/* write region */
	isa_bs_wr_1,
	isa_bs_wr_2,
	isa_bs_wr_4,
	bs_notimpl_bs_wr_8,

	/* set multiple */
	bs_notimpl_bs_sm_1,
	bs_notimpl_bs_sm_2,
	bs_notimpl_bs_sm_4,
	bs_notimpl_bs_sm_8,

	/* set region */
	bs_notimpl_bs_sr_1,
	isa_bs_sr_2,
	bs_notimpl_bs_sr_4,
	bs_notimpl_bs_sr_8,

	/* copy */
	bs_notimpl_bs_c_1,
	bs_notimpl_bs_c_2,
	bs_notimpl_bs_c_4,
	bs_notimpl_bs_c_8,
};

/* bus space functions */

void
isa_io_init(isa_io_addr, isa_mem_addr)
	vaddr_t isa_io_addr;
	vaddr_t isa_mem_addr;
{
	isa_io_bs_tag.bs_cookie = (void *)isa_io_addr;
	isa_mem_bs_tag.bs_cookie = (void *)isa_mem_addr;
}

/*
 * break the abstraction: sometimes, other parts of the system
 * (e.g. X servers) need to map ISA space directly.  use these
 * functions sparingly!
 */
vaddr_t
isa_io_data_vaddr(void)
{
	return (vaddr_t)isa_io_bs_tag.bs_cookie;
}

vaddr_t
isa_mem_data_vaddr(void)
{
	return (vaddr_t)isa_mem_bs_tag.bs_cookie;
}

int
isa_bs_map(t, bpa, size, cacheable, bshp)
	void *t;
	bus_addr_t bpa;
	bus_size_t size;
	int cacheable;
	bus_space_handle_t *bshp;
{
	*bshp = bpa + (bus_addr_t)t;
	return(0);
}

void
isa_bs_unmap(t, bsh, size)
	void *t;
	bus_space_handle_t bsh;
	bus_size_t size;
{
	/* Nothing to do. */
}

int
isa_bs_subregion(t, bsh, offset, size, nbshp)
	void *t;
	bus_space_handle_t bsh;
	bus_size_t offset, size;
	bus_space_handle_t *nbshp;
{
/*	printf("isa_subregion(tag=%p, bsh=%lx, off=%lx, sz=%lx)\n",
	    t, bsh, offset, size);*/
	*nbshp = bsh + offset;
	return(0);
}

int
isa_bs_alloc(t, rstart, rend, size, alignment, boundary, cacheable,
    bpap, bshp)
	void *t;
	bus_addr_t rstart, rend;
	bus_size_t size, alignment, boundary;
	int cacheable;
	bus_addr_t *bpap;
	bus_space_handle_t *bshp;
{
	panic("isa_alloc(): Help!");
}

void    
isa_bs_free(t, bsh, size)
	void *t;
	bus_space_handle_t bsh;
	bus_size_t size;
{
	panic("isa_free(): Help!");
}

void *
isa_bs_vaddr(t, bsh)
	void *t;
	bus_space_handle_t bsh;
{

	return ((void *)bsh);
}

void
isa_bs_barrier(t, bsh, offset, len, flags)
	void *t;
	bus_space_handle_t bsh;
	bus_size_t offset, len;
	int flags;
{
	/* just return */
}	
