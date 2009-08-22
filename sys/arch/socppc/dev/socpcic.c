/*	$OpenBSD: socpcic.c,v 1.4 2009/08/22 02:54:51 mk Exp $	*/

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

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

struct socpcic_softc {
	struct device	sc_dev;
	bus_space_tag_t	sc_iot;
	bus_space_handle_t sc_cfg_ioh;
	struct ppc_bus_space sc_mem_bus_space;
	struct ppc_bus_space sc_io_bus_space;
	struct ppc_pci_chipset sc_pc;
};

int	socpcic_match(struct device *, void *, void *);
void	socpcic_attach(struct device *, struct device *, void *);

struct cfattach socpcic_ca = {
	sizeof(struct socpcic_softc), socpcic_match, socpcic_attach
};

struct cfdriver socpcic_cd = {
	NULL, "socpcic", DV_DULL
};

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
socpcic_match(struct device *parent, void *cfdata, void *aux)
{
	return (1);
}

void
socpcic_attach(struct device *parent, struct device *self, void *aux)
{
	struct socpcic_softc *sc = (void *)self;
	struct obio_attach_args *oa = aux;
	struct pcibus_attach_args pba;

	sc->sc_iot = oa->oa_iot;
	if (bus_space_map(sc->sc_iot, oa->oa_offset, 16, 0, &sc->sc_cfg_ioh)) {
		printf(": can't map configuration registers\n");
		return;
	}

	sc->sc_io_bus_space.bus_base = 0xe2000000;
	sc->sc_io_bus_space.bus_size = 0x01000000;

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

	bzero(&pba, sizeof(pba));
	pba.pba_busname = "pci";
	pba.pba_iot = &sc->sc_io_bus_space;
	pba.pba_memt = &sc->sc_mem_bus_space;
	pba.pba_dmat = oa->oa_dmat;
	pba.pba_pc = &sc->sc_pc;
	pba.pba_domain = pci_ndomains++;
	pba.pba_bus = 0;

	printf("\n");

	config_found(self, &pba, socpcic_print);
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
	*ihp = 20;		/* XXX */
	return (0);
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
