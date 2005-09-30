/*	$OpenBSD: ht.c,v 1.5 2005/09/30 21:37:21 kettenis Exp $	*/

/*
 * Copyright (c) 2005 Mark Kettenis
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

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <macppc/pci/pcibrvar.h>

#include <dev/ofw/openfirm.h>

int	 ht_match(struct device *, void *, void *);
void	 ht_attach(struct device *, struct device *, void *);

void	 ht_attach_hook(struct device *, struct device *,
	     struct pcibus_attach_args *);
int	 ht_bus_maxdevs(void *, int);
pcitag_t ht_make_tag(void *, int, int, int);
void	 ht_decompose_tag(void *, pcitag_t, int *, int *, int *);
pcireg_t ht_conf_read(void *, pcitag_t, int);
void	 ht_conf_write(void *, pcitag_t, int, pcireg_t);
int	 ht_intr_map(void *, pcitag_t, int, int, pci_intr_handle_t *);
const char *ht_intr_string(void *, pci_intr_handle_t);
int	 ht_intr_line(void *, pci_intr_handle_t);
void	*ht_intr_establish(void *, pci_intr_handle_t, int, int (*)(void *),
	     void *, char *);
void	 ht_intr_disestablish(void *, void *);

int	 ht_ether_hw_addr(struct ppc_pci_chipset *, u_int8_t *);

int	 ht_print(void *, const char *);

struct ht_softc {
	struct device	sc_dev;
	int		sc_maxdevs;
	struct ppc_bus_space sc_mem_bus_space;
	struct ppc_bus_space sc_io_bus_space;
	struct ppc_pci_chipset sc_pc;
	bus_space_tag_t sc_memt;
	bus_space_handle_t sc_config0_memh;
	bus_space_handle_t sc_config1_memh;
	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_config0_ioh;
};

struct cfattach ht_ca = {
	sizeof(struct ht_softc), ht_match, ht_attach
};

struct cfdriver ht_cd = {
	NULL, "ht", DV_DULL,
};

#if 0
struct powerpc_bus_dma_tag pci_bus_dma_tag = {
	NULL,
	_dmamap_create,
	_dmamap_destroy,
	_dmamap_load,
	_dmamap_load_mbuf,
	_dmamap_load_uio,
	_dmamap_load_raw,
	_dmamap_unload,
	_dmamap_sync,
	_dmamem_alloc,
	_dmamem_free,
	_dmamem_map,
	_dmamem_unmap,
	_dmamem_mmap
};
#else
extern struct powerpc_bus_dma_tag pci_bus_dma_tag;
#endif

int
ht_match(struct device *parent, void *cf, void *aux)
{
	struct confargs *ca = aux;

	if (strcmp(ca->ca_name, "ht") == 0)
		return (1);
	return (0);
}

void
ht_attach(struct device *parent, struct device *self, void *aux)
{
	struct ht_softc *sc = (struct ht_softc *)self;
	struct confargs *ca = aux;
	struct pcibus_attach_args pba;
	u_int32_t regs[6];
	char compat[32];
	int node, nn;
	int len;

	if (ca->ca_node == 0) {
		printf("invalid node on ht config\n");
		return;
	}

	len = OF_getprop(ca->ca_node, "reg", regs, sizeof(regs));
	if (len < sizeof(regs)) {
		printf(": regs lookup failed, node %x\n", ca->ca_node);
		return;
	}

	sc->sc_mem_bus_space.bus_base = 0x80000000;
	sc->sc_mem_bus_space.bus_size = 0;
	sc->sc_mem_bus_space.bus_io = 0;
	sc->sc_memt = &sc->sc_mem_bus_space;

	sc->sc_io_bus_space.bus_base = 0x80000000;
	sc->sc_io_bus_space.bus_size = 0;
	sc->sc_io_bus_space.bus_io = 1;
	sc->sc_iot = &sc->sc_io_bus_space;

	if (bus_space_map(sc->sc_memt, regs[1], 0x4000, 0,
		&sc->sc_config0_memh)) {
		printf(": can't map PCI config0 memory\n");
		return;
	}

	if (bus_space_map(sc->sc_memt, regs[1] + 0x01000000, 0x80000, 0,
		&sc->sc_config1_memh)) {
		printf(": can't map PCI config1 memory\n");
		return;
	}

	if (bus_space_map(sc->sc_iot, regs[4], 0x1000, 0,
		&sc->sc_config0_ioh)) {
		printf(": can't map PCI config0 io\n");
		return;
	}

	len = OF_getprop(ca->ca_node, "compatible", compat, sizeof(compat));
	if (len <= 0)
		printf(": unknown");
	else
		printf(": %s", compat);

	sc->sc_pc.pc_conf_v = sc;
	sc->sc_pc.pc_attach_hook = ht_attach_hook;
	sc->sc_pc.pc_bus_maxdevs = ht_bus_maxdevs;
	sc->sc_pc.pc_make_tag = ht_make_tag;
	sc->sc_pc.pc_decompose_tag = ht_decompose_tag;
	sc->sc_pc.pc_conf_read = ht_conf_read;
	sc->sc_pc.pc_conf_write = ht_conf_write;

	sc->sc_pc.pc_intr_v = sc;
	sc->sc_pc.pc_intr_map = ht_intr_map;
	sc->sc_pc.pc_intr_string = ht_intr_string;
	sc->sc_pc.pc_intr_line = ht_intr_line;
	sc->sc_pc.pc_intr_establish = ht_intr_establish;
	sc->sc_pc.pc_intr_disestablish = ht_intr_disestablish;
	sc->sc_pc.pc_ether_hw_addr = ht_ether_hw_addr;

	pba.pba_busname = "pci";
	pba.pba_iot = sc->sc_iot;
	pba.pba_memt = sc->sc_memt;
	pba.pba_dmat = &pci_bus_dma_tag;
	pba.pba_pc = &sc->sc_pc;
	pba.pba_bus = 0;

	sc->sc_maxdevs = 1;
	for (node = OF_child(ca->ca_node); node; node = OF_peer(node))
		sc->sc_maxdevs++;
	printf(": %d devices\n", sc->sc_maxdevs);

	extern void fix_node_irq(int, struct pcibus_attach_args *);

	for (node = OF_child(ca->ca_node); node; node = nn) {
		fix_node_irq(node, &pba);

		if ((nn = OF_child(node)) != 0)
			continue;

		while ((nn = OF_peer(node)) == 0) {
			node = OF_parent(node);
			if (node == ca->ca_node) {
				nn = 0;
				break;
			}
		}
	}

	config_found(self, &pba, ht_print);
}

void
ht_attach_hook(struct device *parent, struct device *self,
    struct pcibus_attach_args *pba)
{
}

int
ht_bus_maxdevs(void *cpv, int bus)
{
	struct ht_softc *sc = cpv;

	/* XXX Probing more busses doesn't work. */
	if (bus == 0)
		return sc->sc_maxdevs;
	return 32;
}

