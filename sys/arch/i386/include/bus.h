/*	$OpenBSD: bus.h,v 1.9 1998/01/17 09:58:39 niklas Exp $	*/
/*	$NetBSD: bus.h,v 1.6 1996/11/10 03:19:25 thorpej Exp $	*/

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

#ifndef _I386_BUS_H_
#define _I386_BUS_H_

#include <machine/pio.h>

/*
 * Values for the i386 bus space tag, not to be used directly by MI code.
 */
#define	I386_BUS_SPACE_IO	0	/* space is i/o space */
#define I386_BUS_SPACE_MEM	1	/* space is mem space */

/*
 * Bus address and size types
 */
typedef u_long bus_addr_t;
typedef u_long bus_size_t;

/*
 * Access methods for bus resources and address space.
 */
typedef	int bus_space_tag_t;
typedef	u_long bus_space_handle_t;

int	bus_space_map __P((bus_space_tag_t t, bus_addr_t addr,
	    bus_size_t size, int cacheable, bus_space_handle_t *bshp));
void	bus_space_unmap __P((bus_space_tag_t t, bus_space_handle_t bsh,
	    bus_size_t size));
int	bus_space_subregion __P((bus_space_tag_t t, bus_space_handle_t bsh,
	    bus_size_t offset, bus_size_t size, bus_space_handle_t *nbshp));

int	bus_space_alloc __P((bus_space_tag_t t, bus_addr_t rstart,
	    bus_addr_t rend, bus_size_t size, bus_size_t align,
	    bus_size_t boundary, int cacheable, bus_addr_t *addrp,
	    bus_space_handle_t *bshp));
void	bus_space_free __P((bus_space_tag_t t, bus_space_handle_t bsh,
	    bus_size_t size));

/*
 *	u_intN_t bus_space_read_N __P((bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset));
 *
 * Read a 1, 2, 4, or 8 byte quantity from bus space
 * described by tag/handle/offset.
 */

#define	bus_space_read_1(t, h, o)					\
	((t) == I386_BUS_SPACE_IO ? (inb((h) + (o))) :			\
	    (*(volatile u_int8_t *)((h) + (o))))

#define	bus_space_read_2(t, h, o)					\
	((t) == I386_BUS_SPACE_IO ? (inw((h) + (o))) :			\
	    (*(volatile u_int16_t *)((h) + (o))))

#define	bus_space_read_4(t, h, o)					\
	((t) == I386_BUS_SPACE_IO ? (inl((h) + (o))) :			\
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

#define	bus_space_read_multi_1(t, h, o, a, c) do {			\
	if ((t) == I386_BUS_SPACE_IO) {					\
		insb((h) + (o), (a), (c));				\
	} else {							\
		__asm __volatile("					\
			cld					;	\
		1:	movb (%0),%%al				;	\
			stosb					;	\
			loop 1b"				: 	\
								:	\
		    "r" ((h) + (o)), "D" ((a)), "c" ((c))	:	\
		    "%edi", "%ecx", "%eax", "memory");			\
	}								\
} while (0)

#define	bus_space_read_multi_2(t, h, o, a, c) do {			\
	if ((t) == I386_BUS_SPACE_IO) {					\
		insw((h) + (o), (a), (c));				\
	} else {							\
		__asm __volatile("					\
			cld					;	\
		1:	movw (%0),%%ax				;	\
			stosw					;	\
			loop 1b"				:	\
								:	\
		    "r" ((h) + (o)), "D" ((a)), "c" ((c))	:	\
		    "%edi", "%ecx", "%eax", "memory");			\
	}								\
} while (0)

#define	bus_space_read_multi_4(t, h, o, a, c) do {			\
	if ((t) == I386_BUS_SPACE_IO) {					\
		insl((h) + (o), (a), (c));				\
	} else {							\
		__asm __volatile("					\
			cld					;	\
		1:	movl (%0),%%eax				;	\
			stosl					;	\
			loop 1b"				:	\
								:	\
		    "r" ((h) + (o)), "D" ((a)), "c" ((c))	:	\
		    "%edi", "%ecx", "%eax", "memory");			\
	}								\
} while (0)

