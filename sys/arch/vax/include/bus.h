/*	$OpenBSD: bus.h,v 1.3 2001/07/30 14:16:00 art Exp $	*/
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

#ifdef BUS_SPACE_DEBUG
#include <sys/systm.h> /* for printf() prototype */
/*
 * Macros for sanity-checking the aligned-ness of pointers passed to
 * bus space ops.  These are not strictly necessary on the VAX, but
 * could lead to performance improvements, and help catch problems
 * with drivers that would creep up on other architectures.
 */
#define	__BUS_SPACE_ALIGNED_ADDRESS(p, t)				\
	((((u_long)(p)) & (sizeof(t)-1)) == 0)

#define	__BUS_SPACE_ADDRESS_SANITY(p, t, d)				\
({									\
	if (__BUS_SPACE_ALIGNED_ADDRESS((p), t) == 0) {			\
		printf("%s 0x%lx not aligned to %d bytes %s:%d\n",	\
		    d, (u_long)(p), sizeof(t), __FILE__, __LINE__);	\
	}								\
	(void) 0;							\
})

#define BUS_SPACE_ALIGNED_POINTER(p, t) __BUS_SPACE_ALIGNED_ADDRESS(p, t)
#else
#define	__BUS_SPACE_ADDRESS_SANITY(p,t,d)	(void) 0
#define BUS_SPACE_ALIGNED_POINTER(p, t) ALIGNED_POINTER(p, t)
#endif /* BUS_SPACE_DEBUG */

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
	int		(*vbs_map) __P((void *, bus_addr_t, bus_size_t,
			    int, bus_space_handle_t *, int));
	void		(*vbs_unmap) __P((void *, bus_space_handle_t,
			    bus_size_t, int));
	int		(*vbs_subregion) __P((void *, bus_space_handle_t,
			    bus_size_t, bus_size_t, bus_space_handle_t *));

	/* allocation/deallocation */
	int		(*vbs_alloc) __P((void *, bus_addr_t, bus_addr_t,
			    bus_size_t, bus_size_t, bus_size_t, int,
			    bus_addr_t *, bus_space_handle_t *));
	void		(*vbs_free) __P((void *, bus_space_handle_t,
			    bus_size_t));
};

/*
 *	int bus_space_map  __P((bus_space_tag_t t, bus_addr_t addr,
 *	    bus_size_t size, int flags, bus_space_handle_t *bshp));
 *
 * Map a region of bus space.
 */

#define	BUS_SPACE_MAP_CACHEABLE		0x01
#define	BUS_SPACE_MAP_LINEAR		0x02
#define	BUS_SPACE_MAP_PREFETCHABLE	0x04

#define	bus_space_map(t, a, s, f, hp)					\
	(*(t)->vbs_map)((t)->vbs_cookie, (a), (s), (f), (hp), 1)
#define	vax_bus_space_map_noacct(t, a, s, f, hp)			\
	(*(t)->vbs_map)((t)->vbs_cookie, (a), (s), (f), (hp), 0)

/*
 *	int bus_space_unmap __P((bus_space_tag_t t,
 *	    bus_space_handle_t bsh, bus_size_t size));
 *
 * Unmap a region of bus space.
 */

#define bus_space_unmap(t, h, s)					\
	(*(t)->vbs_unmap)((t)->vbs_cookie, (h), (s), 1)
#define vax_bus_space_unmap_noacct(t, h, s)				\
	(*(t)->vbs_unmap)((t)->vbs_cookie, (h), (s), 0)

/*
 *	int bus_space_subregion __P((bus_space_tag_t t,
 *	    bus_space_handle_t bsh, bus_size_t offset, bus_size_t size,
 *	    bus_space_handle_t *nbshp));
 *
 * Get a new handle for a subregion of an already-mapped area of bus space.
 */

#define bus_space_subregion(t, h, o, s, nhp)				\
	(*(t)->vbs_subregion)((t)->vbs_cookie, (h), (o), (s), (nhp))

/*
 *	int bus_space_alloc __P((bus_space_tag_t t, bus_addr_t rstart,
 *	    bus_addr_t rend, bus_size_t size, bus_size_t align,
 *	    bus_size_t boundary, int flags, bus_addr_t *addrp,
 *	    bus_space_handle_t *bshp));
 *
 * Allocate a region of bus space.
 */

#define bus_space_alloc(t, rs, re, s, a, b, f, ap, hp)			\
	(*(t)->vbs_alloc)((t)->vbs_cookie, (rs), (re), (s), (a), (b),   \
	    (f), (ap), (hp))

