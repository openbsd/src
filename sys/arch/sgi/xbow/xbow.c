/*	$OpenBSD: xbow.c,v 1.31 2011/04/05 14:43:11 miod Exp $	*/

/*
 * Copyright (c) 2008, 2009 Miodrag Vallat.
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
 * Copyright (c) 2004 Opsycon AB  (www.opsycon.se)
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/*
 *  XBOW is the mux between two nodes and XIO.
 *
 *  A Crossbow (XBOW) connects two nodeboards via their respective
 *  HUB to up to six different I/O controllers in XIO slots. In a
 *  multiprocessor system all processors have access to the XIO
 *  slots but may need to pass traffic via the routers.
 *
 *  To each XIO port on the XBOW a XIO interface is attached. Such
 *  interfaces can be for example PCI bridges which then add another
 *  level to the hierarchy.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/malloc.h>
#include <sys/device.h>

#include <mips64/archtype.h>

#include <machine/atomic.h>
#include <machine/autoconf.h>
#include <machine/intr.h>
#include <machine/mnode.h>

#include <sgi/xbow/hub.h>
#include <sgi/xbow/widget.h>
#include <sgi/xbow/xbow.h>

#include <sgi/xbow/xbowdevs.h>
#include <sgi/xbow/xbowdevs_data.h>

int	xbowmatch(struct device *, void *, void *);
void	xbowattach(struct device *, struct device *, void *);
int	xbowprint(void *, const char *);
int	xbowsubmatch(struct device *, void *, void *);
int	xbow_attach_widget(struct device *, int16_t, int,
	    int (*)(struct device *, void *, void *), cfprint_t);

int	xbow_kl_search_brd(lboard_t *, void *);
int	xbow_kl_search_mplane(klinfo_t *, void *);

uint32_t xbow_read_4(bus_space_tag_t, bus_space_handle_t, bus_size_t);
uint64_t xbow_read_8(bus_space_tag_t, bus_space_handle_t, bus_size_t);
void	xbow_write_4(bus_space_tag_t, bus_space_handle_t, bus_size_t, uint32_t);
void	xbow_write_8(bus_space_tag_t, bus_space_handle_t, bus_size_t, uint64_t);
void	xbow_read_raw_4(bus_space_tag_t, bus_space_handle_t, bus_addr_t,
	    uint8_t *, bus_size_t);
void	xbow_write_raw_4(bus_space_tag_t, bus_space_handle_t, bus_addr_t,
	    const uint8_t *, bus_size_t);
void	xbow_read_raw_8(bus_space_tag_t, bus_space_handle_t, bus_addr_t,
	    uint8_t *, bus_size_t);
void	xbow_write_raw_8(bus_space_tag_t, bus_space_handle_t, bus_addr_t,
	    const uint8_t *, bus_size_t);

void	xbow_space_unmap(bus_space_tag_t, bus_space_handle_t, bus_size_t);
int	xbow_space_region(bus_space_tag_t, bus_space_handle_t, bus_size_t,
	    bus_size_t, bus_space_handle_t *);
void	*xbow_space_vaddr(bus_space_tag_t, bus_space_handle_t);

const struct xbow_product *xbow_identify(uint32_t, uint32_t);

struct	xbow_softc {
	struct device	sc_dev;
	int16_t		sc_nasid;
};

const struct cfattach xbow_ca = {
	sizeof(struct xbow_softc), xbowmatch, xbowattach
};

struct cfdriver xbow_cd = {
	NULL, "xbow", DV_DULL
};

static const bus_space_t xbowbus_tag = {
	(bus_addr_t)0,		/* will be modified in widgets bus_space_t */
	NULL,
	xbow_read_1,
	xbow_write_1,
	xbow_read_2,
	xbow_write_2,
	xbow_read_4,
	xbow_write_4,
	xbow_read_8,
	xbow_write_8,
	xbow_read_raw_2,
	xbow_write_raw_2,
	xbow_read_raw_4,
	xbow_write_raw_4,
	xbow_read_raw_8,
	xbow_write_raw_8,
	xbow_space_map,
	xbow_space_unmap,
	xbow_space_region,
	xbow_space_vaddr,
	NULL
};

/*
 * Function pointers to hide widget discovery and mapping differences accross
 * systems.
 */
