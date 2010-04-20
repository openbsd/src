/*	$OpenBSD: bus.h,v 1.3 2010/04/20 22:53:24 miod Exp $	*/

/*
 * Copyright (c) 2003-2004 Opsycon AB Sweden.  All rights reserved.
 * Copyright (c) 2004, Miodrag Vallat.
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

#ifndef _AVIION_BUS_H_
#define _AVIION_BUS_H_

#include <machine/asm_macro.h>

#ifdef __STDC__
#define CAT(a,b)	a##b
#define CAT3(a,b,c)	a##b##c
#else
#define CAT(a,b)	a/**/b
#define CAT3(a,b,c)	a/**/b/**/c
#endif

/*
 * bus_space implementation
 */

typedef uint32_t bus_addr_t;
typedef uint32_t bus_size_t;

struct aviion_bus_space_tag;
typedef const struct aviion_bus_space_tag *bus_space_tag_t;
typedef uint32_t bus_space_handle_t;

struct aviion_bus_space_tag {
	int		(*_space_map)(bus_space_tag_t, bus_addr_t,
			  bus_size_t, int, bus_space_handle_t *);
	void		(*_space_unmap)(bus_space_tag_t, bus_space_handle_t,
			  bus_size_t);
	int		(*_space_subregion)(bus_space_tag_t, bus_space_handle_t,
			  bus_size_t, bus_size_t, bus_space_handle_t *);
	void *		(*_space_vaddr)(bus_space_tag_t, bus_space_handle_t);
	/* alloc, free not implemented yet */

	u_int8_t	(*_space_read_1)(bus_space_tag_t , bus_space_handle_t,
			  bus_size_t);
	void		(*_space_write_1)(bus_space_tag_t , bus_space_handle_t,
			  bus_size_t, u_int8_t);
	u_int16_t	(*_space_read_2)(bus_space_tag_t , bus_space_handle_t,
			  bus_size_t);
	void		(*_space_write_2)(bus_space_tag_t , bus_space_handle_t,
			  bus_size_t, u_int16_t);
	u_int32_t	(*_space_read_4)(bus_space_tag_t , bus_space_handle_t,
			  bus_size_t);
	void		(*_space_write_4)(bus_space_tag_t , bus_space_handle_t,
			  bus_size_t, u_int32_t);
	void		(*_space_read_raw_2)(bus_space_tag_t, bus_space_handle_t,
			  bus_addr_t, u_int8_t *, bus_size_t);
	void		(*_space_write_raw_2)(bus_space_tag_t, bus_space_handle_t,
			  bus_addr_t, const u_int8_t *, bus_size_t);
	void		(*_space_read_raw_4)(bus_space_tag_t, bus_space_handle_t,
			  bus_addr_t, u_int8_t *, bus_size_t);
	void		(*_space_write_raw_4)(bus_space_tag_t, bus_space_handle_t,
			  bus_addr_t, const u_int8_t *, bus_size_t);
};

#define	bus_space_read_1(t, h, o) (*(t)->_space_read_1)((t), (h), (o))
#define	bus_space_read_2(t, h, o) (*(t)->_space_read_2)((t), (h), (o))
#define	bus_space_read_4(t, h, o) (*(t)->_space_read_4)((t), (h), (o))

#define	bus_space_write_1(t, h, o, v) (*(t)->_space_write_1)((t), (h), (o), (v))
#define	bus_space_write_2(t, h, o, v) (*(t)->_space_write_2)((t), (h), (o), (v))
#define	bus_space_write_4(t, h, o, v) (*(t)->_space_write_4)((t), (h), (o), (v))

#define	bus_space_read_raw_multi_2(t, h, a, b, l) \
	(*(t)->_space_read_raw_2)((t), (h), (a), (b), (l))
#define	bus_space_read_raw_multi_4(t, h, a, b, l) \
	(*(t)->_space_read_raw_4)((t), (h), (a), (b), (l))

#define	bus_space_write_raw_multi_2(t, h, a, b, l) \
	(*(t)->_space_write_raw_2)((t), (h), (a), (b), (l))
