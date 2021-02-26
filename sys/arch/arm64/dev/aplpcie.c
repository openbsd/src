/*	$OpenBSD: aplpcie.c,v 1.1 2021/02/26 11:09:23 kettenis Exp $	*/
/*
 * Copyright (c) 2021 Mark Kettenis <kettenis@openbsd.org>
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
#include <dev/ofw/ofw_misc.h>
#include <dev/ofw/fdt.h>

/*
 * This driver is based on preliminary device tree bindings and will
 * almost certainly need changes once the official bindings land in
 * mainline Linux.  Support for these preliminary bindings will be
 * dropped as soon as official bindings are available.
 *
 * The driver assumes that the hardware has been (almost) completely
 * initialized by U-Boot.  More code will be needed to support
 * alternate boot paths.
 */

#define PCIE_MSI_CTRL		0x0124
#define  PCIE_MSI_CTRL_ENABLE	(1 << 0)
#define  PCIE_MSI_CTRL_32	(5 << 4)
#define PCIE_MSI_REMAP		0x0128
#define PCIE_MSI_DOORBELL	0x0168

#define HREAD4(sc, reg)							\
	(bus_space_read_4((sc)->sc_iot, (sc)->sc_ioh, (reg)))
#define HWRITE4(sc, reg, val)						\
	bus_space_write_4((sc)->sc_iot, (sc)->sc_ioh, (reg), (val))

struct aplpcie_range {
	uint32_t		flags;
	uint64_t		pci_base;
	uint64_t		phys_base;
	uint64_t		size;
};

struct aplpcie_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	bus_space_handle_t	sc_port_ioh[3];
	bus_dma_tag_t		sc_dmat;

	int			sc_node;
	int			sc_acells;
	int			sc_scells;
	int			sc_pacells;
	int			sc_pscells;
	struct aplpcie_range	*sc_ranges;
	int			sc_nranges;

	struct bus_space	sc_bus_iot;
	struct bus_space	sc_bus_memt;
	
	struct arm64_pci_chipset sc_pc;
	struct extent		*sc_busex;
	struct extent		*sc_memex;
	struct extent		*sc_pmemex;
	struct extent		*sc_ioex;
	int			sc_bus;

	int			sc_msi;
	bus_addr_t		sc_msi_doorbell;
};

int	aplpcie_match(struct device *, void *, void *);
void	aplpcie_attach(struct device *, struct device *, void *);

struct cfattach	aplpcie_ca = {
	sizeof (struct aplpcie_softc), aplpcie_match, aplpcie_attach
};

struct cfdriver aplpcie_cd = {
	NULL, "aplpcie", DV_DULL
};

int
aplpcie_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return OF_is_compatible(faa->fa_node, "apple,pcie-m1");
}

void	aplpcie_attach_hook(struct device *, struct device *,
	    struct pcibus_attach_args *);
int	aplpcie_bus_maxdevs(void *, int);
pcitag_t aplpcie_make_tag(void *, int, int, int);
void	aplpcie_decompose_tag(void *, pcitag_t, int *, int *, int *);
int	aplpcie_conf_size(void *, pcitag_t);
pcireg_t aplpcie_conf_read(void *, pcitag_t, int);
void	aplpcie_conf_write(void *, pcitag_t, int, pcireg_t);
int	aplpcie_probe_device_hook(void *, struct pci_attach_args *);

int	aplpcie_intr_map(struct pci_attach_args *, pci_intr_handle_t *);
const char *aplpcie_intr_string(void *, pci_intr_handle_t);
void	*aplpcie_intr_establish(void *, pci_intr_handle_t, int,
	    struct cpu_info *, int (*)(void *), void *, char *);
void	aplpcie_intr_disestablish(void *, void *);

int	aplpcie_bs_iomap(bus_space_tag_t, bus_addr_t, bus_size_t, int,
	    bus_space_handle_t *);
int	aplpcie_bs_memmap(bus_space_tag_t, bus_addr_t, bus_size_t, int,
	    bus_space_handle_t *);

