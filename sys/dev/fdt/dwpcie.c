/*	$OpenBSD: dwpcie.c,v 1.5 2018/04/09 21:55:25 kettenis Exp $	*/
/*
 * Copyright (c) 2018 Mark Kettenis <kettenis@openbsd.org>
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
#include <dev/ofw/ofw_clock.h>
#include <dev/ofw/ofw_pinctrl.h>
#include <dev/ofw/fdt.h>

/* Registers */
#define MISC_CONTROL_1		0x8bc
#define  MISC_CONTROL_1_DBI_RO_WR_EN	(1 << 0)
#define IATU_VIEWPORT		0x900
#define IATU_REGION_CTRL_1	0x904
#define  IATU_REGION_CTRL_1_TYPE_CFG0	4
#define  IATU_REGION_CTRL_1_TYPE_CFG1	5
#define IATU_REGION_CTRL_2	0x908
#define  IATU_REGION_CTRL_2_REGION_EN	(1U << 31)
#define IATU_LWR_BASE_ADDR	0x90c
#define IATU_UPPER_BASE_ADDR	0x910
#define IATU_LIMIT_ADDR		0x914
#define IATU_LWR_TARGET_ADDR	0x918
#define IATU_UPPER_TARGET_ADDR	0x91c

#define PCIE_GLOBAL_CTRL	0x8000
#define  PCIE_GLOBAL_CTRL_APP_LTSSM_EN		(1 << 2)
#define  PCIE_GLOBAL_CTRL_DEVICE_TYPE_MASK	(0xf << 4)
#define  PCIE_GLOBAL_CTRL_DEVICE_TYPE_RC	(0x4 << 4)
#define PCIE_GLOBAL_STATUS	0x8008
#define  PCIE_GLOBAL_STATUS_RDLH_LINK_UP	(1 << 1)
#define  PCIE_GLOBAL_STATUS_PHY_LINK_UP		(1 << 9)
#define PCIE_PM_STATUS		0x8014
#define PCIE_GLOBAL_INT_CAUSE	0x801c
#define PCIE_GLOBAL_INT_MASK	0x8020
#define  PCIE_GLOBAL_INT_MASK_INT_A		(1 << 9)
#define  PCIE_GLOBAL_INT_MASK_INT_B		(1 << 10)
#define  PCIE_GLOBAL_INT_MASK_INT_C		(1 << 11)
#define  PCIE_GLOBAL_INT_MASK_INT_D		(1 << 12)
#define PCIE_ARCACHE_TRC	0x8050
#define  PCIE_ARCACHE_TRC_DEFAULT		0x3511
#define PCIE_AWCACHE_TRC	0x8054
#define  PCIE_AWCACHE_TRC_DEFAULT		0x5311
#define PCIE_ARUSER		0x805c
#define PCIE_AWUSER		0x8060
#define  PCIE_AXUSER_DOMAIN_MASK		(0x3 << 4)
#define  PCIE_AXUSER_DOMAIN_INNER_SHARABLE	(0x1 << 4)
#define  PCIE_AXUSER_DOMAIN_OUTER_SHARABLE	(0x2 << 4)

#define HREAD4(sc, reg)							\
	(bus_space_read_4((sc)->sc_iot, (sc)->sc_ioh, (reg)))
#define HWRITE4(sc, reg, val)						\
	bus_space_write_4((sc)->sc_iot, (sc)->sc_ioh, (reg), (val))
#define HSET4(sc, reg, bits)						\
	HWRITE4((sc), (reg), HREAD4((sc), (reg)) | (bits))
#define HCLR4(sc, reg, bits)						\
	HWRITE4((sc), (reg), HREAD4((sc), (reg)) & ~(bits))

struct dwpcie_range {
	uint32_t		flags;
	uint64_t		pci_base;
	uint64_t		phys_base;
	uint64_t		size;
};

