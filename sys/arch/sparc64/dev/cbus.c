/*	$OpenBSD: cbus.c,v 1.8 2010/11/11 17:58:23 miod Exp $	*/
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
#include <machine/mdesc.h>
#include <machine/openfirm.h>

#include <sparc64/dev/cbusvar.h>
#include <sparc64/dev/vbusvar.h>

struct cbus_softc {
	struct device		sc_dv;
	bus_space_tag_t		sc_bustag;
	bus_dma_tag_t		sc_dmatag;

	/* Machine description. */
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

int	cbus_get_channel_endpoint(struct cbus_softc *,
	    struct cbus_attach_args *);

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

	printf("\n");

	sc->sc_idx = mdesc_find(va->va_name, va->va_reg[0]);
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

int
cbus_intr_setstate(uint64_t sysino, uint64_t state)
{
	uint64_t devhandle = INTIGN(sysino);
	uint64_t devino = INTINO(sysino);
	int err;

	err = hv_vintr_setstate(devhandle, devino, state);
	if (err != H_EOK)
		return (-1);

	return (0);
}

int
cbus_intr_setenabled(uint64_t sysino, uint64_t enabled)
{
	uint64_t devhandle = INTIGN(sysino);
	uint64_t devino = INTINO(sysino);
	int err;

	err = hv_vintr_setenabled(devhandle, devino, enabled);
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

	hv_vintr_setstate(devhandle, devino, INTR_IDLE);
}

bus_space_tag_t
cbus_alloc_bus_tag(struct cbus_softc *sc, bus_space_tag_t parent)
{
	struct sparc_bus_space_tag *bt;

	bt = malloc(sizeof(*bt), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (bt == NULL)
		panic("could not allocate cbus bus tag");

	strlcpy(bt->name, sc->sc_dv.dv_xname, sizeof(bt->name));
	bt->cookie = sc;
	bt->parent = parent;
	bt->asi = parent->asi;
	bt->sasi = parent->sasi;
	bt->sparc_bus_map = parent->sparc_bus_map;
	bt->sparc_intr_establish = cbus_intr_establish;

	return (bt);
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

	idx = mdesc_find_child(sc->sc_idx, ca->ca_name, ca->ca_reg[0]);
	if (idx == -1)
		return (ENOENT);

	hdr = (struct md_header *)mdesc;
	elem = (struct md_element *)(mdesc + sizeof(struct md_header));
	name_blk = mdesc + sizeof(struct md_header) + hdr->node_blk_sz;

	ca->ca_idx = idx;

	ca->ca_id = -1;
	ca->ca_tx_ino = -1;
	ca->ca_rx_ino = -1;

	if (strcmp(ca->ca_name, "disk") != 0 &&
	    strcmp(ca->ca_name, "network") != 0)
		return (0);

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
			ca->ca_id = mdesc_get_prop_val(arc, "id");
			ca->ca_tx_ino = mdesc_get_prop_val(arc, "tx-ino");
			ca->ca_rx_ino = mdesc_get_prop_val(arc, "rx-ino");
			return (0);
		}
	}

	return (0);
}
