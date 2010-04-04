/*	$OpenBSD: bus.h,v 1.15 2010/04/04 12:49:30 miod Exp $	*/
/*	$NetBSD: bus.h,v 1.14 2000/06/26 04:56:13 simonb Exp $	*/

/*-
 * Copyright (c) 1996, 1997, 1998 The NetBSD Foundation, Inc.
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

/*
 * Copyright (c) 1996 Charles M. Hannum.  All rights reserved.
 * Copyright (c) 1996 Christopher G. Demetriou.  All rights reserved.
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

#ifndef _VAX_BUS_H_
#define _VAX_BUS_H_

/*
 * Bus address and size types
 */
typedef u_long bus_addr_t;
typedef u_long bus_size_t;

/*
 * Access methods for bus resources and address space.
 */
typedef	struct vax_bus_space *bus_space_tag_t;
typedef	u_long bus_space_handle_t;

struct vax_bus_space {
	/* cookie */
	void		*vbs_cookie;

	/* mapping/unmapping */
	int		(*vbs_map)(void *, bus_addr_t, bus_size_t,
			    int, bus_space_handle_t *);
	void		(*vbs_unmap)(void *, bus_space_handle_t,
			    bus_size_t);
	int		(*vbs_subregion)(void *, bus_space_handle_t,
			    bus_size_t, bus_size_t, bus_space_handle_t *);

	/* allocation/deallocation */
	int		(*vbs_alloc)(void *, bus_addr_t, bus_addr_t,
			    bus_size_t, bus_size_t, bus_size_t, int,
			    bus_addr_t *, bus_space_handle_t *);
	void		(*vbs_free)(void *, bus_space_handle_t,
			    bus_size_t);

	/* get kernel virtual address */
	void *		(*vbs_vaddr)(void *, bus_space_handle_t);
};

/*
 *	int bus_space_map(bus_space_tag_t t, bus_addr_t addr,
 *	    bus_size_t size, int flags, bus_space_handle_t *bshp);
 *
 * Map a region of bus space.
 */

#define	BUS_SPACE_MAP_CACHEABLE		0x01
#define	BUS_SPACE_MAP_LINEAR		0x02
#define	BUS_SPACE_MAP_PREFETCHABLE	0x04

#define	bus_space_map(t, a, s, f, hp)					\
	(*(t)->vbs_map)((t)->vbs_cookie, (a), (s), (f), (hp))

/*
 *	int bus_space_unmap(bus_space_tag_t t,
 *	    bus_space_handle_t bsh, bus_size_t size);
 *
 * Unmap a region of bus space.
 */

#define bus_space_unmap(t, h, s)					\
	(*(t)->vbs_unmap)((t)->vbs_cookie, (h), (s))

/*
 *	int bus_space_subregion(bus_space_tag_t t,
 *	    bus_space_handle_t bsh, bus_size_t offset, bus_size_t size,
 *	    bus_space_handle_t *nbshp);
 *
 * Get a new handle for a subregion of an already-mapped area of bus space.
 */

#define bus_space_subregion(t, h, o, s, nhp)				\
	(*(t)->vbs_subregion)((t)->vbs_cookie, (h), (o), (s), (nhp))

/*
 *	int bus_space_alloc(bus_space_tag_t t, bus_addr_t rstart,
 *	    bus_addr_t rend, bus_size_t size, bus_size_t align,
 *	    bus_size_t boundary, int flags, bus_addr_t *addrp,
 *	    bus_space_handle_t *bshp);
 *
 * Allocate a region of bus space.
 */

#define bus_space_alloc(t, rs, re, s, a, b, f, ap, hp)			\
	(*(t)->vbs_alloc)((t)->vbs_cookie, (rs), (re), (s), (a), (b),   \
	    (f), (ap), (hp))

/*
 *	int bus_space_free(bus_space_tag_t t,
 *	    bus_space_handle_t bsh, bus_size_t size);
 *
 * Free a region of bus space.
 */

