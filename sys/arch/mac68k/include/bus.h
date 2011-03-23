/*	$OpenBSD: bus.h,v 1.15 2011/03/23 16:54:35 pirofti Exp $	*/
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

#ifndef _MACHINE_BUS_H_
#define _MACHINE_BUS_H_

/*
 * Value for the mac68k bus space tag, not to be used directly by MI code.
 */
#define MAC68K_BUS_SPACE_MEM	0	/* space is mem space */

/*
 * Bus address and size types
 */
typedef u_long bus_addr_t;
typedef u_long bus_size_t;

/*
 * Access methods for bus resources and address space.
 */
#define BSH_T	struct bus_space_handle_s
typedef int	bus_space_tag_t;
typedef struct	bus_space_handle_s {
	u_long	base;
	int	swapped;

	u_int8_t	(*bsr1)(bus_space_tag_t, BSH_T *, bus_size_t);
	u_int16_t	(*bsr2)(bus_space_tag_t, BSH_T *, bus_size_t);
	u_int32_t	(*bsr4)(bus_space_tag_t, BSH_T *, bus_size_t);
	void		(*bsrm1)(bus_space_tag_t, BSH_T *, bus_size_t,
				u_int8_t *, size_t);
	void		(*bsrm2)(bus_space_tag_t, BSH_T *, bus_size_t,
				u_int16_t *, size_t);
	void		(*bsrm4)(bus_space_tag_t, BSH_T *, bus_size_t,
				u_int32_t *, size_t);
	void		(*bsrms2)(bus_space_tag_t, BSH_T *, bus_size_t,
				u_int16_t *, size_t);
	void		(*bsrms4)(bus_space_tag_t, BSH_T *, bus_size_t,
				u_int32_t *, size_t);
	void		(*bsrr1)(bus_space_tag_t, BSH_T *, bus_size_t,
				u_int8_t *, size_t);
	void		(*bsrr2)(bus_space_tag_t, BSH_T *, bus_size_t,
				u_int16_t *, size_t);
	void		(*bsrr4)(bus_space_tag_t, BSH_T *, bus_size_t,
				u_int32_t *, size_t);
	void		(*bsrrs1)(bus_space_tag_t, BSH_T *, bus_size_t,
				u_int8_t *, size_t);
	void		(*bsrrs2)(bus_space_tag_t, BSH_T *, bus_size_t,
				u_int16_t *, size_t);
	void		(*bsrrs4)(bus_space_tag_t, BSH_T *, bus_size_t,
				u_int32_t *, size_t);
	void		(*bsw1)(bus_space_tag_t, BSH_T *, bus_size_t, u_int8_t);
	void		(*bsw2)(bus_space_tag_t, BSH_T *, bus_size_t,
				u_int16_t);
	void		(*bsw4)(bus_space_tag_t, BSH_T *, bus_size_t,
				u_int32_t);
	void		(*bswm1)(bus_space_tag_t, BSH_T *, bus_size_t,
				const u_int8_t *, size_t);
	void		(*bswm2)(bus_space_tag_t, BSH_T *, bus_size_t,
				const u_int16_t *, size_t);
	void		(*bswm4)(bus_space_tag_t, BSH_T *, bus_size_t,
				const u_int32_t *, size_t);
	void		(*bswms1)(bus_space_tag_t, BSH_T *, bus_size_t,
				const u_int8_t *, size_t);
	void		(*bswms2)(bus_space_tag_t, BSH_T *, bus_size_t,
				const u_int16_t *, size_t);
	void		(*bswms4)(bus_space_tag_t, BSH_T *, bus_size_t,
				const u_int32_t *, size_t);
	void		(*bswr1)(bus_space_tag_t, BSH_T *, bus_size_t,
				const u_int8_t *, size_t);
	void		(*bswr2)(bus_space_tag_t, BSH_T *, bus_size_t,
				const u_int16_t *, size_t);
	void		(*bswr4)(bus_space_tag_t, BSH_T *, bus_size_t,
				const u_int32_t *, size_t);
	void		(*bswrs1)(bus_space_tag_t, BSH_T *, bus_size_t,
				const u_int8_t *, size_t);
	void		(*bswrs2)(bus_space_tag_t, BSH_T *, bus_size_t,
				const u_int16_t *, size_t);
	void		(*bswrs4)(bus_space_tag_t, BSH_T *, bus_size_t,
				const u_int32_t *, size_t);
	void		(*bssm1)(bus_space_tag_t, BSH_T *, bus_size_t,
				u_int8_t v, size_t);
	void		(*bssm2)(bus_space_tag_t, BSH_T *, bus_size_t,
				u_int16_t v, size_t);
	void		(*bssm4)(bus_space_tag_t, BSH_T *, bus_size_t,
				u_int32_t v, size_t);
	void		(*bssr1)(bus_space_tag_t, BSH_T *, bus_size_t,
				u_int8_t v, size_t);
	void		(*bssr2)(bus_space_tag_t, BSH_T *, bus_size_t,
				u_int16_t v, size_t);
	void		(*bssr4)(bus_space_tag_t, BSH_T *, bus_size_t,
				u_int32_t v, size_t);	
} bus_space_handle_t;
#undef BSH_T