#if 0	/* Cause a link error for bus_space_read_multi_8 */
#define	bus_space_read_multi_8	!!! bus_space_read_multi_8 unimplemented !!!
#endif

/*
 *	void bus_space_read_raw_multi_N __P((bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset,
 *	    u_int8_t *addr, size_t count));
 *
 * Read `count' bytes in 2, 4 or 8 byte wide quantities from bus space
 * described by tag/handle/offset and copy into buffer provided.  The buffer
 * must have proper alignment for the N byte wide entities.  Furthermore
 * possible byte-swapping should be done by these functions.
 */

#define	bus_space_read_raw_multi_2(t, h, o, a, c) \
    bus_space_read_multi_2((t), (h), (o), (u_int16_t *)(a), (c) >> 1)
#define	bus_space_read_raw_multi_4(t, h, o, a, c) \
    bus_space_read_multi_4((t), (h), (o), (u_int32_t *)(a), (c) >> 2)

#if 0	/* Cause a link error for bus_space_read_raw_multi_8 */
#define	bus_space_read_raw_multi_8 \
    !!! bus_space_read_raw_multi_8 unimplemented !!!
#endif

/*
 *	void bus_space_read_region_N __P((bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset,
 *	    u_intN_t *addr, size_t count));
 *
 * Read `count' 1, 2, 4, or 8 byte quantities from bus space
 * described by tag/handle and starting at `offset' and copy into
 * buffer provided.
 */

#define	bus_space_read_region_1(t, h, o, a, c) do {			\
	if ((t) == I386_BUS_SPACE_IO) {					\
		__asm __volatile("					\
			cld					;	\
		1:	inb %w0,%%al				;	\
			stosb					;	\
			incl %0					;	\
			loop 1b"				: 	\
								:	\
		    "d" ((h) + (o)), "D" ((a)), "c" ((c))	:	\
		    "%edx", "%edi", "%ecx", "%eax", "memory");		\
	} else {							\
		__asm __volatile("					\
			cld					;	\
			repne					;	\
			movsb"					:	\
								:	\
		    "S" ((h) + (o)), "D" ((a)), "c" ((c))	:	\
		    "%esi", "%edi", "%ecx", "memory");			\
	}								\
} while (0)

#define	bus_space_read_region_2(t, h, o, a, c) do {			\
	if ((t) == I386_BUS_SPACE_IO) {					\
		__asm __volatile("					\
			cld					;	\
		1:	inw %w0,%%ax				;	\
			stosw					;	\
			addl $2,%0				;	\
			loop 1b"				: 	\
								:	\
		    "d" ((h) + (o)), "D" ((a)), "c" ((c))	:	\
		    "%edx", "%edi", "%ecx", "%eax", "memory");		\
	} else {							\
		__asm __volatile("					\
			cld					;	\
			repne					;	\
			movsw"					:	\
								:	\
		    "S" ((h) + (o)), "D" ((a)), "c" ((c))	:	\
		    "%esi", "%edi", "%ecx", "memory");			\
	}								\
} while (0)

#define	bus_space_read_region_4(t, h, o, a, c) do {			\
	if ((t) == I386_BUS_SPACE_IO) {					\
		__asm __volatile("					\
			cld					;	\
		1:	inl %w0,%%eax				;	\
			stosl					;	\
			addl $4,%0				;	\
			loop 1b"				: 	\
								:	\
		    "d" ((h) + (o)), "D" ((a)), "c" ((c))	:	\
		    "%edx", "%edi", "%ecx", "%eax", "memory");		\
	} else {							\
		__asm __volatile("					\
			cld					;	\
			repne					;	\
			movsl"					:	\
								:	\
		    "S" ((h) + (o)), "D" ((a)), "c" ((c))	:	\
		    "%esi", "%edi", "%ecx", "memory");			\
	}								\
} while (0)

#if 0	/* Cause a link error for bus_space_read_region_8 */
#define	bus_space_read_region_8	!!! bus_space_read_region_8 unimplemented !!!
#endif

