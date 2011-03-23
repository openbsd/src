/*	$NetBSD: bus.h,v 1.1 2001/06/06 17:37:37 matt Exp $	*/
/*	$OpenBSD: bus_mi.h,v 1.13 2011/03/23 16:54:36 pirofti Exp $	*/

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
 * Copyright (c) 1996 Jason R. Thorpe.  All rights reserved.
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
 *	This product includes software developed by Christopher G. Demetriou
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
 *	This product includes software developed by Christopher G. Demetriou
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

#ifndef _MACHINE_BUS_MI_H_
#define _MACHINE_BUS_MI_H_

#include <machine/pio.h>

/*
 * Bus access types.
 */
typedef u_int32_t bus_addr_t;
typedef u_int32_t bus_size_t;
typedef u_int32_t bus_space_handle_t;
typedef const struct ppc_bus_space *bus_space_tag_t;

struct ppc_bus_space {
	u_int32_t pbs_type;
	bus_addr_t pbs_offset;
	bus_addr_t pbs_base;
	bus_addr_t pbs_limit;
	int (*pbs_map)(bus_space_tag_t, bus_addr_t, bus_size_t, int,
	    bus_space_handle_t *);
	void (*pbs_unmap)(bus_space_tag_t, bus_space_handle_t,
	    bus_size_t);
	int (*pbs_alloc)(bus_space_tag_t, bus_addr_t, bus_addr_t,
	    bus_size_t, bus_size_t align, bus_size_t, int, bus_addr_t *,
	    bus_space_handle_t *);
	void (*pbs_free)(bus_space_tag_t, bus_space_handle_t, bus_size_t);
};

#define BUS_SPACE_MAP_CACHEABLE		0x01
#define BUS_SPACE_MAP_LINEAR		0x02
#define BUS_SPACE_MAP_PREFETCHABLE	0x04

#ifdef __STDC__
#define CAT(a,b)	a##b
#define CAT3(a,b,c)	a##b##c
#else
#define CAT(a,b)	a/**/b
#define CAT3(a,b,c)	a/**/b/**/c
#endif

/*
 * Access methods for bus resources
 */

#define __BUS_SPACE_HAS_STREAM_METHODS

/*
 *	int bus_space_map(bus_space_tag_t t, bus_addr_t addr,
 *	    bus_size_t size, int flags, bus_space_handle_t *bshp);
 *
 * Map a region of bus space.
 */

#define bus_space_map(t, a, s, f, hp)	\
    ((*(t)->pbs_map)((t), (a), (s), (f), (hp)))

/*
 *	int bus_space_unmap(bus_space_tag_t t,
 *	    bus_space_handle_t bsh, bus_size_t size);
 *
 * Unmap a region of bus space.
 */

#define bus_space_unmap(t, h, s)					\
    ((void)(*(t)->pbs_unmap)((t), (h), (s)))

/*
 *	int bus_space_subregion(bus_space_tag_t t,
 *	    bus_space_handle_t bsh, bus_size_t offset, bus_size_t size,
 *	    bus_space_handle_t *nbshp);
 *
 * Get a new handle for a subregion of an already-mapped area of bus space.
 */

#define bus_space_subregion(t, h, o, s, hp)				\
    ((*(hp) = (h) + (o)), 0)

/*
 *	int bus_space_alloc(bus_space_tag_t t, bus_addr_t rstart,
 *	    bus_addr_t rend, bus_size_t size, bus_size_t align,
 *	    bus_size_t boundary, int flags, bus_addr_t *bpap,
 *	    bus_space_handle_t *bshp);
 *
 * Allocate a region of bus space.
 */

#define bus_space_alloc(t, rs, re, s, a, b, f, ap, hp)			\
    ((*(t)->pbs_alloc)((t), (rs), (re), (s), (a), (b), (f), (ap), (hp)))

/*
 *	int bus_space_free(bus_space_tag_t t,
 *	    bus_space_handle_t bsh, bus_size_t size);
 *
 * Free a region of bus space.
 */

#define bus_space_free(t, h, s)						\
    ((void)(*(t)->pbs_free)((t), (h), (s)))

/*
 *	u_intN_t bus_space_read_N(bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset);
 *
 * Read a 1, 2, 4, or 8 byte quantity from bus space
 * described by tag/handle/offset.
 */

