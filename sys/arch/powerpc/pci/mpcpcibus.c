/*	$OpenBSD: mpcpcibus.c,v 1.12 2000/01/22 03:55:40 rahnds Exp $ */

/*
 * Copyright (c) 1997 Per Fogelstrom
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
 * MPC106  PCI BUS Bridge driver.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <vm/vm.h>

#include <machine/autoconf.h>
#include <machine/bat.h>

#if 0
#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>
#endif

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <powerpc/pci/pcibrvar.h>
#include <powerpc/pci/mpc106reg.h>

extern vm_map_t phys_map;
extern ofw_eth_addr[];

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
void     *mpc_intr_establish __P((void *, pci_intr_handle_t,
            int, int (*func)(void *), void *, char *));
void     mpc_intr_disestablish __P((void *, void *));
int      mpc_ether_hw_addr __P((u_int8_t *, u_int8_t, u_int8_t));

struct cfattach mpcpcibr_ca = {
        sizeof(struct pcibr_softc), mpcpcibrmatch, mpcpcibrattach,
};

struct cfdriver mpcpcibr_cd = {
	NULL, "mpcpcibr", DV_DULL,
};

static int      mpcpcibrprint __P((void *, const char *pnp));

/* ick, static variables */
static struct mpc_pci_io {
	bus_space_tag_t	iot;
	bus_space_handle_t	ioh_cf8;
	bus_space_handle_t	ioh_cfc;
} mpc_io;

struct pcibr_config mpc_config;

/*
 * Code from "pci/if_de.c" used to calculate crc32 of ether rom data.
 */
#define      TULIP_CRC32_POLY  0xEDB88320UL
static __inline__ unsigned
srom_crc32(
    const unsigned char *databuf,
    size_t datalen)
{
    u_int idx, bit, data, crc = 0xFFFFFFFFUL;

    for (idx = 0; idx < datalen; idx++)
        for (data = *databuf++, bit = 0; bit < 8; bit++, data >>= 1)
            crc = (crc >> 1) ^ (((crc ^ data) & 1) ? TULIP_CRC32_POLY : 0);
    return crc;
}

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

	if (strcmp(ca->ca_name, mpcpcibr_cd.cd_name) != 0)
		return (found);

	handle = ppc_open_pci_bridge();
	if (handle != 0) {
		err = OF_call_method("config-l@", handle, 1, 1,
			0x80000000, &val);
		if (err == 0) {
			switch (val) {
			/* supported ppc-pci bridges */
			case (PCI_VENDOR_MOT | ( PCI_PRODUCT_MOT_MPC105 <<16)):
			case (PCI_VENDOR_MOT | ( PCI_PRODUCT_MOT_MPC106 <<16)):
				found = 1;
				break;
			default:
				found = 0;
			}

		}
	}
	ppc_close_pci_bridge(handle);

	return found;
}

int pci_map_a = 0;
void
mpcpcibrattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct pcibr_softc *sc = (struct pcibr_softc *)self;
	struct pcibr_config *lcp;
	struct pcibus_attach_args pba;
	int map;
	char *bridge;

	switch(system_type) {
	case POWER4e:
		lcp = sc->sc_pcibr = &mpc_config;

		addbatmap(MPC106_V_PCI_MEM_SPACE,
			  MPC106_P_PCI_MEM_SPACE, BAT_I);

		sc->sc_membus_space.bus_base = MPC106_V_PCI_MEM_SPACE;
		sc->sc_membus_space.bus_reverse = 1;
		sc->sc_iobus_space.bus_base = MPC106_V_PCI_IO_SPACE;
		sc->sc_iobus_space.bus_reverse = 1;

		lcp->lc_pc.pc_conf_v = lcp;
		lcp->lc_pc.pc_attach_hook = mpc_attach_hook;
		lcp->lc_pc.pc_bus_maxdevs = mpc_bus_maxdevs;
		lcp->lc_pc.pc_make_tag = mpc_make_tag;
		lcp->lc_pc.pc_decompose_tag = mpc_decompose_tag;
		lcp->lc_pc.pc_conf_read = mpc_conf_read;
		lcp->lc_pc.pc_conf_write = mpc_conf_write;
		lcp->lc_pc.pc_ether_hw_addr = mpc_ether_hw_addr;

	        lcp->lc_pc.pc_intr_v = lcp;
		lcp->lc_pc.pc_intr_map = mpc_intr_map;
		lcp->lc_pc.pc_intr_string = mpc_intr_string;
		lcp->lc_pc.pc_intr_establish = mpc_intr_establish;
		lcp->lc_pc.pc_intr_disestablish = mpc_intr_disestablish;

		printf(": MPC106, Revision %x.\n", 0);
#if 0
				mpc_cfg_read_1(sc->sc_iobus_space, sc->ioh_cf8,
					sc->ioh_cfc, MPC106_PCI_REVID));
		mpc_cfg_write_2(sc->sc_iobus_space, sc->ioh_cf8, sc->ioh_cfc,
			MPC106_PCI_STAT, 0xff80); /* Reset status */
