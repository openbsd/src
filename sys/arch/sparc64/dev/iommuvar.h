/*	$OpenBSD: iommuvar.h,v 1.7 2003/02/17 01:29:20 henric Exp $	*/
/*	$NetBSD: iommuvar.h,v 1.9 2001/10/07 20:30:41 eeh Exp $	*/

/*
 * Copyright (c) 1999 Matthew R. Green
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _SPARC64_DEV_IOMMUVAR_H_
#define _SPARC64_DEV_IOMMUVAR_H_

/*
 * per-Streaming Buffer state
 */

struct strbuf_ctl {
	bus_space_tag_t		sb_bustag;	/* streaming buffer registers */
	bus_space_handle_t	sb_sb;		/* Handle for our regs */
	paddr_t			sb_flushpa;	/* to flush streaming buffers */
	volatile int64_t	*sb_flush;
};

/*
 * per-IOMMU state
 */
struct iommu_state {
	paddr_t			is_ptsb;	/* TSB physical address */
	int64_t			*is_tsb;	/* TSB virtual address */
	int			is_tsbsize;	/* 0 = 8K, ... */
	u_int			is_dvmabase;
	u_int			is_dvmaend;
	int64_t			is_cr;		/* IOMMU control register value */
	struct extent		*is_dvmamap;	/* DVMA map for this instance */

	struct strbuf_ctl	*is_sb[2];	/* Streaming buffers if any */

	/* copies of our parents state, to allow us to be self contained */
	bus_space_tag_t		is_bustag;	/* our bus tag */
	bus_space_handle_t	is_iommu;	/* IOMMU registers */
};

/* interfaces for PCI/SBUS code */
void	iommu_init(char *, struct iommu_state *, int, u_int32_t);
void	iommu_reset(struct iommu_state *);
void    iommu_enter(struct iommu_state *, vaddr_t, int64_t, int);
void    iommu_remove(struct iommu_state *, vaddr_t, size_t);
paddr_t iommu_extract(struct iommu_state *, vaddr_t);

int	iommu_dvmamap_load(bus_dma_tag_t, struct iommu_state *,
	    bus_dmamap_t, void *, bus_size_t, struct proc *, int);
void	iommu_dvmamap_unload(bus_dma_tag_t, struct iommu_state *,
	    bus_dmamap_t);
int	iommu_dvmamap_load_raw(bus_dma_tag_t, struct iommu_state *,
	    bus_dmamap_t, bus_dma_segment_t *, int, int, bus_size_t);
void	iommu_dvmamap_sync(bus_dma_tag_t, struct iommu_state *,
	    bus_dmamap_t, bus_addr_t, bus_size_t, int);
int	iommu_dvmamem_alloc(bus_dma_tag_t, struct iommu_state *,
	    bus_size_t, bus_size_t, bus_size_t, bus_dma_segment_t *,
	    int, int *, int);
void	iommu_dvmamem_free(bus_dma_tag_t, struct iommu_state *,
	    bus_dma_segment_t *, int);
int	iommu_dvmamem_map(bus_dma_tag_t, struct iommu_state *,
	    bus_dma_segment_t *, int, size_t, caddr_t *, int);
void	iommu_dvmamem_unmap(bus_dma_tag_t, struct iommu_state *,
	    caddr_t, size_t);

#define IOMMUREG_READ(is, reg)				\
	bus_space_read_8((is)->is_bustag,		\
		(is)->is_iommu,				\
		IOMMUREG(reg))	

#define IOMMUREG_WRITE(is, reg, v)			\
	bus_space_write_8((is)->is_bustag,		\
		(is)->is_iommu,				\
		IOMMUREG(reg),				\
		(v))

#endif /* _SPARC64_DEV_IOMMUVAR_H_ */

