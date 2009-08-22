/*	$OpenBSD: mpcpcibr.c,v 1.20 2009/08/22 02:54:50 mk Exp $ */

/*
 * Copyright (c) 2001 Steve Murphree, Jr.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed under OpenBSD for RTMX Inc
 *      by Per Fogelstrom, Opsycon AB.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/*
 * Motorola 'Raven' PCI BUS Bridge driver.
 * specialized hooks for different config methods.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <uvm/uvm_extern.h>

#include <machine/autoconf.h>
#include <machine/bus.h>
#include <machine/pcb.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <mvmeppc/pci/pcibrvar.h>
#include <mvmeppc/dev/ravenreg.h>
#include <mvmeppc/dev/ravenvar.h>

int    mpcpcibrmatch(struct device *, void *, void *);
void   mpcpcibrattach(struct device *, struct device *, void *);

void   mpc_attach_hook(struct device *, struct device *,
    struct pcibus_attach_args *);
int    mpc_bus_maxdevs(void *, int);
pcitag_t mpc_make_tag(void *, int, int, int);
void   mpc_decompose_tag(void *, pcitag_t, int *, int *, int *);
pcireg_t mpc_conf_read(void *, pcitag_t, int);
void   mpc_conf_write(void *, pcitag_t, int, pcireg_t);

int      mpc_intr_map(void *, pcitag_t, int, int, pci_intr_handle_t *);
const char *mpc_intr_string(void *, pci_intr_handle_t);
int	 mpc_intr_line(void *, pci_intr_handle_t);
void     *mpc_intr_establish(void *, pci_intr_handle_t,
    int, int (*)(void *), void *, const char *);
void     mpc_intr_disestablish(void *, void *);
int      mpc_ether_hw_addr(struct ppc_pci_chipset *, u_int8_t *);

void mpc_cfg_write_1(struct pcibr_config *, u_int32_t, u_int8_t);
void mpc_cfg_write_2(struct pcibr_config *, u_int32_t, u_int16_t);
void mpc_cfg_write_4(struct pcibr_config *, u_int32_t, u_int32_t);

u_int8_t mpc_cfg_read_1(struct pcibr_config *, u_int32_t);
u_int16_t mpc_cfg_read_2(struct pcibr_config *, u_int32_t);
u_int32_t mpc_cfg_read_4(struct pcibr_config *, u_int32_t);

u_int32_t pci_iack(void);
u_int32_t mpc_gen_config_reg(void *, pcitag_t, int);

struct cfattach mpcpcibr_ca = {
	sizeof(struct pcibr_softc), mpcpcibrmatch, mpcpcibrattach,
};

struct cfdriver mpcpcibr_cd = {
	NULL, "mpcpcibr", DV_DULL,
};

int      mpcpcibrprint(void *, const char *pnp);

struct pcibr_config mpc_config;

struct powerpc_bus_dma_tag pci_bus_dma_tag = {
	NULL,
	_bus_dmamap_create,
	_bus_dmamap_destroy,
	_bus_dmamap_load,
	_bus_dmamap_load_mbuf,
	_bus_dmamap_load_uio,
	_bus_dmamap_load_raw,
	_bus_dmamap_unload,
	_bus_dmamap_sync,
	_bus_dmamem_alloc,
	_bus_dmamem_free,
	_bus_dmamem_map,
	_bus_dmamem_unmap,
	_bus_dmamem_mmap
};

extern u_int8_t *ravenregs;
extern vaddr_t isaspace_va;

struct raven_setup {
	unsigned int	pci_reg;
	u_int32_t	value;
};

const struct raven_setup raven_prep_setup[] = {
	/* PCI registers */
	{ RAVEN_PCI_MEM, RAVEN_PCI_MEM_VAL },
	{ RAVEN_PCI_PSADD0, RAVEN_PCI_PSADD0_VAL },
	{ RAVEN_PCI_PSOFF0, RAVEN_PCI_PSOFF0_VAL },
	{ RAVEN_PCI_PSADD1, RAVEN_PCI_PSADD1_VAL },
	{ RAVEN_PCI_PSOFF1, RAVEN_PCI_PSOFF1_VAL },
	{ RAVEN_PCI_PSADD2, RAVEN_PCI_PSADD2_VAL },
	{ RAVEN_PCI_PSOFF2, RAVEN_PCI_PSOFF2_VAL },
	{ RAVEN_PCI_PSADD3, RAVEN_PCI_PSADD3_VAL },
	{ RAVEN_PCI_PSOFF3, RAVEN_PCI_PSOFF3_VAL },

