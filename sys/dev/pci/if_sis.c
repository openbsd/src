/*	$OpenBSD: if_sis.c,v 1.3 2000/02/15 02:28:14 jason Exp $ */
/*
 * Copyright (c) 1997, 1998, 1999
 *	Bill Paul <wpaul@ctr.columbia.edu>.  All rights reserved.
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
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/pci/if_sis.c,v 1.13 1999/09/25 17:29:01 wpaul Exp $
 */

/*
 * SiS 900/SiS 7016 fast ethernet PCI NIC driver. Datasheets are
 * available from http://www.sis.com.tw.
 *
 * Written by Bill Paul <wpaul@ee.columbia.edu>
 * Electrical Engineering Department
 * Columbia University, New York City
 */

/*
 * The SiS 900 is a fairly simple chip. It uses bus master DMA with
 * simple TX and RX descriptors of 3 longwords in size. The receiver
 * has a single perfect filter entry for the station address and a
 * 128-bit multicast hash table. The SiS 900 has a built-in MII-based
 * transceiver while the 7016 requires an external transceiver chip.
 * Both chips offer the standard bit-bang MII interface as well as
 * an enchanced PHY interface which simplifies accessing MII registers.
 *
 * The only downside to this chipset is that RX descriptors must be
 * longword aligned.
 */

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/kernel.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#endif

#include <net/if_media.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#include <vm/vm.h>              /* for vtophys */
#include <vm/pmap.h>            /* for vtophys */

#include <sys/device.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#define SIS_USEIOSPACE

#include <dev/pci/if_sisreg.h>

int sis_probe		__P((struct device *, void *, void *));
void sis_attach		__P((struct device *, struct device *, void *));
int sis_intr		__P((void *));
void sis_shutdown	__P((void *));
int sis_newbuf		__P((struct sis_softc *, struct sis_desc *,
				struct mbuf *));
int sis_encap		__P((struct sis_softc *, struct mbuf *, u_int32_t *));
void sis_rxeof		__P((struct sis_softc *));
void sis_rxeoc		__P((struct sis_softc *));
void sis_txeof		__P((struct sis_softc *));
void sis_tick		__P((void *));
void sis_start		__P((struct ifnet *));
int sis_ioctl		__P((struct ifnet *, u_long, caddr_t));
void sis_init		__P((void *));
void sis_stop		__P((struct sis_softc *));
void sis_watchdog	__P((struct ifnet *));
int sis_ifmedia_upd	__P((struct ifnet *));
void sis_ifmedia_sts	__P((struct ifnet *, struct ifmediareq *));

void sis_delay		__P((struct sis_softc *));
void sis_eeprom_idle	__P((struct sis_softc *));
void sis_eeprom_putbyte	__P((struct sis_softc *, int));
void sis_eeprom_getword	__P((struct sis_softc *, int, u_int16_t *));
void sis_read_eeprom	__P((struct sis_softc *, caddr_t, int, int, int));

int sis_miibus_readreg	__P((struct device *, int, int));
void sis_miibus_writereg	__P((struct device *, int, int, int));
void sis_miibus_statchg	__P((struct device *));

void sis_setmulti	__P((struct sis_softc *));
u_int32_t sis_calchash	__P((caddr_t));
void sis_reset		__P((struct sis_softc *));
int sis_list_rx_init	__P((struct sis_softc *));
int sis_list_tx_init	__P((struct sis_softc *));

#define SIS_SETBIT(sc, reg, x)				\
	CSR_WRITE_4(sc, reg,				\
		CSR_READ_4(sc, reg) | (x))

#define SIS_CLRBIT(sc, reg, x)				\
	CSR_WRITE_4(sc, reg,				\
		CSR_READ_4(sc, reg) & ~(x))

#define SIO_SET(x)					\
	CSR_WRITE_4(sc, SIS_EECTL, CSR_READ_4(sc, SIS_EECTL) | x)

#define SIO_CLR(x)					\
	CSR_WRITE_4(sc, SIS_EECTL, CSR_READ_4(sc, SIS_EECTL) & ~x)

void sis_delay(sc)
	struct sis_softc	*sc;
{
	int			idx;

	for (idx = (300 / 33) + 1; idx > 0; idx--)
		CSR_READ_4(sc, SIS_CSR);

	return;
}

void sis_eeprom_idle(sc)
	struct sis_softc	*sc;
{
	register int		i;

	SIO_SET(SIS_EECTL_CSEL);
	sis_delay(sc);
	SIO_SET(SIS_EECTL_CLK);
	sis_delay(sc);

	for (i = 0; i < 25; i++) {
		SIO_CLR(SIS_EECTL_CLK);
		sis_delay(sc);
		SIO_SET(SIS_EECTL_CLK);
		sis_delay(sc);
	}

	SIO_CLR(SIS_EECTL_CLK);
	sis_delay(sc);
	SIO_CLR(SIS_EECTL_CSEL);
	sis_delay(sc);
	CSR_WRITE_4(sc, SIS_EECTL, 0x00000000);

	return;
}

/*
 * Send a read command and address to the EEPROM, check for ACK.
 */
