/*	$OpenBSD: if_sq.c,v 1.13 2015/03/29 11:03:34 mpi Exp $	*/
/*	$NetBSD: if_sq.c,v 1.42 2011/07/01 18:53:47 dyoung Exp $	*/

/*
 * Copyright (c) 2001 Rafal K. Boni
 * Copyright (c) 1998, 1999, 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * Portions of this code are derived from software contributed to The
 * NetBSD Foundation by Jason R. Thorpe of the Numerical Aerospace
 * Simulation Facility, NASA Ames Research Center.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
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

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/timeout.h>
#include <sys/mbuf.h>
#include <sys/pool.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/syslog.h>

#include <uvm/uvm_extern.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <machine/autoconf.h>
#include <machine/bus.h>
#include <machine/cpu.h>	/* guarded_read_4 */
#include <machine/intr.h>
#include <mips64/arcbios.h>	/* bios_enaddr */
#include <sgi/sgi/ip22.h>

#include <dev/ic/seeq8003reg.h>

#include <sgi/hpc/hpcvar.h>
#include <sgi/hpc/hpcreg.h>
#include <sgi/hpc/iocreg.h>	/* IOC_READ / IOC_WRITE */
#include <sgi/hpc/if_sqvar.h>

/*
 * Short TODO list:
 *	(1) Do counters for bad-RX packets.
 *	(2) Allow multi-segment transmits, instead of copying to a single,
 *	    contiguous mbuf.
 *	(3) Verify sq_stop() turns off enough stuff; I was still getting
 *	    seeq interrupts after sq_stop().
 *	(5) Should the driver filter out its own transmissions in non-EDLC
 *	    mode?
 *	(6) Multicast support -- multicast filter, address management, ...
 *	(7) Deal with RB0 (recv buffer overflow) on reception.  Will need
 *	    to figure out if RB0 is read-only as stated in one spot in the
 *	    HPC spec or read-write (ie, is the 'write a one to clear it')
 *	    the correct thing?
 *
 * Note that it is no use to implement EDLC auto-padding: the HPC glue will
 * not start a packet transfer until it has been fed 64 bytes, which defeats
 * the auto-padding purpose.
 */

#ifdef SQ_DEBUG
int sq_debug = 0;
#define SQ_DPRINTF(x) do { if (sq_debug) printf x; } while (0)
#else
#define SQ_DPRINTF(x) do { } while (0)
#endif

int	sq_match(struct device *, void *, void *);
void	sq_attach(struct device *, struct device *, void *);
int	sq_init(struct ifnet *);
void	sq_start(struct ifnet *);
void	sq_stop(struct ifnet *);
void	sq_watchdog(struct ifnet *);
int	sq_ioctl(struct ifnet *, u_long, caddr_t);

void	sq_set_filter(struct sq_softc *);
int	sq_intr(void *);
void	sq_rxintr(struct sq_softc *);
void	sq_txintr(struct sq_softc *);
void	sq_txring_hpc1(struct sq_softc *);
void	sq_txring_hpc3(struct sq_softc *);
void	sq_reset(struct sq_softc *);
int	sq_add_rxbuf(struct sq_softc *, int);
#ifdef SQ_DEBUG
void	sq_trace_dump(struct sq_softc *);
#endif

int	sq_ifmedia_change_ip22(struct ifnet *);
int	sq_ifmedia_change_singlemedia(struct ifnet *);
void	sq_ifmedia_status_ip22(struct ifnet *, struct ifmediareq *);
void	sq_ifmedia_status_singlemedia(struct ifnet *, struct ifmediareq *);

const struct cfattach sq_ca = {
	sizeof(struct sq_softc), sq_match, sq_attach
};

struct cfdriver sq_cd = {
	NULL, "sq", DV_IFNET
};

/* XXX these values should be moved to <net/if_ether.h> ? */
#define ETHER_PAD_LEN (ETHER_MIN_LEN - ETHER_CRC_LEN)

#define sq_seeq_read(sc, off) \
	bus_space_read_1(sc->sc_regt, sc->sc_regh, ((off) << 2) | 3)
#define sq_seeq_write(sc, off, val) \
	bus_space_write_1(sc->sc_regt, sc->sc_regh, ((off) << 2) | 3, val)

#define sq_hpc_read(sc, off) \
	bus_space_read_4(sc->sc_hpct, sc->sc_hpch, off)
#define sq_hpc_write(sc, off, val) \
	bus_space_write_4(sc->sc_hpct, sc->sc_hpch, off, val)

/* MAC address offset for non-onboard implementations */
#define SQ_HPC_EEPROM_ENADDR	250

#define SGI_OUI_0		0x08
#define SGI_OUI_1		0x00
#define SGI_OUI_2		0x69

int
sq_match(struct device *parent, void *vcf, void *aux)
{
	struct hpc_attach_args *ha = aux;
	struct cfdata *cf = vcf;
	vaddr_t reset, txstat;
	uint32_t dummy;

	if (strcmp(ha->ha_name, cf->cf_driver->cd_name) != 0)
		return 0;

	reset = PHYS_TO_XKPHYS(ha->ha_sh + ha->ha_dmaoff +
	    ha->hpc_regs->enetr_reset, CCA_NC);
	txstat = PHYS_TO_XKPHYS(ha->ha_sh + ha->ha_devoff + (SEEQ_TXSTAT << 2),
	    CCA_NC);

	if (guarded_read_4(reset, &dummy) != 0)
		return 0;

	*(volatile uint32_t *)reset = 0x1;
	delay(20);
	*(volatile uint32_t *)reset = 0x0;

	if (guarded_read_4(txstat, &dummy) != 0)
		return 0;

	if ((*(volatile uint32_t *)txstat & 0xff) != TXSTAT_OLDNEW)
		return 0;

	return 1;
}

