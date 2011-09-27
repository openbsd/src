/*	$OpenBSD: bus.h,v 1.11 2011/09/27 20:47:30 miod Exp $	*/
/*
 * Copyright (c) 2004, Miodrag Vallat.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Simple mvme88k bus_space and bus_dma implementation.
 *
 * Currently, we only need specific handling for 32 bit read/writes in D16
 * space, and this choice is made at compile time.  As a result, all the
 * implementation can go through macros or inline functions, except for
 * the management functions.
 */

#ifndef	_MACHINE_BUS_H_
#define	_MACHINE_BUS_H_

#include <machine/asm_macro.h>

typedef	u_long	bus_addr_t;
typedef	u_long	bus_size_t;

typedef	u_long	bus_space_handle_t;

struct mvme88k_bus_space_tag {
	int	(*bs_map)(bus_addr_t, bus_size_t, int, bus_space_handle_t *);
	void	(*bs_unmap)(bus_space_handle_t, bus_size_t);
	int	(*bs_subregion)(bus_space_handle_t, bus_size_t, bus_size_t,
		    bus_space_handle_t *);
	void *	(*bs_vaddr)(bus_space_handle_t);
	/* alloc, free not implemented yet */
};

typedef const struct mvme88k_bus_space_tag *bus_space_tag_t;

#define	BUS_SPACE_BARRIER_READ	0x01
#define	BUS_SPACE_BARRIER_WRITE	0x02

#define	BUS_SPACE_MAP_CACHEABLE		0x01
#define	BUS_SPACE_MAP_LINEAR		0x02
#define	BUS_SPACE_MAP_PREFETCHABLE	0x04

/* 
 * General bus_space function set
 */

#define	bus_space_map(t,a,s,f,r)	((t)->bs_map(a,s,f,r))
#define	bus_space_unmap(t,h,s)		((t)->bs_unmap(h,s))
#define	bus_space_subregion(t,h,o,s,r)	((t)->bs_subregion(h,o,s,r))
#define	bus_space_vaddr(t,h)		((t)->bs_vaddr(h))

static void bus_space_barrier(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, bus_size_t, int);

static __inline__ void
bus_space_barrier(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, bus_size_t size, int flags)
{
	flush_pipeline();	/* overkill? */
}

/*
 * Read/Write/Region functions for D8 and D16 access.
 * Most of these are straightforward and assume that everything is properly
 * aligned.
 */

#define	bus_space_read_1(tag, handle, offset) \
	((void)(tag), *(volatile u_int8_t *)((handle) + (offset)))
#define	bus_space_read_2(tag, handle, offset) \
	((void)(tag), *(volatile u_int16_t *)((handle) + (offset)))

static void bus_space_read_multi_1(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int8_t *, size_t);

static __inline__ void
bus_space_read_multi_1(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int8_t *dest, size_t count)
{
	offset += handle;
	while ((int)--count >= 0)
		*dest++ = bus_space_read_1(tag, 0, offset);
}

static void bus_space_read_multi_2(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int16_t *, size_t);

static __inline__ void
bus_space_read_multi_2(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int16_t *dest, size_t count)
{
	offset += handle;
	while ((int)--count >= 0)
		*dest++ = bus_space_read_2(tag, 0, offset);
}

static void bus_space_read_raw_multi_2(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int8_t *, size_t);

static __inline__ void
bus_space_read_raw_multi_2(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int8_t *dest, size_t size)
{
	offset += handle;
	size >>= 1;
	while ((int)--size >= 0) {
		*(u_int16_t *)dest =
		    bus_space_read_2(tag, 0, offset);
		dest += 2;
	}
}

static void bus_space_read_region_1(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int8_t *, size_t);

static __inline__ void
bus_space_read_region_1(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int8_t *dest, size_t count)
{
	offset += handle;
	while ((int)--count >= 0)
		*dest++ = bus_space_read_1(tag, 0, offset++);
}