void sis_eeprom_putbyte(sc, addr)
	struct sis_softc	*sc;
	int			addr;
{
	register int		d, i;

	d = addr | SIS_EECMD_READ;

	/*
	 * Feed in each bit and stobe the clock.
	 */
	for (i = 0x400; i; i >>= 1) {
		if (d & i) {
			SIO_SET(SIS_EECTL_DIN);
		} else {
			SIO_CLR(SIS_EECTL_DIN);
		}
		sis_delay(sc);
		SIO_SET(SIS_EECTL_CLK);
		sis_delay(sc);
		SIO_CLR(SIS_EECTL_CLK);
		sis_delay(sc);
	}

	return;
}

/*
 * Read a word of data stored in the EEPROM at address 'addr.'
 */
void sis_eeprom_getword(sc, addr, dest)
	struct sis_softc	*sc;
	int			addr;
	u_int16_t		*dest;
{
	register int		i;
	u_int16_t		word = 0;

	/* Force EEPROM to idle state. */
	sis_eeprom_idle(sc);

	/* Enter EEPROM access mode. */
	sis_delay(sc);
	SIO_SET(SIS_EECTL_CSEL);
	sis_delay(sc);
	SIO_SET(SIS_EECTL_CLK);
	sis_delay(sc);

	/*
	 * Send address of word we want to read.
	 */
	sis_eeprom_putbyte(sc, addr);

	/*
	 * Start reading bits from EEPROM.
	 */
	for (i = 0x8000; i; i >>= 1) {
		SIO_SET(SIS_EECTL_CLK);
		sis_delay(sc);
		if (CSR_READ_4(sc, SIS_EECTL) & SIS_EECTL_DOUT)
			word |= i;
		sis_delay(sc);
		SIO_CLR(SIS_EECTL_CLK);
		sis_delay(sc);
	}

	/* Turn off EEPROM access mode. */
	sis_eeprom_idle(sc);

	*dest = word;

	return;
}

/*
 * Read a sequence of words from the EEPROM.
 */
void sis_read_eeprom(sc, dest, off, cnt, swap)
	struct sis_softc	*sc;
	caddr_t			dest;
	int			off;
	int			cnt;
	int			swap;
{
	int			i;
	u_int16_t		word = 0, *ptr;

	for (i = 0; i < cnt; i++) {
		sis_eeprom_getword(sc, off + i, &word);
		ptr = (u_int16_t *)(dest + (i * 2));
		if (swap)
			*ptr = ntohs(word);
		else
			*ptr = word;
	}

	return;
}

int sis_miibus_readreg(self, phy, reg)
	struct device		*self;
	int			phy, reg;
{
	struct sis_softc	*sc = (struct sis_softc *)self;
	int			i, val;

	if (sc->sis_type == SIS_TYPE_900 && phy != 0)
		return(0);

	CSR_WRITE_4(sc, SIS_PHYCTL, (phy << 11) | (reg << 6) | SIS_PHYOP_READ);
	SIS_SETBIT(sc, SIS_PHYCTL, SIS_PHYCTL_ACCESS);

	for (i = 0; i < SIS_TIMEOUT; i++) {
		if (!(CSR_READ_4(sc, SIS_PHYCTL) & SIS_PHYCTL_ACCESS))
			break;
	}

	if (i == SIS_TIMEOUT) {
		printf("sis%d: PHY failed to come ready\n", sc->sis_unit);
		return(0);
	}

	val = (CSR_READ_4(sc, SIS_PHYCTL) >> 16) & 0xFFFF;

	if (val == 0xFFFF)
		return(0);

	return(val);
}

void sis_miibus_writereg(self, phy, reg, data)
	struct device		*self;
	int			phy, reg, data;
{
	struct sis_softc	*sc = (struct sis_softc *)self;
	int			i;

	if (sc->sis_type == SIS_TYPE_900 && phy != 0)
		return;

	CSR_WRITE_4(sc, SIS_PHYCTL, (data << 16) | (phy << 11) |
	    (reg << 6) | SIS_PHYOP_WRITE);
	SIS_SETBIT(sc, SIS_PHYCTL, SIS_PHYCTL_ACCESS);

	for (i = 0; i < SIS_TIMEOUT; i++) {
		if (!(CSR_READ_4(sc, SIS_PHYCTL) & SIS_PHYCTL_ACCESS))
			break;
	}

	if (i == SIS_TIMEOUT)
		printf("sis%d: PHY failed to come ready\n", sc->sis_unit);

	return;
}

void sis_miibus_statchg(self)
	struct device		*self;
{
	struct sis_softc	*sc = (struct sis_softc *)self;
	struct mii_data		*mii;

	mii = &sc->sc_mii;

	if ((mii->mii_media_active & IFM_GMASK) == IFM_FDX) {
		SIS_SETBIT(sc, SIS_TX_CFG,
		    (SIS_TXCFG_IGN_HBEAT|SIS_TXCFG_IGN_CARR));
		SIS_SETBIT(sc, SIS_RX_CFG, SIS_RXCFG_RX_TXPKTS);
	} else {
		SIS_CLRBIT(sc, SIS_TX_CFG,
		    (SIS_TXCFG_IGN_HBEAT|SIS_TXCFG_IGN_CARR));
		SIS_CLRBIT(sc, SIS_RX_CFG, SIS_RXCFG_RX_TXPKTS);
	}

	return;
}

