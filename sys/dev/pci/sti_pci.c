/*	$OpenBSD: sti_pci.c,v 1.2 2007/01/11 21:58:05 miod Exp $	*/
/*
 * Copyright (c) 2006 Miodrag Vallat.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice, this permission notice, and the disclaimer below
 * appear in all copies.
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

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/ic/stireg.h>
#include <dev/ic/stivar.h>

int	sti_pci_match(struct device *, void *, void *);
void	sti_pci_attach(struct device *, struct device *, void *);

struct cfattach sti_pci_ca = {
	sizeof(struct sti_softc), sti_pci_match, sti_pci_attach
};

const struct pci_matchid sti_pci_devices[] = {
	{ PCI_VENDOR_HP, PCI_PRODUCT_HP_VISUALIZE_EG },
	{ PCI_VENDOR_HP, PCI_PRODUCT_HP_VISUALIZE_FX2 },
	{ PCI_VENDOR_HP, PCI_PRODUCT_HP_VISUALIZE_FX4 },
	{ PCI_VENDOR_HP, PCI_PRODUCT_HP_VISUALIZE_FX6 },
	{ PCI_VENDOR_HP, PCI_PRODUCT_HP_VISUALIZE_FXE },
};

int	sti_readbar(struct sti_softc *, struct pci_attach_args *, u_int, int);
int	sti_maprom(struct sti_softc *, struct pci_attach_args *);

int
sti_pci_match(struct device *parent, void *cf, void *aux)
{
	struct pci_attach_args *paa = aux;

	return (pci_matchbyid(paa, sti_pci_devices,
	    sizeof(sti_pci_devices) / sizeof(sti_pci_devices[0])));
}

void
sti_pci_attach(struct device *parent, struct device *self, void *aux)
{
	struct sti_softc *sc = (void *)self;
	struct pci_attach_args *paa = aux;

	printf("\n");

	if (sti_maprom(sc, paa) != 0)
		return;

	printf("%s", self->dv_xname);
	if (sti_attach_common(&spc->sc_base, STI_CODEBASE_MAIN) == 0) {
		startuphook_establish(sti_end_attach, spc);
	}
}

/*
 * Map the STI ROM image.
 */
int
sti_maprom(struct sti_softc *sc, struct pci_attach_args *pa)
{
	pcireg_t address, mask;
	bus_space_handle_t romh;
	bus_size_t romsize, stiromsize;
	bus_addr_t offs;
	u_int32_t tmp;
	int i;
	int rc;

	address = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_ROM_REG);
	pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_ROM_REG, 0xfffffffe);
	mask = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_ROM_REG);
	address |= PCI_ROM_ENABLE;
	pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_ROM_REG, address);

	/*
	 * Map the complete ROM for now.
	 */

	romsize = PCI_ROM_SIZE(mask);
	rc = bus_space_map(pa->pa_memt, PCI_ROM_ADDR(address), romsize,
	    0, &romh);
	if (rc != 0) {
		printf("%s: can't map PCI ROM (%d)\n",
		    sc->sc_dev.dv_xname, rc);
		goto fail2;
	}

	/*
	 * Check for a valid ROM header.
	 */

	tmp = bus_space_read_4(pa->pa_memt, romh, 0);
	tmp = letoh32(tmp);
	if (tmp != 0x55aa0000) {
		printf("%s: invalid PCI ROM header signature (%08x)\n",
		    sc->sc_dev.dv_xname, tmp);
		rc = EINVAL;
		goto fail;
	}

	tmp = bus_space_read_4(pa->pa_memt, romh, 4);
	tmp = letoh32(tmp);
	if (tmp != 0x00000001) {	/* 1 == STI ROM */
		printf("%s: invalid PCI ROM type (%08x)\n",
		    sc->sc_dev.dv_xname, tmp);
		rc = EINVAL;
		goto fail;
	}

	/*
	 * Read the STI region BAR assignments.
	 */

	offs = (bus_addr_t)bus_space_read_2(pa->pa_memt, romh, 0x0e);
	for (i = 0; i < STI_REGION_MAX; i++) {
		rc = sti_readbar(sc, pa, i,
		    bus_space_read_1(pa->pa_memt, romh, offs + i));
		if (rc != 0)
			goto fail;
	}

	/*
	 * Check for a valid ROM data structure.
	 * We do not need it except to know what architecture the ROM
	 * code is for.
	 */

	offs = (bus_addr_t)bus_space_read_2(pa->pa_memt, romh, 0x18);
	tmp = bus_space_read_4(pa->pa_memt, romh, offs + 0);
	tmp = letoh32(tmp);
	if (tmp != 0x50434952) {	/* PCIR */
		printf("%s: invalid PCI data signature (%08x)\n",
		    sc->sc_dev.dv_xname, tmp);
		rc = EINVAL;
		goto fail;
	}

	tmp = bus_space_read_1(pa->pa_memt, romh, offs + 0x14);
	switch (tmp) {
#ifdef __hppa__
	case 0x10:
		break;
#endif
#ifdef __i386__
	case 0x00:
		break;
#endif
	default:
		printf("%s: wrong microcode architecture (%02x)\n",
		    sc->sc_dev.dv_xname, tmp);
		rc = ENOEXEC;
		goto fail;
	}

	/*
	 * Find out where the STI ROM itself lies, and its size.
	 */

	offs = (bus_addr_t)bus_space_read_4(pa->pa_memt, romh, 0x08);
	stiromsize = (bus_addr_t)bus_space_read_4(pa->pa_memt, romh,
	    offs + 0x18);
	stiromsize = letoh32(stiromsize);

	/*
	 * Replace our mapping with a smaller mapping of only the area
	 * we are interested in.
	 */

	bus_space_unmap(pa->pa_memt, romh, romsize);
	rc = bus_space_map(pa->pa_memt, PCI_ROM_ADDR(address) + offs,
	    stiromsize, 0, &sc->romh);
	if (rc != 0) {
		printf("%s: can't map STI ROM (%d)\n",
		    sc->sc_dev.dv_xname, rc);
		goto fail2;
	}
	sc->memt = pa->pa_memt;

	return (0);

fail:
	bus_space_unmap(pa->pa_memt, romh, romsize);
fail2:
	address = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_ROM_REG);
	address &= ~PCI_ROM_ENABLE;
	pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_ROM_REG, address);

	return (rc);
}

/*
 * Decode a BAR register.
 */
int
sti_readbar(struct sti_softc *sc, struct pci_attach_args *pa, u_int region,
    int bar)
{
	bus_addr_t addr;
	bus_size_t size;
	u_int32_t cf;
	int rc;

	if (bar == 0) {
		sc->bases[region] = 0;
		return (0);
	}

#ifdef DIAGNOSTIC
	if (bar < PCI_MAPREG_START || bar > PCI_MAPREG_PPB_END)
		printf("%s: unexpected bar %02x for region %d\n", bar, region);
#endif

	cf = pci_conf_read(pa->pa_pc, pa->pa_tag, bar);

	if (PCI_MAPREG_TYPE(cf) == PCI_MAPREG_TYPE_IO)
		rc = pci_io_find(pa->pa_pc, pa->pa_tag, bar, &addr, &size);
	else
		rc = pci_mem_find(pa->pa_pc, pa->pa_tag, bar, &addr, &size,
		    NULL);

	if (rc != 0) {
		printf("%s: invalid bar %02x for region %d\n", bar, region);
		return (rc);
	}

	sc->bases[region] = addr;
	return (0);
}