void
sq_attach(struct device *parent, struct device *self, void *aux)
{
	struct sq_softc *sc = (struct sq_softc *)self;
	struct hpc_attach_args *haa = aux;
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	int media;
	int i, rc;

	sc->sc_hpct = haa->ha_st;
	sc->sc_hpcbh = haa->ha_sh;
	sc->hpc_regs = haa->hpc_regs;      /* HPC register definitions */

	if ((rc = bus_space_subregion(haa->ha_st, haa->ha_sh,
	    haa->ha_dmaoff, sc->hpc_regs->enet_regs_size,
	    &sc->sc_hpch)) != 0) {
		printf(": can't map HPC DMA registers, error = %d\n", rc);
		goto fail_0;
	}

	sc->sc_regt = haa->ha_st;
	if ((rc = bus_space_subregion(haa->ha_st, haa->ha_sh,
	    haa->ha_devoff, sc->hpc_regs->enet_devregs_size,
	    &sc->sc_regh)) != 0) {
		printf(": can't map Seeq registers, error = %d\n", rc);
		goto fail_0;
	}

	sc->sc_dmat = haa->ha_dmat;

	if ((rc = bus_dmamem_alloc(sc->sc_dmat, sizeof(struct sq_control),
	    0, 0, &sc->sc_cdseg, 1, &sc->sc_ncdseg, BUS_DMA_NOWAIT)) != 0) {
		printf(": unable to allocate control data, error = %d\n", rc);
		goto fail_0;
	}

	/*
	 * Note that we need to pass BUS_DMA_BUS1 in order to get this
	 * allocation to succeed on ECC MC systems. This code is
	 * uncached-write safe, as all updates of the DMA descriptors are
	 * handled in RCU style with hpc_{read,write}_dma_desc().
	 */
	if ((rc = bus_dmamem_map(sc->sc_dmat, &sc->sc_cdseg, sc->sc_ncdseg,
	    sizeof(struct sq_control), (caddr_t *)&sc->sc_control,
	    BUS_DMA_NOWAIT | BUS_DMA_COHERENT | BUS_DMA_BUS1)) != 0) {
		printf(": unable to map control data, error = %d\n", rc);
		goto fail_1;
	}

	if ((rc = bus_dmamap_create(sc->sc_dmat,
	    sizeof(struct sq_control), 1, sizeof(struct sq_control),
	    0, BUS_DMA_NOWAIT, &sc->sc_cdmap)) != 0) {
		printf(": unable to create DMA map for control data, error "
		    "= %d\n", rc);
		goto fail_2;
	}

	if ((rc = bus_dmamap_load(sc->sc_dmat, sc->sc_cdmap,
	    sc->sc_control, sizeof(struct sq_control), NULL,
	    BUS_DMA_NOWAIT)) != 0) {
		printf(": unable to load DMA map for control data, error "
		    "= %d\n", rc);
		goto fail_3;
	}

	memset(sc->sc_control, 0, sizeof(struct sq_control));

	/* Create transmit buffer DMA maps */
	for (i = 0; i < SQ_NTXDESC; i++) {
		if ((rc = bus_dmamap_create(sc->sc_dmat,
		    MCLBYTES, 1, MCLBYTES, 0,
		    BUS_DMA_NOWAIT, &sc->sc_txmap[i])) != 0) {
			printf(": unable to create tx DMA map %d, error = %d\n",
			    i, rc);
			goto fail_4;
		}
	}

	/* Create receive buffer DMA maps */
	for (i = 0; i < SQ_NRXDESC; i++) {
		if ((rc = bus_dmamap_create(sc->sc_dmat,
		    MCLBYTES, 1, MCLBYTES, 0,
		    BUS_DMA_NOWAIT, &sc->sc_rxmap[i])) != 0) {
			printf(": unable to create rx DMA map %d, error = %d\n",
			    i, rc);
			goto fail_5;
		}
	}

	/* Pre-allocate the receive buffers.  */
	for (i = 0; i < SQ_NRXDESC; i++) {
		if ((rc = sq_add_rxbuf(sc, i)) != 0) {
			printf(": unable to allocate or map rx buffer %d\n,"
			    " error = %d\n", i, rc);
			goto fail_6;
		}
	}

	bcopy(&haa->hpc_eeprom[SQ_HPC_EEPROM_ENADDR], sc->sc_ac.ac_enaddr,
	    ETHER_ADDR_LEN);

	/*
	 * If our mac address is bogus, obtain it from ARCBIOS. This will
	 * be true of the onboard HPC3 on IP22, since there is no eeprom,
	 * but rather the DS1386 RTC's battery-backed ram is used.
	 */
	if (sc->sc_ac.ac_enaddr[0] != SGI_OUI_0 ||
	    sc->sc_ac.ac_enaddr[1] != SGI_OUI_1 ||
	    sc->sc_ac.ac_enaddr[2] != SGI_OUI_2)
		enaddr_aton(bios_enaddr, sc->sc_ac.ac_enaddr);

	if ((hpc_intr_establish(haa->ha_irq, IPL_NET, sq_intr, sc,
	    self->dv_xname)) == NULL) {
		printf(": unable to establish interrupt!\n");
		goto fail_6;
	}

	/*
	 * Set up HPC Ethernet PIO and DMA configurations.
	 *
	 * The PROM appears to do most of this for the onboard HPC3, but
	 * not for the Challenge S's IOPLUS chip. We copy how the onboard
	 * chip is configured and assume that it's correct for both.
	 */
	if (haa->hpc_regs->revision == 3 &&
	    sys_config.system_subtype != IP22_INDIGO2) {
		uint32_t dmareg, pioreg;

		if (haa->ha_giofast) {
			pioreg =
			    HPC3_ENETR_PIOCFG_P1(1) |
			    HPC3_ENETR_PIOCFG_P2(5) |
			    HPC3_ENETR_PIOCFG_P3(0);
			dmareg =
			    HPC3_ENETR_DMACFG_D1(5) |
			    HPC3_ENETR_DMACFG_D2(1) |
			    HPC3_ENETR_DMACFG_D3(0);
		} else {
			pioreg =
			    HPC3_ENETR_PIOCFG_P1(1) |
			    HPC3_ENETR_PIOCFG_P2(6) |
			    HPC3_ENETR_PIOCFG_P3(1);
			dmareg =
			    HPC3_ENETR_DMACFG_D1(6) |
			    HPC3_ENETR_DMACFG_D2(2) |
			    HPC3_ENETR_DMACFG_D3(0);
		}
		dmareg |= HPC3_ENETR_DMACFG_FIX_RXDC |
		    HPC3_ENETR_DMACFG_FIX_INTR | HPC3_ENETR_DMACFG_FIX_EOP |
		    HPC3_ENETR_DMACFG_TIMEOUT;

		sq_hpc_write(sc, HPC3_ENETR_PIOCFG, pioreg);
		sq_hpc_write(sc, HPC3_ENETR_DMACFG, dmareg);
	}

	/* Reset the chip to a known state. */
	sq_reset(sc);

	/*
	 * Determine if we're an 8003 or 80c03 by setting the first
	 * MAC address register to non-zero, and then reading it back.
	 * If it's zero, we have an 80c03, because we will have read
	 * the TxCollLSB register.
	 */
	sq_seeq_write(sc, SEEQ_TXCOLLS0, 0xa5);
	if (sq_seeq_read(sc, SEEQ_TXCOLLS0) == 0)
		sc->sc_type = SQ_TYPE_80C03;
	else
		sc->sc_type = SQ_TYPE_8003;
	sq_seeq_write(sc, SEEQ_TXCOLLS0, 0x00);

	printf(": Seeq %s, address %s\n",
	    sc->sc_type == SQ_TYPE_80C03 ? "80c03" : "8003",
	    ether_sprintf(sc->sc_ac.ac_enaddr));

	bcopy(sc->sc_dev.dv_xname, ifp->if_xname, IFNAMSIZ);
	ifp->if_softc = sc;
	ifp->if_start = sq_start;
	ifp->if_ioctl = sq_ioctl;
	ifp->if_watchdog = sq_watchdog;
	ifp->if_flags = IFF_BROADCAST | IFF_NOTRAILERS | IFF_MULTICAST;
	IFQ_SET_READY(&ifp->if_snd);

	if_attach(ifp);
	IFQ_SET_MAXLEN(&ifp->if_snd, SQ_NTXDESC - 1);
	ether_ifattach(ifp);

	if (haa->hpc_regs->revision == 3) {
		uint8_t mask, set;
		if (/* sys_config.system_type != SGI_IP20 && */ /* implied */
		    sys_config.system_subtype == IP22_CHALLS) {
			/*
			 * Challenge S: onboard has AUI connector only,
			 * IO+ has TP connector only.
			 */
			if (haa->ha_base == HPC_BASE_ADDRESS_0) {
				ifmedia_init(&sc->sc_ifmedia, 0,
				    sq_ifmedia_change_singlemedia,
				    sq_ifmedia_status_singlemedia);
				/*
				 * Force 10Base5.
				 */
				media = IFM_ETHER | IFM_10_5;
				mask = IOC_WRITE_ENET_AUTO;
				set = IOC_WRITE_ENET_AUI;
			} else {
				ifmedia_init(&sc->sc_ifmedia, 0,
				    sq_ifmedia_change_singlemedia,
				    sq_ifmedia_status_singlemedia);
				/*
				 * Force 10BaseT, and set the 10BaseT port
				 * to use UTP cable.
				 */
				media = IFM_ETHER | IFM_10_T;
				mask = set = 0;
			}
		} else {
			/*
			 * Indy, Indigo 2: onboard has AUI and TP connectors.
			 */
			ifmedia_init(&sc->sc_ifmedia, 0,
			    sq_ifmedia_change_ip22, sq_ifmedia_status_ip22);
			ifmedia_add(&sc->sc_ifmedia,
			    IFM_ETHER | IFM_10_5, 0, NULL);
			ifmedia_add(&sc->sc_ifmedia,
			    IFM_ETHER | IFM_10_T, 0, NULL);

			/*
			 * Force autoselect, and set the the 10BaseT port
			 * to use UTP cable.
			 */
			media = IFM_ETHER | IFM_AUTO;
			mask = IOC_WRITE_ENET_AUI;
			set = IOC_WRITE_ENET_AUTO | IOC_WRITE_ENET_UTP;
		}

		if (haa->ha_base == HPC_BASE_ADDRESS_0) {
			bus_space_write_4(haa->ha_st, haa->ha_sh,
			    IOC_BASE + IOC_WRITE,
			    (bus_space_read_4(haa->ha_st, haa->ha_sh,
			      IOC_BASE + IOC_WRITE) & ~mask) | set);
			bus_space_barrier(haa->ha_st, haa->ha_sh,
			    IOC_BASE + IOC_WRITE, 4,
			    BUS_SPACE_BARRIER_READ | BUS_SPACE_BARRIER_WRITE);
		}
	} else {
		/*
		 * HPC1/1.5: IP20 on-board, or E++: AUI connector only,
		 * and career information unreliable.
		 */
		ifmedia_init(&sc->sc_ifmedia, 0,
		    sq_ifmedia_change_singlemedia,
		    sq_ifmedia_status_singlemedia);
		media = IFM_ETHER | IFM_10_5;
		sc->sc_flags |= SQF_NOLINKDOWN;
	}

	ifmedia_add(&sc->sc_ifmedia, media, 0, NULL);
	ifmedia_set(&sc->sc_ifmedia, media);

	/* supposedly connected, until TX says otherwise */
	sc->sc_flags |= SQF_LINKUP;

	/* Done! */
	return;

	/*
	 * Free any resources we've allocated during the failed attach
	 * attempt.  Do this in reverse order and fall through.
	 */
 fail_6:
	for (i = 0; i < SQ_NRXDESC; i++) {
		if (sc->sc_rxmbuf[i] != NULL) {
			bus_dmamap_unload(sc->sc_dmat, sc->sc_rxmap[i]);
			m_freem(sc->sc_rxmbuf[i]);
		}
	}
 fail_5:
	for (i = 0; i < SQ_NRXDESC; i++) {
		if (sc->sc_rxmap[i] != NULL)
			bus_dmamap_destroy(sc->sc_dmat, sc->sc_rxmap[i]);
	}
 fail_4:
	for (i = 0; i < SQ_NTXDESC; i++) {
		if (sc->sc_txmap[i] !=  NULL)
			bus_dmamap_destroy(sc->sc_dmat, sc->sc_txmap[i]);
	}
	bus_dmamap_unload(sc->sc_dmat, sc->sc_cdmap);
 fail_3:
	bus_dmamap_destroy(sc->sc_dmat, sc->sc_cdmap);
 fail_2:
	bus_dmamem_unmap(sc->sc_dmat,
	    (void *)sc->sc_control, sizeof(struct sq_control));
 fail_1:
	bus_dmamem_free(sc->sc_dmat, &sc->sc_cdseg, sc->sc_ncdseg);
 fail_0:
	return;
}

