/*	$OpenBSD: raven.c,v 1.5 2001/11/06 19:53:15 miod Exp $ */

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
#include <machine/bat.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <mvmeppc/pci/pcibrvar.h>
#include <mvmeppc/pci/ravenreg.h>

int	 mpcpcibrmatch __P((struct device *, void *, void *));
void	 mpcpcibrattach __P((struct device *, struct device *, void *));

void	 mpc_attach_hook __P((struct device *, struct device *,
				struct pcibus_attach_args *));
int	 mpc_bus_maxdevs __P((void *, int));
pcitag_t mpc_make_tag __P((void *, int, int, int));
void	 mpc_decompose_tag __P((void *, pcitag_t, int *, int *, int *));
pcireg_t mpc_conf_read __P((void *, pcitag_t, int));
void	 mpc_conf_write __P((void *, pcitag_t, int, pcireg_t));

int      mpc_intr_map __P((void *, pcitag_t, int, int, pci_intr_handle_t *));
const char *mpc_intr_string __P((void *, pci_intr_handle_t));
int	 mpc_intr_line __P((void *, pci_intr_handle_t));
void     *mpc_intr_establish __P((void *, pci_intr_handle_t,
            int, int (*func)(void *), void *, char *));
void     mpc_intr_disestablish __P((void *, void *));
int      mpc_ether_hw_addr __P((struct ppc_pci_chipset *, u_int8_t *));

struct cfattach mpcpcibr_ca = {
        sizeof(struct pcibr_softc), mpcpcibrmatch, mpcpcibrattach,
};

struct cfdriver mpcpcibr_cd = {
	NULL, "mpcpcibr", DV_DULL,
};

static int      mpcpcibrprint __P((void *, const char *pnp));

struct pcibr_config mpc_config;

/*
 * config types
 * bit meanings
 * 0 - standard cf8/cfc type configurations,
 *     sometimes the base addresses for these are different
 * 1 - Config Method #2 configuration - uni-north
 *
 * 2 - 64 bit config bus, data for accesses &4 is at daddr+4;
 */

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

int
mpcpcibrmatch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct confargs *ca = aux;
	int handle; 
	int found = 0;
	int err;
	unsigned int val;
	int qhandle;
	char type[32];

	if (strcmp(ca->ca_name, mpcpcibr_cd.cd_name) != 0)
		return (found);

	found = 1;

	return found;
}

int pci_map_a = 0;
void
mpcpcibrattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct pcibr_softc *sc = (struct pcibr_softc *)self;
	struct confargs *ca = aux;
	struct pcibr_config *lcp;
	struct pcibus_attach_args pba;
	int map, node;
	char *bridge;
	int of_node = 0;

	lcp = sc->sc_pcibr = &mpc_config;

	addbatmap(RAVEN_V_PCI_MEM_SPACE, RAVEN_P_PCI_MEM_SPACE, BAT_I);

	sc->sc_membus_space.bus_base = RAVEN_V_PCI_MEM_SPACE;
	sc->sc_membus_space.bus_reverse = 1;
	sc->sc_iobus_space.bus_base = RAVEN_V_PCI_IO_SPACE;
	sc->sc_iobus_space.bus_reverse = 1;

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
	
	lcp->ioh_cf8 = PREP_CONFIG_ADD;
	lcp->ioh_cfc = PREP_CONFIG_DAT;
	
	lcp->config_type = 0;
        
	lcp->lc_pc.pc_intr_v = lcp;
	lcp->lc_pc.pc_intr_map = mpc_intr_map;
	lcp->lc_pc.pc_intr_string = mpc_intr_string;
	lcp->lc_pc.pc_intr_line = mpc_intr_line;
	lcp->lc_pc.pc_intr_establish = mpc_intr_establish;
	lcp->lc_pc.pc_intr_disestablish = mpc_intr_disestablish;

	printf(": RAVEN, Revision 0x%x.\n", 
	       mpc_cfg_read_1(lcp, RAVEN_PCI_REVID));
	bridge = "RAVEN";
	pba.pba_dmat = &pci_bus_dma_tag;

	pba.pba_busname = "pci";
	pba.pba_iot = &sc->sc_iobus_space;
	pba.pba_memt = &sc->sc_membus_space;
	pba.pba_pc = &lcp->lc_pc;
	pba.pba_bus = 0;
	pba.pba_flags = PCI_FLAGS_MEM_ENABLED | PCI_FLAGS_IO_ENABLED;
	/* set mem and io base addresses */
	mpc_cfg_write_4(lcp, RAVEN_PCI_MEM, RAVEN_P_PCI_MEM_SPACE);
	mpc_cfg_write_4(lcp, RAVEN_PCI_IO, RAVEN_P_PCI_IO_SPACE);
	/* enable mem and io mapping, and bus master */
	mpc_cfg_write_2(lcp, RAVEN_PCI_CMD, 
			RAVEN_CMD_IOSP|RAVEN_CMD_MEMSP|RAVEN_CMD_MASTR);

	config_found(self, &pba, mpcpcibrprint);
}
                 
