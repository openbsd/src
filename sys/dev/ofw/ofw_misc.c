/*	$OpenBSD: ofw_misc.c,v 1.5 2018/04/02 17:42:15 patrick Exp $	*/
/*
 * Copyright (c) 2017 Mark Kettenis
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

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/malloc.h>

#include <machine/bus.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_misc.h>

struct regmap {
	int			rm_node;
	uint32_t		rm_phandle;
	bus_space_tag_t		rm_tag;
	bus_space_handle_t	rm_handle;
	bus_size_t		rm_size;
	
	LIST_ENTRY(regmap)	rm_list;
};

LIST_HEAD(, regmap) regmaps = LIST_HEAD_INITIALIZER(regmap);

void
regmap_register(int node, bus_space_tag_t tag, bus_space_handle_t handle,
    bus_size_t size)
{
	struct regmap *rm;

	rm = malloc(sizeof(struct regmap), M_DEVBUF, M_WAITOK);
	rm->rm_node = node;
	rm->rm_phandle = OF_getpropint(node, "phandle", 0);
	rm->rm_tag = tag;
	rm->rm_handle = handle;
	rm->rm_size = size;
	LIST_INSERT_HEAD(&regmaps, rm, rm_list);
}

struct regmap *
regmap_bycompatible(char *compatible)
{
	struct regmap *rm;

	LIST_FOREACH(rm, &regmaps, rm_list) {
		if (OF_is_compatible(rm->rm_node, compatible))
			return rm;
	}

	return NULL;
}

struct regmap *
regmap_bynode(int node)
{
	struct regmap *rm;

	LIST_FOREACH(rm, &regmaps, rm_list) {
		if (rm->rm_node == node)
			return rm;
	}

	return NULL;
}

struct regmap *
regmap_byphandle(uint32_t phandle)
{
	struct regmap *rm;

	LIST_FOREACH(rm, &regmaps, rm_list) {
		if (rm->rm_phandle == phandle)
			return rm;
	}

	return NULL;
}

void
regmap_write_4(struct regmap *rm, bus_size_t offset, uint32_t value)
{
	KASSERT(offset <= rm->rm_size - sizeof(uint32_t));
	bus_space_write_4(rm->rm_tag, rm->rm_handle, offset, value);
}

uint32_t
regmap_read_4(struct regmap *rm, bus_size_t offset)
{
	KASSERT(offset <= rm->rm_size - sizeof(uint32_t));
	return bus_space_read_4(rm->rm_tag, rm->rm_handle, offset);
}