paddr_t	(*xbow_widget_base)(int16_t, u_int);
paddr_t	(*xbow_widget_map)(int16_t, u_int, bus_addr_t *, bus_size_t *);

int	(*xbow_widget_id)(int16_t, u_int, uint32_t *);

/*
 * Attachment glue.
 */

int
xbowmatch(struct device *parent, void *match, void *aux)
{
	struct mainbus_attach_args *maa = aux;

	if (strcmp(maa->maa_name, xbow_cd.cd_name) != 0)
		return (0);

	switch (sys_config.system_type) {
	case SGI_IP27:
	case SGI_IP35:
	case SGI_OCTANE:
		return (1);
	default:
		return (0);
	}
}

const struct xbow_product *
xbow_identify(uint32_t vendor, uint32_t product)
{
	const struct xbow_product *p;

	for (p = xbow_products; p->productname != NULL; p++)
		if (p->vendor == vendor && p->product == product)
			return p;

	return NULL;
}

int
xbowprint(void *aux, const char *pnp)
{
	struct xbow_attach_args *xaa = aux;
	const struct xbow_product *p;

	p = xbow_identify(xaa->xaa_vendor, xaa->xaa_product);

	if (pnp != NULL) {
		if (p != NULL)
			printf("\"%s\"", p->productname);
		else
			printf("vendor %x product %x",
			    xaa->xaa_vendor, xaa->xaa_product);
		printf(" revision %d at %s",
		    xaa->xaa_revision, pnp);
	}
	printf(" widget %d", xaa->xaa_widget);
	if (pnp == NULL) {
		if (p != NULL)
			printf(": %s", p->productname);
	}

	return (UNCONF);
}

int
xbowsubmatch(struct device *parent, void *vcf, void *aux)
{
	struct xbow_attach_args *xaa = aux;
	struct cfdata *cf = vcf;

	if (cf->cf_loc[0] != -1 && cf->cf_loc[0] != xaa->xaa_widget)
		return 0;
	if (cf->cf_loc[1] != -1 && cf->cf_loc[1] != xaa->xaa_vendor)
		return 0;
	if (cf->cf_loc[2] != -1 && cf->cf_loc[2] != xaa->xaa_product)
		return 0;

	return (*cf->cf_attach->ca_match)(parent, vcf, aux);
}

/*
 * Widget probe order for various components
 */

/* Octane: probe Heart first, then onboard devices, then other slots */
const uint8_t xbow_probe_octane[] =
	{ 0x08, 0x0f, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0 };
/* Origin 200: probe onboard devices, and there is nothing more */
const uint8_t xbow_probe_singlebridge[] =
	{ 0x08, 0 };
/* Base I/O board: probe onboard devices, then other slots in ascending order */
const uint8_t xbow_probe_baseio[] =
	{ 0x0f, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0 };
/* I-Brick: probe PCI buses first (starting with the onboard devices) */
const uint8_t xbow_probe_ibrick[] =
	{ 0x0f, 0x0e, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0 };
/* P-Brick: all widgets are PCI buses, probe in recommended order */
const uint8_t xbow_probe_pbrick[] =
	{ 0x09, 0x08, 0x0f, 0x0e, 0x0c, 0x0d, 0x0a, 0x0b, 0 };
/* X-Brick: all widgets are XIO devices, probe in recommended order */
const uint8_t xbow_probe_xbrick[] =
	{ 0x08, 0x09, 0x0c, 0x0d, 0x0a, 0x0b, 0x0e, 0x0f, 0 };

/*
 * Structures used to carry information between KL and attachment code.
 */

struct xbow_config {
	int	valid;
	int	master;
	int	widgets[WIDGET_MAX + 1 - WIDGET_MIN];
};

struct xbow_kl_config {
	const uint8_t *probe_order;
	struct xbow_config *cfg;
};

