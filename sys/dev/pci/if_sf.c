/*	$OpenBSD: if_sf.c,v 1.23 2004/04/26 19:00:35 tedu Exp $ */
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
 * $FreeBSD: src/sys/pci/if_sf.c,v 1.23 2000/07/14 19:11:02 wpaul Exp $
 */

/*
 * Adaptec AIC-6915 "Starfire" PCI fast ethernet driver for FreeBSD.
 * Programming manual is available from:
 * ftp.adaptec.com:/pub/BBS/userguides/aic6915_pg.pdf.
 *
 * Written by Bill Paul <wpaul@ctr.columbia.edu>
 * Department of Electical Engineering
 * Columbia University, New York City
 */

/*
 * The Adaptec AIC-6915 "Starfire" is a 64-bit 10/100 PCI ethernet
 * controller designed with flexibility and reducing CPU load in mind.
 * The Starfire offers high and low priority buffer queues, a
 * producer/consumer index mechanism and several different buffer
 * queue and completion queue descriptor types. Any one of a number
 * of different driver designs can be used, depending on system and
 * OS requirements. This driver makes use of type0 transmit frame
 * descriptors (since BSD fragments packets across an mbuf chain)
 * and two RX buffer queues prioritized on size (one queue for small
 * frames that will fit into a single mbuf, another with full size
 * mbuf clusters for everything else). The producer/consumer indexes
 * and completion queues are also used.
 *
 * One downside to the Starfire has to do with alignment: buffer
 * queues must be aligned on 256-byte boundaries, and receive buffers
 * must be aligned on longword boundaries. The receive buffer alignment
 * causes problems on the Alpha platform, where the packet payload
 * should be longword aligned. There is no simple way around this.
 *
 * For receive filtering, the Starfire offers 16 perfect filter slots
 * and a 512-bit hash table.
 *
 * The Starfire has no internal transceiver, relying instead on an
 * external MII-based transceiver. Accessing registers on external
 * PHYs is done through a special register map rather than with the
 * usual bitbang MDIO method.
 *
 * Acesssing the registers on the Starfire is a little tricky. The
 * Starfire has a 512K internal register space. When programmed for
 * PCI memory mapped mode, the entire register space can be accessed
 * directly. However in I/O space mode, only 256 bytes are directly
 * mapped into PCI I/O space. The other registers can be accessed
 * indirectly using the SF_INDIRECTIO_ADDR and SF_INDIRECTIO_DATA
 * registers inside the 256-byte I/O window.
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
#include <sys/timeout.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>

#include <net/if_media.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#include <uvm/uvm_extern.h>              /* for vtophys */

#include <sys/device.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#define SF_USEIOSPACE

#include <dev/pci/if_sfreg.h>

int sf_probe(struct device *, void *, void *);
void sf_attach(struct device *, struct device *, void *);
int sf_intr(void *);
void sf_shutdown(void *);
void sf_stats_update(void *);
void sf_rxeof(struct sf_softc *);
void sf_txeof(struct sf_softc *);
int sf_encap(struct sf_softc *, struct sf_tx_bufdesc_type0 *,
				struct mbuf *);
void sf_start(struct ifnet *);
int sf_ioctl(struct ifnet *, u_long, caddr_t);
void sf_init(void *);
void sf_stop(struct sf_softc *);
void sf_watchdog(struct ifnet *);
int sf_ifmedia_upd(struct ifnet *);
void sf_ifmedia_sts(struct ifnet *, struct ifmediareq *);
void sf_reset(struct sf_softc *);
int sf_init_rx_ring(struct sf_softc *);
void sf_init_tx_ring(struct sf_softc *);
int sf_newbuf(struct sf_softc *, struct sf_rx_bufdesc_type0 *,
				struct mbuf *);
void sf_setmulti(struct sf_softc *);
int sf_setperf(struct sf_softc *, int, caddr_t);
int sf_sethash(struct sf_softc *, caddr_t, int);
#ifdef notdef
int sf_setvlan(struct sf_softc *, int, u_int32_t);
#endif

u_int8_t sf_read_eeprom(struct sf_softc *, int);
u_int32_t sf_calchash(caddr_t);

int sf_miibus_readreg(struct device *, int, int);
void sf_miibus_writereg(struct device *, int, int, int);
void sf_miibus_statchg(struct device *);

u_int32_t csr_read_4(struct sf_softc *, int);
void csr_write_4(struct sf_softc *, int, u_int32_t);

#define SF_SETBIT(sc, reg, x)	\
	csr_write_4(sc, reg, csr_read_4(sc, reg) | x)

#define SF_CLRBIT(sc, reg, x)				\
	csr_write_4(sc, reg, csr_read_4(sc, reg) & ~x)

u_int32_t csr_read_4(sc, reg)
	struct sf_softc		*sc;
	int			reg;
{
	u_int32_t		val;

#ifdef SF_USEIOSPACE
	CSR_WRITE_4(sc, SF_INDIRECTIO_ADDR, reg + SF_RMAP_INTREG_BASE);
	val = CSR_READ_4(sc, SF_INDIRECTIO_DATA);
#else
	val = CSR_READ_4(sc, (reg + SF_RMAP_INTREG_BASE));
#endif

	return(val);
}

u_int8_t sf_read_eeprom(sc, reg)
	struct sf_softc		*sc;
	int			reg;
{
	u_int8_t		val;

	val = (csr_read_4(sc, SF_EEADDR_BASE +
	    (reg & 0xFFFFFFFC)) >> (8 * (reg & 3))) & 0xFF;

	return(val);
}

