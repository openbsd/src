/*	$OpenBSD: tc_bus_mem.c,v 1.5 1996/12/08 00:20:58 niklas Exp $	*/
/*	$NetBSD: tc_bus_mem.c,v 1.9 1996/10/23 04:12:37 cgd Exp $	*/

/*
 * Copyright (c) 1996 Carnegie-Mellon University.
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
 * Common TurboChannel Chipset "bus memory" functions.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <vm/vm.h>

#include <machine/bus.h>
#include <dev/tc/tcvar.h>

/* mapping/unmapping */
int		tc_mem_map __P((void *, bus_addr_t, bus_size_t, int,
		    bus_space_handle_t *));
void		tc_mem_unmap __P((void *, bus_space_handle_t, bus_size_t));
int		tc_mem_subregion __P((void *, bus_space_handle_t, bus_size_t,
		    bus_size_t, bus_space_handle_t *));

/* allocation/deallocation */
int		tc_mem_alloc __P((void *, bus_addr_t, bus_addr_t, bus_size_t,
		    bus_size_t, bus_addr_t, int, bus_addr_t *,
		    bus_space_handle_t *));
void		tc_mem_free __P((void *, bus_space_handle_t, bus_size_t));

/* read (single) */
u_int8_t	tc_mem_read_1 __P((void *, bus_space_handle_t, bus_size_t));
u_int16_t	tc_mem_read_2 __P((void *, bus_space_handle_t, bus_size_t));
u_int32_t	tc_mem_read_4 __P((void *, bus_space_handle_t, bus_size_t));
u_int64_t	tc_mem_read_8 __P((void *, bus_space_handle_t, bus_size_t));

/* read multiple */
void		tc_mem_read_multi_1 __P((void *, bus_space_handle_t,
		    bus_size_t, u_int8_t *, bus_size_t));
void		tc_mem_read_multi_2 __P((void *, bus_space_handle_t,
		    bus_size_t, u_int16_t *, bus_size_t));
void		tc_mem_read_multi_4 __P((void *, bus_space_handle_t,
		    bus_size_t, u_int32_t *, bus_size_t));
void		tc_mem_read_multi_8 __P((void *, bus_space_handle_t,
		    bus_size_t, u_int64_t *, bus_size_t));

/* read region */
void		tc_mem_read_region_1 __P((void *, bus_space_handle_t,
		    bus_size_t, u_int8_t *, bus_size_t));
void		tc_mem_read_region_2 __P((void *, bus_space_handle_t,
		    bus_size_t, u_int16_t *, bus_size_t));
void		tc_mem_read_region_4 __P((void *, bus_space_handle_t,
		    bus_size_t, u_int32_t *, bus_size_t));
void		tc_mem_read_region_8 __P((void *, bus_space_handle_t,
		    bus_size_t, u_int64_t *, bus_size_t));

/* write (single) */
void		tc_mem_write_1 __P((void *, bus_space_handle_t, bus_size_t,
		    u_int8_t));
void		tc_mem_write_2 __P((void *, bus_space_handle_t, bus_size_t,
		    u_int16_t));
void		tc_mem_write_4 __P((void *, bus_space_handle_t, bus_size_t,
		    u_int32_t));
void		tc_mem_write_8 __P((void *, bus_space_handle_t, bus_size_t,
		    u_int64_t));

/* write multiple */
void		tc_mem_write_multi_1 __P((void *, bus_space_handle_t,
		    bus_size_t, const u_int8_t *, bus_size_t));
void		tc_mem_write_multi_2 __P((void *, bus_space_handle_t,
		    bus_size_t, const u_int16_t *, bus_size_t));
void		tc_mem_write_multi_4 __P((void *, bus_space_handle_t,
		    bus_size_t, const u_int32_t *, bus_size_t));
void		tc_mem_write_multi_8 __P((void *, bus_space_handle_t,
		    bus_size_t, const u_int64_t *, bus_size_t));