void
xbowattach(struct device *parent, struct device *self, void *aux)
{
	struct xbow_softc *sc = (struct xbow_softc *)self;
	struct mainbus_attach_args *maa = aux;
	int16_t nasid = maa->maa_nasid;
	uint32_t wid, vendor, product;
	const struct xbow_product *p;
	struct xbow_config cfg;
	struct xbow_kl_config klcfg;
	uint widget;

	sc->sc_nasid = nasid;

	/*
	 * This assumes widget 0 is the XBow itself (or an XXBow).
	 * If it isn't - feel free to haunt my bedroom at night.
	 */
	if (xbow_widget_id(nasid, 0, &wid) != 0)
		panic("no xbow");
	vendor = WIDGET_ID_VENDOR(wid);
	product = WIDGET_ID_PRODUCT(wid);
	p = xbow_identify(vendor, product);
	if (p == NULL)
		printf(": unknown xbow (vendor %x product %x)",
		    vendor, product);
	else
		printf(": %s", p->productname);
	printf(" revision %d\n", WIDGET_ID_REV(wid));

	memset(&cfg, 0, sizeof cfg);
	switch (sys_config.system_type) {
	case SGI_OCTANE:
		klcfg.probe_order = xbow_probe_octane;
		break;
#ifdef TGT_ORIGIN
	default:
		klcfg.cfg = &cfg;
		klcfg.probe_order = NULL;

		/*
		 * If widget 0 reports itself as a bridge, this is not a
		 * complete XBow, but only a limited topology. This is
		 * found on at least the Origin 200.
		 */
		if (vendor == XBOW_VENDOR_SGI4 &&
		    product == XBOW_PRODUCT_SGI4_BRIDGE) {
			/*
			 * Interrupt widget is hardwired to #a (this is another
			 * facet of this bridge).
			 */
			xbow_node_hub_widget[nasid] = 0x0a;
			klcfg.probe_order = xbow_probe_singlebridge;
		} else {
			/*
			 * Get crossbow information from the KL configuration.
			 */
			kl_scan_node(nasid, KLBRD_ANY, xbow_kl_search_brd,
			    &klcfg);

			if (cfg.valid == 0)
				panic("no hub");

			/*
			 * This widget number is actually the Hub part of the
			 * crossbow, and is where memory and interrupt logic
			 * resources are connected to.
			 */
			xbow_node_hub_widget[nasid] = cfg.master;
		}
		break;
#endif
	}

	if (klcfg.probe_order == NULL)
		klcfg.probe_order = xbow_probe_baseio;
	for (; *klcfg.probe_order != 0; klcfg.probe_order++) {
		widget = *klcfg.probe_order;
		if (cfg.valid != 0 &&
		    !ISSET(cfg.widgets[widget - WIDGET_MIN], XBOW_PORT_ENABLE))
			continue;
		(void)xbow_attach_widget(self, nasid, widget, xbowsubmatch,
		    xbowprint);
	}
}

int
xbow_attach_widget(struct device *self, int16_t nasid, int widget,
    int (*sm)(struct device *, void *, void *), cfprint_t print)
{
	struct xbow_attach_args xaa;
	uint32_t wid;
	struct mips_bus_space bs;
	int rc;

	if ((rc = xbow_widget_id(nasid, widget, &wid)) != 0)
		return rc;

	/*
	 * Build a bus_space_t suitable for this widget.
	 */
	xbow_build_bus_space(&bs, nasid, widget);

	xaa.xaa_nasid = nasid;
	xaa.xaa_widget = widget;
	xaa.xaa_vendor = WIDGET_ID_VENDOR(wid);
	xaa.xaa_product = WIDGET_ID_PRODUCT(wid);
	xaa.xaa_revision = WIDGET_ID_REV(wid);
	xaa.xaa_iot = &bs;

	if (config_found_sm(self, &xaa, print, sm) == NULL)
		return ENOENT;

	return 0;
}

#ifdef TGT_ORIGIN

/*
 * These two functions try to figure out the configuration of the XBow
 * on this node.
 *
 * We are looking for two pieces of information:
 * - the Hub widget, to which memory is attached and interrupts are routed.
 * - what kind of Brick we are.
 *
 * The first information can be obtained easily by looking for a MPLANE type
 * board. However there is no easy way to figure the second part, except for
 * checking what kind of boards are reported.
 *
 * A BaseIO board will report itself once, as a single widget. Bricks, on the
 * other hand, appear for each of the widgets they provide.
 */