#ifdef notyet
	/* Universe PCI registers */
	{ 0x100, 0xc0825100 },
	{ 0x104, 0x01000000 },
	{ 0x108, 0x30000000 },
	{ 0x10c, 0x00000000 },
	{ 0x114, 0xc0425100 },
	{ 0x118, 0x30000000 },
	{ 0x11c, 0x38000000 },
	{ 0x120, 0x00000000 },
	{ 0x128, 0x00000000 },
	{ 0x12c, 0x00000000 },
	{ 0x130, 0x00000000 },
	{ 0x134, 0x00000000 },
	{ 0x13c, 0x00000000 },
	{ 0x140, 0x00000000 },
	{ 0x144, 0x00000000 },
	{ 0x148, 0x00000000 },
	{ 0x188, 0xc0a05338 },

	/* Default Universe VME Slave Map */
	{ 0xf00, 0xc0f20001 },
	{ 0xf04, 0x40000000 },
	{ 0xf08, 0x40001000 },
	{ 0xf0c, 0xc0001000 },
	{ 0xf14, 0xe0f200c0 },
	{ 0xf18, 0x10000000 },
	{ 0xf1c, 0x20000000 },
	{ 0xf20, 0x70000000 },
	{ 0xf28, 0x00000000 },
	{ 0xf2c, 0x00000000 },
	{ 0xf30, 0x00000000 },
	{ 0xf34, 0x00000000 },
	{ 0xf3c, 0x00000000 },
	{ 0xf40, 0x00000000 },
	{ 0xf44, 0x00000000 },
	{ 0xf48, 0x00000000 },
#endif

	{ 0, 0 },
};

int
mpcpcibrmatch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	/* We must be a child of the raven device */
	if (strcmp(parent->dv_cfdata->cf_driver->cd_name, "raven") != 0)
		return (0);

	return 1;
}

void
mpcpcibrattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct pcibr_softc *sc = (struct pcibr_softc *)self;
	struct pcibr_config *lcp;
	struct pcibus_attach_args pba;
	const struct raven_setup *rs;

	lcp = sc->sc_pcibr = &mpc_config;

	sc->sc_membus_space = prep_mem_space_tag;
	sc->sc_iobus_space = prep_io_space_tag;

	lcp->lc_pc.pc_conf_v = lcp;
	lcp->lc_pc.pc_attach_hook = mpc_attach_hook;
	lcp->lc_pc.pc_bus_maxdevs = mpc_bus_maxdevs;
	lcp->lc_pc.pc_make_tag = mpc_make_tag;
	lcp->lc_pc.pc_decompose_tag = mpc_decompose_tag;
	lcp->lc_pc.pc_conf_read = mpc_conf_read;
	lcp->lc_pc.pc_conf_write = mpc_conf_write;
	lcp->lc_pc.pc_ether_hw_addr = mpc_ether_hw_addr;
	lcp->lc_iot = &sc->sc_iobus_space;
	lcp->lc_memt = &sc->sc_membus_space;

	lcp->ioh_cf8 = (PREP_CONFIG_ADD - RAVEN_P_ISA_IO_SPACE) +
		(bus_space_handle_t)isaspace_va;
	lcp->ioh_cfc = (PREP_CONFIG_DAT - RAVEN_P_ISA_IO_SPACE) +
		(bus_space_handle_t)isaspace_va;

	lcp->config_type = 0;

	lcp->lc_pc.pc_intr_v = lcp;
	lcp->lc_pc.pc_intr_map = mpc_intr_map;
	lcp->lc_pc.pc_intr_string = mpc_intr_string;
	lcp->lc_pc.pc_intr_line = mpc_intr_line;
	lcp->lc_pc.pc_intr_establish = mpc_intr_establish;
	lcp->lc_pc.pc_intr_disestablish = mpc_intr_disestablish;

	printf(": revision 0x%x\n", 
	    mpc_cfg_read_1(lcp, RAVEN_PCI_REVID));

	bzero(&pba, sizeof(pba));
	pba.pba_dmat = &pci_bus_dma_tag;

	pba.pba_busname = "pci";
	pba.pba_iot = &sc->sc_iobus_space;
	pba.pba_memt = &sc->sc_membus_space;
	pba.pba_pc = &lcp->lc_pc;
	pba.pba_domain = pci_ndomains++;
	pba.pba_bus = 0; 

	/*
	 * Set up PREP environment
	 */

	*(u_int32_t *)(ravenregs + RAVEN_MSADD0) = RAVEN_MSADD0_PREP;
	*(u_int32_t *)(ravenregs + RAVEN_MSOFF0) = RAVEN_MSOFF0_PREP;
	*(u_int32_t *)(ravenregs + RAVEN_MSADD1) = RAVEN_MSADD1_PREP;
	*(u_int32_t *)(ravenregs + RAVEN_MSOFF1) = RAVEN_MSOFF1_PREP;
	*(u_int32_t *)(ravenregs + RAVEN_MSADD2) = RAVEN_MSADD2_PREP;
	*(u_int32_t *)(ravenregs + RAVEN_MSOFF2) = RAVEN_MSOFF2_PREP;
	*(u_int32_t *)(ravenregs + RAVEN_MSADD3) = RAVEN_MSADD3_PREP;
	*(u_int32_t *)(ravenregs + RAVEN_MSOFF3) = RAVEN_MSOFF3_PREP;

	for (rs = raven_prep_setup; rs->pci_reg != 0; rs++) {
		mpc_cfg_write_4(lcp, rs->pci_reg, rs->value);
	}

	/* enable mem and io mapping, and bus master */
	mpc_cfg_write_2(lcp, RAVEN_PCI_CMD, 
	    RAVEN_CMD_IOSP | RAVEN_CMD_MEMSP | RAVEN_CMD_MASTR);

	config_found(self, &pba, mpcpcibrprint);
}