struct dwpcie_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	bus_space_handle_t	sc_cfg_ioh;
	bus_addr_t		sc_cfg_addr;
	bus_size_t		sc_cfg_size;

	int			sc_node;
	int			sc_acells;
	int			sc_scells;
	int			sc_pacells;
	int			sc_pscells;
	struct dwpcie_range	*sc_ranges;
	int			sc_rangeslen;

	struct arm64_pci_chipset sc_pc;
	struct extent		*sc_busex;
	struct extent		*sc_memex;
	struct extent		*sc_ioex;
	char			sc_busex_name[32];
	char			sc_ioex_name[32];
	char			sc_memex_name[32];
	int			sc_bus;

	void			*sc_ih;
};

int dwpcie_match(struct device *, void *, void *);
void dwpcie_attach(struct device *, struct device *, void *);

struct cfattach	dwpcie_ca = {
	sizeof (struct dwpcie_softc), dwpcie_match, dwpcie_attach
};

struct cfdriver dwpcie_cd = {
	NULL, "dwpcie", DV_DULL
};

int
dwpcie_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return OF_is_compatible(faa->fa_node, "marvell,armada8k-pcie");
}

void	dwpcie_armada8k_init(struct dwpcie_softc *);
int	dwpcie_armada8k_link_up(struct dwpcie_softc *);
int	dwpcie_armada8k_intr(void *);

void	dwpcie_attach_hook(struct device *, struct device *,
	    struct pcibus_attach_args *);
int	dwpcie_bus_maxdevs(void *, int);
pcitag_t dwpcie_make_tag(void *, int, int, int);
void	dwpcie_decompose_tag(void *, pcitag_t, int *, int *, int *);
int	dwpcie_conf_size(void *, pcitag_t);
pcireg_t dwpcie_conf_read(void *, pcitag_t, int);
void	dwpcie_conf_write(void *, pcitag_t, int, pcireg_t);

int	dwpcie_intr_map(struct pci_attach_args *, pci_intr_handle_t *);
int	dwpcie_intr_map_msi(struct pci_attach_args *, pci_intr_handle_t *);
int	dwpcie_intr_map_msix(struct pci_attach_args *, int,
	    pci_intr_handle_t *);
const char *dwpcie_intr_string(void *, pci_intr_handle_t);
void	*dwpcie_intr_establish(void *, pci_intr_handle_t, int,
	    int (*)(void *), void *, char *);
void	dwpcie_intr_disestablish(void *, void *);

void	*dwpcie_armada8k_intr_establish(void *, pci_intr_handle_t, int,
	    int (*)(void *), void *, char *);

