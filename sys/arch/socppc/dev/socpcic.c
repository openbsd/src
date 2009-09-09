/*	$OpenBSD: socpcic.c,v 1.8 2009/09/09 20:42:41 kettenis Exp $	*/

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
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/autoconf.h>
#include <machine/bus.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_pci.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

struct socpcic_softc {
	struct device	sc_dev;

	int		sc_node;

	bus_space_tag_t	sc_iot;
	bus_space_handle_t sc_cfg_ioh;
	struct ppc_bus_space sc_mem_bus_space;
	struct ppc_bus_space sc_io_bus_space;
	bus_dma_tag_t	sc_dmat;
	struct ppc_pci_chipset sc_pc;

	/* Interrupt mapping. */
	int		sc_map_mask[4];
	int		*sc_map;
	int		sc_map_len;

	/* Addres space. */
	uint32_t	*sc_ranges;
	int		sc_ranges_len;
};

int	socpcic_mainbus_match(struct device *, void *, void *);
void	socpcic_mainbus_attach(struct device *, struct device *, void *);
int	socpcic_obio_match(struct device *, void *, void *);
void	socpcic_obio_attach(struct device *, struct device *, void *);

struct cfattach socpcic_mainbus_ca = {
	sizeof(struct socpcic_softc),
	socpcic_mainbus_match, socpcic_mainbus_attach
};

struct cfattach socpcic_obio_ca = {
	sizeof(struct socpcic_softc),
	socpcic_obio_match, socpcic_obio_attach
};

struct cfdriver socpcic_cd = {
	NULL, "socpcic", DV_DULL
};

void	socpcic_attach(struct socpcic_softc *sc);
void	socpcic_attach_hook(struct device *, struct device *,
	    struct pcibus_attach_args *);
int	socpcic_bus_maxdevs(void *, int);
pcitag_t socpcic_make_tag(void *, int, int, int);
void	socpcic_decompose_tag(void *, pcitag_t, int *, int *, int *);
pcireg_t socpcic_conf_read(void *, pcitag_t, int);
void	socpcic_conf_write(void *, pcitag_t, int, pcireg_t);
int	 socpcic_intr_map(void *, pcitag_t, int, int, pci_intr_handle_t *);
const char *socpcic_intr_string(void *, pci_intr_handle_t);
int	 socpcic_intr_line(void *, pci_intr_handle_t);
void	*socpcic_intr_establish(void *, pci_intr_handle_t, int,
	     int (*)(void *), void *, const char *);
void	 socpcic_intr_disestablish(void *, void *);
int	 socpcic_ether_hw_addr(struct ppc_pci_chipset *, u_int8_t *);

int	 socpcic_print(void *, const char *);

int
socpcic_mainbus_match(struct device *parent, void *cfdata, void *aux)
{
	struct mainbus_attach_args *ma = aux;
	char buf[32];

	if (OF_getprop(ma->ma_node, "device_type", buf, sizeof(buf)) <= 0 ||
	    strcmp(buf, "pci") != 0)
		return (0);

	if (OF_getprop(ma->ma_node, "compatible", buf, sizeof(buf)) <= 0 ||
	    strcmp(buf, "fsl,mpc8349-pci") != 0)
		return (0);

	return (1);
}

void
socpcic_mainbus_attach(struct device *parent, struct device *self, void *aux)
{
	struct socpcic_softc *sc = (void *)self;
	struct mainbus_attach_args *ma = aux;
	int reg[4];

	if (OF_getprop(ma->ma_node, "reg", &reg, sizeof(reg)) < sizeof(reg)) {
		printf(": missing registers\n");
		return;
	}

	sc->sc_iot = ma->ma_iot;
	if (bus_space_map(sc->sc_iot, reg[2], 16, 0, &sc->sc_cfg_ioh)) {
		printf(": can't map configuration registers\n");
		return;
	}

	sc->sc_node = ma->ma_node;
	sc->sc_dmat = ma->ma_dmat;
	socpcic_attach(sc);
}

int
socpcic_obio_match(struct device *parent, void *cfdata, void *aux)
{
	struct obio_attach_args *oa = aux;
	char buf[32];

	if (OF_getprop(oa->oa_node, "device_type", buf, sizeof(buf)) <= 0 ||
	    strcmp(buf, "pci") != 0)
		return (0);

	if (OF_getprop(oa->oa_node, "compatible", buf, sizeof(buf)) <= 0 ||
	    strcmp(buf, "83xx") != 0)
		return (0);

	return (1);
}

void
socpcic_obio_attach(struct device *parent, struct device *self, void *aux)
{
	struct socpcic_softc *sc = (void *)self;
	struct obio_attach_args *oa = aux;

	if (oa->oa_offset == 0x8500)
		oa->oa_offset = 0x8300;

	sc->sc_iot = oa->oa_iot;
	if (bus_space_map(sc->sc_iot, oa->oa_offset, 16, 0, &sc->sc_cfg_ioh)) {
		printf(": can't map configuration registers\n");
		return;
	}

	sc->sc_node = oa->oa_node;
	sc->sc_dmat = oa->oa_dmat;
	socpcic_attach(sc);
}