int
xbow_kl_search_brd(lboard_t *brd, void *arg)
{
	struct xbow_kl_config *cfg = arg;

	switch (brd->brd_type & IP27_BC_MASK) {
	case IP27_BC_MPLANE:
		if (cfg->cfg->valid == 0)
			kl_scan_board(brd, KLSTRUCT_XBOW, xbow_kl_search_mplane,
			    cfg->cfg);
		break;
	case IP27_BC_IO:
		if (cfg->probe_order == NULL)
			cfg->probe_order = xbow_probe_baseio;
		break;
	case IP27_BC_BRICK:
		if (cfg->probe_order == NULL)
			switch (brd->brd_type) {
			case IP27_BRD_IBRICK:
			case IP27_BRD_IXBRICK:
				cfg->probe_order = xbow_probe_ibrick;
				break;
			case IP27_BRD_PBRICK:
			case IP27_BRD_PXBRICK:
				cfg->probe_order = xbow_probe_pbrick;
				break;
			case IP27_BRD_XBRICK:
				cfg->probe_order = xbow_probe_xbrick;
				break;
			default:
				/* other brick */
				break;
			}
		break;
	}

	if (cfg->cfg->valid != 0 && cfg->probe_order != NULL)
		return 1;	/* stop enumeration */

	return 0;
}

int
xbow_kl_search_mplane(klinfo_t *c, void *arg)
{
	klxbow_t *xbow = (klxbow_t *)c;
	struct xbow_config *cfg = arg;
	uint w;

	cfg->valid = 1;
	cfg->master = xbow->xbow_hub_master_link;
	for (w = WIDGET_MIN; w <= WIDGET_MAX; w++)
		cfg->widgets[w - WIDGET_MIN] =
		    xbow->xbow_port_info[w - WIDGET_MIN].port_flag;

	return 1;
}

#endif

/*
 * Bus access primitives.
 */

void
xbow_build_bus_space(struct mips_bus_space *bs, int nasid, int widget)
{
	bcopy(&xbowbus_tag, bs, sizeof (*bs));
	bs->bus_base = (*xbow_widget_base)(nasid, widget);
}

uint8_t
xbow_read_1(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o)
{
	return *(volatile uint8_t *)(h + o);
}

uint16_t
xbow_read_2(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o)
{
	return *(volatile uint16_t *)(h + o);
}

uint32_t
xbow_read_4(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o)
{
	return *(volatile uint32_t *)(h + o);
}

uint64_t
xbow_read_8(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o)
{
	return *(volatile uint64_t *)(h + o);
}

void
xbow_write_1(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o, uint8_t v)
{
	*(volatile uint8_t *)(h + o) = v;
}

void
xbow_write_2(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o, uint16_t v)
{
	*(volatile uint16_t *)(h + o) = v;
}

void
xbow_write_4(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o, uint32_t v)
{
	*(volatile uint32_t *)(h + o) = v;
}

void
xbow_write_8(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o, uint64_t v)
{
	*(volatile uint64_t *)(h + o) = v;
}

void
xbow_read_raw_2(bus_space_tag_t t, bus_space_handle_t h, bus_addr_t o,
    uint8_t *buf, bus_size_t len)
{
	volatile uint16_t *addr = (volatile uint16_t *)(h + o);
	len >>= 1;
	while (len-- != 0) {
		*(uint16_t *)buf = *addr;
		buf += 2;
	}
}

void
xbow_write_raw_2(bus_space_tag_t t, bus_space_handle_t h, bus_addr_t o,
    const uint8_t *buf, bus_size_t len)
{
	volatile uint16_t *addr = (volatile uint16_t *)(h + o);
	len >>= 1;
	while (len-- != 0) {
		*addr = *(uint16_t *)buf;
		buf += 2;
	}
}

void
xbow_read_raw_4(bus_space_tag_t t, bus_space_handle_t h, bus_addr_t o,
    uint8_t *buf, bus_size_t len)
{
	volatile uint32_t *addr = (volatile uint32_t *)(h + o);
	len >>= 2;
	while (len-- != 0) {
		*(uint32_t *)buf = *addr;
		buf += 4;
	}
}

void
xbow_write_raw_4(bus_space_tag_t t, bus_space_handle_t h, bus_addr_t o,
    const uint8_t *buf, bus_size_t len)
{
	volatile uint32_t *addr = (volatile uint32_t *)(h + o);
	len >>= 2;
	while (len-- != 0) {
		*addr = *(uint32_t *)buf;
		buf += 4;
	}
}

