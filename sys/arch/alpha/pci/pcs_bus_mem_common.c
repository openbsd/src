/*	$OpenBSD: pcs_bus_mem_common.c,v 1.4 1996/12/08 00:20:46 niklas Exp $	*/
/*	$NetBSD: pcs_bus_mem_common.c,v 1.10 1996/10/23 04:12:32 cgd Exp $	*/

/*
 * Copyright (c) 1995, 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

/*
 * Common PCI Chipset "bus I/O" functions, for chipsets which have to
 * deal with only a single PCI interface chip in a machine.
 *
 * uses:
 *	CHIP		name of the 'chip' it's being compiled for.
 *	CHIP_D_MEM_BASE	Dense Mem space base to use.
 *	CHIP_S_MEM_BASE	Sparse Mem space base to use.
 */

#define	__C(A,B)	__CONCAT(A,B)
#define	__S(S)		__STRING(S)

/* mapping/unmapping */
int		__C(CHIP,_mem_map) __P((void *, bus_addr_t, bus_size_t, int,
		    bus_space_handle_t *));
void		__C(CHIP,_mem_unmap) __P((void *, bus_space_handle_t,
		    bus_size_t));
int		__C(CHIP,_mem_subregion) __P((void *, bus_space_handle_t,
		    bus_size_t, bus_size_t, bus_space_handle_t *));

/* allocation/deallocation */
int		__C(CHIP,_mem_alloc) __P((void *, bus_addr_t, bus_addr_t,
		    bus_size_t, bus_size_t, bus_addr_t, int, bus_addr_t *,
                    bus_space_handle_t *));
void		__C(CHIP,_mem_free) __P((void *, bus_space_handle_t,
		    bus_size_t));

/* read (single) */
u_int8_t	__C(CHIP,_mem_read_1) __P((void *, bus_space_handle_t,
		    bus_size_t));
u_int16_t	__C(CHIP,_mem_read_2) __P((void *, bus_space_handle_t,
		    bus_size_t));
u_int32_t	__C(CHIP,_mem_read_4) __P((void *, bus_space_handle_t,
		    bus_size_t));
u_int64_t	__C(CHIP,_mem_read_8) __P((void *, bus_space_handle_t,
		    bus_size_t));

/* read multiple */
void		__C(CHIP,_mem_read_multi_1) __P((void *, bus_space_handle_t,
		    bus_size_t, u_int8_t *, bus_size_t));
void		__C(CHIP,_mem_read_multi_2) __P((void *, bus_space_handle_t,
		    bus_size_t, u_int16_t *, bus_size_t));
void		__C(CHIP,_mem_read_multi_4) __P((void *, bus_space_handle_t,
		    bus_size_t, u_int32_t *, bus_size_t));
void		__C(CHIP,_mem_read_multi_8) __P((void *, bus_space_handle_t,
		    bus_size_t, u_int64_t *, bus_size_t));

/* read region */
void		__C(CHIP,_mem_read_region_1) __P((void *, bus_space_handle_t,
		    bus_size_t, u_int8_t *, bus_size_t));
void		__C(CHIP,_mem_read_region_2) __P((void *, bus_space_handle_t,
		    bus_size_t, u_int16_t *, bus_size_t));
void		__C(CHIP,_mem_read_region_4) __P((void *, bus_space_handle_t,
		    bus_size_t, u_int32_t *, bus_size_t));
void		__C(CHIP,_mem_read_region_8) __P((void *, bus_space_handle_t,
		    bus_size_t, u_int64_t *, bus_size_t));

/* write (single) */
void		__C(CHIP,_mem_write_1) __P((void *, bus_space_handle_t,
		    bus_size_t, u_int8_t));
void		__C(CHIP,_mem_write_2) __P((void *, bus_space_handle_t,
		    bus_size_t, u_int16_t));
void		__C(CHIP,_mem_write_4) __P((void *, bus_space_handle_t,
		    bus_size_t, u_int32_t));
void		__C(CHIP,_mem_write_8) __P((void *, bus_space_handle_t,
		    bus_size_t, u_int64_t));

/* write multiple */
void		__C(CHIP,_mem_write_multi_1) __P((void *, bus_space_handle_t,
		    bus_size_t, const u_int8_t *, bus_size_t));