#define	bus_space_write_raw_multi_4(t, h, a, b, l) \
	(*(t)->_space_write_raw_4)((t), (h), (a), (b), (l))

#define	bus_space_map(t, o, s, c, p) \
    (*(t)->_space_map)((t), (o), (s), (c), (p))
#define	bus_space_unmap(t, h, s) \
    (*(t)->_space_unmap)((t), (h), (s))
#define	bus_space_subregion(t, h, o, s, p) \
    (*(t)->_space_subregion)((t), (h), (o), (s), (p))

#define	BUS_SPACE_MAP_CACHEABLE		0x01
#define	BUS_SPACE_MAP_LINEAR		0x02
#define	BUS_SPACE_MAP_PREFETCHABLE	0x04

#define	bus_space_vaddr(t, h)	(*(t)->_space_vaddr)((t), (h))

/*----------------------------------------------------------------------------*/
#define bus_space_read_multi(n,m)					      \
static __inline void							      \
CAT(bus_space_read_multi_,n)(bus_space_tag_t bst, bus_space_handle_t bsh,     \
     bus_size_t o, CAT3(u_int,m,_t) *x, size_t cnt)			      \
{									      \
	while (cnt--)							      \
		*x++ = CAT(bus_space_read_,n)(bst, bsh, o);		      \
}

bus_space_read_multi(1,8)
bus_space_read_multi(2,16)
bus_space_read_multi(4,32)

/*----------------------------------------------------------------------------*/
#define bus_space_read_region(n,m)					      \
static __inline void							      \
CAT(bus_space_read_region_,n)(bus_space_tag_t bst, bus_space_handle_t bsh,    \
     bus_addr_t ba, CAT3(u_int,m,_t) *x, size_t cnt)			      \
{									      \
	while (cnt--)							      \
		*x++ = CAT(bus_space_read_,n)(bst, bsh, ba++);		      \
}

bus_space_read_region(1,8)
bus_space_read_region(2,16)
bus_space_read_region(4,32)

/*----------------------------------------------------------------------------*/
#define bus_space_read_raw_region(n,m)					      \
static __inline void							      \
CAT(bus_space_read_raw_region_,n)(bus_space_tag_t bst,			      \
     bus_space_handle_t bsh,						      \
     bus_addr_t ba, u_int8_t *x, size_t cnt)				      \
{									      \
	cnt >>= ((n) >> 1);						      \
	while (cnt--) {							      \
		CAT(bus_space_read_raw_multi_,n)(bst, bsh, ba, x, (n));	      \
		ba += (n);						      \
		x += (n);						      \
	}								      \
}

bus_space_read_raw_region(2,16)
bus_space_read_raw_region(4,32)

/*----------------------------------------------------------------------------*/
#define bus_space_write_multi(n,m)					      \
static __inline void							      \
CAT(bus_space_write_multi_,n)(bus_space_tag_t bst, bus_space_handle_t bsh,    \
     bus_size_t o, const CAT3(u_int,m,_t) *x, size_t cnt)		      \
{									      \
	while (cnt--) {							      \
		CAT(bus_space_write_,n)(bst, bsh, o, *x++);		      \
	}								      \
}

bus_space_write_multi(1,8)
bus_space_write_multi(2,16)
bus_space_write_multi(4,32)

/*----------------------------------------------------------------------------*/
#define bus_space_write_region(n,m)					      \
static __inline void							      \
CAT(bus_space_write_region_,n)(bus_space_tag_t bst, bus_space_handle_t bsh,   \
     bus_addr_t ba, const CAT3(u_int,m,_t) *x, size_t cnt)		      \
{									      \
	while (cnt--) {							      \
		CAT(bus_space_write_,n)(bst, bsh, ba, *x++);		      \
		ba += sizeof(x);					      \
	}								      \
}

bus_space_write_region(1,8)
bus_space_write_region(2,16)
bus_space_write_region(4,32)

