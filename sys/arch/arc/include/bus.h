/*	$OpenBSD: bus.h,v 1.3 1996/07/30 20:24:24 pefo Exp $	*/

/*
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

#ifndef _ARC_BUS_H_
#define _ARC_BUS_H_

#include <machine/autoconf.h>
#include <machine/pio.h>

/*
 * I/O addresses (in bus space)
 */
typedef u_long bus_io_addr_t;
typedef u_long bus_io_size_t;
typedef u_long bus_io_handle_t;

/*
 * Memory addresses (in bus space)
 */
typedef u_long bus_mem_addr_t;
typedef u_long bus_mem_size_t;
typedef caddr_t bus_mem_handle_t;

/*
 * Access methods for bus resources, I/O space, and memory space.
 */
typedef
struct arc_isa_busmap {
	void	*isa_io_base;
	void	*isa_mem_base;
} *bus_chipset_tag_t;


#define bus_io_map(t, port, size, iohp)					\
    (*iohp = (t == NULL ? port : port + (ulong)(t->isa_io_base)), 0)
#define bus_io_unmap(t, ioh, size)

#define	bus_io_read_1(t, h, o)		inb((h) + (o))
#define	bus_io_read_2(t, h, o)		inw((h) + (o))
#define	bus_io_read_4(t, h, o)		inl((h) + (o))
#if 0 /* Cause a link error for bus_io_read_8 */
#define	bus_io_read_8(t, h, o)		!!! bus_io_read_8 unimplemented !!!
#endif

#define	bus_io_read_multi_1(t, h, o, a, c)	\
					insb((h) + (o), (a), (c))
#define	bus_io_read_multi_2(t, h, o, a, c)	\
					insw((h) + (o), (a), (c))
#define	bus_io_read_multi_4(t, h, o, a, c)	\
					insl((h) + (o), (a), (c))
#if 0 /* Cause a link error for bus_io_read_multi_8 */
#define	bus_io_read_multi_8(t, h, o, a, c)	\
				!!! bus_io_read_multi_8 unimplemented !!!
#endif

#define	bus_io_write_1(t, h, o, v)	outb((h) + (o), (v))
#define	bus_io_write_2(t, h, o, v)	outw((h) + (o), (v))
#define	bus_io_write_4(t, h, o, v)	outl((h) + (o), (v))
#if 0 /* Cause a link error for bus_io_write_8 */
#define	bus_io_write_8(t, h, o, v)	!!! bus_io_write_8 unimplemented !!!
#endif

#define	bus_io_write_multi_1(t, h, o, a, c)	\
					outsb((h) + (o), (a), (c))
#define	bus_io_write_multi_2(t, h, o, a, c)	\
					outsw((h) + (o), (a), (c))
#define	bus_io_write_multi_4(t, h, o, a, c)	\
					outsl((h) + (o), (a), (c))
#if 0 /* Cause a link error for bus_io_write_multi_8 */
#define	bus_io_write_multi_8(t, h, o, a, c)	\
				!!! bus_io_write_multi_8 unimplimented !!!
#endif

#define bus_mem_map(t, addr, size, cacheable, mhp)			\
    (*mhp = (t == NULL ? port : port + (ulong)(t->isa_mem_base)), 0)
#define bus_mem_unmap(t, ioh, size)

#define	bus_mem_read_1(t, h, o)		(*(volatile u_int8_t *)((h) + (o)))
#define	bus_mem_read_2(t, h, o)		(*(volatile u_int16_t *)((h) + (o)))
#define	bus_mem_read_4(t, h, o)		(*(volatile u_int32_t *)((h) + (o)))
#define	bus_mem_read_8(t, h, o)		(*(volatile u_int64_t *)((h) + (o)))

#define	bus_mem_write_1(t, h, o, v)					\
    ((void)(*(volatile u_int8_t *)((h) + (o)) = (v)))
#define	bus_mem_write_2(t, h, o, v)					\
    ((void)(*(volatile u_int16_t *)((h) + (o)) = (v)))
#define	bus_mem_write_4(t, h, o, v)					\
    ((void)(*(volatile u_int32_t *)((h) + (o)) = (v)))
#define	bus_mem_write_8(t, h, o, v)					\
    ((void)(*(volatile u_int64_t *)((h) + (o)) = (v)))

/* These are extensions to the general NetBSD bus interface.  */
/* XXX Check endianess */
#define bus_to_host_2(t, v) (v)
#define bus_to_host_4(t, v) (v)
#define bus_to_host_8(t, v) (v)

#define bus_from_host_2(t, v) (v)
#define bus_from_host_4(t, v) (v)
#define bus_from_host_8(t, v) (v)

#endif /* _ARC_BUS_H_ */