#define bus_space_free(t, h, s)						\
	(*(t)->vbs_free)((t)->vbs_cookie, (h), (s))

/*
 *	void *bus_space_vaddr(bus_space_tag_t t, bus_space_handle_t h);
 *
 * Get kernel virtual address.
 */

#define	bus_space_vaddr(t, h)						\
	(*(t)->vbs_vaddr)((t)->vbs_cookie, (h))

/*
 *	u_intN_t bus_space_read_N(bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset);
 *
 * Read a 1, 2, 4, or 8 byte quantity from bus space
 * described by tag/handle/offset.
 */

#define	bus_space_read_1(t, h, o)					\
	    (*(volatile u_int8_t *)((h) + (o)))

#define	bus_space_read_2(t, h, o)					\
	    (*(volatile u_int16_t *)((h) + (o)))

#define	bus_space_read_4(t, h, o)					\
	    (*(volatile u_int32_t *)((h) + (o)))

#if 0	/* Cause a link error for bus_space_read_8 */
#define	bus_space_read_8(t, h, o)	!!! bus_space_read_8 unimplemented !!!
#endif

/*
 *	void bus_space_read_multi_N(bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset,
 *	    u_intN_t *addr, size_t count);
 *
 * Read `count' 1, 2, 4, or 8 byte quantities from bus space
 * described by tag/handle/offset and copy into buffer provided.
 */
static __inline void vax_mem_read_multi_1(bus_space_tag_t,
	bus_space_handle_t, bus_size_t, u_int8_t *, size_t);
static __inline void vax_mem_read_multi_2(bus_space_tag_t,
	bus_space_handle_t, bus_size_t, u_int16_t *, size_t);
static __inline void vax_mem_read_multi_4(bus_space_tag_t,
	bus_space_handle_t, bus_size_t, u_int32_t *, size_t);

#define	bus_space_read_multi_1(t, h, o, a, c)				\
	vax_mem_read_multi_1((t), (h), (o), (a), (c))

#define bus_space_read_multi_2(t, h, o, a, c)				\
	vax_mem_read_multi_2((t), (h), (o), (a), (c))

#define bus_space_read_multi_4(t, h, o, a, c)				\
	vax_mem_read_multi_4((t), (h), (o), (a), (c))

#if 0	/* Cause a link error for bus_space_read_multi_8 */
#define	bus_space_read_multi_8	!!! bus_space_read_multi_8 unimplemented !!!
#endif

static __inline void
vax_mem_read_multi_1(t, h, o, a, c)
	bus_space_tag_t t;
	bus_space_handle_t h;
	bus_size_t o;
	u_int8_t *a;
	size_t c;
{
	const bus_addr_t addr = h + o;

	for (; c != 0; c--, a++)
		*a = *(volatile u_int8_t *)(addr);
}

static __inline void
vax_mem_read_multi_2(t, h, o, a, c)
	bus_space_tag_t t;
	bus_space_handle_t h;
	bus_size_t o;
	u_int16_t *a;
	size_t c;
{
	const bus_addr_t addr = h + o;

	for (; c != 0; c--, a++)
		*a = *(volatile u_int16_t *)(addr);
}

static __inline void
vax_mem_read_multi_4(t, h, o, a, c)
	bus_space_tag_t t;
	bus_space_handle_t h;
	bus_size_t o;
	u_int32_t *a;
	size_t c;
{
	const bus_addr_t addr = h + o;

	for (; c != 0; c--, a++)
		*a = *(volatile u_int32_t *)(addr);
}

/*
 *	void bus_space_read_region_N(bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset,
 *	    u_intN_t *addr, size_t count);
 *
 * Read `count' 1, 2, 4, or 8 byte quantities from bus space
 * described by tag/handle and starting at `offset' and copy into
 * buffer provided.
 */

static __inline void vax_mem_read_region_1(bus_space_tag_t,
	bus_space_handle_t, bus_size_t, u_int8_t *, size_t);
