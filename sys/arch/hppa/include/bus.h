/*	$OpenBSD: bus.h,v 1.6 1998/12/29 22:20:27 mickey Exp $	*/

/*
 * Copyright (c) 1998 Michael Shalayeff
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
 *	This product includes software developed by Michael Shalayeff.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

#ifndef _MACHINE_BUS_H_
#define _MACHINE_BUS_H_

#include <machine/cpufunc.h>

/* addresses in bus space */
typedef u_long bus_addr_t;
typedef u_long bus_size_t;

/* access methods for bus space */
typedef u_long bus_space_tag_t;
typedef u_long bus_space_handle_t;

#define	HPPA_BUS_TAG_SET_BASE(tag,base)	\
		((((tag) & 0x00000fff)) | ((base) & 0xfffff000))
#define	HPPA_BUS_TAG_BASE(tag)		((tag) & 0xfffff000)

/* bus access routines */
#define DCIAS(pa) \
	__asm __volatile ("rsm %1, %%r0\n\tfdc %%r0(%0)\n\tssm %1, %%r0" \
			  :: "r" (pa), "i" (PSW_D));

/* no extent handlng for now
   we won't have overlaps from PDC anyway */
static __inline int
bus_space_map (bus_space_tag_t t, bus_addr_t addr, bus_size_t size,
	       int cacheable, bus_space_handle_t *bshp)
{
	*bshp = addr + HPPA_BUS_TAG_BASE(t);
	return 0;
}

static __inline void
bus_space_unmap (bus_space_tag_t t, bus_space_handle_t bsh, bus_size_t size)
{
	/* nothing to do */
}

int	bus_space_subregion __P((bus_space_tag_t t, bus_space_handle_t bsh,
	    bus_size_t offset, bus_size_t size, bus_space_handle_t *nbshp));

int	bus_space_alloc __P((bus_space_tag_t t, bus_addr_t rstart,
	    bus_addr_t rend, bus_size_t size, bus_size_t align,
	    bus_size_t boundary, int cacheable, bus_addr_t *addrp,
	    bus_space_handle_t *bshp));
void	bus_space_free __P((bus_space_tag_t t, bus_space_handle_t bsh,
	    bus_size_t size));

#define	bus_space_read_1(t, h, o)	\
	((void)(t), (*((volatile u_int8_t *)((h) + (o)))))
#define	bus_space_read_2(t, h, o)	\
	((void)(t), (*((volatile u_int16_t *)((h) + (o)))))
#define	bus_space_read_4(t, h, o)	\
	((void)(t), (*((volatile u_int32_t *)((h) + (o)))))
#define	bus_space_read_8(t, h, o)	\
	((void)(t), (*((volatile u_int64_t *)((h) + (o)))))

static __inline void
bus_space_read_multi_1(bus_space_tag_t t, bus_space_handle_t h,
		       bus_size_t o, u_int8_t *a, size_t c)
{
	h += o;
	while (c--)
		*(a++) = *((volatile u_int8_t *)h)++;
}

static __inline void
bus_space_read_multi_2(bus_space_tag_t t, bus_space_handle_t h,
		       bus_size_t o, u_int16_t *a, size_t c)
{
	h += o;
	while (c--)
		*(a++) = *((volatile u_int16_t *)h)++;
}

static __inline void
bus_space_read_multi_4(bus_space_tag_t t, bus_space_handle_t h,
		       bus_size_t o, u_int32_t *a, size_t c)
{
	h += o;
	while (c--)
		*(a++) = *((volatile u_int32_t *)h)++;
}

static __inline void
bus_space_read_multi_8(bus_space_tag_t t, bus_space_handle_t h,
		       bus_size_t o, u_int64_t *a, size_t c)
{
	h += o;
	while (c--)
		*(a++) = *((volatile u_int64_t *)h)++;
}


#define	bus_space_read_raw_multi_2(t, h, o, a, c) \
    bus_space_read_multi_2((t), (h), (o), (u_int16_t *)(a), (c) >> 1)
