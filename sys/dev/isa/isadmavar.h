/*	$OpenBSD: isadmavar.h,v 1.12 2001/10/31 11:00:24 art Exp $	*/
/*	$NetBSD: isadmavar.h,v 1.10 1997/08/04 22:13:33 augustss Exp $	*/

/*-
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
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

/* XXX for now... */
#ifndef __ISADMA_COMPAT
#define __ISADMA_COMPAT
#endif /* __ISADMA_COMPAT */

#ifdef __ISADMA_COMPAT

/* XXX ugly.. but it's a deprecated API that uses it so it will go.. */
extern struct device *isa_dev;

#define	ISADMA_MAP_WAITOK	0x0001	/* OK for isadma_map to sleep */
#define	ISADMA_MAP_BOUNCE	0x0002	/* use bounce buffer if necessary */
#define	ISADMA_MAP_CONTIG	0x0004	/* must be physically contiguous */
#define	ISADMA_MAP_8BIT		0x0008	/* must not cross 64k boundary */
#define	ISADMA_MAP_16BIT	0x0010	/* must not cross 128k boundary */

struct isadma_seg {		/* a physical contiguous segment */
	vm_offset_t addr;	/* address of this segment */
	vm_size_t length;	/* length of this segment (bytes) */
	bus_dmamap_t dmam;	/* DMA handle for bus_dma routines. */
};

int isadma_map __P((caddr_t, vm_size_t, struct isadma_seg *, int));
void isadma_unmap __P((caddr_t, vm_size_t, int, struct isadma_seg *));
void isadma_copytobuf __P((caddr_t, vm_size_t, int, struct isadma_seg *));
void isadma_copyfrombuf __P((caddr_t, vm_size_t, int, struct isadma_seg *));

#define isadma_acquire(c)		isa_dma_acquire(isa_dev, (c))
#define isadma_release(c)		isa_dma_release(isa_dev, (c))
#define isadma_cascade(c)		isa_dmacascade(isa_dev, (c))
#define isadma_start(a, s, c, f) \
    isa_dmastart(isa_dev, (c), (a), (s), 0, (f), BUS_DMA_WAITOK|BUS_DMA_BUS1)
#define isadma_abort(c)			isa_dmaabort(isa_dev, (c))
#define isadma_finished(c)		isa_dmafinished(isa_dev, (c))
#define isadma_done(c)			isa_dmadone(isa_dev, (c))

#endif /* __ISADMA_COMPAT */

#define MAX_ISADMA	65536

#define	DMAMODE_WRITE	0
#define	DMAMODE_READ	1
#define	DMAMODE_LOOP	2

struct proc;

void	   isa_dmacascade __P((struct device *, int));

int	   isa_dmamap_create __P((struct device *, int, bus_size_t, int));
void	   isa_dmamap_destroy __P((struct device *, int));

int	   isa_dmastart __P((struct device *, int, void *, bus_size_t,
	       struct proc *, int, int));
void	   isa_dmaabort __P((struct device *, int));
bus_size_t isa_dmacount __P((struct device *, int));
int	   isa_dmafinished __P((struct device *, int));
void	   isa_dmadone __P((struct device *, int));

int	   isa_dmamem_alloc __P((struct device *, int, bus_size_t,
	       bus_addr_t *, int));
void	   isa_dmamem_free __P((struct device *, int, bus_addr_t, bus_size_t));
int	   isa_dmamem_map __P((struct device *, int, bus_addr_t, bus_size_t,
	       caddr_t *, int));
void	   isa_dmamem_unmap __P((struct device *, int, caddr_t, size_t));
int	   isa_dmamem_mmap __P((struct device *, int, bus_addr_t, bus_size_t,
	       int, int, int));

int	   isa_drq_isfree __P((struct device *, int));

void      *isa_malloc __P((struct device *, int, size_t, int, int));
void	   isa_free __P((void *, int));
paddr_t	   isa_mappage __P((void *, off_t, int));