u_int32_t sis_calchash(addr)
	caddr_t			addr;
{
	u_int32_t		crc, carry; 
	int			i, j;
	u_int8_t		c;

	/* Compute CRC for the address value. */
	crc = 0xFFFFFFFF; /* initial value */

	for (i = 0; i < 6; i++) {
		c = *(addr + i);
		for (j = 0; j < 8; j++) {
			carry = ((crc & 0x80000000) ? 1 : 0) ^ (c & 0x01);
			crc <<= 1;
			c >>= 1;
			if (carry)
				crc = (crc ^ 0x04c11db6) | carry;
		}
	}

	/* return the filter bit position */
	return((crc >> 25) & 0x0000007F);
}

void sis_setmulti(sc)
	struct sis_softc	*sc;
{
	struct ifnet		*ifp;
	struct arpcom		*ac = &sc->arpcom;
	struct ether_multi	*enm;
	struct ether_multistep	step;
	u_int32_t		h = 0, i, filtsave;

	ifp = &sc->arpcom.ac_if;

	if (ifp->if_flags & IFF_ALLMULTI || ifp->if_flags & IFF_PROMISC) {
		SIS_SETBIT(sc, SIS_RXFILT_CTL, SIS_RXFILTCTL_ALLMULTI);
		return;
	}

	SIS_CLRBIT(sc, SIS_RXFILT_CTL, SIS_RXFILTCTL_ALLMULTI);

	filtsave = CSR_READ_4(sc, SIS_RXFILT_CTL);

	/* first, zot all the existing hash bits */
	for (i = 0; i < 8; i++) {
		CSR_WRITE_4(sc, SIS_RXFILT_CTL, (4 + ((i * 16) >> 4)) << 16);
		CSR_WRITE_4(sc, SIS_RXFILT_DATA, 0);
	}

	/* now program new ones */
	ETHER_FIRST_MULTI(step, ac, enm);
	while (enm != NULL) {
		h = sis_calchash(enm->enm_addrlo);
		CSR_WRITE_4(sc, SIS_RXFILT_CTL, (4 + (h >> 4)) << 16);
		SIS_SETBIT(sc, SIS_RXFILT_DATA, (1 << (h & 0xF)));
		ETHER_NEXT_MULTI(step, enm);
	}

	CSR_WRITE_4(sc, SIS_RXFILT_CTL, filtsave);

	return;
}

void sis_reset(sc)
	struct sis_softc	*sc;
{
	register int		i;

	SIS_SETBIT(sc, SIS_CSR, SIS_CSR_RESET);

	for (i = 0; i < SIS_TIMEOUT; i++) {
		if (!(CSR_READ_4(sc, SIS_CSR) & SIS_CSR_RESET))
			break;
	}

	if (i == SIS_TIMEOUT)
		printf("sis%d: reset never completed\n", sc->sis_unit);

	/* Wait a little while for the chip to get its brains in order. */
	DELAY(1000);
        return;
}

/*
 * Probe for an SiS chip. Check the PCI vendor and device
 * IDs against our list and return a device name if we find a match.
 */
int sis_probe(parent, match, aux)
	struct device		*parent;
	void			*match;
	void			*aux;
{
	struct pci_attach_args	*pa = (struct pci_attach_args *)aux;

	if (PCI_VENDOR(pa->pa_id) != PCI_VENDOR_SIS)
		return(0);

	switch (PCI_PRODUCT(pa->pa_id)) {
	case PCI_PRODUCT_SIS_900:
		return(1);
	case PCI_PRODUCT_SIS_7016:
		return(1);
	}

	return(0);

}

/*
 * Attach the interface. Allocate softc structures, do ifmedia
 * setup and ethernet/BPF attach.
 */
