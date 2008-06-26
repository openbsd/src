/*	$OpenBSD: bus.h,v 1.6 2008/06/26 05:42:10 ray Exp $	*/
/*	$NetBSD: bus.h,v 1.9 1998/01/13 18:32:15 scottr Exp $	*/

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
 * Copyright (C) 1997 Scott Reynolds.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
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

#ifndef _HP300_BUS_H_
#define _HP300_BUS_H_

/*
 * Bus address and size types
 */
typedef u_long bus_addr_t;
typedef u_long bus_size_t;

/*
 * Access methods for bus resources and address space.
 */
typedef u_long bus_space_handle_t;

struct hp300_bus_space_tag {
	int	(*bs_map)(bus_addr_t, bus_size_t, int, bus_space_handle_t *);
	void	(*bs_unmap)(bus_space_handle_t, bus_size_t);
	int	(*bs_subregion)(bus_space_handle_t, bus_size_t, bus_size_t,
		    bus_space_handle_t *);
	void *	(*bs_vaddr)(bus_space_handle_t);

	u_int8_t	(*bsr1)(bus_space_handle_t, bus_size_t);
	u_int16_t	(*bsr2)(bus_space_handle_t, bus_size_t);
	u_int32_t	(*bsr4)(bus_space_handle_t, bus_size_t);
	void		(*bsrm1)(bus_space_handle_t, bus_size_t,
				u_int8_t *, size_t);
	void		(*bsrm2)(bus_space_handle_t, bus_size_t,
				u_int16_t *, size_t);
	void		(*bsrm4)(bus_space_handle_t, bus_size_t,
				u_int32_t *, size_t);
	void		(*bsrrm2)(bus_space_handle_t, bus_size_t,
				u_int8_t *, size_t);
	void		(*bsrrm4)(bus_space_handle_t, bus_size_t,
				u_int8_t *, size_t);
	void		(*bsrr1)(bus_space_handle_t, bus_size_t,
				u_int8_t *, size_t);
	void		(*bsrr2)(bus_space_handle_t, bus_size_t,
				u_int16_t *, size_t);
	void		(*bsrr4)(bus_space_handle_t, bus_size_t,
				u_int32_t *, size_t);
	void		(*bsrrr2)(bus_space_handle_t, bus_size_t,
				u_int8_t *, size_t);
	void		(*bsrrr4)(bus_space_handle_t, bus_size_t,
				u_int8_t *, size_t);
	void		(*bsw1)(bus_space_handle_t, bus_size_t, u_int8_t);
	void		(*bsw2)(bus_space_handle_t, bus_size_t,
				u_int16_t);
	void		(*bsw4)(bus_space_handle_t, bus_size_t,
				u_int32_t);
	void		(*bswm1)(bus_space_handle_t, bus_size_t,
				const u_int8_t *, size_t);
	void		(*bswm2)(bus_space_handle_t, bus_size_t,
				const u_int16_t *, size_t);
	void		(*bswm4)(bus_space_handle_t, bus_size_t,
				const u_int32_t *, size_t);
	void		(*bswrm2)(bus_space_handle_t, bus_size_t,
				const u_int8_t *, size_t);
	void		(*bswrm4)(bus_space_handle_t, bus_size_t,
				const u_int8_t *, size_t);
	void		(*bswr1)(bus_space_handle_t, bus_size_t,
				const u_int8_t *, size_t);
	void		(*bswr2)(bus_space_handle_t, bus_size_t,
				const u_int16_t *, size_t);
	void		(*bswr4)(bus_space_handle_t, bus_size_t,
				const u_int32_t *, size_t);
	void		(*bswrr2)(bus_space_handle_t, bus_size_t,
				const u_int8_t *, size_t);
	void		(*bswrr4)(bus_space_handle_t, bus_size_t,
				const u_int8_t *, size_t);
	void		(*bssm1)(bus_space_handle_t, bus_size_t,
				u_int8_t v, size_t);
	void		(*bssm2)(bus_space_handle_t, bus_size_t,
				u_int16_t v, size_t);
	void		(*bssm4)(bus_space_handle_t, bus_size_t,
				u_int32_t v, size_t);
	void		(*bssr1)(bus_space_handle_t, bus_size_t,
				u_int8_t v, size_t);
	void		(*bssr2)(bus_space_handle_t, bus_size_t,
				u_int16_t v, size_t);
	void		(*bssr4)(bus_space_handle_t, bus_size_t,
				u_int32_t v, size_t);	
};

typedef const struct hp300_bus_space_tag *bus_space_tag_t;

#define	BUS_SPACE_MAP_CACHEABLE		0x01
#define	BUS_SPACE_MAP_LINEAR		0x02
#define	BUS_SPACE_MAP_PREFETCHABLE	0x04

#define bus_space_map(t,a,s,f,r)	((t)->bs_map(a,s,f,r))
#define bus_space_unmap(t,h,s)		((t)->bs_unmap(h,s))
#define bus_space_subregion(t,h,o,s,r)	((t)->bs_subregion(h,o,s,r))
#define bus_space_vaddr(t,h)		((t)->bs_vaddr(h))

/*
 *	u_intN_t bus_space_read_N(bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset);
 *
 * Read a 1, 2, 4, or 8 byte quantity from bus space
 * described by tag/handle/offset.
 */

#define	bus_space_read_1(t,h,o) (t)->bsr1(h, o)
#define	bus_space_read_2(t,h,o) (t)->bsr2(h, o)
#define	bus_space_read_4(t,h,o) (t)->bsr4(h, o)