void
aplpcie_attach(struct device *parent, struct device *self, void *aux)
{
	struct aplpcie_softc *sc = (struct aplpcie_softc *)self;
	struct fdt_attach_args *faa = aux;
	struct pcibus_attach_args pba;
	uint32_t *ranges;
	int i, j, nranges, rangeslen;
	uint32_t bus_range[2];

	if (faa->fa_nreg < 6) {
		printf(": no registers\n");
		return;
	}

	sc->sc_iot = faa->fa_iot;
	if (bus_space_map(sc->sc_iot, faa->fa_reg[0].addr,
	    faa->fa_reg[0].size, 0, &sc->sc_ioh)) {
		printf(": can't map registers\n");
		return;
	}

	for (i = 3; i < 6; i++) {
		if (bus_space_map(sc->sc_iot, faa->fa_reg[i].addr,
		    faa->fa_reg[i].size, 0, &sc->sc_port_ioh[i - 3])) {
			printf(": can't map registers\n");
			return;
		}
	}

	sc->sc_dmat = faa->fa_dmat;
	sc->sc_node = faa->fa_node;
	printf("\n");

	sc->sc_msi_doorbell =
	    OF_getpropint64(sc->sc_node, "msi-doorbell", 0xffff000ULL);

	/*
	 * Set things up such that we can share the 32 available MSIs
	 * across all ports.
	 */
	for (i = 0; i < 3; i++) {
		bus_space_write_4(sc->sc_iot, sc->sc_port_ioh[i],
		    PCIE_MSI_CTRL, PCIE_MSI_CTRL_32 | PCIE_MSI_CTRL_ENABLE);
		bus_space_write_4(sc->sc_iot, sc->sc_port_ioh[i],
		    PCIE_MSI_REMAP, 0);
		bus_space_write_4(sc->sc_iot, sc->sc_port_ioh[i],
		    PCIE_MSI_DOORBELL, sc->sc_msi_doorbell);
	}
	sc->sc_msi = 0;

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
	    sizeof(struct aplpcie_range), M_TEMP, M_WAITOK);
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

	/* Create extents for our address spaces. */
	sc->sc_busex = extent_create("pcibus", 0, 255,
	    M_DEVBUF, NULL, 0, EX_WAITOK | EX_FILLED);
	sc->sc_memex = extent_create("pcimem", 0, (u_long)-1,
	    M_DEVBUF, NULL, 0, EX_WAITOK | EX_FILLED);
	sc->sc_pmemex = extent_create("pcipmem", 0, (u_long)-1,
	    M_DEVBUF, NULL, 0, EX_WAITOK | EX_FILLED);
	sc->sc_ioex = extent_create("pciio", 0, 0xffffffff,
	    M_DEVBUF, NULL, 0, EX_WAITOK | EX_FILLED);
	for (i = 0; i < sc->sc_nranges; i++) {
		if ((sc->sc_ranges[i].flags & 0x03000000) == 0x01000000) {
			extent_free(sc->sc_ioex, sc->sc_ranges[i].pci_base,
			    sc->sc_ranges[i].size, EX_WAITOK);
		}
		if ((sc->sc_ranges[i].flags & 0x03000000) == 0x02000000) {
			extent_free(sc->sc_memex, sc->sc_ranges[i].pci_base,
			    sc->sc_ranges[i].size, EX_WAITOK);
		}
		if ((sc->sc_ranges[i].flags & 0x03000000) == 0x03000000) {
			extent_free(sc->sc_pmemex, sc->sc_ranges[i].pci_base,
			    sc->sc_ranges[i].size, EX_WAITOK);
		}
	}

	/* Set up bus range. */
	if (OF_getpropintarray(sc->sc_node, "bus-range", bus_range,
	    sizeof(bus_range)) != sizeof(bus_range) ||
	    bus_range[0] >= 32 || bus_range[1] >= 32) {
		bus_range[0] = 0;
		bus_range[1] = 31;
	}
	sc->sc_bus = bus_range[0];
	extent_free(sc->sc_busex, bus_range[0],
	    bus_range[1] - bus_range[0] + 1, EX_WAITOK);

	memcpy(&sc->sc_bus_iot, sc->sc_iot, sizeof(sc->sc_bus_iot));
	sc->sc_bus_iot.bus_private = sc;
	sc->sc_bus_iot._space_map = aplpcie_bs_iomap;
	memcpy(&sc->sc_bus_memt, sc->sc_iot, sizeof(sc->sc_bus_memt));
	sc->sc_bus_memt.bus_private = sc;
	sc->sc_bus_memt._space_map = aplpcie_bs_memmap;

	sc->sc_pc.pc_conf_v = sc;
	sc->sc_pc.pc_attach_hook = aplpcie_attach_hook;
	sc->sc_pc.pc_bus_maxdevs = aplpcie_bus_maxdevs;
	sc->sc_pc.pc_make_tag = aplpcie_make_tag;
	sc->sc_pc.pc_decompose_tag = aplpcie_decompose_tag;
	sc->sc_pc.pc_conf_size = aplpcie_conf_size;
	sc->sc_pc.pc_conf_read = aplpcie_conf_read;
	sc->sc_pc.pc_conf_write = aplpcie_conf_write;
	sc->sc_pc.pc_probe_device_hook = aplpcie_probe_device_hook;

	sc->sc_pc.pc_intr_v = sc;
	sc->sc_pc.pc_intr_map = aplpcie_intr_map;
	sc->sc_pc.pc_intr_map_msi = _pci_intr_map_msi;
	sc->sc_pc.pc_intr_map_msix = _pci_intr_map_msix;
	sc->sc_pc.pc_intr_string = aplpcie_intr_string;
	sc->sc_pc.pc_intr_establish = aplpcie_intr_establish;
	sc->sc_pc.pc_intr_disestablish = aplpcie_intr_disestablish;

	memset(&pba, 0, sizeof(pba));
	pba.pba_busname = "pci";
	pba.pba_iot = &sc->sc_bus_iot;
	pba.pba_memt = &sc->sc_bus_memt;
	pba.pba_dmat = sc->sc_dmat;
	pba.pba_pc = &sc->sc_pc;
	pba.pba_busex = sc->sc_busex;
	pba.pba_memex = sc->sc_memex;
	pba.pba_pmemex = sc->sc_pmemex;
	pba.pba_ioex = sc->sc_ioex;
	pba.pba_domain = pci_ndomains++;
	pba.pba_bus = sc->sc_bus;
	pba.pba_flags |= PCI_FLAGS_MSI_ENABLED;

	config_found(self, &pba, NULL);
}

