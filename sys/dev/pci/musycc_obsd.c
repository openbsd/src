/*	$OpenBSD: musycc_obsd.c,v 1.9 2006/01/25 11:02:54 claudio Exp $ */

/*
 * Copyright (c) 2004,2005  Internet Business Solutions AG, Zurich, Switzerland
 * Written by: Claudio Jeker <jeker@accoom.net>
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
#include <sys/types.h>

#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/systm.h>
#include <sys/socket.h>

#include <machine/cpu.h>
#include <machine/bus.h>
#include <machine/intr.h>

#include <net/if.h>
#include <net/if_media.h>
#include <net/if_sppp.h>

#include <dev/pci/musyccvar.h>
#include <dev/pci/musyccreg.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>

int	musycc_match(struct device *, void *, void *);
void	musycc_softc_attach(struct device *, struct device *, void *);
void	musycc_ebus_attach(struct device *, struct musycc_softc *,
	    struct pci_attach_args *);
int	musycc_ebus_print(void *, const char *);

struct cfattach musycc_ca = {
	sizeof(struct musycc_softc), musycc_match, musycc_softc_attach
};

struct cfdriver musycc_cd = {
	NULL, "musycc", DV_DULL
};

SLIST_HEAD(, musycc_softc) msc_list = SLIST_HEAD_INITIALIZER(msc_list);

const struct pci_matchid musycc_pci_devices[] = {
	{ PCI_VENDOR_CONEXANT, PCI_PRODUCT_CONEXANT_MUSYCC8478 },
	{ PCI_VENDOR_CONEXANT, PCI_PRODUCT_CONEXANT_MUSYCC8474 },
	{ PCI_VENDOR_CONEXANT, PCI_PRODUCT_CONEXANT_MUSYCC8472 },
	{ PCI_VENDOR_CONEXANT, PCI_PRODUCT_CONEXANT_MUSYCC8471 }
};

int
musycc_match(struct device *parent, void *match, void *aux)
{
	return (pci_matchbyid((struct pci_attach_args *)aux, musycc_pci_devices,
	    sizeof(musycc_pci_devices)/sizeof(musycc_pci_devices[0])));
}

void
musycc_softc_attach(struct device *parent, struct device *self, void *aux)
{
	struct musycc_softc	*sc = (struct musycc_softc *)self;
	struct pci_attach_args	*pa = aux;
	pci_chipset_tag_t	 pc = pa->pa_pc;
	pci_intr_handle_t	 ih;
	const char		*intrstr = NULL;

	if (pci_mapreg_map(pa, MUSYCC_PCI_BAR,
	    PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_32BIT, 0,
	    &sc->mc_st, &sc->mc_sh, NULL, &sc->mc_iosize, 0)) {
		printf(": can't map mem space\n");
		return;
	}
	sc->mc_dmat = pa->pa_dmat;

	switch (PCI_PRODUCT(pa->pa_id)) {
	case PCI_PRODUCT_CONEXANT_MUSYCC8478:
		sc->mc_ngroups = 8;
		sc->mc_nports = 8;
		break;
	case PCI_PRODUCT_CONEXANT_MUSYCC8474:
		sc->mc_ngroups = 4;
		sc->mc_nports = 4;
		break;
	case PCI_PRODUCT_CONEXANT_MUSYCC8472:
		sc->mc_ngroups = 2;
		sc->mc_nports = 2;
		break;
	case PCI_PRODUCT_CONEXANT_MUSYCC8471:
		sc->mc_ngroups = 1;
		sc->mc_nports = 1;
		break;
	}

	if (pa->pa_function == 1)
		return (musycc_ebus_attach(parent, sc, pa));

	sc->bus = parent->dv_unit;
	sc->device = pa->pa_device;
	SLIST_INSERT_HEAD(&msc_list, sc, list);

	/*
	 * Allocate our interrupt.
	 */
	if (pci_intr_map(pa, &ih)) {
		printf(": couldn't map interrupt\n");
		bus_space_unmap(sc->mc_st, sc->mc_sh, sc->mc_iosize);
		return;
	}

	intrstr = pci_intr_string(pc, ih);
	sc->mc_ih = pci_intr_establish(pc, ih, IPL_NET, musycc_intr, sc,
	    self->dv_xname);
	if (sc->mc_ih == NULL) {
		printf(": couldn't establish interrupt");
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		bus_space_unmap(sc->mc_st, sc->mc_sh, sc->mc_iosize);
		return;
	}

	printf(": %s\n", intrstr);

	/* soft reset device */
	bus_space_write_4(sc->mc_st, sc->mc_sh, MUSYCC_SERREQ(0),
	    MUSYCC_SREQ_SET(1));
	bus_space_barrier(sc->mc_st, sc->mc_sh, MUSYCC_SERREQ(0),
	    sizeof(u_int32_t), BUS_SPACE_BARRIER_WRITE);

	/*
	 * preload global configuration: set EBUS to sane defaults
	 * so that the ROM access will work.
	 * intel mode, elapse = 3, blapse = 3, alapse = 3, disable INTB
	 */
	sc->mc_global_conf = MUSYCC_CONF_MPUSEL | MUSYCC_CONF_ECKEN |
	    MUSYCC_CONF_ELAPSE_SET(3) | MUSYCC_CONF_ALAPSE_SET(3) |
	    MUSYCC_CONF_BLAPSE_SET(3) | MUSYCC_CONF_INTB;

	/* Dual Address Cycle Base Pointer */
	bus_space_write_4(sc->mc_st, sc->mc_sh, MUSYCC_DACB_PTR, 0);
	/* Global Configuration Descriptor */
	bus_space_write_4(sc->mc_st, sc->mc_sh, MUSYCC_GLOBALCONF,
	    sc->mc_global_conf);

	return;
}

