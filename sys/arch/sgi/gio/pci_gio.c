/*	$OpenBSD: pci_gio.c,v 1.3 2014/10/02 18:55:49 miod Exp $	*/
/*	$NetBSD: pci_gio.c,v 1.9 2011/07/01 18:53:46 dyoung Exp $	*/

/*
 * Copyright (c) 2006 Stephen M. Rumble
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Glue for PCI devices that are connected to the GIO bus by various little
 * GIO<->PCI ASICs.
 *
 * We presently recognize the following boards:
 *	o Phobos G100/G130/G160	(dc, lxtphy)
 *	o Set Engineering GFE	(tl, nsphy)
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/extent.h>

#include <machine/autoconf.h>
#include <machine/bus.h>
#include <machine/cpu.h>

#include <mips64/archtype.h>
#include <sgi/sgi/ip22.h>

#include <sgi/gio/giovar.h>
#include <sgi/gio/gioreg.h>
#include <sgi/gio/giodevs.h>

#include <sgi/localbus/imcvar.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>

#include "cardbus.h"

#if NCARDBUS > 0
#include <dev/cardbus/rbus.h>
#endif

int giopci_debug = 0;
#define DPRINTF(_x)	if (giopci_debug) printf _x

struct giopci_softc {
	struct device			sc_dev;
	int				sc_slot;

	bus_space_tag_t			sc_cfgt;
	bus_space_handle_t		sc_cfgh;
	uint32_t			sc_cfg_len;

	struct mips_pci_chipset		sc_pc;

	bus_dma_tag_t			sc_dmat;
	bus_size_t			sc_dma_boundary;
	struct machine_bus_dma_tag	sc_dmat_store;
};

int	 giopci_match(struct device *, void *, void *);
void	 giopci_attach(struct device *, struct device *, void *);

const struct cfattach giopci_ca = {
	sizeof(struct giopci_softc), giopci_match, giopci_attach
};

struct cfdriver giopci_cd = {
	NULL, "giopci", DV_DULL
};

void	 giopci_attach_hook(struct device *, struct device *,
	    struct pcibus_attach_args *);
int	 giopci_bus_maxdevs(void *, int);
pcitag_t giopci_make_tag(void *, int, int, int);
void	 giopci_decompose_tag(void *, pcitag_t, int *, int *, int *);
int	 giopci_conf_size(void *, pcitag_t);
pcireg_t giopci_conf_read(void *, pcitag_t, int);
void	 giopci_conf_write(void *, pcitag_t, int, pcireg_t);
int	 giopci_probe_device_hook(void *, struct pci_attach_args *);
int	 giopci_get_widget(void *);
int	 giopci_get_dl(void *, pcitag_t, struct sgi_device_location *);
int	 giopci_intr_map(struct pci_attach_args *, pci_intr_handle_t *);
const char *giopci_intr_string(void *, pci_intr_handle_t);
void	*giopci_intr_establish(void *, pci_intr_handle_t, int, int (*)(void *),
	    void *, const char *);
void	 giopci_intr_disestablish(void *, void *);
int	 giopci_intr_line(void *, pci_intr_handle_t);
int	 giopci_ppb_setup(void *, pcitag_t, bus_addr_t *, bus_addr_t *,
	    bus_addr_t *, bus_addr_t *);
void	*giopci_rbus_parent_io(struct pci_attach_args *);
void	*giopci_rbus_parent_mem(struct pci_attach_args *);

static const struct mips_pci_chipset giopci_pci_chipset = {
	.pc_attach_hook = giopci_attach_hook,
	.pc_bus_maxdevs = giopci_bus_maxdevs,
	.pc_make_tag = giopci_make_tag,
	.pc_decompose_tag = giopci_decompose_tag,
	.pc_conf_size = giopci_conf_size,
	.pc_conf_read = giopci_conf_read,
	.pc_conf_write = giopci_conf_write,
	.pc_probe_device_hook = giopci_probe_device_hook,
	.pc_get_widget = giopci_get_widget,
	.pc_get_dl = giopci_get_dl,
	.pc_intr_map = giopci_intr_map,
	.pc_intr_string = giopci_intr_string,
	.pc_intr_establish = giopci_intr_establish,
	.pc_intr_disestablish = giopci_intr_disestablish,
	.pc_intr_line = giopci_intr_line,
	.pc_ppb_setup = giopci_ppb_setup,
#if NCARDBUS > 0
	.pc_rbus_parent_io = giopci_rbus_parent_io,
	.pc_rbus_parent_mem = giopci_rbus_parent_mem
#endif
};

int	 giopci_dmamap_create(bus_dma_tag_t, bus_size_t, int, bus_size_t,
	    bus_size_t, int, bus_dmamap_t *);
int      giopci_print(void *, const char *);

#define PHOBOS_PCI_OFFSET	0x00100000
#define PHOBOS_PCI_LENGTH	0x00000080	/* verified on G130 */
#define PHOBOS_TULIP_START	0x00101000
#define PHOBOS_TULIP_END	0x001fffff