/* write region */
void		tc_mem_write_region_1 __P((void *, bus_space_handle_t,
		    bus_size_t, const u_int8_t *, bus_size_t));
void		tc_mem_write_region_2 __P((void *, bus_space_handle_t,
		    bus_size_t, const u_int16_t *, bus_size_t));
void		tc_mem_write_region_4 __P((void *, bus_space_handle_t,
		    bus_size_t, const u_int32_t *, bus_size_t));
void		tc_mem_write_region_8 __P((void *, bus_space_handle_t,
		    bus_size_t, const u_int64_t *, bus_size_t));

/* barrier */
void		tc_mem_barrier __P((void *, bus_space_handle_t,
		    bus_size_t, bus_size_t, int));


static struct alpha_bus_space tc_mem_space = {
	/* cookie */
	NULL,

	/* mapping/unmapping */
	tc_mem_map,
	tc_mem_unmap,
	tc_mem_subregion,

	/* allocation/deallocation */
	tc_mem_alloc,
	tc_mem_free,

	/* read (single) */
	tc_mem_read_1,
	tc_mem_read_2,
	tc_mem_read_4,
	tc_mem_read_8,

	/* read multi */
	tc_mem_read_multi_1,
	tc_mem_read_multi_2,
	tc_mem_read_multi_4,
	tc_mem_read_multi_8,

	/* read region */
	tc_mem_read_region_1,
	tc_mem_read_region_2,
	tc_mem_read_region_4,
	tc_mem_read_region_8,

	/* write (single) */
	tc_mem_write_1,
	tc_mem_write_2,
	tc_mem_write_4,
	tc_mem_write_8,

	/* write multi */
	tc_mem_write_multi_1,
	tc_mem_write_multi_2,
	tc_mem_write_multi_4,
	tc_mem_write_multi_8,

	/* write region */
	tc_mem_write_region_1,
	tc_mem_write_region_2,
	tc_mem_write_region_4,
	tc_mem_write_region_8,

	/* set multi */
	/* XXX IMPLEMENT */

	/* set region */
	/* XXX IMPLEMENT */

	/* copy */
	/* XXX IMPLEMENT */

	/* barrier */
	tc_mem_barrier,
};

bus_space_tag_t
tc_bus_mem_init(memv)
	void *memv;
{
	bus_space_tag_t h = &tc_mem_space;

	h->abs_cookie = memv;
	return (h);
}

int
tc_mem_map(v, memaddr, memsize, cacheable, memhp)
	void *v;
	bus_addr_t memaddr;
	bus_size_t memsize;
	int cacheable;
	bus_space_handle_t *memhp;
{

	if (memaddr & 0x7)
		panic("tc_mem_map needs 8 byte alignment");
	if (cacheable)
		*memhp = ALPHA_PHYS_TO_K0SEG(memaddr);
	else
		*memhp = ALPHA_PHYS_TO_K0SEG(TC_DENSE_TO_SPARSE(memaddr));
	return (0);
}

void
tc_mem_unmap(v, memh, memsize)
	void *v;
	bus_space_handle_t memh;
	bus_size_t memsize;
{

	/* XXX XX XXX nothing to do. */
}

int
tc_mem_subregion(v, memh, offset, size, nmemh)
	void *v;
	bus_space_handle_t memh, *nmemh;
	bus_size_t offset, size;
{

	/* Disallow subregioning that would make the handle unaligned. */
	if ((offset & 0x7) != 0)
		return (1);

	if ((memh & TC_SPACE_SPARSE) != 0)
		*nmemh = memh + (offset << 1);
	else
		*nmemh = memh + offset;

	return (0);
}

int
tc_mem_alloc(v, rstart, rend, size, align, boundary, cacheable, addrp, bshp)
	void *v;
	bus_addr_t rstart, rend, *addrp;
	bus_size_t size, align, boundary;
	int cacheable;
	bus_space_handle_t *bshp;
{

	/* XXX XXX XXX XXX XXX XXX */
	panic("tc_mem_alloc unimplemented");
}

