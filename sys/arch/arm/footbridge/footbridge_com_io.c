/*	$OpenBSD: footbridge_com_io.c,v 1.2 2007/05/25 16:45:59 krw Exp $	*/
/*	$NetBSD: footbridge_com_io.c,v 1.4 2002/09/27 15:35:44 provos Exp $	*/

/*
 * Copyright (c) 1997 Mark Brinicombe.
 * Copyright (c) 1997 Causality Limited.
 * All rights reserved.
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
 *	This product includes software developed by Mark Brinicombe
 *	for the NetBSD Project.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * This file provides the bus space tag for the footbridge serial console
 */

/*
 * bus_space I/O functions for mainbus
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <machine/bus.h>

/* Prototypes for all the bus_space structure functions */

bs_protos(fcomcons);
bs_protos(generic);
bs_protos(bs_notimpl);

/* Declare the fcomcons bus space tag */

struct bus_space fcomcons_bs_tag = {
	/* cookie */
	NULL,

	/* mapping/unmapping */
	fcomcons_bs_map,
	fcomcons_bs_unmap,
	fcomcons_bs_subregion,

	/* allocation/deallocation */
	fcomcons_bs_alloc,
	fcomcons_bs_free,

	/* get kernel virtual address */
	0, /* never used */

	/* Mmap bus space for user */
	bs_notimpl_bs_mmap,

	/* barrier */
	fcomcons_bs_barrier,

	/* read (single) */
	bs_notimpl_bs_r_1,
	bs_notimpl_bs_r_2,
	generic_bs_r_4,
	bs_notimpl_bs_r_8,

	/* read multiple */
	bs_notimpl_bs_rm_1,
	bs_notimpl_bs_rm_2,
	bs_notimpl_bs_rm_4,
	bs_notimpl_bs_rm_8,

	/* read region */
	bs_notimpl_bs_rr_1,
	bs_notimpl_bs_rr_2,
	bs_notimpl_bs_rr_4,
	bs_notimpl_bs_rr_8,

	/* write (single) */
	bs_notimpl_bs_w_1,
	bs_notimpl_bs_w_2,
	generic_bs_w_4,
	bs_notimpl_bs_w_8,

	/* write multiple */
	bs_notimpl_bs_wm_1,
	bs_notimpl_bs_wm_2,
	bs_notimpl_bs_wm_4,
	bs_notimpl_bs_wm_8,

	/* write region */
	bs_notimpl_bs_wr_1,
	bs_notimpl_bs_wr_2,
	bs_notimpl_bs_wr_4,
	bs_notimpl_bs_wr_8,

	bs_notimpl_bs_sm_1,
	bs_notimpl_bs_sm_2,
	bs_notimpl_bs_sm_4,
	bs_notimpl_bs_sm_8,

	/* set region */
	bs_notimpl_bs_sr_1,
	bs_notimpl_bs_sr_2,
	bs_notimpl_bs_sr_4,
	bs_notimpl_bs_sr_8,

	/* copy */
	bs_notimpl_bs_c_1,
	bs_notimpl_bs_c_2,
	bs_notimpl_bs_c_4,
	bs_notimpl_bs_c_8,
};

/* bus space functions */

int
fcomcons_bs_map(t, bpa, size, cacheable, bshp)
	void *t;
	bus_addr_t bpa;
	bus_size_t size;
	int cacheable;
	bus_space_handle_t *bshp;
{
	/*
	 * Temporary implementation as all I/O is already mapped etc.
	 *
	 * Eventually this function will do the mapping check for multiple maps
	 */
	*bshp = bpa;
	return(0);
	}

int
fcomcons_bs_alloc(t, rstart, rend, size, alignment, boundary, cacheable,
    bpap, bshp)
	void *t;
	bus_addr_t rstart, rend;
	bus_size_t size, alignment, boundary;
	int cacheable;
	bus_addr_t *bpap;
	bus_space_handle_t *bshp;
{
	panic("fcomcons_alloc(): Help!");
}


void
fcomcons_bs_unmap(t, bsh, size)
	void *t;
	bus_space_handle_t bsh;
	bus_size_t size;
{
	/*
	 * Temporary implementation
	 */
}

void    
fcomcons_bs_free(t, bsh, size)
	void *t;
	bus_space_handle_t bsh;
	bus_size_t size;
{

	panic("fcomcons_free(): Help!");
	/* fcomcons_unmap() does all that we need to do. */
/*	fcomcons_unmap(t, bsh, size);*/
}

int
fcomcons_bs_subregion(t, bsh, offset, size, nbshp)
	void *t;
	bus_space_handle_t bsh;
	bus_size_t offset, size;
	bus_space_handle_t *nbshp;
{

	*nbshp = bsh + offset;
	return (0);
}

void
fcomcons_bs_barrier(t, bsh, offset, len, flags)
	void *t;
	bus_space_handle_t bsh;
	bus_size_t offset, len;
	int flags;
{
}	

/* End of footbridge_com_io.c */