#define bus_space_read(n,m)						      \
static __inline CAT3(u_int,m,_t)					      \
CAT(bus_space_read_,n)(bus_space_tag_t tag, bus_space_handle_t bsh,	      \
    bus_size_t offset)							      \
{									      \
	return CAT3(in,m,rb)((volatile CAT3(u_int,m,_t) *)(bsh + (offset)));  \
}

bus_space_read(1,8)
bus_space_read(2,16)
bus_space_read(4,32)
#define bus_space_read_8	!!! bus_space_read_8 unimplemented !!!

/*
 *	u_intN_t bus_space_read_stream_N(bus_space_tag_t tag,
 *	     bus_space_handle_t bsh, bus_size_t offset);
 *
 * Read a 2, 4, or 8 byte stream quantity from bus space
 * described by tag/handle/offset.
 */

#define bus_space_read_stream(n,m)					      \
static __inline CAT3(u_int,m,_t)					      \
CAT(bus_space_read_stream_,n)(bus_space_tag_t tag, bus_space_handle_t bsh,    \
    bus_size_t offset)							      \
{									      \
	return CAT(in,m)((volatile CAT3(u_int,m,_t) *)(bsh + (offset)));      \
}

bus_space_read_stream(2,16)
bus_space_read_stream(4,32)
#define bus_space_read_stream_8	!!! bus_space_read_stream_8 unimplemented !!!

/*
 *	void bus_space_read_multi_N(bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset,
 *	    u_intN_t *addr, size_t count);
 *
 * Read `count' 1, 2, 4, or 8 byte quantities from bus space
 * described by tag/handle/offset and copy into buffer provided.
 */

#define bus_space_read_multi(n,m)					     \
static __inline void							     \
CAT(bus_space_read_multi_,n)(bus_space_tag_t tag, bus_space_handle_t bsh,    \
     bus_size_t offset, CAT3(u_int,m,_t) *addr, size_t count)		     \
{									     \
	while (count--)							     \
		*addr++ = CAT(bus_space_read_,n)(tag, bsh, offset);	     \
}

bus_space_read_multi(1,8)
bus_space_read_multi(2,16)
bus_space_read_multi(4,32)
#define bus_space_read_multi_8	!!! bus_space_read_multi_8 not implemented !!!

#if 0
/*
 *	void bus_space_read_multi_stream_N(bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset,
 *	    u_intN_t *addr, size_t count);
 *
 * Read `count' 2, 4, or 8 byte stream quantities from bus space
 * described by tag/handle/offset and copy into buffer provided.
 */

#define bus_space_read_multi_stream(n,m)				      \
static __inline void							      \
CAT(bus_space_read_multi_stream_,n)(bus_space_tag_t tag,		      \
     bus_space_handle_t bsh,						      \
     bus_size_t offset, CAT3(u_int,m,_t) *addr, size_t count)		      \
{									      \
	CAT(ins,m)((volatile CAT3(u_int,m,_t) *)(bsh + (offset)),	      \
	    (CAT3(u_int,m,_t) *)addr, (size_t)count);			      \
}

bus_space_read_multi_stream(2,16)
bus_space_read_multi_stream(4,32)
#define bus_space_read_multi_stream_8					      \
	!!! bus_space_read_multi_stream_8 not implemented !!!

/*
 *      void bus_space_write_N(bus_space_tag_t tag,
 *	  bus_space_handle_t bsh, bus_size_t offset,
 *	  u_intN_t value);
 *
 * Write the 1, 2, 4, or 8 byte value `value' to bus space
 * described by tag/handle/offset.
 */
#endif

#define bus_space_write(n,m)						     \
static __inline void							     \
CAT(bus_space_write_,n)(bus_space_tag_t tag, bus_space_handle_t bsh,	     \
    bus_size_t offset, CAT3(u_int,m,_t) x)				     \
{									     \
	CAT3(out,m,rb)((volatile CAT3(u_int,m,_t) *)(bsh + (offset)), x);    \
}

bus_space_write(1,8)
bus_space_write(2,16)
bus_space_write(4,32)
#define bus_space_write_8	!!! bus_space_write_8 unimplemented !!!

/*
 *	void bus_space_write_stream_N(bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset,
 *	    u_intN_t value);
 *
 * Write the 2, 4, or 8 byte stream value `value' to bus space
 * described by tag/handle/offset.
 */