void		__C(CHIP,_mem_write_multi_2) __P((void *, bus_space_handle_t,
		    bus_size_t, const u_int16_t *, bus_size_t));
void		__C(CHIP,_mem_write_multi_4) __P((void *, bus_space_handle_t,
		    bus_size_t, const u_int32_t *, bus_size_t));
void		__C(CHIP,_mem_write_multi_8) __P((void *, bus_space_handle_t,
		    bus_size_t, const u_int64_t *, bus_size_t));

/* write region */
void		__C(CHIP,_mem_write_region_1) __P((void *, bus_space_handle_t,
		    bus_size_t, const u_int8_t *, bus_size_t));
void		__C(CHIP,_mem_write_region_2) __P((void *, bus_space_handle_t,
		    bus_size_t, const u_int16_t *, bus_size_t));
void		__C(CHIP,_mem_write_region_4) __P((void *, bus_space_handle_t,
		    bus_size_t, const u_int32_t *, bus_size_t));
void		__C(CHIP,_mem_write_region_8) __P((void *, bus_space_handle_t,
		    bus_size_t, const u_int64_t *, bus_size_t));

/* barrier */
void		__C(CHIP,_mem_barrier) __P((void *, bus_space_handle_t,
		    bus_size_t, bus_size_t, int));

static struct alpha_bus_space __C(CHIP,_mem_space) = {
	/* cookie */
	NULL,

	/* mapping/unmapping */
	__C(CHIP,_mem_map),
	__C(CHIP,_mem_unmap),
	__C(CHIP,_mem_subregion),

	/* allocation/deallocation */
	__C(CHIP,_mem_alloc),
	__C(CHIP,_mem_free),
	
	/* read (single) */
	__C(CHIP,_mem_read_1),
	__C(CHIP,_mem_read_2),
	__C(CHIP,_mem_read_4),
	__C(CHIP,_mem_read_8),
	
	/* read multi */
	__C(CHIP,_mem_read_multi_1),
	__C(CHIP,_mem_read_multi_2),
	__C(CHIP,_mem_read_multi_4),
	__C(CHIP,_mem_read_multi_8),
	
	/* read region */
	__C(CHIP,_mem_read_region_1),
	__C(CHIP,_mem_read_region_2),
	__C(CHIP,_mem_read_region_4),
	__C(CHIP,_mem_read_region_8),
	
	/* write (single) */
	__C(CHIP,_mem_write_1),
	__C(CHIP,_mem_write_2),
	__C(CHIP,_mem_write_4),
	__C(CHIP,_mem_write_8),
	
	/* write multi */
	__C(CHIP,_mem_write_multi_1),
	__C(CHIP,_mem_write_multi_2),
	__C(CHIP,_mem_write_multi_4),
	__C(CHIP,_mem_write_multi_8),
	
	/* write region */
	__C(CHIP,_mem_write_region_1),
	__C(CHIP,_mem_write_region_2),
	__C(CHIP,_mem_write_region_4),
	__C(CHIP,_mem_write_region_8),

	/* set multi */
	/* XXX IMPLEMENT */

	/* set region */
	/* XXX IMPLEMENT */

	/* copy */
	/* XXX IMPLEMENT */

	/* barrier */
	__C(CHIP,_mem_barrier),
};

bus_space_tag_t
__C(CHIP,_bus_mem_init)(iov)
	void *iov;
{
        bus_space_tag_t h = &__C(CHIP,_mem_space);;

	h->abs_cookie = iov;
	return (h);
}