int
mpcpcibrprint(aux, pnp)
	void *aux;
	const char *pnp;
{
	struct pcibus_attach_args *pba = aux;

	if (pnp)
		printf("%s at %s", pba->pba_busname, pnp);
	printf(" bus %d", pba->pba_bus);
	return (UNCONF);
}

void
mpc_attach_hook(parent, self, pba)
	struct device *parent, *self;
	struct pcibus_attach_args *pba;
{
}

int
mpc_ether_hw_addr(p, ethaddr)
	struct ppc_pci_chipset *p;
	u_int8_t *ethaddr;
{
	printf("mpc_ether_hw_addr not supported\n");
	return (0);
}

int
mpc_bus_maxdevs(cpv, busno)
	void *cpv;
	int busno;
{
	return (32);
}

#define BUS_SHIFT 16
#define DEVICE_SHIFT 11
#define FNC_SHIFT 8

pcitag_t
mpc_make_tag(cpv, bus, dev, fnc)
	void *cpv;
	int bus, dev, fnc;
{
	return (bus << BUS_SHIFT) | (dev << DEVICE_SHIFT) | (fnc << FNC_SHIFT);
}

void
mpc_decompose_tag(cpv, tag, busp, devp, fncp)
	void *cpv;
	pcitag_t tag;
	int *busp, *devp, *fncp;
{
	if (busp != NULL)
		*busp = (tag >> BUS_SHIFT) & 0xff;
	if (devp != NULL)
		*devp = (tag >> DEVICE_SHIFT) & 0x1f;
	if (fncp != NULL)
		*fncp = (tag >> FNC_SHIFT) & 0x7;
}