static __inline void vax_mem_read_region_2(bus_space_tag_t,
	bus_space_handle_t, bus_size_t, u_int16_t *, size_t);
static __inline void vax_mem_read_region_4(bus_space_tag_t,
	bus_space_handle_t, bus_size_t, u_int32_t *, size_t);

#define	bus_space_read_region_1(t, h, o, a, c)				\
	vax_mem_read_region_1((t), (h), (o), (a), (c))

#define bus_space_read_region_2(t, h, o, a, c)				\
	vax_mem_read_region_2((t), (h), (o), (a), (c))

#define bus_space_read_region_4(t, h, o, a, c)				\
	vax_mem_read_region_4((t), (h), (o), (a), (c))

#if 0	/* Cause a link error for bus_space_read_region_8 */
#define	bus_space_read_region_8					\
			!!! bus_space_read_region_8 unimplemented !!!
#endif

static __inline void
vax_mem_read_region_1(t, h, o, a, c)
	bus_space_tag_t t;
	bus_space_handle_t h;
	bus_size_t o;
	u_int8_t *a;
	size_t c;
{
	bus_addr_t addr = h + o;

	for (; c != 0; c--, addr++, a++)
		*a = *(volatile u_int8_t *)(addr);
}

static __inline void
vax_mem_read_region_2(t, h, o, a, c)
	bus_space_tag_t t;
	bus_space_handle_t h;
	bus_size_t o;
	u_int16_t *a;
	size_t c;
{
	bus_addr_t addr = h + o;

	for (; c != 0; c--, addr++, a++)
		*a = *(volatile u_int16_t *)(addr);
}

static __inline void
vax_mem_read_region_4(t, h, o, a, c)
	bus_space_tag_t t;
	bus_space_handle_t h;
	bus_size_t o;
	u_int32_t *a;
	size_t c;
{
	bus_addr_t addr = h + o;

	for (; c != 0; c--, addr++, a++)
		*a = *(volatile u_int32_t *)(addr);
}

/*
 *	void bus_space_write_N(bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset,
 *	    u_intN_t value);
 *
 * Write the 1, 2, 4, or 8 byte value `value' to bus space
 * described by tag/handle/offset.
 */

#define	bus_space_write_1(t, h, o, v)					\
	((void)(*(volatile u_int8_t *)((h) + (o)) = (v)))

#define	bus_space_write_2(t, h, o, v)					\
	((void)(*(volatile u_int16_t *)((h) + (o)) = (v)))

#define	bus_space_write_4(t, h, o, v)					\
	((void)(*(volatile u_int32_t *)((h) + (o)) = (v)))

#if 0	/* Cause a link error for bus_space_write_8 */
#define	bus_space_write_8	!!! bus_space_write_8 not implemented !!!
#endif

/*
 *	void bus_space_write_multi_N(bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset,
 *	    const u_intN_t *addr, size_t count);
 *
 * Write `count' 1, 2, 4, or 8 byte quantities from the buffer
 * provided to bus space described by tag/handle/offset.
 */
static __inline void vax_mem_write_multi_1(bus_space_tag_t,
	bus_space_handle_t, bus_size_t, const u_int8_t *, size_t);
static __inline void vax_mem_write_multi_2(bus_space_tag_t,
	bus_space_handle_t, bus_size_t, const u_int16_t *, size_t);
static __inline void vax_mem_write_multi_4(bus_space_tag_t,
	bus_space_handle_t, bus_size_t, const u_int32_t *, size_t);

#define	bus_space_write_multi_1(t, h, o, a, c)				\
	vax_mem_write_multi_1((t), (h), (o), (a), (c))

#define bus_space_write_multi_2(t, h, o, a, c)				\
	vax_mem_write_multi_2((t), (h), (o), (a), (c))

#define bus_space_write_multi_4(t, h, o, a, c)				\
	vax_mem_write_multi_4((t), (h), (o), (a), (c))

#if 0	/* Cause a link error for bus_space_write_multi_8 */
#define	bus_space_write_multi_8(t, h, o, a, c)				\
			!!! bus_space_write_multi_8 unimplemented !!!