int
__C(CHIP,_mem_map)(v, memaddr, memsize, cacheable, memhp)
	void *v;
	bus_addr_t memaddr;
	bus_size_t memsize;
	int cacheable;
	bus_space_handle_t *memhp;
{

	if (cacheable) {
#ifdef CHIP_D_MEM_W1_START
                if (memaddr >= CHIP_D_MEM_W1_START(v) &&
                    memaddr <= CHIP_D_MEM_W1_END(v)) {
                        *memhp = ALPHA_PHYS_TO_K0SEG(CHIP_D_MEM_W1_BASE(v)) +
                            (memaddr & CHIP_D_MEM_W1_MASK(v));
                } else
#endif
		{
			printf("\n");
#ifdef CHIP_D_MEM_W1_START
			printf("%s: window[1]=0x%lx-0x%lx\n",
			    __S(__C(CHIP,_mem_map)), CHIP_D_MEM_W1_START(v),
			    CHIP_D_MEM_W1_END(v)-1);
#endif
			panic("%s: don't know how to map %lx cacheable",
			    __S(__C(CHIP,_mem_map)), memaddr);
		}
	} else {
#ifdef CHIP_S_MEM_W1_START
		if (memaddr >= CHIP_S_MEM_W1_START(v) &&
		    memaddr <= CHIP_S_MEM_W1_END(v)) {
			*memhp = (ALPHA_PHYS_TO_K0SEG(CHIP_S_MEM_W1_BASE(v)) >> 5) +
			    (memaddr & CHIP_S_MEM_W1_MASK(v));
		} else
#endif
#ifdef CHIP_S_MEM_W2_START
		if (memaddr >= CHIP_S_MEM_W2_START(v) &&
		    memaddr <= CHIP_S_MEM_W2_END(v)) {
			*memhp = (ALPHA_PHYS_TO_K0SEG(CHIP_S_MEM_W2_BASE(v)) >> 5) +
			    (memaddr & CHIP_S_MEM_W2_MASK(v));
		} else
#endif
#ifdef CHIP_S_MEM_W3_START
		if (memaddr >= CHIP_S_MEM_W3_START(v) &&
		    memaddr <= CHIP_S_MEM_W3_END(v)) {
			*memhp = (ALPHA_PHYS_TO_K0SEG(CHIP_S_MEM_W3_BASE(v)) >> 5) +
			    (memaddr & CHIP_S_MEM_W3_MASK(v));
		} else
#endif
		{
			printf("\n");
#ifdef CHIP_S_MEM_W1_START
			printf("%s: window[1]=0x%lx-0x%lx\n",
			    __S(__C(CHIP,_mem_map)), CHIP_S_MEM_W1_START(v),
			    CHIP_S_MEM_W1_END(v)-1);
#endif
#ifdef CHIP_S_MEM_W2_START
			printf("%s: window[2]=0x%lx-0x%lx\n",
			    __S(__C(CHIP,_mem_map)), CHIP_S_MEM_W2_START(v),
			    CHIP_S_MEM_W2_END(v)-1);
#endif
#ifdef CHIP_S_MEM_W3_START
			printf("%s: window[3]=0x%lx-0x%lx\n",
			    __S(__C(CHIP,_mem_map)), CHIP_S_MEM_W3_START(v),
			    CHIP_S_MEM_W3_END(v)-1);
#endif
			panic("%s: don't know how to map %lx non-cacheable",
			    __S(__C(CHIP,_mem_map)), memaddr);
		}
	}

	/* XXX XXX XXX XXX XXX XXX */
	return (0);
}

void
__C(CHIP,_mem_unmap)(v, memh, memsize)
	void *v;
	bus_space_handle_t memh;
	bus_size_t memsize;
{

	/* XXX nothing to do. */
	/* XXX XXX XXX XXX XXX XXX */
}

int
__C(CHIP,_mem_subregion)(v, memh, offset, size, nmemh)
	void *v;
	bus_space_handle_t memh, *nmemh;
	bus_size_t offset, size;
{

	*nmemh = memh + offset;
	return (0);
}

int
__C(CHIP,_mem_alloc)(v, rstart, rend, size, align, boundary, cacheable,
    addrp, bshp)
	void *v;
	bus_addr_t rstart, rend, *addrp;
	bus_size_t size, align, boundary;
	int cacheable;
	bus_space_handle_t *bshp;
{

	/* XXX XXX XXX XXX XXX XXX */
	panic("%s not implemented", __S(__C(CHIP,_mem_alloc)));
}

void
__C(CHIP,_mem_free)(v, bsh, size)
	void *v;
	bus_space_handle_t bsh;
	bus_size_t size;
{

	/* XXX XXX XXX XXX XXX XXX */
	panic("%s not implemented", __S(__C(CHIP,_mem_free)));
}