void	mac68k_bus_space_handle_swapped(bus_space_tag_t,
		bus_space_handle_t *h);

/*
 *	int bus_space_map(bus_space_tag_t t, bus_addr_t addr,
 *	    bus_size_t size, int flags, bus_space_handle_t *bshp);
 *
 * Map a region of bus space.
 */

#define	BUS_SPACE_MAP_CACHEABLE		0x01
#define	BUS_SPACE_MAP_LINEAR		0x02
#define	BUS_SPACE_MAP_PREFETCHABLE	0x04

int	bus_space_map(bus_space_tag_t, bus_addr_t, bus_size_t,
	    int, bus_space_handle_t *);

/*
 *	void bus_space_unmap(bus_space_tag_t t,
 *	    bus_space_handle_t bsh, bus_size_t size);
 *
 * Unmap a region of bus space.
 */

void	bus_space_unmap(bus_space_tag_t, bus_space_handle_t, bus_size_t);

/*
 *	int bus_space_subregion(bus_space_tag_t t,
 *	    bus_space_handle_t bsh, bus_size_t offset, bus_size_t size,
 *	    bus_space_handle_t *nbshp);
 *
 * Get a new handle for a subregion of an already-mapped area of bus space.
 */

int	bus_space_subregion(bus_space_tag_t t, bus_space_handle_t bsh,
	    bus_size_t offset, bus_size_t size, bus_space_handle_t *nbshp);

/*
 *	int bus_space_alloc(bus_space_tag_t t, bus_addr_t, rstart,
 *	    bus_addr_t rend, bus_size_t size, bus_size_t align,
 *	    bus_size_t boundary, int flags, bus_addr_t *addrp,
 *	    bus_space_handle_t *bshp);
 *
 * Allocate a region of bus space.
 */

int	bus_space_alloc(bus_space_tag_t t, bus_addr_t rstart,
	    bus_addr_t rend, bus_size_t size, bus_size_t align,
	    bus_size_t boundary, int flags, bus_addr_t *addrp,
	    bus_space_handle_t *bshp);

/*
 *	int bus_space_free(bus_space_tag_t t,
 *	    bus_space_handle_t bsh, bus_size_t size);
 *
 * Free a region of bus space.
 */

void	bus_space_free(bus_space_tag_t t, bus_space_handle_t bsh,
	    bus_size_t size);

/*
 *	int mac68k_bus_space_probe(bus_space_tag_t t,
 *	    bus_space_handle_t bsh, bus_size_t offset, int sz);
 *
 * Probe the bus at t/bsh/offset, using sz as the size of the load.
 *
 * This is a machine-dependent extension, and is not to be used by
 * machine-independent code.
 */

int	mac68k_bus_space_probe(bus_space_tag_t t,
	    bus_space_handle_t bsh, bus_size_t offset, int sz);

/*
 *	u_intN_t bus_space_read_N(bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset);
 *
 * Read a 1, 2, 4, or 8 byte quantity from bus space
 * described by tag/handle/offset.
 */

u_int8_t mac68k_bsr1(bus_space_tag_t tag, bus_space_handle_t *bsh,
			  bus_size_t offset);
u_int16_t mac68k_bsr2(bus_space_tag_t tag, bus_space_handle_t *bsh,
			  bus_size_t offset);
u_int16_t mac68k_bsr2_swap(bus_space_tag_t tag, bus_space_handle_t *bsh,
				bus_size_t offset);
u_int32_t mac68k_bsr4(bus_space_tag_t tag, bus_space_handle_t *bsh,
				bus_size_t offset);
u_int32_t mac68k_bsr4_swap(bus_space_tag_t tag, bus_space_handle_t *bsh,
				bus_size_t offset);	

#define	bus_space_read_1(t,h,o) (h).bsr1((t), &(h), (o))
#define	bus_space_read_2(t,h,o) (h).bsr2((t), &(h), (o))
#define	bus_space_read_4(t,h,o) (h).bsr4((t), &(h), (o))

