/*	$OpenBSD: pcs_bus_io_common.c,v 1.4 1996/12/08 00:20:45 niklas Exp $	*/
/*	$NetBSD: pcs_bus_io_common.c,v 1.9 1996/10/23 04:12:31 cgd Exp $	*/

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
 *	CHIP_IO_BASE	Sparse I/O space base to use.
 */

#define	__C(A,B)	__CONCAT(A,B)
#define	__S(S)		__STRING(S)

/* mapping/unmapping */
int		__C(CHIP,_io_map) __P((void *, bus_addr_t, bus_size_t, int,
		    bus_space_handle_t *));
void		__C(CHIP,_io_unmap) __P((void *, bus_space_handle_t,
		    bus_size_t));
int		__C(CHIP,_io_subregion) __P((void *, bus_space_handle_t,
		    bus_size_t, bus_size_t, bus_space_handle_t *));

/* allocation/deallocation */
int		__C(CHIP,_io_alloc) __P((void *, bus_addr_t, bus_addr_t,
		    bus_size_t, bus_size_t, bus_addr_t, int, bus_addr_t *,
                    bus_space_handle_t *));
void		__C(CHIP,_io_free) __P((void *, bus_space_handle_t,
		    bus_size_t));

/* read (single) */
u_int8_t	__C(CHIP,_io_read_1) __P((void *, bus_space_handle_t,
		    bus_size_t));
u_int16_t	__C(CHIP,_io_read_2) __P((void *, bus_space_handle_t,
		    bus_size_t));
u_int32_t	__C(CHIP,_io_read_4) __P((void *, bus_space_handle_t,
		    bus_size_t));
u_int64_t	__C(CHIP,_io_read_8) __P((void *, bus_space_handle_t,
		    bus_size_t));

/* read multiple */
void		__C(CHIP,_io_read_multi_1) __P((void *, bus_space_handle_t,
		    bus_size_t, u_int8_t *, bus_size_t));
void		__C(CHIP,_io_read_multi_2) __P((void *, bus_space_handle_t,
		    bus_size_t, u_int16_t *, bus_size_t));
void		__C(CHIP,_io_read_multi_4) __P((void *, bus_space_handle_t,
		    bus_size_t, u_int32_t *, bus_size_t));
void		__C(CHIP,_io_read_multi_8) __P((void *, bus_space_handle_t,
		    bus_size_t, u_int64_t *, bus_size_t));

/* read region */
void		__C(CHIP,_io_read_region_1) __P((void *, bus_space_handle_t,
		    bus_size_t, u_int8_t *, bus_size_t));
void		__C(CHIP,_io_read_region_2) __P((void *, bus_space_handle_t,
		    bus_size_t, u_int16_t *, bus_size_t));
void		__C(CHIP,_io_read_region_4) __P((void *, bus_space_handle_t,
		    bus_size_t, u_int32_t *, bus_size_t));
void		__C(CHIP,_io_read_region_8) __P((void *, bus_space_handle_t,
		    bus_size_t, u_int64_t *, bus_size_t));

/* write (single) */
void		__C(CHIP,_io_write_1) __P((void *, bus_space_handle_t,
		    bus_size_t, u_int8_t));
void		__C(CHIP,_io_write_2) __P((void *, bus_space_handle_t,
		    bus_size_t, u_int16_t));
void		__C(CHIP,_io_write_4) __P((void *, bus_space_handle_t,
		    bus_size_t, u_int32_t));
void		__C(CHIP,_io_write_8) __P((void *, bus_space_handle_t,
		    bus_size_t, u_int64_t));

/* write multiple */
void		__C(CHIP,_io_write_multi_1) __P((void *, bus_space_handle_t,
		    bus_size_t, const u_int8_t *, bus_size_t));
void		__C(CHIP,_io_write_multi_2) __P((void *, bus_space_handle_t,
		    bus_size_t, const u_int16_t *, bus_size_t));
void		__C(CHIP,_io_write_multi_4) __P((void *, bus_space_handle_t,
		    bus_size_t, const u_int32_t *, bus_size_t));
void		__C(CHIP,_io_write_multi_8) __P((void *, bus_space_handle_t,
		    bus_size_t, const u_int64_t *, bus_size_t));

/* write region */
void		__C(CHIP,_io_write_region_1) __P((void *, bus_space_handle_t,
		    bus_size_t, const u_int8_t *, bus_size_t));
