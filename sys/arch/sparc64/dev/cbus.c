/*	$OpenBSD: cbus.c,v 1.4 2009/04/04 11:35:03 kettenis Exp $	*/
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
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/systm.h>

#include <machine/autoconf.h>
#include <machine/hypervisor.h>
#include <machine/openfirm.h>

#include <sparc64/dev/cbusvar.h>
#include <sparc64/dev/vbusvar.h>

struct md_header {
	uint32_t	transport_version;
	uint32_t	node_blk_sz;
	uint32_t	name_blk_sz;
	uint32_t	data_blk_sz;
};

struct md_element {
	uint8_t		tag;
	uint8_t		name_len;
	uint16_t	_reserved_field;
	uint32_t	name_offset;
	union {
		struct {
			uint32_t	data_len;
			uint32_t	data_offset;
		} y;
		uint64_t	val;
	} d;
};

struct cbus_softc {
	struct device		sc_dv;
	bus_space_tag_t		sc_bustag;
	bus_dma_tag_t		sc_dmatag;

	/* Machine description. */
	caddr_t			sc_md;
	int			sc_idx;
};

int	cbus_match(struct device *, void *, void *);
void	cbus_attach(struct device *, struct device *, void *);
int	cbus_print(void *, const char *);

struct cfattach cbus_ca = {
	sizeof(struct cbus_softc), cbus_match, cbus_attach
};

struct cfdriver cbus_cd = {
	NULL, "cbus", DV_DULL
};

void	*cbus_intr_establish(bus_space_tag_t, bus_space_tag_t, int, int, int,
    int (*)(void *), void *, const char *);
void	cbus_intr_ack(struct intrhand *);
bus_space_tag_t cbus_alloc_bus_tag(struct cbus_softc *, bus_space_tag_t);

caddr_t	cbus_get_mach_desc(struct cbus_softc *);
int	cbus_get_channel_endpoint(struct cbus_softc *,
	    struct cbus_attach_args *);

uint64_t sun4v_mdesc_get_prop_val(caddr_t, int, const char *);
const char *sun4v_mdesc_get_prop_string(caddr_t, int, const char *);
int	sun4v_mdesc_find(caddr_t, const char *, uint64_t);
int	sun4v_mdesc_find_child(caddr_t, int, const char *, uint64_t);

int
cbus_match(struct device *parent, void *match, void *aux)
{
	struct vbus_attach_args *va = aux;

	if (strcmp(va->va_name, "channel-devices") == 0)
		return (1);

	return (0);
}

void
cbus_attach(struct device *parent, struct device *self, void *aux)
{
	struct cbus_softc *sc = (struct cbus_softc *)self;
	struct vbus_attach_args *va = aux;
	int node;

	sc->sc_bustag = cbus_alloc_bus_tag(sc, va->va_bustag);
	sc->sc_dmatag = va->va_dmatag;

	sc->sc_md = cbus_get_mach_desc(sc);
	if (sc->sc_md == NULL) {
		printf(": can't read machine description\n");
		return;
	}

	printf("\n");

	sc->sc_idx = sun4v_mdesc_find(sc->sc_md, va->va_name, va->va_reg[0]);
	if (sc->sc_idx == -1)
		return;

	for (node = OF_child(va->va_node); node; node = OF_peer(node)) {
		struct cbus_attach_args ca;
		char buf[32];

		bzero(&ca, sizeof(ca));
		ca.ca_node = node;
		if (OF_getprop(node, "name", buf, sizeof(buf)) <= 0)
			continue;
		ca.ca_name = buf;
		ca.ca_bustag = sc->sc_bustag;
		ca.ca_dmatag = sc->sc_dmatag;
		getprop(node, "reg", sizeof(*ca.ca_reg),
		    &ca.ca_nreg, (void **)&ca.ca_reg);
		if (cbus_get_channel_endpoint(sc, &ca) != 0)
			continue;

		config_found(self, &ca, cbus_print);
	}
}

int
cbus_print(void *aux, const char *name)
{
	struct cbus_attach_args *ca = aux;

	if (name)
		printf("\"%s\" at %s", ca->ca_name, name);
	if (ca->ca_id != -1)
		printf(" chan 0x%llx", ca->ca_id);
	return (UNCONF);
}

int
cbus_intr_map(int node, int ino, uint64_t *sysino)
{
	int parent;
	int reg;
	int err;

	parent = OF_parent(node);
	if (OF_getprop(parent, "reg", &reg, sizeof(reg)) != sizeof(reg))
		return (-1);

	*sysino = INTIGN(reg) | INTINO(ino);
	err = hv_vintr_setcookie(reg, ino, *sysino);
	if (err != H_EOK)
		return (-1);

	return (0);
}