void sis_attach(parent, self, aux)
	struct device		*parent, *self;
	void			*aux;
{
	int			s;
	const char		*intrstr = NULL;
	u_int32_t		command;
	struct sis_softc	*sc = (struct sis_softc *)self;
	struct pci_attach_args	*pa = aux;
	pci_chipset_tag_t	pc = pa->pa_pc;
	pci_intr_handle_t	ih;
	struct ifnet		*ifp;
	bus_addr_t		iobase;
	bus_size_t		iosize;

	s = splimp();
	sc->sis_unit = sc->sc_dev.dv_unit;
	sc->sis_type = SIS_TYPE_900;

	/*
	 * Handle power management nonsense.
	 */
	command = pci_conf_read(pc, pa->pa_tag, SIS_PCI_CAPID) & 0x000000FF;
	if (command == 0x01) {

		command = pci_conf_read(pc, pa->pa_tag, SIS_PCI_PWRMGMTCTRL);
		if (command & SIS_PSTATE_MASK) {
			u_int32_t		iobase, membase, irq;

			/* Save important PCI config data. */
			iobase = pci_conf_read(pc, pa->pa_tag, SIS_PCI_LOIO);
			membase = pci_conf_read(pc, pa->pa_tag, SIS_PCI_LOMEM);
			irq = pci_conf_read(pc, pa->pa_tag, SIS_PCI_INTLINE);

			/* Reset the power state. */
			printf("sis%d: chip is in D%d power mode "
			"-- setting to D0\n", sc->sis_unit, command & SIS_PSTATE_MASK);
			command &= 0xFFFFFFFC;
			pci_conf_write(pc, pa->pa_tag, SIS_PCI_PWRMGMTCTRL, command);

			/* Restore PCI config data. */
			pci_conf_write(pc, pa->pa_tag, SIS_PCI_LOIO, iobase);
			pci_conf_write(pc, pa->pa_tag, SIS_PCI_LOMEM, membase);
			pci_conf_write(pc, pa->pa_tag, SIS_PCI_INTLINE, irq);
		}
	}

	/*
	 * Map control/status registers.
	 */
	command = pci_conf_read(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	command |= PCI_COMMAND_IO_ENABLE | PCI_COMMAND_MEM_ENABLE |
	    PCI_COMMAND_MASTER_ENABLE;
	pci_conf_write(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG, command);
	command = pci_conf_read(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);

#ifdef SIS_USEIOSPACE
	if (!(command & PCI_COMMAND_IO_ENABLE)) {
		printf(": failed to enable I/O ports\n");
		goto fail;
	}
	if (pci_io_find(pc, pa->pa_tag, SIS_PCI_LOIO, &iobase, &iosize)) {
		printf(": can't find I/O space\n");
		goto fail;
	}
	if (bus_space_map(pa->pa_iot, iobase, iosize, 0, &sc->sis_bhandle)) {
		printf(": can't map I/O space\n");
		goto fail;
	}
	sc->sis_btag = pa->pa_iot;
#else
	if (!(command & PCI_COMMAND_MEM_ENABLE)) {
		printf(": failed to enable memory mapping\n");
		goto fail;
	}
	if (pci_mem_find(pc, pa->pa_tag, SIS_PCI_LOMEM, &iobase, &iosize,NULL)){
		printf(": can't find mem space\n");
		goto fail;
	}
	if (bus_space_map(pa->pa_memt, iobase, iosize, 0, &sc->sis_bhandle)) {
		printf(": can't map mem space\n");
		goto fail;
	}
	sc->sis_btag = pa->pa_memt;
#endif

	/* Allocate interrupt */
	if (pci_intr_map(pc, pa->pa_intrtag, pa->pa_intrpin, pa->pa_intrline,
	    &ih)) {
		printf(": couldn't map interrupt\n");
		goto fail;
	}
	intrstr = pci_intr_string(pc, ih);
	sc->sc_ih = pci_intr_establish(pc, ih, IPL_NET, sis_intr, sc,
	    self->dv_xname);
	if (sc->sc_ih == NULL) {
		printf(": couldn't establish interrupt");
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		goto fail;
	}
	printf(": %s", intrstr);

	/* Reset the adapter. */
	sis_reset(sc);

	/*
	 * Get station address from the EEPROM.
	 */
	sis_read_eeprom(sc, (caddr_t)&sc->arpcom.ac_enaddr, SIS_EE_NODEADDR,3,0);

	printf(" address %s\n", ether_sprintf(sc->arpcom.ac_enaddr));

	sc->sis_ldata_ptr = malloc(sizeof(struct sis_list_data) + 8,
				M_DEVBUF, M_NOWAIT);
	if (sc->sis_ldata_ptr == NULL) {
		printf("%s: no memory for list buffers!\n", sc->sis_unit);
		goto fail;
	}

	sc->sis_ldata = (struct sis_list_data *)sc->sis_ldata_ptr;
	bzero(sc->sis_ldata, sizeof(struct sis_list_data));

	ifp = &sc->arpcom.ac_if;
	ifp->if_softc = sc;
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = sis_ioctl;
	ifp->if_output = ether_output;
	ifp->if_start = sis_start;
	ifp->if_watchdog = sis_watchdog;
	ifp->if_baudrate = 10000000;
	ifp->if_snd.ifq_maxlen = SIS_TX_LIST_CNT - 1;
	bcopy(sc->sc_dev.dv_xname, ifp->if_xname, IFNAMSIZ);

	sc->sc_mii.mii_ifp = ifp;
	sc->sc_mii.mii_readreg = sis_miibus_readreg;
	sc->sc_mii.mii_writereg = sis_miibus_writereg;
	sc->sc_mii.mii_statchg = sis_miibus_statchg;
	ifmedia_init(&sc->sc_mii.mii_media, 0, sis_ifmedia_upd,sis_ifmedia_sts);
	mii_phy_probe(self, &sc->sc_mii, 0xffffffff);
	if (LIST_FIRST(&sc->sc_mii.mii_phys) == NULL) {
		ifmedia_add(&sc->sc_mii.mii_media, IFM_ETHER|IFM_NONE, 0, NULL);
		ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER|IFM_NONE);
	} else
		ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER|IFM_AUTO);

	/*
	 * Call MI attach routines.
	 */
	if_attach(ifp);
	ether_ifattach(ifp);

#if NBPFILTER > 0
	bpfattach(&sc->arpcom.ac_if.if_bpf, ifp, DLT_EN10MB,
		  sizeof(struct ether_header));
#endif
	shutdownhook_establish(sis_shutdown, sc);

fail:
	splx(s);
	return;
}

/*
 * Initialize the transmit descriptors.
 */
int sis_list_tx_init(sc)
	struct sis_softc	*sc;
{
	struct sis_list_data	*ld;
	struct sis_ring_data	*cd;
	int			i;

	cd = &sc->sis_cdata;
	ld = sc->sis_ldata;

	for (i = 0; i < SIS_TX_LIST_CNT; i++) {
		if (i == (SIS_TX_LIST_CNT - 1)) {
			ld->sis_tx_list[i].sis_nextdesc =
			    &ld->sis_tx_list[0];
			ld->sis_tx_list[i].sis_next =
			    vtophys(&ld->sis_tx_list[0]);
		} else {
			ld->sis_tx_list[i].sis_nextdesc =
			    &ld->sis_tx_list[i + 1];
			ld->sis_tx_list[i].sis_next =
			    vtophys(&ld->sis_tx_list[i + 1]);
		}
		ld->sis_tx_list[i].sis_mbuf = NULL;
		ld->sis_tx_list[i].sis_ptr = 0;
		ld->sis_tx_list[i].sis_ctl = 0;
	}