void csr_write_4(sc, reg, val)
	struct sf_softc		*sc;
	int			reg;
	u_int32_t		val;
{
#ifdef SF_USEIOSPACE
	CSR_WRITE_4(sc, SF_INDIRECTIO_ADDR, reg + SF_RMAP_INTREG_BASE);
	CSR_WRITE_4(sc, SF_INDIRECTIO_DATA, val);
#else
	CSR_WRITE_4(sc, (reg + SF_RMAP_INTREG_BASE), val);
#endif
	return;
}

u_int32_t sf_calchash(addr)
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
	return(crc >> 23 & 0x1FF);
}

/*
 * Copy the address 'mac' into the perfect RX filter entry at
 * offset 'idx.' The perfect filter only has 16 entries so do
 * some sanity tests.
 */
int sf_setperf(sc, idx, mac)
	struct sf_softc		*sc;
	int			idx;
	caddr_t			mac;
{
	u_int16_t		*p;

	if (idx < 0 || idx > SF_RXFILT_PERFECT_CNT)
		return(EINVAL);

	if (mac == NULL)
		return(EINVAL);

	p = (u_int16_t *)mac;

	csr_write_4(sc, SF_RXFILT_PERFECT_BASE +
	    (idx * SF_RXFILT_PERFECT_SKIP), htons(p[2]));
	csr_write_4(sc, SF_RXFILT_PERFECT_BASE +
	    (idx * SF_RXFILT_PERFECT_SKIP) + 4, htons(p[1]));
	csr_write_4(sc, SF_RXFILT_PERFECT_BASE +
	    (idx * SF_RXFILT_PERFECT_SKIP) + 8, htons(p[0]));

	return(0);
}

/*
 * Set the bit in the 512-bit hash table that corresponds to the
 * specified mac address 'mac.' If 'prio' is nonzero, update the
 * priority hash table instead of the filter hash table.
 */
int sf_sethash(sc, mac, prio)
	struct sf_softc		*sc;
	caddr_t			mac;
	int			prio;
{
	u_int32_t		h = 0;

	if (mac == NULL)
		return(EINVAL);

	h = sf_calchash(mac);

	if (prio) {
		SF_SETBIT(sc, SF_RXFILT_HASH_BASE + SF_RXFILT_HASH_PRIOOFF +
		    (SF_RXFILT_HASH_SKIP * (h >> 4)), (1 << (h & 0xF)));
	} else {
		SF_SETBIT(sc, SF_RXFILT_HASH_BASE + SF_RXFILT_HASH_ADDROFF +
		    (SF_RXFILT_HASH_SKIP * (h >> 4)), (1 << (h & 0xF)));
	}

	return(0);
}

#ifdef notdef
/*
 * Set a VLAN tag in the receive filter.
 */
int sf_setvlan(sc, idx, vlan)
	struct sf_softc		*sc;
	int			idx;
	u_int32_t		vlan;
{
	if (idx < 0 || idx >> SF_RXFILT_HASH_CNT)
		return(EINVAL);

	csr_write_4(sc, SF_RXFILT_HASH_BASE +
	    (idx * SF_RXFILT_HASH_SKIP) + SF_RXFILT_HASH_VLANOFF, vlan);

	return(0);
}
#endif

int sf_miibus_readreg(self, phy, reg)
	struct device		*self;
	int			phy, reg;
{
	struct sf_softc		*sc = (struct sf_softc *)self;
	int			i;
	u_int32_t		val = 0;

	for (i = 0; i < SF_TIMEOUT; i++) {
		val = csr_read_4(sc, SF_PHY_REG(phy, reg));
		if (val & SF_MII_DATAVALID)
			break;
	}

	if (i == SF_TIMEOUT)
		return(0);

	if ((val & 0x0000FFFF) == 0xFFFF)
		return(0);

	return(val & 0x0000FFFF);
}

void sf_miibus_writereg(self, phy, reg, val)
	struct device		*self;
	int phy, reg, val;
{
	struct sf_softc		*sc = (struct sf_softc *)self;
	int			i;
	int			busy;

	csr_write_4(sc, SF_PHY_REG(phy, reg), val);

	for (i = 0; i < SF_TIMEOUT; i++) {
		busy = csr_read_4(sc, SF_PHY_REG(phy, reg));
		if (!(busy & SF_MII_BUSY))
			break;
	}

	return;
}

void sf_miibus_statchg(self)
	struct device		*self;
{
	struct sf_softc		*sc = (struct sf_softc *)self;
	struct mii_data		*mii;

	mii = &sc->sc_mii;

	if ((mii->mii_media_active & IFM_GMASK) == IFM_FDX) {
		SF_SETBIT(sc, SF_MACCFG_1, SF_MACCFG1_FULLDUPLEX);
		csr_write_4(sc, SF_BKTOBKIPG, SF_IPGT_FDX);
	} else {
		SF_CLRBIT(sc, SF_MACCFG_1, SF_MACCFG1_FULLDUPLEX);
		csr_write_4(sc, SF_BKTOBKIPG, SF_IPGT_HDX);
	}

	return;
}

void sf_setmulti(sc)
	struct sf_softc		*sc;
{
	struct ifnet		*ifp;
	int			i;
	struct arpcom		*ac = &sc->arpcom;
	struct ether_multi	*enm;
	struct ether_multistep	step;
	u_int8_t		dummy[] = { 0, 0, 0, 0, 0, 0 };

	ifp = &sc->arpcom.ac_if;