#endif

static __inline void
vax_mem_write_multi_1(t, h, o, a, c)
	bus_space_tag_t t;
	bus_space_handle_t h;
	bus_size_t o;
	const u_int8_t *a;
	size_t c;
{
	const bus_addr_t addr = h + o;

	for (; c != 0; c--, a++)
		*(volatile u_int8_t *)(addr) = *a;
}

static __inline void
vax_mem_write_multi_2(t, h, o, a, c)
	bus_space_tag_t t;
	bus_space_handle_t h;
	bus_size_t o;
	const u_int16_t *a;
	size_t c;
{
	const bus_addr_t addr = h + o;

	for (; c != 0; c--, a++)
		*(volatile u_int16_t *)(addr) = *a;
}

static __inline void
vax_mem_write_multi_4(t, h, o, a, c)
	bus_space_tag_t t;
	bus_space_handle_t h;
	bus_size_t o;
	const u_int32_t *a;
	size_t c;
{
	const bus_addr_t addr = h + o;

	for (; c != 0; c--, a++)
		*(volatile u_int32_t *)(addr) = *a;
}

/*
 *	void bus_space_write_region_N(bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset,
 *	    const u_intN_t *addr, size_t count);
 *
 * Write `count' 1, 2, 4, or 8 byte quantities from the buffer provided
 * to bus space described by tag/handle starting at `offset'.
 */
static __inline void vax_mem_write_region_1(bus_space_tag_t,
	bus_space_handle_t, bus_size_t, const u_int8_t *, size_t);
static __inline void vax_mem_write_region_2(bus_space_tag_t,
	bus_space_handle_t, bus_size_t, const u_int16_t *, size_t);
static __inline void vax_mem_write_region_4(bus_space_tag_t,
	bus_space_handle_t, bus_size_t, const u_int32_t *, size_t);

#define	bus_space_write_region_1(t, h, o, a, c)				\
	vax_mem_write_region_1((t), (h), (o), (a), (c))

#define bus_space_write_region_2(t, h, o, a, c)				\
	vax_mem_write_region_2((t), (h), (o), (a), (c))

#define bus_space_write_region_4(t, h, o, a, c)				\
	vax_mem_write_region_4((t), (h), (o), (a), (c))

#if 0	/* Cause a link error for bus_space_write_region_8 */
#define	bus_space_write_region_8					\
			!!! bus_space_write_region_8 unimplemented !!!
#endif

static __inline void
vax_mem_write_region_1(t, h, o, a, c)
	bus_space_tag_t t;
	bus_space_handle_t h;
	bus_size_t o;
	const u_int8_t *a;
	size_t c;
{
	bus_addr_t addr = h + o;

	for (; c != 0; c--, addr++, a++)
		*(volatile u_int8_t *)(addr) = *a;
}

static __inline void
vax_mem_write_region_2(t, h, o, a, c)
	bus_space_tag_t t;
	bus_space_handle_t h;
	bus_size_t o;
	const u_int16_t *a;
	size_t c;
{
	bus_addr_t addr = h + o;

	for (; c != 0; c--, addr++, a++)
		*(volatile u_int16_t *)(addr) = *a;
}

static __inline void
vax_mem_write_region_4(t, h, o, a, c)
	bus_space_tag_t t;
	bus_space_handle_t h;
	bus_size_t o;
	const u_int32_t *a;
	size_t c;
{
	bus_addr_t addr = h + o;

	for (; c != 0; c--, addr++, a++)
		*(volatile u_int32_t *)(addr) = *a;
}

/*
 *	void bus_space_set_multi_N(bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset, u_intN_t val,
 *	    size_t count);
 *
 * Write the 1, 2, 4, or 8 byte value `val' to bus space described
 * by tag/handle/offset `count' times.
 */

static __inline void vax_mem_set_multi_1(bus_space_tag_t,
	bus_space_handle_t, bus_size_t, u_int8_t, size_t);
