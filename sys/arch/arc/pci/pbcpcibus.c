/*	$OpenBSD: pbcpcibus.c,v 1.7 1998/03/25 11:52:48 pefo Exp $ */

/*
 * Copyright (c) 1997, 1998 Per Fogelstrom, Opsycon AB
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
 *	This product includes software developed under OpenBSD by
 *	Per Fogelstrom, Opsycon AB.
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
 * ARC PCI BUS Bridge driver.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <vm/vm.h>

#include <machine/autoconf.h>
#include <machine/pte.h>
#include <machine/cpu.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <mips/archtype.h>
#include <arc/algor/algor.h>
#include <arc/pci/pcibrvar.h>
#include <arc/pci/v962pcbreg.h>

extern vm_map_t phys_map;
extern char eth_hw_addr[];	/* Hardware ethernet address stored elsewhere */

int	 pbcpcibrmatch __P((struct device *, void *, void *));
void	 pbcpcibrattach __P((struct device *, struct device *, void *));

void	 pbc_attach_hook __P((struct device *, struct device *,
				struct pcibus_attach_args *));
int	 pbc_bus_maxdevs __P((void *, int));
pcitag_t pbc_make_tag __P((void *, int, int, int));
void	 pbc_decompose_tag __P((void *, pcitag_t, int *, int *, int *));
pcireg_t pbc_conf_read __P((void *, pcitag_t, int));
void	 pbc_conf_write __P((void *, pcitag_t, int, pcireg_t));

int      pbc_intr_map __P((void *, pcitag_t, int, int, pci_intr_handle_t *));
const char *pbc_intr_string __P((void *, pci_intr_handle_t));
void     *pbc_intr_establish __P((void *, pci_intr_handle_t,
            int, int (*func)(void *), void *, char *));
void     pbc_intr_disestablish __P((void *, void *));
int      pbc_ether_hw_addr __P((u_int8_t *));

struct cfattach pbcpcibr_ca = {
        sizeof(struct pcibr_softc), pbcpcibrmatch, pbcpcibrattach,
};

struct cfdriver pbcpcibr_cd = {
	NULL, "pbcpcibr", DV_DULL,
};

/*
 * Code from "pci/if_de.c" used to calculate crc32 of ether rom data.
 * Another example can be found in document EC-QPQWA-TE from DEC.
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
static int      pbcpcibrprint __P((void *, const char *pnp));

struct pcibr_config pbc_config;
static int pbc_version;

int
pbcpcibrmatch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct confargs *ca = aux;

	/* Make sure that we're looking for a PCI bridge. */
	if (strcmp(ca->ca_name, pbcpcibr_cd.cd_name) != 0)
		return (0);

	return (1);
}

void
pbcpcibrattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct pcibr_softc *sc = (struct pcibr_softc *)self;
	struct pcibr_config *lcp;
	struct pcibus_attach_args pba;

	switch(system_type) {
	case ALGOR_P4032:
	case ALGOR_P5064:
		V96X_PCI_BASE0 = V96X_PCI_BASE0 & 0xffff0000;

		lcp = sc->sc_pcibr = &pbc_config;

		sc->sc_bus_space.bus_base = V96X_PCI_MEM_SPACE;
		sc->sc_bus_space.bus_sparse1 = 0;
		sc->sc_bus_space.bus_sparse2 = 0;
		sc->sc_bus_space.bus_sparse4 = 0;
		sc->sc_bus_space.bus_sparse8 = 0;

		lcp->lc_pc.pc_conf_v = lcp;
		lcp->lc_pc.pc_attach_hook = pbc_attach_hook;
		lcp->lc_pc.pc_bus_maxdevs = pbc_bus_maxdevs;
		lcp->lc_pc.pc_make_tag = pbc_make_tag;
		lcp->lc_pc.pc_decompose_tag = pbc_decompose_tag;
		lcp->lc_pc.pc_conf_read = pbc_conf_read;
		lcp->lc_pc.pc_conf_write = pbc_conf_write;
		lcp->lc_pc.pc_ether_hw_addr = pbc_ether_hw_addr;
		lcp->lc_pc.pc_sync_cache = R4K_HitFlushDCache;

	        lcp->lc_pc.pc_intr_v = lcp;
		lcp->lc_pc.pc_intr_map = pbc_intr_map;
		lcp->lc_pc.pc_intr_string = pbc_intr_string;
		lcp->lc_pc.pc_intr_establish = pbc_intr_establish;
		lcp->lc_pc.pc_intr_disestablish = pbc_intr_disestablish;

		pbc_version = V96X_PCI_CC_REV;
		printf(": V3 V962, Revision %x.\n", pbc_version);
		break;
	}

	pba.pba_busname = "pci";
	pba.pba_iot = &sc->sc_bus_space;
	pba.pba_memt = &sc->sc_bus_space;
	pba.pba_pc = &lcp->lc_pc;
	pba.pba_bus = 0;
	config_found(self, &pba, pbcpcibrprint);

}