#define bus_space_write_stream(n,m)					      \
static __inline void							      \
CAT(bus_space_write_stream_,n)(bus_space_tag_t tag, bus_space_handle_t bsh,   \
    bus_size_t offset, CAT3(u_int,m,_t) x)				      \
{									      \
	CAT(out,m)((volatile CAT3(u_int,m,_t) *)(bsh + (offset)), x);	      \
}

bus_space_write_stream(2,16)
bus_space_write_stream(4,32)
#define bus_space_write_stream_8 !!! bus_space_write_stream_8 unimplemented !!!

/*
 *	void bus_space_write_multi_N(bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset,
 *	    const u_intN_t *addr, size_t count);
 *
 * Write `count' 1, 2, 4, or 8 byte quantities from the buffer
 * provided to bus space described by tag/handle/offset.
 */

#define bus_space_write_multi(n,m)					      \
static __inline void							      \
CAT(bus_space_write_multi_,n)(bus_space_tag_t tag, bus_space_handle_t bsh,    \
    bus_size_t offset, const CAT3(u_int,m,_t) *addr, size_t count)	      \
{									      \
	while (count--)							      \
		CAT(bus_space_write_,n)(tag, bsh, offset, *addr++);	      \
}

bus_space_write_multi(1,8)
bus_space_write_multi(2,16)
bus_space_write_multi(4,32)
#define bus_space_write_multi_8	!!! bus_space_write_multi_8 not implemented !!!

#if 0
/*
 *      void bus_space_write_multi_stream_N(bus_space_tag_t tag,
 *	  bus_space_handle_t bsh, bus_size_t offset,
 *	  const u_intN_t *addr, size_t count);
 *
 * Write `count' 2, 4, or 8 byte stream quantities from the buffer
 * provided to bus space described by tag/handle/offset.
 */

#define bus_space_write_multi_stream(n,m)				     \
static __inline void							     \
CAT(bus_space_write_multi_stream_,n)(bus_space_tag_t tag,		     \
     bus_space_handle_t bsh,						     \
     bus_size_t offset, const CAT3(u_int,m,_t) *addr, size_t count)	     \
{									     \
	CAT(outs,m)((volatile CAT3(u_int,m,_t) *)(bsh + (offset)),	     \
	    (CAT3(u_int,m,_t) *)addr, (size_t)count);			     \
}

bus_space_write_multi_stream(2,16)
bus_space_write_multi_stream(4,32)
#define bus_space_write_multi_stream_8					     \
	!!! bus_space_write_multi_stream_8 not implemented !!!
#endif

/*
 *      void bus_space_read_region_N(bus_space_tag_t tag,
 *	  bus_space_handle_t bsh, bus_size_t offset,
 *	  u_intN_t *addr, size_t count);
 *
 * Read `count' 1, 2, 4, or 8 byte quantities from bus space
 * described by tag/handle and starting at `offset' and copy into
 * buffer provided.
 */
static __inline void bus_space_read_region_1(bus_space_tag_t,
	bus_space_handle_t, bus_size_t, u_int8_t *, size_t);
static __inline void bus_space_read_region_2(bus_space_tag_t,
	bus_space_handle_t, bus_size_t, u_int16_t *, size_t);
static __inline void bus_space_read_region_4(bus_space_tag_t,
	bus_space_handle_t, bus_size_t, u_int32_t *, size_t);

static __inline void
bus_space_read_region_1(tag, bsh, offset, addr, count)
	bus_space_tag_t tag;
	bus_space_handle_t bsh;
	bus_size_t offset;
	u_int8_t *addr;
	size_t count;
{
	volatile u_int8_t *s;

	s = (volatile u_int8_t *)(bsh + offset);
	while (count--)
		*addr++ = *s++;
	__asm__ volatile("eieio; sync");
}

static __inline void
bus_space_read_region_2(tag, bsh, offset, addr, count)
	bus_space_tag_t tag;
	bus_space_handle_t bsh;
	bus_size_t offset;
	u_int16_t *addr;
	size_t count;
{
	volatile u_int16_t *s;

	s = (volatile u_int16_t *)(bsh + offset);
	while (count--)
		__asm__ volatile("lhbrx %0, 0, %1" :
			"=r"(*addr++) : "r"(s++));
	__asm__ volatile("eieio; sync");
}