void
xbow_read_raw_8(bus_space_tag_t t, bus_space_handle_t h, bus_addr_t o,
    uint8_t *buf, bus_size_t len)
{
	volatile uint64_t *addr = (volatile uint64_t *)(h + o);
	len >>= 3;
	while (len-- != 0) {
		*(uint64_t *)buf = *addr;
		buf += 8;
	}
}

void
xbow_write_raw_8(bus_space_tag_t t, bus_space_handle_t h, bus_addr_t o,
    const uint8_t *buf, bus_size_t len)
{
	volatile uint64_t *addr = (volatile uint64_t *)(h + o);
	len >>= 3;
	while (len-- != 0) {
		*addr = *(uint64_t *)buf;
		buf += 8;
	}
}

int
xbow_space_map(bus_space_tag_t t, bus_addr_t offs, bus_size_t size,
    int flags, bus_space_handle_t *bshp)
{
	bus_addr_t bpa;

	bpa = t->bus_base + offs;

#ifdef DIAGNOSTIC
	/* check that this does not overflow the window */
	if (((bpa + size - 1) >> 24) != (t->bus_base >> 24))
		return (EINVAL);
#endif

	*bshp = bpa;
	return 0;
}

void
xbow_space_unmap(bus_space_tag_t t, bus_space_handle_t bsh, bus_size_t size)
{
}

int
xbow_space_region(bus_space_tag_t t, bus_space_handle_t bsh,
    bus_size_t offset, bus_size_t size, bus_space_handle_t *nbshp)
{
#ifdef DIAGNOSTIC
	bus_addr_t bpa;

	bpa = (bus_addr_t)bsh - t->bus_base;
	/* check that this does not overflow the window */
	if (((bpa + offset) >> 24) != (t->bus_base >> 24))
		return (EINVAL);
#endif

	*nbshp = bsh + offset;
	return 0;
}

void *
xbow_space_vaddr(bus_space_tag_t t, bus_space_handle_t h)
{
	return (void *)h;
}

/*
 * Interrupt handling code.
 *
 * Interrupt handling should be done at the Heart/Hub driver level, we only
 * act as a proxy here.
 *
 * Note that, for the time being, interrupt handling is implicitly done at
 * the master nasid; other nodes do not handle interrupts.
 */

int	xbow_node_hub_widget[XBOW_MAX];
uint64_t xbow_intr_address;

int	(*xbow_intr_widget_intr_register)(int, int, int *) = NULL;
int	(*xbow_intr_widget_intr_establish)(int (*)(void *), void *, int, int,
	    const char *, struct intrhand *) = NULL;
void	(*xbow_intr_widget_intr_disestablish)(int) = NULL;
void	(*xbow_intr_widget_intr_set)(int) = NULL;
void	(*xbow_intr_widget_intr_clear)(int) = NULL;

int
xbow_intr_register(int widget, int level, int *intrbit)
{
	if (xbow_intr_widget_intr_register == NULL)
		return EINVAL;

	return (*xbow_intr_widget_intr_register)(widget, level, intrbit);
}

int
xbow_intr_establish(int (*func)(void *), void *arg, int intrbit, int level,
    const char *name, struct intrhand *ihstore)
{
	if (xbow_intr_widget_intr_establish == NULL)
		return EINVAL;

	return (*xbow_intr_widget_intr_establish)(func, arg, intrbit, level,
	    name, ihstore);
}

void
xbow_intr_disestablish(int intrbit)
{
	if (xbow_intr_widget_intr_disestablish == NULL)
		return;

	(*xbow_intr_widget_intr_disestablish)(intrbit);
}

void
xbow_intr_clear(int intrbit)
{
	if (xbow_intr_widget_intr_clear == NULL)
		return;

	(*xbow_intr_widget_intr_clear)(intrbit);
}

void
xbow_intr_set(int intrbit)
{
	if (xbow_intr_widget_intr_set == NULL)
		return;

	(*xbow_intr_widget_intr_set)(intrbit);
}

/*
 * Widget mapping code.
 */

paddr_t
xbow_widget_map_space(struct device *dev, u_int widget, bus_addr_t *offs,
    bus_size_t *len)
{
	struct xbow_softc *sc = (struct xbow_softc *)dev;

	if (xbow_widget_map == NULL)
		return 0UL;

	return (*xbow_widget_map)(sc->sc_nasid, widget, offs, len);
}
