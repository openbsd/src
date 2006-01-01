/*	$OpenBSD: bus.h,v 1.6 2006/01/01 00:41:02 millert Exp $	*/
/*
 * Copyright (c) 2003, Miodrag Vallat.
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
 * ``No-nonsense'' sparc bus_space implementation.
 *
 * This is a stripped down bus_space implementation for sparc, providing
 * simple enough functions, suitable for inlining.
 * To achieve this goal, it relies upon the following assumptions:
 * - everything is memory-mapped. Hence, tags are basically not used,
 *   and most operation are just simple pointer arithmetic with the handle.
 * - interrupt functions are not implemented; callers will provide their
 *   own wrappers on a need-to-do basis.
 */

#ifndef	_SPARC_BUS_H_
#define	_SPARC_BUS_H_

#include <machine/autoconf.h>

#include <uvm/uvm_extern.h>

#include <machine/pmap.h>

typedef	u_int32_t	bus_space_handle_t;

/*
 * bus_space_tag_t are pointer to *modified* rom_reg structures.
 * rr_iospace is used to also carry bus endianness information.
 */
typedef	struct rom_reg 	*bus_space_tag_t;

#define	TAG_LITTLE_ENDIAN		0x80000000

#define	SET_TAG_BIG_ENDIAN(t)		((t))->rr_iospace &= ~TAG_LITTLE_ENDIAN
#define	SET_TAG_LITTLE_ENDIAN(t)	((t))->rr_iospace |= TAG_LITTLE_ENDIAN

#define	IS_TAG_LITTLE_ENDIAN(t)		((t)->rr_iospace & TAG_LITTLE_ENDIAN)

typedef	u_int32_t	bus_addr_t;
typedef	u_int32_t	bus_size_t;

/* 
 * General bus_space function set
 */

static int bus_space_map(bus_space_tag_t, bus_addr_t, bus_size_t, int,
    bus_space_handle_t *);
static int bus_space_unmap(bus_space_tag_t, bus_space_handle_t,
    bus_size_t);

#define	BUS_SPACE_BARRIER_READ	0
#define	BUS_SPACE_BARRIER_WRITE	1

static void bus_space_barrier(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, bus_size_t, int);

static void* bus_space_vaddr(bus_space_tag_t, bus_space_handle_t);
static int bus_space_subregion(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, bus_size_t, bus_space_handle_t *);

static __inline__ int
bus_space_map(bus_space_tag_t tag, bus_addr_t addr, bus_size_t size, int flags,
    bus_space_handle_t *handle)
{
	if ((*handle = (bus_space_handle_t)mapiodev(tag,
	    addr, size)) != NULL)
		return (0);

	return (ENOMEM);
}

static __inline__ int
bus_space_unmap(bus_space_tag_t tag, bus_space_handle_t handle, bus_size_t size)
{
	/*
	 * The mapiodev() call eventually ended up as a set of pmap_kenter_pa()
	 * calls. Although the iospace va will not be reclaimed, at least
	 * relinquish the wiring.
	 */
	pmap_kremove((vaddr_t)handle, size);
	pmap_update(pmap_kernel());
	return (0);
}

static __inline__ void
bus_space_barrier(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, bus_size_t size, int flags)
{
	/* no membar is necessary for sparc so far */
}

static __inline__ void *
bus_space_vaddr(bus_space_tag_t tag, bus_space_handle_t handle)
{
	u_int32_t iospace = tag->rr_iospace;
	void *rc;

	tag->rr_iospace &= ~TAG_LITTLE_ENDIAN;
	rc = (void *)(REG2PHYS(tag, 0) | PMAP_NC);
	tag->rr_iospace = iospace;
	return (rc);
}

static __inline__ int
bus_space_subregion(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, bus_size_t size, bus_space_handle_t *newh)
{
	*newh = handle + offset;
	return (0);
}

/*
 * Read/Write/Region functions
 * Most of these are straightforward and assume that everything is properly
 * aligned.
 */

#define	bus_space_read_1(tag, handle, offset) \
	((void)(tag), *(volatile u_int8_t *)((handle) + (offset)))
#define	__bus_space_read_2(tag, handle, offset) \
	*(volatile u_int16_t *)((handle) + (offset))
#define	__bus_space_read_4(tag, handle, offset) \
	*(volatile u_int32_t *)((handle) + (offset))
#define	bus_space_read_2(tag, handle, offset) \
	((IS_TAG_LITTLE_ENDIAN(tag)) ? \
		letoh16(__bus_space_read_2(tag, handle, offset)) : \
		__bus_space_read_2(tag, handle, offset))