/* Set up data to get the interface up and running. */
int
sq_init(struct ifnet *ifp)
{
	struct sq_softc *sc = ifp->if_softc;
	int i;

	/* Cancel any in-progress I/O */
	sq_stop(ifp);

	sc->sc_nextrx = 0;

	sc->sc_nfreetx = SQ_NTXDESC;
	sc->sc_nexttx = sc->sc_prevtx = 0;

	SQ_TRACE(SQ_RESET, sc, 0, 0);

	/* Set into 8003 or 80C03 mode, bank 0 to program Ethernet address */
	if (sc->sc_type == SQ_TYPE_80C03)
		sq_seeq_write(sc, SEEQ_TXCMD, TXCMD_ENABLE_C);
	sq_seeq_write(sc, SEEQ_TXCMD, TXCMD_BANK0);

	/* Now write the address */
	for (i = 0; i < ETHER_ADDR_LEN; i++)
		sq_seeq_write(sc, i, sc->sc_ac.ac_enaddr[i]);

	sc->sc_rxcmd = RXCMD_IE_CRC | RXCMD_IE_DRIB | RXCMD_IE_SHORT |
	    RXCMD_IE_END | RXCMD_IE_GOOD;

	/*
	 * Set the receive filter -- this will add some bits to the
	 * prototype RXCMD register.  Do this before setting the
	 * transmit config register, since we might need to switch
	 * banks.
	 */
	sq_set_filter(sc);

	if (sc->sc_type == SQ_TYPE_80C03) {
		sq_seeq_write(sc, SEEQ_TXCMD, TXCMD_BANK2);
		sq_seeq_write(sc, SEEQ_TXCTRL, 0);
		sq_seeq_write(sc, SEEQ_TXCTRL, TXCTRL_SQE | TXCTRL_NOCARR);
#if 0 /* HPC expects a minimal packet size of ETHER_MIN_LEN anyway */
		sq_seeq_write(sc, SEEQ_CFG, CFG_TX_AUTOPAD);
#endif
		sq_seeq_write(sc, SEEQ_TXCMD, TXCMD_BANK0);
	}

	/* Set up Seeq transmit command register */
	sc->sc_txcmd =
	    TXCMD_IE_UFLOW | TXCMD_IE_COLL | TXCMD_IE_16COLL | TXCMD_IE_GOOD;
	sq_seeq_write(sc, SEEQ_TXCMD, sc->sc_txcmd);

	/* Now write the receive command register. */
	sq_seeq_write(sc, SEEQ_RXCMD, sc->sc_rxcmd);

	/* Pass the start of the receive ring to the HPC */
	sq_hpc_write(sc, sc->hpc_regs->enetr_ndbp, SQ_CDRXADDR(sc, 0));

	/* And turn on the HPC Ethernet receive channel */
	sq_hpc_write(sc, sc->hpc_regs->enetr_ctl,
	    sc->hpc_regs->enetr_ctl_active);

	/*
	 * Turn off delayed receive interrupts on HPC1.
	 * (see Hollywood HPC Specification 2.1.4.3)
	 */
	if (sc->hpc_regs->revision != 3)
		sq_hpc_write(sc, HPC1_ENET_INTDELAY, HPC1_ENET_INTDELAY_OFF);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;
	sq_start(ifp);

	return 0;
}