void
dwpcie_attach(struct device *parent, struct device *self, void *aux)
{
	struct dwpcie_softc *sc = (struct dwpcie_softc *)self;
	struct fdt_attach_args *faa = aux;
	struct pcibus_attach_args pba;
	uint32_t bus_range[2];
	uint32_t *ranges;
	int i, j, nranges, rangeslen;

	if (faa->fa_nreg < 2) {
		printf(": no registers\n");
		return;
	}

	sc->sc_iot = faa->fa_iot;
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
	    sizeof(struct dwpcie_range), M_TEMP, M_WAITOK);
	sc->sc_rangeslen = nranges;

	for (i = 0, j = 0; i < nranges; i++) {
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

	if (bus_space_map(sc->sc_iot, faa->fa_reg[0].addr,
	    faa->fa_reg[0].size, 0, &sc->sc_ioh)) {
		printf(": can't map ctrl registers\n");
		return;
	}

	if (bus_space_map(sc->sc_iot, faa->fa_reg[1].addr,
	    faa->fa_reg[1].size, 0, &sc->sc_cfg_ioh)) {
		bus_space_unmap(sc->sc_iot, sc->sc_ioh, faa->fa_reg[0].size);
		printf(": can't map config registers\n");
		return;
	}
	sc->sc_cfg_addr = faa->fa_reg[1].addr;
	sc->sc_cfg_size = faa->fa_reg[1].size;

	printf("\n");

	pinctrl_byname(sc->sc_node, "default");

	clock_enable_all(sc->sc_node);

	if (OF_is_compatible(sc->sc_node, "marvell,armada8k-pcie"))
		dwpcie_armada8k_init(sc);

	/* Enable modification of read-only bits. */
	HSET4(sc, MISC_CONTROL_1, MISC_CONTROL_1_DBI_RO_WR_EN);

	/* A Root Port is a PCI-PCI Bridge. */
	HWRITE4(sc, PCI_CLASS_REG,
	    PCI_CLASS_BRIDGE << PCI_CLASS_SHIFT |
	    PCI_SUBCLASS_BRIDGE_PCI << PCI_SUBCLASS_SHIFT);

	/* Clear BAR as U-Boot seems to leave garbage in it. */
	HWRITE4(sc, PCI_MAPREG_START, PCI_MAPREG_MEM_TYPE_64BIT);
	HWRITE4(sc, PCI_MAPREG_START + 4, 0);

	/* Make sure read-only bits are write-protected. */
	HCLR4(sc, MISC_CONTROL_1, MISC_CONTROL_1_DBI_RO_WR_EN);

	/* Create extents for our address spaces. */
	snprintf(sc->sc_busex_name, sizeof(sc->sc_busex_name),
	    "%s pcibus", sc->sc_dev.dv_xname);
	snprintf(sc->sc_ioex_name, sizeof(sc->sc_ioex_name),
	    "%s pciio", sc->sc_dev.dv_xname);
	snprintf(sc->sc_memex_name, sizeof(sc->sc_memex_name),
	    "%s pcimem", sc->sc_dev.dv_xname);
	sc->sc_busex = extent_create("pcibus", 0, 255,
	    M_DEVBUF, NULL, 0, EX_WAITOK | EX_FILLED);
	sc->sc_ioex = extent_create(sc->sc_ioex_name, 0, 0xffffffff,
	    M_DEVBUF, NULL, 0, EX_WAITOK | EX_FILLED);
	sc->sc_memex = extent_create(sc->sc_memex_name, 0, (u_long)-1,
	    M_DEVBUF, NULL, 0, EX_WAITOK | EX_FILLED);

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

	/* Set up memory mapped I/O range. */
	for (i = 0; i < nranges; i++) {
		if ((sc->sc_ranges[i].flags & 0x03000000) == 0x02000000)
			extent_free(sc->sc_memex, sc->sc_ranges[i].pci_base,
			    sc->sc_ranges[i].size, EX_NOWAIT);
	}

	sc->sc_pc.pc_conf_v = sc;
	sc->sc_pc.pc_attach_hook = dwpcie_attach_hook;
	sc->sc_pc.pc_bus_maxdevs = dwpcie_bus_maxdevs;
	sc->sc_pc.pc_make_tag = dwpcie_make_tag;
	sc->sc_pc.pc_decompose_tag = dwpcie_decompose_tag;
	sc->sc_pc.pc_conf_size = dwpcie_conf_size;
	sc->sc_pc.pc_conf_read = dwpcie_conf_read;
	sc->sc_pc.pc_conf_write = dwpcie_conf_write;

	sc->sc_pc.pc_intr_v = sc;
	sc->sc_pc.pc_intr_map = dwpcie_intr_map;
	sc->sc_pc.pc_intr_map_msi = dwpcie_intr_map_msi;
	sc->sc_pc.pc_intr_map_msix = dwpcie_intr_map_msix;
	sc->sc_pc.pc_intr_string = dwpcie_intr_string;
	sc->sc_pc.pc_intr_establish = dwpcie_intr_establish;
	sc->sc_pc.pc_intr_disestablish = dwpcie_intr_disestablish;

	memset(&pba, 0, sizeof(pba));
	pba.pba_busname = "pci";
	pba.pba_iot = faa->fa_iot;
	pba.pba_memt = faa->fa_iot;
	pba.pba_dmat = faa->fa_dmat;
	pba.pba_pc = &sc->sc_pc;
	pba.pba_busex = sc->sc_busex;
	pba.pba_memex = sc->sc_memex;
	pba.pba_ioex = sc->sc_ioex;
	pba.pba_domain = pci_ndomains++;
	pba.pba_bus = sc->sc_bus;

	config_found(self, &pba, NULL);
}