void		__C(CHIP,_io_write_region_2) __P((void *, bus_space_handle_t,
		    bus_size_t, const u_int16_t *, bus_size_t));
void		__C(CHIP,_io_write_region_4) __P((void *, bus_space_handle_t,
		    bus_size_t, const u_int32_t *, bus_size_t));
void		__C(CHIP,_io_write_region_8) __P((void *, bus_space_handle_t,
		    bus_size_t, const u_int64_t *, bus_size_t));

/* barrier */
void		__C(CHIP,_io_barrier) __P((void *, bus_space_handle_t,
		    bus_size_t, bus_size_t, int));

static struct alpha_bus_space __C(CHIP,_io_space) = {
	/* cookie */
	NULL,

	/* mapping/unmapping */
	__C(CHIP,_io_map),
	__C(CHIP,_io_unmap),
	__C(CHIP,_io_subregion),

	/* allocation/deallocation */
	__C(CHIP,_io_alloc),
	__C(CHIP,_io_free),
	
	/* read (single) */
	__C(CHIP,_io_read_1),
	__C(CHIP,_io_read_2),
	__C(CHIP,_io_read_4),
	__C(CHIP,_io_read_8),
	
	/* read multi */
	__C(CHIP,_io_read_multi_1),
	__C(CHIP,_io_read_multi_2),
	__C(CHIP,_io_read_multi_4),
	__C(CHIP,_io_read_multi_8),
	
	/* read region */
	__C(CHIP,_io_read_region_1),
	__C(CHIP,_io_read_region_2),
	__C(CHIP,_io_read_region_4),
	__C(CHIP,_io_read_region_8),
	
	/* write (single) */
	__C(CHIP,_io_write_1),
	__C(CHIP,_io_write_2),
	__C(CHIP,_io_write_4),
	__C(CHIP,_io_write_8),
	
	/* write multi */
	__C(CHIP,_io_write_multi_1),
	__C(CHIP,_io_write_multi_2),
	__C(CHIP,_io_write_multi_4),
	__C(CHIP,_io_write_multi_8),
	
	/* write region */
	__C(CHIP,_io_write_region_1),
	__C(CHIP,_io_write_region_2),
	__C(CHIP,_io_write_region_4),
	__C(CHIP,_io_write_region_8),

	/* set multi */
	/* XXX IMPLEMENT */

	/* set region */
	/* XXX IMPLEMENT */

	/* copy */
	/* XXX IMPLEMENT */

	/* barrier */
	__C(CHIP,_io_barrier),
};

bus_space_tag_t
__C(CHIP,_bus_io_init)(iov)
	void *iov;
{
        bus_space_tag_t h = &__C(CHIP,_io_space);;

	h->abs_cookie = iov;
	return (h);
}

int
__C(CHIP,_io_map)(v, ioaddr, iosize, cacheable, iohp)
	void *v;
	bus_addr_t ioaddr;
	bus_size_t iosize;
	int cacheable;
	bus_space_handle_t *iohp;
{

#ifdef CHIP_IO_W1_START
	if (ioaddr >= CHIP_IO_W1_START(v) &&
	    ioaddr <= CHIP_IO_W1_END(v)) {
		*iohp = (ALPHA_PHYS_TO_K0SEG(CHIP_IO_W1_BASE(v)) >> 5) +
		    (ioaddr & CHIP_IO_W1_MASK(v));
	} else
#endif
#ifdef CHIP_IO_W2_START
	if (ioaddr >= CHIP_IO_W2_START(v) &&
	    ioaddr <= CHIP_IO_W2_END(v)) {
		*iohp = (ALPHA_PHYS_TO_K0SEG(CHIP_IO_W2_BASE(v)) >> 5) +
		    (ioaddr & CHIP_IO_W2_MASK(v));
	} else
#endif
	{
		printf("\n");
#ifdef CHIP_IO_W1_START
		printf("%s: window[1]=0x%lx-0x%lx\n",
		    __S(__C(CHIP,_io_map)), CHIP_IO_W1_START(v),
		    CHIP_IO_W1_END(v)-1);
#endif
#ifdef CHIP_IO_W2_START
		printf("%s: window[2]=0x%lx-0x%lx\n",
		    __S(__C(CHIP,_io_map)), CHIP_IO_W2_START(v),
		    CHIP_IO_W2_END(v)-1);
#endif
		panic("%s: don't know how to map %lx non-cacheable",
		    __S(__C(CHIP,_io_map)), ioaddr);
	}

	/* XXX XXX XXX XXX XXX XXX */
	return (0);
}