static void bus_space_read_region_2(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int16_t *, size_t);

static __inline__ void
bus_space_read_region_2(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int16_t *dest, size_t count)
{
	offset += handle;
	while ((int)--count >= 0) {
		*dest++ = bus_space_read_2(tag, 0, offset);
		offset += 2;
	}
}

static void bus_space_read_raw_region_2(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int8_t *, size_t);

static __inline__ void
bus_space_read_raw_region_2(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int8_t *dest, size_t size)
{
	size >>= 1;
	offset += handle;
	while ((int)--size >= 0) {
		*(u_int16_t *)dest = bus_space_read_2(tag, 0, offset);
		offset += 2;
		dest += 2;
	}
}

#define	bus_space_write_1(tag, handle, offset, value) \
	((void)(tag), *(volatile u_int8_t *)((handle) + (offset)) = (value))
#define	bus_space_write_2(tag, handle, offset, value) \
	((void)(tag), *(volatile u_int16_t *)((handle) + (offset)) = (value))

static void bus_space_write_multi_1(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int8_t *, size_t);

static __inline__ void
bus_space_write_multi_1(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int8_t *dest, size_t count)
{
	offset += handle;
	while ((int)--count >= 0)
		bus_space_write_1(tag, 0, offset, *dest++);
}

static void bus_space_write_multi_2(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int16_t *, size_t);

static __inline__ void
bus_space_write_multi_2(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int16_t *dest, size_t count)
{
	offset += handle;
	while ((int)--count >= 0)
		bus_space_write_2(tag, 0, offset, *dest++);
}

static void bus_space_write_raw_multi_2(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int8_t *, size_t);

static __inline__ void
bus_space_write_raw_multi_2(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int8_t *dest, size_t size)
{
	offset += handle;
	size >>= 1;
	while ((int)--size >= 0) {
		bus_space_write_2(tag, 0, offset, *(u_int16_t *)dest);
		dest += 2;
	}
}

static void bus_space_set_multi_1(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int8_t, size_t);

static __inline__ void
bus_space_set_multi_1(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int8_t value, size_t count)
{
	offset += handle;
	while ((int)--count >= 0)
		bus_space_write_1(tag, 0, offset, value);
}

static void bus_space_set_multi_2(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int16_t, size_t);

static __inline__ void
bus_space_set_multi_2(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int16_t value, size_t count)
{
	offset += handle;
	while ((int)--count >= 0)
		bus_space_write_2(tag, 0, offset, value);
}

static void bus_space_write_region_1(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int8_t *, size_t);

static __inline__ void
bus_space_write_region_1(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int8_t *dest, size_t count)
{
	offset += handle;
	while ((int)--count >= 0)
		bus_space_write_1(tag, 0, offset++, *dest++);
}

static void bus_space_write_region_2(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int16_t *, size_t);

static __inline__ void
bus_space_write_region_2(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int16_t *dest, size_t count)
{
	offset += handle;
	while ((int)--count >= 0) {
		bus_space_write_2(tag, 0, offset, *dest++);
		offset += 2;
	}
}

static void bus_space_write_raw_region_2(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int8_t *, size_t);

static __inline__ void
bus_space_write_raw_region_2(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int8_t *dest, size_t size)
{
	offset += handle;
	size >>= 1;
	while ((int)--size >= 0) {
		bus_space_write_2(tag, 0, offset, *(u_int16_t *)dest);
		offset += 2;
		dest += 2;
	}
}

static void bus_space_set_region_1(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int8_t, size_t);

static __inline__ void
bus_space_set_region_1(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int8_t value, size_t count)
{
	offset += handle;
	while ((int)--count >= 0)
		bus_space_write_1(tag, 0, offset++, value);
}

static void bus_space_set_region_2(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int16_t, size_t);

static __inline__ void
bus_space_set_region_2(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int16_t value, size_t count)
{
	offset += handle;
	while ((int)--count >= 0) {
		bus_space_write_2(tag, 0, offset, value);
		offset += 2;
	}
}

