/*	$OpenBSD: bus.h,v 1.12 2011/04/07 15:30:16 miod Exp $	*/
/*
 * Copyright (c) 2003, Miodrag Vallat.
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
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * ``No-nonsense'' sparc bus_space implementation.
 *
 * This is a stripped down bus_space implementation for sparc, providing
 * simple enough functions, suitable for inlining.
 * To achieve this goal, it relies upon the following assumptions:
 * - everything is memory-mapped. Hence, tags are basically not used,
 *   and most operation are just simple pointer arithmetic with the handle.
 * - interrupt functions are not implemented; callers will provide their
 *   own wrappers on a need-to-do basis.
 */

#ifndef	_MACHINE_BUS_H_
#define	_MACHINE_BUS_H_

#include <machine/autoconf.h>

#include <uvm/uvm_extern.h>

#include <machine/pmap.h>

typedef	u_int32_t	bus_space_handle_t;

/*
 * bus_space_tag_t are pointer to *modified* rom_reg structures.
 * rr_iospace is used to also carry bus endianness information.
 */
typedef	struct rom_reg 	*bus_space_tag_t;

#define	TAG_LITTLE_ENDIAN		0x80000000

#define	SET_TAG_BIG_ENDIAN(t)		((t))->rr_iospace &= ~TAG_LITTLE_ENDIAN
#define	SET_TAG_LITTLE_ENDIAN(t)	((t))->rr_iospace |= TAG_LITTLE_ENDIAN

#define	IS_TAG_LITTLE_ENDIAN(t)		((t)->rr_iospace & TAG_LITTLE_ENDIAN)

typedef	u_int32_t	bus_addr_t;
typedef	u_int32_t	bus_size_t;

/* 
 * General bus_space function set
 */

static int bus_space_map(bus_space_tag_t, bus_addr_t, bus_size_t, int,
    bus_space_handle_t *);
static int bus_space_unmap(bus_space_tag_t, bus_space_handle_t,
    bus_size_t);

#define	BUS_SPACE_MAP_CACHEABLE		0x01
#define	BUS_SPACE_MAP_LINEAR		0x02
#define	BUS_SPACE_MAP_PREFETCHABLE	0x04

#define	BUS_SPACE_BARRIER_READ	0x01
#define	BUS_SPACE_BARRIER_WRITE	0x02

static void bus_space_barrier(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, bus_size_t, int);

static void* bus_space_vaddr(bus_space_tag_t, bus_space_handle_t);
static int bus_space_subregion(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, bus_size_t, bus_space_handle_t *);

static __inline__ int
bus_space_map(bus_space_tag_t tag, bus_addr_t addr, bus_size_t size, int flags,
    bus_space_handle_t *handle)
{
	if ((*handle = (bus_space_handle_t)mapiodev(tag, addr, size)) != 0)
		return (0);

	return (ENOMEM);
}

static __inline__ int
bus_space_unmap(bus_space_tag_t tag, bus_space_handle_t handle, bus_size_t size)
{
	/*
	 * The mapiodev() call eventually ended up as a set of pmap_kenter_pa()
	 * calls. Although the iospace va will not be reclaimed, at least
	 * relinquish the wiring.
	 */
	pmap_kremove((vaddr_t)handle, size);
	pmap_update(pmap_kernel());
	return (0);
}

static __inline__ void
bus_space_barrier(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, bus_size_t size, int flags)
{
	/* no membar is necessary for sparc so far */
}

static __inline__ void *
bus_space_vaddr(bus_space_tag_t tag, bus_space_handle_t handle)
{
	return ((void *)handle);
}

static __inline__ int
bus_space_subregion(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, bus_size_t size, bus_space_handle_t *newh)
{
	*newh = handle + offset;
	return (0);
}

/*
 * Read/Write/Region functions
 * Most of these are straightforward and assume that everything is properly
 * aligned.
 */

#define	bus_space_read_1(tag, handle, offset) \
	((void)(tag), *(volatile u_int8_t *)((handle) + (offset)))
#define	__bus_space_read_2(tag, handle, offset) \
	*(volatile u_int16_t *)((handle) + (offset))
#define	__bus_space_read_4(tag, handle, offset) \
	*(volatile u_int32_t *)((handle) + (offset))
