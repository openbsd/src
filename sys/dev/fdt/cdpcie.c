/*	$OpenBSD: cdpcie.c,v 1.1 2025/12/07 23:02:25 kettenis Exp $	*/
/*
 * Copyright (c) 2025 Mark Kettenis <kettenis@openbsd.org>
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

#include <machine/intr.h>
#include <machine/bus.h>
#include <machine/fdt.h>

#include <dev/pci/pcidevs.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/ppbreg.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/fdt.h>

#define PCIE_LM_BASE			0x100000
#define PCIE_LM_ID			(PCIE_LM_BASE + 0x044)
#define PCIE_LM_RC_BAR_CFG		(PCIE_LM_BASE + 0x300)
#define PCIE_LM_RC_BAR_CFG_PMEM_ENABLE	(1 << 17)
#define PCIE_LM_RC_BAR_CFG_PMEM_64BITS	(1 << 18)
#define PCIE_LM_RC_BAR_CFG_IO_ENABLE	(1 << 19)
#define PCIE_LM_RC_BAR_CFG_IO_32BITS	(1 << 20)

#define PCIE_RP_BASE			0x200000

#define PCIE_AT_BASE			0x400000
#define PCIE_AT_OB_ADDR0(i)		(PCIE_AT_BASE + 0x000 + (i) * 0x20)
#define  PCIE_AT_OB_ADDR0_NBITS(n)	((n) - 1)
#define PCIE_AT_OB_ADDR1(i)		(PCIE_AT_BASE + 0x004 + (i) * 0x20)
#define PCIE_AT_OB_DESC0(i)		(PCIE_AT_BASE + 0x008 + (i) * 0x20)
#define PCIE_AT_OB_DESC1(i)		(PCIE_AT_BASE + 0x00c + (i) * 0x20)
#define PCIE_AT_OB_CPU_ADDR0(i)		(PCIE_AT_BASE + 0x018 + (i) * 0x20)
#define PCIE_AT_OB_CPU_ADDR1(i)		(PCIE_AT_BASE + 0x01c + (i) * 0x20)
#define PCIE_AT_IB_ADDR0(i)		(PCIE_AT_BASE + 0x800 + (i) * 0x8)
#define PCIE_AT_IB_ADDR1(i)		(PCIE_AT_BASE + 0x804 + (i) * 0x8)
#define  PCIE_AT_HDR_MEM		0x2
#define  PCIE_AT_HDR_IO			0x6
#define  PCIE_AT_HDR_CFG_TYPE0		0xa
#define  PCIE_AT_HDR_CFG_TYPE1		0xb
#define  PCIE_AT_HDR_RID		(1 << 23)

#define HREAD4(sc, reg)							\
	(bus_space_read_4((sc)->sc_iot, (sc)->sc_ioh, (reg)))
#define HWRITE4(sc, reg, val)						\
	bus_space_write_4((sc)->sc_iot, (sc)->sc_ioh, (reg), (val))

struct cdpcie_range {
	uint32_t		flags;
	uint64_t		pci_base;
	uint64_t		phys_base;
	uint64_t		size;
};

struct cdpcie_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	bus_space_handle_t	sc_cfg_ioh;

	int			sc_node;
	struct cdpcie_range	sc_ranges[3];
	int			sc_nranges;

	struct bus_space	sc_bus_iot;
	struct bus_space	sc_bus_memt;

	struct machine_pci_chipset sc_pc;
	struct extent		*sc_busex;
	struct extent		*sc_memex;
	struct extent		*sc_pmemex;
	struct extent		*sc_ioex;
	int			sc_bus;
};

int	cdpcie_match(struct device *, void *, void *);
void	cdpcie_attach(struct device *, struct device *, void *);

const struct cfattach cdpcie_ca = {
	sizeof(struct cdpcie_softc), cdpcie_match, cdpcie_attach
};

struct cfdriver cdpcie_cd = {
	NULL, "cdpcie", DV_DULL
};

int
cdpcie_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return OF_is_compatible(faa->fa_node, "sophgo,sg2042-pcie-host");
}

void	cdpcie_at_init(struct cdpcie_softc *);

void	cdpcie_attach_hook(struct device *, struct device *,
	    struct pcibus_attach_args *);
int	cdpcie_bus_maxdevs(void *, int);
pcitag_t cdpcie_make_tag(void *, int, int, int);
void	cdpcie_decompose_tag(void *, pcitag_t, int *, int *, int *);
int	cdpcie_conf_size(void *, pcitag_t);
pcireg_t cdpcie_conf_read(void *, pcitag_t, int);
void	cdpcie_conf_write(void *, pcitag_t, int, pcireg_t);
int	cdpcie_probe_device_hook(void *, struct pci_attach_args *);

int	cdpcie_intr_map(struct pci_attach_args *, pci_intr_handle_t *);
const char *cdpcie_intr_string(void *, pci_intr_handle_t);
void	*cdpcie_intr_establish(void *, pci_intr_handle_t, int,
	    struct cpu_info *, int (*)(void *), void *, char *);
void	cdpcie_intr_disestablish(void *, void *);

int	cdpcie_bs_iomap(bus_space_tag_t, bus_addr_t, bus_size_t, int,
	    bus_space_handle_t *);
int	cdpcie_bs_memmap(bus_space_tag_t, bus_addr_t, bus_size_t, int,
	    bus_space_handle_t *);

void
cdpcie_attach(struct device *parent, struct device *self, void *aux)
{
	struct cdpcie_softc *sc = (struct cdpcie_softc *)self;
	struct fdt_attach_args *faa = aux;
	struct pcibus_attach_args pba;
	bus_addr_t base, limit, physbase;
	uint32_t bus_range[2];
	uint32_t vendor_id, device_id;
	pcireg_t bir, blr, csr, id;
	int idx, i;

	sc->sc_iot = faa->fa_iot;

	idx = OF_getindex(faa->fa_node, "reg", "reg-names");
	if (idx < 0 || idx > faa->fa_nreg ||
	    bus_space_map(sc->sc_iot, faa->fa_reg[idx].addr,
	    faa->fa_reg[idx].size, 0, &sc->sc_ioh)) {
		printf(": can't map registers\n");
		return;
	}

	idx = OF_getindex(faa->fa_node, "cfg", "reg-names");
	if (idx < 0 || idx > faa->fa_nreg ||
	    bus_space_map(sc->sc_iot, faa->fa_reg[idx].addr,
	    faa->fa_reg[idx].size, 0, &sc->sc_cfg_ioh)) {
		printf(": can't map cfg registers\n");
		bus_space_unmap(sc->sc_iot, sc->sc_ioh, faa->fa_reg[0].size);
		return;
	}
	physbase = faa->fa_reg[idx].addr;

	sc->sc_node = faa->fa_node;
	printf("\n");

	/*
	 * XXX Ignore the "ranges" property from the device tree and
	 * use the firmware configuration instead.  Our PCI
	 * configuration code isn't good enough to handle the multiple
	 * bridges of the Milk-V Pioneer.
	 */
	blr = HREAD4(sc, PPB_REG_IOSTATUS);
	base = (blr & 0x000000f0) << 8;
	limit = (blr & 0x0000f000) | 0x00000fff;
	blr = HREAD4(sc, PPB_REG_IO_HI);
	base |= (blr & 0x0000ffff) << 16;
	limit |= (blr & 0xffff0000); 
	if (limit > base) {
		sc->sc_ranges[sc->sc_nranges].flags = 0x01000000;
		sc->sc_ranges[sc->sc_nranges].pci_base = base;
		sc->sc_ranges[sc->sc_nranges].phys_base = physbase | base;
		sc->sc_ranges[sc->sc_nranges].size = limit - base + 1;
		sc->sc_nranges++;
	}
	blr = HREAD4(sc, PPB_REG_MEM);
	base = (blr & 0x0000fff0) << 16;
	limit = (blr & 0xfff00000) | 0x000fffff;
	if (limit > base) {
		sc->sc_ranges[sc->sc_nranges].flags = 0x02000000;
		sc->sc_ranges[sc->sc_nranges].pci_base = base;
		sc->sc_ranges[sc->sc_nranges].phys_base = physbase | base;
		sc->sc_ranges[sc->sc_nranges].size = limit - base + 1;
		sc->sc_nranges++;
	}
	blr = HREAD4(sc, PPB_REG_PREFMEM);
	base = (blr & 0x0000fff0) << 16;
	limit = (blr & 0xfff00000) | 0x000fffff;
	base |= (bus_addr_t)HREAD4(sc, PPB_REG_PREFBASE_HI32) << 32;
	limit |= (bus_addr_t)HREAD4(sc, PPB_REG_PREFLIM_HI32) << 32;
	if (limit > base) {
		sc->sc_ranges[sc->sc_nranges].flags = 0x43000000;
		sc->sc_ranges[sc->sc_nranges].pci_base = base;
		sc->sc_ranges[sc->sc_nranges].phys_base = physbase | base;
		sc->sc_ranges[sc->sc_nranges].size = limit - base + 1;
		sc->sc_nranges++;
	}

	/* Create extents for our address spaces. */
	sc->sc_busex = extent_create("pcibus", 0, 255,
	    M_DEVBUF, NULL, 0, EX_WAITOK | EX_FILLED);
	sc->sc_memex = extent_create("pcimem", 0, (u_long)-1,
	    M_DEVBUF, NULL, 0, EX_WAITOK | EX_FILLED);
	sc->sc_pmemex = extent_create("pcipmem", 0, (u_long)-1,
	    M_DEVBUF, NULL, 0, EX_WAITOK | EX_FILLED);
	sc->sc_ioex = extent_create("pciio", 0, 0xffffffff,
	    M_DEVBUF, NULL, 0, EX_WAITOK | EX_FILLED);

	/* Set up bus range. */
	if (OF_getpropintarray(sc->sc_node, "bus-range", bus_range,
	    sizeof(bus_range)) != sizeof(bus_range)) {
		bus_range[0] = 0;
		bus_range[1] = 255;
	}
	sc->sc_bus = bus_range[0];
	extent_free(sc->sc_busex, bus_range[0],
	    bus_range[1] - bus_range[0] + 1, EX_WAITOK);

	/* Disable BARs; enable 32-bit IO and 64-bit MMIO. */
	HWRITE4(sc, PCIE_LM_RC_BAR_CFG, PCIE_LM_RC_BAR_CFG_IO_ENABLE |
	    PCIE_LM_RC_BAR_CFG_IO_32BITS | PCIE_LM_RC_BAR_CFG_PMEM_ENABLE |
	    PCIE_LM_RC_BAR_CFG_PMEM_64BITS);

	/* Program vendor and product if requested. */
	vendor_id = OF_getpropint(sc->sc_node, "vendor-id", -1);
	if (vendor_id != -1)
		HWRITE4(sc, PCIE_LM_ID, PCI_ID_CODE(vendor_id, vendor_id));
	device_id = OF_getpropint(sc->sc_node, "device-id", -1);
	if (device_id != -1) {
		id = HREAD4(sc, PCI_ID_REG);
		id = PCI_ID_CODE(PCI_VENDOR(id), device_id);
		HWRITE4(sc, PCIE_RP_BASE + PCI_ID_REG, id);
	}

	/* Program class and set revision to 0. */
	HWRITE4(sc, PCIE_RP_BASE + PCI_CLASS_REG,
	    PCI_CLASS_BRIDGE << PCI_CLASS_SHIFT |
	    PCI_SUBCLASS_BRIDGE_PCI << PCI_SUBCLASS_SHIFT);

	/* Initialize bus range. */
	bir = bus_range[0];
	bir |= ((bus_range[0] + 1) << 8);
	bir |= (bus_range[1] << 16);
	HWRITE4(sc, PPB_REG_BUSINFO, bir);

	/* Configure Address Translation. */
	cdpcie_at_init(sc);

	csr = PCI_COMMAND_MASTER_ENABLE;
	for (i = 0; i < sc->sc_nranges; i++) {
		switch (sc->sc_ranges[i].flags & 0x03000000) {
		case 0x01000000:
			csr |= PCI_COMMAND_IO_ENABLE;
			break;
		case 0x02000000:
		case 0x03000000:
			csr |= PCI_COMMAND_MEM_ENABLE;
			break;
		}
	}
	HWRITE4(sc, PCI_COMMAND_STATUS_REG, csr);

	memcpy(&sc->sc_bus_iot, sc->sc_iot, sizeof(sc->sc_bus_iot));
	sc->sc_bus_iot.bus_private = sc;
	sc->sc_bus_iot._space_map = cdpcie_bs_iomap;
	memcpy(&sc->sc_bus_memt, sc->sc_iot, sizeof(sc->sc_bus_memt));
	sc->sc_bus_memt.bus_private = sc;
	sc->sc_bus_memt._space_map = cdpcie_bs_memmap;

	sc->sc_pc.pc_conf_v = sc;
	sc->sc_pc.pc_attach_hook = cdpcie_attach_hook;
	sc->sc_pc.pc_bus_maxdevs = cdpcie_bus_maxdevs;
	sc->sc_pc.pc_make_tag = cdpcie_make_tag;
	sc->sc_pc.pc_decompose_tag = cdpcie_decompose_tag;
	sc->sc_pc.pc_conf_size = cdpcie_conf_size;
	sc->sc_pc.pc_conf_read = cdpcie_conf_read;
	sc->sc_pc.pc_conf_write = cdpcie_conf_write;
	sc->sc_pc.pc_probe_device_hook = cdpcie_probe_device_hook;

	sc->sc_pc.pc_intr_v = sc;
	sc->sc_pc.pc_intr_map = cdpcie_intr_map;
	sc->sc_pc.pc_intr_map_msi = _pci_intr_map_msi;
	sc->sc_pc.pc_intr_map_msivec = _pci_intr_map_msivec;
	sc->sc_pc.pc_intr_map_msix = _pci_intr_map_msix;
	sc->sc_pc.pc_intr_string = cdpcie_intr_string;
	sc->sc_pc.pc_intr_establish = cdpcie_intr_establish;
	sc->sc_pc.pc_intr_disestablish = cdpcie_intr_disestablish;

	memset(&pba, 0, sizeof(pba));
	pba.pba_busname = "pci";
	pba.pba_iot = &sc->sc_bus_iot;
	pba.pba_memt = &sc->sc_bus_memt;
	pba.pba_dmat = faa->fa_dmat;
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
cdpcie_at_init(struct cdpcie_softc *sc)
{
	uint32_t type;
	int region, nbits;
	int i;

	/* Use region 0 to map PCI configuration space. */
	HWRITE4(sc, PCIE_AT_OB_ADDR1(0), 0);
	HWRITE4(sc, PCIE_AT_OB_DESC1(0), sc->sc_bus);

	region = 1;
	for (i = 0; i < sc->sc_nranges; i ++) {
		/* Skip empty ranges. */
		if (sc->sc_ranges[i].size == 0)
			continue;

		/* Handle IO and MMIO. */
		switch (sc->sc_ranges[i].flags & 0x03000000) {
		case 0x01000000:
			type = PCIE_AT_HDR_IO;
			break;
		case 0x02000000:
		case 0x03000000:
			type = PCIE_AT_HDR_MEM;
			break;
		default:
			continue;
		}

		if ((sc->sc_ranges[i].flags & 0x43000000) == 0x01000000) {
			bus_addr_t iobase, iolimit;
			pcireg_t blr;

			iobase = sc->sc_ranges[i].pci_base;
			iolimit = iobase + sc->sc_ranges[i].size - 1;
			blr = HREAD4(sc, PPB_REG_IOSTATUS) & 0xffff0000;
			blr |= iolimit & PPB_IO_MASK;
			blr |= (iobase & PPB_IO_MASK) >> PPB_IO_SHIFT;
			HWRITE4(sc, PPB_REG_IOSTATUS, blr);
			blr = iobase >> 16;
			blr |= iolimit & 0xffff0000;
			HWRITE4(sc, PPB_REG_IO_HI, blr);

			extent_free(sc->sc_ioex, iobase,
			    iolimit - iobase + 1, EX_WAITOK);
		}

		if ((sc->sc_ranges[i].flags & 0x43000000) == 0x02000000) {
			bus_addr_t membase, memlimit;
			pcireg_t blr;

			membase = sc->sc_ranges[i].pci_base;
			memlimit = membase + sc->sc_ranges[i].size - 1;
			blr = memlimit & PPB_MEM_MASK;
			blr |= (membase & PPB_MEM_MASK) >> PPB_MEM_SHIFT;
			HWRITE4(sc, PPB_REG_MEM, blr);

			extent_free(sc->sc_memex, membase,
			    memlimit - membase + 1, EX_WAITOK);
		}

		if ((sc->sc_ranges[i].flags & 0x43000000) == 0x43000000) {
			bus_addr_t pmembase, pmemlimit;
			pcireg_t blr;

			pmembase = sc->sc_ranges[i].pci_base;
			pmemlimit = pmembase + sc->sc_ranges[i].size - 1;
			blr = pmemlimit & PPB_MEM_MASK;
			blr |= ((pmembase & PPB_MEM_MASK) >> PPB_MEM_SHIFT);
			HWRITE4(sc, PPB_REG_PREFMEM, blr);
			HWRITE4(sc, PPB_REG_PREFBASE_HI32, pmembase >> 32);
			HWRITE4(sc, PPB_REG_PREFLIM_HI32, pmemlimit >> 32);

			extent_free(sc->sc_pmemex, pmembase,
			    pmemlimit - pmembase + 1, EX_WAITOK);
		}

		nbits = 0;
		while ((1ULL << nbits) < sc->sc_ranges[i].size)
			nbits++;

		HWRITE4(sc, PCIE_AT_OB_ADDR0(region),
		    PCIE_AT_OB_ADDR0_NBITS(nbits) | sc->sc_ranges[i].pci_base);
		HWRITE4(sc, PCIE_AT_OB_ADDR1(region),
		    sc->sc_ranges[i].pci_base >> 32);
		HWRITE4(sc, PCIE_AT_OB_CPU_ADDR0(region),
		    PCIE_AT_OB_ADDR0_NBITS(nbits) | sc->sc_ranges[i].phys_base);
		HWRITE4(sc, PCIE_AT_OB_CPU_ADDR1(region),
		    sc->sc_ranges[i].phys_base >> 32);
		HWRITE4(sc, PCIE_AT_OB_DESC0(region), PCIE_AT_HDR_RID | type);
		HWRITE4(sc, PCIE_AT_OB_DESC1(region), sc->sc_bus);
		region++;
	}

	/* Passthrough inbound translations unmodified. */
	HWRITE4(sc, PCIE_AT_IB_ADDR0(2), PCIE_AT_OB_ADDR0_NBITS(48));
	HWRITE4(sc, PCIE_AT_IB_ADDR1(2), 0);
}