void
sq_set_filter(struct sq_softc *sc)
{
	struct arpcom *ac = &sc->sc_ac;
	struct ifnet *ifp = &sc->sc_ac.ac_if;

	sc->sc_rxcmd &= ~RXCMD_REC_MASK;
	ifp->if_flags &= ~IFF_ALLMULTI;

	/*
	 * The 8003 has no hash table.  If we have any multicast
	 * addresses on the list, enable reception of all multicast
	 * frames.
	 *
	 * XXX The 80c03 has a hash table.  We should use it.
	 */
	if (ifp->if_flags & IFF_PROMISC || ac->ac_multicnt > 0) {
		ifp->if_flags |= IFF_ALLMULTI;
		if (ifp->if_flags & IFF_PROMISC)
			sc->sc_rxcmd |= RXCMD_REC_ALL;
		else
			sc->sc_rxcmd |= RXCMD_REC_MULTI;
	}

	/*
	 * Unless otherwise specified, always accept broadcast frames.
	 */
	if ((sc->sc_rxcmd & ~RXCMD_REC_MASK) == RXCMD_REC_NONE)
		sc->sc_rxcmd |= RXCMD_REC_BROAD;
}

int
sq_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct sq_softc *sc = ifp->if_softc;
	struct ifaddr *ifa = (struct ifaddr *)data;
	struct ifreq *ifr = (struct ifreq *)data;
	int s, error = 0;

	SQ_TRACE(SQ_IOCTL, sc, 0, 0);

	s = splnet();

	switch (cmd) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		if (!(ifp->if_flags & IFF_RUNNING))
			sq_init(ifp);
		if (ifa->ifa_addr->sa_family == AF_INET)
			arp_ifinit(&sc->sc_ac, ifa);
		break;

	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->sc_ifmedia, cmd);
		break;

	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_flags & IFF_RUNNING)
				error = ENETRESET;
			else
				sq_init(ifp);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				sq_stop(ifp);
		}
		break;

	default:
		error = ether_ioctl(ifp, &sc->sc_ac, cmd, data);
		break;
	}

	if (error == ENETRESET) {
		/*
		 * Multicast list has changed; set the hardware filter
		 * accordingly.
		 */
		if (ifp->if_flags & IFF_RUNNING)
			error = sq_init(ifp);
		else
			error = 0;
	}

	splx(s);
	return error;
}