void
tc_mem_free(v, bsh, size)
	void *v;
	bus_space_handle_t bsh;
	bus_size_t size;
{

	/* XXX XXX XXX XXX XXX XXX */
	panic("tc_mem_free unimplemented");
}

u_int8_t
tc_mem_read_1(v, memh, off)
	void *v;
	bus_space_handle_t memh;
	bus_size_t off;
{
	volatile u_int8_t *p;

	alpha_mb();		/* XXX XXX XXX */

	if ((memh & TC_SPACE_SPARSE) != 0)
		panic("tc_mem_read_1 not implemented for sparse space");

	p = (u_int8_t *)(memh + off);
	return (*p);
}

u_int16_t
tc_mem_read_2(v, memh, off)
	void *v;
	bus_space_handle_t memh;
	bus_size_t off;
{
	volatile u_int16_t *p;

	alpha_mb();		/* XXX XXX XXX */

	if ((memh & TC_SPACE_SPARSE) != 0)
		panic("tc_mem_read_2 not implemented for sparse space");

	p = (u_int16_t *)(memh + off);
	return (*p);
}

u_int32_t
tc_mem_read_4(v, memh, off)
	void *v;
	bus_space_handle_t memh;
	bus_size_t off;
{
	volatile u_int32_t *p;

	alpha_mb();		/* XXX XXX XXX */

	if ((memh & TC_SPACE_SPARSE) != 0)
		/* Nothing special to do for 4-byte sparse space accesses */
		p = (u_int32_t *)(memh + (off << 1));
	else
		p = (u_int32_t *)(memh + off);
	return (*p);
}

u_int64_t
tc_mem_read_8(v, memh, off)
	void *v;
	bus_space_handle_t memh;
	bus_size_t off;
{
	volatile u_int64_t *p;

	alpha_mb();		/* XXX XXX XXX */

	if ((memh & TC_SPACE_SPARSE) != 0)
		panic("tc_mem_read_8 not implemented for sparse space");

	p = (u_int64_t *)(memh + off);
	return (*p);
}


#define	tc_mem_read_multi_N(BYTES,TYPE)					\
void									\
__abs_c(tc_mem_read_multi_,BYTES)(v, h, o, a, c)			\
	void *v;							\
	bus_space_handle_t h;						\
	bus_size_t o, c;						\
	TYPE *a;							\
{									\
									\
	while (c-- > 0) {						\
		tc_mem_barrier(v, h, o, sizeof *a, BUS_BARRIER_READ);	\
		*a++ = __abs_c(tc_mem_read_,BYTES)(v, h, o);		\
	}								\
}
tc_mem_read_multi_N(1,u_int8_t)
tc_mem_read_multi_N(2,u_int16_t)
tc_mem_read_multi_N(4,u_int32_t)
tc_mem_read_multi_N(8,u_int64_t)

#define	tc_mem_read_region_N(BYTES,TYPE)				\
void									\
__abs_c(tc_mem_read_region_,BYTES)(v, h, o, a, c)			\
	void *v;							\
	bus_space_handle_t h;						\
	bus_size_t o, c;						\
	TYPE *a;							\
{									\
									\
	while (c-- > 0) {						\
		*a++ = __abs_c(tc_mem_read_,BYTES)(v, h, o);		\
		o += sizeof *a;						\
	}								\
}
tc_mem_read_region_N(1,u_int8_t)
tc_mem_read_region_N(2,u_int16_t)
tc_mem_read_region_N(4,u_int32_t)
tc_mem_read_region_N(8,u_int64_t)

