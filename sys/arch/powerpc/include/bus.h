/*	$OpenBSD: bus.h,v 1.5 2000/01/14 05:42:16 rahnds Exp $	*/

/*
 * Copyright (c) 1997 Per Fogelstrom.  All rights reserved.
 * Copyright (c) 1996 Niklas Hallqvist.  All rights reserved.
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
 *      This product includes software developed by Christopher G. Demetriou
 *	for the NetBSD Project.
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

#ifndef _MACHINE_BUS_H_
#define _MACHINE_BUS_H_

#include <machine/pio.h>

#ifdef __STDC__
#define CAT(a,b)	a##b
#define CAT3(a,b,c)	a##b##c
#else
#define CAT(a,b)	a/**/b
#define CAT3(a,b,c)	a/**/b/**/c
#endif

/*
 * Bus access types.
 */
typedef u_int32_t bus_addr_t;
typedef u_int32_t bus_size_t;
typedef u_int32_t bus_space_handle_t;
typedef struct ppc_bus_space *bus_space_tag_t;

struct ppc_bus_space {
	u_int32_t	bus_base;
	u_int8_t	bus_reverse;	/* Reverse bytes */
};
#define POWERPC_BUS_TAG_BASE(x)  ((x)->bus_base)

extern struct ppc_bus_space ppc_isa_io, ppc_isa_mem;

/*
 * Access methods for bus resources
 */
int	bus_space_map __P((bus_space_tag_t t, bus_addr_t addr,
	    bus_size_t size, int cacheable, bus_space_handle_t *bshp));
void	bus_space_unmap __P((bus_space_tag_t t, bus_space_handle_t bsh,
	    bus_size_t size));
int	bus_space_subregion __P((bus_space_tag_t t, bus_space_handle_t bsh,
	    bus_size_t offset, bus_size_t size, bus_space_handle_t *nbshp));

#define bus_space_read(n,m)						      \
static __inline CAT3(u_int,m,_t)					      \
CAT(bus_space_read_,n)(bus_space_tag_t bst, bus_space_handle_t bsh,	      \
     bus_addr_t ba)							      \
{									      \
    if(bst->bus_reverse)						      \
	return CAT3(in,m,rb)((volatile CAT3(u_int,m,_t) *)(bsh + (ba)));      \
    else								      \
	return CAT(in,m)((volatile CAT3(u_int,m,_t) *)(bsh + (ba)));	      \
}

bus_space_read(1,8)
bus_space_read(2,16)
bus_space_read(4,32)

#define	bus_space_read_8	!!! bus_space_read_8 unimplemented !!!

#define bus_space_write(n,m)						      \
static __inline void							      \
CAT(bus_space_write_,n)(bus_space_tag_t bst, bus_space_handle_t bsh,	      \
     bus_addr_t ba, CAT3(u_int,m,_t) x)					      \
{									      \
    if(bst->bus_reverse)						      \
	CAT3(out,m,rb)((volatile CAT3(u_int,m,_t) *)(bsh + (ba)), x);	      \
    else								      \
	CAT(out,m)((volatile CAT3(u_int,m,_t) *)(bsh + (ba)), x);	      \
}

bus_space_write(1,8)
bus_space_write(2,16)
bus_space_write(4,32)

#define	bus_space_write_8	!!! bus_space_write_8 unimplemented !!!

#define bus_space_read_multi(n, m)					      \
static __inline void						       	      \
CAT(bus_space_read_multi_,n)(bus_space_tag_t bst, bus_space_handle_t bsh,     \
    bus_size_t ba, CAT3(u_int,m,_t) *buf, bus_size_t cnt)		      \
{									      \
	while (cnt--)							      \
		*buf++ = CAT(bus_space_read_,n)(bst, bsh, ba);		      \
}

bus_space_read_multi(1,8)
bus_space_read_multi(2,16)
bus_space_read_multi(4,32)

#define	bus_space_read_multi_8	!!! bus_space_read_multi_8 not implemented !!!


#define	bus_space_write_multi_8	!!! bus_space_write_multi_8 not implemented !!!

#define bus_space_write_multi(n, m)					      \
static __inline void								      \
CAT(bus_space_write_multi_,n)(bus_space_tag_t bst, bus_space_handle_t bsh,    \
    bus_size_t ba, const CAT3(u_int,m,_t) *buf, bus_size_t cnt)		      \
{									      \
	while (cnt--)							      \
		CAT(bus_space_write_,n)(bst, bsh, ba, *buf++);		      \
}

bus_space_write_multi(1,8)
bus_space_write_multi(2,16)
bus_space_write_multi(4,32)

#define	bus_space_write_multi_8	!!! bus_space_write_multi_8 not implemented !!!

/* These are OpenBSD extensions to the general NetBSD bus interface.  */
void
bus_space_read_raw_multi_1(bus_space_tag_t bst, bus_space_handle_t bsh,
	bus_addr_t ba, u_int8_t *dst, bus_size_t size);
void
bus_space_read_raw_multi_2(bus_space_tag_t bst, bus_space_handle_t bsh,
	bus_addr_t ba, u_int16_t *dst, bus_size_t size);
void
bus_space_read_raw_multi_4(bus_space_tag_t bst, bus_space_handle_t bsh,
	bus_addr_t ba, u_int32_t *dst, bus_size_t size);