/*
 *	void bus_space_write_N __P((bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset,
 *	    u_intN_t value));
 *
 * Write the 1, 2, 4, or 8 byte value `value' to bus space
 * described by tag/handle/offset.
 */

#define	bus_space_write_1(t, h, o, v)	do {				\
	if ((t) == I386_BUS_SPACE_IO)					\
		outb((h) + (o), (v));					\
	else								\
		((void)(*(volatile u_int8_t *)((h) + (o)) = (v)));	\
} while (0)

#define	bus_space_write_2(t, h, o, v)	do {				\
	if ((t) == I386_BUS_SPACE_IO)					\
		outw((h) + (o), (v));					\
	else								\
		((void)(*(volatile u_int16_t *)((h) + (o)) = (v)));	\
} while (0)

#define	bus_space_write_4(t, h, o, v)	do {				\
	if ((t) == I386_BUS_SPACE_IO)					\
		outl((h) + (o), (v));					\
	else								\
		((void)(*(volatile u_int32_t *)((h) + (o)) = (v)));	\
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

#define	bus_space_write_multi_1(t, h, o, a, c) do {			\
	if ((t) == I386_BUS_SPACE_IO) {					\
		outsb((h) + (o), (a), (c));				\
	} else {							\
		__asm __volatile("					\
			cld					;	\
		1:	lodsb					;	\
			movb %%al,(%0)				;	\
			loop 1b"				: 	\
								:	\
		    "r" ((h) + (o)), "S" ((a)), "c" ((c))	:	\
		    "%esi", "%ecx", "%eax");				\
	}								\
} while (0)

#define bus_space_write_multi_2(t, h, o, a, c) do {			\
	if ((t) == I386_BUS_SPACE_IO) {					\
		outsw((h) + (o), (a), (c));				\
	} else {							\
		__asm __volatile("					\
			cld					;	\
		1:	lodsw					;	\
			movw %%ax,(%0)				;	\
			loop 1b"				: 	\
								:	\
		    "r" ((h) + (o)), "S" ((a)), "c" ((c))	:	\
		    "%esi", "%ecx", "%eax");				\
	}								\
} while (0)

#define bus_space_write_multi_4(t, h, o, a, c) do {			\
	if ((t) == I386_BUS_SPACE_IO) {					\
		outsl((h) + (o), (a), (c));				\
	} else {							\
		__asm __volatile("					\
			cld					;	\
		1:	lodsl					;	\
			movl %%eax,(%0)				;	\
			loop 1b"				: 	\
								:	\
		    "r" ((h) + (o)), "S" ((a)), "c" ((c))	:	\
		    "%esi", "%ecx", "%eax");				\
	}								\
} while (0)

#if 0	/* Cause a link error for bus_space_write_multi_8 */
#define	bus_space_write_multi_8(t, h, o, a, c)				\
			!!! bus_space_write_multi_8 unimplimented !!!
#endif

/*
 *	void bus_space_write_raw_multi_N __P((bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset,
 *	    u_int8_t *addr, size_t count));
 *
 * Write `count' bytes in 2, 4 or 8 byte wide quantities from the buffer
 * provided to bus space described by tag/handle/offset.  The buffer
 * must have proper alignment for the N byte wide entities.  Furthermore
 * possible byte-swapping should be done by these functions.
 */

#define	bus_space_write_raw_multi_2(t, h, o, a, c) \
    bus_space_write_multi_2((t), (h), (o), (u_int16_t *)(a), (c) >> 1)
#define	bus_space_write_raw_multi_4(t, h, o, a, c) \
    bus_space_write_multi_4((t), (h), (o), (u_int32_t *)(a), (c) >> 2)

#if 0	/* Cause a link error for bus_space_write_raw_multi_8 */
#define	bus_space_write_raw_multi_8 \
    !!! bus_space_write_raw_multi_8 unimplemented !!!
#endif

/*
 *	void bus_space_write_region_N __P((bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset,
 *	    const u_intN_t *addr, size_t count));
 *
 * Write `count' 1, 2, 4, or 8 byte quantities from the buffer provided
 * to bus space described by tag/handle starting at `offset'.
 */