#define SETENG_MAGIC_OFFSET	0x00020000
#define SETENG_MAGIC_VALUE	0x00001000
#define SETENG_PCI_OFFSET	0x00080000
#define SETENG_PCI_LENGTH	0x00000080	/* ~arbitrary */
#define SETENG_TLAN_START	0x00100000
#define SETENG_TLAN_END		0x001fffff

int
giopci_match(struct device *parent, void *vcf, void *aux)
{
	struct gio_attach_args *ga = aux;

	switch (GIO_PRODUCT_PRODUCTID(ga->ga_product)) {
	case GIO_PRODUCT_PHOBOS_G160:
	case GIO_PRODUCT_PHOBOS_G130:
	case GIO_PRODUCT_PHOBOS_G100:
	case GIO_PRODUCT_SETENG_GFE:
		return 1;
	}

	return 0;
}

void 
giopci_attach(struct device *parent, struct device *self, void *aux)
{
	struct giopci_softc *sc = (struct giopci_softc *)self;
	pci_chipset_tag_t pc = &sc->sc_pc;
	struct gio_attach_args *ga = aux;
	uint32_t cfg_off, cfg_len, arb, reg;
	struct pcibus_attach_args pba;
	uint32_t m_start, m_end;
	struct extent *ex;
	pcireg_t csr;

	printf(": %s\n",
	    gio_product_string(GIO_PRODUCT_PRODUCTID(ga->ga_product)));

	sc->sc_cfgt = ga->ga_iot;
	sc->sc_dmat = ga->ga_dmat;
	sc->sc_slot = ga->ga_slot;

	if (sys_config.system_type != SGI_IP20 &&
	    sys_config.system_subtype == IP22_INDIGO2)
		arb = GIO_ARB_RT | GIO_ARB_MST | GIO_ARB_PIPE;
	else
		arb = GIO_ARB_RT | GIO_ARB_MST;

	if (gio_arb_config(ga->ga_slot, arb)) {
		printf("%s: failed to configure GIO bus arbiter\n",
		    self->dv_xname);
		return;
	}

	imc_disable_sysad_parity();

	switch (GIO_PRODUCT_PRODUCTID(ga->ga_product)) {
	case GIO_PRODUCT_PHOBOS_G160:
	case GIO_PRODUCT_PHOBOS_G130:
	case GIO_PRODUCT_PHOBOS_G100:
		cfg_off = PHOBOS_PCI_OFFSET;
		cfg_len = PHOBOS_PCI_LENGTH;
		m_start = PHOBOS_TULIP_START;
		m_end = PHOBOS_TULIP_END;
		sc->sc_dma_boundary = 0;
		break;

	case GIO_PRODUCT_SETENG_GFE:
		cfg_off = SETENG_PCI_OFFSET;
		cfg_len = SETENG_PCI_LENGTH;
		m_start = SETENG_TLAN_START;
		m_end = SETENG_TLAN_END;
		sc->sc_dma_boundary = 0x1000;

		bus_space_write_4(ga->ga_iot, ga->ga_ioh,
		    SETENG_MAGIC_OFFSET, SETENG_MAGIC_VALUE);

		break;
	}

	if (bus_space_subregion(ga->ga_iot, ga->ga_ioh, cfg_off, cfg_len,
	    &sc->sc_cfgh)) {
		printf("%s: unable to map PCI configuration space\n",
		    self->dv_xname);
		return;
	}

	sc->sc_cfg_len = cfg_len;

	bcopy(&giopci_pci_chipset, pc, sizeof(giopci_pci_chipset));
	pc->pc_conf_v = pc->pc_intr_v = sc;

	/*
	 * Setup a bus_dma tag if necessary.
	 */

	if (sc->sc_dma_boundary != 0) {
		bcopy(ga->ga_dmat, &sc->sc_dmat_store,
		    sizeof(struct machine_bus_dma_tag));
		sc->sc_dmat_store._dmamap_create = giopci_dmamap_create;
		sc->sc_dmat_store._cookie = sc;
	}

	/*
	 * Setup resource extent for memory BARs.
	 */

	ex = extent_create(self->dv_xname, 0, (u_long)-1L, M_DEVBUF,
	    NULL, 0, EX_NOWAIT | EX_FILLED);
	if (ex == NULL || extent_free(ex, ga->ga_addr + m_start,
	    m_end + 1 - m_start, EX_NOWAIT) != 0) {
		printf("%s: unable to setup PCI resource management\n",
		    self->dv_xname);
		return;
	}

	/*
	 * Reset all BARs. Note that we are assuming there is only
	 * one device, which is neither a bridge nor a multifunction
	 * device.
	 * This is necessary because they contain garbage upon poweron,
	 * and although the bridge chip does not support I/O mappings,
	 * the chips behind it (at least on Phobos boards) have I/O BARs.
	 */

	for (reg = PCI_MAPREG_START; reg < PCI_MAPREG_END; reg += 4)
		bus_space_write_4(sc->sc_cfgt, sc->sc_cfgh, reg, 0);

	csr =
	    bus_space_read_4(sc->sc_cfgt, sc->sc_cfgh, PCI_COMMAND_STATUS_REG);
	bus_space_write_4(sc->sc_cfgt, sc->sc_cfgh, PCI_COMMAND_STATUS_REG,
	    (csr & ~(PCI_COMMAND_IO_ENABLE | PCI_COMMAND_MEM_ENABLE)) |
	    PCI_COMMAND_MASTER_ENABLE);

	bzero(&pba, sizeof(pba));
	pba.pba_busname = "pci";
	pba.pba_iot = pba.pba_memt = ga->ga_iot;
	if (sc->sc_dma_boundary == 0)
		pba.pba_dmat = ga->ga_dmat;
	else
		pba.pba_dmat = &sc->sc_dmat_store;
	pba.pba_pc = pc;
	pba.pba_ioex = NULL;
	pba.pba_memex = ex;
	pba.pba_domain = pci_ndomains++;
	pba.pba_bus = 0;

	config_found(self, &pba, giopci_print);
}