/*
 *	int bus_space_free __P((bus_space_tag_t t,
 *	    bus_space_handle_t bsh, bus_size_t size));
 *
 * Free a region of bus space.
 */

#define bus_space_free(t, h, s)						\
	(*(t)->vbs_free)((t)->vbs_cookie, (h), (s))

/*
 *	u_intN_t bus_space_read_N __P((bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset));
 *
 * Read a 1, 2, 4, or 8 byte quantity from bus space
 * described by tag/handle/offset.
 */

#define	bus_space_read_1(t, h, o)					\
	    (*(volatile u_int8_t *)((h) + (o)))

#define	bus_space_read_2(t, h, o)					\
	 (__BUS_SPACE_ADDRESS_SANITY((h) + (o), u_int16_t, "bus addr"),	\
	    (*(volatile u_int16_t *)((h) + (o))))

#define	bus_space_read_4(t, h, o)					\
	 (__BUS_SPACE_ADDRESS_SANITY((h) + (o), u_int32_t, "bus addr"),	\
	    (*(volatile u_int32_t *)((h) + (o))))

#if 0	/* Cause a link error for bus_space_read_8 */
#define	bus_space_read_8(t, h, o)	!!! bus_space_read_8 unimplemented !!!
#endif

/*
 *	void bus_space_read_multi_N __P((bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset,
 *	    u_intN_t *addr, size_t count));
 *
 * Read `count' 1, 2, 4, or 8 byte quantities from bus space
 * described by tag/handle/offset and copy into buffer provided.
 */
static __inline void vax_mem_read_multi_1 __P((bus_space_tag_t,
	bus_space_handle_t, bus_size_t, u_int8_t *, size_t));
static __inline void vax_mem_read_multi_2 __P((bus_space_tag_t,
	bus_space_handle_t, bus_size_t, u_int16_t *, size_t));
static __inline void vax_mem_read_multi_4 __P((bus_space_tag_t,
	bus_space_handle_t, bus_size_t, u_int32_t *, size_t));

#define	bus_space_read_multi_1(t, h, o, a, c)				\
	vax_mem_read_multi_1((t), (h), (o), (a), (c))

#define bus_space_read_multi_2(t, h, o, a, c)				\
do {									\
	__BUS_SPACE_ADDRESS_SANITY((a), u_int16_t, "buffer");		\
	__BUS_SPACE_ADDRESS_SANITY((h) + (o), u_int16_t, "bus addr");	\
	vax_mem_read_multi_2((t), (h), (o), (a), (c));		\
} while (0)

#define bus_space_read_multi_4(t, h, o, a, c)				\
do {									\
	__BUS_SPACE_ADDRESS_SANITY((a), u_int32_t, "buffer");		\
	__BUS_SPACE_ADDRESS_SANITY((h) + (o), u_int32_t, "bus addr");	\
	vax_mem_read_multi_4((t), (h), (o), (a), (c));		\
} while (0)

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
 *	void bus_space_read_region_N __P((bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset,
 *	    u_intN_t *addr, size_t count));
 *
 * Read `count' 1, 2, 4, or 8 byte quantities from bus space
 * described by tag/handle and starting at `offset' and copy into
 * buffer provided.
 */

static __inline void vax_mem_read_region_1 __P((bus_space_tag_t,
	bus_space_handle_t, bus_size_t, u_int8_t *, size_t));
static __inline void vax_mem_read_region_2 __P((bus_space_tag_t,
	bus_space_handle_t, bus_size_t, u_int16_t *, size_t));
static __inline void vax_mem_read_region_4 __P((bus_space_tag_t,
	bus_space_handle_t, bus_size_t, u_int32_t *, size_t));

#define	bus_space_read_region_1(t, h, o, a, c)				\
do {									\
	vax_mem_read_region_1((t), (h), (o), (a), (c));		\
} while (0)

#define bus_space_read_region_2(t, h, o, a, c)				\
do {									\
	__BUS_SPACE_ADDRESS_SANITY((a), u_int16_t, "buffer");		\
	__BUS_SPACE_ADDRESS_SANITY((h) + (o), u_int16_t, "bus addr");	\
	vax_mem_read_region_2((t), (h), (o), (a), (c));		\
} while (0)