static __inline void vax_mem_set_multi_2(bus_space_tag_t,
	bus_space_handle_t, bus_size_t, u_int16_t, size_t);
static __inline void vax_mem_set_multi_4(bus_space_tag_t,
	bus_space_handle_t, bus_size_t, u_int32_t, size_t);

#define	bus_space_set_multi_1(t, h, o, v, c)				\
	vax_mem_set_multi_1((t), (h), (o), (v), (c))

#define	bus_space_set_multi_2(t, h, o, v, c)				\
	vax_mem_set_multi_2((t), (h), (o), (v), (c))

#define	bus_space_set_multi_4(t, h, o, v, c)				\
	vax_mem_set_multi_4((t), (h), (o), (v), (c))

static __inline void
vax_mem_set_multi_1(t, h, o, v, c)
	bus_space_tag_t t;
	bus_space_handle_t h;
	bus_size_t o;
	u_int8_t v;
	size_t c;
{
	bus_addr_t addr = h + o;

	while (c--)
		*(volatile u_int8_t *)(addr) = v;
}

static __inline void
vax_mem_set_multi_2(t, h, o, v, c)
	bus_space_tag_t t;
	bus_space_handle_t h;
	bus_size_t o;
	u_int16_t v;
	size_t c;
{
	bus_addr_t addr = h + o;

	while (c--)
		*(volatile u_int16_t *)(addr) = v;
}

static __inline void
vax_mem_set_multi_4(t, h, o, v, c)
	bus_space_tag_t t;
	bus_space_handle_t h;
	bus_size_t o;
	u_int32_t v;
	size_t c;
{
	bus_addr_t addr = h + o;

	while (c--)
		*(volatile u_int32_t *)(addr) = v;
}

#if 0	/* Cause a link error for bus_space_set_multi_8 */
#define	bus_space_set_multi_8 !!! bus_space_set_multi_8 unimplemented !!!
#endif

/*
 *	void bus_space_set_region_N(bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset, u_intN_t val,
 *	    size_t count);
 *
 * Write `count' 1, 2, 4, or 8 byte value `val' to bus space described
 * by tag/handle starting at `offset'.
 */

static __inline void vax_mem_set_region_1(bus_space_tag_t,
	bus_space_handle_t, bus_size_t, u_int8_t, size_t);
static __inline void vax_mem_set_region_2(bus_space_tag_t,
	bus_space_handle_t, bus_size_t, u_int16_t, size_t);
static __inline void vax_mem_set_region_4(bus_space_tag_t,
	bus_space_handle_t, bus_size_t, u_int32_t, size_t);

#define	bus_space_set_region_1(t, h, o, v, c)				\
	vax_mem_set_region_1((t), (h), (o), (v), (c))

#define	bus_space_set_region_2(t, h, o, v, c)				\
	vax_mem_set_region_2((t), (h), (o), (v), (c))

#define	bus_space_set_region_4(t, h, o, v, c)				\
	vax_mem_set_region_4((t), (h), (o), (v), (c))

static __inline void
vax_mem_set_region_1(t, h, o, v, c)
	bus_space_tag_t t;
	bus_space_handle_t h;
	bus_size_t o;
	u_int8_t v;
	size_t c;
{
	bus_addr_t addr = h + o;

	for (; c != 0; c--, addr++)
		*(volatile u_int8_t *)(addr) = v;
}

static __inline void
vax_mem_set_region_2(t, h, o, v, c)
	bus_space_tag_t t;
	bus_space_handle_t h;
	bus_size_t o;
	u_int16_t v;
	size_t c;
{
	bus_addr_t addr = h + o;

	for (; c != 0; c--, addr += 2)
		*(volatile u_int16_t *)(addr) = v;
}

static __inline void
vax_mem_set_region_4(t, h, o, v, c)
	bus_space_tag_t t;
	bus_space_handle_t h;
	bus_size_t o;
	u_int32_t v;
	size_t c;
{
	bus_addr_t addr = h + o;

	for (; c != 0; c--, addr += 4)
		*(volatile u_int32_t *)(addr) = v;
}