static __inline void
bus_space_read_region_4(tag, bsh, offset, addr, count)
	bus_space_tag_t tag;
	bus_space_handle_t bsh;
	bus_size_t offset;
	u_int32_t *addr;
	size_t count;
{
	volatile u_int32_t *s;

	s = (volatile u_int32_t *)(bsh + offset);
	while (count--)
		__asm__ volatile("lwbrx %0, 0, %1" :
			"=r"(*addr++) : "r"(s++));
	__asm__ volatile("eieio; sync");
}

#define bus_space_read_region_8	!!! bus_space_read_region_8 unimplemented !!!

/*
 *	void bus_space_read_region_stream_N(bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset,
 *	    u_intN_t *addr, size_t count);
 *
 * Read `count' 2, 4, or 8 byte stream quantities from bus space
 * described by tag/handle and starting at `offset' and copy into
 * buffer provided.
 */
static __inline void bus_space_read_region_stream_2(bus_space_tag_t,
	bus_space_handle_t, bus_size_t, u_int16_t *, size_t);
static __inline void bus_space_read_region_stream_4(bus_space_tag_t,
	bus_space_handle_t, bus_size_t, u_int32_t *, size_t);

static __inline void
bus_space_read_region_stream_2(tag, bsh, offset, addr, count)
	bus_space_tag_t tag;
	bus_space_handle_t bsh;
	bus_size_t offset;
	u_int16_t *addr;
	size_t count;
{
	volatile u_int16_t *s;

	s = (volatile u_int16_t *)(bsh + offset);
	while (count--)
		*addr++ = *s++;
	__asm__ volatile("eieio; sync");
}

static __inline void
bus_space_read_region_stream_4(tag, bsh, offset, addr, count)
	bus_space_tag_t tag;
	bus_space_handle_t bsh;
	bus_size_t offset;
	u_int32_t *addr;
	size_t count;
{
	volatile u_int32_t *s;

	s = (volatile u_int32_t *)(bsh + offset);
	while (count--)
		*addr++ = *s++;
	__asm__ volatile("eieio; sync");
}

#define bus_space_read_region_stream_8					      \
	!!! bus_space_read_region_stream_8 unimplemented !!!

/*
 *	void bus_space_write_region_N(bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset,
 *	    const u_intN_t *addr, size_t count);
 *
 * Write `count' 1, 2, 4, or 8 byte quantities from the buffer provided
 * to bus space described by tag/handle starting at `offset'.
 */
static __inline void bus_space_write_region_1(bus_space_tag_t,
	bus_space_handle_t, bus_size_t, const u_int8_t *, size_t);
static __inline void bus_space_write_region_2(bus_space_tag_t,
	bus_space_handle_t, bus_size_t, const u_int16_t *, size_t);
static __inline void bus_space_write_region_4(bus_space_tag_t,
	bus_space_handle_t, bus_size_t, const u_int32_t *, size_t);

static __inline void
bus_space_write_region_1(tag, bsh, offset, addr, count)
	bus_space_tag_t tag;
	bus_space_handle_t bsh;
	bus_size_t offset;
	const u_int8_t *addr;
	size_t count;
{
	volatile u_int8_t *d;

	d = (volatile u_int8_t *)(bsh + offset);
	while (count--)
		*d++ = *addr++;
	__asm__ volatile("eieio; sync");
}

static __inline void
bus_space_write_region_2(tag, bsh, offset, addr, count)
	bus_space_tag_t tag;
	bus_space_handle_t bsh;
	bus_size_t offset;
	const u_int16_t *addr;
	size_t count;
{
	volatile u_int16_t *d;

	d = (volatile u_int16_t *)(bsh + offset);
	while (count--)
		__asm__ volatile("sthbrx %0, 0, %1" ::
			"r"(*addr++), "r"(d++));
	__asm__ volatile("eieio; sync");
}

static __inline void
bus_space_write_region_4(tag, bsh, offset, addr, count)
	bus_space_tag_t tag;
	bus_space_handle_t bsh;
	bus_size_t offset;
	const u_int32_t *addr;
	size_t count;
{
	volatile u_int32_t *d;

	d = (volatile u_int32_t *)(bsh + offset);
	while (count--)
		__asm__ volatile("stwbrx %0, 0, %1" ::
			"r"(*addr++), "r"(d++));
	__asm__ volatile("eieio; sync");
}