void
__C(CHIP,_io_unmap)(v, ioh, iosize)
	void *v;
	bus_space_handle_t ioh;
	bus_size_t iosize;
{

	/* XXX nothing to do. */
	/* XXX XXX XXX XXX XXX XXX */
}

int
__C(CHIP,_io_subregion)(v, ioh, offset, size, nioh)
	void *v;
	bus_space_handle_t ioh, *nioh;
	bus_size_t offset, size;
{

	*nioh = ioh + offset;
	return (0);
}

int
__C(CHIP,_io_alloc)(v, rstart, rend, size, align, boundary, cacheable,
    addrp, bshp)
	void *v;
	bus_addr_t rstart, rend, *addrp;
	bus_size_t size, align, boundary;
	int cacheable;
	bus_space_handle_t *bshp;
{

	/* XXX XXX XXX XXX XXX XXX */
	panic("%s not implemented", __S(__C(CHIP,_io_alloc)));
}

void
__C(CHIP,_io_free)(v, bsh, size)
	void *v;
	bus_space_handle_t bsh;
	bus_size_t size;
{

	/* XXX XXX XXX XXX XXX XXX */
	panic("%s not implemented", __S(__C(CHIP,_io_free)));
}

u_int8_t
__C(CHIP,_io_read_1)(v, ioh, off)
	void *v;
	bus_space_handle_t ioh;
	bus_size_t off;
{
	register bus_space_handle_t tmpioh;
	register u_int32_t *port, val;
	register u_int8_t rval;
	register int offset;

	alpha_mb();

	tmpioh = ioh + off;
	offset = tmpioh & 3;
	port = (u_int32_t *)((tmpioh << 5) | (0 << 3));
	val = *port;
	rval = ((val) >> (8 * offset)) & 0xff;

	return rval;
}

u_int16_t
__C(CHIP,_io_read_2)(v, ioh, off)
	void *v;
	bus_space_handle_t ioh;
	bus_size_t off;
{
	register bus_space_handle_t tmpioh;
	register u_int32_t *port, val;
	register u_int16_t rval;
	register int offset;

	alpha_mb();

	tmpioh = ioh + off;
	offset = tmpioh & 3;
	port = (u_int32_t *)((tmpioh << 5) | (1 << 3));
	val = *port;
	rval = ((val) >> (8 * offset)) & 0xffff;

	return rval;
}

u_int32_t
__C(CHIP,_io_read_4)(v, ioh, off)
	void *v;
	bus_space_handle_t ioh;
	bus_size_t off;
{
	register bus_space_handle_t tmpioh;
	register u_int32_t *port, val;
	register u_int32_t rval;
	register int offset;

	alpha_mb();

	tmpioh = ioh + off;
	offset = tmpioh & 3;
	port = (u_int32_t *)((tmpioh << 5) | (3 << 3));
	val = *port;
#if 0
	rval = ((val) >> (8 * offset)) & 0xffffffff;
#else
	rval = val;
#endif

	return rval;
}

u_int64_t
__C(CHIP,_io_read_8)(v, ioh, off)
	void *v;
	bus_space_handle_t ioh;
	bus_size_t off;
{

	/* XXX XXX XXX */
	panic("%s not implemented", __S(__C(CHIP,_io_read_8)));
}

#define CHIP_io_read_multi_N(BYTES,TYPE)				\
void									\
__C(__C(CHIP,_io_read_multi_),BYTES)(v, h, o, a, c)			\
	void *v;							\
	bus_space_handle_t h;						\
	bus_size_t o, c;						\
	TYPE *a;							\
{									\
									\
	while (c-- > 0) {						\
		__C(CHIP,_io_barrier)(v, h, o, sizeof *a,		\
		    BUS_BARRIER_READ);					\
		*a++ = __C(__C(CHIP,_io_read_),BYTES)(v, h, o);		\
	}								\
}
CHIP_io_read_multi_N(1,u_int8_t)
CHIP_io_read_multi_N(2,u_int16_t)
CHIP_io_read_multi_N(4,u_int32_t)
CHIP_io_read_multi_N(8,u_int64_t)