	cd->sis_tx_prod = cd->sis_tx_cons = cd->sis_tx_cnt = 0;

	return(0);
}


/*
 * Initialize the RX descriptors and allocate mbufs for them. Note that
 * we arrange the descriptors in a closed ring, so that the last descriptor
 * points back to the first.
 */
int sis_list_rx_init(sc)
	struct sis_softc	*sc;
{
	struct sis_list_data	*ld;
	struct sis_ring_data	*cd;
	int			i;

	ld = sc->sis_ldata;
	cd = &sc->sis_cdata;

	for (i = 0; i < SIS_RX_LIST_CNT; i++) {
		if (sis_newbuf(sc, &ld->sis_rx_list[i], NULL) == ENOBUFS)
			return(ENOBUFS);
		if (i == (SIS_RX_LIST_CNT - 1)) {
			ld->sis_rx_list[i].sis_nextdesc =
			    &ld->sis_rx_list[0];
			ld->sis_rx_list[i].sis_next =
			    vtophys(&ld->sis_rx_list[0]);
		} else {
			ld->sis_rx_list[i].sis_nextdesc =
			    &ld->sis_rx_list[i + 1];
			ld->sis_rx_list[i].sis_next =
			    vtophys(&ld->sis_rx_list[i + 1]);
		}
	}

	cd->sis_rx_prod = 0;

	return(0);
}

/*
 * Initialize an RX descriptor and attach an MBUF cluster.
 */
int sis_newbuf(sc, c, m)
	struct sis_softc	*sc;
	struct sis_desc		*c;
	struct mbuf		*m;
{
	struct mbuf		*m_new = NULL;

	if (m == NULL) {
		MGETHDR(m_new, M_DONTWAIT, MT_DATA);
		if (m_new == NULL) {
			printf("sis%d: no memory for rx list "
			    "-- packet dropped!\n", sc->sis_unit);
			return(ENOBUFS);
		}

		MCLGET(m_new, M_DONTWAIT);
		if (!(m_new->m_flags & M_EXT)) {
			printf("sis%d: no memory for rx list "
			    "-- packet dropped!\n", sc->sis_unit);
			m_freem(m_new);
			return(ENOBUFS);
		}
		m_new->m_len = m_new->m_pkthdr.len = MCLBYTES;
	} else {
		m_new = m;
		m_new->m_len = m_new->m_pkthdr.len = MCLBYTES;
		m_new->m_data = m_new->m_ext.ext_buf;
	}

	m_adj(m_new, sizeof(u_int64_t));

	c->sis_mbuf = m_new;
	c->sis_ptr = vtophys(mtod(m_new, caddr_t));
	c->sis_ctl = SIS_RXLEN;

	return(0);
}

/*
 * A frame has been uploaded: pass the resulting mbuf chain up to
 * the higher level protocols.
 */
void sis_rxeof(sc)
	struct sis_softc	*sc;
{
        struct ether_header	*eh;
        struct mbuf		*m;
        struct ifnet		*ifp;
	struct sis_desc		*cur_rx;
	int			i, total_len = 0;
	u_int32_t		rxstat;

	ifp = &sc->arpcom.ac_if;
	i = sc->sis_cdata.sis_rx_prod;

	while(SIS_OWNDESC(&sc->sis_ldata->sis_rx_list[i])) {
		struct mbuf		*m0 = NULL;

		cur_rx = &sc->sis_ldata->sis_rx_list[i];
		rxstat = cur_rx->sis_rxstat;
		m = cur_rx->sis_mbuf;
		cur_rx->sis_mbuf = NULL;
		total_len = SIS_RXBYTES(cur_rx);
		SIS_INC(i, SIS_RX_LIST_CNT);

		/*
		 * If an error occurs, update stats, clear the
		 * status word and leave the mbuf cluster in place:
		 * it should simply get re-used next time this descriptor
	 	 * comes up in the ring.
		 */
		if (!(rxstat & SIS_CMDSTS_PKT_OK)) {
			ifp->if_ierrors++;
			if (rxstat & SIS_RXSTAT_COLL)
				ifp->if_collisions++;
			sis_newbuf(sc, cur_rx, m);
			continue;
		}

		/* No errors; receive the packet. */	
		m0 = m_devget(mtod(m, char *) - ETHER_ALIGN,
		    total_len + ETHER_ALIGN, 0, ifp, NULL);
		sis_newbuf(sc, cur_rx, m);
		if (m0 == NULL) {
			ifp->if_ierrors++;
			continue;
		}
		m_adj(m0, ETHER_ALIGN);
		m = m0;

		ifp->if_ipackets++;
		eh = mtod(m, struct ether_header *);

#if NBPFILTER > 0
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m);
#endif

		/* Remove header from mbuf and pass it on. */
		m_adj(m, sizeof(struct ether_header));
		ether_input(ifp, eh, m);
	}

	sc->sis_cdata.sis_rx_prod = i;

	return;
}