void
tc_mem_write_1(v, memh, off, val)
	void *v;
	bus_space_handle_t memh;
	bus_size_t off;
	u_int8_t val;
{

	if ((memh & TC_SPACE_SPARSE) != 0) {
		volatile u_int64_t *p, v;
		u_int64_t shift, msk;

		shift = off & 0x3;
		off &= 0x3;

		p = (u_int64_t *)(memh + (off << 1));

		msk = ~(0x1 << shift) & 0xf;
		v = (msk << 32) | (((u_int64_t)val) << (shift * 8));

		*p = val;
	} else {
		volatile u_int8_t *p;

		p = (u_int8_t *)(memh + off);
		*p = val;
	}
        alpha_mb();		/* XXX XXX XXX */
}

void
tc_mem_write_2(v, memh, off, val)
	void *v;
	bus_space_handle_t memh;
	bus_size_t off;
	u_int16_t val;
{

	if ((memh & TC_SPACE_SPARSE) != 0) {
		volatile u_int64_t *p, v;
		u_int64_t shift, msk;

		shift = off & 0x2;
		off &= 0x3;

		p = (u_int64_t *)(memh + (off << 1));

		msk = ~(0x3 << shift) & 0xf;
		v = (msk << 32) | (((u_int64_t)val) << (shift * 8));

		*p = val;
	} else {
		volatile u_int16_t *p;

		p = (u_int16_t *)(memh + off);
		*p = val;
	}
        alpha_mb();		/* XXX XXX XXX */
}

void
tc_mem_write_4(v, memh, off, val)
	void *v;
	bus_space_handle_t memh;
	bus_size_t off;
	u_int32_t val;
{
	volatile u_int32_t *p;

	if ((memh & TC_SPACE_SPARSE) != 0)
		/* Nothing special to do for 4-byte sparse space accesses */
		p = (u_int32_t *)(memh + (off << 1));
	else
		p = (u_int32_t *)(memh + off);
	*p = val;
        alpha_mb();		/* XXX XXX XXX */
}

void
tc_mem_write_8(v, memh, off, val)
	void *v;
	bus_space_handle_t memh;
	bus_size_t off;
	u_int64_t val;
{
	volatile u_int64_t *p;

	if ((memh & TC_SPACE_SPARSE) != 0)
		panic("tc_mem_read_8 not implemented for sparse space");

	p = (u_int64_t *)(memh + off);
	*p = val;
        alpha_mb();		/* XXX XXX XXX */
}
#define	tc_mem_write_multi_N(BYTES,TYPE)				\
void									\
__abs_c(tc_mem_write_multi_,BYTES)(v, h, o, a, c)			\
	void *v;							\
	bus_space_handle_t h;						\
	bus_size_t o, c;						\
	const TYPE *a;							\
{									\
									\
	while (c-- > 0) {						\
		__abs_c(tc_mem_write_,BYTES)(v, h, o, *a++);		\
		tc_mem_barrier(v, h, o, sizeof *a, BUS_BARRIER_WRITE);	\
	}								\
}
tc_mem_write_multi_N(1,u_int8_t)
tc_mem_write_multi_N(2,u_int16_t)
tc_mem_write_multi_N(4,u_int32_t)
tc_mem_write_multi_N(8,u_int64_t)

#define	tc_mem_write_region_N(BYTES,TYPE)				\
void									\
__abs_c(tc_mem_write_region_,BYTES)(v, h, o, a, c)			\
	void *v;							\
	bus_space_handle_t h;						\
	bus_size_t o, c;						\
	const TYPE *a;							\
{									\
									\
	while (c-- > 0) {						\
		__abs_c(tc_mem_write_,BYTES)(v, h, o, *a++);		\
		o += sizeof *a;						\
	}								\
}
tc_mem_write_region_N(1,u_int8_t)
tc_mem_write_region_N(2,u_int16_t)
tc_mem_write_region_N(4,u_int32_t)
tc_mem_write_region_N(8,u_int64_t)

void
tc_mem_barrier(v, h, o, l, f)
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
