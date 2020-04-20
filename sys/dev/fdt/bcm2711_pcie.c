/*	$OpenBSD: bcm2711_pcie.c,v 1.2 2020/04/20 16:01:39 kettenis Exp $	*/
/*
 * Copyright (c) 2020 Mark Kettenis <kettenis@openbsd.org>
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
#include <sys/extent.h>
#include <sys/malloc.h>

#include <machine/intr.h>
#include <machine/bus.h>
#include <machine/fdt.h>

#include <dev/pci/pcidevs.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/fdt.h>

#define PCIE_EXT_CFG_DATA	0x8000
#define PCIE_EXT_CFG_INDEX	0x9000

#define HREAD4(sc, reg)							\
	(bus_space_read_4((sc)->sc_iot, (sc)->sc_ioh, (reg)))
#define HWRITE4(sc, reg, val)						\
	bus_space_write_4((sc)->sc_iot, (sc)->sc_ioh, (reg), (val))

struct bcmpcie_range {
	uint32_t		flags;
	uint64_t		pci_base;
	uint64_t		phys_base;
	uint64_t		size;
};

struct bcmpcie_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;

	int			sc_node;
	int			sc_acells;
	int			sc_scells;
	int			sc_pacells;
	int			sc_pscells;
	struct bcmpcie_range	*sc_ranges;
	int			sc_nranges;

	struct bus_space	sc_bus_iot;
	struct bus_space	sc_bus_memt;

	struct arm64_pci_chipset sc_pc;
	int			sc_bus;
};

int bcmpcie_match(struct device *, void *, void *);
void bcmpcie_attach(struct device *, struct device *, void *);

struct cfattach bcmpcie_ca = {
	sizeof (struct bcmpcie_softc), bcmpcie_match, bcmpcie_attach
};

struct cfdriver bcmpcie_cd = {
	NULL, "bcmpcie", DV_DULL
};

int
bcmpcie_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return OF_is_compatible(faa->fa_node, "brcm,bcm2711-pcie");
}

void	bcmpcie_attach_hook(struct device *, struct device *,
	    struct pcibus_attach_args *);
int	bcmpcie_bus_maxdevs(void *, int);
pcitag_t bcmpcie_make_tag(void *, int, int, int);
void	bcmpcie_decompose_tag(void *, pcitag_t, int *, int *, int *);
int	bcmpcie_conf_size(void *, pcitag_t);
pcireg_t bcmpcie_conf_read(void *, pcitag_t, int);
void	bcmpcie_conf_write(void *, pcitag_t, int, pcireg_t);

int	bcmpcie_intr_map(struct pci_attach_args *, pci_intr_handle_t *);
const char *bcmpcie_intr_string(void *, pci_intr_handle_t);
void	*bcmpcie_intr_establish(void *, pci_intr_handle_t, int,
	    int (*)(void *), void *, char *);
void	bcmpcie_intr_disestablish(void *, void *);

int	bcmpcie_bs_iomap(bus_space_tag_t, bus_addr_t, bus_size_t, int,
	    bus_space_handle_t *);
int	bcmpcie_bs_memmap(bus_space_tag_t, bus_addr_t, bus_size_t, int,
	    bus_space_handle_t *);

void
bcmpcie_attach(struct device *parent, struct device *self, void *aux)
{
	struct bcmpcie_softc *sc = (struct bcmpcie_softc *)self;
	struct fdt_attach_args *faa = aux;
	struct pcibus_attach_args pba;
	uint32_t *ranges;
	int i, j, nranges, rangeslen;

	if (faa->fa_nreg < 1) {
		printf(": no registers\n");
		return;
	}

	sc->sc_iot = faa->fa_iot;
	if (bus_space_map(sc->sc_iot, faa->fa_reg[0].addr,
	    faa->fa_reg[0].size, 0, &sc->sc_ioh)) {
		printf(": can't map registers\n");
		return;
	}

	sc->sc_node = faa->fa_node;

	sc->sc_acells = OF_getpropint(sc->sc_node, "#address-cells",
	    faa->fa_acells);
	sc->sc_scells = OF_getpropint(sc->sc_node, "#size-cells",
	    faa->fa_scells);
	sc->sc_pacells = faa->fa_acells;
	sc->sc_pscells = faa->fa_scells;

	rangeslen = OF_getproplen(sc->sc_node, "ranges");
	if (rangeslen <= 0 || (rangeslen % sizeof(uint32_t)) ||
	     (rangeslen / sizeof(uint32_t)) % (sc->sc_acells +
	     sc->sc_pacells + sc->sc_scells)) {
		printf(": invalid ranges property\n");
		return;
	}

	ranges = malloc(rangeslen, M_TEMP, M_WAITOK);
	OF_getpropintarray(sc->sc_node, "ranges", ranges,
	    rangeslen);

	nranges = (rangeslen / sizeof(uint32_t)) /
	    (sc->sc_acells + sc->sc_pacells + sc->sc_scells);
	sc->sc_ranges = mallocarray(nranges,
	    sizeof(struct bcmpcie_range), M_TEMP, M_WAITOK);
	sc->sc_nranges = nranges;

	for (i = 0, j = 0; i < sc->sc_nranges; i++) {
		sc->sc_ranges[i].flags = ranges[j++];
		sc->sc_ranges[i].pci_base = ranges[j++];
		if (sc->sc_acells - 1 == 2) {
			sc->sc_ranges[i].pci_base <<= 32;
			sc->sc_ranges[i].pci_base |= ranges[j++];
		}
		sc->sc_ranges[i].phys_base = ranges[j++];
		if (sc->sc_pacells == 2) {
			sc->sc_ranges[i].phys_base <<= 32;
			sc->sc_ranges[i].phys_base |= ranges[j++];
		}
		sc->sc_ranges[i].size = ranges[j++];
		if (sc->sc_scells == 2) {
			sc->sc_ranges[i].size <<= 32;
			sc->sc_ranges[i].size |= ranges[j++];
		}
	}

	free(ranges, M_TEMP, rangeslen);

	printf("\n");

	memcpy(&sc->sc_bus_iot, sc->sc_iot, sizeof(sc->sc_bus_iot));
	sc->sc_bus_iot.bus_private = sc;
	sc->sc_bus_iot._space_map = bcmpcie_bs_iomap;
	memcpy(&sc->sc_bus_memt, sc->sc_iot, sizeof(sc->sc_bus_memt));
	sc->sc_bus_memt.bus_private = sc;
	sc->sc_bus_memt._space_map = bcmpcie_bs_memmap;

	sc->sc_pc.pc_conf_v = sc;
	sc->sc_pc.pc_attach_hook = bcmpcie_attach_hook;
	sc->sc_pc.pc_bus_maxdevs = bcmpcie_bus_maxdevs;
	sc->sc_pc.pc_make_tag = bcmpcie_make_tag;
	sc->sc_pc.pc_decompose_tag = bcmpcie_decompose_tag;
	sc->sc_pc.pc_conf_size = bcmpcie_conf_size;
	sc->sc_pc.pc_conf_read = bcmpcie_conf_read;
	sc->sc_pc.pc_conf_write = bcmpcie_conf_write;

	sc->sc_pc.pc_intr_v = sc;
	sc->sc_pc.pc_intr_map = bcmpcie_intr_map;
	sc->sc_pc.pc_intr_map_msi = _pci_intr_map_msi;
	sc->sc_pc.pc_intr_map_msix = _pci_intr_map_msix;
	sc->sc_pc.pc_intr_string = bcmpcie_intr_string;
	sc->sc_pc.pc_intr_establish = bcmpcie_intr_establish;
	sc->sc_pc.pc_intr_disestablish = bcmpcie_intr_disestablish;

	memset(&pba, 0, sizeof(pba));
	pba.pba_busname = "pci";
	pba.pba_iot = &sc->sc_bus_iot;
	pba.pba_memt = &sc->sc_bus_memt;
	pba.pba_dmat = faa->fa_dmat;
	pba.pba_pc = &sc->sc_pc;
	pba.pba_domain = pci_ndomains++;
	pba.pba_bus = 0;

	config_found(self, &pba, NULL);
}

void
bcmpcie_attach_hook(struct device *parent, struct device *self,
    struct pcibus_attach_args *pba)
{
}

int
bcmpcie_bus_maxdevs(void *v, int bus)
{
	struct bcmpcie_softc *sc = v;

	if (bus == sc->sc_bus || bus == sc->sc_bus + 1)
		return 1;
	return 32;
}

pcitag_t
bcmpcie_make_tag(void *v, int bus, int device, int function)
{
	/* Return ECAM address. */
	return ((bus << 20) | (device << 15) | (function << 12));
}