/*
 *	void bus_space_read_multi_N(bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset,
 *	    u_intN_t *addr, size_t count);
 *
 * Read `count' 1, 2, 4, or 8 byte quantities from bus space
 * described by tag/handle/offset and copy into buffer provided.
 */

#define bus_space_read_multi_1(t, h, o, a, c) (t)->bsrm1(h, o, a, c)
#define bus_space_read_multi_2(t, h, o, a, c) (t)->bsrm2(h, o, a, c)
#define bus_space_read_multi_4(t, h, o, a, c) (t)->bsrm4(h, o, a, c)
#define bus_space_read_raw_multi_2(t, h, o, a, c) (t)->bsrrm2(h, o, a, c)
#define bus_space_read_raw_multi_4(t, h, o, a, c) (t)->bsrrm4(h, o, a, c)

/*
 *	void bus_space_read_region_N(bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset,
 *	    u_intN_t *addr, size_t count);
 *
 * Read `count' 1, 2, 4, or 8 byte quantities from bus space
 * described by tag/handle and starting at `offset' and copy into
 * buffer provided.
 */

#define bus_space_read_region_1(t, h, o, a, c) (t)->bsrr1(h,o,a,c)
#define bus_space_read_region_2(t, h, o, a, c) (t)->bsrr2(h,o,a,c)
#define bus_space_read_region_4(t, h, o, a, c) (t)->bsrr4(h,o,a,c)
#define bus_space_read_raw_region_2(t, h, o, a, c) (t)->bsrrr2(h,o,a,c)
#define bus_space_read_raw_region_4(t, h, o, a, c) (t)->bsrrr4(h,o,a,c)

/*
 *	void bus_space_write_N(bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset,
 *	    u_intN_t value);
 *
 * Write the 1, 2, 4, or 8 byte value `value' to bus space
 * described by tag/handle/offset.
 */

#define bus_space_write_1(t, h, o, v) (t)->bsw1(h, o, v)
#define bus_space_write_2(t, h, o, v) (t)->bsw2(h, o, v)
#define bus_space_write_4(t, h, o, v) (t)->bsw4(h, o, v)

/*
 *	void bus_space_write_multi_N(bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset,
 *	    const u_intN_t *addr, size_t count);
 *
 * Write `count' 1, 2, 4, or 8 byte quantities from the buffer
 * provided to bus space described by tag/handle/offset.
 */

#define bus_space_write_multi_1(t, h, o, a, c) (t)->bswm1(h, o, a, c)
#define bus_space_write_multi_2(t, h, o, a, c) (t)->bswm2(h, o, a, c)
#define bus_space_write_multi_4(t, h, o, a, c) (t)->bswm4(h, o, a, c)
#define bus_space_write_raw_multi_2(t, h, o, a, c) (t)->bswrm2(h, o, a, c)
#define bus_space_write_raw_multi_4(t, h, o, a, c) (t)->bswrm4(h, o, a, c)

/*
 *	void bus_space_write_region_N(bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset,
 *	    const u_intN_t *addr, size_t count);
 *
 * Write `count' 1, 2, 4, or 8 byte quantities from the buffer provided
 * to bus space described by tag/handle starting at `offset'.
 */

#define bus_space_write_region_1(t, h, o, a, c) (t)->bswr1(h, o, a, c)
#define bus_space_write_region_2(t, h, o, a, c) (t)->bswr2(h, o, a, c)
#define bus_space_write_region_4(t, h, o, a, c) (t)->bswr4(h, o, a, c)
#define bus_space_write_raw_region_2(t, h, o, a, c) (t)->bswrr2(h, o, a, c)
#define bus_space_write_raw_region_4(t, h, o, a, c) (t)->bswrr4(h, o, a, c)

/*
 *	void bus_space_set_multi_N(bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset, u_intN_t val,
 *	    size_t count);
 *
 * Write the 1, 2, 4, or 8 byte value `val' to bus space described
 * by tag/handle/offset `count' times.
 */

#define bus_space_set_multi_1(t, h, o, val, c) (t)->bssm1(h, o, val, c)
#define bus_space_set_multi_2(t, h, o, val, c) (t)->bssm2(h, o, val, c)
#define bus_space_set_multi_4(t, h, o, val, c) (t)->bssm4(h, o, val, c)

/*
 *	void bus_space_set_region_N(bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset, u_intN_t val,
 *	    size_t count);
 *
 * Write `count' 1, 2, 4, or 8 byte value `val' to bus space described
 * by tag/handle starting at `offset'.
 */

#define bus_space_set_region_1(t, h, o, val, c) (t)->bssr1(h, o, val, c)
#define bus_space_set_region_2(t, h, o, val, c) (t)->bssr2(h, o, val, c)
#define bus_space_set_region_4(t, h, o, val, c) (t)->bssr4(h, o, val, c)

/*
 * Bus read/write barrier methods.
 *
 *	void bus_space_barrier(bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset,
 *	    bus_size_t len, int flags);
 *
 * Note: the 680x0 does not currently require barriers, but we must
 * provide the flags to MI code.
 */
#define	bus_space_barrier(t, h, o, l, f)	\
	((void)((void)(t), (void)(h), (void)(o), (void)(l), (void)(f)))
#define	BUS_SPACE_BARRIER_READ	0x01		/* force read barrier */
#define	BUS_SPACE_BARRIER_WRITE	0x02		/* force write barrier */

#endif /* _HP300_BUS_H_ */