static void bus_space_copy_1(bus_space_tag_t, bus_space_handle_t, bus_addr_t,
    bus_space_handle_t, bus_addr_t, bus_size_t);

static __inline__ void
bus_space_copy_1(bus_space_tag_t tag, bus_space_handle_t h1, bus_addr_t o1,
    bus_space_handle_t h2, bus_addr_t o2, bus_size_t count)
{
	o1 += h1;
	o2 += h2;
	while ((int)--count >= 0) {
		*((volatile u_int8_t *)o1)++ = *((volatile u_int8_t *)o2)++;
	}
}

static void bus_space_copy_2(bus_space_tag_t, bus_space_handle_t, bus_addr_t,
    bus_space_handle_t, bus_addr_t, bus_size_t);

static __inline__ void
bus_space_copy_2(bus_space_tag_t tag, bus_space_handle_t h1, bus_addr_t o1,
    bus_space_handle_t h2, bus_addr_t o2, bus_size_t count)
{
	o1 += h1;
	o2 += h2;
	while ((int)--count >= 0) {
		*(volatile u_int16_t *)o1 = *(volatile u_int16_t *)o2;
		o1 += 2;
		o2 += 2;
	}
}

/*
 * Unrestricted D32 access
 */

#ifndef	__BUS_SPACE_RESTRICT_D16__

#define	bus_space_read_4(tag, handle, offset) \
	((void)(tag), *(volatile u_int32_t *)((handle) + (offset)))

static void bus_space_read_multi_4(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int32_t *, size_t);

static __inline__ void
bus_space_read_multi_4(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int32_t *dest, size_t count)
{
	offset += handle;
	while ((int)--count >= 0)
		*dest++ = bus_space_read_4(tag, 0, offset);
}

static void bus_space_read_raw_multi_4(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int8_t *, size_t);

static __inline__ void
bus_space_read_raw_multi_4(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int8_t *dest, size_t size)
{
	offset += handle;
	size >>= 2;
	while ((int)--size >= 0) {
		*(u_int32_t *)dest =
		    bus_space_read_4(tag, 0, offset);
		dest += 4;
	}
}

static void bus_space_read_region_4(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int32_t *, size_t);

static __inline__ void
bus_space_read_region_4(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int32_t *dest, size_t count)
{
	offset += handle;
	while ((int)--count >= 0) {
		*dest++ = bus_space_read_4(tag, 0, offset);
		offset += 4;
	}
}

static void bus_space_read_raw_region_4(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int8_t *, size_t);

static __inline__ void
bus_space_read_raw_region_4(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int8_t *dest, size_t size)
{
	size >>= 2;
	offset += handle;
	while ((int)--size >= 0) {
		*(u_int32_t *)dest = bus_space_read_4(tag, 0, offset);
		offset += 4;
		dest += 4;
	}
}

#define	bus_space_write_4(tag, handle, offset, value) \
	((void)(tag), *(volatile u_int32_t *)((handle) + (offset)) = (value))

static void bus_space_write_multi_4(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int32_t *, size_t);

static __inline__ void
bus_space_write_multi_4(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int32_t *dest, size_t count)
{
	offset += handle;
	while ((int)--count >= 0)
		bus_space_write_4(tag, 0, offset, *dest++);
}

static void bus_space_write_raw_multi_4(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int8_t *, size_t);

static __inline__ void
bus_space_write_raw_multi_4(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int8_t *dest, size_t size)
{
	offset += handle;
	size >>= 2;
	while ((int)--size >= 0) {
		bus_space_write_4(tag, 0, offset, *(u_int32_t *)dest);
		dest += 4;
	}
}

static void bus_space_set_multi_4(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int32_t, size_t);

static __inline__ void
bus_space_set_multi_4(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int32_t value, size_t count)
{
	offset += handle;
	while ((int)--count >= 0)
		bus_space_write_4(tag, 0, offset, value);
}