/*
 *	void bus_space_read_multi_N(bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset,
 *	    u_intN_t *addr, size_t count);
 *
 * Read `count' 1, 2, 4, or 8 byte quantities from bus space
 * described by tag/handle/offset and copy into buffer provided.
 */

void mac68k_bsrm1(bus_space_tag_t, bus_space_handle_t *, bus_size_t,
	u_int8_t *, size_t);
void mac68k_bsrm2(bus_space_tag_t, bus_space_handle_t *, bus_size_t,
	u_int16_t *, size_t);
void mac68k_bsrm2_swap(bus_space_tag_t, bus_space_handle_t *, bus_size_t,
	u_int16_t *, size_t);
void mac68k_bsrm4(bus_space_tag_t, bus_space_handle_t *, bus_size_t,
	u_int32_t *, size_t);
void mac68k_bsrms4(bus_space_tag_t, bus_space_handle_t *, bus_size_t,
	u_int32_t *, size_t);
void mac68k_bsrm4_swap(bus_space_tag_t, bus_space_handle_t *, bus_size_t,
	u_int32_t *, size_t);

#define bus_space_read_multi_1(t, h, o, a, c) (h).bsrm1(t, &(h), o, a, c)
#define bus_space_read_multi_2(t, h, o, a, c) (h).bsrm2(t, &(h), o, a, c)
#define bus_space_read_multi_4(t, h, o, a, c) (h).bsrm4(t, &(h), o, a, c)

/*
 *	void bus_space_read_region_N(bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset,
 *	    u_intN_t *addr, size_t count);
 *
 * Read `count' 1, 2, 4, or 8 byte quantities from bus space
 * described by tag/handle and starting at `offset' and copy into
 * buffer provided.
 */

void mac68k_bsrr1(bus_space_tag_t, bus_space_handle_t *, bus_size_t,
	u_int8_t *, size_t);
void mac68k_bsrr2(bus_space_tag_t, bus_space_handle_t *, bus_size_t,
	u_int16_t *, size_t);
void mac68k_bsrr2_swap(bus_space_tag_t, bus_space_handle_t *, bus_size_t,
	u_int16_t *, size_t);
void mac68k_bsrr4(bus_space_tag_t, bus_space_handle_t *, bus_size_t,
	u_int32_t *, size_t);
void mac68k_bsrr4_swap(bus_space_tag_t, bus_space_handle_t *, bus_size_t,
	u_int32_t *, size_t);

#define bus_space_read_region_1(t, h, o, a, c) (h).bsrr1(t,&(h),o,a,c)
#define bus_space_read_region_2(t, h, o, a, c) (h).bsrr2(t,&(h),o,a,c)
#define bus_space_read_region_4(t, h, o, a, c) (h).bsrr4(t,&(h),o,a,c)

/*
 *	void bus_space_write_N(bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset,
 *	    u_intN_t value);
 *
 * Write the 1, 2, 4, or 8 byte value `value' to bus space
 * described by tag/handle/offset.
 */

void mac68k_bsw1(bus_space_tag_t, bus_space_handle_t *, bus_size_t, u_int8_t);
void mac68k_bsw2(bus_space_tag_t, bus_space_handle_t *, bus_size_t, u_int16_t);
void mac68k_bsw2_swap(bus_space_tag_t, bus_space_handle_t *, bus_size_t,
	u_int16_t);
void mac68k_bsw4(bus_space_tag_t, bus_space_handle_t *, bus_size_t, u_int32_t);
void mac68k_bsw4_swap(bus_space_tag_t, bus_space_handle_t *, bus_size_t,
	u_int32_t);

#define bus_space_write_1(t, h, o, v) (h).bsw1(t, &(h), o, v)
#define bus_space_write_2(t, h, o, v) (h).bsw2(t, &(h), o, v)
#define bus_space_write_4(t, h, o, v) (h).bsw4(t, &(h), o, v)

/*
 *	void bus_space_write_multi_N(bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset,
 *	    const u_intN_t *addr, size_t count);
 *
 * Write `count' 1, 2, 4, or 8 byte quantities from the buffer
 * provided to bus space described by tag/handle/offset.
 */

void mac68k_bswm1(bus_space_tag_t, bus_space_handle_t *, bus_size_t,
	const u_int8_t *, size_t);
void mac68k_bswm2(bus_space_tag_t, bus_space_handle_t *, bus_size_t,
	const u_int16_t *, size_t);
void mac68k_bswm2_swap(bus_space_tag_t, bus_space_handle_t *, bus_size_t,
	const u_int16_t *, size_t);
void mac68k_bswm4(bus_space_tag_t, bus_space_handle_t *, bus_size_t,
	const u_int32_t *, size_t);