void
cdpcie_attach_hook(struct device *parent, struct device *self,
    struct pcibus_attach_args *pba)
{
}

int
cdpcie_bus_maxdevs(void *v, int bus)
{
	struct cdpcie_softc *sc = v;

	if (bus == sc->sc_bus || bus == sc->sc_bus + 1)
		return 1;
	return 32;
}

pcitag_t
cdpcie_make_tag(void *v, int bus, int device, int function)
{
	/* Return ECAM address. */
	return ((bus << 20) | (device << 15) | (function << 12));
}

void
cdpcie_decompose_tag(void *v, pcitag_t tag, int *bp, int *dp, int *fp)
{
	if (bp != NULL)
		*bp = (tag >> 20) & 0xff;
	if (dp != NULL)
		*dp = (tag >> 15) & 0x1f;
	if (fp != NULL)
		*fp = (tag >> 12) & 0x7;
}

int
cdpcie_conf_size(void *v, pcitag_t tag)
{
	return PCIE_CONFIG_SPACE_SIZE;
}

pcireg_t
cdpcie_conf_read(void *v, pcitag_t tag, int reg)
{
	struct cdpcie_softc *sc = v;
	int bus, dev, fn;

	cdpcie_decompose_tag(sc, tag, &bus, &dev, &fn);
	if (bus == sc->sc_bus) {
		KASSERT(dev == 0 && fn == 0);
		return HREAD4(sc, reg);
	}

	HWRITE4(sc, PCIE_AT_OB_ADDR0(0), PCIE_AT_OB_ADDR0_NBITS(12) | tag);
	if (bus == sc->sc_bus + 1)
		HWRITE4(sc, PCIE_AT_OB_DESC0(0),
		   PCIE_AT_HDR_CFG_TYPE0 | PCIE_AT_HDR_RID);
	else
		HWRITE4(sc, PCIE_AT_OB_DESC0(0),
		   PCIE_AT_HDR_CFG_TYPE1 | PCIE_AT_HDR_RID);

	return bus_space_read_4(sc->sc_iot, sc->sc_cfg_ioh, reg);
}