#define bus_space_write_region_8 !!! bus_space_write_region_8 unimplemented !!!

/*
 *	void bus_space_write_region_stream_N(bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset,
 *	    const u_intN_t *addr, size_t count);
 *
 * Write `count' 2, 4, or 8 byte stream quantities from the buffer provided
 * to bus space described by tag/handle starting at `offset'.
 */
static __inline void bus_space_write_region_stream_2(bus_space_tag_t,
	bus_space_handle_t, bus_size_t, const u_int16_t *, size_t);
static __inline void bus_space_write_region_stream_4(bus_space_tag_t,
	bus_space_handle_t, bus_size_t, const u_int32_t *, size_t);

static __inline void
bus_space_write_region_stream_2(tag, bsh, offset, addr, count)
	bus_space_tag_t tag;
	bus_space_handle_t bsh;
	bus_size_t offset;
	const u_int16_t *addr;
	size_t count;
{
	volatile u_int16_t *d;

	d = (volatile u_int16_t *)(bsh + offset);
	while (count--)
		*d++ = *addr++;
	__asm__ volatile("eieio; sync");
}

static __inline void
bus_space_write_region_stream_4(tag, bsh, offset, addr, count)
	bus_space_tag_t tag;
	bus_space_handle_t bsh;
	bus_size_t offset;
	const u_int32_t *addr;
	size_t count;
{
	volatile u_int32_t *d;

	d = (volatile u_int32_t *)(bsh + offset);
	while (count--)
		*d++ = *addr++;
	__asm__ volatile("eieio; sync");
}

#define bus_space_write_region_stream_8					      \
	 !!! bus_space_write_region_stream_8 unimplemented !!!

/*
 *	void bus_space_set_multi_N(bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset, u_intN_t val,
 *	    size_t count);
 *
 * Write the 1, 2, 4, or 8 byte value `val' to bus space described
 * by tag/handle/offset `count' times.
 */
static __inline void bus_space_set_multi_1(bus_space_tag_t,
	bus_space_handle_t, bus_size_t, u_int8_t, size_t);
static __inline void bus_space_set_multi_2(bus_space_tag_t,
	bus_space_handle_t, bus_size_t, u_int16_t, size_t);
static __inline void bus_space_set_multi_4(bus_space_tag_t,
	bus_space_handle_t, bus_size_t, u_int32_t, size_t);

static __inline void
bus_space_set_multi_1(tag, bsh, offset, val, count)
	bus_space_tag_t tag;
	bus_space_handle_t bsh;
	bus_size_t offset;
	u_int8_t val;
	size_t count;
{
	volatile u_int8_t *d;

	d = (volatile u_int8_t *)(bsh + offset);
	while (count--)
		*d = val;
	__asm__ volatile("eieio; sync");
}

static __inline void
bus_space_set_multi_2(tag, bsh, offset, val, count)
	bus_space_tag_t tag;
	bus_space_handle_t bsh;
	bus_size_t offset;
	u_int16_t val;
	size_t count;
{
	volatile u_int16_t *d;

	d = (volatile u_int16_t *)(bsh + offset);
	while (count--)
		__asm__ volatile("sthbrx %0, 0, %1" ::
			"r"(val), "r"(d));
	__asm__ volatile("eieio; sync");
}

static __inline void
bus_space_set_multi_4(tag, bsh, offset, val, count)
	bus_space_tag_t tag;
	bus_space_handle_t bsh;
	bus_size_t offset;
	u_int32_t val;
	size_t count;
{
	volatile u_int32_t *d;

	d = (volatile u_int32_t *)(bsh + offset);
	while (count--)
		__asm__ volatile("stwbrx %0, 0, %1" ::
			"r"(val), "r"(d));
	__asm__ volatile("eieio; sync");
}

#define bus_space_set_multi_8 !!! bus_space_set_multi_8 unimplemented !!!

/*
 *	void bus_space_set_multi_stream_N(bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset, u_intN_t val,
 *	    size_t count);
 *
 * Write the 2, 4, or 8 byte stream value `val' to bus space described
 * by tag/handle/offset `count' times.
 */