void mac68k_bswm4_swap(bus_space_tag_t, bus_space_handle_t *, bus_size_t,
	const u_int32_t *, size_t);

#define bus_space_write_multi_1(t, h, o, a, c) (h).bswm1(t, &(h), o, a, c)
#define bus_space_write_multi_2(t, h, o, a, c) (h).bswm2(t, &(h), o, a, c)
#define bus_space_write_multi_4(t, h, o, a, c) (h).bswm4(t, &(h), o, a, c)

/*
 *	void bus_space_write_region_N(bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset,
 *	    const u_intN_t *addr, size_t count);
 *
 * Write `count' 1, 2, 4, or 8 byte quantities from the buffer provided
 * to bus space described by tag/handle starting at `offset'.
 */

void mac68k_bswr1(bus_space_tag_t, bus_space_handle_t *, bus_size_t,
	const u_int8_t *, size_t);
void mac68k_bswr2(bus_space_tag_t, bus_space_handle_t *, bus_size_t,
	const u_int16_t *, size_t);
void mac68k_bswr2_swap(bus_space_tag_t, bus_space_handle_t *, bus_size_t,
	const u_int16_t *, size_t);
void mac68k_bswr4(bus_space_tag_t, bus_space_handle_t *, bus_size_t,
	const u_int32_t *, size_t);
void mac68k_bswr4_swap(bus_space_tag_t, bus_space_handle_t *, bus_size_t,
	const u_int32_t *, size_t);

#define bus_space_write_region_1(t, h, o, a, c) (h).bswr1(t, &(h), o, a, c)
#define bus_space_write_region_2(t, h, o, a, c) (h).bswr2(t, &(h), o, a, c)
#define bus_space_write_region_4(t, h, o, a, c) (h).bswr4(t, &(h), o, a, c)

/*
 *	void bus_space_set_multi_N(bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset, u_intN_t val,
 *	    size_t count);
 *
 * Write the 1, 2, 4, or 8 byte value `val' to bus space described
 * by tag/handle/offset `count' times.
 */

void mac68k_bssm1(bus_space_tag_t t, bus_space_handle_t *h,
			bus_size_t o, u_int8_t v, size_t c);
void mac68k_bssm2(bus_space_tag_t t, bus_space_handle_t *h,
			bus_size_t o, u_int16_t v, size_t c);
void mac68k_bssm2_swap(bus_space_tag_t t, bus_space_handle_t *h,
			bus_size_t o, u_int16_t v, size_t c);
void mac68k_bssm4(bus_space_tag_t t, bus_space_handle_t *h,
			bus_size_t o, u_int32_t v, size_t c);
void mac68k_bssm4_swap(bus_space_tag_t t, bus_space_handle_t *h,
			bus_size_t o, u_int32_t v, size_t c);

#define bus_space_set_multi_1(t, h, o, val, c) (h).bssm1(t, &(h), o, val, c)
#define bus_space_set_multi_2(t, h, o, val, c) (h).bssm2(t, &(h), o, val, c)
#define bus_space_set_multi_4(t, h, o, val, c) (h).bssm4(t, &(h), o, val, c)

/*
 *	void bus_space_set_region_N(bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset, u_intN_t val,
 *	    size_t count);
 *
 * Write `count' 1, 2, 4, or 8 byte value `val' to bus space described
 * by tag/handle starting at `offset'.
 */

void mac68k_bssr1(bus_space_tag_t t, bus_space_handle_t *h,
			bus_size_t o, u_int8_t v, size_t c);
void mac68k_bssr2(bus_space_tag_t t, bus_space_handle_t *h,
			bus_size_t o, u_int16_t v, size_t c);
void mac68k_bssr2_swap(bus_space_tag_t t, bus_space_handle_t *h,
			bus_size_t o, u_int16_t v, size_t c);
void mac68k_bssr4(bus_space_tag_t t, bus_space_handle_t *h,
			bus_size_t o, u_int32_t v, size_t c);
void mac68k_bssr4_swap(bus_space_tag_t t, bus_space_handle_t *h,
			bus_size_t o, u_int32_t v, size_t c);

#define bus_space_set_region_1(t, h, o, val, c) (h).bssr1(t, &(h), o, val, c)
#define bus_space_set_region_2(t, h, o, val, c) (h).bssr2(t, &(h), o, val, c)
#define bus_space_set_region_4(t, h, o, val, c) (h).bssr4(t, &(h), o, val, c)

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

#define	bus_space_vaddr(t, h)			(void *)((h).base)

#endif /* _MACHINE_BUS_H_ */