void
bcmpcie_decompose_tag(void *v, pcitag_t tag, int *bp, int *dp, int *fp)
{
	if (bp != NULL)
		*bp = (tag >> 20) & 0xff;
	if (dp != NULL)
		*dp = (tag >> 15) & 0x1f;
	if (fp != NULL)
		*fp = (tag >> 12) & 0x7;
}

int
bcmpcie_conf_size(void *v, pcitag_t tag)
{
	return PCIE_CONFIG_SPACE_SIZE;
}

pcireg_t
bcmpcie_conf_read(void *v, pcitag_t tag, int reg)
{
	struct bcmpcie_softc *sc = v;
	int bus, dev, fn;

	bcmpcie_decompose_tag(sc, tag, &bus, &dev, &fn);
	if (bus == 0) {
		KASSERT(dev == 0);
		return HREAD4(sc, tag | reg);
	}

	HWRITE4(sc, PCIE_EXT_CFG_INDEX, tag);
	return HREAD4(sc, PCIE_EXT_CFG_DATA + reg);
}

void
bcmpcie_conf_write(void *v, pcitag_t tag, int reg, pcireg_t data)
{
	struct bcmpcie_softc *sc = v;
	int bus, dev, fn;

	bcmpcie_decompose_tag(sc, tag, &bus, &dev, &fn);
	if (bus == 0) {
		KASSERT(dev == 0);
		HWRITE4(sc, tag | reg, data);
		return;
	}

	HWRITE4(sc, PCIE_EXT_CFG_INDEX, tag);
	HWRITE4(sc, PCIE_EXT_CFG_DATA + reg, data);
}