	/* First zot all the existing filters. */
	for (i = 1; i < SF_RXFILT_PERFECT_CNT; i++)
		sf_setperf(sc, i, (char *)&dummy);
	for (i = SF_RXFILT_HASH_BASE;
	    i < (SF_RXFILT_HASH_MAX + 1); i += 4)
		csr_write_4(sc, i, 0);
	SF_CLRBIT(sc, SF_RXFILT, SF_RXFILT_ALLMULTI);

	/* Now program new ones. */
	if (ifp->if_flags & IFF_ALLMULTI || ifp->if_flags & IFF_PROMISC) {
		SF_SETBIT(sc, SF_RXFILT, SF_RXFILT_ALLMULTI);
	} else {
		i = 1;
		/* First find the tail of the list. */
		ETHER_FIRST_MULTI(step, ac, enm);

		/* Now traverse the list backwards. */
		while (enm != NULL) {
			/* if (enm->enm_addrlo->sa_family != AF_LINK)
				continue; */
			/*
			 * Program the first 15 multicast groups
			 * into the perfect filter. For all others,
			 * use the hash table.
			 */
			if (i < SF_RXFILT_PERFECT_CNT) {
				sf_setperf(sc, i,
			LLADDR((struct sockaddr_dl *)enm->enm_addrlo));
				i++;
				continue;
			}

			sf_sethash(sc,
			    LLADDR((struct sockaddr_dl *)enm->enm_addrlo), 0);
			ETHER_NEXT_MULTI(step, enm);
		}
	}

	return;
}

/*
 * Set media options.
 */
int sf_ifmedia_upd(ifp)
	struct ifnet		*ifp;
{
	struct sf_softc		*sc;
	struct mii_data		*mii;

	sc = ifp->if_softc;
	mii = &sc->sc_mii;
	sc->sf_link = 0;
	if (mii->mii_instance) {
		struct mii_softc	*miisc;
		for (miisc = LIST_FIRST(&mii->mii_phys); miisc != NULL;
		    miisc = LIST_NEXT(miisc, mii_list))
			mii_phy_reset(miisc);
	}
	mii_mediachg(mii);

	return(0);
}

/*
 * Report current media status.
 */
void sf_ifmedia_sts(ifp, ifmr)
	struct ifnet		*ifp;
	struct ifmediareq	*ifmr;
{
	struct sf_softc		*sc;
	struct mii_data		*mii;

	sc = ifp->if_softc;
	mii = &sc->sc_mii;

	mii_pollstat(mii);
	ifmr->ifm_active = mii->mii_media_active;
	ifmr->ifm_status = mii->mii_media_status;

	return;
}

int sf_ioctl(ifp, command, data)
	struct ifnet		*ifp;
	u_long			command;
	caddr_t			data;
{
	struct sf_softc		*sc = ifp->if_softc;
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
			sf_init(sc);
			arp_ifinit(&sc->arpcom, ifa);
			break;
		default:
			sf_init(sc);
			break;
		}
		break;
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_flags & IFF_RUNNING &&
			    ifp->if_flags & IFF_PROMISC &&
			    !(sc->sf_if_flags & IFF_PROMISC)) {
				SF_SETBIT(sc, SF_RXFILT, SF_RXFILT_PROMISC);
			} else if (ifp->if_flags & IFF_RUNNING &&
			    !(ifp->if_flags & IFF_PROMISC) &&
			    sc->sf_if_flags & IFF_PROMISC) {
				SF_CLRBIT(sc, SF_RXFILT, SF_RXFILT_PROMISC);
			} else if (!(ifp->if_flags & IFF_RUNNING))
				sf_init(sc);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				sf_stop(sc);
		}
		sc->sf_if_flags = ifp->if_flags;
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
			sf_setmulti(sc);
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

	splx(s);

	return(error);
}

void sf_reset(sc)
	struct sf_softc		*sc;
{
	register int		i;

	csr_write_4(sc, SF_GEN_ETH_CTL, 0);
	SF_SETBIT(sc, SF_MACCFG_1, SF_MACCFG1_SOFTRESET);
	DELAY(1000);
	SF_CLRBIT(sc, SF_MACCFG_1, SF_MACCFG1_SOFTRESET);

	SF_SETBIT(sc, SF_PCI_DEVCFG, SF_PCIDEVCFG_RESET);

	for (i = 0; i < SF_TIMEOUT; i++) {
		DELAY(10);
		if (!(csr_read_4(sc, SF_PCI_DEVCFG) & SF_PCIDEVCFG_RESET))
			break;
	}

	if (i == SF_TIMEOUT)
		printf("%s: reset never completed!\n", sc->sc_dev.dv_xname);

	/* Wait a little while for the chip to get its brains in order. */
	DELAY(1000);
	return;
}

/*
 * Probe for an Adaptec AIC-6915 chip. Check the PCI vendor and device
 * IDs against our list and return a device name if we find a match.
 * We also check the subsystem ID so that we can identify exactly which
 * NIC has been found, if possible.
 */
int sf_probe(parent, match, aux)
	struct device		*parent;
	void			*match;
	void			*aux;
{
	struct pci_attach_args	*pa = (struct pci_attach_args *)aux;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_ADP &&
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_ADP_AIC6915)
		return(1);

	return(0);
}

/*
 * Attach the interface. Allocate softc structures, do ifmedia
 * setup and ethernet/BPF attach.
 */