#define bus_space_read_region_4(t, h, o, a, c)				\
do {									\
	__BUS_SPACE_ADDRESS_SANITY((a), u_int32_t, "buffer");		\
	__BUS_SPACE_ADDRESS_SANITY((h) + (o), u_int32_t, "bus addr");	\
	vax_mem_read_region_4((t), (h), (o), (a), (c));		\
} while (0)

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
 *	void bus_space_write_N __P((bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset,
 *	    u_intN_t value));
 *
 * Write the 1, 2, 4, or 8 byte value `value' to bus space
 * described by tag/handle/offset.
 */

#define	bus_space_write_1(t, h, o, v)					\
do {									\
	((void)(*(volatile u_int8_t *)((h) + (o)) = (v)));		\
} while (0)

#define	bus_space_write_2(t, h, o, v)					\
do {									\
	__BUS_SPACE_ADDRESS_SANITY((h) + (o), u_int16_t, "bus addr");	\
	((void)(*(volatile u_int16_t *)((h) + (o)) = (v)));		\
} while (0)

#define	bus_space_write_4(t, h, o, v)					\
do {									\
	__BUS_SPACE_ADDRESS_SANITY((h) + (o), u_int32_t, "bus addr");	\
	((void)(*(volatile u_int32_t *)((h) + (o)) = (v)));		\
} while (0)

#if 0	/* Cause a link error for bus_space_write_8 */
#define	bus_space_write_8	!!! bus_space_write_8 not implemented !!!
#endif

/*
 *	void bus_space_write_multi_N __P((bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset,
 *	    const u_intN_t *addr, size_t count));
 *
 * Write `count' 1, 2, 4, or 8 byte quantities from the buffer
 * provided to bus space described by tag/handle/offset.
 */
static __inline void vax_mem_write_multi_1 __P((bus_space_tag_t,
	bus_space_handle_t, bus_size_t, const u_int8_t *, size_t));
static __inline void vax_mem_write_multi_2 __P((bus_space_tag_t,
	bus_space_handle_t, bus_size_t, const u_int16_t *, size_t));
static __inline void vax_mem_write_multi_4 __P((bus_space_tag_t,
	bus_space_handle_t, bus_size_t, const u_int32_t *, size_t));

#define	bus_space_write_multi_1(t, h, o, a, c)				\
do {									\
	vax_mem_write_multi_1((t), (h), (o), (a), (c));		\
} while (0)

#define bus_space_write_multi_2(t, h, o, a, c)				\
do {									\
	__BUS_SPACE_ADDRESS_SANITY((a), u_int16_t, "buffer");		\
	__BUS_SPACE_ADDRESS_SANITY((h) + (o), u_int16_t, "bus addr");	\
	vax_mem_write_multi_2((t), (h), (o), (a), (c));		\
} while (0)

#define bus_space_write_multi_4(t, h, o, a, c)				\
do {									\
	__BUS_SPACE_ADDRESS_SANITY((a), u_int32_t, "buffer");		\
	__BUS_SPACE_ADDRESS_SANITY((h) + (o), u_int32_t, "bus addr");	\
	vax_mem_write_multi_4((t), (h), (o), (a), (c));		\
} while (0)

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
 *	void bus_space_write_region_N __P((bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset,
 *	    const u_intN_t *addr, size_t count));
 *
 * Write `count' 1, 2, 4, or 8 byte quantities from the buffer provided
 * to bus space described by tag/handle starting at `offset'.
 */
static __inline void vax_mem_write_region_1 __P((bus_space_tag_t,
	bus_space_handle_t, bus_size_t, const u_int8_t *, size_t));
static __inline void vax_mem_write_region_2 __P((bus_space_tag_t,
	bus_space_handle_t, bus_size_t, const u_int16_t *, size_t));
static __inline void vax_mem_write_region_4 __P((bus_space_tag_t,
	bus_space_handle_t, bus_size_t, const u_int32_t *, size_t));

#define	bus_space_write_region_1(t, h, o, a, c)				\
	vax_mem_write_region_1((t), (h), (o), (a), (c))

#define bus_space_write_region_2(t, h, o, a, c)				\
do {									\
	__BUS_SPACE_ADDRESS_SANITY((a), u_int16_t, "buffer");		\
	__BUS_SPACE_ADDRESS_SANITY((h) + (o), u_int16_t, "bus addr");	\
	vax_mem_write_region_2((t), (h), (o), (a), (c));		\
} while (0)

#define bus_space_write_region_4(t, h, o, a, c)				\
do {									\
	__BUS_SPACE_ADDRESS_SANITY((a), u_int32_t, "buffer");		\
	__BUS_SPACE_ADDRESS_SANITY((h) + (o), u_int32_t, "bus addr");	\
	vax_mem_write_region_4((t), (h), (o), (a), (c));		\
} while (0)

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
 *	void bus_space_set_multi_N __P((bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset, u_intN_t val,
 *	    size_t count));
 *
 * Write the 1, 2, 4, or 8 byte value `val' to bus space described
 * by tag/handle/offset `count' times.
 */