void
dwpcie_armada8k_init(struct dwpcie_softc *sc)
{
	uint32_t reg;
	int timo;

	if (!dwpcie_armada8k_link_up(sc)) {
		reg = HREAD4(sc, PCIE_GLOBAL_CTRL);
		reg &= ~PCIE_GLOBAL_CTRL_APP_LTSSM_EN;
		HWRITE4(sc, PCIE_GLOBAL_CTRL, reg);
	}

	/* Enable Root Complex mode. */
	reg = HREAD4(sc, PCIE_GLOBAL_CTRL);
	reg &= ~PCIE_GLOBAL_CTRL_DEVICE_TYPE_MASK;
	reg |= PCIE_GLOBAL_CTRL_DEVICE_TYPE_RC;
	HWRITE4(sc, PCIE_GLOBAL_CTRL, reg);

	HWRITE4(sc, PCIE_ARCACHE_TRC, PCIE_ARCACHE_TRC_DEFAULT);
	HWRITE4(sc, PCIE_AWCACHE_TRC, PCIE_AWCACHE_TRC_DEFAULT);
	reg = HREAD4(sc, PCIE_ARUSER);
	reg &= ~PCIE_AXUSER_DOMAIN_MASK;
	reg |= PCIE_AXUSER_DOMAIN_OUTER_SHARABLE;
	HWRITE4(sc, PCIE_ARUSER, reg);
	reg = HREAD4(sc, PCIE_AWUSER);
	reg &= ~PCIE_AXUSER_DOMAIN_MASK;
	reg |= PCIE_AXUSER_DOMAIN_OUTER_SHARABLE;
	HWRITE4(sc, PCIE_AWUSER, reg);

	/* Set up addres translation for PCI confg space. */
	HWRITE4(sc, IATU_VIEWPORT, 0);
	HWRITE4(sc, IATU_LWR_BASE_ADDR, sc->sc_cfg_addr);
	HWRITE4(sc, IATU_UPPER_BASE_ADDR, sc->sc_cfg_addr >> 32);
	HWRITE4(sc, IATU_LIMIT_ADDR, sc->sc_cfg_addr + sc->sc_cfg_size - 1);
	HWRITE4(sc, IATU_LWR_TARGET_ADDR, 0);
	HWRITE4(sc, IATU_UPPER_TARGET_ADDR, 0);
	HWRITE4(sc, IATU_REGION_CTRL_2, IATU_REGION_CTRL_2_REGION_EN);

	if (!dwpcie_armada8k_link_up(sc)) {
		reg = HREAD4(sc, PCIE_GLOBAL_CTRL);
		reg |= PCIE_GLOBAL_CTRL_APP_LTSSM_EN;
		HWRITE4(sc, PCIE_GLOBAL_CTRL, reg);
	}

	for (timo = 40; timo > 0; timo--) {
		if (dwpcie_armada8k_link_up(sc))
			break;
		delay(1000);
	}

	sc->sc_ih = arm_intr_establish_fdt(sc->sc_node, IPL_AUDIO | IPL_MPSAFE,
	    dwpcie_armada8k_intr, sc, sc->sc_dev.dv_xname);

	/* Unmask INTx interrupts. */
	HWRITE4(sc, PCIE_GLOBAL_INT_MASK,
	    PCIE_GLOBAL_INT_MASK_INT_A | PCIE_GLOBAL_INT_MASK_INT_B |
	    PCIE_GLOBAL_INT_MASK_INT_C | PCIE_GLOBAL_INT_MASK_INT_D);
}

int
dwpcie_armada8k_link_up(struct dwpcie_softc *sc)
{
	uint32_t reg, mask;

	mask = PCIE_GLOBAL_STATUS_RDLH_LINK_UP;
	mask |= PCIE_GLOBAL_STATUS_PHY_LINK_UP;
	reg = HREAD4(sc, PCIE_GLOBAL_STATUS);
	return ((reg & mask) == mask);
}