static int
pbcpcibrprint(aux, pnp)
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
 */

vm_offset_t
vtophysaddr(dp, p)
	struct device *dp;
	vm_offset_t p;
{
	vm_offset_t pa;
	vm_offset_t va;

	va = p;
	if(va >= UADDR) {	/* Stupid driver have buf on stack!! */
		va = (vm_offset_t)curproc->p_addr + (va & ~UADDR);
	}
	if((vm_offset_t)va < VM_MIN_KERNEL_ADDRESS) {
		pa = CACHED_TO_PHYS(va);
	}
	else {
		pa = pmap_extract(vm_map_pmap(phys_map), va);
	}
	if(dp->dv_class == DV_IFNET && pbc_version < V96X_VREV_C0) { 
					/* BUG in early V962PBC's */
		pa |= 0xc0000000;	/* Use aparture II */
	}
	return(pa);
}

void
pbc_attach_hook(parent, self, pba)
	struct device *parent, *self;
	struct pcibus_attach_args *pba;
{
}

int
pbc_bus_maxdevs(cpv, busno)
	void *cpv;
	int busno;
{
	return(16);
}

pcitag_t
pbc_make_tag(cpv, bus, dev, fnc)
	void *cpv;
	int bus, dev, fnc;
{
	return (bus << 16) | (dev << 11) | (fnc << 8);
}

void
pbc_decompose_tag(cpv, tag, busp, devp, fncp)
	void *cpv;
	pcitag_t tag;
	int *busp, *devp, *fncp;
{
	if (busp != NULL)
		*busp = (tag >> 16) & 0x7;
	if (devp != NULL)
		*devp = (tag >> 11) & 0x1f;
	if (fncp != NULL)
		*fncp = (tag >> 8) & 0x7;
}

pcireg_t
pbc_conf_read(cpv, tag, offset)
	void *cpv;
	pcitag_t tag;
	int offset;
{
	pcireg_t data;
	u_int32_t addr;
	int bus, device, func, ad_low;
	int s;

	if(offset & 3 || offset < 0 || offset >= 0x100) {
		printf ("pci_conf_read: bad reg %x\n", offset);
		return(~0);
	}
	pbc_decompose_tag(cpv, tag, &bus, &device, &func);
	ad_low = 0;

	if(system_type == ALGOR_P4032) {
		if(bus != 0 || device > 5 || func > 7) {
			return(~0);
		}
		addr = (0x800 << device) | (func << 8) | offset;
		ad_low = 0;
	}
	else {	/* P5064 */
		if(bus == 0) {
			if(device > 5 || func > 7) {
				return(~0);
			}
			addr = (1L << (device + 24)) | (func << 8) | offset;
			ad_low = 0;
		}
		else if(pbc_version >= V96X_VREV_C0) {
			if(bus > 255 || device > 15 || func > 7) {
				return(~0);
			}
			addr = (bus << 16) | (device << 11) | (func << 8);
			ad_low = V96X_LB_MAPx_AD_LOW_EN;
		}
		else {
			return(~0);
		}
	}

	s = splhigh();

	/* high 12 bits of address go in map register, and set for conf space */
	V96X_LB_MAP0 = ((addr >> 16) & V96X_LB_MAPx_MAP_ADR) | ad_low | V96X_LB_TYPE_CONF;
	/* clear aborts */
	V96X_PCI_STAT |= V96X_PCI_STAT_M_ABORT | V96X_PCI_STAT_T_ABORT;

	wbflush();

	/* low 20 bits of address are in the actual address */
	data = *(volatile pcireg_t *) (V96X_PCI_CONF_SPACE + (addr&0xfffff));

	if (V96X_PCI_STAT & V96X_PCI_STAT_M_ABORT) {
		V96X_PCI_STAT |= V96X_PCI_STAT_M_ABORT;
		return(~0);	/* Nothing there */
	}

	if (V96X_PCI_STAT & V96X_PCI_STAT_T_ABORT) {
		V96X_PCI_STAT |= V96X_PCI_STAT_T_ABORT;
		printf ("PCI slot %d: target abort!\n", device);
		return(~0);	/* Ooops! */
	}

	splx(s);
	return(data);
}