void
musycc_ebus_attach(struct device *parent, struct musycc_softc *esc,
    struct pci_attach_args *pa)
{
	struct ebus_dev			 rom;
	struct musycc_attach_args	 ma;
	struct musycc_softc		*sc;
	pci_chipset_tag_t		 pc = pa->pa_pc;
#if 0
	pci_intr_handle_t		 ih;
	const char			*intrstr = NULL;
#endif
	struct musycc_rom		 baseconf;
	struct musycc_rom_framer	 framerconf;
	bus_size_t			 offset;
	int				 i;

	/* find HDLC controller softc ... */
	SLIST_FOREACH(sc, &msc_list, list)
		if (sc->bus == parent->dv_unit && sc->device == pa->pa_device)
			break;
	if (sc == NULL) {
		printf(": corresponding hdlc controller not found\n");
		return;
	}

	/* ... and link them together */
	esc->mc_other = sc;
	sc->mc_other = esc;

#if 0
	/*
	 * Allocate our interrupt.
	 */
	if (pci_intr_map(pa, &ih)) {
		printf(": couldn't map interrupt\n");
		goto failed;
	}

	intrstr = pci_intr_string(pc, ih);
	esc->mc_ih = pci_intr_establish(pc, ih, IPL_NET, ebus_intr, esc,
	    esc->mc_dev.dv_xname);
	if (esc->mc_ih == NULL) {
		printf(": couldn't establish interrupt");
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		goto failed;
	}

	/* XXX this printf should actually move to the end of the function */
	printf(": %s\n", intrstr);
#endif

	if (ebus_attach_device(&rom, sc, 0, 0x400) != 0) {
		printf(": failed to map rom @ %05p\n", 0);
		goto failed;
	}

	offset = 0;
	ebus_read_buf(&rom, offset, &baseconf, sizeof(baseconf));
	offset += sizeof(baseconf);

	if (baseconf.magic != MUSYCC_ROM_MAGIC) {
		printf(": bad rom\n");
		goto failed;
	}

	/* Do generic parts of attach. */
	if (musycc_attach_common(sc, baseconf.portmap, baseconf.portmode))
		goto failed;

	/* map and reset leds */
	/* (15 * 0x4000) << 2 */
	esc->mc_ledbase = ntohl(baseconf.ledbase) << 2;
	esc->mc_ledmask = baseconf.ledmask;
	esc->mc_ledstate = 0;
	bus_space_write_1(esc->mc_st, esc->mc_sh, esc->mc_ledbase, 0);

	printf("\n");

	for (i = 0; i < baseconf.numframer; i++) {
		if (offset >= 0x400) {
			printf("%s: bad rom\n", sc->mc_dev.dv_xname);
			goto failed;
		}
		ebus_read_buf(&rom, offset, &framerconf, sizeof(framerconf));
		offset += sizeof(framerconf);

		strlcpy(ma.ma_product, baseconf.product, sizeof(ma.ma_product));
		ma.ma_base = ntohl(framerconf.base);
		ma.ma_size = ntohl(framerconf.size);
		ma.ma_type = ntohl(framerconf.type);
		ma.ma_gnum = framerconf.gnum;
		ma.ma_port = framerconf.port;
		ma.ma_flags = framerconf.flags;
		ma.ma_slot = framerconf.slot;

		(void)config_found(&sc->mc_dev, &ma, musycc_ebus_print);
	}

	return;
failed:
	/* Failed! */
	pci_intr_disestablish(pc, sc->mc_ih);
	if (esc->mc_ih != NULL)
		pci_intr_disestablish(pc, esc->mc_ih);
	bus_space_unmap(sc->mc_st, sc->mc_sh, sc->mc_iosize);
	bus_space_unmap(esc->mc_st, esc->mc_sh, esc->mc_iosize);
	return;
}

int
musycc_ebus_print(void *aux, const char *pnp)
{
	struct musycc_attach_args *ma = aux;

	if (pnp)
		printf("framer at %s port %d slot %c",
		    pnp, ma->ma_port, ma->ma_slot);
	else
		printf(" port %d slot %c", ma->ma_port, ma->ma_slot);
	return (UNCONF);
}