int
dwpcie_armada8k_intr(void *arg)
{
	struct dwpcie_softc *sc = arg;
	uint32_t cause;

	/* Acknowledge interrupts. */
	cause = HREAD4(sc, PCIE_GLOBAL_INT_CAUSE);
	HWRITE4(sc, PCIE_GLOBAL_INT_CAUSE, cause);

	/* INTx interrupt, so not really ours. */
	return 0;
}

void
dwpcie_atu_config(struct dwpcie_softc *sc, pcitag_t tag, int type)
{
	HWRITE4(sc, IATU_VIEWPORT, 0);
	HWRITE4(sc, IATU_REGION_CTRL_1, type);
	HWRITE4(sc, IATU_LWR_TARGET_ADDR, tag);
}

void
dwpcie_attach_hook(struct device *parent, struct device *self,
    struct pcibus_attach_args *pba)
{
}

int
dwpcie_bus_maxdevs(void *v, int bus)
{
	struct dwpcie_softc *sc = v;

	if (bus == sc->sc_bus || bus == sc->sc_bus + 1)
		return 1;
	return 32;
}

pcitag_t
dwpcie_make_tag(void *v, int bus, int device, int function)
{
	return ((bus << 24) | (device << 19) | (function << 16));
}

void
dwpcie_decompose_tag(void *v, pcitag_t tag, int *bp, int *dp, int *fp)
{
	if (bp != NULL)
		*bp = (tag >> 24) & 0xff;
	if (dp != NULL)
		*dp = (tag >> 19) & 0x1f;
	if (fp != NULL)
		*fp = (tag >> 16) & 0x7;
}

int
dwpcie_conf_size(void *v, pcitag_t tag)
{
	return PCIE_CONFIG_SPACE_SIZE;
}

pcireg_t
dwpcie_conf_read(void *v, pcitag_t tag, int reg)
{
	struct dwpcie_softc *sc = v;
	int bus, dev, fn;

	dwpcie_decompose_tag(sc, tag, &bus, &dev, &fn);
	if (bus == sc->sc_bus) {
		KASSERT(dev == 0);
		return HREAD4(sc, tag | reg);
	}

	if (bus == sc->sc_bus + 1)
		dwpcie_atu_config(sc, tag, IATU_REGION_CTRL_1_TYPE_CFG0);
	else
		dwpcie_atu_config(sc, tag, IATU_REGION_CTRL_1_TYPE_CFG1);
	return bus_space_read_4(sc->sc_iot, sc->sc_cfg_ioh, reg);
}

void
dwpcie_conf_write(void *v, pcitag_t tag, int reg, pcireg_t data)
{
	struct dwpcie_softc *sc = v;
	int bus, dev, fn;

	dwpcie_decompose_tag(sc, tag, &bus, &dev, &fn);
	if (bus == sc->sc_bus) {
		KASSERT(dev == 0);
		HWRITE4(sc, tag | reg, data);
		return;
	}

	if (bus == sc->sc_bus + 1)
		dwpcie_atu_config(sc, tag, IATU_REGION_CTRL_1_TYPE_CFG0);
	else
		dwpcie_atu_config(sc, tag, IATU_REGION_CTRL_1_TYPE_CFG1);
	bus_space_write_4(sc->sc_iot, sc->sc_cfg_ioh, reg, data);
}

struct dwpcie_intr_handle {
	pci_chipset_tag_t	ih_pc;
	pcitag_t		ih_tag;
	int			ih_intrpin;
	int			ih_msi;
};