void
socpcic_attach(struct socpcic_softc *sc)
{
	struct pcibus_attach_args pba;
	struct extent *io_ex;
	struct extent *mem_ex;
	bus_addr_t io_base, mem_base;
	bus_size_t io_size, mem_size;
	uint32_t *ranges;
	int len;

	if (OF_getprop(sc->sc_node, "interrupt-map-mask", sc->sc_map_mask,
	    sizeof(sc->sc_map_mask)) != sizeof(sc->sc_map_mask)) {
		printf(": missing interrupt-map-mask\n");
		return;
	}

	sc->sc_map_len = OF_getproplen(sc->sc_node, "interrupt-map");
	if (sc->sc_map_len > 0) {
		sc->sc_map = malloc(sc->sc_map_len, M_DEVBUF, M_NOWAIT);
		if (sc->sc_map == NULL)
			panic("out of memory");

		len = OF_getprop(sc->sc_node, "interrupt-map",
		    sc->sc_map, sc->sc_map_len);
		KASSERT(len == sc->sc_map_len);
	}

	sc->sc_ranges_len = OF_getproplen(sc->sc_node, "ranges");
	if (sc->sc_ranges_len <= 0) {
		printf(": missing ranges\n");
		return;
	}

	sc->sc_ranges = malloc(sc->sc_ranges_len, M_DEVBUF, M_NOWAIT);
	if (ranges == NULL)
		panic("out of memory");

	len = OF_getprop(sc->sc_node, "ranges", sc->sc_ranges,
	    sc->sc_ranges_len);
	KASSERT(len == sc->sc_ranges_len);

	ranges = sc->sc_ranges;
	while (len >= 6 * sizeof(uint32_t)) {
		switch (ranges[0] & OFW_PCI_PHYS_HI_SPACEMASK) {
		case OFW_PCI_PHYS_HI_SPACE_IO:
			KASSERT(ranges[1] == 0);
			KASSERT(ranges[4] == 0);
			sc->sc_io_bus_space.bus_base = ranges[3];
			sc->sc_io_bus_space.bus_size = ranges[5];
			sc->sc_io_bus_space.bus_io = 1;
			io_base = ranges[2];
			io_size = ranges[5];
			break;
		case  OFW_PCI_PHYS_HI_SPACE_MEM32:
			KASSERT(ranges[1] == 0);
			KASSERT(ranges[4] == 0);
			sc->sc_mem_bus_space.bus_base = ranges[3];
			sc->sc_mem_bus_space.bus_size = ranges[5];
			sc->sc_mem_bus_space.bus_io = 0;
			mem_base = ranges[2];
			mem_size = ranges[5];
			break;
		}
		len -= 6 * sizeof(uint32_t);
		ranges += 6;
	}

	sc->sc_pc.pc_conf_v = sc;
	sc->sc_pc.pc_attach_hook = socpcic_attach_hook;
	sc->sc_pc.pc_bus_maxdevs = socpcic_bus_maxdevs;
	sc->sc_pc.pc_make_tag = socpcic_make_tag;
	sc->sc_pc.pc_decompose_tag = socpcic_decompose_tag;
	sc->sc_pc.pc_conf_read = socpcic_conf_read;
	sc->sc_pc.pc_conf_write = socpcic_conf_write;

	sc->sc_pc.pc_intr_v = sc;
	sc->sc_pc.pc_intr_map = socpcic_intr_map;
	sc->sc_pc.pc_intr_string = socpcic_intr_string;
	sc->sc_pc.pc_intr_line = socpcic_intr_line;
	sc->sc_pc.pc_intr_establish = socpcic_intr_establish;
	sc->sc_pc.pc_intr_disestablish = socpcic_intr_disestablish;
	sc->sc_pc.pc_ether_hw_addr = socpcic_ether_hw_addr;

	io_ex = extent_create("pciio", 0, 0xffffffff, M_DEVBUF, NULL, 0,
	    EX_NOWAIT | EX_FILLED);
	if (io_ex != NULL && io_size)
		extent_free(io_ex, io_base, io_size, EX_NOWAIT);
	mem_ex = extent_create("pcimem", 0, 0xffffffff, M_DEVBUF, NULL, 0,
	    EX_NOWAIT | EX_FILLED);
	if (mem_ex != NULL)
		extent_free(mem_ex, mem_base, mem_size, EX_NOWAIT);

	bzero(&pba, sizeof(pba));
	pba.pba_busname = "pci";
	pba.pba_iot = &sc->sc_io_bus_space;
	pba.pba_memt = &sc->sc_mem_bus_space;
	pba.pba_dmat = sc->sc_dmat;
	pba.pba_ioex = io_ex;
	pba.pba_memex = mem_ex;
	pba.pba_pc = &sc->sc_pc;
	pba.pba_domain = pci_ndomains++;
	pba.pba_bus = 0;

	printf("\n");

	config_found((struct device *)sc, &pba, socpcic_print);
}