void sis_rxeoc(sc)
	struct sis_softc	*sc;
{
	sis_rxeof(sc);
	sis_init(sc);
	return;
}

/*
 * A frame was downloaded to the chip. It's safe for us to clean up
 * the list buffers.
 */

void sis_txeof(sc)
	struct sis_softc	*sc;
{
	struct sis_desc		*cur_tx = NULL;
	struct ifnet		*ifp;
	u_int32_t		idx;

	ifp = &sc->arpcom.ac_if;

	/* Clear the timeout timer. */
	ifp->if_timer = 0;

	/*
	 * Go through our tx list and free mbufs for those
	 * frames that have been transmitted.
	 */
	idx = sc->sis_cdata.sis_tx_cons;
	while (idx != sc->sis_cdata.sis_tx_prod) {
		cur_tx = &sc->sis_ldata->sis_tx_list[idx];

		if (SIS_OWNDESC(cur_tx))
			break;

		if (cur_tx->sis_ctl & SIS_CMDSTS_MORE) {
			sc->sis_cdata.sis_tx_cnt--;
			SIS_INC(idx, SIS_TX_LIST_CNT);
			continue;
		}

		if (!(cur_tx->sis_ctl & SIS_CMDSTS_PKT_OK)) {
			ifp->if_oerrors++;
			if (cur_tx->sis_txstat & SIS_TXSTAT_EXCESSCOLLS)
				ifp->if_collisions++;
			if (cur_tx->sis_txstat & SIS_TXSTAT_OUTOFWINCOLL)
				ifp->if_collisions++;
		}

		ifp->if_collisions +=
		    (cur_tx->sis_txstat & SIS_TXSTAT_COLLCNT) >> 16;

		ifp->if_opackets++;
		if (cur_tx->sis_mbuf != NULL) {
			m_freem(cur_tx->sis_mbuf);
			cur_tx->sis_mbuf = NULL;
		}

		sc->sis_cdata.sis_tx_cnt--;
		SIS_INC(idx, SIS_TX_LIST_CNT);
		ifp->if_timer = 0;
	}

	sc->sis_cdata.sis_tx_cons = idx;

	if (cur_tx != NULL)
		ifp->if_flags &= ~IFF_OACTIVE;

	return;
}

void sis_tick(xsc)
	void			*xsc;
{
	struct sis_softc	*sc = (struct sis_softc *)xsc;
	struct mii_data		*mii;
	int			s;

	s = splimp();

	mii = &sc->sc_mii;
	mii_tick(mii);
	timeout(sis_tick, sc, hz);

	splx(s);

	return;
}

int sis_intr(arg)
	void			*arg;
{
	struct sis_softc	*sc;
	struct ifnet		*ifp;
	u_int32_t		status;
	int			claimed = 0;

	sc = arg;
	ifp = &sc->arpcom.ac_if;

	/* Supress unwanted interrupts */
	if (!(ifp->if_flags & IFF_UP)) {
		sis_stop(sc);
		return claimed;
	}

	/* Disable interrupts. */
	CSR_WRITE_4(sc, SIS_IER, 0);

	for (;;) {
		/* Reading the ISR register clears all interrupts. */
		status = CSR_READ_4(sc, SIS_ISR);

		if ((status & SIS_INTRS) == 0)
			break;

		claimed = 1;

		if ((status & SIS_ISR_TX_OK) ||
		    (status & SIS_ISR_TX_ERR) ||
		    (status & SIS_ISR_TX_IDLE))
			sis_txeof(sc);

		if (status & SIS_ISR_RX_OK)
			sis_rxeof(sc);

		if ((status & SIS_ISR_RX_ERR) ||
		    (status & SIS_ISR_RX_OFLOW)) {
			sis_rxeoc(sc);
		}

		if (status & SIS_ISR_SYSERR) {
			sis_reset(sc);
			sis_init(sc);
		}
	}

	/* Re-enable interrupts. */
	CSR_WRITE_4(sc, SIS_IER, 1);

	if (ifp->if_snd.ifq_head != NULL)
		sis_start(ifp);

	return claimed;
}

/*
 * Encapsulate an mbuf chain in a descriptor by coupling the mbuf data
 * pointers to the fragment pointers.
 */
int sis_encap(sc, m_head, txidx)
	struct sis_softc	*sc;
	struct mbuf		*m_head;
	u_int32_t		*txidx;
{
	struct sis_desc		*f = NULL;
	struct mbuf		*m;
	int			frag, cur, cnt = 0;

	/*
 	 * Start packing the mbufs in this chain into
	 * the fragment pointers. Stop when we run out
 	 * of fragments or hit the end of the mbuf chain.
	 */
	m = m_head;
	cur = frag = *txidx;

	for (m = m_head; m != NULL; m = m->m_next) {
		if (m->m_len != 0) {
			if ((SIS_TX_LIST_CNT -
			    (sc->sis_cdata.sis_tx_cnt + cnt)) < 2)
				return(ENOBUFS);
			f = &sc->sis_ldata->sis_tx_list[frag];
			f->sis_ctl = SIS_CMDSTS_MORE | m->m_len;
			f->sis_ptr = vtophys(mtod(m, vm_offset_t));
			if (cnt != 0)
				f->sis_ctl |= SIS_CMDSTS_OWN;
			cur = frag;
			SIS_INC(frag, SIS_TX_LIST_CNT);
			cnt++;
		}
	}

	if (m != NULL)
		return(ENOBUFS);

	sc->sis_ldata->sis_tx_list[cur].sis_mbuf = m_head;
	sc->sis_ldata->sis_tx_list[cur].sis_ctl &= ~SIS_CMDSTS_MORE;
	sc->sis_ldata->sis_tx_list[*txidx].sis_ctl |= SIS_CMDSTS_OWN;
	sc->sis_cdata.sis_tx_cnt += cnt;
	*txidx = frag;

	return(0);
}