#define	bus_space_read_4(tag, handle, offset) \
	((IS_TAG_LITTLE_ENDIAN(tag)) ? \
		letoh32(__bus_space_read_4(tag, handle, offset)) : \
		__bus_space_read_4(tag, handle, offset))

static void bus_space_read_multi_1(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int8_t *, size_t);

static __inline__ void
bus_space_read_multi_1(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int8_t *dest, size_t count)
{
	while ((int)--count >= 0)
		*dest++ = bus_space_read_1(tag, handle, offset);
}

static void bus_space_read_multi_2(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int16_t *, size_t);

static __inline__ void
bus_space_read_multi_2(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int16_t *dest, size_t count)
{
	while ((int)--count >= 0)
		*dest++ = bus_space_read_2(tag, handle, offset);
}

static void bus_space_read_multi_4(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int32_t *, size_t);

static __inline__ void
bus_space_read_multi_4(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int32_t *dest, size_t count)
{
	while ((int)--count >= 0)
		*dest++ = bus_space_read_4(tag, handle, offset);
}

static void bus_space_read_raw_multi_2(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int8_t *, size_t);

static __inline__ void
bus_space_read_raw_multi_2(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int8_t *dest, size_t size)
{
	size >>= 1;
	while ((int)--size >= 0) {
		*(u_int16_t *)dest =
		    __bus_space_read_2(tag, handle, offset);
		dest += 2;
	}
}

static void bus_space_read_raw_multi_4(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int8_t *, size_t);

static __inline__ void
bus_space_read_raw_multi_4(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int8_t *dest, size_t size)
{
	size >>= 2;
	while ((int)--size >= 0) {
		*(u_int32_t *)dest =
		    __bus_space_read_4(tag, handle, offset);
		dest += 4;
	}
}

#define	bus_space_write_1(tag, handle, offset, value) \
	((void)(tag), *(volatile u_int8_t *)((handle) + (offset)) = (value))
#define	__bus_space_write_2(tag, handle, offset, value) \
	*(volatile u_int16_t *)((handle) + (offset)) = (value)
#define	__bus_space_write_4(tag, handle, offset, value) \
	*(volatile u_int32_t *)((handle) + (offset)) = (value)
#define	bus_space_write_2(tag, handle, offset, value) \
	__bus_space_write_2(tag, handle, offset, \
	    (IS_TAG_LITTLE_ENDIAN(tag)) ? htole16(value) : (value))
#define	bus_space_write_4(tag, handle, offset, value) \
	__bus_space_write_4(tag, handle, offset, \
	    (IS_TAG_LITTLE_ENDIAN(tag)) ? htole32(value) : (value))

static void bus_space_write_multi_1(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int8_t *, size_t);

static __inline__ void
bus_space_write_multi_1(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int8_t *dest, size_t count)
{
	while ((int)--count >= 0)
		bus_space_write_1(tag, handle, offset, *dest++);
}

static void bus_space_write_multi_2(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int16_t *, size_t);

static __inline__ void
bus_space_write_multi_2(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int16_t *dest, size_t count)
{
	while ((int)--count >= 0)
		bus_space_write_2(tag, handle, offset, *dest++);
}

static void bus_space_write_multi_4(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int32_t *, size_t);

static __inline__ void
bus_space_write_multi_4(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int32_t *dest, size_t count)
{
	while ((int)--count >= 0)
		bus_space_write_4(tag, handle, offset, *dest);
}

static void bus_space_write_raw_multi_2(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int8_t *, size_t);

static __inline__ void
bus_space_write_raw_multi_2(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int8_t *dest, size_t size)
{
	size >>= 1;
	while ((int)--size >= 0) {
		__bus_space_write_2(tag, handle, offset,
		    *(u_int16_t *)dest);
		dest += 2;
	}
}

static void bus_space_write_raw_multi_4(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int8_t *, size_t);

static __inline__ void
bus_space_write_raw_multi_4(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int8_t *dest, size_t size)
{
	size >>= 2;
	while ((int)--size >= 0) {
		__bus_space_write_4(tag, handle, offset,
		    *(u_int32_t *)dest);
		dest += 4;
	}
}

static void bus_space_set_multi_1(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int8_t, size_t);

static __inline__ void
bus_space_set_multi_1(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int8_t value, size_t count)
{
	while ((int)--count >= 0)
		bus_space_write_1(tag, handle, offset, value);
}

static void bus_space_set_multi_2(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int16_t, size_t);