static __inline void bus_space_set_multi_stream_2(bus_space_tag_t,
	bus_space_handle_t, bus_size_t, u_int16_t, size_t);
static __inline void bus_space_set_multi_stream_4(bus_space_tag_t,
	bus_space_handle_t, bus_size_t, u_int32_t, size_t);

static __inline void
bus_space_set_multi_stream_2(tag, bsh, offset, val, count)
	bus_space_tag_t tag;
	bus_space_handle_t bsh;
	bus_size_t offset;
	u_int16_t val;
	size_t count;
{
	volatile u_int16_t *d;

	d = (volatile u_int16_t *)(bsh + offset);
	while (count--)
		*d = val;
	__asm__ volatile("eieio; sync");
}

static __inline void
bus_space_set_multi_stream_4(tag, bsh, offset, val, count)
	bus_space_tag_t tag;
	bus_space_handle_t bsh;
	bus_size_t offset;
	u_int32_t val;
	size_t count;
{
	volatile u_int32_t *d;

	d = (volatile u_int32_t *)(bsh + offset);
	while (count--)
		*d = val;
	__asm__ volatile("eieio; sync");
}

#define bus_space_set_multi_stream_8					      \
	!!! bus_space_set_multi_stream_8 unimplemented !!!

/*
 *	void bus_space_set_region_N(bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset, u_intN_t val,
 *	    size_t count);
 *
 * Write `count' 1, 2, 4, or 8 byte value `val' to bus space described
 * by tag/handle starting at `offset'.
 */
static __inline void bus_space_set_region_1(bus_space_tag_t,
	bus_space_handle_t, bus_size_t, u_int8_t, size_t);
static __inline void bus_space_set_region_2(bus_space_tag_t,
	bus_space_handle_t, bus_size_t, u_int16_t, size_t);
static __inline void bus_space_set_region_4(bus_space_tag_t,
	bus_space_handle_t, bus_size_t, u_int32_t, size_t);

static __inline void
bus_space_set_region_1(tag, bsh, offset, val, count)
	bus_space_tag_t tag;
	bus_space_handle_t bsh;
	bus_size_t offset;
	u_int8_t val;
	size_t count;
{
	volatile u_int8_t *d;

	d = (volatile u_int8_t *)(bsh + offset);
	while (count--)
		*d++ = val;
	__asm__ volatile("eieio; sync");
}

static __inline void
bus_space_set_region_2(tag, bsh, offset, val, count)
	bus_space_tag_t tag;
	bus_space_handle_t bsh;
	bus_size_t offset;
	u_int16_t val;
	size_t count;
{
	volatile u_int16_t *d;

	d = (volatile u_int16_t *)(bsh + offset);
	while (count--)
		__asm__ volatile("sthbrx %0, 0, %1" ::
			"r"(val), "r"(d++));
	__asm__ volatile("eieio; sync");
}

static __inline void
bus_space_set_region_4(tag, bsh, offset, val, count)
	bus_space_tag_t tag;
	bus_space_handle_t bsh;
	bus_size_t offset;
	u_int32_t val;
	size_t count;
{
	volatile u_int32_t *d;

	d = (volatile u_int32_t *)(bsh + offset);
	while (count--)
		__asm__ volatile("stwbrx %0, 0, %1" ::
			"r"(val), "r"(d++));
	__asm__ volatile("eieio; sync");
}

#define bus_space_set_region_8 !!! bus_space_set_region_8 unimplemented !!!

/*
 *	void bus_space_set_region_stream_N(bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset, u_intN_t val,
 *	    size_t count);
 *
 * Write `count' 2, 4, or 8 byte stream value `val' to bus space described
 * by tag/handle starting at `offset'.
 */
static __inline void bus_space_set_region_stream_2(bus_space_tag_t,
	bus_space_handle_t, bus_size_t, u_int16_t, size_t);
static __inline void bus_space_set_region_stream_4(bus_space_tag_t,
	bus_space_handle_t, bus_size_t, u_int32_t, size_t);


static __inline void
bus_space_set_region_stream_2(tag, bsh, offset, val, count)
	bus_space_tag_t tag;
	bus_space_handle_t bsh;
	bus_size_t offset;
	u_int16_t val;
	size_t count;
{
	volatile u_int16_t *d;

	d = (volatile u_int16_t *)(bsh + offset);
	while (count--)
		*d++ = val;
	__asm__ volatile("eieio; sync");
}