#endif
		bridge = "MPC106";
		break;

	case OFWMACH:
	case PWRSTK:
	case APPL:
		lcp = sc->sc_pcibr = &mpc_config;
		{
			int handle; 
			int err;
			unsigned int val;
			handle = ppc_open_pci_bridge();
			/* if open fails something odd has happened,
			 * we did this before during probe...
			 */
			err = OF_call_method("config-l@", handle, 1, 1,
				0x80000000, &val);
			if (err == 0) {
				switch (val) {
				/* supported ppc-pci bridges */
				case (PCI_VENDOR_MOT | ( PCI_PRODUCT_MOT_MPC105 <<16)):
					bridge = "MPC105";
					break;
				case (PCI_VENDOR_MOT | ( PCI_PRODUCT_MOT_MPC106 <<16)):
					bridge = "MPC106";
					break;
				default:
					;
				}

			}
			
			/* read the PICR1 register to find what 
			 * address map is being used
			 */
			err = OF_call_method("config-l@", handle, 1, 1,
				0x800000a8, &val);
			if (val & 0x00010000) {
				map = 1; /* map A */
				pci_map_a = 1;
			} else {
				map = 0; /* map B */
				pci_map_a = 0;
			}

			ppc_close_pci_bridge(handle);
		}


		if (map == 1) {
			sc->sc_membus_space.bus_base = MPC106_P_PCI_MEM_SPACE;
			sc->sc_membus_space.bus_reverse = 1;
			sc->sc_iobus_space.bus_base = MPC106_P_PCI_IO_SPACE;
			sc->sc_iobus_space.bus_reverse = 1;
			if ( bus_space_map(&(sc->sc_iobus_space), 0, NBPG, 0,
				&sc->ioh_cf8) != 0 )
			{
				panic("mpcpcibus: unable to map self\n");
			}
			sc->ioh_cfc = sc->ioh_cf8;
		} else {
			sc->sc_membus_space.bus_base =
				MPC106_P_PCI_MEM_SPACE_MAP_B;
			sc->sc_membus_space.bus_reverse = 1;
			sc->sc_iobus_space.bus_base =
				MPC106_P_PCI_IO_SPACE_MAP_B;
			sc->sc_iobus_space.bus_reverse = 1;
			if ( bus_space_map(&(sc->sc_iobus_space), 0xfec00000,
				NBPG, 0, &sc->ioh_cf8) != 0 )
			{
				panic("mpcpcibus: unable to map self\n");
			}
			if ( bus_space_map(&(sc->sc_iobus_space), 0xfee00000,
				NBPG, 0, &sc->ioh_cfc) != 0 )
			{
				panic("mpcpcibus: unable to map self\n");
			}
		}

		mpc_io.ioh_cf8 = sc->ioh_cf8;
		mpc_io.ioh_cfc = sc->ioh_cfc;
		lcp->lc_pc.pc_conf_v = lcp;
		lcp->lc_pc.pc_attach_hook = mpc_attach_hook;
		lcp->lc_pc.pc_bus_maxdevs = mpc_bus_maxdevs;
		lcp->lc_pc.pc_make_tag = mpc_make_tag;
		lcp->lc_pc.pc_decompose_tag = mpc_decompose_tag;
		lcp->lc_pc.pc_conf_read = mpc_conf_read;
		lcp->lc_pc.pc_conf_write = mpc_conf_write;
		lcp->lc_pc.pc_ether_hw_addr = mpc_ether_hw_addr;

	        lcp->lc_pc.pc_intr_v = lcp;
		lcp->lc_pc.pc_intr_map = mpc_intr_map;
		lcp->lc_pc.pc_intr_string = mpc_intr_string;
		lcp->lc_pc.pc_intr_establish = mpc_intr_establish;
		lcp->lc_pc.pc_intr_disestablish = mpc_intr_disestablish;


		printf(": %s, Revision %x. ", bridge, 
			mpc_cfg_read_1(&sc->sc_iobus_space, sc->ioh_cf8,
				sc->ioh_cfc, MPC106_PCI_REVID));
		if (map == 1) {
			printf("Using Map A\n");
		} else  {
			printf("Using Map B\n");
		}
#if 0
		mpc_cfg_write_2(sc->sc_iobus_space, sc->ioh_cf8, sc->ioh_cfc,
			MPC106_PCI_STAT, 0xff80); /* Reset status */