u_int8_t
__C(CHIP,_mem_read_1)(v, memh, off)
	void *v;
	bus_space_handle_t memh;
	bus_size_t off;
{
	register bus_space_handle_t tmpmemh;
	register u_int32_t *port, val;
	register u_int8_t rval;
	register int offset;

	alpha_mb();

	if ((memh >> 63) != 0)
		return (*(u_int8_t *)(memh + off));

	tmpmemh = memh + off;
	offset = tmpmemh & 3;
	port = (u_int32_t *)((tmpmemh << 5) | (0 << 3));
	val = *port;
	rval = ((val) >> (8 * offset)) & 0xff;

	return rval;
}

u_int16_t
__C(CHIP,_mem_read_2)(v, memh, off)
	void *v;
	bus_space_handle_t memh;
	bus_size_t off;
{
	register bus_space_handle_t tmpmemh;
	register u_int32_t *port, val;
	register u_int16_t rval;
	register int offset;

	alpha_mb();

	if ((memh >> 63) != 0)
		return (*(u_int16_t *)(memh + off));

	tmpmemh = memh + off;
	offset = tmpmemh & 3;
	port = (u_int32_t *)((tmpmemh << 5) | (1 << 3));
	val = *port;
	rval = ((val) >> (8 * offset)) & 0xffff;

	return rval;
}

u_int32_t
__C(CHIP,_mem_read_4)(v, memh, off)
	void *v;
	bus_space_handle_t memh;
	bus_size_t off;
{
	register bus_space_handle_t tmpmemh;
	register u_int32_t *port, val;
	register u_int32_t rval;
	register int offset;

	alpha_mb();

	if ((memh >> 63) != 0)
		return (*(u_int32_t *)(memh + off));

	tmpmemh = memh + off;
	offset = tmpmemh & 3;
	port = (u_int32_t *)((tmpmemh << 5) | (3 << 3));
	val = *port;
#if 0
	rval = ((val) >> (8 * offset)) & 0xffffffff;
#else
	rval = val;
#endif

	return rval;
}

u_int64_t
__C(CHIP,_mem_read_8)(v, memh, off)
	void *v;
	bus_space_handle_t memh;
	bus_size_t off;
{

	alpha_mb();

        if ((memh >> 63) != 0)
                return (*(u_int64_t *)(memh + off));

	/* XXX XXX XXX */
	panic("%s not implemented", __S(__C(CHIP,_mem_read_8)));
}

#define CHIP_mem_read_multi_N(BYTES,TYPE)				\
void									\
__C(__C(CHIP,_mem_read_multi_),BYTES)(v, h, o, a, c)			\
	void *v;							\
	bus_space_handle_t h;						\
	bus_size_t o, c;						\
	TYPE *a;							\
{									\
									\
	while (c-- > 0) {						\
		__C(CHIP,_mem_barrier)(v, h, o, sizeof *a,		\
		    BUS_BARRIER_READ);					\
		*a++ = __C(__C(CHIP,_mem_read_),BYTES)(v, h, o);	\
	}								\
}
CHIP_mem_read_multi_N(1,u_int8_t)
CHIP_mem_read_multi_N(2,u_int16_t)
CHIP_mem_read_multi_N(4,u_int32_t)
CHIP_mem_read_multi_N(8,u_int64_t)

#define CHIP_mem_read_region_N(BYTES,TYPE)				\
void									\
__C(__C(CHIP,_mem_read_region_),BYTES)(v, h, o, a, c)			\
	void *v;							\
	bus_space_handle_t h;						\
	bus_size_t o, c;						\
	TYPE *a;							\
{									\
									\
	while (c-- > 0) {						\
		*a++ = __C(__C(CHIP,_mem_read_),BYTES)(v, h, o);	\
		o += sizeof *a;						\
	}								\
}
CHIP_mem_read_region_N(1,u_int8_t)
CHIP_mem_read_region_N(2,u_int16_t)
CHIP_mem_read_region_N(4,u_int32_t)
CHIP_mem_read_region_N(8,u_int64_t)