#define	bus_space_read_2(tag, handle, offset) \
	((IS_TAG_LITTLE_ENDIAN(tag)) ? \
		letoh16(__bus_space_read_2(tag, handle, offset)) : \
		__bus_space_read_2(tag, handle, offset))
#define	bus_space_read_4(tag, handle, offset) \
	((IS_TAG_LITTLE_ENDIAN(tag)) ? \
		letoh32(__bus_space_read_4(tag, handle, offset)) : \
		__bus_space_read_4(tag, handle, offset))

static void bus_space_read_multi_1(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int8_t *, size_t);

static __inline__ void
bus_space_read_multi_1(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int8_t *dest, size_t count)
{
	while ((int)--count >= 0)
		*dest++ = bus_space_read_1(tag, handle, offset);
}

static void bus_space_read_multi_2(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int16_t *, size_t);

static __inline__ void
bus_space_read_multi_2(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int16_t *dest, size_t count)
{
	while ((int)--count >= 0)
		*dest++ = bus_space_read_2(tag, handle, offset);
}

static void bus_space_read_multi_4(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int32_t *, size_t);

static __inline__ void
bus_space_read_multi_4(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int32_t *dest, size_t count)
{
	while ((int)--count >= 0)
		*dest++ = bus_space_read_4(tag, handle, offset);
}

static void bus_space_read_raw_multi_2(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int8_t *, size_t);

static __inline__ void
bus_space_read_raw_multi_2(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int8_t *dest, size_t size)
{
	size >>= 1;
	while ((int)--size >= 0) {
		*(u_int16_t *)dest =
		    __bus_space_read_2(tag, handle, offset);
		dest += 2;
	}
}

static void bus_space_read_raw_multi_4(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int8_t *, size_t);

static __inline__ void
bus_space_read_raw_multi_4(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int8_t *dest, size_t size)
{
	size >>= 2;
	while ((int)--size >= 0) {
		*(u_int32_t *)dest =
		    __bus_space_read_4(tag, handle, offset);
		dest += 4;
	}
}

#define	bus_space_write_1(tag, handle, offset, value) \
	((void)(tag), *(volatile u_int8_t *)((handle) + (offset)) = (value))
#define	__bus_space_write_2(tag, handle, offset, value) \
	*(volatile u_int16_t *)((handle) + (offset)) = (value)
#define	__bus_space_write_4(tag, handle, offset, value) \
	*(volatile u_int32_t *)((handle) + (offset)) = (value)
#define	bus_space_write_2(tag, handle, offset, value) \
	__bus_space_write_2(tag, handle, offset, \
	    (IS_TAG_LITTLE_ENDIAN(tag)) ? htole16(value) : (value))
#define	bus_space_write_4(tag, handle, offset, value) \
	__bus_space_write_4(tag, handle, offset, \
	    (IS_TAG_LITTLE_ENDIAN(tag)) ? htole32(value) : (value))

static void bus_space_write_multi_1(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int8_t *, size_t);

static __inline__ void
bus_space_write_multi_1(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int8_t *dest, size_t count)
{
	while ((int)--count >= 0)
		bus_space_write_1(tag, handle, offset, *dest++);
}

static void bus_space_write_multi_2(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int16_t *, size_t);

static __inline__ void
bus_space_write_multi_2(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int16_t *dest, size_t count)
{
	while ((int)--count >= 0)
		bus_space_write_2(tag, handle, offset, *dest++);
}

static void bus_space_write_multi_4(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int32_t *, size_t);

static __inline__ void
bus_space_write_multi_4(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int32_t *dest, size_t count)
{
	while ((int)--count >= 0)
		bus_space_write_4(tag, handle, offset, *dest);
}

static void bus_space_write_raw_multi_2(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int8_t *, size_t);

static __inline__ void
bus_space_write_raw_multi_2(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int8_t *dest, size_t size)
{
	size >>= 1;
	while ((int)--size >= 0) {
		__bus_space_write_2(tag, handle, offset,
		    *(u_int16_t *)dest);
		dest += 2;
	}
}

static void bus_space_write_raw_multi_4(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int8_t *, size_t);

static __inline__ void
bus_space_write_raw_multi_4(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int8_t *dest, size_t size)
{
	size >>= 2;
	while ((int)--size >= 0) {
		__bus_space_write_4(tag, handle, offset,
		    *(u_int32_t *)dest);
		dest += 4;
	}
}

