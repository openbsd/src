/*	$OpenBSD: bus.h,v 1.1 1998/11/23 03:36:53 mickey Exp $	*/

/*
 * Copyright (c) 1998 Michael Shalayeff
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
 *	This product includes software developed by Michael Shalayeff.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

#include <machine/cpufunc.h>

/* addresses in bus space */
typedef u_long bus_addr_t;
typedef u_long bus_size_t;

/* access methods for bus space */
typedef u_long bus_space_tag_t;
typedef u_long bus_space_handle_t;
typedef	u_long bus_dma_tag_t;

#define	HPPA_BUS_TAG_SET_BYTE(tag)	((tag) & (~1))
#define	HPPA_BUS_TAG_SET_WORD(tag,off)	((tag) | (1) | ((off) << 1))
#define	HPPA_BUS_TAG_PROTO(tag)		((tag) & 1)
#define	HPPA_BUS_TAG_OFFSET(tag)	(((tag) >> 1) & 3)
#define	HPPA_BUS_TAG_SET_BASE(tag,base)	\
		((((tag) & 0x00000fff)) | ((base) & 0xfffff000))
#define	HPPA_BUS_TAG_BASE(tag)		((tag) & 0xfffff000)

/* bus access routines */
#define DCIAS(pa) \
	__asm __volatile ("rsm %1, %%r0\n\tpdc %%r0(%0)\n\tssm %1, %%r0" \
			  :: "r" (pa), "i" (PSW_D));

/* no extent handlng for now
   we won't have overlaps from PDC anyway */
static __inline int bus_space_map (bus_space_tag_t t, bus_addr_t addr,
				   bus_size_t size, int cacheable,
				   bus_space_handle_t *bshp)
{
	*bshp = addr + HPPA_BUS_TAG_BASE(t);
	return 0;
}

static __inline void bus_space_unmap (bus_space_tag_t t,
				      bus_space_handle_t bsh,
				      bus_size_t size)
{
	/* nothing to do */
}

int	bus_space_subregion __P((bus_space_tag_t t, bus_space_handle_t bsh,
	    bus_size_t offset, bus_size_t size, bus_space_handle_t *nbshp));

int	bus_space_alloc __P((bus_space_tag_t t, bus_addr_t rstart,
	    bus_addr_t rend, bus_size_t size, bus_size_t align,
	    bus_size_t boundary, int cacheable, bus_addr_t *addrp,
	    bus_space_handle_t *bshp));
void	bus_space_free __P((bus_space_tag_t t, bus_space_handle_t bsh,
	    bus_size_t size));

static __inline u_int8_t
bus_space_read_1(bus_space_tag_t t, bus_space_handle_t h, int o)
{		
	register u_int32_t v;

	if (HPPA_BUS_TAG_PROTO(t))
		o = (o << 2) | HPPA_BUS_TAG_OFFSET(t);

	__asm __volatile ("rsm %3, %%r0\n\t"
			  "ldbx %2(%1), %0\n\t"
			  "pdc %2(%1)\n\t"
			  "ssm %3, %%r0"
			  : "=r" (v): "r" (h), "r" (o), "i" (PSW_D));
	return v & 0xff;
}

static __inline u_int16_t
bus_space_read_2(bus_space_tag_t t, bus_space_handle_t h, int o)
{		
	register u_int32_t v;

	if (HPPA_BUS_TAG_PROTO(t))
		o = (o << 2) | HPPA_BUS_TAG_OFFSET(t);

	__asm __volatile ("rsm %3, %%r0\n\t"
			  "ldhx %2(%1), %0\n\t"
			  "pdc %2(%1)\n\t"
			  "ssm %3, %%r0"
			  : "=r" (v): "r" (h), "r" (o), "i" (PSW_D));
	return v & 0xffff;
}