int
giopci_print(void *aux, const char *pnp)
{
	struct pcibus_attach_args *pba = aux;

	if (pnp != NULL)
		printf("%s at %s", pba->pba_busname, pnp);
	printf(" bus %d", pba->pba_bus);

	return UNCONF;
}

/*
 * pci_chipset_t routines
 */

void
giopci_attach_hook(struct device *parent, struct device *self,
    struct pcibus_attach_args *pba)
{
}

pcitag_t
giopci_make_tag(void *v, int bus, int dev, int fnc)
{
	return (bus << 16) | (dev << 11) | (fnc << 8);
}

void
giopci_decompose_tag(void *v, pcitag_t tag, int *busp, int *devp, int *fncp)
{
	if (busp != NULL)
		*busp = (tag >> 16) & 0xff;
	if (devp != NULL)
		*devp = (tag >> 11) & 0x1f;
	if (fncp != NULL)
		*fncp = (tag >> 8) & 0x7;
}

int
giopci_bus_maxdevs(void *v, int busno)
{
	return busno == 0 ? 1 : 0;
}

int
giopci_conf_size(void *v, pcitag_t tag)
{
	struct giopci_softc *sc = (struct giopci_softc *)v;

	return sc->sc_cfg_len;
}

pcireg_t
giopci_conf_read(void *v, pcitag_t tag, int reg)
{
	struct giopci_softc *sc = v;
	int bus, dev, func;
	pcireg_t data;

	giopci_decompose_tag(v, tag, &bus, &dev, &func);
	if (bus != 0 || dev != 0 || func != 0)
		return reg == PCI_ID_REG ? 0xffffffff : 0;

	if (reg >= sc->sc_cfg_len)
		return 0;

	DPRINTF(("giopci_conf_read: reg 0x%x = 0x", reg));
	data = bus_space_read_4(sc->sc_cfgt, sc->sc_cfgh, reg);
	DPRINTF(("%08x\n", data));

	return data;
}