static void bus_space_write_region_4(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int32_t *, size_t);

static __inline__ void
bus_space_write_region_4(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int32_t *dest, size_t count)
{
	offset += handle;
	while ((int)--count >= 0) {
		bus_space_write_4(tag, 0, offset, *dest++);
		offset += 4;
	}
}

static void bus_space_write_raw_region_4(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int8_t *, size_t);

static __inline__ void
bus_space_write_raw_region_4(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int8_t *dest, size_t size)
{
	offset += handle;
	size >>= 2;
	while ((int)--size >= 0) {
		bus_space_write_4(tag, 0, offset, *(u_int32_t *)dest);
		offset += 4;
		dest += 4;
	}
}

static void bus_space_set_region_4(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int32_t, size_t);

static __inline__ void
bus_space_set_region_4(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int32_t value, size_t count)
{
	offset += handle;
	while ((int)--count >= 0) {
		bus_space_write_4(tag, 0, offset, value);
		offset += 4;
	}
}

static void bus_space_copy_4(bus_space_tag_t, bus_space_handle_t, bus_addr_t,
    bus_space_handle_t, bus_addr_t, bus_size_t);

static __inline__ void
bus_space_copy_4(bus_space_tag_t tag, bus_space_handle_t h1, bus_addr_t o1,
    bus_space_handle_t h2, bus_addr_t o2, bus_size_t count)
{
	o1 += h1;
	o2 += h2;
	while ((int)--count >= 0) {
		*(volatile u_int32_t *)o1 = *(volatile u_int32_t *)o2;
		o1 += 4;
		o2 += 4;
	}
}

#else	/* __BUS_SPACE_RESTRICT_D16__ */

/*
 * Restricted D32 access - done through two adjacent D16 access.
 *
 * The speed of the basic read and write routines is critical.
 * This implementation uses a temporary variable on stack, and does
 * two 16 bit load&store sequences. Since the stack is in Dcache, this
 * is faster and spills fewer register than a register-only sequence
 * (which would need to ld.h into two distinct registers, then extu
 * the second one into itself, and or both in the result register).
 */

static u_int32_t d16_read_4(vaddr_t);
static void d16_write_4(vaddr_t, u_int32_t);

static __inline__ u_int32_t
d16_read_4(vaddr_t va)
{
	u_int32_t tmp;

	*(u_int16_t *)&tmp = *(volatile u_int16_t *)va;
	*(u_int16_t *)((vaddr_t)&tmp + 2) = *(volatile u_int16_t *)(va + 2);

	return tmp;
}

static __inline__ void
d16_write_4(vaddr_t va, u_int32_t value)
{
	u_int32_t tmp = value;

	*(volatile u_int16_t *)va = *(u_int16_t *)&tmp;
	*(volatile u_int16_t *)(va + 2) = *(u_int16_t *)((vaddr_t)&tmp + 2);
}

#define	bus_space_read_4(tag, handle, offset) \
	((void)(tag), d16_read_4((handle) + (offset)))

static void bus_space_read_multi_4(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int32_t *, size_t);

static __inline__ void
bus_space_read_multi_4(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int32_t *dest, size_t count)
{
	offset += handle;
	while ((int)--count >= 0)
		*dest++ = bus_space_read_4(tag, 0, offset);
}

static void bus_space_read_raw_multi_4(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int8_t *, size_t);

static __inline__ void
bus_space_read_raw_multi_4(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int8_t *dest, size_t size)
{
	offset += handle;
	size >>= 1;
	while ((int)--size >= 0) {
		*(u_int16_t *)dest = bus_space_read_2(tag, 0, offset);
		dest += 2;
	}
}

static void bus_space_read_region_4(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int32_t *, size_t);

static __inline__ void
bus_space_read_region_4(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int32_t *__dest, size_t count)
{
	u_int16_t *dest = (u_int16_t *)__dest;

	offset += handle;
	count <<= 1;
	while ((int)--count >= 0) {
		*dest++ = bus_space_read_2(tag, 0, offset);
		offset += 2;
	}
}