#define BUS_SHIFT 16
#define DEVICE_SHIFT 11
#define FNC_SHIFT 8

pcitag_t
ht_make_tag(void *cpv, int bus, int dev, int fnc)
{
	return (bus << BUS_SHIFT) | (dev << DEVICE_SHIFT) | (fnc << FNC_SHIFT);
}

void
ht_decompose_tag(void *cpv, pcitag_t tag, int *busp, int *devp, int *fncp)
{
	if (busp != NULL)
		*busp = (tag >> BUS_SHIFT) & 0xff;
	if (devp != NULL)
		*devp = (tag >> DEVICE_SHIFT) & 0x1f;
	if (fncp != NULL)
		*fncp = (tag >> FNC_SHIFT) & 0x7;
}

pcireg_t
ht_conf_read(void *cpv, pcitag_t tag, int offset)
{
	struct ht_softc *sc = cpv;
	int bus, dev, fcn;
	pcireg_t reg;

#ifdef DEBUG
	printf("ht_conf_read: tag=%x, offset=%x\n", tag, offset);
#endif
	ht_decompose_tag(NULL, tag, &bus, &dev, &fcn);
	if (bus == 0 && dev == 0) {
		tag |= (offset << 2);
		reg = bus_space_read_4(sc->sc_iot, sc->sc_config0_ioh, tag);
		reg = letoh32(reg);
	} else if (bus == 0) {
		if (tag >= 0x4000)
			panic("tag >= 0x4000");
		/* XXX Needed on some PowerMac G5's.  Why? */
		if (fcn > 1)
			return ~0;
		tag |= offset;
		reg = bus_space_read_4(sc->sc_memt, sc->sc_config0_memh, tag);
	} else {
		tag |= offset;
		reg = bus_space_read_4(sc->sc_memt, sc->sc_config1_memh, tag);
	}
#ifdef DEBUG
	printf("ht_conf_read: reg=%x\n", reg);
#endif
	return reg;
}