#if 0	/* Cause a link error for bus_space_set_region_8 */
#define	bus_space_set_region_8	!!! bus_space_set_region_8 unimplemented !!!
#endif

/*
 *	void bus_space_copy_N(bus_space_tag_t tag,
 *	    bus_space_handle_t bsh1, bus_size_t off1,
 *	    bus_space_handle_t bsh2, bus_size_t off2,
 *	    size_t count);
 *
 * Copy `count' 1, 2, 4, or 8 byte values from bus space starting
 * at tag/bsh1/off1 to bus space starting at tag/bsh2/off2.
 */

static __inline void vax_mem_copy_1(bus_space_tag_t,
	bus_space_handle_t, bus_size_t, bus_space_handle_t,
	bus_size_t, size_t);
static __inline void vax_mem_copy_2(bus_space_tag_t,
	bus_space_handle_t, bus_size_t, bus_space_handle_t,
	bus_size_t, size_t);
static __inline void vax_mem_copy_4(bus_space_tag_t,
	bus_space_handle_t, bus_size_t, bus_space_handle_t,
	bus_size_t, size_t);

#define	bus_space_copy_1(t, h1, o1, h2, o2, c)				\
	vax_mem_copy_1((t), (h1), (o1), (h2), (o2), (c))

#define	bus_space_copy_2(t, h1, o1, h2, o2, c)				\
	vax_mem_copy_2((t), (h1), (o1), (h2), (o2), (c))

#define	bus_space_copy_4(t, h1, o1, h2, o2, c)				\
	vax_mem_copy_4((t), (h1), (o1), (h2), (o2), (c))

static __inline void
vax_mem_copy_1(t, h1, o1, h2, o2, c)
	bus_space_tag_t t;
	bus_space_handle_t h1;
	bus_size_t o1;
	bus_space_handle_t h2;
	bus_size_t o2;
	size_t c;
{
	bus_addr_t addr1 = h1 + o1;
	bus_addr_t addr2 = h2 + o2;

	if (addr1 >= addr2) {
		/* src after dest: copy forward */
		for (; c != 0; c--, addr1++, addr2++)
			*(volatile u_int8_t *)(addr2) =
			    *(volatile u_int8_t *)(addr1);
	} else {
		/* dest after src: copy backwards */
		for (addr1 += (c - 1), addr2 += (c - 1);
		    c != 0; c--, addr1--, addr2--)
			*(volatile u_int8_t *)(addr2) =
			    *(volatile u_int8_t *)(addr1);
	}
}

static __inline void
vax_mem_copy_2(t, h1, o1, h2, o2, c)
	bus_space_tag_t t;
	bus_space_handle_t h1;
	bus_size_t o1;
	bus_space_handle_t h2;
	bus_size_t o2;
	size_t c;
{
	bus_addr_t addr1 = h1 + o1;
	bus_addr_t addr2 = h2 + o2;

	if (addr1 >= addr2) {
		/* src after dest: copy forward */
		for (; c != 0; c--, addr1 += 2, addr2 += 2)
			*(volatile u_int16_t *)(addr2) =
			    *(volatile u_int16_t *)(addr1);
	} else {
		/* dest after src: copy backwards */
		for (addr1 += 2 * (c - 1), addr2 += 2 * (c - 1);
		    c != 0; c--, addr1 -= 2, addr2 -= 2)
			*(volatile u_int16_t *)(addr2) =
			    *(volatile u_int16_t *)(addr1);
	}
}