static __inline void
bus_space_set_region_stream_4(tag, bsh, offset, val, count)
	bus_space_tag_t tag;
	bus_space_handle_t bsh;
	bus_size_t offset;
	u_int32_t val;
	size_t count;
{
	volatile u_int32_t *d;

	d = (volatile u_int32_t *)(bsh + offset);
	while (count--)
		*d++ = val;
	__asm__ volatile("eieio; sync");
}

#define bus_space_set_region_stream_8					      \
	!!! bus_space_set_region_stream_8 unimplemented !!!

/*
 *	void bus_space_copy_N(bus_space_tag_t tag,
 *	    bus_space_handle_t bsh1, bus_size_t off1,
 *	    bus_space_handle_t bsh2, bus_size_t off2,
 *	    size_t count);
 *
 * Copy `count' 1, 2, 4, or 8 byte values from bus space starting
 * at tag/bsh1/off1 to bus space starting at tag/bsh2/off2.
 */

static __inline void bus_space_copy_1(bus_space_tag_t,
	bus_space_handle_t, bus_size_t, bus_space_handle_t,
	bus_size_t, size_t);
static __inline void bus_space_copy_2(bus_space_tag_t,
	bus_space_handle_t, bus_size_t, bus_space_handle_t,
	bus_size_t, size_t);
static __inline void bus_space_copy_4(bus_space_tag_t,
	bus_space_handle_t, bus_size_t, bus_space_handle_t,
	bus_size_t, size_t);

static __inline void
bus_space_copy_1(t, h1, o1, h2, o2, c)
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
bus_space_copy_2(t, h1, o1, h2, o2, c)
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
bus_space_copy_4(t, h1, o1, h2, o2, c)
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

#define bus_space_copy_8	!!! bus_space_copy_8 unimplemented !!!

/*
 * Bus read/write barrier methods.
 *
 *	void bus_space_barrier(bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset,
 *	    bus_size_t len, int flags);
 *
 */
#define bus_space_barrier(t, h, o, l, f)	\
     ((void)((void)(t), (void)(h), (void)(o), (void)(l), (void)(f)))
#define BUS_SPACE_BARRIER_READ	0x01	/* force read barrier */
#define BUS_SPACE_BARRIER_WRITE	0x02	/* force write barrier */

/*
 * Bus DMA methods.
 */

/*
 * Flags used in various bus DMA methods.
 */
#define	BUS_DMA_WAITOK		0x000	/* safe to sleep (pseudo-flag) */
#define	BUS_DMA_NOWAIT		0x001	/* not safe to sleep */
#define	BUS_DMA_ALLOCNOW	0x002	/* perform resource allocation now */
#define	BUS_DMA_COHERENT	0x008	/* hint: map memory DMA coherent */
#define	BUS_DMA_BUS1		0x010	/* placeholders for bus functions... */
#define	BUS_DMA_BUS2		0x020
#define	BUS_DMA_BUS3		0x040
#define	BUS_DMA_BUS4		0x080
#define	BUS_DMA_READ		0x100	/* mapping is device -> memory only */
#define	BUS_DMA_WRITE		0x200	/* mapping is memory -> device only */
#define	BUS_DMA_STREAMING	0x400	/* hint: sequential, unidirectional */
#define	BUS_DMA_ZERO		0x800	/* zero memory in dmamem_alloc */

/* Forwards needed by prototypes below. */
struct mbuf;
struct uio;

#define BUS_DMASYNC_PREREAD	0x01
#define BUS_DMASYNC_POSTREAD	0x02
#define BUS_DMASYNC_PREWRITE	0x04
#define BUS_DMASYNC_POSTWRITE	0x08

typedef struct powerpc_bus_dma_tag		*bus_dma_tag_t;
typedef struct powerpc_bus_dmamap		*bus_dmamap_t;

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
typedef struct powerpc_bus_dma_segment  bus_dma_segment_t;

/*
 *	bus_dma_tag_t
 *
 *	A machine-dependent opaque type describing the implementation of
 *	DMA for a given bus.
 */