#define	bus_space_read_raw_multi_4(t, h, o, a, c) \
    bus_space_read_multi_4((t), (h), (o), (u_int32_t *)(a), (c) >> 2)
#define	bus_space_read_raw_multi_8(t, h, o, a, c) \
    bus_space_read_multi_8((t), (h), (o), (u_int32_t *)(a), (c) >> 2)

#if 0
#define	bus_space_read_region_1(t, h, o, a, c) do {		\
} while (0)

#define	bus_space_read_region_2(t, h, o, a, c) do {		\

} while (0)

#define	bus_space_read_region_4(t, h, o, a, c) do {		\
} while (0)

#define	bus_space_read_region_8
#endif

#define	bus_space_read_raw_region_2(t, h, o, a, c) \
    bus_space_read_region_2((t), (h), (o), (u_int16_t *)(a), (c) >> 1)
#define	bus_space_read_raw_region_4(t, h, o, a, c) \
    bus_space_read_region_4((t), (h), (o), (u_int32_t *)(a), (c) >> 2)
#define	bus_space_read_raw_region_8(t, h, o, a, c) \
    bus_space_read_region_8((t), (h), (o), (u_int32_t *)(a), (c) >> 2)

#define	bus_space_write_1(t, h, o, v)	\
	((void)(t), *((volatile u_int8_t *)((h) + (o))) = (v))
#define	bus_space_write_2(t, h, o, v)	\
	((void)(t), *((volatile u_int16_t *)((h) + (o))) = (v))
#define	bus_space_write_4(t, h, o, v)	\
	((void)(t), *((volatile u_int32_t *)((h) + (o))) = (v))
#define	bus_space_write_8(t, h, o, v)	\
	((void)(t), *((volatile u_int64_t *)((h) + (o))) = (v))

static __inline void
bus_space_write_multi_1(bus_space_tag_t t, bus_space_handle_t h,
		       bus_size_t o, const u_int8_t *a, size_t c)
{
	h += o;
	while (c--)
		*((volatile u_int8_t *)h)++ = *(a++);
}

static __inline void
bus_space_write_multi_2(bus_space_tag_t t, bus_space_handle_t h,
		       bus_size_t o, const u_int16_t *a, size_t c)
{
	h += o;
	while (c--)
		*((volatile u_int16_t *)h)++ = *(a++);
}

static __inline void
bus_space_write_multi_4(bus_space_tag_t t, bus_space_handle_t h,
		       bus_size_t o, const u_int32_t *a, size_t c)
{
	h += o;
	while (c--)
		*((volatile u_int32_t *)h)++ = *(a++);
}

static __inline void
bus_space_write_multi_8(bus_space_tag_t t, bus_space_handle_t h,
		       bus_size_t o, const u_int64_t *a, size_t c)
{
	h += o;
	while (c--)
		*((volatile u_int64_t *)h)++ = *(a++);
}


#define	bus_space_write_raw_multi_2(t, h, o, a, c) \
    bus_space_write_multi_2((t), (h), (o), (const u_int16_t *)(a), (c) >> 1)
#define	bus_space_write_raw_multi_4(t, h, o, a, c) \
    bus_space_write_multi_4((t), (h), (o), (const u_int32_t *)(a), (c) >> 2)
#define	bus_space_write_raw_multi_8(t, h, o, a, c) \
    bus_space_write_multi_8((t), (h), (o), (const u_int32_t *)(a), (c) >> 2)

#if 0
#define	bus_space_write_region_1(t, h, o, a, c) do {		\
} while (0)

#define	bus_space_write_region_2(t, h, o, a, c) do {		\
} while (0)

#define	bus_space_write_region_4(t, h, o, a, c) do {		\
} while (0)

#define	bus_space_write_region_8
#endif

#define	bus_space_write_raw_region_2(t, h, o, a, c) \
    bus_space_write_region_2((t), (h), (o), (const u_int16_t *)(a), (c) >> 1)