static __inline void
vax_mem_copy_4(t, h1, o1, h2, o2, c)
	bus_space_tag_t t;
	bus_space_handle_t h1;
	bus_size_t o1;
	bus_space_handle_t h2;
	bus_size_t o2;
	size_t c;
{
	bus_addr_t addr1 = h1 + o1;
	bus_addr_t addr2 = h2 + o2;

	if (addr1 >= addr2) {
		/* src after dest: copy forward */
		for (; c != 0; c--, addr1 += 4, addr2 += 4)
			*(volatile u_int32_t *)(addr2) =
			    *(volatile u_int32_t *)(addr1);
	} else {
		/* dest after src: copy backwards */
		for (addr1 += 4 * (c - 1), addr2 += 4 * (c - 1);
		    c != 0; c--, addr1 -= 4, addr2 -= 4)
			*(volatile u_int32_t *)(addr2) =
			    *(volatile u_int32_t *)(addr1);
	}
}

#if 0	/* Cause a link error for bus_space_copy_8 */
#define	bus_space_copy_8	!!! bus_space_copy_8 unimplemented !!!
#endif


/*
 * Bus read/write barrier methods.
 *
 *	void bus_space_barrier(bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset,
 *	    bus_size_t len, int flags);
 *
 * Note: the vax does not currently require barriers, but we must
 * provide the flags to MI code.
 */
#define	bus_space_barrier(t, h, o, l, f)	\
	((void)((void)(t), (void)(h), (void)(o), (void)(l), (void)(f)))
#define	BUS_SPACE_BARRIER_READ	0x01		/* force read barrier */
#define	BUS_SPACE_BARRIER_WRITE	0x02		/* force write barrier */


/*
 * Flags used in various bus DMA methods.
 */
#define	BUS_DMA_WAITOK		0x000	/* safe to sleep (pseudo-flag) */
#define	BUS_DMA_NOWAIT		0x001	/* not safe to sleep */
#define	BUS_DMA_ALLOCNOW	0x002	/* perform resource allocation now */
#define	BUS_DMA_COHERENT	0x004	/* hint: map memory DMA coherent */
#define	BUS_DMA_BUS1		0x010	/* placeholders for bus functions... */
#define	BUS_DMA_BUS2		0x020
#define	BUS_DMA_BUS3		0x040
#define	BUS_DMA_BUS4		0x080
#define	BUS_DMA_STREAMING	0x100	/* hint: sequential, unidirectional */
#define	BUS_DMA_READ		0x200	/* mapping is device -> memory only */
#define	BUS_DMA_WRITE		0x400	/* mapping is memory -> device only */
#define	BUS_DMA_ZERO		0x800	/* zero memory in dmamem_alloc */

#define	VAX_BUS_DMA_SPILLPAGE	BUS_DMA_BUS1	/* VS4000 kludge */
/*
 * Private flags stored in the DMA map.
 */
#define DMAMAP_HAS_SGMAP	0x80000000	/* sgva/len are valid */

/* Forwards needed by prototypes below. */
struct mbuf;
struct uio;
struct vax_sgmap;

/*
 * Operations performed by bus_dmamap_sync().
 */
#define	BUS_DMASYNC_PREREAD	0x01	/* pre-read synchronization */
#define	BUS_DMASYNC_POSTREAD	0x02	/* post-read synchronization */
#define	BUS_DMASYNC_PREWRITE	0x04	/* pre-write synchronization */
#define	BUS_DMASYNC_POSTWRITE	0x08	/* post-write synchronization */

typedef struct vax_bus_dma_tag	*bus_dma_tag_t;
typedef struct vax_bus_dmamap	*bus_dmamap_t;

/*
 *	bus_dma_segment_t
 *
 *	Describes a single contiguous DMA transaction.  Values
 *	are suitable for programming into DMA registers.
 */
struct vax_bus_dma_segment {
	bus_addr_t	ds_addr;	/* DMA address */
	bus_size_t	ds_len;		/* length of transfer */
};
typedef struct vax_bus_dma_segment	bus_dma_segment_t;

struct proc;

/*
 *	bus_dma_tag_t
 *
 *	A machine-dependent opaque type describing the implementation of
 *	DMA for a given bus.
 */