static int
mpcpcibrprint(aux, pnp)
	void *aux;
	const char *pnp;
{
	struct pcibus_attach_args *pba = aux;

	if(pnp)
		printf("%s at %s", pba->pba_busname, pnp);
	printf(" bus %d", pba->pba_bus);
	return(UNCONF);
}

/*
 *  Get PCI physical address from given virtual address.
 *  XXX Note that cross page boundaries are *not* guaranteed to work!
 */

vm_offset_t
vtophys(p)
	void *p;
{
	vm_offset_t pa;
	vm_offset_t va;

	va = (vm_offset_t)p;
	if((vm_offset_t)va < VM_MIN_KERNEL_ADDRESS) {
		pa = va;
	}
	else {
		pa = pmap_extract(vm_map_pmap(phys_map), va);
	}
	return(pa | ((pci_map_a == 1) ? RAVEN_PCI_CPUMEM : 0 ));
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
	return(0);
}

int
mpc_bus_maxdevs(cpv, busno)
	void *cpv;
	int busno;
{
	return(32);
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

static u_int32_t
mpc_gen_config_reg(cpv, tag, offset)
	void *cpv;
	pcitag_t tag;
	int offset;
{
	struct pcibr_config *cp = cpv;
	unsigned int bus, dev, fcn;
	u_int32_t reg;
	/*
	static int spin = 0;
	while (spin > 85);
	spin++;
	*/

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
		/* config mechanism #2, type 0
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
	int device;
	int s;
	int handle; 
	int daddr = 0;

	if(offset & 3 || offset < 0 || offset >= 0x100) {
		printf ("pci_conf_read: bad reg %x\n", offset);
		return(~0);
	}

	reg = mpc_gen_config_reg(cpv, tag, offset);
	/* if invalid tag, return -1 */
	if (reg == 0xffffffff) {
		return 0xffffffff;
	}

	if ((cp->config_type & 2) && (offset & 0x04)) {
		daddr += 4;
	}

	s = splhigh();

	bus_space_write_4(cp->lc_iot, cp->ioh_cf8, 0, reg);
	bus_space_read_4(cp->lc_iot, cp->ioh_cf8, 0); /* XXX */
	data = bus_space_read_4(cp->lc_iot, cp->ioh_cfc, daddr);
	bus_space_write_4(cp->lc_iot, cp->ioh_cf8, 0, 0); /* disable */
	bus_space_read_4(cp->lc_iot, cp->ioh_cf8, 0); /* XXX */

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

	return(data);
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
	int handle; 
	int daddr = 0;

	reg = mpc_gen_config_reg(cpv, tag, offset);

	/* if invalid tag, return ??? */
	if (reg == 0xffffffff) {
		return;
	}
	if ((cp->config_type & 2) && (offset & 0x04)) {
		daddr += 4;
	}
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


int
mpc_intr_map(lcv, bustag, buspin, line, ihp)
	void *lcv;
	pcitag_t bustag;
	int buspin, line;
	pci_intr_handle_t *ihp;
{
	struct pcibr_config *lcp = lcv;
	pci_chipset_tag_t pc = &lcp->lc_pc; 
	int error = 0;
	int route;
	int lvl;
	int device;

	*ihp = -1;
        if (buspin == 0) {
                /* No IRQ used. */
                error = 1;
        }
        else if (buspin > 4) {
                printf("mpc_intr_map: bad interrupt pin %d\n", buspin);
                error = 1;
        }

	if(!error)
		*ihp = line;
	return error;
}

const char *
mpc_intr_string(lcv, ih)
	void *lcv;
	pci_intr_handle_t ih;
{
	static char str[16];

	sprintf(str, "irq %d", ih);
	return(str);
}

int
mpc_intr_string(lcv, ih)
	void *lcv;
	pci_intr_handle_t ih;
{
	return (ih);
}

typedef void     *(intr_establish_t) __P((void *, pci_intr_handle_t,
            int, int, int (*func)(void *), void *, char *));
typedef void     (intr_disestablish_t) __P((void *, void *));
extern intr_establish_t *intr_establish_func;
extern intr_disestablish_t *intr_disestablish_func;

void *
mpc_intr_establish(lcv, ih, level, func, arg, name)
	void *lcv;
	pci_intr_handle_t ih;
	int level;
	int (*func) __P((void *));
	void *arg;
	char *name;
{
	return (*intr_establish_func)(lcv, ih, IST_LEVEL, level, func, arg,
		name);
}

void
mpc_intr_disestablish(lcv, cookie)
	void *lcv, *cookie;
{
	/* XXX We should probably do something clever here.... later */
}

u_int32_t
pci_iack()
{
	/* do pci IACK cycle */
	/* this should be bus allocated. */
	volatile u_int8_t *iack = (u_int8_t *)RAVEN_PIACK;
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
	bus_space_write_4(cp->lc_iot, cp->ioh_cf8, 0,
		RAVEN_REGOFFS(reg));
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
	return(_v_);
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
	return(_v_);
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
	return(_v_);
}