void *
cbus_intr_establish(bus_space_tag_t t, bus_space_tag_t t0, int ihandle,
    int level, int flags, int (*handler)(void *), void *arg, const char *what)
{
	uint64_t devhandle = INTIGN(ihandle);
	uint64_t devino = INTINO(ihandle);
	struct intrhand *ih;
	int err;

	ih = bus_intr_allocate(t0, handler, arg, ihandle, level,
	    NULL, NULL, what);
	if (ih == NULL)
		return (NULL);

	intr_establish(ih->ih_pil, ih);
	ih->ih_ack = cbus_intr_ack;

	err = hv_vintr_settarget(devhandle, devino, cpus->ci_upaid);
	if (err != H_EOK) {
		printf("hv_vintr_settarget: %d\n", err);
		return (NULL);
	}

	/* Clear pending interrupts. */
	err = hv_vintr_setstate(devhandle, devino, INTR_IDLE);
	if (err != H_EOK) {
		printf("hv_vintr_setstate: %d\n", err);
		return (NULL);
	}

	err = hv_vintr_setenabled(devhandle, devino, INTR_ENABLED);
	if (err != H_EOK) {
		printf("hv_vintr_setenabled: %d\n", err);
		return (NULL);
	}

	return (ih);
}

void
cbus_intr_ack(struct intrhand *ih)
{
	uint64_t devhandle = INTIGN(ih->ih_number);
	uint64_t devino = INTINO(ih->ih_number);

	hv_vintr_setstate(devhandle, devino,  INTR_IDLE);
}