static void bus_space_read_raw_region_4(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int8_t *, size_t);

static __inline__ void
bus_space_read_raw_region_4(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int8_t *dest, size_t size)
{
	size >>= 1;
	offset += handle;
	while ((int)--size >= 0) {
		*(u_int16_t *)dest = bus_space_read_2(tag, 0, offset);
		offset += 2;
		dest += 2;
	}
}

#define	bus_space_write_4(tag, handle, offset, value) \
	((void)(tag), d16_write_4((handle) + (offset), (value)))

static void bus_space_write_multi_4(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int32_t *, size_t);

static __inline__ void
bus_space_write_multi_4(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int32_t *dest, size_t count)
{
	offset += handle;
	while ((int)--count >= 0)
		bus_space_write_4(tag, 0, offset, *dest++);
}

static void bus_space_write_raw_multi_4(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int8_t *, size_t);

static __inline__ void
bus_space_write_raw_multi_4(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int8_t *dest, size_t size)
{
	offset += handle;
	size >>= 1;
	while ((int)--size >= 0) {
		bus_space_write_2(tag, 0, offset, *(u_int16_t *)dest);
		dest += 2;
	}
}

static void bus_space_set_multi_4(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int32_t, size_t);

static __inline__ void
bus_space_set_multi_4(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int32_t value, size_t count)
{
	offset += handle;
	while ((int)--count >= 0)
		bus_space_write_4(tag, 0, offset, value);
}

static void bus_space_write_region_4(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int32_t *, size_t);

static __inline__ void
bus_space_write_region_4(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int32_t *__dest, size_t count)
{
	u_int16_t *dest = (u_int16_t *)__dest;

	offset += handle;
	count <<= 1;
	while ((int)--count >= 0) {
		bus_space_write_2(tag, 0, offset, *dest++);
		offset += 2;
	}
}

static void bus_space_write_raw_region_4(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int8_t *, size_t);

static __inline__ void
bus_space_write_raw_region_4(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int8_t *dest, size_t size)
{
	offset += handle;
	size >>= 1;
	while ((int)--size >= 0) {
		bus_space_write_2(tag, 0, offset, *(u_int16_t *)dest);
		offset += 2;
		dest += 2;
	}
}

static void bus_space_set_region_4(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int32_t, size_t);

static __inline__ void
bus_space_set_region_4(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int32_t value, size_t count)
{
	offset += handle;
	while ((int)--count >= 0) {
		bus_space_write_4(tag, 0, offset, value);
		offset += 4;
	}
}

static void bus_space_copy_4(bus_space_tag_t, bus_space_handle_t, bus_addr_t,
    bus_space_handle_t, bus_addr_t, bus_size_t);

static __inline__ void
bus_space_copy_4(bus_space_tag_t tag, bus_space_handle_t h1, bus_addr_t o1,
    bus_space_handle_t h2, bus_addr_t o2, bus_size_t count)
{
	o1 += h1;
	o2 += h2;
	count <<= 1;
	while ((int)--count >= 0) {
		*(volatile u_int16_t *)o1 = *(volatile u_int16_t *)o2;
		o1 += 2;
		o2 += 2;
	}
}

#endif	/* __BUS_SPACE_RESTRICT_D16__ */

/*
 * Extra D16 access functions (see vme.c)
 */

void d16_bcopy(const void *, void *, size_t);
void d16_bzero(void *, size_t);

/*
 * Bus DMA implementation
 */