int
dwpcie_intr_map(struct pci_attach_args *pa, pci_intr_handle_t *ihp)
{
	struct dwpcie_intr_handle *ih;
	int pin = pa->pa_rawintrpin;

	if (pin == 0 || pin > PCI_INTERRUPT_PIN_MAX)
		return -1;

	if (pa->pa_tag == 0)
		return -1;

	ih = malloc(sizeof(struct dwpcie_intr_handle), M_DEVBUF, M_WAITOK);
	ih->ih_pc = pa->pa_pc;
	ih->ih_tag = pa->pa_intrtag;
	ih->ih_intrpin = pa->pa_intrpin;
	ih->ih_msi = 0;
	*ihp = (pci_intr_handle_t)ih;

	return 0;
}

int
dwpcie_intr_map_msi(struct pci_attach_args *pa, pci_intr_handle_t *ihp)
{
	pci_chipset_tag_t pc = pa->pa_pc;
	pcitag_t tag = pa->pa_tag;
	struct dwpcie_intr_handle *ih;

	if (pci_get_capability(pc, tag, PCI_CAP_MSI, NULL, NULL) == 0)
		return -1;

	ih = malloc(sizeof(struct dwpcie_intr_handle), M_DEVBUF, M_WAITOK);
	ih->ih_pc = pa->pa_pc;
	ih->ih_tag = pa->pa_tag;
	ih->ih_intrpin = pa->pa_intrpin;
	ih->ih_msi = 1;
	*ihp = (pci_intr_handle_t)ih;

	return 0;
}

int
dwpcie_intr_map_msix(struct pci_attach_args *pa, int vec,
    pci_intr_handle_t *ihp)
{
	return -1;
}

const char *
dwpcie_intr_string(void *v, pci_intr_handle_t ihp)
{
	struct dwpcie_intr_handle *ih = (struct dwpcie_intr_handle *)ihp;

	if (ih->ih_msi)
		return "msi";

	return "intx";
}

void *
dwpcie_intr_establish(void *v, pci_intr_handle_t ihp, int level,
    int (*func)(void *), void *arg, char *name)
{
	struct dwpcie_softc *sc = v;
	struct dwpcie_intr_handle *ih = (struct dwpcie_intr_handle *)ihp;
	void *cookie;

	if (ih->ih_msi) {
		uint64_t addr, data;
		pcireg_t reg;
		int off;

		cookie = arm_intr_establish_fdt_msi(sc->sc_node, &addr,
		    &data, level, func, arg, (void *)name);
		if (cookie == NULL)
			return NULL;

		/* TODO: translate address to the PCI device's view */

		if (pci_get_capability(ih->ih_pc, ih->ih_tag, PCI_CAP_MSI,
		    &off, &reg) == 0)
			panic("%s: no msi capability", __func__);

		if (reg & PCI_MSI_MC_C64) {
			pci_conf_write(ih->ih_pc, ih->ih_tag,
			    off + PCI_MSI_MA, addr);
			pci_conf_write(ih->ih_pc, ih->ih_tag,
			    off + PCI_MSI_MAU32, addr >> 32);
			pci_conf_write(ih->ih_pc, ih->ih_tag,
			    off + PCI_MSI_MD64, data);
		} else {
			pci_conf_write(ih->ih_pc, ih->ih_tag,
			    off + PCI_MSI_MA, addr);
			pci_conf_write(ih->ih_pc, ih->ih_tag,
			    off + PCI_MSI_MD32, data);
		}
		pci_conf_write(ih->ih_pc, ih->ih_tag,
		    off, reg | PCI_MSI_MC_MSIE);
	} else {
		int bus, dev, fn;
		uint32_t reg[4];

		dwpcie_decompose_tag(sc, ih->ih_tag, &bus, &dev, &fn);

		reg[0] = bus << 16 | dev << 11 | fn << 8;
		reg[1] = reg[2] = 0;
		reg[3] = ih->ih_intrpin;

		cookie = arm_intr_establish_fdt_imap(sc->sc_node, reg,
		    sizeof(reg), level, func, arg, name);
	}

	free(ih, M_DEVBUF, sizeof(struct dwpcie_intr_handle));
	return cookie;
}

void
dwpcie_intr_disestablish(void *v, void *cookie)
{
	panic("%s", __func__);
}