/*----------------------------------------------------------------------------*/
#define bus_space_write_raw_region(n,m)					      \
static __inline void							      \
CAT(bus_space_write_raw_region_,n)(bus_space_tag_t bst,			      \
     bus_space_handle_t bsh,						      \
     bus_addr_t ba, const u_int8_t *x, size_t cnt)		              \
{									      \
	cnt >>= ((n) >> 1);						      \
	while (cnt--) {							      \
		CAT(bus_space_write_raw_multi_,n)(bst, bsh, ba, x, (n));      \
		ba += (n);						      \
		x += (n);						      \
	}								      \
}

bus_space_write_raw_region(2,16)
bus_space_write_raw_region(4,32)

/*----------------------------------------------------------------------------*/
#define bus_space_set_region(n,m)					      \
static __inline void							      \
CAT(bus_space_set_region_,n)(bus_space_tag_t bst, bus_space_handle_t bsh,     \
     bus_addr_t ba, CAT3(u_int,m,_t) x, size_t cnt)			      \
{									      \
	while (cnt--) {							      \
		CAT(bus_space_write_,n)(bst, bsh, ba, x);		      \
		ba += sizeof(x);					      \
	}								      \
}

bus_space_set_region(1,8)
bus_space_set_region(2,16)
bus_space_set_region(4,32)

/*----------------------------------------------------------------------------*/
#define bus_space_copy(n)					      	      \
static __inline void							      \
CAT(bus_space_copy_,n)(bus_space_tag_t bst, bus_space_handle_t bsh1,	      \
     bus_size_t o1, bus_space_handle_t bsh2, bus_size_t o2, bus_size_t cnt)   \
{									      \
	while (cnt--) {							      \
		CAT(bus_space_write_,n)(bst, bsh2, o2,			      \
		    CAT(bus_space_read_,n)(bst, bsh1, o1));		      \
		o1 += (n);						      \
		o2 += (n);						      \
	}								      \
}

bus_space_copy(1)
bus_space_copy(2)
bus_space_copy(4)

uint8_t generic_space_read_1(bus_space_tag_t, bus_space_handle_t, bus_size_t);
uint16_t generic_space_read_2(bus_space_tag_t, bus_space_handle_t, bus_size_t);
uint32_t generic_space_read_4(bus_space_tag_t, bus_space_handle_t, bus_size_t);
void	generic_space_read_raw_2(bus_space_tag_t, bus_space_handle_t,
	    bus_addr_t, uint8_t *, bus_size_t);
void	generic_space_write_1(bus_space_tag_t, bus_space_handle_t, bus_size_t,
	    uint8_t);
void	generic_space_write_2(bus_space_tag_t, bus_space_handle_t, bus_size_t,
	    uint16_t);
void	generic_space_write_4(bus_space_tag_t, bus_space_handle_t, bus_size_t,
	    uint32_t);
void	generic_space_write_raw_2(bus_space_tag_t, bus_space_handle_t,
	    bus_addr_t, const uint8_t *, bus_size_t);
void	generic_space_read_raw_4(bus_space_tag_t, bus_space_handle_t,
	    bus_addr_t, uint8_t *, bus_size_t);
void	generic_space_write_raw_4(bus_space_tag_t, bus_space_handle_t,
	    bus_addr_t, const uint8_t *, bus_size_t);

/*----------------------------------------------------------------------------*/
/*
 * Bus read/write barrier methods.
 *
 *	void bus_space_barrier(bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset,
 *	    bus_size_t len, int flags);
 *
 */
static inline void
bus_space_barrier(bus_space_tag_t t, bus_space_handle_t h, bus_size_t offset,
    bus_size_t length, int flags)
{
	flush_pipeline();
}

#define BUS_SPACE_BARRIER_READ  0x01		/* force read barrier */
#define BUS_SPACE_BARRIER_WRITE 0x02		/* force write barrier */

/*
 * bus_dma implementation
 */