/*
 * Main transmit routine. To avoid having to do mbuf copies, we put pointers
 * to the mbuf data regions directly in the transmit lists. We also save a
 * copy of the pointers since the transmit list fragment pointers are
 * physical addresses.
 */

void sis_start(ifp)
	struct ifnet		*ifp;
{
	struct sis_softc	*sc;
	struct mbuf		*m_head = NULL;
	u_int32_t		idx;

	sc = ifp->if_softc;

	idx = sc->sis_cdata.sis_tx_prod;

	if (ifp->if_flags & IFF_OACTIVE)
		return;

	while(sc->sis_ldata->sis_tx_list[idx].sis_mbuf == NULL) {
		IF_DEQUEUE(&ifp->if_snd, m_head);
		if (m_head == NULL)
			break;

		if (sis_encap(sc, m_head, &idx)) {
			IF_PREPEND(&ifp->if_snd, m_head);
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}

		/*
		 * If there's a BPF listener, bounce a copy of this frame
		 * to him.
		 */
#if NBPFILTER > 0
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m_head);
#endif
	}

	/* Transmit */
	sc->sis_cdata.sis_tx_prod = idx;
	SIS_SETBIT(sc, SIS_CSR, SIS_CSR_TX_ENABLE);

	/*
	 * Set a timeout in case the chip goes out to lunch.
	 */
	ifp->if_timer = 5;

	return;
}

void sis_init(xsc)
	void			*xsc;
{
	struct sis_softc	*sc = (struct sis_softc *)xsc;
	struct ifnet		*ifp = &sc->arpcom.ac_if;
	struct mii_data		*mii;
	int			s;

	s = splimp();

	/*
	 * Cancel pending I/O and free all RX/TX buffers.
	 */
	sis_stop(sc);

	mii = &sc->sc_mii;

	/* Set MAC address */
	CSR_WRITE_4(sc, SIS_RXFILT_CTL, SIS_FILTADDR_PAR0);
	CSR_WRITE_4(sc, SIS_RXFILT_DATA,
	    ((u_int16_t *)sc->arpcom.ac_enaddr)[0]);
	CSR_WRITE_4(sc, SIS_RXFILT_CTL, SIS_FILTADDR_PAR1);
	CSR_WRITE_4(sc, SIS_RXFILT_DATA,
	    ((u_int16_t *)sc->arpcom.ac_enaddr)[1]);
	CSR_WRITE_4(sc, SIS_RXFILT_CTL, SIS_FILTADDR_PAR2);
	CSR_WRITE_4(sc, SIS_RXFILT_DATA,
	    ((u_int16_t *)sc->arpcom.ac_enaddr)[2]);

	/* Init circular RX list. */
	if (sis_list_rx_init(sc) == ENOBUFS) {
		printf("sis%d: initialization failed: no "
			"memory for rx buffers\n", sc->sis_unit);
		sis_stop(sc);
		(void)splx(s);
		return;
	}

	/*
	 * Init tx descriptors.
	 */
	sis_list_tx_init(sc);

	 /* If we want promiscuous mode, set the allframes bit. */
	if (ifp->if_flags & IFF_PROMISC) {
		SIS_SETBIT(sc, SIS_RXFILT_CTL, SIS_RXFILTCTL_ALLPHYS);
	} else {
		SIS_CLRBIT(sc, SIS_RXFILT_CTL, SIS_RXFILTCTL_ALLPHYS);
	}

	/*
	 * Set the capture broadcast bit to capture broadcast frames.
	 */
	if (ifp->if_flags & IFF_BROADCAST) {
		SIS_SETBIT(sc, SIS_RXFILT_CTL, SIS_RXFILTCTL_BROAD);
	} else {
		SIS_CLRBIT(sc, SIS_RXFILT_CTL, SIS_RXFILTCTL_BROAD);
	}

	/*
	 * Load the multicast filter.
	 */
	sis_setmulti(sc);

	/* Turn the receive filter on */
	SIS_SETBIT(sc, SIS_RXFILT_CTL, SIS_RXFILTCTL_ENABLE);

	/*
	 * Load the address of the RX and TX lists.
	 */
	CSR_WRITE_4(sc, SIS_RX_LISTPTR,
	    vtophys(&sc->sis_ldata->sis_rx_list[0]));
	CSR_WRITE_4(sc, SIS_TX_LISTPTR,
	    vtophys(&sc->sis_ldata->sis_tx_list[0]));

	/* Set RX configuration */
	CSR_WRITE_4(sc, SIS_RX_CFG, SIS_RXCFG);
	/* Set TX configuration */
	CSR_WRITE_4(sc, SIS_TX_CFG, SIS_TXCFG);

	/*
	 * Enable interrupts.
	 */
	CSR_WRITE_4(sc, SIS_IMR, SIS_INTRS);
	CSR_WRITE_4(sc, SIS_IER, 1);

	/* Enable receiver and transmitter. */
	SIS_CLRBIT(sc, SIS_CSR, SIS_CSR_TX_DISABLE|SIS_CSR_RX_DISABLE);
	SIS_SETBIT(sc, SIS_CSR, SIS_CSR_RX_ENABLE);

	mii_mediachg(mii);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	(void)splx(s);

	timeout(sis_tick, sc, hz);

	return;
}