void
sq_start(struct ifnet *ifp)
{
	struct sq_softc *sc = ifp->if_softc;
	struct mbuf *m0, *m;
	struct hpc_dma_desc *txd, txd_store;
	bus_dmamap_t dmamap;
	uint32_t status;
	int err, len, totlen, nexttx, firsttx, lasttx = -1, ofree, seg;

	if ((ifp->if_flags & (IFF_RUNNING|IFF_OACTIVE)) != IFF_RUNNING)
		return;

	/*
	 * Remember the previous number of free descriptors and
	 * the first descriptor we'll use.
	 */
	ofree = sc->sc_nfreetx;
	firsttx = sc->sc_nexttx;

	/*
	 * Loop through the send queue, setting up transmit descriptors
	 * until we drain the queue, or use up all available transmit
	 * descriptors.
	 */
	while (sc->sc_nfreetx != 0) {
		/*
		 * Grab a packet off the queue.
		 */
		IFQ_POLL(&ifp->if_snd, m0);
		if (m0 == NULL)
			break;
		m = NULL;

		dmamap = sc->sc_txmap[sc->sc_nexttx];

		/*
		 * Load the DMA map.  If this fails, the packet either
		 * didn't fit in the alloted number of segments, or we were
		 * short on resources.  In this case, we'll copy and try
		 * again.
		 * Also copy it if we need to pad, so that we are sure there
		 * is room for the pad buffer.
		 * XXX the right way of doing this is to use a static buffer
		 * for padding and adding it to the transmit descriptor (see
		 * sys/dev/pci/if_tl.c for example). We can't do this here yet
		 * because we can't send packets with more than one fragment.
		 */
		len = m0->m_pkthdr.len;
		if (len < ETHER_PAD_LEN ||
		    bus_dmamap_load_mbuf(sc->sc_dmat, dmamap, m0,
		    BUS_DMA_NOWAIT) != 0) {
			MGETHDR(m, M_DONTWAIT, MT_DATA);
			if (m == NULL) {
				printf("%s: unable to allocate Tx mbuf\n",
				    sc->sc_dev.dv_xname);
				break;
			}
			if (len > MHLEN) {
				MCLGET(m, M_DONTWAIT);
				if ((m->m_flags & M_EXT) == 0) {
					printf("%s: unable to allocate Tx "
					    "cluster\n",
					    sc->sc_dev.dv_xname);
					m_freem(m);
					break;
				}
			}

			m_copydata(m0, 0, len, mtod(m, void *));
			if (len < ETHER_PAD_LEN /* &&
			    sc->sc_type != SQ_TYPE_80C03 */) {
				memset(mtod(m, char *) + len, 0,
				    ETHER_PAD_LEN - len);
				len = ETHER_PAD_LEN;
			}
			m->m_pkthdr.len = m->m_len = len;

			if ((err = bus_dmamap_load_mbuf(sc->sc_dmat, dmamap,
			    m, BUS_DMA_NOWAIT)) != 0) {
				printf("%s: unable to load Tx buffer, "
				    "error = %d\n",
				    sc->sc_dev.dv_xname, err);
				break;
			}
		}

		/*
		 * Ensure we have enough descriptors free to describe
		 * the packet.
		 */
		if (dmamap->dm_nsegs > sc->sc_nfreetx) {
			/*
			 * Not enough free descriptors to transmit this
			 * packet.  We haven't committed to anything yet,
			 * so just unload the DMA map, put the packet
			 * back on the queue, and punt.  Notify the upper
			 * layer that there are no more slots left.
			 *
			 * XXX We could allocate an mbuf and copy, but
			 * XXX it is worth it?
			 */
			ifp->if_flags |= IFF_OACTIVE;
			bus_dmamap_unload(sc->sc_dmat, dmamap);
			if (m != NULL)
				m_freem(m);
			break;
		}

		IFQ_DEQUEUE(&ifp->if_snd, m0);
#if NBPFILTER > 0
		/*
		 * Pass the packet to any BPF listeners.
		 */
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m0, BPF_DIRECTION_OUT);
#endif
		if (m != NULL) {
			m_freem(m0);
			m0 = m;
		}

		/*
		 * WE ARE NOW COMMITTED TO TRANSMITTING THE PACKET.
		 */

		SQ_TRACE(SQ_ENQUEUE, sc, sc->sc_nexttx, 0);

		/* Sync the DMA map. */
		bus_dmamap_sync(sc->sc_dmat, dmamap, 0, dmamap->dm_mapsize,
		    BUS_DMASYNC_PREWRITE);

		/*
		 * Initialize the transmit descriptors.
		 */
		for (nexttx = sc->sc_nexttx, seg = 0, totlen = 0;
		     seg < dmamap->dm_nsegs;
		     seg++, nexttx = SQ_NEXTTX(nexttx)) {
			txd = hpc_read_dma_desc(sc->sc_txdesc + nexttx,
			    &txd_store);
			if (sc->hpc_regs->revision == 3) {
				txd->hpc3_hdd_bufptr =
				    dmamap->dm_segs[seg].ds_addr;
				txd->hpc3_hdd_ctl = dmamap->dm_segs[seg].ds_len;
			} else {
				txd->hpc1_hdd_bufptr =
				    dmamap->dm_segs[seg].ds_addr;
				txd->hpc1_hdd_ctl = dmamap->dm_segs[seg].ds_len;
			}
			txd->hdd_descptr = SQ_CDTXADDR(sc, SQ_NEXTTX(nexttx));
			hpc_write_dma_desc(sc->sc_txdesc + nexttx, txd);
			lasttx = nexttx;
			totlen += dmamap->dm_segs[seg].ds_len;
		}

		/* Last descriptor gets end-of-packet */
		KASSERT(lasttx != -1);
		/* txd = hpc_read_dma_desc(sc->sc_txdesc + lasttx, &txd_store); */
		if (sc->hpc_regs->revision == 3)
			txd->hpc3_hdd_ctl |= HPC3_HDD_CTL_EOPACKET;
		else
			txd->hpc1_hdd_ctl |= HPC1_HDD_CTL_EOPACKET;
		hpc_write_dma_desc(sc->sc_txdesc + lasttx, txd);

		SQ_DPRINTF(("%s: transmit %d-%d, len %d\n",
		    sc->sc_dev.dv_xname, sc->sc_nexttx, lasttx, totlen));

		if (ifp->if_flags & IFF_DEBUG) {
			printf("     transmit chain:\n");
			for (seg = sc->sc_nexttx;; seg = SQ_NEXTTX(seg)) {
				printf("     descriptor %d:\n", seg);
				printf("       hdd_bufptr:      0x%08x\n",
				    (sc->hpc_regs->revision == 3) ?
				    sc->sc_txdesc[seg].hpc3_hdd_bufptr :
				    sc->sc_txdesc[seg].hpc1_hdd_bufptr);
				printf("       hdd_ctl: 0x%08x\n",
				    (sc->hpc_regs->revision == 3) ?
				    sc->sc_txdesc[seg].hpc3_hdd_ctl:
				    sc->sc_txdesc[seg].hpc1_hdd_ctl);
				printf("       hdd_descptr:      0x%08x\n",
				    sc->sc_txdesc[seg].hdd_descptr);

				if (seg == lasttx)
					break;
			}
		}

		/* Sync the descriptors we're using. */
		SQ_CDTXSYNC(sc, sc->sc_nexttx, dmamap->dm_nsegs,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

		/* Store a pointer to the packet so we can free it later */
		sc->sc_txmbuf[sc->sc_nexttx] = m0;

		/* Advance the tx pointer. */
		sc->sc_nfreetx -= dmamap->dm_nsegs;
		sc->sc_nexttx = nexttx;
	}

	/* All transmit descriptors used up, let upper layers know */
	if (sc->sc_nfreetx == 0)
		ifp->if_flags |= IFF_OACTIVE;

	if (sc->sc_nfreetx != ofree) {
		SQ_DPRINTF(("%s: %d packets enqueued, first %d, INTR on %d\n",
		    sc->sc_dev.dv_xname, lasttx - firsttx + 1,
		    firsttx, lasttx));

		/*
		 * Cause a transmit interrupt to happen on the
		 * last packet we enqueued, mark it as the last
		 * descriptor.
		 *
		 * HPC1_HDD_CTL_INTR will generate an interrupt on
		 * HPC1. HPC3 requires HPC3_HDD_CTL_EOCHAIN in
		 * addition to HPC3_HDD_CTL_INTR to interrupt.
		 */
		KASSERT(lasttx != -1);
		txd = hpc_read_dma_desc(sc->sc_txdesc + lasttx, &txd_store);
		if (sc->hpc_regs->revision == 3) {
			txd->hpc3_hdd_ctl |=
			    HPC3_HDD_CTL_INTR | HPC3_HDD_CTL_EOCHAIN;
		} else {
			txd->hpc1_hdd_ctl |= HPC1_HDD_CTL_INTR;
			txd->hpc1_hdd_bufptr |= HPC1_HDD_CTL_EOCHAIN;
		}
		hpc_write_dma_desc(sc->sc_txdesc + lasttx, txd);
		SQ_CDTXSYNC(sc, lasttx, 1,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

		/*
		 * There is a potential race condition here if the HPC
		 * DMA channel is active and we try and either update
		 * the 'next descriptor' pointer in the HPC PIO space
		 * or the 'next descriptor' pointer in a previous desc-
		 * riptor.
		 *
		 * To avoid this, if the channel is active, we rely on
		 * the transmit interrupt routine noticing that there
		 * are more packets to send and restarting the HPC DMA
		 * engine, rather than mucking with the DMA state here.
		 */
		status = sq_hpc_read(sc, sc->hpc_regs->enetx_ctl);

		if ((status & sc->hpc_regs->enetx_ctl_active) != 0) {
			SQ_TRACE(SQ_ADD_TO_DMA, sc, firsttx, status);

			txd = hpc_read_dma_desc(sc->sc_txdesc +
			    SQ_PREVTX(firsttx), &txd_store);
			/*
			 * NB: hpc3_hdd_ctl == hpc1_hdd_bufptr, and
			 * HPC1_HDD_CTL_EOCHAIN == HPC3_HDD_CTL_EOCHAIN
			 */
			txd->hpc3_hdd_ctl &= ~HPC3_HDD_CTL_EOCHAIN;
			if (sc->hpc_regs->revision != 3)
				txd->hpc1_hdd_ctl &= ~HPC1_HDD_CTL_INTR;

			hpc_write_dma_desc(sc->sc_txdesc + SQ_PREVTX(firsttx),
			    txd);
			SQ_CDTXSYNC(sc, SQ_PREVTX(firsttx),  1,
			    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
		} else if (sc->hpc_regs->revision == 3) {
			SQ_TRACE(SQ_START_DMA, sc, firsttx, status);

			sq_hpc_write(sc, HPC3_ENETX_NDBP, SQ_CDTXADDR(sc,
			    firsttx));

			/* Kick DMA channel into life */
			sq_hpc_write(sc, HPC3_ENETX_CTL, HPC3_ENETX_CTL_ACTIVE);
		} else {
			/*
			 * In the HPC1 case where transmit DMA is
			 * inactive, we can either kick off if
			 * the ring was previously empty, or call
			 * our transmit interrupt handler to
			 * figure out if the ring stopped short
			 * and restart at the right place.
			 */
			if (ofree == SQ_NTXDESC) {
				SQ_TRACE(SQ_START_DMA, sc, firsttx, status);

				sq_hpc_write(sc, HPC1_ENETX_NDBP,
				    SQ_CDTXADDR(sc, firsttx));
				sq_hpc_write(sc, HPC1_ENETX_CFXBP,
				    SQ_CDTXADDR(sc, firsttx));
				sq_hpc_write(sc, HPC1_ENETX_CBP,
				    SQ_CDTXADDR(sc, firsttx));

				/* Kick DMA channel into life */
				sq_hpc_write(sc, HPC1_ENETX_CTL,
				    HPC1_ENETX_CTL_ACTIVE);
			} else
				sq_txring_hpc1(sc);
		}

		/* Set a watchdog timer in case the chip flakes out. */
		ifp->if_timer = 5;
	}
}

void
sq_stop(struct ifnet *ifp)
{
	struct sq_softc *sc = ifp->if_softc;
	int i;

	ifp->if_timer = 0;
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);

	for (i = 0; i < SQ_NTXDESC; i++) {
		if (sc->sc_txmbuf[i] != NULL) {
			bus_dmamap_unload(sc->sc_dmat, sc->sc_txmap[i]);
			m_freem(sc->sc_txmbuf[i]);
			sc->sc_txmbuf[i] = NULL;
		}
	}

	/* Clear Seeq transmit/receive command registers */
	sc->sc_txcmd = 0;
	sq_seeq_write(sc, SEEQ_TXCMD, 0);
	sq_seeq_write(sc, SEEQ_RXCMD, 0);

	sq_reset(sc);
}

/* Device timeout/watchdog routine. */
void
sq_watchdog(struct ifnet *ifp)
{
	struct sq_softc *sc = ifp->if_softc;
	uint32_t status;

	status = sq_hpc_read(sc, sc->hpc_regs->enetx_ctl);
	log(LOG_ERR, "%s: device timeout (prev %d, next %d, free %d, "
	    "status %08x)\n", sc->sc_dev.dv_xname, sc->sc_prevtx,
	    sc->sc_nexttx, sc->sc_nfreetx, status);

#ifdef SQ_DEBUG
	sq_trace_dump(sc);
#endif

	++ifp->if_oerrors;

	sq_init(ifp);
}

#ifdef SQ_DEBUG
void
sq_trace_dump(struct sq_softc *sc)
{
	int i;
	const char *act;

	for (i = 0; i < sc->sq_trace_idx; i++) {
		switch (sc->sq_trace[i].action) {
		case SQ_RESET:		act = "SQ_RESET";		break;
		case SQ_ADD_TO_DMA:	act = "SQ_ADD_TO_DMA";		break;
		case SQ_START_DMA:	act = "SQ_START_DMA";		break;
		case SQ_DONE_DMA:	act = "SQ_DONE_DMA";		break;
		case SQ_RESTART_DMA:	act = "SQ_RESTART_DMA";		break;
		case SQ_TXINTR_ENTER:	act = "SQ_TXINTR_ENTER";	break;
		case SQ_TXINTR_EXIT:	act = "SQ_TXINTR_EXIT";		break;
		case SQ_TXINTR_BUSY:	act = "SQ_TXINTR_BUSY";		break;
		case SQ_IOCTL:		act = "SQ_IOCTL";		break;
		case SQ_ENQUEUE:	act = "SQ_ENQUEUE";		break;
		default:		act = "UNKNOWN";
		}

		printf("%s: [%03d] action %-16s buf %03d free %03d "
		    "status %08x line %d\n", sc->sc_dev.dv_xname, i, act,
		    sc->sq_trace[i].bufno, sc->sq_trace[i].freebuf,
		    sc->sq_trace[i].status, sc->sq_trace[i].line);
	}

	memset(&sc->sq_trace, 0, sizeof(sc->sq_trace));
	sc->sq_trace_idx = 0;
}
#endif

int
sq_intr(void *arg)
{
	struct sq_softc *sc = arg;
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	int oldlink = sc->sc_flags & SQF_LINKUP;
	uint32_t stat;
	uint8_t sqe;

	stat = sq_hpc_read(sc, sc->hpc_regs->enetr_reset);

	if ((stat & 2) == 0) {
		SQ_DPRINTF(("%s: Unexpected interrupt!\n",
		    sc->sc_dev.dv_xname));
	} else
		sq_hpc_write(sc, sc->hpc_regs->enetr_reset, (stat | 2));

	/*
	 * If the interface isn't running, the interrupt couldn't
	 * possibly have come from us.
	 */
	if ((ifp->if_flags & IFF_RUNNING) == 0)
		return 0;

	/*
	 * Check for loss of carrier detected during transmission if we
	 * can detect it.
	 * Unfortunately, this does not work on IP20 and E++ designs.
	 */
	if (sc->sc_type == SQ_TYPE_80C03 &&
	    !ISSET(sc->sc_flags, SQF_NOLINKDOWN)) {
		sqe = sq_seeq_read(sc, SEEQ_SQE) & (SQE_FLAG | SQE_NOCARR);
		if (sqe != 0) {
			sq_seeq_write(sc, SEEQ_TXCMD,
			    TXCMD_BANK2 | sc->sc_txcmd);
			/* reset counters */
			sq_seeq_write(sc, SEEQ_TXCTRL, 0);
			sq_seeq_write(sc, SEEQ_TXCTRL,
			    TXCTRL_SQE | TXCTRL_NOCARR);
			sq_seeq_write(sc, SEEQ_TXCMD,
			    TXCMD_BANK0 | sc->sc_txcmd);
			if (sqe == (SQE_FLAG | SQE_NOCARR))
				sc->sc_flags &= ~SQF_LINKUP;
		}
	}

	/* Always check for received packets */
	sq_rxintr(sc);

	/* Only handle transmit interrupts if we actually sent something */
	if (sc->sc_nfreetx < SQ_NTXDESC)
		sq_txintr(sc);

	/* Notify link status change */
	if (oldlink != (sc->sc_flags & SQF_LINKUP)) {
		if (oldlink != 0) {
			ifp->if_link_state = LINK_STATE_DOWN;
			ifp->if_baudrate = 0;
		} else {
			ifp->if_link_state = LINK_STATE_UP;
			ifp->if_baudrate = IF_Mbps(10);
		}
		if_link_state_change(ifp);
	}

	/*
	 * XXX Always claim the interrupt, even if we did nothing.
	 * XXX There seem to be extra interrupts when the receiver becomes
	 * XXX idle.
	 */
	return 1;
}

void
sq_rxintr(struct sq_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	struct mbuf_list ml = MBUF_LIST_INITIALIZER();
	struct mbuf* m;
	struct hpc_dma_desc *rxd, rxd_store;
	int i, framelen;
	uint8_t pktstat;
	uint32_t status;
	uint32_t ctl_reg;
	int new_end, orig_end;

	for (i = sc->sc_nextrx; ; i = SQ_NEXTRX(i)) {
		SQ_CDRXSYNC(sc, i,
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

		/*
		 * If this is a CPU-owned buffer, we're at the end of the list.
		 */
		if (sc->hpc_regs->revision == 3)
			ctl_reg =
			    sc->sc_rxdesc[i].hpc3_hdd_ctl & HPC3_HDD_CTL_OWN;
		else
			ctl_reg =
			    sc->sc_rxdesc[i].hpc1_hdd_ctl & HPC1_HDD_CTL_OWN;

		if (ctl_reg) {
#if defined(SQ_DEBUG)
			uint32_t reg;

			reg = sq_hpc_read(sc, sc->hpc_regs->enetr_ctl);
			SQ_DPRINTF(("%s: rxintr: done at %d (ctl %08x)\n",
			    sc->sc_dev.dv_xname, i, reg));
#endif
			break;
		}

		m = sc->sc_rxmbuf[i];
		framelen = m->m_ext.ext_size - 3;
		if (sc->hpc_regs->revision == 3)
		    framelen -=
			HPC3_HDD_CTL_BYTECNT(sc->sc_rxdesc[i].hpc3_hdd_ctl);
		else
		    framelen -=
			HPC1_HDD_CTL_BYTECNT(sc->sc_rxdesc[i].hpc1_hdd_ctl);

		/* Now sync the actual packet data */
		bus_dmamap_sync(sc->sc_dmat, sc->sc_rxmap[i], 0,
		    sc->sc_rxmap[i]->dm_mapsize, BUS_DMASYNC_POSTREAD);

		pktstat = *((uint8_t *)m->m_data + framelen + 2);

		if ((pktstat & RXSTAT_GOOD) == 0) {
			ifp->if_ierrors++;

			if (pktstat & RXSTAT_OFLOW)
				printf("%s: receive FIFO overflow\n",
				    sc->sc_dev.dv_xname);

			bus_dmamap_sync(sc->sc_dmat, sc->sc_rxmap[i], 0,
			    sc->sc_rxmap[i]->dm_mapsize, BUS_DMASYNC_PREREAD);
			SQ_INIT_RXDESC(sc, i);
			SQ_DPRINTF(("%s: sq_rxintr: buf %d no RXSTAT_GOOD\n",
			    sc->sc_dev.dv_xname, i));
			continue;
		}

		/* Link must be good if we have received data. */
		sc->sc_flags |= SQF_LINKUP;

		if (sq_add_rxbuf(sc, i) != 0) {
			ifp->if_ierrors++;
			bus_dmamap_sync(sc->sc_dmat, sc->sc_rxmap[i], 0,
			    sc->sc_rxmap[i]->dm_mapsize, BUS_DMASYNC_PREREAD);
			SQ_INIT_RXDESC(sc, i);
			SQ_DPRINTF(("%s: sq_rxintr: buf %d sq_add_rxbuf() "
			    "failed\n", sc->sc_dev.dv_xname, i));
			continue;
		}


		m->m_data += 2;
		m->m_pkthdr.len = m->m_len = framelen;

		ifp->if_ipackets++;

		SQ_DPRINTF(("%s: sq_rxintr: buf %d len %d\n",
		    sc->sc_dev.dv_xname, i, framelen));

		ml_enqueue(&ml, m);
	}

	if_input(ifp, &ml);

	/* If anything happened, move ring start/end pointers to new spot */
	if (i != sc->sc_nextrx) {
		/*
		 * NB: hpc3_hdd_ctl == hpc1_hdd_bufptr, and
		 * HPC1_HDD_CTL_EOCHAIN == HPC3_HDD_CTL_EOCHAIN
		 */

		new_end = SQ_PREVRX(i);
		rxd = hpc_read_dma_desc(sc->sc_rxdesc + new_end, &rxd_store);
		rxd->hpc3_hdd_ctl |= HPC3_HDD_CTL_EOCHAIN;
		hpc_write_dma_desc(sc->sc_rxdesc + new_end, rxd);
		SQ_CDRXSYNC(sc, new_end,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

		orig_end = SQ_PREVRX(sc->sc_nextrx);
		rxd = hpc_read_dma_desc(sc->sc_rxdesc + orig_end, &rxd_store);
		rxd->hpc3_hdd_ctl &= ~HPC3_HDD_CTL_EOCHAIN;
		hpc_write_dma_desc(sc->sc_rxdesc + orig_end, rxd);
		SQ_CDRXSYNC(sc, orig_end,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

		sc->sc_nextrx = i;
	}

	status = sq_hpc_read(sc, sc->hpc_regs->enetr_ctl);

	/* If receive channel is stopped, restart it... */
	if ((status & sc->hpc_regs->enetr_ctl_active) == 0) {
		/* Pass the start of the receive ring to the HPC */
		sq_hpc_write(sc, sc->hpc_regs->enetr_ndbp,
		    SQ_CDRXADDR(sc, sc->sc_nextrx));

		/* And turn on the HPC Ethernet receive channel */
		sq_hpc_write(sc, sc->hpc_regs->enetr_ctl,
		    sc->hpc_regs->enetr_ctl_active);
	}
}

void
sq_txintr(struct sq_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	uint shift = 0;
	uint32_t status, tmp;

	if (sc->hpc_regs->revision != 3)
		shift = 16;

	status = sq_hpc_read(sc, sc->hpc_regs->enetx_ctl) >> shift;

	SQ_TRACE(SQ_TXINTR_ENTER, sc, sc->sc_prevtx, status);

	tmp = (sc->hpc_regs->enetx_ctl_active >> shift) | TXSTAT_GOOD;
	if ((status & tmp) == 0) {
		if (status & TXSTAT_COLL)
			ifp->if_collisions++;

		if (status & TXSTAT_UFLOW) {
			printf("%s: transmit underflow\n",
			    sc->sc_dev.dv_xname);
			ifp->if_oerrors++;
#ifdef SQ_DEBUG
			sq_trace_dump(sc);
#endif
			sq_init(ifp);
			return;
		}

		if (status & TXSTAT_16COLL) {
			if (ifp->if_flags & IFF_DEBUG)
				printf("%s: max collisions reached\n",
				    sc->sc_dev.dv_xname);
			ifp->if_oerrors++;
			ifp->if_collisions += 16;
		}
	}

	/* prevtx now points to next xmit packet not yet finished */
	if (sc->hpc_regs->revision == 3)
		sq_txring_hpc3(sc);
	else
		sq_txring_hpc1(sc);

	/* If we have buffers free, let upper layers know */
	if (sc->sc_nfreetx > 0)
		ifp->if_flags &= ~IFF_OACTIVE;

	/* If all packets have left the coop, cancel watchdog */
	if (sc->sc_nfreetx == SQ_NTXDESC)
		ifp->if_timer = 0;

	SQ_TRACE(SQ_TXINTR_EXIT, sc, sc->sc_prevtx, status);
	sq_start(ifp);
}

/*
 * Reclaim used transmit descriptors and restart the transmit DMA
 * engine if necessary.
 */
void
sq_txring_hpc1(struct sq_softc *sc)
{
	/*
	 * HPC1 doesn't tag transmitted descriptors, however,
	 * the NDBP register points to the next descriptor that
	 * has not yet been processed. If DMA is not in progress,
	 * we can safely reclaim all descriptors up to NDBP, and,
	 * if necessary, restart DMA at NDBP. Otherwise, if DMA
	 * is active, we can only safely reclaim up to CBP.
	 *
	 * For now, we'll only reclaim on inactive DMA and assume
	 * that a sufficiently large ring keeps us out of trouble.
	 */
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	uint32_t reclaimto, status;
	int reclaimall, i = sc->sc_prevtx;

	status = sq_hpc_read(sc, HPC1_ENETX_CTL);
	if (status & HPC1_ENETX_CTL_ACTIVE) {
		SQ_TRACE(SQ_TXINTR_BUSY, sc, i, status);
		return;
	} else
		reclaimto = sq_hpc_read(sc, HPC1_ENETX_NDBP);

	if (sc->sc_nfreetx == 0 && SQ_CDTXADDR(sc, i) == reclaimto)
		reclaimall = 1;
	else
		reclaimall = 0;

	while (sc->sc_nfreetx < SQ_NTXDESC) {
		if (SQ_CDTXADDR(sc, i) == reclaimto && !reclaimall)
			break;

		SQ_CDTXSYNC(sc, i, sc->sc_txmap[i]->dm_nsegs,
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

		/* Sync the packet data, unload DMA map, free mbuf */
		bus_dmamap_sync(sc->sc_dmat, sc->sc_txmap[i],
		    0, sc->sc_txmap[i]->dm_mapsize, BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat, sc->sc_txmap[i]);
		m_freem(sc->sc_txmbuf[i]);
		sc->sc_txmbuf[i] = NULL;

		ifp->if_opackets++;
		sc->sc_nfreetx++;

		SQ_TRACE(SQ_DONE_DMA, sc, i, status);

		i = SQ_NEXTTX(i);
	}

	if (sc->sc_nfreetx < SQ_NTXDESC) {
		SQ_TRACE(SQ_RESTART_DMA, sc, i, status);

		KASSERT(reclaimto == SQ_CDTXADDR(sc, i));

		sq_hpc_write(sc, HPC1_ENETX_CFXBP, reclaimto);
		sq_hpc_write(sc, HPC1_ENETX_CBP, reclaimto);

		/* Kick DMA channel into life */
		sq_hpc_write(sc, HPC1_ENETX_CTL, HPC1_ENETX_CTL_ACTIVE);

		/*
		 * Set a watchdog timer in case the chip
		 * flakes out.
		 */
		ifp->if_timer = 5;
	}

	sc->sc_prevtx = i;
}

/*
 * Reclaim used transmit descriptors and restart the transmit DMA
 * engine if necessary.
 */
void
sq_txring_hpc3(struct sq_softc *sc)
{
	/*
	 * HPC3 tags descriptors with a bit once they've been
	 * transmitted. We need only free each XMITDONE'd
	 * descriptor, and restart the DMA engine if any
	 * descriptors are left over.
	 */
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	int i;
	uint32_t status = 0;

	i = sc->sc_prevtx;
	while (sc->sc_nfreetx < SQ_NTXDESC) {
		/*
		 * Check status first so we don't end up with a case of
		 * the buffer not being finished while the DMA channel
		 * has gone idle.
		 */
		status = sq_hpc_read(sc, HPC3_ENETX_CTL);

		SQ_CDTXSYNC(sc, i, sc->sc_txmap[i]->dm_nsegs,
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

		/* Check for used descriptor and restart DMA chain if needed */
		if ((sc->sc_txdesc[i].hpc3_hdd_ctl &
		    HPC3_HDD_CTL_XMITDONE) == 0) {
			if ((status & HPC3_ENETX_CTL_ACTIVE) == 0) {
				SQ_TRACE(SQ_RESTART_DMA, sc, i, status);

				sq_hpc_write(sc, HPC3_ENETX_NDBP,
				    SQ_CDTXADDR(sc, i));

				/* Kick DMA channel into life */
				sq_hpc_write(sc, HPC3_ENETX_CTL,
				    HPC3_ENETX_CTL_ACTIVE);

				/*
				 * Set a watchdog timer in case the chip
				 * flakes out.
				 */
				ifp->if_timer = 5;
			} else
				SQ_TRACE(SQ_TXINTR_BUSY, sc, i, status);
			break;
		}

		/* Sync the packet data, unload DMA map, free mbuf */
		bus_dmamap_sync(sc->sc_dmat, sc->sc_txmap[i],
		    0, sc->sc_txmap[i]->dm_mapsize, BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat, sc->sc_txmap[i]);
		m_freem(sc->sc_txmbuf[i]);
		sc->sc_txmbuf[i] = NULL;

		ifp->if_opackets++;
		sc->sc_nfreetx++;

		SQ_TRACE(SQ_DONE_DMA, sc, i, status);
		i = SQ_NEXTTX(i);
	}

	sc->sc_prevtx = i;
}

void
sq_reset(struct sq_softc *sc)
{
	/* Stop HPC dma channels */
	sq_hpc_write(sc, sc->hpc_regs->enetr_ctl, 0);
	sq_hpc_write(sc, sc->hpc_regs->enetx_ctl, 0);

	sq_hpc_write(sc, sc->hpc_regs->enetr_reset, 3);
	delay(20);
	sq_hpc_write(sc, sc->hpc_regs->enetr_reset, 0);
}

/* sq_add_rxbuf: Add a receive buffer to the indicated descriptor. */
int
sq_add_rxbuf(struct sq_softc *sc, int idx)
{
	int err;
	struct mbuf *m;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return ENOBUFS;

	MCLGET(m, M_DONTWAIT);
	if ((m->m_flags & M_EXT) == 0) {
		m_freem(m);
		return ENOBUFS;
	}

	if (sc->sc_rxmbuf[idx] != NULL)
		bus_dmamap_unload(sc->sc_dmat, sc->sc_rxmap[idx]);

	sc->sc_rxmbuf[idx] = m;

	if ((err = bus_dmamap_load(sc->sc_dmat, sc->sc_rxmap[idx],
	    m->m_ext.ext_buf, m->m_ext.ext_size, NULL, BUS_DMA_NOWAIT)) != 0) {
		printf("%s: can't load rx DMA map %d, error = %d\n",
		    sc->sc_dev.dv_xname, idx, err);
		panic("sq_add_rxbuf");	/* XXX */
	}

	bus_dmamap_sync(sc->sc_dmat, sc->sc_rxmap[idx],
	    0, sc->sc_rxmap[idx]->dm_mapsize, BUS_DMASYNC_PREREAD);

	SQ_INIT_RXDESC(sc, idx);

	return 0;
}

/*
 * Media handling
 */

int
sq_ifmedia_change_ip22(struct ifnet *ifp)
{
	struct sq_softc *sc = ifp->if_softc;
	struct ifmedia *ifm = &sc->sc_ifmedia;
	uint32_t iocw;

	iocw =
	    bus_space_read_4(sc->sc_hpct, sc->sc_hpcbh, IOC_BASE + IOC_WRITE);

	switch (IFM_SUBTYPE(ifm->ifm_media)) {
	case IFM_10_5:
		iocw &= ~IOC_WRITE_ENET_AUTO;
		iocw |= IOC_WRITE_ENET_AUI;
		break;
	case IFM_10_T:
		iocw &= ~(IOC_WRITE_ENET_AUTO | IOC_WRITE_ENET_AUI);
		iocw |= IOC_WRITE_ENET_UTP;	/* in case it cleared */
		break;
	default:
	case IFM_AUTO:
		iocw |= IOC_WRITE_ENET_AUTO;
		break;
	}

	bus_space_write_4(sc->sc_hpct, sc->sc_hpcbh, IOC_BASE + IOC_WRITE,
	    iocw);
	bus_space_barrier(sc->sc_hpct, sc->sc_hpcbh, IOC_BASE + IOC_WRITE, 4,
	    BUS_SPACE_BARRIER_READ | BUS_SPACE_BARRIER_WRITE);
	return 0;
}

int
sq_ifmedia_change_singlemedia(struct ifnet *ifp)
{
	return 0;
}

void
sq_ifmedia_status_ip22(struct ifnet *ifp, struct ifmediareq *req)
{
	struct sq_softc *sc = ifp->if_softc;
	uint32_t iocr, iocw;

	iocw =
	    bus_space_read_4(sc->sc_hpct, sc->sc_hpcbh, IOC_BASE + IOC_WRITE);

	req->ifm_status = IFM_AVALID;
	if (sc->sc_flags & SQF_LINKUP)
		req->ifm_status |= IFM_ACTIVE;
	if ((iocw & IOC_WRITE_ENET_AUTO) != 0) {
		iocr = bus_space_read_4(sc->sc_hpct, sc->sc_hpcbh,
		    IOC_BASE + IOC_READ);
		if ((iocr & IOC_READ_ENET_LINK) != 0)
			req->ifm_active = IFM_10_5 | IFM_ETHER;
		else
			req->ifm_active = IFM_10_T | IFM_ETHER;
	} else {
		if ((iocw & IOC_WRITE_ENET_AUI) != 0)
			req->ifm_active = IFM_10_5 | IFM_ETHER;
		else
			req->ifm_active = IFM_10_T | IFM_ETHER;
	}
}

void
sq_ifmedia_status_singlemedia(struct ifnet *ifp, struct ifmediareq *req)
{
	struct sq_softc *sc = ifp->if_softc;
	struct ifmedia *ifm = &sc->sc_ifmedia;

	req->ifm_status = IFM_AVALID;
	if (sc->sc_flags & SQF_LINKUP)
		req->ifm_status |= IFM_ACTIVE;
	req->ifm_active = ifm->ifm_media;
}