u_int32_t
mpc_gen_config_reg(cpv, tag, offset)
	void *cpv;
	pcitag_t tag;
	int offset;
{
#if 0
	struct pcibr_config *cp = cpv;
	unsigned int bus, dev, fcn;
#endif
	u_int32_t reg;

#if 0
	mpc_decompose_tag(cpv, tag, &bus, &dev, &fcn);

	if (cp->config_type & 1) {
		/* Config Mechanism #2 */
		if (bus == 0) {
			if (dev < 11) {
				return 0xffffffff;
			}
			/*
			 * Need to do config type 0 operation
			 *  1 << (11?+dev) | fcn << 8 | reg
			 * 11? is because pci spec states
			 * that 11-15 is reserved.
			 */
			reg = 1 << (dev) | fcn << 8 | offset;

		} else {
			if (dev > 15) {
				return 0xffffffff;
			}
			/*
			 * config type 1 
			 */
			reg =  tag  | offset | 1;

		}
	} else {
#else
	{
#endif
		/* config mechanism #2, type 0 */
		/* standard cf8/cfc config */
		reg =  0x80000000 | tag  | offset;

	}
	return reg;
}

/*#define DEBUG_CONFIG */
pcireg_t
mpc_conf_read(cpv, tag, offset)
	void *cpv;
	pcitag_t tag;
	int offset;
{
	struct pcibr_config *cp = cpv;
	pcireg_t data;
	u_int32_t reg;
	int s;
	int daddr = 0;
	faultbuf env;
	void *oldh;

	if (offset & 3 || offset < 0 || offset >= 0x100) {
#ifdef DEBUG_CONFIG
		printf ("pci_conf_read: bad reg %x\n", offset);
#endif
		return (~0);
	}

	reg = mpc_gen_config_reg(cpv, tag, offset);
	/* if invalid tag, return -1 */
	if (reg == 0xffffffff) {
		return (~0);
	}

	if ((cp->config_type & 2) && (offset & 0x04)) {
		daddr += 4;
	}

	s = splhigh();

	oldh = curpcb->pcb_onfault;
	if (setfault(&env)) {
		/* did we fault during the read? */
		curpcb->pcb_onfault = oldh;
		return (~0);
	}

	bus_space_write_4(cp->lc_iot, cp->ioh_cf8, 0, reg);
	bus_space_read_4(cp->lc_iot, cp->ioh_cf8, 0); /* XXX */
	data = bus_space_read_4(cp->lc_iot, cp->ioh_cfc, daddr);
	bus_space_write_4(cp->lc_iot, cp->ioh_cf8, 0, 0); /* disable */
	bus_space_read_4(cp->lc_iot, cp->ioh_cf8, 0); /* XXX */

	curpcb->pcb_onfault = oldh;

	splx(s);
#ifdef DEBUG_CONFIG
	if (!((offset == 0) && (data == 0xffffffff))) {
		unsigned int bus, dev, fcn;
		mpc_decompose_tag(cpv, tag, &bus, &dev, &fcn);
		printf("mpc_conf_read bus %x dev %x fcn %x offset %x", bus, dev, fcn,
				 offset);
		printf(" daddr %x reg %x",daddr, reg);
		printf(" data %x\n", data);
	}
#endif

	return (data);
}

void
mpc_conf_write(cpv, tag, offset, data)
	void *cpv;
	pcitag_t tag;
	int offset;
	pcireg_t data;
{
	struct pcibr_config *cp = cpv;
	u_int32_t reg;
	int s;
	int daddr = 0;

	reg = mpc_gen_config_reg(cpv, tag, offset);

	/* if invalid tag, return ??? */
	if (reg == 0xffffffff) {
		return;
	}
	if ((cp->config_type & 2) && (offset & 0x04))
		daddr += 4;

#ifdef DEBUG_CONFIG
	{
		unsigned int bus, dev, fcn;
		mpc_decompose_tag(cpv, tag, &bus, &dev, &fcn);
		printf("mpc_conf_write bus %x dev %x fcn %x offset %x", bus,
				 dev, fcn, offset);
		printf(" daddr %x reg %x",daddr, reg);
		printf(" data %x\n", data);
	}
#endif

	s = splhigh();

	bus_space_write_4(cp->lc_iot, cp->ioh_cf8, 0, reg);
	bus_space_read_4(cp->lc_iot, cp->ioh_cf8, 0); /* XXX */
	bus_space_write_4(cp->lc_iot, cp->ioh_cfc, daddr, data);
	bus_space_write_4(cp->lc_iot, cp->ioh_cf8, 0, 0); /* disable */
	bus_space_read_4(cp->lc_iot, cp->ioh_cf8, 0); /* XXX */

	splx(s);
}

/*ARGSUSED*/
int
mpc_intr_map(lcv, bustag, buspin, line, ihp)
	void *lcv;
	pcitag_t bustag;
	int buspin, line;
	pci_intr_handle_t *ihp;
{
	int error = 0;

	*ihp = -1;
	if (buspin == 0) {
		error = 1; /* No IRQ used. */
	} else if (buspin > 4) {
		printf("mpc_intr_map: bad interrupt pin %d\n", buspin);
		error = 1;
	}

	if (!error)
		*ihp = line;
	return error;
}

const char *
mpc_intr_string(lcv, ih)
	void *lcv;
	pci_intr_handle_t ih;
{
	static char str[16];

	snprintf(str, sizeof str, "irq %ld", ih);
	return (str);
}

int
mpc_intr_line(lcv, ih)
	void *lcv;
	pci_intr_handle_t ih;
{
	return (ih);
}

typedef void     *(intr_establish_t)(void *, pci_intr_handle_t, int, int,
    int (*func)(void *), void *, char *);
typedef void     (intr_disestablish_t)(void *, void *);
extern intr_establish_t *intr_establish_func;
extern intr_disestablish_t *intr_disestablish_func;

void *
mpc_intr_establish(lcv, ih, level, func, arg, name)
	void *lcv;
	pci_intr_handle_t ih;
	int level;
	int (*func)(void *);
	void *arg;
	const char *name;
{
	return (*intr_establish_func)(lcv, ih, IST_LEVEL, level, func, arg,
		name);
}

void
mpc_intr_disestablish(lcv, cookie)
	void *lcv, *cookie;
{
	(*intr_disestablish_func)(lcv, cookie);
}

u_int32_t
pci_iack()
{
	/* do pci IACK cycle */
	/* this should be bus allocated. */
	volatile u_int8_t *iack = ravenregs + RAVEN_PIACK;
	u_int8_t val;

	val = *iack;
	return val;
}

void
mpc_cfg_write_1(cp, reg, val)
	struct pcibr_config *cp;
	u_int32_t reg;
	u_int8_t val;
{
	int s;

	s = splhigh();
	bus_space_write_4(cp->lc_iot, cp->ioh_cf8, 0, RAVEN_REGOFFS(reg));
	bus_space_write_1(cp->lc_iot, cp->ioh_cfc, 0, val);
	splx(s);
}

void
mpc_cfg_write_2(cp, reg, val)
	struct pcibr_config *cp;
	u_int32_t reg;
	u_int16_t val;
{
	int s;

	s = splhigh();
	bus_space_write_4(cp->lc_iot, cp->ioh_cf8, 0, RAVEN_REGOFFS(reg));
	bus_space_write_2(cp->lc_iot, cp->ioh_cfc, 0, val);
	splx(s);
}

void
mpc_cfg_write_4(cp, reg, val)
	struct pcibr_config *cp;
	u_int32_t reg;
	u_int32_t val;
{
	int s;

	s = splhigh();
	bus_space_write_4(cp->lc_iot, cp->ioh_cf8, 0, RAVEN_REGOFFS(reg));
	bus_space_write_4(cp->lc_iot, cp->ioh_cfc, 0, val);
	splx(s);
}

u_int8_t
mpc_cfg_read_1(cp, reg)
	struct pcibr_config *cp;
	u_int32_t reg;
{
	u_int8_t _v_;
	int s;

	s = splhigh();
	bus_space_write_4(cp->lc_iot, cp->ioh_cf8, 0, RAVEN_REGOFFS(reg));
	_v_ = bus_space_read_1(cp->lc_iot, cp->ioh_cfc, 0);
	splx(s);
	return (_v_);
}

u_int16_t
mpc_cfg_read_2(cp, reg)
	struct pcibr_config *cp;
	u_int32_t reg;
{
	u_int16_t _v_;
	int s;

	s = splhigh();
	bus_space_write_4(cp->lc_iot, cp->ioh_cf8, 0, RAVEN_REGOFFS(reg));
	_v_ = bus_space_read_2(cp->lc_iot, cp->ioh_cfc, 0);
	splx(s);
	return (_v_);
}

u_int32_t
mpc_cfg_read_4(cp, reg)
	struct pcibr_config *cp;
	u_int32_t reg;
{
	u_int32_t _v_;
	int s;

	s = splhigh();
	bus_space_write_4(cp->lc_iot, cp->ioh_cf8, 0, RAVEN_REGOFFS(reg));
	_v_ = bus_space_read_4(cp->lc_iot, cp->ioh_cfc, 0);
	splx(s);
	return (_v_);
}