int
bcmpcie_intr_map(struct pci_attach_args *pa, pci_intr_handle_t *ihp)
{
	int pin = pa->pa_rawintrpin;

	if (pin == 0 || pin > PCI_INTERRUPT_PIN_MAX)
		return -1;

	if (pa->pa_tag == 0)
		return -1;

	ihp->ih_pc = pa->pa_pc;
	ihp->ih_tag = pa->pa_intrtag;
	ihp->ih_intrpin = pa->pa_intrpin;
	ihp->ih_type = PCI_INTX;

	return 0;
}

const char *
bcmpcie_intr_string(void *v, pci_intr_handle_t ih)
{
	switch (ih.ih_type) {
	case PCI_MSI:
		return "msi";
	case PCI_MSIX:
		return "msix";
	}

	return "intx";
}

void *
bcmpcie_intr_establish(void *v, pci_intr_handle_t ih, int level,
    int (*func)(void *), void *arg, char *name)
{
	struct bcmpcie_softc *sc = v;
	int bus, dev, fn;
	uint32_t reg[4];

	KASSERT(ih.ih_type == PCI_INTX);
	bcmpcie_decompose_tag(sc, ih.ih_tag, &bus, &dev, &fn);

	reg[0] = bus << 16 | dev << 11 | fn << 8;
	reg[1] = reg[2] = 0;
	reg[3] = ih.ih_intrpin;

	return fdt_intr_establish_imap(sc->sc_node, reg, sizeof(reg),
	    level, func, arg, name);
}

void
bcmpcie_intr_disestablish(void *v, void *cookie)
{
}

int
bcmpcie_bs_iomap(bus_space_tag_t t, bus_addr_t addr, bus_size_t size,
    int flags, bus_space_handle_t *bshp)
{
	struct bcmpcie_softc *sc = t->bus_private;
	int i;

	for (i = 0; i < sc->sc_nranges; i++) {
		uint64_t pci_start = sc->sc_ranges[i].pci_base;
		uint64_t pci_end = pci_start + sc->sc_ranges[i].size;
		uint64_t phys_start = sc->sc_ranges[i].phys_base;

		if ((sc->sc_ranges[i].flags & 0x03000000) == 0x01000000 &&
		    addr >= pci_start && addr + size <= pci_end) {
			return bus_space_map(sc->sc_iot,
			    addr - pci_start + phys_start, size, flags, bshp);
		}
	}
	
	return ENXIO;
}

int
bcmpcie_bs_memmap(bus_space_tag_t t, bus_addr_t addr, bus_size_t size,
    int flags, bus_space_handle_t *bshp)
{
	struct bcmpcie_softc *sc = t->bus_private;
	int i;

	for (i = 0; i < sc->sc_nranges; i++) {
		uint64_t pci_start = sc->sc_ranges[i].pci_base;
		uint64_t pci_end = pci_start + sc->sc_ranges[i].size;
		uint64_t phys_start = sc->sc_ranges[i].phys_base;

		if ((sc->sc_ranges[i].flags & 0x03000000) == 0x02000000 &&
		    addr >= pci_start && addr + size <= pci_end) {
			return bus_space_map(sc->sc_iot,
			    addr - pci_start + phys_start, size, flags, bshp);
		}
	}
	
	return ENXIO;
}