static void bus_space_set_multi_1(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int8_t, size_t);

static __inline__ void
bus_space_set_multi_1(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int8_t value, size_t count)
{
	while ((int)--count >= 0)
		bus_space_write_1(tag, handle, offset, value);
}

static void bus_space_set_multi_2(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int16_t, size_t);

static __inline__ void
bus_space_set_multi_2(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int16_t value, size_t count)
{
	while ((int)--count >= 0)
		bus_space_write_2(tag, handle, offset, value);
}

static void bus_space_set_multi_4(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int32_t, size_t);

static __inline__ void
bus_space_set_multi_4(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int32_t value, size_t count)
{
	while ((int)--count >= 0)
		bus_space_write_4(tag, handle, offset, value);
}

static void bus_space_write_region_1(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int8_t *, size_t);

static __inline__ void
bus_space_write_region_1(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int8_t *dest, size_t count)
{
	while ((int)--count >= 0)
		bus_space_write_1(tag, handle, offset++, *dest++);
}

static void bus_space_write_region_2(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int16_t *, size_t);

static __inline__ void
bus_space_write_region_2(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int16_t *dest, size_t count)
{
	while ((int)--count >= 0) {
		bus_space_write_2(tag, handle, offset, *dest++);
		offset += 2;
	}
}

static void bus_space_write_region_4(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int32_t *, size_t);

static __inline__ void
bus_space_write_region_4(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int32_t *dest, size_t count)
{
	while ((int)--count >= 0) {
		bus_space_write_4(tag, handle, offset, *dest++);
		offset +=4;
	}
}

static void bus_space_write_raw_region_2(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int8_t *, size_t);

static __inline__ void
bus_space_write_raw_region_2(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int8_t *dest, size_t size)
{
	size >>= 1;
	while ((int)--size >= 0) {
		__bus_space_write_2(tag, handle, offset, *(u_int16_t *)dest);
		offset += 2;
		dest += 2;
	}
}

static void bus_space_write_raw_region_4(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int8_t *, size_t);

static __inline__ void
bus_space_write_raw_region_4(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int8_t *dest, size_t size)
{
	size >>= 2;
	while ((int)--size >= 0) {
		__bus_space_write_4(tag, handle, offset, *(u_int32_t *)dest);
		offset += 4;
		dest += 4;
	}
}

static void bus_space_read_region_1(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int8_t *, size_t);

static __inline__ void
bus_space_read_region_1(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int8_t *dest, size_t count)
{
	while ((int)--count >= 0)
		*dest++ = bus_space_read_1(tag, handle, offset++);
}

static void bus_space_read_region_2(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int16_t *, size_t);

static __inline__ void
bus_space_read_region_2(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int16_t *dest, size_t count)
{
	while ((int)--count >= 0) {
		*dest++ = bus_space_read_2(tag, handle, offset);
		offset += 2;
	}
}

static void bus_space_read_region_4(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int32_t *, size_t);

static __inline__ void
bus_space_read_region_4(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int32_t *dest, size_t count)
{
	while ((int)--count >= 0) {
		*dest++ = bus_space_read_4(tag, handle, offset);
		offset += 4;
	}
}

static void bus_space_set_region_1(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int8_t, size_t);

static __inline__ void
bus_space_set_region_1(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int8_t value, size_t count)
{
	while ((int)--count >= 0)
		bus_space_write_1(tag, handle, offset++, value);
}

static void bus_space_set_region_2(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int16_t, size_t);

static __inline__ void
bus_space_set_region_2(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int16_t value, size_t count)
{
	while ((int)--count >= 0) {
		bus_space_write_2(tag, handle, offset, value);
		offset += 2;
	}
}

static void bus_space_set_region_4(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int32_t, size_t);

static __inline__ void
bus_space_set_region_4(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int32_t value, size_t count)
{
	while ((int)--count >= 0) {
		bus_space_write_4(tag, handle, offset, value);
		offset += 4;
	}
}

static void bus_space_read_raw_region_2(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int8_t *, size_t);

static __inline__ void
bus_space_read_raw_region_2(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int8_t *dest, size_t size)
{
	size >>= 1;
	while ((int)--size >= 0) {
		*(u_int16_t *)dest = __bus_space_read_2(tag, handle, offset);
		offset += 2;
		dest += 2;
	}
}