static __inline__ void
bus_space_set_multi_2(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int16_t value, size_t count)
{
	while ((int)--count >= 0)
		bus_space_write_2(tag, handle, offset, value);
}

static void bus_space_set_multi_4(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int32_t, size_t);

static __inline__ void
bus_space_set_multi_4(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int32_t value, size_t count)
{
	while ((int)--count >= 0)
		bus_space_write_4(tag, handle, offset, value);
}

static void bus_space_write_region_1(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int8_t *, size_t);

static __inline__ void
bus_space_write_region_1(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int8_t *dest, size_t count)
{
	while ((int)--count >= 0)
		bus_space_write_1(tag, handle, offset++, *dest++);
}

static void bus_space_write_region_2(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int16_t *, size_t);

static __inline__ void
bus_space_write_region_2(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int16_t *dest, size_t count)
{
	while ((int)--count >= 0) {
		bus_space_write_2(tag, handle, offset, *dest++);
		offset += 2;
	}
}

static void bus_space_write_region_4(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int32_t *, size_t);

static __inline__ void
bus_space_write_region_4(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int32_t *dest, size_t count)
{
	while ((int)--count >= 0) {
		bus_space_write_4(tag, handle, offset, *dest++);
		offset +=4;
	}
}

static void bus_space_write_raw_region_2(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int8_t *, size_t);

static __inline__ void
bus_space_write_raw_region_2(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int8_t *dest, size_t size)
{
	size >>= 1;
	while ((int)--size >= 0) {
		__bus_space_write_2(tag, handle, offset, *(u_int16_t *)dest);
		offset += 2;
		dest += 2;
	}
}

static void bus_space_write_raw_region_4(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int8_t *, size_t);

static __inline__ void
bus_space_write_raw_region_4(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int8_t *dest, size_t size)
{
	size >>= 2;
	while ((int)--size >= 0) {
		__bus_space_write_4(tag, handle, offset, *(u_int32_t *)dest);
		offset += 4;
		dest += 4;
	}
}

static void bus_space_read_region_1(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int8_t *, size_t);

static __inline__ void
bus_space_read_region_1(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int8_t *dest, size_t count)
{
	while ((int)--count >= 0)
		*dest++ = bus_space_read_1(tag, handle, offset++);
}

static void bus_space_read_region_2(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int16_t *, size_t);

static __inline__ void
bus_space_read_region_2(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int16_t *dest, size_t count)
{
	while ((int)--count >= 0) {
		*dest++ = bus_space_read_2(tag, handle, offset);
		offset += 2;
	}
}

static void bus_space_read_region_4(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int32_t *, size_t);

static __inline__ void
bus_space_read_region_4(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int32_t *dest, size_t count)
{
	while ((int)--count >= 0) {
		*dest++ = bus_space_read_4(tag, handle, offset);
		offset += 4;
	}
}

static void bus_space_set_region_1(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int8_t, size_t);

static __inline__ void
bus_space_set_region_1(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int8_t value, size_t count)
{
	while ((int)--count >= 0)
		bus_space_write_1(tag, handle, offset++, value);
}

static void bus_space_set_region_2(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int16_t, size_t);

static __inline__ void
bus_space_set_region_2(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int16_t value, size_t count)
{
	while ((int)--count >= 0) {
		bus_space_write_2(tag, handle, offset, value);
		offset += 2;
	}
}

static void bus_space_set_region_4(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int32_t, size_t);

static __inline__ void
bus_space_set_region_4(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int32_t value, size_t count)
{
	while ((int)--count >= 0) {
		bus_space_write_4(tag, handle, offset, value);
		offset += 4;
	}
}

static void bus_space_read_raw_region_2(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int8_t *, size_t);

static __inline__ void
bus_space_read_raw_region_2(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int8_t *dest, size_t size)
{
	size >>= 1;
	while ((int)--size >= 0) {
		*(u_int16_t *)dest = __bus_space_read_2(tag, handle, offset);
		offset += 2;
		dest += 2;
	}
}

static void bus_space_read_raw_region_4(bus_space_tag_t, bus_space_handle_t,
    bus_addr_t, u_int8_t *, size_t);

static __inline__ void
bus_space_read_raw_region_4(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, u_int8_t *dest, size_t size)
{
	size >>= 2;
	while ((int)--size >= 0) {
		*(u_int32_t *)dest = __bus_space_read_4(tag, handle, offset);
		offset += 4;
		dest += 4;
	}
}

#endif	/* _SPARC_BUS_H_ */
