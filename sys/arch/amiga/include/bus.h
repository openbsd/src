/*	$OpenBSD: bus.h,v 1.7 1998/03/26 12:45:00 niklas Exp $	*/

/*
 * Copyright (c) 1996 Niklas Hallqvist.
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
 *	This product includes software developed by the Niklas Hallqvist.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _AMIGA_BUS_H_
#define _AMIGA_BUS_H_

#include <machine/endian.h>

#ifdef __STDC__
#define CAT(a,b)	a##b
#define CAT3(a,b,c)	a##b##c
#else
#define CAT(a,b)	a/**/b
#define CAT3(a,b,c)	a/**/b/**/c
#endif

/* Bus access types.  */
typedef u_int32_t bus_addr_t;
typedef u_int32_t bus_size_t;
typedef u_int32_t bus_space_handle_t;

typedef struct amiga_bus_space *bus_space_tag_t;

struct amiga_bus_space {
	void	*bs_data;

	int	(*bs_map)(bus_space_tag_t, bus_addr_t, bus_size_t, int,
		    bus_space_handle_t *);
	int	(*bs_unmap)(bus_space_tag_t, bus_space_handle_t, bus_size_t);

	/* We need swapping of 16-bit entities */
	int	bs_swapped;

	/* How much to shift an bus_addr_t */
	int	bs_shift;
};

#define bus_space_map(t, port, size, cacheable, bshp) \
    (*(t)->bs_map)((t), (port), (size), (cacheable), (bshp))
#define bus_space_unmap(t, bshp, size) \
    (*(t)->bs_unmap)((t), (bshp), (size))

static __inline u_int8_t
bus_space_read_1(bus_space_tag_t bst, bus_space_handle_t bsh, bus_addr_t ba)
{
	return *(volatile u_int8_t *)(bsh + (ba << bst->bs_shift));
}

static __inline u_int16_t
bus_space_read_2(bus_space_tag_t bst, bus_space_handle_t bsh, bus_addr_t ba)
{
	u_int16_t x =
	    *(volatile u_int16_t *)((bsh & ~1) + (ba << bst->bs_shift));

	return bst->bs_swapped ? swap16(x) : x;
}

static __inline u_int32_t
bus_space_read_4(bus_space_tag_t bst, bus_space_handle_t bsh, bus_addr_t ba)
{
	panic("bus_space_read_4: operation not allowed on this bus (tag %x)",
	    bst);
	return 0;
}

#define	bus_space_read_8	!!! bus_space_read_8 not implemented !!!

#define bus_space_read_multi(n, m)					      \
static __inline void						       	      \
CAT(bus_space_read_multi_,n)(bus_space_tag_t bst, bus_space_handle_t bsh,     \
    bus_addr_t ba, CAT3(u_int,m,_t) *buf, bus_size_t cnt)		      \
{									      \
	while (cnt--)							      \
		*buf++ = CAT(bus_space_read_,n)(bst, bsh, ba);		      \
}

bus_space_read_multi(1,8)
bus_space_read_multi(2,16)
bus_space_read_multi(4,32)

#define	bus_space_read_multi_8	!!! bus_space_read_multi_8 not implemented !!!

#define bus_space_read_region(n, m)					      \
static __inline void						       	      \
CAT(bus_space_read_region_,n)(bus_space_tag_t bst, bus_space_handle_t bsh,    \
    bus_addr_t ba, CAT3(u_int,m,_t) *buf, bus_size_t cnt)		      \
{									      \
	while (cnt--) {							      \
		*buf++ = CAT(bus_space_read_,n)(bst, bsh, ba);		      \
		ba += n;						      \
	}								      \
}

bus_space_read_region(1,8)
bus_space_read_region(2,16)
bus_space_read_region(4,32)

#define	bus_space_read_region_8	!!! bus_space_read_region_8 not implemented !!!

static __inline void
bus_space_write_1(bus_space_tag_t bst, bus_space_handle_t bsh, bus_addr_t ba,
    u_int8_t x)
{
	*(volatile u_int8_t *)(bsh + (ba << bst->bs_shift)) = x;
}