static __inline u_int32_t
bus_space_read_4(bus_space_tag_t t, bus_space_handle_t h, int o)
{		
	register u_int32_t v;

	if (HPPA_BUS_TAG_PROTO(t))
		o = (o << 2) | HPPA_BUS_TAG_OFFSET(t);

	__asm __volatile ("rsm %3, %%r0\n\t"
			  "ldwx %2(%1), %0\n\t"
			  "pdc %2(%1)\n\t"
			  "ssm %3, %%r0"
			  : "=r" (v): "r" (h), "r" (o), "i" (PSW_D));
	return v;
}

#if 0
#define	bus_space_read_8(t, h, o)
#endif

static __inline void
bus_space_read_multi_1(bus_space_tag_t t, bus_space_handle_t h,
		       bus_size_t o, u_int8_t *a, size_t c)
{
	register u_int32_t v;

	if (HPPA_BUS_TAG_PROTO(t))
		o = (o << 2) | HPPA_BUS_TAG_OFFSET(t);

	for (; c--; *(a++) = v & 0xff)
		__asm __volatile ("rsm %3, %%r0\n\t"
				  "ldbx %2(%1), %0\n\t"
				  "pdc %2(%1)\n\t"
				  "ssm %3, %%r0"
				  : "=r" (v): "r" (h), "r" (o), "i" (PSW_D));
}

static __inline void
bus_space_read_multi_2(bus_space_tag_t t, bus_space_handle_t h,
		       bus_size_t o, u_int16_t *a, size_t c)
{
	register u_int32_t v;

	if (HPPA_BUS_TAG_PROTO(t))
		o = (o << 2) | HPPA_BUS_TAG_OFFSET(t);

	for (; c--; *(a++) = v & 0xffff)
		__asm __volatile ("rsm %3, %%r0\n\t"
				  "ldhx %2(%1), %0\n\t"
				  "pdc %2(%1)\n\t"
				  "ssm %3, %%r0"
				  : "=r" (v): "r" (h), "r" (o), "i" (PSW_D));
}

static __inline void
bus_space_read_multi_4(bus_space_tag_t t, bus_space_handle_t h,
		       bus_size_t o, u_int32_t *a, size_t c)
{
	register u_int32_t v;

	if (HPPA_BUS_TAG_PROTO(t))
		o = (o << 2) | HPPA_BUS_TAG_OFFSET(t);

	for (; c--; *(a++) = v)
		__asm __volatile ("rsm %3, %%r0\n\t"
				  "ldwx %2(%1), %0\n\t"
				  "pdc %2(%1)\n\t"
				  "ssm %3, %%r0"
				  : "=r" (v): "r" (h), "r" (o), "i" (PSW_D));
}

#if 0
#define	bus_space_read_multi_8
#endif

#define	bus_space_read_raw_multi_2(t, h, o, a, c) \
    bus_space_read_multi_2((t), (h), (o), (u_int16_t *)(a), (c) >> 1)
#define	bus_space_read_raw_multi_4(t, h, o, a, c) \
    bus_space_read_multi_4((t), (h), (o), (u_int32_t *)(a), (c) >> 2)

#if 0
#define	bus_space_read_raw_multi_8
#endif

#if 0
#define	bus_space_read_region_1(t, h, o, a, c) do {		\
} while (0)

#define	bus_space_read_region_2(t, h, o, a, c) do {		\

} while (0)

#define	bus_space_read_region_4(t, h, o, a, c) do {		\
} while (0)

#define	bus_space_read_region_8
#endif

#if 0
#define	bus_space_read_raw_region_2(t, h, o, a, c) \
    bus_space_read_region_2((t), (h), (o), (u_int16_t *)(a), (c) >> 1)
#define	bus_space_read_raw_region_4(t, h, o, a, c) \
    bus_space_read_region_4((t), (h), (o), (u_int32_t *)(a), (c) >> 2)

#define	bus_space_read_raw_region_8
#endif

#define	bus_space_write_1(t, h, o, v)	do {				\
	__asm __volatile (						\
		"rsm %0, %%r0\n\t"					\
		"stbs %1, 0(%2)\n\t"					\
		"fdc %%r0(%2)\n\t"					\
		"ssm %0, %%r0"						\
		:: "i" (PSW_D), "r" (v),				\
		   "r" (h + ((HPPA_BUS_TAG_PROTO(t))?			\
			     ((o) << 2) | HPPA_BUS_TAG_OFFSET(t):(o))));\
} while (0)