void
bus_space_write_raw_multi_1(bus_space_tag_t bst, bus_space_handle_t bsh,
	bus_addr_t ba, const u_int8_t *src, bus_size_t size);
void
bus_space_write_raw_multi_2(bus_space_tag_t bst, bus_space_handle_t bsh,
	bus_addr_t ba, const u_int16_t *src, bus_size_t size);
void
bus_space_write_raw_multi_4(bus_space_tag_t bst, bus_space_handle_t bsh,
	bus_addr_t ba, const u_int32_t *src, bus_size_t size);

#define	bus_space_read_raw_multi_8 \
    !!! bus_space_read_raw_multi_8 not implemented !!!

#define	bus_space_write_raw_multi_8 \
    !!! bus_space_write_raw_multi_8 not implemented !!!

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

typedef struct powerpc_bus_dma_tag	*bus_dma_tag_t;
typedef struct powerpc_bus_dmamap	*bus_dmamap_t;

/*
 *	bus_dma_segment_t
 *
 *	Describes a single contiguous DMA transaction.  Values
 *	are suitable for programming into DMA registers.
 */
struct powerpc_bus_dma_segment {
	bus_addr_t	ds_addr;	/* DMA address */
	bus_size_t	ds_len;		/* length of transfer */
};
typedef struct powerpc_bus_dma_segment	bus_dma_segment_t;

/*
 *	bus_dma_tag_t
 *
 *	A machine-dependent opaque type describing the implementation of
 *	DMA for a given bus.
 */

struct powerpc_bus_dma_tag {
	void	*_cookie;		/* cookie used in the guts */

	/*
	 * DMA mapping methods.
	 */
	int	(*_dmamap_create) __P((void *, bus_size_t, int,
		    bus_size_t, bus_size_t, int, bus_dmamap_t *));
	void	(*_dmamap_destroy) __P((void *, bus_dmamap_t));
	int	(*_dmamap_load) __P((void *, bus_dmamap_t, void *,
		    bus_size_t, struct proc *, int));
	int	(*_dmamap_load_mbuf) __P((void *, bus_dmamap_t,
		    struct mbuf *, int));
	int	(*_dmamap_load_uio) __P((void *, bus_dmamap_t,
		    struct uio *, int));
	int	(*_dmamap_load_raw) __P((void *, bus_dmamap_t,
		    bus_dma_segment_t *, int, bus_size_t, int));
	void	(*_dmamap_unload) __P((void *, bus_dmamap_t));
	void	(*_dmamap_sync) __P((void *, bus_dmamap_t, bus_dmasync_op_t));

	/*
	 * DMA memory utility functions.
	 */
	int	(*_dmamem_alloc) __P((void *, bus_size_t, bus_size_t,
		    bus_size_t, bus_dma_segment_t *, int, int *, int));
	void	(*_dmamem_free) __P((void *, bus_dma_segment_t *, int));
	int	(*_dmamem_map) __P((void *, bus_dma_segment_t *,
		    int, size_t, caddr_t *, int));
	void	(*_dmamem_unmap) __P((void *, caddr_t, size_t));
	int	(*_dmamem_mmap) __P((void *, bus_dma_segment_t *,
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
	(*(t)->_dmamem_alloc)((t)->_cookie, (s), (a), (b), (sg), (n), (r), (f))
#define	bus_dmamem_free(t, sg, n)				\
	(*(t)->_dmamem_free)((t)->_cookie, (sg), (n))
#define	bus_dmamem_map(t, sg, n, s, k, f)			\
	(*(t)->_dmamem_map)((t)->_cookie, (sg), (n), (s), (k), (f))
#define	bus_dmamem_unmap(t, k, s)				\
	(*(t)->_dmamem_unmap)((t)->_cookie, (k), (s))
#define	bus_dmamem_mmap(t, sg, n, o, p, f)			\
	(*(t)->_dmamem_mmap)((t)->_cookie, (sg), (n), (o), (p), (f))

int	_dmamap_create __P((void *, bus_size_t, int,
	    bus_size_t, bus_size_t, int, bus_dmamap_t *));
void	_dmamap_destroy __P((void *, bus_dmamap_t));
int	_dmamap_load __P((void *, bus_dmamap_t, void *,
	    bus_size_t, struct proc *, int));
int	_dmamap_load_mbuf __P((void *, bus_dmamap_t, struct mbuf *, int));
int	_dmamap_load_uio __P((void *, bus_dmamap_t, struct uio *, int));
int	_dmamap_load_raw __P((void *, bus_dmamap_t,
	    bus_dma_segment_t *, int, bus_size_t, int));
void	_dmamap_unload __P((void *, bus_dmamap_t));
void	_dmamap_sync __P((void *, bus_dmamap_t, bus_dmasync_op_t));

int	_dmamem_alloc __P((void *, bus_size_t, bus_size_t,
	    bus_size_t, bus_dma_segment_t *, int, int *, int));
void	_dmamem_free __P((void *, bus_dma_segment_t *, int));
int	_dmamem_map __P((void *, bus_dma_segment_t *,
	    int, size_t, caddr_t *, int));
void	_dmamem_unmap __P((void *, caddr_t, size_t));
int	_dmamem_mmap __P((void *, bus_dma_segment_t *, int, int, int, int));

/*
 *	bus_dmamap_t
 *
 *	Describes a DMA mapping.
 */
struct powerpc_bus_dmamap {
	/*
	 * PRIVATE MEMBERS: not for use by machine-independent code.
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