bus_space_tag_t
cbus_alloc_bus_tag(struct cbus_softc *sc, bus_space_tag_t parent)
{
	struct sparc_bus_space_tag *bt;

	bt = malloc(sizeof(*bt), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (bt == NULL)
		panic("could not allocate cbus bus tag");

	snprintf(bt->name, sizeof(bt->name), "%s", sc->sc_dv.dv_xname);
	bt->cookie = sc;
	bt->parent = parent;
	bt->asi = parent->asi;
	bt->sasi = parent->sasi;
	bt->sparc_bus_map = parent->sparc_bus_map;
	bt->sparc_intr_establish = cbus_intr_establish;

	return (bt);
}

caddr_t
cbus_get_mach_desc(struct cbus_softc *sc)
{
	bus_dmamap_t map;
	bus_dma_segment_t seg;
	psize_t len;
	bus_size_t size;
	caddr_t va;
	int nsegs, err = 0;

	len = 0;
	hv_mach_desc((paddr_t)NULL, &len);
	KASSERT(len != 0);

again:
	size = roundup(len, PAGE_SIZE);

	if (bus_dmamap_create(sc->sc_dmatag, size, 1, size, 0,
	    BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW, &map) != 0)
		return (NULL);

	if (bus_dmamem_alloc(sc->sc_dmatag, size, PAGE_SIZE, 0, &seg,
	    1, &nsegs, BUS_DMA_NOWAIT) != 0)
		goto destroy;

	if (bus_dmamem_map(sc->sc_dmatag, &seg, 1, size,
	    &va, BUS_DMA_NOWAIT) != 0)
		goto free;

	if (bus_dmamap_load(sc->sc_dmatag, map, va, size,
	    NULL, BUS_DMA_NOWAIT) != 0)
		goto unmap;

	len = size;
	err = hv_mach_desc(map->dm_segs[0].ds_addr, &len);
	if (err != H_EOK)
		goto unload;

	return (va);

unload:
	bus_dmamap_unload(sc->sc_dmatag, map);
unmap:
	bus_dmamem_unmap(sc->sc_dmatag, va, size);
free:
	bus_dmamem_free(sc->sc_dmatag, &seg, 1);
destroy:
	bus_dmamap_destroy(sc->sc_dmatag, map);

	/*
	 * If the machine description was updated while we were trying
	 * to fetch it, the allocated buffer may have been to small.
	 * Try again in that case.
	 */
	if (err == H_EINVAL && len > size)
		goto again;

	return (NULL);
}

int
cbus_get_channel_endpoint(struct cbus_softc *sc, struct cbus_attach_args *ca)
{
	struct md_header *hdr;
	struct md_element *elem;
	const char *name_blk;
	const char *str;
	int idx;
	int arc;

	idx = sun4v_mdesc_find_child(sc->sc_md, sc->sc_idx,
	    ca->ca_name, ca->ca_reg[0]);
	if (idx == -1)
		return (ENOENT);

	hdr = (struct md_header *)sc->sc_md;
	elem = (struct md_element *)(sc->sc_md + sizeof(struct md_header));
	name_blk = sc->sc_md + sizeof(struct md_header) + hdr->node_blk_sz;

	for (; elem[idx].tag != 'E'; idx++) {
		str = name_blk + elem[idx].name_offset;
		if (elem[idx].tag != 'a' || strcmp(str, "fwd") != 0)
			continue;

		arc = elem[idx].d.val;
		str = name_blk + elem[arc].name_offset;
		if (strcmp(str, "virtual-device-port") == 0) {
			idx = arc;
			continue;
		}

		if (strcmp(str, "channel-endpoint") == 0) {
			ca->ca_id =
			    sun4v_mdesc_get_prop_val(sc->sc_md, arc, "id");
			ca->ca_tx_ino =
			    sun4v_mdesc_get_prop_val(sc->sc_md, arc, "tx-ino");
			ca->ca_rx_ino = 
			    sun4v_mdesc_get_prop_val(sc->sc_md, arc, "rx-ino");
			return (0);
		}
	}

	ca->ca_id = -1;
	ca->ca_tx_ino = -1;
	ca->ca_rx_ino = -1;
	return (0);
}

uint64_t
sun4v_mdesc_get_prop_val(caddr_t md, int idx, const char *name)
{
	struct md_header *hdr;
	struct md_element *elem;
	const char *name_blk;
	const char *str;

	hdr = (struct md_header *)md;
	elem = (struct md_element *)(md + sizeof(struct md_header));
	name_blk = md + sizeof(struct md_header) + hdr->node_blk_sz;

	while (elem[idx].tag != 'E') {
		str = name_blk + elem[idx].name_offset;
		if (elem[idx].tag == 'v' && strcmp(str, name) == 0)
			return (elem[idx].d.val);
		idx++;
	}

	return (-1);
}

const char *
sun4v_mdesc_get_prop_string(caddr_t md, int idx, const char *name)
{
	struct md_header *hdr;
	struct md_element *elem;
	const char *name_blk;
	const char *data_blk;
	const char *str;

	hdr = (struct md_header *)md;
	elem = (struct md_element *)(md + sizeof(struct md_header));
	name_blk = md + sizeof(struct md_header) + hdr->node_blk_sz;
	data_blk = name_blk + hdr->name_blk_sz;

	while (elem[idx].tag != 'E') {
		str = name_blk + elem[idx].name_offset;
		if (elem[idx].tag == 's' && strcmp(str, name) == 0)
			return (data_blk + elem[idx].d.y.data_offset);
		idx++;
	}

	return (NULL);
}

int
sun4v_mdesc_find(caddr_t md, const char *name, uint64_t cfg_handle)
{
	struct md_header *hdr;
	struct md_element *elem;
	const char *str;
	uint64_t val;
	int idx;

	hdr = (struct md_header *)md;
	elem = (struct md_element *)(md + sizeof(struct md_header));

	for (idx = 0; elem[idx].tag == 'N'; idx = elem[idx].d.val) {
		str = sun4v_mdesc_get_prop_string(md, idx, "name");
		val = sun4v_mdesc_get_prop_val(md, idx, "cfg-handle");
		if (str && strcmp(str, name) == 0 && val == cfg_handle)
			return (idx);
	}

	return (-1);
}

int
sun4v_mdesc_find_child(caddr_t md, int idx, const char *name,
    uint64_t cfg_handle)
{
	struct md_header *hdr;
	struct md_element *elem;
	const char *name_blk;
	const char *str;
	uint64_t val;
	int arc;

	hdr = (struct md_header *)md;
	elem = (struct md_element *)(md + sizeof(struct md_header));
	name_blk = md + sizeof(struct md_header) + hdr->node_blk_sz;

	for (; elem[idx].tag != 'E'; idx++) {
		str = name_blk + elem[idx].name_offset;
		if (elem[idx].tag != 'a' || strcmp(str, "fwd") != 0)
			continue;

		arc = elem[idx].d.val;
		str = sun4v_mdesc_get_prop_string(md, arc, "name");
		val = sun4v_mdesc_get_prop_val(md, arc, "cfg-handle");
		if (str && strcmp(str, name) == 0 && val == cfg_handle)
			return (arc);
	}

	return (-1);
}
