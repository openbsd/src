/*	$OpenBSD: mainbus.c,v 1.6 2015/01/24 20:59:42 kettenis Exp $	*/

/*
 * Copyright (c) 2008 Mark Kettenis
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/autoconf.h>

#include <dev/ofw/openfirm.h>

int	mainbus_match(struct device *, void *, void *);
void	mainbus_attach(struct device *, struct device *, void *);

struct cfattach mainbus_ca = {
	sizeof(struct device), mainbus_match, mainbus_attach
};

struct cfdriver mainbus_cd = {
	NULL, "mainbus", DV_DULL
};

struct ppc_bus_space mainbus_bus_space = { 0xff400000, 0x00100000, 0 };

struct powerpc_bus_dma_tag mainbus_bus_dma_tag = {
	NULL,
	_dmamap_create,
	_dmamap_destroy,
	_dmamap_load,
	_dmamap_load_mbuf,
	_dmamap_load_uio,
	_dmamap_load_raw,
	_dmamap_unload,
	_dmamap_sync,
	_dmamem_alloc,
	_dmamem_alloc_range,
	_dmamem_free,
	_dmamem_map,
	_dmamem_unmap,
	_dmamem_mmap
};

int	mainbus_print(void *, const char *);

int
mainbus_match(struct device *parent, void *cfdata, void *aux)
{
	return (1);
}

void
mainbus_attach(struct device *parent, struct device *self, void *aux)
{
	struct mainbus_attach_args ma;
	char name[32];
	int node;

	printf("\n");

	ncpusfound = 0;
	node = OF_finddevice("/cpus");
	if (node != -1) {
		for (node = OF_child(node); node != 0; node = OF_peer(node)) {
			if (OF_getprop(node, "name", name, sizeof(name)) <= 0)
				continue;

			bzero(&ma, sizeof(ma));
			ma.ma_name = name;
			ma.ma_node = node;
			config_found(self, &ma, mainbus_print);
			ncpusfound++;
		}
	}

	for (node = OF_child(OF_peer(0)); node != 0; node = OF_peer(node)) {
		if (OF_getprop(node, "name", name, sizeof(name)) <= 0)
			continue;

		if (strcmp(name, "aliases") == 0 ||
		    strcmp(name, "chosen") == 0 ||
		    strcmp(name, "cpus") == 0 ||
		    strcmp(name, "memory") == 0)
			continue;

		bzero(&ma, sizeof(ma));
		ma.ma_iot = &mainbus_bus_space;
		ma.ma_dmat = &mainbus_bus_dma_tag;
		ma.ma_name = name;
		ma.ma_node = node;
		config_found(self, &ma, mainbus_print);
	}
}

int
mainbus_print(void *aux, const char *name)
{
	struct mainbus_attach_args *ma = aux;

	if (name)
		printf("\"%s\" at %s", ma->ma_name, name);
		
	return (UNCONF);
}