#define	bus_space_write_raw_region_4(t, h, o, a, c) \
    bus_space_write_region_4((t), (h), (o), (const u_int32_t *)(a), (c) >> 2)
#define	bus_space_write_raw_region_8(t, h, o, a, c) \
    bus_space_write_region_8((t), (h), (o), (const u_int32_t *)(a), (c) >> 2)

#if 0
#define	bus_space_set_multi_1(t, h, o, v, c) do {		\
} while (0)

#define	bus_space_set_multi_2(t, h, o, v, c) do {		\
} while (0)

#define	bus_space_set_multi_4(t, h, o, v, c) do {		\
} while (0)

#define	bus_space_set_multi_8
#endif

#if 0
#define	bus_space_set_region_1(t, h, o, v, c) do {		\
} while (0)

#define	bus_space_set_region_2(t, h, o, v, c) do {		\
} while (0)

#define	bus_space_set_region_4(t, h, o, v, c) do {		\
} while (0)

#define	bus_space_set_region_8
#endif

#if 0
#define	bus_space_copy_1(t, h1, o1, h2, o2, c) do {		\
} while (0)

#define	bus_space_copy_2(t, h1, o1, h2, o2, c) do {		\
} while (0)

#define	bus_space_copy_4(t, h1, o1, h2, o2, c) do {		\
} while (0)

#define	bus_space_copy_8
#endif

#define	BUS_SPACE_BARRIER_READ	0
#define	BUS_SPACE_BARRIER_WRITE	1

void bus_space_barrier __P((bus_space_tag_t tag, bus_space_handle_t h,
	bus_addr_t off, bus_size_t len, int op));

#define	BUS_DMA_WAITOK		0x00
#define	BUS_DMA_NOWAIT		0x01
#define	BUS_DMA_ALLOCNOW	0x02
#define	BUS_DMAMEM_NOSYNC	0x04

/* Forwards needed by prototypes below. */
struct mbuf;
struct proc;
struct uio;

typedef enum {
	BUS_DMASYNC_POSTREAD,
	BUS_DMASYNC_POSTWRITE,
	BUS_DMASYNC_PREREAD,
	BUS_DMASYNC_PREWRITE
} bus_dmasync_op_t;

typedef struct hppa_bus_dma_tag	*bus_dma_tag_t;
typedef struct hppa_bus_dmamap	*bus_dmamap_t;

/*
 *	bus_dma_segment_t
 *
 *	Describes a single contiguous DMA transaction.  Values
 *	are suitable for programming into DMA registers.
 */
struct hppa_bus_dma_segment {
	bus_addr_t	ds_addr;	/* DMA address */
	bus_size_t	ds_len;		/* length of transfer */
};
typedef struct hppa_bus_dma_segment	bus_dma_segment_t;

/*
 *	bus_dma_tag_t
 *
 *	A machine-dependent opaque type describing the implementation of
 *	DMA for a given bus.
 */

struct hppa_bus_dma_tag {
	void	*_cookie;		/* cookie used in the guts */

	/*
	 * DMA mapping methods.
	 */
	int	(*_dmamap_create) __P((bus_dma_tag_t, bus_size_t, int,
		    bus_size_t, bus_size_t, int, bus_dmamap_t *));
	void	(*_dmamap_destroy) __P((bus_dma_tag_t, bus_dmamap_t));
	int	(*_dmamap_load) __P((bus_dma_tag_t, bus_dmamap_t, void *,
		    bus_size_t, struct proc *, int));
	int	(*_dmamap_load_mbuf) __P((bus_dma_tag_t, bus_dmamap_t,
		    struct mbuf *, int));
	int	(*_dmamap_load_uio) __P((bus_dma_tag_t, bus_dmamap_t,
		    struct uio *, int));
	int	(*_dmamap_load_raw) __P((bus_dma_tag_t, bus_dmamap_t,
		    bus_dma_segment_t *, int, bus_size_t, int));
	void	(*_dmamap_unload) __P((bus_dma_tag_t, bus_dmamap_t));
	void	(*_dmamap_sync) __P((bus_dma_tag_t, bus_dmamap_t,
		    bus_dmasync_op_t));

