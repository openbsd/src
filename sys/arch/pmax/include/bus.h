/*	$OpenBSD: bus.h,v 1.2 1998/05/22 08:04:50 graichen Exp $	*/
/*	$NetBSD: bus.h,v 1.4 1997/11/28 00:33:53 jonathan Exp $	*/

/*
 * Copyright (c) 1997 Jonathan Stone (hereinafter referred to as the author)
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
 *      This product includes software developed by Jonathan Stone for
 *      the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */


/*
 * OpenBSD machine-indepedent bus accessor macros/functions for Decstations.
 */
#ifndef _PMAX_BUS_H_
#define _PMAX_BUS_H_

#include <machine/locore.h>			/* wbflush() */


/*
 * Bus address and size types
 */
typedef u_long bus_addr_t;
typedef u_long bus_size_t;

/*
 * Access types for bus resources and addresses.
 */
typedef int bus_space_tag_t;
typedef u_long bus_space_handle_t;


/*
 * Read or write a 1, 2, or 4-byte quantity from/to a bus-space
 * address, as defined by (space-tag,  handle, offset
 */
#define bus_space_read_1(t, h, o) \
	(*(volatile u_int8_t *)((h) + (o)))

#define bus_space_read_2(t, h, o) \
	(*(volatile u_int16_t *)((h) + (o)))

#define bus_space_read_4(t, h, o) \
	(*(volatile u_int32_t *)((h) + (o)))

#define bus_space_write_1(t, h, o, v) \
	do { ((void)(*(volatile u_int8_t *)((h) + (o)) = (v))); } while (0)

#define bus_space_write_2(t, h, o, v) \
	do { ((void)(*(volatile u_int16_t *)((h) + (o)) = (v))); } while (0)

#define bus_space_write_4(t, h, o, v) \
	do { ((void)(*(volatile u_int32_t *)((h) + (o)) = (v))); } while (0)

/*
 * Read `count'  1, 2, or 4-byte quantities from bus-space
 * address, defined by (space-tag,  handle, offset).
 * Copy to the specified buffer address.
 */
#define	bus_space_read_multi_1(t, h, o, a, c) \
    do {								\
    	register int __i ;						\
	for (__i = 0; i < (c); i++)					\
	  ((u_char *)(a))[__i] = bus_space_read_1(t, h, o);		\
    } while (0)


#define	bus_space_read_multi_2(t, h, o, a, c) \
    do {								\
    	register int __i ;						\
	for (__i = 0; i < (c); i++)					\
	  ((u_int16t_t *)(a))[__i] = bus_space_read_2(t, h, o);		\
    } while (0)

#define	bus_space_read_multi_4(t, h, o, a, c) \
    do {								\
    	register int __i ;						\
	for (__i = 0; i < (c); i++)					\
	  ((u_int32_t *)(a))[__i] = bus_space_read_4(t, h, o);		\
    } while (0)

/*
 * Write `count'  1, 2, or 4-byte quantities to a bus-space
 * address, defined by (space-tag,  handle, offset).
 * Copy from the specified buffer address.
 */
#define	bus_space_write_multi_1(t, h, o, a, c)  \
    do {								\
    	register int __i ;						\
	for (__i = 0; i < (c); i++)					\
	  bus_space_write_1(t, h, o, ((u_char *)(a))[__i]);		\
    } while (0)

#define	bus_space_write_multi_2(t, h, o, a, c)  \
    do {								\
    	register int __i ;						\
	for (__i = 0; i < (c); i++)					\
	  bus_space_write_2(t, h, o, ((u_int16_t *)(a))[__i]);		\
    } while (0)

#define	bus_space_write_multi_4(t, h, o, a, c)  \
    do {								\
    	register int __i ;						\
	for (__i = 0; i < (c); i++)					\
	  bus_space_write_4(t, h, o, ((u_int32_t *)(a))[__i]);		\
    } while (0)

/*
 * Copy `count' 1, 2, or 4-byte values from one bus-space address
 * (t,  h, o triple) to another.
 */
#define	bus_space_copy_multi_1(t, h1, h2, o1, o2, c) \
    do {								\
    	register int __i ;						\
	for (__i = 0; i < (c); i++)					\
	  bus_space_write_1(t, h1, o1, bus_space_read_1(t, h2, o2));	\
    } while (0)

#define	bus_space_copy_multi_2(t, h1, h2, o1, o2, c) \
    do {								\
    	register int __i ;						\
	for (__i = 0; i < (c); i++)					\
	  bus_space_write_2(t, h1, o1, bus_space_read_2(t, h2, o2));	\
    while (0)

#define	bus_space_copy_multi_4(t,  h1, h2, o1, o2, c) \
    do {								\
    	register int __i ;						\
	for (__i = 0; i < (c); i++)					\
	  bus_space_write_4(t, h1, o1, bus_space_read_4(t, h2, o2));	\
    } while (0)


/*
 * Bus-space barriers.
 * Since DECstation DMA is non-cache-coherent, we have to handle
 * consistency in software anyway (e.g., via bus -DMA, or by ensuring
 * that DMA buffers are referenced via  uncached address space.
 * For now, simply do CPU writebuffer flushes and export the flags
 * to  MI code.
 */
#define bus_space_barrier(t, h, o, l, f)	wbflush()

#define BUS_BARRIER_READ 	0x01
#define BUS_BARRIER_WRITE	0x02

#endif /* _PMAX_BUS_H_ */