#define	BUS_DMA_WAITOK		0x000	/* safe to sleep (pseudo-flag) */
#define	BUS_DMA_NOWAIT		0x001	/* not safe to sleep */
#define	BUS_DMA_ALLOCNOW	0x002	/* perform resource allocation now */
#define	BUS_DMA_COHERENT	0x004	/* hint: map memory DMA coherent */
#define	BUS_DMA_BUS1		0x010	/* placeholders for bus functions... */
#define	BUS_DMA_BUS2		0x020
#define	BUS_DMA_BUS3		0x040
#define	BUS_DMA_BUS4		0x080
#define	BUS_DMA_READ		0x100	/* mapping is device -> memory only */
#define	BUS_DMA_WRITE		0x200	/* mapping is memory -> device only */
#define	BUS_DMA_STREAMING	0x400	/* hint: sequential, unidirectional */
#define	BUS_DMA_ZERO		0x800	/* zero memory in dmamem_alloc */

#define BUS_DMASYNC_PREREAD	0x01
#define BUS_DMASYNC_POSTREAD	0x02
#define BUS_DMASYNC_PREWRITE	0x04
#define BUS_DMASYNC_POSTWRITE	0x08

typedef	u_int32_t	bus_dma_tag_t;	/* ignored, really */

/*
 *	bus_dma_segment_t
 *
 *	Describes a single contiguous DMA transaction.  Values
 *	are suitable for programming into DMA registers.
 */
struct m88k_bus_dma_segment {
	bus_addr_t	ds_addr;	/* DMA address */
	bus_size_t	ds_len;		/* length of transfer */
};
typedef struct m88k_bus_dma_segment  bus_dma_segment_t;

/*
 *	bus_dmamap_t
 *
 *	Describes a DMA mapping.
 */
struct m88k_bus_dmamap {
	bus_size_t	_dm_size;	/* largest DMA transfer mappable */
	int		_dm_segcnt;	/* number of segs this map can map */
	bus_size_t	_dm_maxsegsz;	/* largest possible segment */
	bus_size_t	_dm_boundary;	/* don't cross this */

	bus_size_t	dm_mapsize;	/* size of the mapping */
	int		dm_nsegs;	/* # valid segments in mapping */
	bus_dma_segment_t dm_segs[1];	/* segments; variable length */
};
typedef struct m88k_bus_dmamap		*bus_dmamap_t;

struct mbuf;
struct proc;
struct uio;

int	bus_dmamap_create(bus_dma_tag_t, bus_size_t, int, bus_size_t,
	    bus_size_t, int, bus_dmamap_t *);
void	bus_dmamap_destroy(bus_dma_tag_t, bus_dmamap_t);
int	bus_dmamap_load(bus_dma_tag_t, bus_dmamap_t, void *,
	    bus_size_t, struct proc *, int);
int	bus_dmamap_load_mbuf(bus_dma_tag_t, bus_dmamap_t,
	    struct mbuf *, int);
int	bus_dmamap_load_uio(bus_dma_tag_t, bus_dmamap_t,
	    struct uio *, int);
int	bus_dmamap_load_raw(bus_dma_tag_t, bus_dmamap_t, bus_dma_segment_t *,
	    int, bus_size_t, int);
void	bus_dmamap_unload(bus_dma_tag_t, bus_dmamap_t);
void	bus_dmamap_sync(bus_dma_tag_t, bus_dmamap_t, bus_addr_t,
	    bus_size_t, int);

int	bus_dmamem_alloc(bus_dma_tag_t tag, bus_size_t size,
	    bus_size_t alignment, bus_size_t boundary,
	    bus_dma_segment_t *segs, int nsegs, int *rsegs, int flags);
void	bus_dmamem_free(bus_dma_tag_t tag, bus_dma_segment_t *segs,
	    int nsegs);
int	bus_dmamem_map(bus_dma_tag_t tag, bus_dma_segment_t *segs,
	    int nsegs, size_t size, caddr_t *kvap, int flags);
void	bus_dmamem_unmap(bus_dma_tag_t tag, caddr_t kva,
	    size_t size);
paddr_t bus_dmamem_mmap(bus_dma_tag_t tag, bus_dma_segment_t *segs,
	    int nsegs, off_t off, int prot, int flags);

#endif	/* _MACHINE_BUS_H_ */