void
cdpcie_conf_write(void *v, pcitag_t tag, int reg, pcireg_t data)
{
	struct cdpcie_softc *sc = v;
	int bus, dev, fn;

	cdpcie_decompose_tag(sc, tag, &bus, &dev, &fn);
	if (bus == sc->sc_bus) {
		KASSERT(dev == 0 && fn == 0);
		HWRITE4(sc, reg, data);
		return;
	}

	HWRITE4(sc, PCIE_AT_OB_ADDR0(0), PCIE_AT_OB_ADDR0_NBITS(12) | tag);
	if (bus == sc->sc_bus + 1)
		HWRITE4(sc, PCIE_AT_OB_DESC0(0),
		   PCIE_AT_HDR_CFG_TYPE0 | PCIE_AT_HDR_RID);
	else
		HWRITE4(sc, PCIE_AT_OB_DESC0(0),
		   PCIE_AT_HDR_CFG_TYPE1 | PCIE_AT_HDR_RID);

	bus_space_write_4(sc->sc_iot, sc->sc_cfg_ioh, reg, data);
}

int
cdpcie_probe_device_hook(void *v, struct pci_attach_args *pa)
{
	return 0;
}

int
cdpcie_intr_map(struct pci_attach_args *pa, pci_intr_handle_t *ihp)
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
cdpcie_intr_string(void *v, pci_intr_handle_t ih)
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
cdpcie_intr_establish(void *v, pci_intr_handle_t ih, int level,
    struct cpu_info *ci, int (*func)(void *), void *arg, char *name)
{
	struct cdpcie_softc *sc = v;
	void *cookie = NULL;