struct vax_bus_dma_tag {
	void	*_cookie;		/* cookie used in the guts */
	bus_addr_t _wbase;		/* DMA window base */
	bus_size_t _wsize;		/* DMA window size */

	/*
	 * Some chipsets have a built-in boundary constraint, independent
	 * of what the device requests.  This allows that boundary to
	 * be specified.  If the device has a more restrictive contraint,
	 * the map will use that, otherwise this boundary will be used.
	 * This value is ignored if 0.
	 */
	bus_size_t _boundary;

	/*
	 * A bus may have more than one SGMAP window, so SGMAP
	 * windows also get a pointer to their SGMAP state.
	 */
	struct vax_sgmap *_sgmap;

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
	void	(*_dmamem_unmap)(bus_dma_tag_t, caddr_t, size_t);
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
	(*(t)->_dmamap_sync)((t), (p), (o), (l), (ops))
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
struct vax_bus_dmamap {
	/*
	 * PRIVATE MEMBERS: not for use by machine-independent code.
	 */
	bus_size_t	_dm_size;	/* largest DMA transfer mappable */
	int		_dm_segcnt;	/* number of segs this map can map */
	bus_size_t	_dm_maxsegsz;	/* largest possible segment */
	bus_size_t	_dm_boundary;	/* don't cross this */
	int		_dm_flags;	/* misc. flags */

	/*
	 * This is used only for SGMAP-mapped DMA, but we keep it
	 * here to avoid pointless indirection.
	 */
	int		_dm_pteidx;	/* PTE index */
	int		_dm_ptecnt;	/* PTE count */
	u_long		_dm_sgva;	/* allocated sgva */
	bus_size_t	_dm_sgvalen;	/* svga length */

	/*
	 * PUBLIC MEMBERS: these are used by machine-independent code.
	 */
	bus_size_t	dm_mapsize;	/* size of the mapping */
	int		dm_nsegs;	/* # valid segments in mapping */
	bus_dma_segment_t dm_segs[1];	/* segments; variable length */
};

#ifdef _VAX_BUS_DMA_PRIVATE
int	_bus_dmamap_create(bus_dma_tag_t, bus_size_t, int, bus_size_t,
	    bus_size_t, int, bus_dmamap_t *);
void	_bus_dmamap_destroy(bus_dma_tag_t, bus_dmamap_t);

int	_bus_dmamap_load(bus_dma_tag_t, bus_dmamap_t,
	    void *, bus_size_t, struct proc *, int);
int	_bus_dmamap_load_mbuf(bus_dma_tag_t,
	    bus_dmamap_t, struct mbuf *, int);
int	_bus_dmamap_load_uio(bus_dma_tag_t,
	    bus_dmamap_t, struct uio *, int);
int	_bus_dmamap_load_raw(bus_dma_tag_t,
	    bus_dmamap_t, bus_dma_segment_t *, int, bus_size_t, int);

void	_bus_dmamap_unload(bus_dma_tag_t, bus_dmamap_t);
void	_bus_dmamap_sync(bus_dma_tag_t, bus_dmamap_t, bus_addr_t,
	    bus_size_t, int);

int	_bus_dmamem_alloc(bus_dma_tag_t tag, bus_size_t size,
	    bus_size_t alignment, bus_size_t boundary,
	    bus_dma_segment_t *segs, int nsegs, int *rsegs, int flags);
void	_bus_dmamem_free(bus_dma_tag_t tag, bus_dma_segment_t *segs,
	    int nsegs);
int	_bus_dmamem_map(bus_dma_tag_t tag, bus_dma_segment_t *segs,
	    int nsegs, size_t size, caddr_t *kvap, int flags);
void	_bus_dmamem_unmap(bus_dma_tag_t tag, caddr_t kva,
	    size_t size);
paddr_t	_bus_dmamem_mmap(bus_dma_tag_t tag, bus_dma_segment_t *segs,
	    int nsegs, off_t off, int prot, int flags);
#endif /* _VAX_BUS_DMA_PRIVATE */

#endif /* _VAX_BUS_H_ */