struct powerpc_bus_dma_tag {
	/*
	 * The `bounce threshold' is checked while we are loading
	 * the DMA map.  If the physical address of the segment
	 * exceeds the threshold, an error will be returned.  The
	 * caller can then take whatever action is necessary to
	 * bounce the transfer.  If this value is 0, it will be
	 * ignored.
	 */
	bus_addr_t _bounce_thresh;

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
	void	    (*_dmamap_sync)(bus_dma_tag_t, bus_dmamap_t,
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

#define bus_dmamap_create(t, s, n, m, b, f, p)			\
	(*(t)->_dmamap_create)((t), (s), (n), (m), (b), (f), (p))
#define bus_dmamap_destroy(t, p)				\
	(*(t)->_dmamap_destroy)((t), (p))
#define bus_dmamap_load(t, m, b, s, p, f)			\
	(*(t)->_dmamap_load)((t), (m), (b), (s), (p), (f))
#define bus_dmamap_load_mbuf(t, m, b, f)			\
	(*(t)->_dmamap_load_mbuf)((t), (m), (b), (f))
#define bus_dmamap_load_uio(t, m, u, f)				\
	(*(t)->_dmamap_load_uio)((t), (m), (u), (f))
#define bus_dmamap_load_raw(t, m, sg, n, s, f)			\
	(*(t)->_dmamap_load_raw)((t), (m), (sg), (n), (s), (f))
#define bus_dmamap_unload(t, p)					\
	(*(t)->_dmamap_unload)((t), (p))
#define	bus_dmamap_sync(t, p, a, l, o)				\
	(void)((t)->_dmamap_sync ?				\
	    (*(t)->_dmamap_sync)((t), (p), (a), (l), (o)) : (void)0)

#define bus_dmamem_alloc(t, s, a, b, sg, n, r, f)		\
	(*(t)->_dmamem_alloc)((t), (s), (a), (b), (sg), (n), (r), (f))
#define bus_dmamem_free(t, sg, n)				\
	(*(t)->_dmamem_free)((t), (sg), (n))
#define bus_dmamem_map(t, sg, n, s, k, f)			\
	(*(t)->_dmamem_map)((t), (sg), (n), (s), (k), (f))
#define bus_dmamem_unmap(t, k, s)				\
	(*(t)->_dmamem_unmap)((t), (k), (s))
#define bus_dmamem_mmap(t, sg, n, o, p, f)			\
	(*(t)->_dmamem_mmap)((t), (sg), (n), (o), (p), (f))

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
	bus_addr_t	_dm_bounce_thresh; /* bounce threshold; see tag */
	int		_dm_flags;	/* misc. flags */

	void		*_dm_cookie;	/* cookie for bus-specific functions */

	/*
	 * PUBLIC MEMBERS: these are used by machine-independent code.
	 */
	bus_size_t	dm_mapsize;	/* size of the mapping */
	int		dm_nsegs;	/* # valid segments in mapping */
	bus_dma_segment_t dm_segs[1];	/* segments; variable length */
};

#ifdef _POWERPC_BUS_DMA_PRIVATE
int	_bus_dmamap_create(bus_dma_tag_t, bus_size_t, int, bus_size_t,
	    bus_size_t, int, bus_dmamap_t *);
void	_bus_dmamap_destroy(bus_dma_tag_t, bus_dmamap_t);
int	_bus_dmamap_load(bus_dma_tag_t, bus_dmamap_t, void *,
	    bus_size_t, struct proc *, int);
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
int	_bus_dmamem_map(bus_dma_tag_t tag, bus_dma_segment_t *segs,
	    int nsegs, size_t size, caddr_t *kvap, int flags);
void	_bus_dmamem_unmap(bus_dma_tag_t tag, caddr_t kva,
	    size_t size);
paddr_t _bus_dmamem_mmap(bus_dma_tag_t tag, bus_dma_segment_t *segs,
	    int nsegs, off_t off, int prot, int flags);

int	_bus_dmamem_alloc_range(bus_dma_tag_t tag, bus_size_t size,
	    bus_size_t alignment, bus_size_t boundary,
	    bus_dma_segment_t *segs, int nsegs, int *rsegs, int flags,
	    paddr_t low, paddr_t high);
#endif /* _POWERPC_BUS_DMA_PRIVATE */
#endif /* _MACHINE_BUS_MI_H_ */