void
pbc_conf_write(cpv, tag, offset, data)
	void *cpv;
	pcitag_t tag;
	int offset;
	pcireg_t data;
{
	u_int32_t addr;
	int bus, device, func, ad_low;
	int s;

	pbc_decompose_tag(cpv, tag, &bus, &device, &func);
	ad_low = 0;

	if(system_type == ALGOR_P4032) {
		if(bus != 0 || device > 5 || func > 7) {
			return;
		}
		addr = (0x800 << device) | (func << 8) | offset;
		ad_low = 0;
	}
	else {	/* P5064 */
		if(bus == 0) {
			if(device > 5 || func > 7) {
				return;
			}
			addr = (1L << (device + 24)) | (func << 8) | offset;
			ad_low = 0;
		}
		else if(pbc_version >= V96X_VREV_C0) {
			if(bus > 255 || device > 15 || func > 7) {
				return;
			}
			addr = (bus << 16) | (device << 11) | (func << 8);
			ad_low = V96X_LB_MAPx_AD_LOW_EN;
		}
		else {
			return;
		}
	}

	s = splhigh();

	/* high 12 bits of address go in map register, and set for conf space */
	V96X_LB_MAP0 = ((addr >> 16) & V96X_LB_MAPx_MAP_ADR) | ad_low | V96X_LB_TYPE_CONF;
	/* clear aborts */
	V96X_PCI_STAT |= V96X_PCI_STAT_M_ABORT | V96X_PCI_STAT_T_ABORT;

	wbflush();

	/* low 20 bits of address are in the actual address */
	*(volatile pcireg_t *) (V96X_PCI_CONF_SPACE + (addr&0xfffff)) = data;

	/* wait for write FIFO to empty */
	do {
	} while (V96X_FIFO_STAT & V96X_FIFO_STAT_L2P_WR);

	if (V96X_PCI_STAT & V96X_PCI_STAT_M_ABORT) {
		V96X_PCI_STAT |= V96X_PCI_STAT_M_ABORT;
		printf ("PCI slot %d: conf_write: master abort\n", device);
	}

	if (V96X_PCI_STAT & V96X_PCI_STAT_T_ABORT) {
		V96X_PCI_STAT |= V96X_PCI_STAT_T_ABORT;
		printf ("PCI slot %d: conf_write: target abort!\n", device);
	}

	splx(s);
}

/*
 *	Build the serial rom info normaly stored in an EEROM on
 *	PCI DEC21x4x boards. Cheapo designs skips the rom so
 *	we do the job here. The setup is not 100% correct but
 *	close enough to make the driver happy!
 */
int
pbc_ether_hw_addr(p)
	u_int8_t *p;
{
	int i;

	for(i = 0; i < 128; i++)
		p[i] = 0x00;
	p[18] = 0x03;	/* Srom version. */
	p[19] = 0x01;	/* One chip. */
	/* Next six, ethernet address. */
	bcopy(eth_hw_addr, &p[20], 6);

	p[26] = 0x00;	/* Chip 0 device number */
	p[27] = 30;		/* Descriptor offset */
	p[28] = 00;
	p[29] = 00;		/* MBZ */
					/* Descriptor */
	p[30] = 0x00;	/* Autosense. */
	p[31] = 0x08;
	if(system_type == ALGOR_P4032 ||
	   system_type == ALGOR_P5064) {
		p[32] = 0x01;	/* Block cnt */
		p[33] = 0x02;	/* Medium type is AUI */
	}
	else {
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
	}

	i = (srom_crc32(p, 126) & 0xFFFF) ^ 0xFFFF;
	p[126] = i;
	p[127] = i >> 8;
	return(1);	/* Got it! */
}

int
pbc_intr_map(lcv, bustag, buspin, line, ihp)
	void *lcv;
	pcitag_t bustag;
	int buspin, line;
	pci_intr_handle_t *ihp;
{
	struct pcibr_config *lcp = lcv;
	pci_chipset_tag_t pc = &lcp->lc_pc;
	int device, pirq;

        if (buspin == 0) {
                /* No IRQ used. */
		*ihp = -1;
                return 1;
        }
        if (buspin > 4) {
                printf("pbc_intr_map: bad interrupt pin %d\n", buspin);
		*ihp = -1;
                return 1;
        }

	pci_decompose_tag(pc, bustag, NULL, &device, NULL);
	pirq = buspin - 1;

	switch(device) {
	case 0:				/* DC21041 */
		pirq = 9;
		break;
	case 1:				/* NCR SCSI */
		pirq = 10;
		break;
	default:
		switch (buspin) {
		case PCI_INTERRUPT_PIN_A:
			pirq = 0;
			break;
		case PCI_INTERRUPT_PIN_B:
			pirq = 1;
			break;
		case PCI_INTERRUPT_PIN_C:
			pirq = 2;
			break;
		case PCI_INTERRUPT_PIN_D:
			pirq = 3;
			break;
		}
	}
	*ihp = pirq;
	return 0;
}

const char *
pbc_intr_string(lcv, ih)
	void *lcv;
	pci_intr_handle_t ih;
{
	static char str[16];

	sprintf(str, "pciirq%d", ih);
	return(str);
}

void *
pbc_intr_establish(lcv, ih, level, func, arg, name)
	void *lcv;
	pci_intr_handle_t ih;
	int level;
	int (*func) __P((void *));
	void *arg;
	char *name;
{
	return algor_pci_intr_establish(ih, level, func, arg, name);
}

void
pbc_intr_disestablish(lcv, cookie)
	void *lcv, *cookie;
{
	algor_pci_intr_disestablish(cookie);
}
