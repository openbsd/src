/*	$NetBSD: pxa2x0_a4x_space.c,v 1.2 2003/07/15 00:24:54 lukem Exp $ */

/*
 * Copyright (c) 2002  Genetec Corporation.  All rights reserved.
 * Written by Hiroyuki Bessho for Genetec Corporation.
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
 *	This product includes software developed for the NetBSD Project by
 *	Genetec Corporation.
 * 4. The name of Genetec Corporation may not be used to endorse or 
 *    promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY GENETEC CORPORATION ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GENETEC CORPORATION
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Bus space tag for 8/16-bit devices on 32-bit bus.
 * all registers are located at the address of multiple of 4.
 */

#include <sys/cdefs.h>
/*
__KERNEL_RCSID(0, "$NetBSD: pxa2x0_a4x_space.c,v 1.2 2003/07/15 00:24:54 lukem Exp $");
*/

#include <sys/param.h>
#include <sys/systm.h>

#include <uvm/uvm_extern.h>

#include <machine/bus.h>

/* Prototypes for all the bus_space structure functions */
bs_protos(pxa2x0);
bs_protos(a4x);
bs_protos(generic);
bs_protos(generic_armv4);
bs_protos(bs_notimpl);

struct bus_space pxa2x0_a4x_bs_tag = {
	/* cookie */
	(void *) 0,

	/* mapping/unmapping */
	pxa2x0_bs_map,
	pxa2x0_bs_unmap,
	pxa2x0_bs_subregion,

	/* allocation/deallocation */
	pxa2x0_bs_alloc,	/* not implemented */
	pxa2x0_bs_free,		/* not implemented */

	/* get kernel virtual address */
	pxa2x0_bs_vaddr,

	/* mmap */
	bs_notimpl_bs_mmap,

	/* barrier */
	pxa2x0_bs_barrier,

	/* read (single) */
	a4x_bs_r_1,
	a4x_bs_r_2,
	a4x_bs_r_4,
	bs_notimpl_bs_r_8,

	/* read multiple */
	a4x_bs_rm_1,
	a4x_bs_rm_2,
	bs_notimpl_bs_rm_4,
	bs_notimpl_bs_rm_8,

	/* read region */
	bs_notimpl_bs_rr_1,
	bs_notimpl_bs_rr_2,
	bs_notimpl_bs_rr_4,
	bs_notimpl_bs_rr_8,

	/* write (single) */
	a4x_bs_w_1,
	a4x_bs_w_2,
	a4x_bs_w_4,
	bs_notimpl_bs_w_8,

	/* write multiple */
	a4x_bs_wm_1,
	a4x_bs_wm_2,
	bs_notimpl_bs_wm_4,
	bs_notimpl_bs_wm_8,

	/* write region */
	bs_notimpl_bs_wr_1,
	bs_notimpl_bs_wr_2,
	bs_notimpl_bs_wr_4,
	bs_notimpl_bs_wr_8,

	/* set multiple */
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