void
giopci_conf_write(void *v, pcitag_t tag, int reg, pcireg_t data)
{
	struct giopci_softc *sc = v;
	int bus, dev, func;

	giopci_decompose_tag(v, tag, &bus, &dev, &func);
	if (bus != 0 || dev != 0 || func != 0)
		return;

	if (reg >= sc->sc_cfg_len)
		return;

	DPRINTF(("giopci_conf_write: reg 0x%x = 0x%08x\n", reg, data));
	bus_space_write_4(sc->sc_cfgt, sc->sc_cfgh, reg, data);
}

int
giopci_probe_device_hook(void *unused, struct pci_attach_args *notused)
{
	return 0;
}

/* will not actually be used */
int
giopci_get_widget(void *unused)
{
	return 0;
}

/* will not actually be used */
int
giopci_get_dl(void *v, pcitag_t tag, struct sgi_device_location *sdl)
{
	int bus, device, fn;

	memset(sdl, 0, sizeof *sdl);
	giopci_decompose_tag(v, tag, &bus, &device, &fn);
	if (bus != 0)
		return 0;
	sdl->device = device;
	sdl->fn = fn;
	return 1;
}

int
giopci_intr_map(struct pci_attach_args *pa, pci_intr_handle_t *ihp)
{
	struct giopci_softc *sc = (struct giopci_softc *)pa->pa_pc->pc_intr_v;
	int intr;

	if (pa->pa_rawintrpin == 0)
		intr = -1;
	else
		intr = gio_intr_map(sc->sc_slot);

	*ihp = intr;
	return intr < 0 ? 1 : 0;
}

const char *
giopci_intr_string(void *v, pci_intr_handle_t ih)
{
	static char str[10];

	snprintf(str, sizeof(str), "irq %d", (int)ih);
	return str;
}

void *
giopci_intr_establish(void *v, pci_intr_handle_t ih, int level,
    int (*func)(void *), void *arg, const char *name)
{
	return gio_intr_establish((int)ih, level, func, arg, name);
}

void
giopci_intr_disestablish(void *v, void *ih)
{
	panic("%s", __func__);
}

int
giopci_intr_line(void *v, pci_intr_handle_t ih)
{
	return (int)ih;
}

int
giopci_ppb_setup(void *cookie, pcitag_t tag, bus_addr_t *iostart,
    bus_addr_t *ioend, bus_addr_t *memstart, bus_addr_t *memend)
{
	panic("%s", __func__);
}

#if NCARDBUS > 0
void *
giopci_rbus_parent_io(struct pci_attach_args *pa)
{
	panic("%s");
}

void *
giopci_rbus_parent_mem(struct pci_attach_args *pa)
{
	panic("%s");
}
#endif	/* NCARDBUS > 0 */

/*
 *  DMA routines
 */

int
giopci_dmamap_create(bus_dma_tag_t t, bus_size_t size, int nsegments,
    bus_size_t maxsegsz, bus_size_t boundary, int flags, bus_dmamap_t *dmamp)
{
	struct giopci_softc *sc = (struct giopci_softc *)t->_cookie;

	if (boundary == 0 || boundary > sc->sc_dma_boundary)
		boundary = sc->sc_dma_boundary;

	return bus_dmamap_create(sc->sc_dmat, size, nsegments, maxsegsz,
	    boundary, flags, dmamp);
}