#define	bus_space_write_region_1(t, h, o, a, c) do {			\
	if ((t) == I386_BUS_SPACE_IO) {					\
		__asm __volatile("					\
			cld					;	\
		1:	lodsb					;	\
			outb %%al,%w0				;	\
			incl %0					;	\
			loop 1b"				: 	\
								:	\
		    "d" ((h) + (o)), "S" ((a)), "c" ((c))	:	\
		    "%edx", "%esi", "%ecx", "%eax", "memory");		\
	} else {							\
		__asm __volatile("					\
			cld					;	\
			repne					;	\
			movsb"					:	\
								:	\
		    "D" ((h) + (o)), "S" ((a)), "c" ((c))	:	\
		    "%edi", "%esi", "%ecx", "memory");			\
	}								\
} while (0)

#define	bus_space_write_region_2(t, h, o, a, c) do {			\
	if ((t) == I386_BUS_SPACE_IO) {					\
		__asm __volatile("					\
			cld					;	\
		1:	lodsw					;	\
			outw %%ax,%w0				;	\
			addl $2,%0				;	\
			loop 1b"				: 	\
								:	\
		    "d" ((h) + (o)), "S" ((a)), "c" ((c))	:	\
		    "%edx", "%esi", "%ecx", "%eax", "memory");		\
	} else {							\
		__asm __volatile("					\
			cld					;	\
			repne					;	\
			movsw"					:	\
								:	\
		    "D" ((h) + (o)), "S" ((a)), "c" ((c))	:	\
		    "%edi", "%esi", "%ecx", "memory");			\
	}								\
} while (0)

#define	bus_space_write_region_4(t, h, o, a, c) do {			\
	if ((t) == I386_BUS_SPACE_IO) {					\
		__asm __volatile("					\
			cld					;	\
		1:	lodsl					;	\
			outl %%eax,%w0				;	\
			addl $4,%0				;	\
			loop 1b"				: 	\
								:	\
		    "d" ((h) + (o)), "S" ((a)), "c" ((c))	:	\
		    "%edx", "%esi", "%ecx", "%eax", "memory");		\
	} else {							\
		__asm __volatile("					\
			cld					;	\
			repne					;	\
			movsl"					:	\
								:	\
		    "D" ((h) + (o)), "S" ((a)), "c" ((c))	:	\
		    "%edi", "%esi", "%ecx", "memory");			\
	}								\
} while (0)

#if 0	/* Cause a link error for bus_space_write_region_8 */
#define	bus_space_write_region_8					\
			!!! bus_space_write_region_8 unimplemented !!!
#endif

/*
 *	void bus_space_set_multi_N __P((bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset,
 *	    u_intN_t val, size_t count));
 *
 * Write the 1, 2, 4, or 8 byte value `val' to bus space described
 * by tag/handle/offset `count' times.
 */

#define	bus_space_set_multi_1(t, h, o, v, c) do {			\
	if ((t) == I386_BUS_SPACE_IO) {					\
		__asm __volatile("					\
			cld					;	\
		1:	outb %%al,%w0				;	\
			loop 1b"				: 	\
								:	\
		    "d" ((h) + (o)), "c" ((c)), "a" ((v))	:	\
		    "%edx", "%ecx", "%eax");				\
	} else {							\
		__asm __volatile("					\
			cld					;	\
		1:	movb %%al,(%0)				;	\
			loop 1b"				: 	\
								:	\
		    "D" ((h) + (o)), "c" ((c)), "a" ((v))	:	\
		    "%edi", "%ecx", "%eax");				\
	}								\
} while (0)