void sf_attach(parent, self, aux)
	struct device		*parent, *self;
	void			*aux;
{
	int			s, i;
	const char		*intrstr = NULL;
	u_int32_t		command;
	struct sf_softc		*sc = (struct sf_softc *)self;
	struct pci_attach_args	*pa = aux;
	pci_chipset_tag_t	pc = pa->pa_pc;
	pci_intr_handle_t	ih;
	struct ifnet		*ifp;
	bus_addr_t		iobase;
	bus_size_t		iosize;

	s = splimp();

	/*
	 * Handle power management nonsense.
	 */
	command = pci_conf_read(pc, pa->pa_tag, SF_PCI_CAPID) & 0x000000FF;
	if (command == 0x01) {

		command = pci_conf_read(pc, pa->pa_tag, SF_PCI_PWRMGMTCTRL);
		if (command & SF_PSTATE_MASK) {
			u_int32_t		iobase, membase, irq;

			/* Save important PCI config data. */
			iobase = pci_conf_read(pc, pa->pa_tag, SF_PCI_LOIO);
			membase = pci_conf_read(pc, pa->pa_tag, SF_PCI_LOMEM);
			irq = pci_conf_read(pc, pa->pa_tag, SF_PCI_INTLINE);

			/* Reset the power state. */
			printf("%s: chip is in D%d power mode -- setting to D0\n",
				sc->sc_dev.dv_xname, command & SF_PSTATE_MASK);
			command &= 0xFFFFFFFC;
			pci_conf_write(pc, pa->pa_tag, SF_PCI_PWRMGMTCTRL, command);

			/* Restore PCI config data. */
			pci_conf_write(pc, pa->pa_tag, SF_PCI_LOIO, iobase);
			pci_conf_write(pc, pa->pa_tag, SF_PCI_LOMEM, membase);
			pci_conf_write(pc, pa->pa_tag, SF_PCI_INTLINE, irq);
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

#ifdef SF_USEIOSPACE
	if (!(command & PCI_COMMAND_IO_ENABLE)) {
		printf(": failed to enable I/O ports\n");
		goto fail;
	}
	if (pci_io_find(pc, pa->pa_tag, SF_PCI_LOIO, &iobase, &iosize)) {
		printf(": can't find I/O space\n");
		goto fail;
	}
	if (bus_space_map(pa->pa_iot, iobase, iosize, 0, &sc->sf_bhandle)) {
		printf(": can't map I/O space\n");
		goto fail;
	}
	sc->sf_btag = pa->pa_iot;
#else
	if (!(command & PCI_COMMAND_MEM_ENABLE)) {
		printf(": failed to enable memory mapping\n");
		goto fail;
	}
	if (pci_mem_find(pc, pa->pa_tag, SF_PCI_LOMEM, &iobase, &iosize, NULL)){
		printf(": can't find mem space\n");
		goto fail;
	}
	if (bus_space_map(pa->pa_memt, iobase, iosize, 0, &sc->sf_bhandle)) {
		printf(": can't map mem space\n");
		goto fail;
	}
	sc->sf_btag = pa->pa_memt;
#endif

	/* Allocate interrupt */
	if (pci_intr_map(pa, &ih)) {
		printf(": couldn't map interrupt\n");
		goto fail;
	}
	intrstr = pci_intr_string(pc, ih);
	sc->sc_ih = pci_intr_establish(pc, ih, IPL_NET, sf_intr, sc,
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
	sf_reset(sc);

	/*
	 * Get station address from the EEPROM.
	 */
	for (i = 0; i < ETHER_ADDR_LEN; i++)
		sc->arpcom.ac_enaddr[i] =
		    sf_read_eeprom(sc, SF_EE_NODEADDR + ETHER_ADDR_LEN - i);

	printf(" address %s\n", ether_sprintf(sc->arpcom.ac_enaddr));

	/* Allocate the descriptor queues. */
	sc->sf_ldata_ptr = malloc(sizeof(struct sf_list_data) + 8,
				M_DEVBUF, M_NOWAIT);
	if (sc->sf_ldata_ptr == NULL) {
		printf("%s: no memory for list buffers!\n", sc->sc_dev.dv_xname);
		goto fail;
	}

	sc->sf_ldata = (struct sf_list_data *)sc->sf_ldata_ptr;
	bzero(sc->sf_ldata, sizeof(struct sf_list_data));

	ifp = &sc->arpcom.ac_if;
	ifp->if_softc = sc;
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = sf_ioctl;
	ifp->if_output = ether_output;
	ifp->if_start = sf_start;
	ifp->if_watchdog = sf_watchdog;
	ifp->if_baudrate = 10000000;
	IFQ_SET_MAXLEN(&ifp->if_snd, SF_TX_DLIST_CNT - 1);
	IFQ_SET_READY(&ifp->if_snd);
	bcopy(sc->sc_dev.dv_xname, ifp->if_xname, IFNAMSIZ);

	/*
	 * Initialize our media structures and probe the MII.
	 */
	sc->sc_mii.mii_ifp = ifp;
	sc->sc_mii.mii_readreg = sf_miibus_readreg;
	sc->sc_mii.mii_writereg = sf_miibus_writereg;
	sc->sc_mii.mii_statchg = sf_miibus_statchg;
	ifmedia_init(&sc->sc_mii.mii_media, 0, sf_ifmedia_upd, sf_ifmedia_sts);
	mii_attach(self, &sc->sc_mii, 0xffffffff, MII_PHY_ANY, MII_OFFSET_ANY,
	    0);
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

	shutdownhook_establish(sf_shutdown, sc);

fail:
	splx(s);
	return;
}

int sf_init_rx_ring(sc)
	struct sf_softc		*sc;
{
	struct sf_list_data	*ld;
	int			i;

	ld = sc->sf_ldata;

	bzero((char *)ld->sf_rx_dlist_big,
	    sizeof(struct sf_rx_bufdesc_type0) * SF_RX_DLIST_CNT);
	bzero((char *)ld->sf_rx_clist,
	    sizeof(struct sf_rx_cmpdesc_type3) * SF_RX_CLIST_CNT);

	for (i = 0; i < SF_RX_DLIST_CNT; i++) {
		if (sf_newbuf(sc, &ld->sf_rx_dlist_big[i], NULL) == ENOBUFS)
			return(ENOBUFS);
	}

	return(0);
}

void sf_init_tx_ring(sc)
	struct sf_softc		*sc;
{
	struct sf_list_data	*ld;
	int			i;

	ld = sc->sf_ldata;

	bzero((char *)ld->sf_tx_dlist,
	    sizeof(struct sf_tx_bufdesc_type0) * SF_TX_DLIST_CNT);
	bzero((char *)ld->sf_tx_clist,
	    sizeof(struct sf_tx_cmpdesc_type0) * SF_TX_CLIST_CNT);

	for (i = 0; i < SF_TX_DLIST_CNT; i++)
		ld->sf_tx_dlist[i].sf_id = SF_TX_BUFDESC_ID;
	for (i = 0; i < SF_TX_CLIST_CNT; i++)
		ld->sf_tx_clist[i].sf_type = SF_TXCMPTYPE_TX;

	ld->sf_tx_dlist[SF_TX_DLIST_CNT - 1].sf_end = 1;
	sc->sf_tx_cnt = 0;

	return;
}

int sf_newbuf(sc, c, m)
	struct sf_softc		*sc;
	struct sf_rx_bufdesc_type0	*c;
	struct mbuf		*m;
{
	struct mbuf		*m_new = NULL;

	if (m == NULL) {
		MGETHDR(m_new, M_DONTWAIT, MT_DATA);
		if (m_new == NULL)
			return(ENOBUFS);

		MCLGET(m_new, M_DONTWAIT);
		if (!(m_new->m_flags & M_EXT)) {
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

	c->sf_mbuf = m_new;
	c->sf_addrlo = SF_RX_HOSTADDR(vtophys(mtod(m_new, vaddr_t)));
	c->sf_valid = 1;

	return(0);
}

/*
 * The starfire is programmed to use 'normal' mode for packet reception,
 * which means we use the consumer/producer model for both the buffer
 * descriptor queue and the completion descriptor queue. The only problem
 * with this is that it involves a lot of register accesses: we have to
 * read the RX completion consumer and producer indexes and the RX buffer
 * producer index, plus the RX completion consumer and RX buffer producer
 * indexes have to be updated. It would have been easier if Adaptec had
 * put each index in a separate register, especially given that the damn
 * NIC has a 512K register space.
 *
 * In spite of all the lovely features that Adaptec crammed into the 6915,
 * it is marred by one truly stupid design flaw, which is that receive
 * buffer addresses must be aligned on a longword boundary. This forces
 * the packet payload to be unaligned, which is suboptimal on the x86 and
 * completely unuseable on the Alpha. Our only recourse is to copy received
 * packets into properly aligned buffers before handing them off.
 */

void sf_rxeof(sc)
	struct sf_softc		*sc;
{
	struct mbuf		*m;
	struct ifnet		*ifp;
	struct sf_rx_bufdesc_type0	*desc;
	struct sf_rx_cmpdesc_type3	*cur_rx;
	u_int32_t		rxcons, rxprod;
	int			cmpprodidx, cmpconsidx, bufprodidx;

	ifp = &sc->arpcom.ac_if;

	rxcons = csr_read_4(sc, SF_CQ_CONSIDX);
	rxprod = csr_read_4(sc, SF_RXDQ_PTR_Q1);
	cmpprodidx = SF_IDX_LO(csr_read_4(sc, SF_CQ_PRODIDX));
	cmpconsidx = SF_IDX_LO(rxcons);
	bufprodidx = SF_IDX_LO(rxprod);

	while (cmpconsidx != cmpprodidx) {
		struct mbuf		*m0;

		cur_rx = &sc->sf_ldata->sf_rx_clist[cmpconsidx];
		desc = &sc->sf_ldata->sf_rx_dlist_big[cur_rx->sf_endidx];
		m = desc->sf_mbuf;
		SF_INC(cmpconsidx, SF_RX_CLIST_CNT);
		SF_INC(bufprodidx, SF_RX_DLIST_CNT);

		if (!(cur_rx->sf_status1 & SF_RXSTAT1_OK)) {
			ifp->if_ierrors++;
			sf_newbuf(sc, desc, m);
			continue;
		}

		m0 = m_devget(mtod(m, char *) - ETHER_ALIGN,
		    cur_rx->sf_len + ETHER_ALIGN, 0, ifp, NULL);
		sf_newbuf(sc, desc, m);
		if (m0 == NULL) {
			ifp->if_ierrors++;
			continue;
		}
		m_adj(m0, ETHER_ALIGN);
		m = m0;

		ifp->if_ipackets++;

#if NBPFILTER > 0
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m);
#endif

		/* pass it on. */
		ether_input_mbuf(ifp, m);
	}

	csr_write_4(sc, SF_CQ_CONSIDX,
	    (rxcons & ~SF_CQ_CONSIDX_RXQ1) | cmpconsidx);
	csr_write_4(sc, SF_RXDQ_PTR_Q1,
	    (rxprod & ~SF_RXDQ_PRODIDX) | bufprodidx);

	return;
}

/*
 * Read the transmit status from the completion queue and release
 * mbufs. Note that the buffer descriptor index in the completion
 * descriptor is an offset from the start of the transmit buffer
 * descriptor list in bytes. This is important because the manual
 * gives the impression that it should match the producer/consumer
 * index, which is the offset in 8 byte blocks.
 */
void sf_txeof(sc)
	struct sf_softc		*sc;
{
	int			txcons, cmpprodidx, cmpconsidx;
	struct sf_tx_cmpdesc_type1 *cur_cmp;
	struct sf_tx_bufdesc_type0 *cur_tx;
	struct ifnet		*ifp;

	ifp = &sc->arpcom.ac_if;

	txcons = csr_read_4(sc, SF_CQ_CONSIDX);
	cmpprodidx = SF_IDX_HI(csr_read_4(sc, SF_CQ_PRODIDX));
	cmpconsidx = SF_IDX_HI(txcons);

	while (cmpconsidx != cmpprodidx) {
		cur_cmp = &sc->sf_ldata->sf_tx_clist[cmpconsidx];
		cur_tx = &sc->sf_ldata->sf_tx_dlist[cur_cmp->sf_index >> 7];
		SF_INC(cmpconsidx, SF_TX_CLIST_CNT);

		if (cur_cmp->sf_txstat & SF_TXSTAT_TX_OK)
			ifp->if_opackets++;
		else
			ifp->if_oerrors++;

		sc->sf_tx_cnt--;
		if (cur_tx->sf_mbuf != NULL) {
			m_freem(cur_tx->sf_mbuf);
			cur_tx->sf_mbuf = NULL;
		}
	}

	ifp->if_timer = 0;
	ifp->if_flags &= ~IFF_OACTIVE;

	csr_write_4(sc, SF_CQ_CONSIDX,
	    (txcons & ~SF_CQ_CONSIDX_TXQ) |
	    ((cmpconsidx << 16) & 0xFFFF0000));

	return;
}

int sf_intr(arg)
	void			*arg;
{
	struct sf_softc		*sc;
	struct ifnet		*ifp;
	u_int32_t		status;
	int			claimed = 0;

	sc = arg;
	ifp = &sc->arpcom.ac_if;

	if (!(csr_read_4(sc, SF_ISR_SHADOW) & SF_ISR_PCIINT_ASSERTED))
		return claimed;

	/* Disable interrupts. */
	csr_write_4(sc, SF_IMR, 0x00000000);

	for (;;) {
		status = csr_read_4(sc, SF_ISR);
		if (status)
			csr_write_4(sc, SF_ISR, status);

		if (!(status & SF_INTRS))
			break;

		claimed = 1;

		if (status & SF_ISR_RXDQ1_DMADONE)
			sf_rxeof(sc);

		if (status & SF_ISR_TX_TXDONE)
			sf_txeof(sc);

		if (status & SF_ISR_ABNORMALINTR) {
			if (status & SF_ISR_STATSOFLOW) {
				timeout_del(&sc->sc_stats_tmo);
				sf_stats_update(sc);
			} else
				sf_init(sc);
		}
	}

	/* Re-enable interrupts. */
	csr_write_4(sc, SF_IMR, SF_INTRS);

	if (IFQ_IS_EMPTY(&ifp->if_snd) == 0)
		sf_start(ifp);

	return claimed;
}

void sf_init(xsc)
	void			*xsc;
{
	struct sf_softc		*sc = xsc;
	struct ifnet		*ifp = &sc->arpcom.ac_if;
	struct mii_data		*mii;
	int			i, s;

	s = splimp();

	mii = &sc->sc_mii;

	sf_stop(sc);
	sf_reset(sc);

	/* Init all the receive filter registers */
	for (i = SF_RXFILT_PERFECT_BASE;
	    i < (SF_RXFILT_HASH_MAX + 1); i += 4)
		csr_write_4(sc, i, 0);

	/* Empty stats counter registers. */
	for (i = 0; i < sizeof(struct sf_stats)/sizeof(u_int32_t); i++)
		csr_write_4(sc, SF_STATS_BASE +
		    (i + sizeof(u_int32_t)), 0);

	/* Init our MAC address */
	csr_write_4(sc, SF_PAR0, *(u_int32_t *)(&sc->arpcom.ac_enaddr[0]));
	csr_write_4(sc, SF_PAR1, *(u_int32_t *)(&sc->arpcom.ac_enaddr[4]));
	sf_setperf(sc, 0, (caddr_t)&sc->arpcom.ac_enaddr);

	if (sf_init_rx_ring(sc) == ENOBUFS) {
		printf("%s: initialization failed: no "
		    "memory for rx buffers\n", sc->sc_dev.dv_xname);
		splx(s);
		return;
	}

	sf_init_tx_ring(sc);

	csr_write_4(sc, SF_RXFILT, SF_PERFMODE_NORMAL|SF_HASHMODE_WITHVLAN);

	/* If we want promiscuous mode, set the allframes bit. */
	if (ifp->if_flags & IFF_PROMISC) {
		SF_SETBIT(sc, SF_RXFILT, SF_RXFILT_PROMISC);
	} else {
		SF_CLRBIT(sc, SF_RXFILT, SF_RXFILT_PROMISC);
	}

	if (ifp->if_flags & IFF_BROADCAST) {
		SF_SETBIT(sc, SF_RXFILT, SF_RXFILT_BROAD);
	} else {
		SF_CLRBIT(sc, SF_RXFILT, SF_RXFILT_BROAD);
	}

	/*
	 * Load the multicast filter.
	 */
	sf_setmulti(sc);

	/* Init the completion queue indexes */
	csr_write_4(sc, SF_CQ_CONSIDX, 0);
	csr_write_4(sc, SF_CQ_PRODIDX, 0);

	/* Init the RX completion queue */
	csr_write_4(sc, SF_RXCQ_CTL_1,
	    vtophys((vaddr_t)sc->sf_ldata->sf_rx_clist) & SF_RXCQ_ADDR);
	SF_SETBIT(sc, SF_RXCQ_CTL_1, SF_RXCQTYPE_3);

	/* Init RX DMA control. */
	SF_SETBIT(sc, SF_RXDMA_CTL, SF_RXDMA_REPORTBADPKTS);

	/* Init the RX buffer descriptor queue. */
	csr_write_4(sc, SF_RXDQ_ADDR_Q1,
	    vtophys((vaddr_t)sc->sf_ldata->sf_rx_dlist_big));
	csr_write_4(sc, SF_RXDQ_CTL_1, (MCLBYTES << 16) | SF_DESCSPACE_16BYTES);
	csr_write_4(sc, SF_RXDQ_PTR_Q1, SF_RX_DLIST_CNT - 1);

	/* Init the TX completion queue */
	csr_write_4(sc, SF_TXCQ_CTL,
	    vtophys((vaddr_t)sc->sf_ldata->sf_tx_clist) & SF_RXCQ_ADDR);

	/* Init the TX buffer descriptor queue. */
	csr_write_4(sc, SF_TXDQ_ADDR_HIPRIO,
		vtophys((vaddr_t)sc->sf_ldata->sf_tx_dlist));
	SF_SETBIT(sc, SF_TX_FRAMCTL, SF_TXFRMCTL_CPLAFTERTX);
	csr_write_4(sc, SF_TXDQ_CTL,
	    SF_TXBUFDESC_TYPE0|SF_TXMINSPACE_128BYTES|SF_TXSKIPLEN_8BYTES);
	SF_SETBIT(sc, SF_TXDQ_CTL, SF_TXDQCTL_NODMACMP);

	/* Enable autopadding of short TX frames. */
	SF_SETBIT(sc, SF_MACCFG_1, SF_MACCFG1_AUTOPAD);

	/* Enable interrupts. */
	csr_write_4(sc, SF_IMR, SF_INTRS);
	SF_SETBIT(sc, SF_PCI_DEVCFG, SF_PCIDEVCFG_INTR_ENB);

	/* Enable the RX and TX engines. */
	SF_SETBIT(sc, SF_GEN_ETH_CTL, SF_ETHCTL_RX_ENB|SF_ETHCTL_RXDMA_ENB);
	SF_SETBIT(sc, SF_GEN_ETH_CTL, SF_ETHCTL_TX_ENB|SF_ETHCTL_TXDMA_ENB);

	sf_ifmedia_upd(ifp);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	splx(s);

	timeout_set(&sc->sc_stats_tmo, sf_stats_update, sc);
	timeout_add(&sc->sc_stats_tmo, hz);

	return;
}

int sf_encap(sc, c, m_head)
	struct sf_softc		*sc;
	struct sf_tx_bufdesc_type0 *c;
	struct mbuf		*m_head;
{
	int			frag = 0;
	struct sf_frag		*f = NULL;
	struct mbuf		*m;

	m = m_head;

	for (m = m_head, frag = 0; m != NULL; m = m->m_next) {
		if (m->m_len != 0) {
			if (frag == SF_MAXFRAGS)
				break;
			f = &c->sf_frags[frag];
			if (frag == 0)
				f->sf_pktlen = m_head->m_pkthdr.len;
			f->sf_fraglen = m->m_len;
			f->sf_addr = vtophys(mtod(m, vaddr_t));
			frag++;
		}
	}

	if (m != NULL) {
		struct mbuf		*m_new = NULL;

		MGETHDR(m_new, M_DONTWAIT, MT_DATA);
		if (m_new == NULL) {
			printf("%s: no memory for tx list", sc->sc_dev.dv_xname);
			return(1);
		}

		if (m_head->m_pkthdr.len > MHLEN) {
			MCLGET(m_new, M_DONTWAIT);
			if (!(m_new->m_flags & M_EXT)) {
				m_freem(m_new);
				printf("%s: no memory for tx list",
				    sc->sc_dev.dv_xname);
				return(1);
			}
		}
		m_copydata(m_head, 0, m_head->m_pkthdr.len,
		    mtod(m_new, caddr_t));
		m_new->m_pkthdr.len = m_new->m_len = m_head->m_pkthdr.len;
		m_freem(m_head);
		m_head = m_new;
		f = &c->sf_frags[0];
		f->sf_fraglen = f->sf_pktlen = m_head->m_pkthdr.len;
		f->sf_addr = vtophys(mtod(m_head, vaddr_t));
		frag = 1;
	}

	c->sf_mbuf = m_head;
	c->sf_id = SF_TX_BUFDESC_ID;
	c->sf_fragcnt = frag;
	c->sf_intr = 1;
	c->sf_caltcp = 0;
	c->sf_crcen = 1;

	return(0);
}

void sf_start(ifp)
	struct ifnet		*ifp;
{
	struct sf_softc		*sc;
	struct sf_tx_bufdesc_type0 *cur_tx = NULL;
	struct mbuf		*m_head = NULL;
	int			i, txprod;

	sc = ifp->if_softc;

	if (!sc->sf_link)
		return;

	if (ifp->if_flags & IFF_OACTIVE)
		return;

	txprod = csr_read_4(sc, SF_TXDQ_PRODIDX);
	i = SF_IDX_HI(txprod) >> 4;

	while(sc->sf_ldata->sf_tx_dlist[i].sf_mbuf == NULL) {
		IFQ_DEQUEUE(&ifp->if_snd, m_head);
		if (m_head == NULL)
			break;

		cur_tx = &sc->sf_ldata->sf_tx_dlist[i];
		if (sf_encap(sc, cur_tx, m_head)) {
			m_freem(m_head);
			continue;
		}

#if NBPFILTER > 0
		/*
		 * If there's a BPF listener, bounce a copy of this frame
		 * to him.
		 */
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, cur_tx->sf_mbuf);
#endif

		SF_INC(i, SF_TX_DLIST_CNT);
		sc->sf_tx_cnt++;
		if (sc->sf_tx_cnt == (SF_TX_DLIST_CNT - 2))
			break;
	}

	if (cur_tx == NULL)
		return;

	/* Transmit */
	csr_write_4(sc, SF_TXDQ_PRODIDX,
	    (txprod & ~SF_TXDQ_PRODIDX_HIPRIO) |
	    ((i << 20) & 0xFFFF0000));

	ifp->if_timer = 5;

	return;
}

void sf_stop(sc)
	struct sf_softc		*sc;
{
	int			i;
	struct ifnet		*ifp;

	ifp = &sc->arpcom.ac_if;

	timeout_del(&sc->sc_stats_tmo);

	csr_write_4(sc, SF_GEN_ETH_CTL, 0);
	csr_write_4(sc, SF_CQ_CONSIDX, 0);
	csr_write_4(sc, SF_CQ_PRODIDX, 0);
	csr_write_4(sc, SF_RXDQ_ADDR_Q1, 0);
	csr_write_4(sc, SF_RXDQ_CTL_1, 0);
	csr_write_4(sc, SF_RXDQ_PTR_Q1, 0);
	csr_write_4(sc, SF_TXCQ_CTL, 0);
	csr_write_4(sc, SF_TXDQ_ADDR_HIPRIO, 0);
	csr_write_4(sc, SF_TXDQ_CTL, 0);
	sf_reset(sc);

	sc->sf_link = 0;

	for (i = 0; i < SF_RX_DLIST_CNT; i++) {
		if (sc->sf_ldata->sf_rx_dlist_big[i].sf_mbuf != NULL) {
			m_freem(sc->sf_ldata->sf_rx_dlist_big[i].sf_mbuf);
			sc->sf_ldata->sf_rx_dlist_big[i].sf_mbuf = NULL;
		}
	}

	for (i = 0; i < SF_TX_DLIST_CNT; i++) {
		if (sc->sf_ldata->sf_tx_dlist[i].sf_mbuf != NULL) {
			m_freem(sc->sf_ldata->sf_tx_dlist[i].sf_mbuf);
			sc->sf_ldata->sf_tx_dlist[i].sf_mbuf = NULL;
		}
	}

	ifp->if_flags &= ~(IFF_RUNNING|IFF_OACTIVE);

	return;
}

/*
 * Note: it is important that this function not be interrupted. We
 * use a two-stage register access scheme: if we are interrupted in
 * between setting the indirect address register and reading from the
 * indirect data register, the contents of the address register could
 * be changed out from under us.
 */     
void sf_stats_update(xsc)
	void			*xsc;
{
	struct sf_softc		*sc;
	struct ifnet		*ifp;
	struct mii_data		*mii;
	struct sf_stats		stats;
	u_int32_t		*ptr;
	int			i, s;

	s = splimp();

	sc = xsc;
	ifp = &sc->arpcom.ac_if;
	mii = &sc->sc_mii;

	ptr = (u_int32_t *)&stats;
	for (i = 0; i < sizeof(stats)/sizeof(u_int32_t); i++)
		ptr[i] = csr_read_4(sc, SF_STATS_BASE +
		    (i + sizeof(u_int32_t)));

	for (i = 0; i < sizeof(stats)/sizeof(u_int32_t); i++)
		csr_write_4(sc, SF_STATS_BASE +
		    (i + sizeof(u_int32_t)), 0);

	ifp->if_collisions += stats.sf_tx_single_colls +
	    stats.sf_tx_multi_colls + stats.sf_tx_excess_colls;

	mii_tick(mii);
	if (!sc->sf_link) {
		mii_pollstat(mii);
		if (mii->mii_media_status & IFM_ACTIVE &&
		    IFM_SUBTYPE(mii->mii_media_active) != IFM_NONE)
			sc->sf_link++;
		if (IFQ_IS_EMPTY(&ifp->if_snd) == 0)
			sf_start(ifp);
	}

	splx(s);

	timeout_add(&sc->sc_stats_tmo, hz);

	return;
}

void sf_watchdog(ifp)
	struct ifnet		*ifp;
{
	struct sf_softc		*sc;

	sc = ifp->if_softc;

	ifp->if_oerrors++;
	printf("%s: watchdog timeout\n", sc->sc_dev.dv_xname);

	sf_stop(sc);
	sf_reset(sc);
	sf_init(sc);

	if (IFQ_IS_EMPTY(&ifp->if_snd) == 0)
		sf_start(ifp);

	return;
}

void sf_shutdown(v)
	void			*v;
{
	struct sf_softc		*sc = (struct sf_softc *)v;
	
	sf_stop(sc);
}

struct cfattach sf_ca = {
	sizeof(struct sf_softc), sf_probe, sf_attach
};

struct cfdriver sf_cd = {
	0, "sf", DV_IFNET
};