void
socpcic_attach_hook(struct device *parent, struct device *self,
    struct pcibus_attach_args *pba)
{
}

int
socpcic_bus_maxdevs(void *cpv, int bus)
{
	return (32);
}

pcitag_t
socpcic_make_tag(void *cpv, int bus, int dev, int fun)
{
	return ((bus << 16) | (dev << 11) | (fun << 8));
}

void
socpcic_decompose_tag(void *cpv, pcitag_t tag, int *busp, int *devp, int *funp)
{
	if (busp)
		*busp = (tag >> 16) & 0xff;
	if (devp)
		*devp = (tag >> 11) & 0x1f;
	if (funp)
		*funp = (tag >> 8) & 0x7;
}

pcireg_t
socpcic_conf_read(void *cpv, pcitag_t tag, int offset)
{
	struct socpcic_softc *sc = cpv;
	int bus, dev, fun;
	pcireg_t addr, data;

	socpcic_decompose_tag(sc, tag, &bus, &dev, &fun);
	if (bus == 0) {
		if (dev > 0 && dev < 11)
			return (0xffffffff);
		if (dev == 31)
			tag = (10 << 11) | (fun << 8);
	}

	addr = 0x80000000 | tag | offset;
	bus_space_write_4(sc->sc_iot, sc->sc_cfg_ioh, 0, addr);
	bus_space_read_4(sc->sc_iot, sc->sc_cfg_ioh, 0);
	data = bus_space_read_4(sc->sc_iot, sc->sc_cfg_ioh, 4);
	bus_space_write_4(sc->sc_iot, sc->sc_cfg_ioh, 0, 0);
	bus_space_read_4(sc->sc_iot, sc->sc_cfg_ioh, 0);

	return (data);
}

void
socpcic_conf_write(void *cpv, pcitag_t tag, int offset, pcireg_t data)
{
	struct socpcic_softc *sc = cpv;
	int bus, dev, fun;
	pcireg_t addr;

	socpcic_decompose_tag(sc, tag, &bus, &dev, &fun);
	if (bus == 0) {
		if (dev > 0 && dev < 11)
			return;
		if (dev == 31)
			tag = (10 << 11) | (fun << 8);
	}

	addr = 0x80000000 | tag | offset;
	bus_space_write_4(sc->sc_iot, sc->sc_cfg_ioh, 0, addr);
	bus_space_read_4(sc->sc_iot, sc->sc_cfg_ioh, 0);
	bus_space_write_4(sc->sc_iot, sc->sc_cfg_ioh, 4, data);
	bus_space_write_4(sc->sc_iot, sc->sc_cfg_ioh, 0, 0);
	bus_space_read_4(sc->sc_iot, sc->sc_cfg_ioh, 0);
}

int
socpcic_intr_map(void *cpv, pcitag_t tag, int pin, int line,
    pci_intr_handle_t *ihp)
{
	struct socpcic_softc *sc = cpv;
	int bus, dev, func;
	int reg[4];
	int *map;
	int len;

	pci_decompose_tag(&sc->sc_pc, tag, &bus, &dev, &func);

	reg[0] = (dev << 11) | (func << 8);
	reg[1] = reg[2] = 0;
	reg[3] = pin;
	
	map = sc->sc_map;
	len = sc->sc_map_len;
	while (len >= 7 * sizeof(int)) {
		if ((reg[0] & sc->sc_map_mask[0]) == map[0] &&
		    (reg[1] & sc->sc_map_mask[1]) == map[1] &&
		    (reg[2] & sc->sc_map_mask[2]) == map[2] &&
		    (reg[3] & sc->sc_map_mask[3]) == map[3]) {
			*ihp = map[5];
			return (0);
		}
		len -= 7 * sizeof(int);
		map += 7;
	}

	return (1);
}

const char *
socpcic_intr_string(void *cpv, pci_intr_handle_t ih)
{
	static char str[16];

	snprintf(str, sizeof str, "ivec %ld", ih);
	return (str);
}

int
socpcic_intr_line(void *cpv, pci_intr_handle_t ih)
{
	return (ih);
}

void *
socpcic_intr_establish(void *cpv, pci_intr_handle_t ih, int level,
    int (*func)(void *), void *arg, const char *name)
{
	return (intr_establish(ih, IST_LEVEL, level, func, arg, name));
}

void
socpcic_intr_disestablish(void *lcv, void *cookie)
{
}

int
socpcic_ether_hw_addr(struct ppc_pci_chipset *lcpc, u_int8_t *oaddr)
{
	oaddr[0] = oaddr[1] = oaddr[2] = 0xff;
	oaddr[3] = oaddr[4] = oaddr[5] = 0xff;

	return (0);
}

int
socpcic_print(void *aux, const char *pnp)
{
	struct pcibus_attach_args *pba = aux;

	if (pnp)
		printf("%s at %s", pba->pba_busname, pnp);
	printf(" bus %d", pba->pba_bus);
	return (UNCONF);
}