static void bus_space_read_raw_region_4(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int8_t *, size_t);

static __inline__ void
bus_space_read_raw_region_4(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int8_t *dest, size_t size)
{
	size >>= 2;
	while ((int)--size >= 0) {
		*(u_int32_t *)dest = __bus_space_read_4(tag, handle, offset);
		offset += 4;
		dest += 4;
	}
}

/*
 * Flags used in various bus DMA methods.
 */
#define	BUS_DMA_WAITOK		0x000	/* safe to sleep (pseudo-flag) */
#define	BUS_DMA_NOWAIT		0x001	/* not safe to sleep */
#define	BUS_DMA_ALLOCNOW	0x002	/* perform resource allocation now */
#define	BUS_DMA_COHERENT	0x004	/* hint: map memory DMA coherent */
#define	BUS_DMA_STREAMING	0x008	/* hint: sequential, unidirectional */
#define	BUS_DMA_BUS1		0x010	/* placeholders for bus functions... */
#define	BUS_DMA_BUS2		0x020
#define	BUS_DMA_BUS3		0x040
#define	BUS_DMA_BUS4		0x080
#define	BUS_DMA_READ		0x100	/* mapping is device -> memory only */
#define	BUS_DMA_WRITE		0x200	/* mapping is memory -> device only */
#define	BUS_DMA_NOCACHE		0x400	/* hint: map non-cached memory */
#define	BUS_DMA_ZERO		0x800	/* zero memory in dmamem_alloc */

/* For devices that have a 24-bit address space */
#define BUS_DMA_24BIT		BUS_DMA_BUS1

/* Forwards needed by prototypes below. */
struct mbuf;
struct uio;

/*
 * Operations performed by bus_dmamap_sync().
 */
#define	BUS_DMASYNC_PREREAD	0x01	/* pre-read synchronization */
#define	BUS_DMASYNC_POSTREAD	0x02	/* post-read synchronization */
#define	BUS_DMASYNC_PREWRITE	0x04	/* pre-write synchronization */
#define	BUS_DMASYNC_POSTWRITE	0x08	/* post-write synchronization */

typedef struct sparc_bus_dma_tag	*bus_dma_tag_t;
typedef struct sparc_bus_dmamap		*bus_dmamap_t;

/*
 *	bus_dma_segment_t
 *
 *	Describes a single contiguous DMA transaction.  Values
 *	are suitable for programming into DMA registers.
 */
struct sparc_bus_dma_segment {
	bus_addr_t	ds_addr;	/* DVMA address */
	bus_size_t	ds_len;		/* length of transfer */
	bus_size_t	_ds_sgsize;	/* size of allocated DVMA segment */
	void		*_ds_mlist;	/* page list when dmamem_alloc'ed */
	vaddr_t		_ds_va;		/* VA when dmamem_map'ed */
};
typedef struct sparc_bus_dma_segment	bus_dma_segment_t;

/*
 *	bus_dma_tag_t
 *
 *	A machine-dependent opaque type describing the implementation of
 *	DMA for a given bus.
 */
struct sparc_bus_dma_tag {
	void	*_cookie;		/* cookie used in the guts */

	/*
	 * DMA mapping methods.
	 */
	int	(*_dmamap_create)(bus_dma_tag_t, bus_size_t, int,
		    bus_size_t, bus_size_t, int, bus_dmamap_t *);
	void	(*_dmamap_destroy)(bus_dma_tag_t, bus_dmamap_t);
	int	(*_dmamap_load)(bus_dma_tag_t, bus_dmamap_t, void *,
		    bus_size_t, struct proc *, int);
	int	(*_dmamap_load_mbuf)(bus_dma_tag_t, bus_dmamap_t,
		    struct mbuf *, int);
	int	(*_dmamap_load_uio)(bus_dma_tag_t, bus_dmamap_t,
		    struct uio *, int);
	int	(*_dmamap_load_raw)(bus_dma_tag_t, bus_dmamap_t,
		    bus_dma_segment_t *, int, bus_size_t, int);
	void	(*_dmamap_unload)(bus_dma_tag_t, bus_dmamap_t);
	void	(*_dmamap_sync)(bus_dma_tag_t, bus_dmamap_t,
		    bus_addr_t, bus_size_t, int);

