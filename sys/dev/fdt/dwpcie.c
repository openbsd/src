/*	$OpenBSD: dwpcie.c,v 1.1 2018/04/02 15:25:27 kettenis Exp $	*/
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
#include <dev/ofw/ofw_misc.h>
#include <dev/ofw/fdt.h>

#define DWPCIE_MISC_CONTROL_1		0x8bc
#define  DWPCIE_MISC_CONTROL_1_DBI_RO_WR_EN	(1 << 0)
#define DWPCIE_ATU_VIEWPORT		0x900
#define DWPCIE_ATU_RC1			0x904
#define  DWPCIE_ATU_RC1_TYPE_CFG0	4
#define  DWPCIE_ATU_RC1_TYPE_CFG1	5
#define DWPCIE_ATU_RLTA			0x918
#define DWPCIE_ATU_RUTA			0x91c

#define HREAD4(sc, reg)							\
	(bus_space_read_4((sc)->sc_iot, (sc)->sc_ioh, (reg)))
#define HWRITE4(sc, reg, val)						\
	bus_space_write_4((sc)->sc_iot, (sc)->sc_ioh, (reg), (val))
#define HSET4(sc, reg, bits)						\
	HWRITE4((sc), (reg), HREAD4((sc), (reg)) | (bits))
#define HCLR4(sc, reg, bits)						\
	HWRITE4((sc), (reg), HREAD4((sc), (reg)) & ~(bits))

struct dwpcie_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	bus_space_handle_t	sc_cfg_ioh;
	int			sc_node;

	int			sc_acells;
	int			sc_scells;
	int			sc_pacells;
	int			sc_pscells;

	struct arm64_pci_chipset sc_pc;
	struct extent		*sc_busex;
	struct extent		*sc_memex;
	struct extent		*sc_ioex;
	int			sc_bus;
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

	return OF_is_compatible(faa->fa_node, "snps,dw-pcie");
}

void	dwpcie_atr_init(struct dwpcie_softc *);
void	dwpcie_phy_init(struct dwpcie_softc *);
void	dwpcie_phy_poweron(struct dwpcie_softc *);

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

void
dwpcie_attach(struct device *parent, struct device *self, void *aux)
{
	struct dwpcie_softc *sc = (struct dwpcie_softc *)self;
	struct fdt_attach_args *faa = aux;
	struct pcibus_attach_args pba;
	uint32_t bus_range[2];

	if (faa->fa_nreg < 2) {
		printf(": no registers\n");
		return;
	}

	sc->sc_iot = faa->fa_iot;

	sc->sc_acells = OF_getpropint(sc->sc_node, "#address-cells",
	    faa->fa_acells);
	sc->sc_scells = OF_getpropint(sc->sc_node, "#size-cells",
	    faa->fa_scells);
	sc->sc_pacells = faa->fa_acells;
	sc->sc_pscells = faa->fa_scells;

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

	sc->sc_node = faa->fa_node;
	printf("\n");

	clock_enable_all(sc->sc_node);

	/* Clear BAR and make sure read-only bits are write-protected. */
	HSET4(sc, DWPCIE_MISC_CONTROL_1,
	    DWPCIE_MISC_CONTROL_1_DBI_RO_WR_EN);
	HWRITE4(sc, PCI_MAPREG_START, PCI_MAPREG_MEM_TYPE_64BIT);
	HWRITE4(sc, PCI_MAPREG_START + 4, 0);
	HCLR4(sc, DWPCIE_MISC_CONTROL_1,
	    DWPCIE_MISC_CONTROL_1_DBI_RO_WR_EN);

	/* Create extents for our address spaces. */
	sc->sc_busex = extent_create("pcibus", 0, 255,
	    M_DEVBUF, NULL, 0, EX_WAITOK | EX_FILLED);
#ifdef notyet
	sc->sc_memex = extent_create("pcimem", 0, (u_long)-1,
	    M_DEVBUF, NULL, 0, EX_WAITOK | EX_FILLED);
	sc->sc_ioex = extent_create("pciio", 0, 0xffffffff,
	    M_DEVBUF, NULL, 0, EX_WAITOK | EX_FILLED);
#endif

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
dwpcie_atu_config(struct dwpcie_softc *sc, pcitag_t tag, int type)
{
	HWRITE4(sc, DWPCIE_ATU_VIEWPORT, 0);
	HWRITE4(sc, DWPCIE_ATU_RC1, type);
	HWRITE4(sc, DWPCIE_ATU_RLTA, tag);
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
		dwpcie_atu_config(sc, tag, DWPCIE_ATU_RC1_TYPE_CFG0);
	else
		dwpcie_atu_config(sc, tag, DWPCIE_ATU_RC1_TYPE_CFG1);
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
		dwpcie_atu_config(sc, tag, DWPCIE_ATU_RC1_TYPE_CFG0);
	else
		dwpcie_atu_config(sc, tag, DWPCIE_ATU_RC1_TYPE_CFG1);
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
		    sizeof(reg), sc->sc_pacells, level, func, arg,
		    (void *)name);
	}

	free(ih, M_DEVBUF, sizeof(struct dwpcie_intr_handle));
	return cookie;
}

void
dwpcie_intr_disestablish(void *v, void *cookie)
{
}