#endif
		break;

	default:
		printf("unknown system_type %d\n",system_type);
		return;
	}

	pba.pba_busname = "pci";
	pba.pba_iot = &sc->sc_iobus_space;
	pba.pba_memt = &sc->sc_membus_space;
	pba.pba_pc = &lcp->lc_pc;
	pba.pba_bus = 0;
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
 *  Get PCI physical address from given viritual address.
 *  XXX Note that cross page boundarys are *not* garantueed to work!
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
	return(pa | ((pci_map_a == 1) ? MPC106_PCI_CPUMEM : 0 ));
}

void
mpc_attach_hook(parent, self, pba)
	struct device *parent, *self;
	struct pcibus_attach_args *pba;
{
}

int
mpc_ether_hw_addr(p, b, s)
	u_int8_t *p, b, s;
{
	int i;

	for(i = 0; i < 128; i++)
		p[i] = 0x00;
	p[18] = 0x03;	/* Srom version. */
	p[19] = 0x01;	/* One chip. */
	/* Next six, ethernet address. */
	bcopy(ofw_eth_addr, &p[20], 6);

	p[26] = 0x00;	/* Chip 0 device number */
	p[27] = 30;		/* Descriptor offset */
	p[28] = 00;
	p[29] = 00;		/* MBZ */
					/* Descriptor */
	p[30] = 0x00;	/* Autosense. */
	p[31] = 0x08;
	p[32] = 0xff;	/* GP cntrl */
	p[33] = 0x01;	/* Block cnt */
#define GPR_LEN 0
#define	RES_LEN 0
	p[34] = 0x80 + 12 + GPR_LEN + RES_LEN;
	p[35] = 0x01;	/* MII PHY type */
	p[36] = 0x00;	/* PHY number 0 */
	p[37] = 0x00;	/* GPR Length */
	p[38] = 0x00;	/* Reset Length */
	p[39] = 0x00;	/* Media capabilities */
	p[40] = 0x78;	/* Media capabilities */
	p[41] = 0x00;	/* Autoneg advertisment */
	p[42] = 0x78;	/* Autoneg advertisment */
	p[43] = 0x00;	/* Full duplex map */
	p[44] = 0x50;	/* Full duplex map */
	p[45] = 0x00;	/* Treshold map */
	p[46] = 0x18;	/* Treshold map */

	i = (srom_crc32(p, 126) & 0xFFFF) ^ 0xFFFF;
	p[126] = i;
	p[127] = i >> 8;
	return(1);
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


pcireg_t
mpc_conf_read(cpv, tag, offset)
	void *cpv;
	pcitag_t tag;
	int offset;
{
	pcireg_t data;
	u_int32_t reg;
	int device;
	int s;
	int handle; 

	if(offset & 3 || offset < 0 || offset >= 0x100) {
		printf ("pci_conf_read: bad reg %x\n", offset);
		return(~0);
	}

#if 0
	printf("mpc_conf_read tag %x offset %x: ", tag, offset);
#endif

	reg =  0x80000000 | tag  | offset;

	s = splhigh();

	bus_space_write_4(mpc_io.iot, mpc_io.ioh_cf8, 0xcf8, reg);
	data = bus_space_read_4(mpc_io.iot, mpc_io.ioh_cfc, 0xcfc);

	splx(s);
#if 0
	printf("data %x\n", data);
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
	u_int32_t reg;
	int s;
	int handle; 

#if 0
	printf("mpc_conf_write tag %x offset %x data %x\n", tag, offset, data);
#endif

	reg = 0x80000000 | tag | offset;

	s = splhigh();

	bus_space_write_4(mpc_io.iot, mpc_io.ioh_cf8, 0xcf8, reg);
	bus_space_write_4(mpc_io.iot, mpc_io.ioh_cfc, 0xcfc, data);

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

#if 0
	/* this hack belongs elsewhere */
	if(system_type == POWER4e) {
		pci_decompose_tag(pc, bustag, NULL, &device, NULL);
		route = in32rb(MPC106_PCI_CONF_SPACE + 0x860);
		switch(device) {
		case 1:			/* SCSI */
			line = 6;
			route &= ~0x0000ff00;
			route |= line << 8;
			break;

		case 2:			/* Ethernet */
			line = 14;
			route &= ~0x00ff0000;
			route |= line << 16;
			break;

		case 3:			/* Tundra VME */
			line = 15;
			route &= ~0xff000000;
			route |= line << 24;
			break;

		case 4:			/* PMC Slot */
			line = 9;
			route &= ~0x000000ff;
			route |= line;
			break;

		default:
			printf("mpc_intr_map: bad dev slot %d!\n", device);
			error = 1;
			break;
		}

		lvl = isa_inb(0x04d0);
		lvl |= isa_inb(0x04d1) << 8;
		lvl |= 1L << line;
		isa_outb(0x04d0, lvl);
		isa_outb(0x04d1, lvl >> 8);
		out32rb(MPC106_PCI_CONF_SPACE + 0x860, route);
	}
#endif

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

typedef void     *(intr_establish_t) __P((void *, pci_intr_handle_t,
            int, int (*func)(void *), void *, char *));
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
	printf("mpc_pintr_establish called for [%s]\n", name);
	return (*intr_establish_func)(lcv, ih, level, func, arg, name);
#if 0
	return isabr_intr_establish(NULL, ih, IST_LEVEL, level, func, arg,
		name);
#endif
}

void
mpc_intr_disestablish(lcv, cookie)
	void *lcv, *cookie;
{
	/* XXX We should probably do something clever here.... later */
}

#if 0
void
mpc_print_pci_stat()
{
	u_int32_t stat;

	stat = mpc_cfg_read_4(sc->sc_iobus_space, sc->ioh_cf8, sc->ioh_cfc,
		MPC106_PCI_CMD);
	printf("pci: status 0x%08x.\n", stat);
	stat = mpc_cfg_read_2(sc->sc_iobus_space, sc->ioh_cf8, sc->ioh_cfc,
		MPC106_PCI_STAT);
	printf("pci: status 0x%04x.\n", stat);
}
#endif
u_int32_t
pci_iack()
{
	/* do pci IACK cycle */
	/* this should be bus allocated. */
	volatile u_int8_t *iack = (u_int8_t *)0xbffffff0;
	u_int8_t val;

	val = *iack;
	return val;
}

void
mpc_cfg_write_1(iot, ioh_cf8, ioh_cfc, reg, val)
	bus_space_tag_t iot;
	bus_space_handle_t ioh_cf8;
	bus_space_handle_t ioh_cfc;
	u_int32_t reg;
	u_int8_t val;
{
	int s;
	s = splhigh();
	bus_space_write_4(mpc_io.iot, ioh_cf8, 0xcf8,
		MPC106_REGOFFS(reg));
	bus_space_write_1(mpc_io.iot, ioh_cfc, 0xcfc, val);
	splx(s);
}

void
mpc_cfg_write_2(iot, ioh_cf8, ioh_cfc, reg, val)
	bus_space_tag_t iot;
	bus_space_handle_t ioh_cf8;
	bus_space_handle_t ioh_cfc;
	u_int32_t reg;
	u_int16_t val;
{
	int s;
	s = splhigh();
	bus_space_write_4(mpc_io.iot, ioh_cf8, 0xcf8, MPC106_REGOFFS(reg));
	bus_space_write_2(mpc_io.iot, ioh_cfc, 0xcfc, val);
	splx(s);
}

void
mpc_cfg_write_4(iot, ioh_cf8, ioh_cfc, reg, val)
	bus_space_tag_t iot;
	bus_space_handle_t ioh_cf8;
	bus_space_handle_t ioh_cfc;
	u_int32_t reg;
	u_int32_t val;
{

	int s;
	s = splhigh();
	bus_space_write_4(mpc_io.iot, ioh_cf8, 0xcf8, MPC106_REGOFFS(reg));
	bus_space_write_4(mpc_io.iot, ioh_cfc, 0xcfc, val);
	splx(s);
}

u_int8_t
mpc_cfg_read_1(iot, ioh_cf8, ioh_cfc, reg)
	bus_space_tag_t iot;
	bus_space_handle_t ioh_cf8;
	bus_space_handle_t ioh_cfc;
	u_int32_t reg;
{
	u_int8_t _v_;

	int s;
	s = splhigh();
	bus_space_write_4(mpc_io.iot, ioh_cf8, 0xcf8, MPC106_REGOFFS(reg));
	_v_ = bus_space_read_1(mpc_io.iot, ioh_cfc, 0xcfc);
	splx(s);
	return(_v_);
}

u_int16_t
mpc_cfg_read_2(iot, ioh_cf8, ioh_cfc, reg)
	bus_space_tag_t iot;
	bus_space_handle_t ioh_cf8;
	bus_space_handle_t ioh_cfc;
	u_int32_t reg;
{
	u_int16_t _v_;

	int s;
	s = splhigh();
	bus_space_write_4(mpc_io.iot, ioh_cf8, 0xcf8, MPC106_REGOFFS(reg));
	_v_ = bus_space_read_2(mpc_io.iot, ioh_cfc, 0xcfc);
	splx(s);
	return(_v_);
}

u_int32_t
mpc_cfg_read_4(iot, ioh_cf8, ioh_cfc, reg)
	bus_space_tag_t iot;
	bus_space_handle_t ioh_cf8;
	bus_space_handle_t ioh_cfc;
	u_int32_t reg;
{
	u_int32_t _v_;

	int s;
	s = splhigh();
	bus_space_write_4(mpc_io.iot, ioh_cf8, 0xcf8, MPC106_REGOFFS(reg));
	_v_ = bus_space_read_4(mpc_io.iot, ioh_cfc, 0xcfc);
	splx(s);
	return(_v_);
}