void
__C(CHIP,_mem_write_1)(v, memh, off, val)
	void *v;
	bus_space_handle_t memh;
	bus_size_t off;
	u_int8_t val;
{
	register bus_space_handle_t tmpmemh;
	register u_int32_t *port, nval;
	register int offset;

	if ((memh >> 63) != 0)
		(*(u_int8_t *)(memh + off)) = val;
	else {
		tmpmemh = memh + off;
		offset = tmpmemh & 3;
		nval = val << (8 * offset);
		port = (u_int32_t *)((tmpmemh << 5) | (0 << 3));
		*port = nval;
	}
        alpha_mb();
}

void
__C(CHIP,_mem_write_2)(v, memh, off, val)
	void *v;
	bus_space_handle_t memh;
	bus_size_t off;
	u_int16_t val;
{
	register bus_space_handle_t tmpmemh;
	register u_int32_t *port, nval;
	register int offset;

	if ((memh >> 63) != 0)
		(*(u_int16_t *)(memh + off)) = val;
	else {
		tmpmemh = memh + off;
		offset = tmpmemh & 3;
	        nval = val << (8 * offset);
	        port = (u_int32_t *)((tmpmemh << 5) | (1 << 3));
	        *port = nval;
	}
        alpha_mb();
}

void
__C(CHIP,_mem_write_4)(v, memh, off, val)
	void *v;
	bus_space_handle_t memh;
	bus_size_t off;
	u_int32_t val;
{
	register bus_space_handle_t tmpmemh;
	register u_int32_t *port, nval;
	register int offset;

	if ((memh >> 63) != 0)
		(*(u_int32_t *)(memh + off)) = val;
	else {
		tmpmemh = memh + off;
		offset = tmpmemh & 3;
	        nval = val /*<< (8 * offset)*/;
	        port = (u_int32_t *)((tmpmemh << 5) | (3 << 3));
	        *port = nval;
	}
        alpha_mb();
}

void
__C(CHIP,_mem_write_8)(v, memh, off, val)
	void *v;
	bus_space_handle_t memh;
	bus_size_t off;
	u_int64_t val;
{

	if ((memh >> 63) != 0)
		(*(u_int64_t *)(memh + off)) = val;
	else {
		/* XXX XXX XXX */
		panic("%s not implemented",
		    __S(__C(CHIP,_mem_write_8)));
	}
	alpha_mb();
}

#define CHIP_mem_write_multi_N(BYTES,TYPE)				\
void									\
__C(__C(CHIP,_mem_write_multi_),BYTES)(v, h, o, a, c)			\
	void *v;							\
	bus_space_handle_t h;						\
	bus_size_t o, c;						\
	const TYPE *a;							\
{									\
									\
	while (c-- > 0) {						\
		__C(__C(CHIP,_mem_write_),BYTES)(v, h, o, *a++);	\
		__C(CHIP,_mem_barrier)(v, h, o, sizeof *a,		\
		    BUS_BARRIER_WRITE);					\
	}								\
}
CHIP_mem_write_multi_N(1,u_int8_t)
CHIP_mem_write_multi_N(2,u_int16_t)
CHIP_mem_write_multi_N(4,u_int32_t)
CHIP_mem_write_multi_N(8,u_int64_t)

#define CHIP_mem_write_region_N(BYTES,TYPE)				\
void									\
__C(__C(CHIP,_mem_write_region_),BYTES)(v, h, o, a, c)			\
	void *v;							\
	bus_space_handle_t h;						\
	bus_size_t o, c;						\
	const TYPE *a;							\
{									\
									\
	while (c-- > 0) {						\
		__C(__C(CHIP,_mem_write_),BYTES)(v, h, o, *a++);	\
		o += sizeof *a;						\
	}								\
}
CHIP_mem_write_region_N(1,u_int8_t)
CHIP_mem_write_region_N(2,u_int16_t)
CHIP_mem_write_region_N(4,u_int32_t)
CHIP_mem_write_region_N(8,u_int64_t)

void
__C(CHIP,_mem_barrier)(v, h, o, l, f)
	void *v;
	bus_space_handle_t h;
	bus_size_t o, l;
	int f;
{

	if ((f & BUS_BARRIER_READ) != 0)
		alpha_mb();
	else if ((f & BUS_BARRIER_WRITE) != 0)
		alpha_wmb();
}