static __inline void vax_mem_set_multi_1 __P((bus_space_tag_t,
	bus_space_handle_t, bus_size_t, u_int8_t, size_t));
static __inline void vax_mem_set_multi_2 __P((bus_space_tag_t,
	bus_space_handle_t, bus_size_t, u_int16_t, size_t));
static __inline void vax_mem_set_multi_4 __P((bus_space_tag_t,
	bus_space_handle_t, bus_size_t, u_int32_t, size_t));

#define	bus_space_set_multi_1(t, h, o, v, c)				\
	vax_mem_set_multi_1((t), (h), (o), (v), (c))

#define	bus_space_set_multi_2(t, h, o, v, c)				\
do {									\
	__BUS_SPACE_ADDRESS_SANITY((h) + (o), u_int16_t, "bus addr");	\
	vax_mem_set_multi_2((t), (h), (o), (v), (c));		\
} while (0)

#define	bus_space_set_multi_4(t, h, o, v, c)				\
do {									\
	__BUS_SPACE_ADDRESS_SANITY((h) + (o), u_int32_t, "bus addr");	\
	vax_mem_set_multi_4((t), (h), (o), (v), (c));		\
} while (0)

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
 *	void bus_space_set_region_N __P((bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset, u_intN_t val,
 *	    size_t count));
 *
 * Write `count' 1, 2, 4, or 8 byte value `val' to bus space described
 * by tag/handle starting at `offset'.
 */

static __inline void vax_mem_set_region_1 __P((bus_space_tag_t,
	bus_space_handle_t, bus_size_t, u_int8_t, size_t));
static __inline void vax_mem_set_region_2 __P((bus_space_tag_t,
	bus_space_handle_t, bus_size_t, u_int16_t, size_t));
static __inline void vax_mem_set_region_4 __P((bus_space_tag_t,
	bus_space_handle_t, bus_size_t, u_int32_t, size_t));

#define	bus_space_set_region_1(t, h, o, v, c)				\
	vax_mem_set_region_1((t), (h), (o), (v), (c))

#define	bus_space_set_region_2(t, h, o, v, c)				\
do {									\
	__BUS_SPACE_ADDRESS_SANITY((h) + (o), u_int16_t, "bus addr");	\
	vax_mem_set_region_2((t), (h), (o), (v), (c));		\
} while (0)

#define	bus_space_set_region_4(t, h, o, v, c)				\
do {									\
	__BUS_SPACE_ADDRESS_SANITY((h) + (o), u_int32_t, "bus addr");	\
	vax_mem_set_region_4((t), (h), (o), (v), (c));		\
} while (0)

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
 *	void bus_space_copy_region_N __P((bus_space_tag_t tag,
 *	    bus_space_handle_t bsh1, bus_size_t off1,
 *	    bus_space_handle_t bsh2, bus_size_t off2,
 *	    size_t count));
 *
 * Copy `count' 1, 2, 4, or 8 byte values from bus space starting
 * at tag/bsh1/off1 to bus space starting at tag/bsh2/off2.
 */

static __inline void vax_mem_copy_region_1 __P((bus_space_tag_t,
	bus_space_handle_t, bus_size_t, bus_space_handle_t,
	bus_size_t, size_t));
static __inline void vax_mem_copy_region_2 __P((bus_space_tag_t,
	bus_space_handle_t, bus_size_t, bus_space_handle_t,
	bus_size_t, size_t));
static __inline void vax_mem_copy_region_4 __P((bus_space_tag_t,
	bus_space_handle_t, bus_size_t, bus_space_handle_t,
	bus_size_t, size_t));

#define	bus_space_copy_region_1(t, h1, o1, h2, o2, c)			\
	vax_mem_copy_region_1((t), (h1), (o1), (h2), (o2), (c))

#define	bus_space_copy_region_2(t, h1, o1, h2, o2, c)			\
do {									\
	__BUS_SPACE_ADDRESS_SANITY((h1) + (o1), u_int16_t, "bus addr 1"); \
	__BUS_SPACE_ADDRESS_SANITY((h2) + (o2), u_int16_t, "bus addr 2"); \
	vax_mem_copy_region_2((t), (h1), (o1), (h2), (o2), (c));	\
} while (0)

#define	bus_space_copy_region_4(t, h1, o1, h2, o2, c)			\
do {									\
	__BUS_SPACE_ADDRESS_SANITY((h1) + (o1), u_int32_t, "bus addr 1"); \
	__BUS_SPACE_ADDRESS_SANITY((h2) + (o2), u_int32_t, "bus addr 2"); \
	vax_mem_copy_region_4((t), (h1), (o1), (h2), (o2), (c));	\
} while (0)