	/*
	 * DMA memory utility functions.
	 */
	int	(*_dmamem_alloc) __P((bus_dma_tag_t, bus_size_t, bus_size_t,
		    bus_size_t, bus_dma_segment_t *, int, int *, int));
	void	(*_dmamem_free) __P((bus_dma_tag_t,
		    bus_dma_segment_t *, int));
	int	(*_dmamem_map) __P((bus_dma_tag_t, bus_dma_segment_t *,
		    int, size_t, caddr_t *, int));
	void	(*_dmamem_unmap) __P((bus_dma_tag_t, caddr_t, size_t));
	int	(*_dmamem_mmap) __P((bus_dma_tag_t, bus_dma_segment_t *,
		    int, int, int, int));
};

#define	bus_dmamap_create(t, s, n, m, b, f, p)			\
	(*(t)->_dmamap_create)((t), (s), (n), (m), (b), (f), (p))
#define	bus_dmamap_destroy(t, p)				\
	(*(t)->_dmamap_destroy)((t), (p))
#define	bus_dmamap_load(t, m, b, s, p, f)			\
	(*(t)->_dmamap_load)((t), (m), (b), (s), (p), (f))
#define	bus_dmamap_load_mbuf(t, m, b, f)			\
	(*(t)->_dmamap_load_mbuf)((t), (m), (b), (f))
#define	bus_dmamap_load_uio(t, m, u, f)				\
	(*(t)->_dmamap_load_uio)((t), (m), (u), (f))
#define	bus_dmamap_load_raw(t, m, sg, n, s, f)			\
	(*(t)->_dmamap_load_raw)((t), (m), (sg), (n), (s), (f))
#define	bus_dmamap_unload(t, p)					\
	(*(t)->_dmamap_unload)((t), (p))
#define	bus_dmamap_sync(t, p, o)				\
	(void)((t)->_dmamap_sync ?				\
	    (*(t)->_dmamap_sync)((t), (p), (o)) : (void)0)

#define	bus_dmamem_alloc(t, s, a, b, sg, n, r, f)		\
	(*(t)->_dmamem_alloc)((t), (s), (a), (b), (sg), (n), (r), (f))
#define	bus_dmamem_free(t, sg, n)				\
	(*(t)->_dmamem_free)((t), (sg), (n))
#define	bus_dmamem_map(t, sg, n, s, k, f)			\
	(*(t)->_dmamem_map)((t), (sg), (n), (s), (k), (f))
#define	bus_dmamem_unmap(t, k, s)				\
	(*(t)->_dmamem_unmap)((t), (k), (s))
#define	bus_dmamem_mmap(t, sg, n, o, p, f)			\
	(*(t)->_dmamem_mmap)((t), (sg), (n), (o), (p), (f))

/*
 *	bus_dmamap_t
 *
 *	Describes a DMA mapping.
 */
struct hppa_bus_dmamap {
	/*
	 * PRIVATE MEMBERS: not for use my machine-independent code.
	 */
	bus_size_t	_dm_size;	/* largest DMA transfer mappable */
	int		_dm_segcnt;	/* number of segs this map can map */
	bus_size_t	_dm_maxsegsz;	/* largest possible segment */
	bus_size_t	_dm_boundary;	/* don't cross this */
	int		_dm_flags;	/* misc. flags */

	void		*_dm_cookie;	/* cookie for bus-specific functions */

	/*
	 * PUBLIC MEMBERS: these are used by machine-independent code.
	 */
	int		dm_nsegs;	/* # valid segments in mapping */
	bus_dma_segment_t dm_segs[1];	/* segments; variable length */
};

#endif /* _MACHINE_BUS_H_ */
