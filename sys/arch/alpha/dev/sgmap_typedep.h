/* $OpenBSD: sgmap_typedep.h,v 1.2 2002/03/14 01:26:26 millert Exp $ */
/* $NetBSD: sgmap_typedep.h,v 1.4 1998/06/04 01:22:52 thorpej Exp $ */

/*-
 * Copyright (c) 1997, 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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

#undef __C
#undef __S

#define	__C(A,B)	__CONCAT(A,B)
#define	__S(S)		__STRING(S)

extern	SGMAP_PTE_TYPE	__C(SGMAP_TYPE,_prefetch_spill_page_pte);

void	__C(SGMAP_TYPE,_init_spill_page_pte)(void);
int	__C(SGMAP_TYPE,_load)(bus_dma_tag_t, bus_dmamap_t,
	    void *, bus_size_t, struct proc *, int, struct alpha_sgmap *);
int	__C(SGMAP_TYPE,_load_mbuf)(bus_dma_tag_t, bus_dmamap_t,
	    struct mbuf *, int, struct alpha_sgmap *);
int	__C(SGMAP_TYPE,_load_uio)(bus_dma_tag_t, bus_dmamap_t,
	    struct uio *, int, struct alpha_sgmap *);
int	__C(SGMAP_TYPE,_load_raw)(bus_dma_tag_t, bus_dmamap_t,
	    bus_dma_segment_t *, int, bus_size_t, int, struct alpha_sgmap *);
void	__C(SGMAP_TYPE,_unload)(bus_dma_tag_t, bus_dmamap_t,
	    struct alpha_sgmap *);