/*
 * Set media options.
 */
int sis_ifmedia_upd(ifp)
	struct ifnet		*ifp;
{
	struct sis_softc	*sc;

	sc = ifp->if_softc;

	if (ifp->if_flags & IFF_UP)
		sis_init(sc);

	return(0);
}

/*
 * Report current media status.
 */
void sis_ifmedia_sts(ifp, ifmr)
	struct ifnet		*ifp;
	struct ifmediareq	*ifmr;
{
	struct sis_softc	*sc;
	struct mii_data		*mii;

	sc = ifp->if_softc;

	mii = &sc->sc_mii;
	mii_pollstat(mii);
	ifmr->ifm_active = mii->mii_media_active;
	ifmr->ifm_status = mii->mii_media_status;

	return;
}

int sis_ioctl(ifp, command, data)
	struct ifnet		*ifp;
	u_long			command;
	caddr_t			data;
{
	struct sis_softc	*sc = ifp->if_softc;
	struct ifreq		*ifr = (struct ifreq *) data;
	struct ifaddr		*ifa = (struct ifaddr *)data;
	struct mii_data		*mii;
	int			s, error = 0;

	s = splimp();

	if ((error = ether_ioctl(ifp, &sc->arpcom, command, data)) > 0) {
		splx(s);
		return error;
	}

	switch(command) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		switch (ifa->ifa_addr->sa_family) {
		case AF_INET:
			sis_init(sc);
			arp_ifinit(&sc->arpcom, ifa);
			break;
		default:
			sis_init(sc);
			break;
		}
		break;
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			sis_init(sc);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				sis_stop(sc);
		}
		error = 0;
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		error = (command == SIOCADDMULTI) ?
		    ether_addmulti(ifr, &sc->arpcom) :
		    ether_delmulti(ifr, &sc->arpcom);

		if (error == ENETRESET) {
			/*
			 * Multicast list has changed; set the hardware
			 * filter accordingly.
			 */
			sis_setmulti(sc);
			error = 0;
		}
		break;
	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		mii = &sc->sc_mii;
		error = ifmedia_ioctl(ifp, ifr, &mii->mii_media, command);
		break;
	default:
		error = EINVAL;
		break;
	}

	(void)splx(s);

	return(error);
}

void sis_watchdog(ifp)
	struct ifnet		*ifp;
{
	struct sis_softc	*sc;

	sc = ifp->if_softc;

	ifp->if_oerrors++;
	printf("sis%d: watchdog timeout\n", sc->sis_unit);

	sis_stop(sc);
	sis_reset(sc);
	sis_init(sc);

	if (ifp->if_snd.ifq_head != NULL)
		sis_start(ifp);

	return;
}

/*
 * Stop the adapter and free any mbufs allocated to the
 * RX and TX lists.
 */
void sis_stop(sc)
	struct sis_softc	*sc;
{
	register int		i;
	struct ifnet		*ifp;

	ifp = &sc->arpcom.ac_if;
	ifp->if_timer = 0;

	untimeout(sis_tick, sc);
	CSR_WRITE_4(sc, SIS_IER, 0);
	CSR_WRITE_4(sc, SIS_IMR, 0);
	SIS_SETBIT(sc, SIS_CSR, SIS_CSR_TX_DISABLE|SIS_CSR_RX_DISABLE);
	DELAY(1000);
	CSR_WRITE_4(sc, SIS_TX_LISTPTR, 0);
	CSR_WRITE_4(sc, SIS_RX_LISTPTR, 0);

	/*
	 * Free data in the RX lists.
	 */
	for (i = 0; i < SIS_RX_LIST_CNT; i++) {
		if (sc->sis_ldata->sis_rx_list[i].sis_mbuf != NULL) {
			m_freem(sc->sis_ldata->sis_rx_list[i].sis_mbuf);
			sc->sis_ldata->sis_rx_list[i].sis_mbuf = NULL;
		}
	}
	bzero((char *)&sc->sis_ldata->sis_rx_list,
		sizeof(sc->sis_ldata->sis_rx_list));

	/*
	 * Free the TX list buffers.
	 */
	for (i = 0; i < SIS_TX_LIST_CNT; i++) {
		if (sc->sis_ldata->sis_tx_list[i].sis_mbuf != NULL) {
			m_freem(sc->sis_ldata->sis_tx_list[i].sis_mbuf);
			sc->sis_ldata->sis_tx_list[i].sis_mbuf = NULL;
		}
	}

	bzero((char *)&sc->sis_ldata->sis_tx_list,
		sizeof(sc->sis_ldata->sis_tx_list));

	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);

	return;
}

/*
 * Stop all chip I/O so that the kernel's probe routines don't
 * get confused by errant DMAs when rebooting.
 */
void sis_shutdown(v)
	void			*v;
{
	struct sis_softc	*sc = (struct sis_softc *)v;

	sis_stop(sc);
}

struct cfattach sis_ca = {
	sizeof(struct sis_softc), sis_probe, sis_attach
};

struct cfdriver sis_cd = {
	0, "sis", DV_IFNET
};

