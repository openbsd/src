/*      $OpenBSD: bus_space.c,v 1.5 2002/10/12 01:09:43 krw Exp $     */
/*      $NetBSD: bus_space.c,v 1.4 2001/06/15 15:50:05 nonaka Exp $     */

/*-
 * Copyright (c) 1996, 1997, 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum and by Jason R. Thorpe of the Numerical Aerospace
 * Simulation Facility, NASA Ames Research Center.
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
 *      This product includes software developed by the NetBSD
 *      Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/extent.h>
#include <sys/mbuf.h>

#include <machine/bus.h>

static int prep_memio_map(bus_space_tag_t, bus_addr_t, bus_size_t, int,
        bus_space_handle_t *);
static void prep_memio_unmap(bus_space_tag_t, bus_space_handle_t, bus_size_t);
static int prep_memio_alloc(bus_space_tag_t, bus_addr_t, bus_addr_t,
        bus_size_t, bus_size_t, bus_size_t, int, bus_addr_t *,
        bus_space_handle_t *);
static void prep_memio_free(bus_space_tag_t, bus_space_handle_t, bus_size_t);

const struct ppc_bus_space prep_io_space_tag = {
        PREP_BUS_SPACE_IO,  0x80000000, 0x80000000, 0x3f800000,
        prep_memio_map, prep_memio_unmap, prep_memio_alloc, prep_memio_free
};
const struct ppc_bus_space prep_isa_io_space_tag = {
        PREP_BUS_SPACE_IO,  0x80000000, 0x80000000, 0x00010000,
        prep_memio_map, prep_memio_unmap, prep_memio_alloc, prep_memio_free
};
const struct ppc_bus_space prep_mem_space_tag = {
        PREP_BUS_SPACE_MEM, 0xC0000000, 0xC0000000, 0x3f000000,
        prep_memio_map, prep_memio_unmap, prep_memio_alloc, prep_memio_free
};
const struct ppc_bus_space prep_isa_mem_space_tag = {
        PREP_BUS_SPACE_MEM, 0xC0000000, 0xC0000000, 0x01000000,
        prep_memio_map, prep_memio_unmap, prep_memio_alloc, prep_memio_free
};
static long ioport_ex_storage[EXTENT_FIXED_STORAGE_SIZE(8) / sizeof(long)];
static long iomem_ex_storage[EXTENT_FIXED_STORAGE_SIZE(8) / sizeof(long)];

struct extent *ioport_ex;
struct extent *iomem_ex;

static int ioport_malloc_safe;

void
prep_bus_space_init()
{
        int error;

        ioport_ex = extent_create("ioport", 0, 0x3f7fffff, M_DEVBUF,
            (caddr_t)ioport_ex_storage, sizeof(ioport_ex_storage),
            EX_NOCOALESCE|EX_NOWAIT);
        error = extent_alloc_region(ioport_ex, 0x10000, 0x7F0000, EX_NOWAIT);
        if (error)
                panic("prep_bus_space_init: can't block out reserved I/O space 0x10000-0x7fffff: error=%d", error);
        iomem_ex = extent_create("iomem", 0, 0x3effffff, M_DEVBUF,
            (caddr_t)iomem_ex_storage, sizeof(iomem_ex_storage),
            EX_NOCOALESCE|EX_NOWAIT);
}

void
prep_bus_space_mallocok()
{

        ioport_malloc_safe = 1;
}

static int
prep_memio_map(t, bpa, size, flags, bshp)
        bus_space_tag_t t;
        bus_addr_t bpa;
        bus_size_t size;
        int flags;
        bus_space_handle_t *bshp;
{
        int error;
        struct extent *ex;

        if (bpa + size > t->pbs_limit)
                return (EINVAL);
        /*
         * Pick the appropriate extent map.
         */
        if (t->pbs_type == PREP_BUS_SPACE_IO) {
                if (flags & BUS_SPACE_MAP_LINEAR)
                        return (EOPNOTSUPP);
                ex = ioport_ex;
        } else if (t->pbs_type == PREP_BUS_SPACE_MEM) {
                ex = iomem_ex;
        } else
                panic("prep_memio_map: bad bus space tag");

        /*
         * Before we go any further, let's make sure that this
         * region is available.
         */
        error = extent_alloc_region(ex, bpa, size,
            EX_NOWAIT | (ioport_malloc_safe ? EX_MALLOCOK : 0));
        if (error)
                return (error);

        *bshp = t->pbs_base + bpa;

        return (0);
}

static void
prep_memio_unmap(t, bsh, size)
        bus_space_tag_t t;
        bus_space_handle_t bsh;
        bus_size_t size;
{
        struct extent *ex;
        bus_addr_t bpa;

        /*
         * Find the correct extent and bus physical address.
         */
        if (t->pbs_type == PREP_BUS_SPACE_IO)
                ex = ioport_ex;
        else if (t->pbs_type == PREP_BUS_SPACE_MEM)
                ex = iomem_ex;
        else
                panic("prep_memio_unmap: bad bus space tag");

        bpa = bsh - t->pbs_base;

        if (extent_free(ex, bpa, size,
            EX_NOWAIT | (ioport_malloc_safe ? EX_MALLOCOK : 0))) {
                printf("prep_memio_unmap: %s 0x%lx, size 0x%lx\n",
                    (t->pbs_type == PREP_BUS_SPACE_IO) ? "port" : "mem",
                    (unsigned long)bpa, (unsigned long)size);
                printf("prep_memio_unmap: can't free region\n");
        }
}

static int
prep_memio_alloc(t, rstart, rend, size, alignment, boundary, flags,
    bpap, bshp)
        bus_space_tag_t t;
        bus_addr_t rstart, rend;
        bus_size_t size, alignment, boundary;
        int flags;
        bus_addr_t *bpap;
        bus_space_handle_t *bshp;
{
        struct extent *ex;
        u_long bpa;
        int error;

        if (rstart + size > t->pbs_limit)
                return (EINVAL);

        if (t->pbs_type == PREP_BUS_SPACE_IO) {
                if (flags & BUS_SPACE_MAP_LINEAR)
                        return (EOPNOTSUPP);
                ex = ioport_ex;
        } else if (t->pbs_type == PREP_BUS_SPACE_MEM) {
                ex = iomem_ex;
        } else
                panic("prep_memio_alloc: bad bus space tag");

        if (rstart < ex->ex_start || rend > ex->ex_end)
                panic("prep_memio_alloc: bad region start/end");

	error = extent_alloc_subregion(ex, rstart, rend, size, alignment, 0,
            boundary,
            EX_FAST | EX_NOWAIT | (ioport_malloc_safe ?  EX_MALLOCOK : 0),
            &bpa);
        
	if (error)
                return (error);

        *bpap = bpa;
        *bshp = t->pbs_base + bpa;

        return (0);
}

static void    
prep_memio_free(t, bsh, size)
        bus_space_tag_t t;
        bus_space_handle_t bsh;
        bus_size_t size;
{

        /* prep_memio_unmap() does all that we need to do. */
        prep_memio_unmap(t, bsh, size);
}
