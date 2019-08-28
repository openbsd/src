/*	$OpenBSD: ofw_misc.c,v 1.6 2019/08/28 07:03:51 kettenis Exp $	*/
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


/*
 * PHY support.
 */

LIST_HEAD(, phy_device) phy_devices =
	LIST_HEAD_INITIALIZER(phy_devices);

void
phy_register(struct phy_device *pd)
{
	pd->pd_cells = OF_getpropint(pd->pd_node, "#phy-cells", 0);
	pd->pd_phandle = OF_getpropint(pd->pd_node, "phandle", 0);
	if (pd->pd_phandle == 0)
		return;

	LIST_INSERT_HEAD(&phy_devices, pd, pd_list);
}

int
phy_enable_cells(uint32_t *cells)
{
	struct phy_device *pd;
	uint32_t phandle = cells[0];

	LIST_FOREACH(pd, &phy_devices, pd_list) {
		if (pd->pd_phandle == phandle)
			break;
	}

	if (pd && pd->pd_enable)
		return pd->pd_enable(pd->pd_cookie, &cells[1]);

	return -1;
}

uint32_t *
phy_next_phy(uint32_t *cells)
{
	uint32_t phandle = cells[0];
	int node, ncells;

	node = OF_getnodebyphandle(phandle);
	if (node == 0)
		return NULL;

	ncells = OF_getpropint(node, "#phy-cells", 0);
	return cells + ncells + 1;
}

int
phy_enable_idx(int node, int idx)
{
	uint32_t *phys;
	uint32_t *phy;
	int rv = -1;
	int len;

	len = OF_getproplen(node, "phys");
	if (len <= 0)
		return -1;

	phys = malloc(len, M_TEMP, M_WAITOK);
	OF_getpropintarray(node, "phys", phys, len);

	phy = phys;
	while (phy && phy < phys + (len / sizeof(uint32_t))) {
		if (idx <= 0)
			rv = phy_enable_cells(phy);
		if (idx == 0)
			break;
		phy = phy_next_phy(phy);
		idx--;
	}

	free(phys, M_TEMP, len);
	return rv;
}

int
phy_enable(int node, const char *name)
{
	int idx;

	idx = OF_getindex(node, name, "phy-names");
	if (idx == -1)
		return -1;

	return phy_enable_idx(node, idx);
}