#define	bus_space_write_2(t, h, o, v)	do {				\
	__asm __volatile (						\
		"rsm %0, %%r0\n\t"					\
		"sths %1, 0(%2)\n\t"					\
		"fdc %%r0(%2)\n\t"					\
		"ssm %0, %%r0"						\
		:: "i" (PSW_D), "r" (v),				\
		   "r" (h + ((HPPA_BUS_TAG_PROTO(t))?			\
			     ((o) << 2) | HPPA_BUS_TAG_OFFSET(t):(o))));\
} while (0)

#define	bus_space_write_4(t, h, o, v)	do {				\
	__asm __volatile (						\
		"rsm %0, %%r0\n\t"					\
		"stws %1, 0(%2)\n\t"					\
		"fdc %%r0(%2)\n\t"					\
		"ssm %0, %%r0"						\
		:: "i" (PSW_D), "r" (v),				\
		   "r" (h + ((HPPA_BUS_TAG_PROTO(t))?			\
			     ((o) << 2) | HPPA_BUS_TAG_OFFSET(t):(o))));\
} while (0)

#if 0
#define	bus_space_write_8
#endif

#define	bus_space_write_multi_1(t, h, o, a, c) do {		\
} while (0)

#define bus_space_write_multi_2(t, h, o, a, c) do {		\
} while (0)

#define bus_space_write_multi_4(t, h, o, a, c) do {		\
} while (0)

#if 0
#define	bus_space_write_multi_8(t, h, o, a, c)
#endif

#define	bus_space_write_raw_multi_2(t, h, o, a, c) \
    bus_space_write_multi_2((t), (h), (o), (const u_int16_t *)(a), (c) >> 1)
#define	bus_space_write_raw_multi_4(t, h, o, a, c) \
    bus_space_write_multi_4((t), (h), (o), (const u_int32_t *)(a), (c) >> 2)

#if 0
#define	bus_space_write_raw_multi_8
#endif

#if 0
#define	bus_space_write_region_1(t, h, o, a, c) do {		\
} while (0)

#define	bus_space_write_region_2(t, h, o, a, c) do {		\
} while (0)

#define	bus_space_write_region_4(t, h, o, a, c) do {		\
} while (0)

#define	bus_space_write_region_8
#endif

#define	bus_space_write_raw_region_2(t, h, o, a, c) \
    bus_space_write_region_2((t), (h), (o), (const u_int16_t *)(a), (c) >> 1)
#define	bus_space_write_raw_region_4(t, h, o, a, c) \
    bus_space_write_region_4((t), (h), (o), (const u_int32_t *)(a), (c) >> 2)

#if 0
#define	bus_space_write_raw_region_8
#endif

#if 0
#define	bus_space_set_multi_1(t, h, o, v, c) do {		\
} while (0)

#define	bus_space_set_multi_2(t, h, o, v, c) do {		\
} while (0)

#define	bus_space_set_multi_4(t, h, o, v, c) do {		\
} while (0)

#define	bus_space_set_multi_8
#endif

#if 0
#define	bus_space_set_region_1(t, h, o, v, c) do {		\
} while (0)

#define	bus_space_set_region_2(t, h, o, v, c) do {		\
} while (0)

#define	bus_space_set_region_4(t, h, o, v, c) do {		\
} while (0)

#define	bus_space_set_region_8
#endif

#if 0
#define	bus_space_copy_1(t, h1, o1, h2, o2, c) do {		\
} while (0)

#define	bus_space_copy_2(t, h1, o1, h2, o2, c) do {		\
} while (0)

#define	bus_space_copy_4(t, h1, o1, h2, o2, c) do {		\
} while (0)

#define	bus_space_copy_8
#endif

#endif /* _MACHINE_BUS_H_ */