#define	BUS_DMA_WAITOK		0x000	/* safe to sleep (pseudo-flag) */
#define	BUS_DMA_NOWAIT		0x001	/* not safe to sleep */
#define	BUS_DMA_ALLOCNOW	0x002	/* perform resource allocation now */
#define	BUS_DMA_COHERENT	0x004	/* hint: map memory DMA coherent */
#define	BUS_DMA_BUS1		0x010	/* placeholders for bus functions... */
#define	BUS_DMA_BUS2		0x020
#define	BUS_DMA_BUS3		0x040
#define	BUS_DMA_BUS4		0x080
#define	BUS_DMA_READ		0x100	/* mapping is device -> memory only */
#define	BUS_DMA_WRITE		0x200	/* mapping is memory -> device only */
#define	BUS_DMA_STREAMING	0x400	/* hint: sequential, unidirectional */
#define	BUS_DMA_ZERO		0x800	/* zero memory in dmamem_alloc */

#define BUS_DMASYNC_PREREAD	0x01
#define BUS_DMASYNC_POSTREAD	0x02
#define BUS_DMASYNC_PREWRITE	0x04
#define BUS_DMASYNC_POSTWRITE	0x08

typedef	u_int32_t	bus_dma_tag_t;	/* ignored, really */

/*
 *	bus_dma_segment_t
 *
 *	Describes a single contiguous DMA transaction.  Values
 *	are suitable for programming into DMA registers.
 */
struct m88k_bus_dma_segment {
	bus_addr_t	ds_addr;	/* DMA address */
	bus_size_t	ds_len;		/* length of transfer */
};
typedef struct m88k_bus_dma_segment  bus_dma_segment_t;

/*
 *	bus_dmamap_t
 *
 *	Describes a DMA mapping.
 */
struct m88k_bus_dmamap {
	bus_size_t	_dm_size;	/* largest DMA transfer mappable */
	int		_dm_segcnt;	/* number of segs this map can map */
	bus_size_t	_dm_maxsegsz;	/* largest possible segment */
	bus_size_t	_dm_boundary;	/* don't cross this */

	bus_size_t	dm_mapsize;	/* size of the mapping */
	int		dm_nsegs;	/* # valid segments in mapping */
	bus_dma_segment_t dm_segs[1];	/* segments; variable length */
};
typedef struct m88k_bus_dmamap		*bus_dmamap_t;

struct mbuf;
struct proc;
struct uio;

int	bus_dmamap_create(bus_dma_tag_t, bus_size_t, int, bus_size_t,
	    bus_size_t, int, bus_dmamap_t *);
void	bus_dmamap_destroy(bus_dma_tag_t, bus_dmamap_t);
int	bus_dmamap_load(bus_dma_tag_t, bus_dmamap_t, void *,
	    bus_size_t, struct proc *, int);
int	bus_dmamap_load_mbuf(bus_dma_tag_t, bus_dmamap_t,
	    struct mbuf *, int);
int	bus_dmamap_load_uio(bus_dma_tag_t, bus_dmamap_t,
	    struct uio *, int);
int	bus_dmamap_load_raw(bus_dma_tag_t, bus_dmamap_t, bus_dma_segment_t *,
	    int, bus_size_t, int);
void	bus_dmamap_unload(bus_dma_tag_t, bus_dmamap_t);
void	bus_dmamap_sync(bus_dma_tag_t, bus_dmamap_t, bus_addr_t,
	    bus_size_t, int);

int	bus_dmamem_alloc(bus_dma_tag_t tag, bus_size_t size,
	    bus_size_t alignment, bus_size_t boundary,
	    bus_dma_segment_t *segs, int nsegs, int *rsegs, int flags);
void	bus_dmamem_free(bus_dma_tag_t tag, bus_dma_segment_t *segs,
	    int nsegs);
int	bus_dmamem_map(bus_dma_tag_t tag, bus_dma_segment_t *segs,
	    int nsegs, size_t size, caddr_t *kvap, int flags);
void	bus_dmamem_unmap(bus_dma_tag_t tag, caddr_t kva,
	    size_t size);
paddr_t bus_dmamem_mmap(bus_dma_tag_t tag, bus_dma_segment_t *segs,
	    int nsegs, off_t off, int prot, int flags);

#endif	/* _AVIION_BUS_H_ */