static __inline void
vax_mem_copy_region_1(t, h1, o1, h2, o2, c)
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
vax_mem_copy_region_2(t, h1, o1, h2, o2, c)
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
vax_mem_copy_region_4(t, h1, o1, h2, o2, c)
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
#define	bus_space_copy_region_8	!!! bus_space_copy_region_8 unimplemented !!!
#endif


/*
 * Bus read/write barrier methods.
 *
 *	void bus_space_barrier __P((bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset,
 *	    bus_size_t len, int flags));
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
#define	BUS_DMA_WAITOK		0x00	/* safe to sleep (pseudo-flag) */
#define	BUS_DMA_NOWAIT		0x01	/* not safe to sleep */
#define	BUS_DMA_ALLOCNOW	0x02	/* perform resource allocation now */
#define	BUS_DMA_COHERENT	0x04	/* hint: map memory DMA coherent */
#define	BUS_DMA_BUS1		0x10	/* placeholders for bus functions... */
#define	BUS_DMA_BUS2		0x20
#define	BUS_DMA_BUS3		0x40
#define	BUS_DMA_BUS4		0x80

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

/*
 *	vax_bus_t
 *
 *	Busses supported by NetBSD/vax, used by internal
 *	utility functions.  NOT TO BE USED BY MACHINE-INDEPENDENT
 *	CODE!
 */
typedef enum {
	VAX_BUS_MAINBUS,
	VAX_BUS_SBI,
	VAX_BUS_MASSBUS,
	VAX_BUS_UNIBUS,		/* Also handles QBUS */
	VAX_BUS_BI,
	VAX_BUS_XMI,
	VAX_BUS_TURBOCHANNEL
} vax_bus_t;

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
	 * Internal-use only utility methods.  NOT TO BE USED BY
	 * MACHINE-INDEPENDENT CODE!
	 */
	bus_dma_tag_t (*_get_tag) __P((bus_dma_tag_t, vax_bus_t));

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
		    bus_addr_t, bus_size_t, int));

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
	paddr_t	(*_dmamem_mmap) __P((bus_dma_tag_t, bus_dma_segment_t *,
		    int, off_t, int, int));
};

#define	vaxbus_dma_get_tag(t, b)				\
	(*(t)->_get_tag)(t, b)

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
	 * PRIVATE MEMBERS: not for use my machine-independent code.
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
int	_bus_dmamap_create __P((bus_dma_tag_t, bus_size_t, int, bus_size_t,
	    bus_size_t, int, bus_dmamap_t *));
void	_bus_dmamap_destroy __P((bus_dma_tag_t, bus_dmamap_t));

int	_bus_dmamap_load __P((bus_dma_tag_t, bus_dmamap_t,
	    void *, bus_size_t, struct proc *, int));
int	_bus_dmamap_load_mbuf __P((bus_dma_tag_t,
	    bus_dmamap_t, struct mbuf *, int));
int	_bus_dmamap_load_uio __P((bus_dma_tag_t,
	    bus_dmamap_t, struct uio *, int));
int	_bus_dmamap_load_raw __P((bus_dma_tag_t,
	    bus_dmamap_t, bus_dma_segment_t *, int, bus_size_t, int));

void	_bus_dmamap_unload __P((bus_dma_tag_t, bus_dmamap_t));
void	_bus_dmamap_sync __P((bus_dma_tag_t, bus_dmamap_t, bus_addr_t,
	    bus_size_t, int));

int	_bus_dmamem_alloc __P((bus_dma_tag_t tag, bus_size_t size,
	    bus_size_t alignment, bus_size_t boundary,
	    bus_dma_segment_t *segs, int nsegs, int *rsegs, int flags));
void	_bus_dmamem_free __P((bus_dma_tag_t tag, bus_dma_segment_t *segs,
	    int nsegs));
int	_bus_dmamem_map __P((bus_dma_tag_t tag, bus_dma_segment_t *segs,
	    int nsegs, size_t size, caddr_t *kvap, int flags));
void	_bus_dmamem_unmap __P((bus_dma_tag_t tag, caddr_t kva,
	    size_t size));
paddr_t	_bus_dmamem_mmap __P((bus_dma_tag_t tag, bus_dma_segment_t *segs,
	    int nsegs, off_t off, int prot, int flags));
#endif /* _VAX_BUS_DMA_PRIVATE */

#endif /* _VAX_BUS_H_ */