void
aplpcie_attach_hook(struct device *parent, struct device *self,
    struct pcibus_attach_args *pba)
{
}

int
aplpcie_bus_maxdevs(void *v, int bus)
{
	return 32;
}

pcitag_t
aplpcie_make_tag(void *v, int bus, int device, int function)
{
	/* Return ECAM address. */
	return ((bus << 20) | (device << 15) | (function << 12));
}

void
aplpcie_decompose_tag(void *v, pcitag_t tag, int *bp, int *dp, int *fp)
{
	if (bp != NULL)
		*bp = (tag >> 20) & 0xff;
	if (dp != NULL)
		*dp = (tag >> 15) & 0x1f;
	if (fp != NULL)
		*fp = (tag >> 12) & 0x7;
}

int
aplpcie_conf_size(void *v, pcitag_t tag)
{
	return PCIE_CONFIG_SPACE_SIZE;
}

pcireg_t
aplpcie_conf_read(void *v, pcitag_t tag, int reg)
{
	struct aplpcie_softc *sc = v;

	return HREAD4(sc, tag | reg);
}

void
aplpcie_conf_write(void *v, pcitag_t tag, int reg, pcireg_t data)
{
	struct aplpcie_softc *sc = v;

	HWRITE4(sc, tag | reg, data);
}

int
aplpcie_probe_device_hook(void *v, struct pci_attach_args *pa)
{
	struct aplpcie_softc *sc = v;
	uint16_t rid;

	rid = pci_requester_id(pa->pa_pc, pa->pa_tag);
	pa->pa_dmat = iommu_device_map_pci(sc->sc_node, rid, pa->pa_dmat);

	return 0;
}

int
aplpcie_intr_map(struct pci_attach_args *pa, pci_intr_handle_t *ihp)
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
aplpcie_intr_string(void *v, pci_intr_handle_t ih)
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
aplpcie_intr_establish(void *v, pci_intr_handle_t ih, int level,
    struct cpu_info *ci, int (*func)(void *), void *arg, char *name)
{
	struct aplpcie_softc *sc = v;
	void *cookie;

	KASSERT(ih.ih_type != PCI_NONE);

	if (ih.ih_type != PCI_INTX) {
		uint64_t addr, data;

		data = sc->sc_msi++;
		addr = sc->sc_msi_doorbell;
		cookie = fdt_intr_establish_idx_cpu(sc->sc_node, 3 + data,
		    level, ci, func, arg, (void *)name);
		if (cookie == NULL)
			return NULL;

		if (ih.ih_type == PCI_MSIX) {
			pci_msix_enable(ih.ih_pc, ih.ih_tag,
			    &sc->sc_bus_memt, ih.ih_intrpin, addr, data);
		} else
			pci_msi_enable(ih.ih_pc, ih.ih_tag, addr, data);
	} else {
		int bus, dev, fn;
		uint32_t reg[4];

		aplpcie_decompose_tag(sc, ih.ih_tag, &bus, &dev, &fn);

		reg[0] = bus << 16 | dev << 11 | fn << 8;
		reg[1] = reg[2] = 0;
		reg[3] = ih.ih_intrpin;

		cookie = fdt_intr_establish_imap_cpu(sc->sc_node, reg,
		    sizeof(reg), level, ci, func, arg, name);
	}

	return cookie;
}

void
aplpcie_intr_disestablish(void *v, void *cookie)
{
}

int
aplpcie_bs_iomap(bus_space_tag_t t, bus_addr_t addr, bus_size_t size,
    int flags, bus_space_handle_t *bshp)
{
	struct aplpcie_softc *sc = t->bus_private;
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
aplpcie_bs_memmap(bus_space_tag_t t, bus_addr_t addr, bus_size_t size,
    int flags, bus_space_handle_t *bshp)
{
	struct aplpcie_softc *sc = t->bus_private;
	int i;

	flags |= BUS_SPACE_MAP_POSTED;

	for (i = 0; i < sc->sc_nranges; i++) {
		uint64_t pci_start = sc->sc_ranges[i].pci_base;
		uint64_t pci_end = pci_start + sc->sc_ranges[i].size;
		uint64_t phys_start = sc->sc_ranges[i].phys_base;

		if ((sc->sc_ranges[i].flags & 0x02000000) == 0x02000000 &&
		    addr >= pci_start && addr + size <= pci_end) {
			return bus_space_map(sc->sc_iot,
			    addr - pci_start + phys_start, size, flags, bshp);
		}
	}
	
	return ENXIO;
}
