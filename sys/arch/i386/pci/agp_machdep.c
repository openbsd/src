/*	$OpenBSD: agp_machdep.c,v 1.17 2014/04/01 09:05:03 mpi Exp $	*/

/*
 * Copyright (c) 2008 - 2009 Owain G. Ainsworth <oga@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
/*
 * Copyright (c) 2002 Michael Shalayeff
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR HIS RELATIVES BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF MIND, USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/rwlock.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>
#include <dev/pci/agpvar.h>

#include <uvm/uvm_extern.h>

#include <machine/cpufunc.h>
#include <machine/bus.h>
#include <machine/pmap.h>

void
agp_flush_cache(void)
{
	wbinvd();
}

void
agp_flush_cache_range(vaddr_t va, vsize_t sz)
{
	pmap_flush_cache(va, sz);
}

struct agp_map {
	bus_space_tag_t	bst;
	bus_addr_t	addr;
	bus_size_t	size;
	int		flags;
};

extern struct extent	*ioport_ex;
extern struct extent	*iomem_ex;

int
agp_init_map(bus_space_tag_t tag, bus_addr_t address, bus_size_t size,
    int flags, struct agp_map **mapp)
{
	struct extent	*ex;
	struct agp_map	*map;
	int		 error;

	switch (tag) {
	case I386_BUS_SPACE_IO:
		ex = ioport_ex;
		if (flags & BUS_SPACE_MAP_LINEAR)
			return (EINVAL);
		break;

	case I386_BUS_SPACE_MEM:
		ex = iomem_ex;
		break;

	default:
		panic("agp_init_map: bad bus space tag");
	}
	/*
	 * We grab the extent out of the bus region ourselves
	 * so we don't need to do these allocations every time.
	 */
	error = extent_alloc_region(ex, address, size,
	    EX_NOWAIT | EX_MALLOCOK);
	if (error)
		return (error);

	map = malloc(sizeof(*map), M_AGP, M_WAITOK | M_CANFAIL);
	if (map == NULL)
		return (ENOMEM);

	map->bst = tag;
	map->addr = address;
	map->size = size;
	map->flags = flags;

	*mapp = map;
	return (0);
}

void
agp_destroy_map(struct agp_map *map)
{
	struct extent	*ex;

	switch (map->bst) {
	case I386_BUS_SPACE_IO:
		ex = ioport_ex;
		break;

	case I386_BUS_SPACE_MEM:
		ex = iomem_ex;
		break;

	default:
		panic("agp_destroy_map: bad bus space tag");
	}

	if (extent_free(ex, map->addr, map->size,
	    EX_NOWAIT | EX_MALLOCOK ))
		printf("agp_destroy_map: can't free region\n");
	free(map, M_AGP);
}


int
agp_map_subregion(struct agp_map *map, bus_size_t offset, bus_size_t size,
    bus_space_handle_t *bshp)
{
	return (_bus_space_map(map->bst, map->addr + offset, size,
	    map->flags, bshp));
}

void
agp_unmap_subregion(struct agp_map *map, bus_space_handle_t bsh,
    bus_size_t size)
{
	return (_bus_space_unmap(map->bst, bsh, size, NULL));
}