	/*
	 * DMA memory utility functions.
	 */
	int	(*_dmamem_alloc)(bus_dma_tag_t, bus_size_t, bus_size_t,
		    bus_size_t, bus_dma_segment_t *, int, int *, int);
	void	(*_dmamem_free)(bus_dma_tag_t,
		    bus_dma_segment_t *, int);
	int	(*_dmamem_map)(bus_dma_tag_t, bus_dma_segment_t *,
		    int, size_t, caddr_t *, int);
	void	(*_dmamem_unmap)(bus_dma_tag_t, void *, size_t);
	paddr_t	(*_dmamem_mmap)(bus_dma_tag_t, bus_dma_segment_t *,
		    int, off_t, int, int);
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
#define	bus_dmamap_sync(t, p, o, l, ops)			\
	(void)((t)->_dmamap_sync ?				\
	    (*(t)->_dmamap_sync)((t), (p), (o), (l), (ops)) : (void)0)

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

#define bus_dmatag_subregion(t, mna, mxa, nt, f) EOPNOTSUPP
#define bus_dmatag_destroy(t)

/*
 *	bus_dmamap_t
 *
 *	Describes a DMA mapping.
 */
struct sparc_bus_dmamap {
	/*
	 * PRIVATE MEMBERS: not for use by machine-independent code.
	 */
	bus_size_t	_dm_size;	/* largest DMA transfer mappable */
	int		_dm_segcnt;	/* number of segs this map can map */
	bus_size_t	_dm_maxmaxsegsz; /* fixed largest possible segment */
	bus_size_t	_dm_boundary;	/* don't cross this */
	int		_dm_flags;	/* misc. flags */

	void		*_dm_cookie;	/* cookie for bus-specific functions */

	u_long		_dm_align;	/* DVMA alignment; must be a
					   multiple of the page size */
	u_long		_dm_ex_start;	/* constraints on DVMA map */
	u_long		_dm_ex_end;	/* allocations; used by the VME bus
					   driver and by the IOMMU driver
					   when mapping 24-bit devices */

	/*
	 * PUBLIC MEMBERS: these are used by machine-independent code.
	 */
	bus_size_t	dm_maxsegsz;	/* largest possible segment */
	bus_size_t	dm_mapsize;	/* size of the mapping */
	int		dm_nsegs;	/* # valid segments in mapping */
	bus_dma_segment_t dm_segs[1];	/* segments; variable length */
};

int	_bus_dmamap_create(bus_dma_tag_t, bus_size_t, int, bus_size_t,
	    bus_size_t, int, bus_dmamap_t *);
void	_bus_dmamap_destroy(bus_dma_tag_t, bus_dmamap_t);
int	_bus_dmamap_load_mbuf(bus_dma_tag_t, bus_dmamap_t,
	    struct mbuf *, int);
int	_bus_dmamap_load_uio(bus_dma_tag_t, bus_dmamap_t,
	    struct uio *, int);
int	_bus_dmamap_load_raw(bus_dma_tag_t, bus_dmamap_t,
	    bus_dma_segment_t *, int, bus_size_t, int);
void	_bus_dmamap_unload(bus_dma_tag_t, bus_dmamap_t);
void	_bus_dmamap_sync(bus_dma_tag_t, bus_dmamap_t, bus_addr_t,
	    bus_size_t, int);

int	_bus_dmamem_alloc(bus_dma_tag_t tag, bus_size_t size,
	    bus_size_t alignment, bus_size_t boundary,
	    bus_dma_segment_t *segs, int nsegs, int *rsegs, int flags);
void	_bus_dmamem_free(bus_dma_tag_t tag, bus_dma_segment_t *segs,
	    int nsegs);
void	_bus_dmamem_unmap(bus_dma_tag_t tag, void *kva,
	    size_t size);
paddr_t	_bus_dmamem_mmap(bus_dma_tag_t tag, bus_dma_segment_t *segs,
	    int nsegs, off_t off, int prot, int flags);

int	_bus_dmamem_alloc_range(bus_dma_tag_t tag, bus_size_t size,
	    bus_size_t alignment, bus_size_t boundary,
	    bus_dma_segment_t *segs, int nsegs, int *rsegs, int flags,
	    vaddr_t low, vaddr_t high);

vaddr_t	_bus_dma_valloc_skewed(size_t, u_long, u_long, u_long);

#endif	/* _MACHINE_BUS_H_ */