static __inline void
bus_space_write_2(bus_space_tag_t bst, bus_space_handle_t bsh, bus_addr_t ba,
    u_int16_t x)
{
	*(volatile u_int16_t *)((bsh & ~1) + (ba << bst->bs_shift)) =
            bst->bs_swapped ? swap16(x) : x;
}

static __inline void
bus_space_write_4(bus_space_tag_t bst, bus_space_handle_t bsh, bus_addr_t ba,
    u_int32_t x)
{
	panic("bus_space_write_4: operation not allowed on this bus (tag %x)",
	    bst);
}

#define	bus_space_write_8	!!! bus_space_write_8 not implemented !!!

#define bus_space_write_multi(n, m)					      \
static __inline void								      \
CAT(bus_space_write_multi_,n)(bus_space_tag_t bst, bus_space_handle_t bsh,    \
    bus_addr_t ba, const CAT3(u_int,m,_t) *buf, bus_size_t cnt)		      \
{									      \
	while (cnt--)							      \
		CAT(bus_space_write_,n)(bst, bsh, ba, *buf++);		      \
}

bus_space_write_multi(1,8)
bus_space_write_multi(2,16)
bus_space_write_multi(4,32)

#define	bus_space_write_multi_8	!!! bus_space_write_multi_8 not implemented !!!

#define bus_space_write_region(n, m)					      \
static __inline void						       	      \
CAT(bus_space_write_region_,n)(bus_space_tag_t bst, bus_space_handle_t bsh,   \
    bus_addr_t ba, const CAT3(u_int,m,_t) *buf, bus_size_t cnt)		      \
{									      \
	while (cnt--) {							      \
		CAT(bus_space_write_,n)(bst, bsh, ba, *buf++);		      \
		ba += n;						      \
	}								      \
}

bus_space_write_region(1,8)
bus_space_write_region(2,16)
bus_space_write_region(4,32)

#define	bus_space_write_region_8 \
    !!! bus_space_write_region_8 not implemented !!!

/* OpenBSD extensions */
static __inline void
bus_space_read_raw_multi_2(bus_space_tag_t bst, bus_space_handle_t bsh,
    bus_addr_t ba, u_int8_t *buf, bus_size_t cnt)
{
	u_int16_t *buf16 = (u_int16_t *)buf;

	while (cnt) {
		u_int16_t x = *(volatile u_int16_t *)
		    ((bsh & ~1) + (ba << bst->bs_shift));

		*buf16++ = bst->bs_swapped ? x : swap16(x);
		cnt -= 2;
	}
}

static __inline void
bus_space_read_raw_multi_4(bus_space_tag_t bst, bus_space_handle_t bsh,
    bus_addr_t ba, u_int8_t *buf, bus_size_t cnt)
{
	panic("%s: operation not allowed on this bus (tag %x)",
	    "bus_space_read_raw_multi_4", bst);
}

#define	bus_space_read_raw_multi_8 \
    !!! bus_space_read_raw_multi_8 not implemented !!!

static __inline void
bus_space_write_raw_multi_2(bus_space_tag_t bst, bus_space_handle_t bsh,
    bus_addr_t ba, const u_int8_t *buf, bus_size_t cnt)
{
	const u_int16_t *buf16 = (const u_int16_t *)buf;

	while (cnt) {
		*(volatile u_int16_t *)((bsh & ~1) + (ba << bst->bs_shift)) =
		    bst->bs_swapped ? *buf16 : swap16(*buf16);
		buf16++;
		cnt -= 2;
	}
}

static __inline void
bus_space_write_raw_multi_4(bus_space_tag_t bst, bus_space_handle_t bsh,
    bus_addr_t ba, const u_int8_t *buf, bus_size_t cnt)
{
	panic("%s: operation not allowed on this bus (tag %x)",
	    "bus_space_write_raw_multi_4", bst);
}

#define	bus_space_write_raw_multi_8 \
    !!! bus_space_write_raw_multi_8 not implemented !!!

#endif /* _AMIGA_BUS_H_ */