#define	bus_space_set_multi_2(t, h, o, v, c) do {			\
	if ((t) == I386_BUS_SPACE_IO) {					\
		__asm __volatile("					\
			cld					;	\
		1:	outw %%ax,%w0				;	\
			loop 1b"				: 	\
								:	\
		    "d" ((h) + (o)), "c" ((c)), "a" ((v))	:	\
		    "%edx", "%ecx", "%eax");				\
	} else {							\
		__asm __volatile("					\
			cld					;	\
		1:	movw %%ax,(%0)				;	\
			loop 1b"				: 	\
								:	\
		    "D" ((h) + (o)), "c" ((c)), "a" ((v))	:	\
		    "%edi", "%ecx", "%eax");				\
	}								\
} while (0)

#define	bus_space_set_multi_4(t, h, o, v, c) do {			\
	if ((t) == I386_BUS_SPACE_IO) {					\
		__asm __volatile("					\
			cld					;	\
		1:	outl %%eax,%w0				;	\
			loop 1b"				: 	\
								:	\
		    "d" ((h) + (o)), "c" ((c)), "a" ((v))	:	\
		    "%edx", "%ecx", "%eax");				\
	} else {							\
		__asm __volatile("					\
			cld					;	\
		1:	movl %%eax,(%0)				;	\
			loop 1b"				: 	\
								:	\
		    "D" ((h) + (o)), "c" ((c)), "a" ((v))	:	\
		    "%edi", "%ecx", "%eax");				\
	}								\
} while (0)

#if 0	/* Cause a link error for bus_space_set_region_8 */
#define	bus_space_set_multi_8					\
			!!! bus_space_set_multi_8 unimplemented !!!
#endif

/*
 *	void bus_space_set_region_N __P((bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset,
 *	    u_intN_t val, size_t count));
 *
 * Write `count' 1, 2, 4, or 8 byte value `val' to bus space described
 * by tag/handle starting at `offset'.
 */

#define	bus_space_set_region_1(t, h, o, v, c) do {			\
	if ((t) == I386_BUS_SPACE_IO) {					\
		__asm __volatile("					\
		1:	outb %%al,%w0				;	\
			incl %0					;	\
			loop 1b"				: 	\
								:	\
		    "d" ((h) + (o)), "c" ((c)), "a" ((v))	:	\
		    "%edx", "%ecx", "%eax");				\
	} else {							\
		__asm __volatile("					\
			cld					;	\
			repne					;	\
			stosb"					:	\
								:	\
		    "D" ((h) + (o)), "c" ((c)), "a" ((v))	:	\
		    "%edi", "%ecx", "%eax", "memory");			\
	}								\
} while (0)

#define	bus_space_set_region_2(t, h, o, v, c) do {			\
	if ((t) == I386_BUS_SPACE_IO) {					\
		__asm __volatile("					\
		1:	outw %%ax,%w0				;	\
			addl $2, %0				;	\
			loop 1b"				: 	\
								:	\
		    "d" ((h) + (o)), "c" ((c)), "a" ((v))	:	\
		    "%edx", "%ecx", "%eax");				\
	} else {							\
		__asm __volatile("					\
			cld					;	\
			repne					;	\
			stosw"					:	\
								:	\
		    "D" ((h) + (o)), "c" ((c)), "a" ((v))	:	\
		    "%edi", "%ecx", "%eax", "memory");			\
	}								\
} while (0)

#define	bus_space_set_region_4(t, h, o, v, c) do {			\
	if ((t) == I386_BUS_SPACE_IO) {					\
		__asm __volatile("					\
		1:	outl %%eax,%w0				;	\
			addl $4, %0				;	\
			loop 1b"				: 	\
								:	\
		    "d" ((h) + (o)), "c" ((c)), "a" ((v))	:	\
		    "%edx", "%ecx", "%eax");				\
	} else {							\
		__asm __volatile("					\
			cld					;	\
			repne					;	\
			stosl"					:	\
								:	\
		    "D" ((h) + (o)), "c" ((c)), "a" ((v))	:	\
		    "%edi", "%ecx", "%eax", "memory");			\
	}								\
} while (0)

#if 0	/* Cause a link error for bus_space_set_region_8 */
#define	bus_space_set_region_8					\
			!!! bus_space_set_region_8 unimplemented !!!
#endif