	KASSERT(ih.ih_type != PCI_NONE);

	if (ih.ih_type != PCI_INTX) {
		uint64_t addr = 0, data;

		/* Assume hardware passes Requester ID as sideband data. */
		data = pci_requester_id(ih.ih_pc, ih.ih_tag);
		cookie = fdt_intr_establish_msi_cpu(sc->sc_node, &addr,
		    &data, level, ci, func, arg, name);
		if (cookie == NULL)
			return NULL;

		/* TODO: translate address to the PCI device's view */

		if (ih.ih_type == PCI_MSIX) {
			pci_msix_enable(ih.ih_pc, ih.ih_tag,
			    &sc->sc_bus_memt, ih.ih_intrpin, addr, data);
		} else
			pci_msi_enable(ih.ih_pc, ih.ih_tag, addr, data);
	}

	return cookie;
}

void
cdpcie_intr_disestablish(void *v, void *cookie)
{
}

int
cdpcie_bs_iomap(bus_space_tag_t t, bus_addr_t addr, bus_size_t size,
    int flags, bus_space_handle_t *bshp)
{
	struct cdpcie_softc *sc = t->bus_private;
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
cdpcie_bs_memmap(bus_space_tag_t t, bus_addr_t addr, bus_size_t size,
    int flags, bus_space_handle_t *bshp)
{
	struct cdpcie_softc *sc = t->bus_private;
	int i;

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