void
ht_conf_write(void *cpv, pcitag_t tag, int offset, pcireg_t data)
{
	struct ht_softc *sc = cpv;
	int bus, dev;

#ifdef DEBUG
	printf("ht_conf_write: tag=%x, offset=%x, data = %x\n",
	       tag, offset, data);
#endif
	ht_decompose_tag(NULL, tag, &bus, &dev, NULL);
	if (bus == 0 && dev == 0) {
		tag |= (offset << 2);
		data = htole32(data);
		bus_space_write_4(sc->sc_iot, sc->sc_config0_ioh, tag, data);
		bus_space_read_4(sc->sc_iot, sc->sc_config0_ioh, tag);
	} else if (bus == 0) {
		tag |= offset;
		bus_space_write_4(sc->sc_memt, sc->sc_config0_memh, tag, data);
		bus_space_read_4(sc->sc_memt, sc->sc_config0_memh, tag);
	} else {
		tag |= offset;
		bus_space_write_4(sc->sc_memt, sc->sc_config1_memh, tag, data);
		bus_space_read_4(sc->sc_memt, sc->sc_config1_memh, tag);
	}
}

/* XXX */
#define PCI_INTERRUPT_NO_CONNECTION	0xff

int
ht_intr_map(void *cpv, pcitag_t tag, int pin, int line,
    pci_intr_handle_t *ihp)
{
	int error = 0;

#ifdef DEBUG
	printf("ht_intr_map: tag=%x, pin=%d, line=%d\n", tag, pin, line);
#endif

	*ihp = -1;
        if (pin == PCI_INTERRUPT_PIN_NONE ||
	    line == PCI_INTERRUPT_NO_CONNECTION)
                error = 1; /* No IRQ used. */
        else if (pin > PCI_INTERRUPT_PIN_MAX) {
                printf("ht_intr_map: bad interrupt pin %d\n", pin);
                error = 1;
        }

	if (!error)
		*ihp = line;
	return error;
}

const char *
ht_intr_string(void *cpv, pci_intr_handle_t ih)
{
	static char str[16];

	snprintf(str, sizeof str, "irq %ld", ih);
	return (str);
}

int
ht_intr_line(void *cpv, pci_intr_handle_t ih)
{
	return (ih);
}

void *
ht_intr_establish(void *cpv, pci_intr_handle_t ih, int level,
    int (*func)(void *), void *arg, char *name)
{
	return (*intr_establish_func)(cpv, ih, IST_LEVEL, level, func, arg,
		name);
}

void
ht_intr_disestablish(void *lcv, void *cookie)
{
}

int
ht_ether_hw_addr(struct ppc_pci_chipset *lcpc, u_int8_t *oaddr)
{
	u_int8_t laddr[6];
	int node;
	int len;

	node = OF_finddevice("enet");
	len = OF_getprop(node, "local-mac-address", laddr, sizeof(laddr));
	if (sizeof(laddr) == len) {
		memcpy(oaddr, laddr, sizeof(laddr));
		return 1;
	}

	oaddr[0] = oaddr[1] = oaddr[2] = 0xff;
	oaddr[3] = oaddr[4] = oaddr[5] = 0xff;
	return 0;
}

int
ht_print(void *aux, const char *pnp)
{
	struct pcibus_attach_args *pba = aux;

	if (pnp)
		printf("%s at %s", pba->pba_busname, pnp);
	printf(" bus %d", pba->pba_bus);
	return (UNCONF);
}