/*
 *	void bus_space_copy_N __P((bus_space_tag_t tag,
 *	    bus_space_handle_t bsh1, bus_size_t off1,
 *	    bus_space_handle_t bsh2, bus_size_t off2,
 *	    size_t count));
 *
 * Copy `count' 1, 2, 4, or 8 byte values from bus space starting
 * at tag/bsh1/off1 to bus space starting at tag/bsh2/off2.
 */

#define	bus_space_copy_1(t, h1, o1, h2, o2, c) do {			\
	if ((t) == I386_BUS_SPACE_IO) {					\
		__asm __volatile("					\
		1:	movl %w1,%%dx				;	\
			inb  %%dx,%%al				;	\
			movl %w0,%%dx				;	\
			outl %%al,%%dx				;	\
			incl %0					;	\
			incl %1					;	\
			loop 1b"				: 	\
								:	\
		    "D" ((h2)+(o2)), "S" ((h1)+(o1)), "c" ((c))	:	\
		    "%edi", "%esi", "%ecx", "%edx", "%eax", "memory");	\
	} else {							\
		__asm __volatile("					\
			cld					;	\
			repne					;	\
			movsb"					:	\
								:	\
		    "D" ((h2)+(o2)), "S" ((h1)+(o1)), "c" ((c))	:	\
		    "%edi", "%esi", "%ecx", "memory");			\
	}								\
} while (0)

#define	bus_space_copy_2(t, h1, o1, h2, o2, c) do {			\
	if ((t) == I386_BUS_SPACE_IO) {					\
		__asm __volatile("					\
		1:	movl %w1,%%dx				;	\
			inw  %%dx,%%ax				;	\
			movl %w0,%%dx				;	\
			outw %%ax,%%dx				;	\
			addl $2, %0				;	\
			addl $2, %1				;	\
			loop 1b"				: 	\
								:	\
		    "D" ((h2)+(o2)), "S" ((h1)+(o1)), "c" ((c))	:	\
		    "%edi", "%esi", "%ecx", "%edx", "%eax", "memory");	\
	} else {							\
		__asm __volatile("					\
			cld					;	\
			repne					;	\
			movsw"					:	\
								:	\
		    "D" ((h2)+(o2)), "S" ((h1)+(o1)), "c" ((c))	:	\
		    "%edi", "%esi", "%ecx", "memory");			\
	}								\
} while (0)

#define	bus_space_copy_4(t, h1, o1, h2, o2, c) do {			\
	if ((t) == I386_BUS_SPACE_IO) {					\
		__asm __volatile("					\
		1:	movl %w1,%%dx				;	\
			inl  %%dx,%%eax				;	\
			movl %w0,%%dx				;	\
			outl %%eax,%%dx				;	\
			addl $4, %0				;	\
			addl $4, %1				;	\
			loop 1b"				: 	\
								:	\
		    "D" ((h2)+(o2)), "S" ((h1)+(o1)), "c" ((c))	:	\
		    "%edi", "%esi", "%ecx", "%edx", "%eax", "memory");	\
	} else {							\
		__asm __volatile("					\
			cld					;	\
			repne					;	\
			movsl"					:	\
								:	\
		    "D" ((h2)+(o2)), "S" ((h1)+(o1)), "c" ((c))	:	\
		    "%edi", "%esi", "%ecx", "memory");			\
	}								\
} while (0)

#if 0	/* Cause a link error for bus_space_copy_region_8 */
#define	bus_space_copy_8					\
			!!! bus_space_copy_8 unimplemented !!!
#endif

/*
 * Bus read/write barrier methods.
 *
 *	void bus_space_barrier __P((bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset,
 *	    bus_size_t len, int flags));
 *
 * Note: the i386 does not currently require barriers, but we must
 * provide the flags to MI code.
 */
#define	bus_space_barrier(t, h, o, l, f)	\
	((void)((void)(t), (void)(h), (void)(o), (void)(l), (void)(f)))
#define	BUS_BARRIER_READ	0x01		/* force read barrier */
#define	BUS_BARRIER_WRITE	0x02		/* force write barrier */

#endif /* _I386_BUS_H_ */