#define CHIP_io_read_region_N(BYTES,TYPE)				\
void									\
__C(__C(CHIP,_io_read_region_),BYTES)(v, h, o, a, c)			\
	void *v;							\
	bus_space_handle_t h;						\
	bus_size_t o, c;						\
	TYPE *a;							\
{									\
									\
	while (c-- > 0) {						\
		*a++ = __C(__C(CHIP,_io_read_),BYTES)(v, h, o);		\
		o += sizeof *a;						\
	}								\
}
CHIP_io_read_region_N(1,u_int8_t)
CHIP_io_read_region_N(2,u_int16_t)
CHIP_io_read_region_N(4,u_int32_t)
CHIP_io_read_region_N(8,u_int64_t)

void
__C(CHIP,_io_write_1)(v, ioh, off, val)
	void *v;
	bus_space_handle_t ioh;
	bus_size_t off;
	u_int8_t val;
{
	register bus_space_handle_t tmpioh;
	register u_int32_t *port, nval;
	register int offset;

	tmpioh = ioh + off;
	offset = tmpioh & 3;
        nval = val << (8 * offset);
        port = (u_int32_t *)((tmpioh << 5) | (0 << 3));
        *port = nval;
        alpha_mb();
}

void
__C(CHIP,_io_write_2)(v, ioh, off, val)
	void *v;
	bus_space_handle_t ioh;
	bus_size_t off;
	u_int16_t val;
{
	register bus_space_handle_t tmpioh;
	register u_int32_t *port, nval;
	register int offset;

	tmpioh = ioh + off;
	offset = tmpioh & 3;
        nval = val << (8 * offset);
        port = (u_int32_t *)((tmpioh << 5) | (1 << 3));
        *port = nval;
        alpha_mb();
}

void
__C(CHIP,_io_write_4)(v, ioh, off, val)
	void *v;
	bus_space_handle_t ioh;
	bus_size_t off;
	u_int32_t val;
{
	register bus_space_handle_t tmpioh;
	register u_int32_t *port, nval;
	register int offset;

	tmpioh = ioh + off;
	offset = tmpioh & 3;
        nval = val /*<< (8 * offset)*/;
        port = (u_int32_t *)((tmpioh << 5) | (3 << 3));
        *port = nval;
        alpha_mb();
}

void
__C(CHIP,_io_write_8)(v, ioh, off, val)
	void *v;
	bus_space_handle_t ioh;
	bus_size_t off;
	u_int64_t val;
{

	/* XXX XXX XXX */
	panic("%s not implemented", __S(__C(CHIP,_io_write_8)));
	alpha_mb();
}

#define CHIP_io_write_multi_N(BYTES,TYPE)				\
void									\
__C(__C(CHIP,_io_write_multi_),BYTES)(v, h, o, a, c)			\
	void *v;							\
	bus_space_handle_t h;						\
	bus_size_t o, c;						\
	const TYPE *a;							\
{									\
									\
	while (c-- > 0) {						\
		__C(__C(CHIP,_io_write_),BYTES)(v, h, o, *a++);		\
		__C(CHIP,_io_barrier)(v, h, o, sizeof *a,		\
		    BUS_BARRIER_WRITE);					\
	}								\
}
CHIP_io_write_multi_N(1,u_int8_t)
CHIP_io_write_multi_N(2,u_int16_t)
CHIP_io_write_multi_N(4,u_int32_t)
CHIP_io_write_multi_N(8,u_int64_t)

#define CHIP_io_write_region_N(BYTES,TYPE)				\
void									\
__C(__C(CHIP,_io_write_region_),BYTES)(v, h, o, a, c)			\
	void *v;							\
	bus_space_handle_t h;						\
	bus_size_t o, c;						\
	const TYPE *a;							\
{									\
									\
	while (c-- > 0) {						\
		__C(__C(CHIP,_io_write_),BYTES)(v, h, o, *a++);		\
		o += sizeof *a;						\
	}								\
}
CHIP_io_write_region_N(1,u_int8_t)
CHIP_io_write_region_N(2,u_int16_t)
CHIP_io_write_region_N(4,u_int32_t)
CHIP_io_write_region_N(8,u_int64_t)

void
__C(CHIP,_io_barrier)(v, h, o, l, f)
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
