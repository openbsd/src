/*	$OpenBSD: bus.h,v 1.3 1998/08/25 07:43:40 pefo Exp $	*/

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

#ifndef _MACHINE_BUS_H_
#define _MACHINE_BUS_H_

#include <machine/pio.h>

#ifdef __STDC__
#define CAT(a,b)	a##b
#define CAT3(a,b,c)	a##b##c
#else
#define CAT(a,b)	a/**/b
#define CAT3(a,b,c)	a/**/b/**/c
#endif

/*
 * Bus access types.
 */
typedef u_int32_t bus_addr_t;
typedef u_int32_t bus_size_t;
typedef u_int32_t bus_space_handle_t;
typedef struct p4e_bus_space *bus_space_tag_t;

struct p4e_bus_space {
	u_int32_t	bus_base;
	u_int8_t	bus_reverse;	/* Reverse bytes */
};

extern struct p4e_bus_space p4e_isa_io, p4e_isa_mem;

/*
 * Access methods for bus resources
 */
#define bus_space_map(t, addr, size, cacheable, bshp)			      \
    ((*(bshp) = (t)->bus_base + (addr)), 0)

#define bus_space_unmap(t, bsh, size)

#define bus_space_addr(bshp, offs)					      \
    ((vm_offset_t)((bshp) + (offs)))

#define bus_space_read(n,m)						      \
static __inline CAT3(u_int,m,_t)					      \
CAT(bus_space_read_,n)(bus_space_tag_t bst, bus_space_handle_t bsh,	      \
     bus_addr_t ba)							      \
{									      \
    if(bst->bus_reverse)						      \
	return CAT3(in,m,rb)((volatile CAT3(u_int,m,_t) *)(bsh + (ba)));      \
    else								      \
	return CAT(in,m)((volatile CAT3(u_int,m,_t) *)(bsh + (ba)));	      \
}

bus_space_read(1,8)
bus_space_read(2,16)
bus_space_read(4,32)

#define	bus_space_read_8	!!! bus_space_read_8 unimplemented !!!

#define bus_space_read_multi_1(t, h, o, a, c) do {			      \
		insb((u_int8_t *)((h) + (o)), (a), (c));		      \
	} while(0)

#define bus_space_read_multi_2(t, h, o, a, c) do {			      \
		insw((u_int16_t *)((h) + (o)), (a), (c));		      \
	} while(0)

#define bus_space_read_multi_4(t, h, o, a, c) do {			      \
		insl((u_int32_t *)((h) + (o)), (a), (c));		      \
	} while(0)

#define	bus_space_read_multi_8	!!! bus_space_read_multi_8 not implemented !!!

#define bus_space_write(n,m)						      \
static __inline void							      \
CAT(bus_space_write_,n)(bus_space_tag_t bst, bus_space_handle_t bsh,	      \
     bus_addr_t ba, CAT3(u_int,m,_t) x)					      \
{									      \
    if(bst->bus_reverse)						      \
	CAT3(out,m,rb)((volatile CAT3(u_int,m,_t) *)(bsh + (ba)), x);	      \
    else								      \
	CAT(out,m)((volatile CAT3(u_int,m,_t) *)(bsh + (ba)), x);	      \
}

bus_space_write(1,8)
bus_space_write(2,16)
bus_space_write(4,32)

#define	bus_space_write_8	!!! bus_space_write_8 unimplemented !!!


#define bus_space_write_multi_1(t, h, o, a, c) do {			      \
		outsb((u_int8_t *)((h) + (o)), (a), (c));		      \
	} while(0)

#define bus_space_write_multi_2(t, h, o, a, c) do {			      \
		outsw((u_int16_t *)((h) + (o)), (a), (c));		      \
	} while(0)

#define bus_space_write_multi_4(t, h, o, a, c) do {			      \
		outsl((u_int32_t *)((h) + (o)), (a), (c));		      \
	} while(0)

#define	bus_space_write_multi_8	!!! bus_space_write_multi_8 not implemented !!!

/* These are OpenBSD extensions to the general NetBSD bus interface.  */
#define	bus_space_read_raw_multi(n,m,l)					      \
static __inline void							      \
CAT(bus_space_read_raw_multi_,n)(bus_space_tag_t bst, bus_space_handle_t bsh, \
    bus_addr_t ba, u_int8_t *buf, bus_size_t cnt)			      \
{									      \
	CAT(bus_space_read_multi_,n)(bst, bsh, ba, (CAT3(u_int,m,_t) *)buf,   \
	    cnt >> l);							      \
}

bus_space_read_raw_multi(2,16,1)
bus_space_read_raw_multi(4,32,2)

#define	bus_space_read_raw_multi_8 \
    !!! bus_space_read_raw_multi_8 not implemented !!!

#define	bus_space_write_raw_multi(n,m,l)				      \
static __inline void							      \
CAT(bus_space_write_raw_multi_,n)(bus_space_tag_t bst, bus_space_handle_t bsh,\
    bus_addr_t ba, const u_int8_t *buf, bus_size_t cnt)			      \
{									      \
	CAT(bus_space_write_multi_,n)(bst, bsh, ba,			      \
	    (const CAT3(u_int,m,_t) *)buf, cnt >> l);			      \
}

bus_space_write_raw_multi(2,16,1)
bus_space_write_raw_multi(4,32,2)

#define	bus_space_write_raw_multi_8 \
    !!! bus_space_write_raw_multi_8 not implemented !!!

/* XXX placeholders */
typedef void *bus_dma_tag_t;
typedef void *bus_dmamap_t;

#endif /* _MACHINE_BUS_H_ */
