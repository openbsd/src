/*	$OpenBSD: hme.c,v 1.19 1999/04/18 03:13:14 jason Exp $	*/

/*
 * Copyright (c) 1998 Jason L. Wright (jason@thought.net)
 * All rights reserved.
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
 *	This product includes software developed by Jason L. Wright
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Driver for the Happy Meal (hme) ethernet boards
 * Based on information gleaned from reading the
 *	S/Linux driver by David Miller
 *
 * Thanks go to the University of North Carolina at Greensboro, Systems
 * and Networks Department for some of the resources used to develop
 * this driver.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/netisr.h>
#include <net/if_media.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#endif

#include "bpfilter.h"
#if NBPFILTER > 0
#include <net/bpf.h>
#include <net/bpfdesc.h>
#endif

#include <machine/autoconf.h>
#include <sparc/cpu.h>
#include <sparc/sparc/cpuvar.h>
#include <sparc/dev/sbusvar.h>
#include <sparc/dev/dmareg.h>	/* for SBUS_BURST_* */
#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include <sparc/dev/hmereg.h>
#include <sparc/dev/hmevar.h>

int	hmematch	__P((struct device *, void *, void *));
void	hmeattach	__P((struct device *, struct device *, void *));
void	hmewatchdog	__P((struct ifnet *));
int	hmeintr		__P((void *));
int	hmeioctl	__P((struct ifnet *, u_long, caddr_t));
void	hmereset	__P((struct hme_softc *));
void	hmestart	__P((struct ifnet *));
void	hmestop		__P((struct hme_softc *));
void	hmeinit		__P((struct hme_softc *));
void	hme_meminit	__P((struct hme_softc *));

static void	hme_tcvr_bb_writeb  __P((struct hme_softc *, int));
static int	hme_tcvr_bb_readb   __P((struct hme_softc *, int));

static void	hme_poll_stop	__P((struct hme_softc *sc));

static int	hme_rint	__P((struct hme_softc *));
static int	hme_tint	__P((struct hme_softc *));
static int	hme_mint	__P((struct hme_softc *, u_int32_t));
static int	hme_eint	__P((struct hme_softc *, u_int32_t));

static void	hme_reset_rx		__P((struct hme_softc *));
static void	hme_reset_tx		__P((struct hme_softc *));

static struct mbuf *	hme_get __P((struct hme_softc *, int, int));
static void		hme_read __P((struct hme_softc *, int, int));
static int		hme_put __P((struct hme_softc *, int, struct mbuf *));

/*
 * ifmedia glue
 */
static int	hme_mediachange __P((struct ifnet *));
static void	hme_mediastatus __P((struct ifnet *, struct ifmediareq *));

/*
 * mii glue
 */
static int	hme_mii_read __P((struct device *, int, int));
static void	hme_mii_write __P((struct device *, int, int, int));
static void	hme_mii_statchg __P((struct device *));

static void	hme_mcreset __P((struct hme_softc *));

struct cfattach hme_ca = {
	sizeof (struct hme_softc), hmematch, hmeattach
};

struct cfdriver hme_cd = {
	NULL, "hme", DV_IFNET
};

int
hmematch(parent, vcf, aux)
	struct device *parent;
	void *vcf, *aux;
{
	struct cfdata *cf = vcf;
	struct confargs *ca = aux;
	register struct romaux *ra = &ca->ca_ra;

	if (strcmp(cf->cf_driver->cd_name, ra->ra_name) &&
	    strcmp("SUNW,hme", ra->ra_name) &&
	    strcmp("SUNW,qfe", ra->ra_name)) {
		return (0);
	}
	if (!sbus_testdma((struct sbus_softc *)parent, ca))
		return(0);
	return (1);
}

void    
hmeattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct confargs *ca = aux;
	struct hme_softc *sc = (struct hme_softc *)self;
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	int pri;
	/* XXX the following declaration should be elsewhere */
	extern void myetheraddr __P((u_char *));

	if (ca->ca_ra.ra_nintr != 1) {
		printf(": expected 1 interrupt, got %d\n",
			ca->ca_ra.ra_nintr);
		return;
	}
	pri = ca->ca_ra.ra_intr[0].int_pri;

	/* map registers */
	if (ca->ca_ra.ra_nreg != 5) {
		printf(": expected 5 registers, got %d\n", ca->ca_ra.ra_nreg);
		return;
	}
	sc->sc_gr = mapiodev(&(ca->ca_ra.ra_reg[0]), 0,
			ca->ca_ra.ra_reg[0].rr_len);
	sc->sc_txr = mapiodev(&(ca->ca_ra.ra_reg[1]), 0,
			ca->ca_ra.ra_reg[1].rr_len);
	sc->sc_rxr = mapiodev(&(ca->ca_ra.ra_reg[2]), 0,
			ca->ca_ra.ra_reg[2].rr_len);
	sc->sc_cr = mapiodev(&(ca->ca_ra.ra_reg[3]), 0,
			ca->ca_ra.ra_reg[3].rr_len);
	sc->sc_tcvr = mapiodev(&(ca->ca_ra.ra_reg[4]), 0,
			ca->ca_ra.ra_reg[4].rr_len);

	sc->sc_node = ca->ca_ra.ra_node;

	sc->sc_rev = getpropint(ca->ca_ra.ra_node, "hm-rev", -1);
	if (sc->sc_rev == 0xff)
		sc->sc_rev = 0xa0;
	if (sc->sc_rev == 0x20 || sc->sc_rev == 0x21)
		sc->sc_flags = HME_FLAG_20_21;
	else if (sc->sc_rev != 0xa0)
		sc->sc_flags = HME_FLAG_NOT_A0;

	sc->sc_burst = getpropint(ca->ca_ra.ra_node, "burst-sizes", -1);
	if (sc->sc_burst == -1)
		sc->sc_burst = ((struct sbus_softc *)parent)->sc_burst;

        /* Clamp at parent's burst sizes */
	sc->sc_burst &= ((struct sbus_softc *)parent)->sc_burst;

	hme_meminit(sc);

	sbus_establish(&sc->sc_sd, &sc->sc_dev);

	sc->sc_ih.ih_fun = hmeintr;
	sc->sc_ih.ih_arg = sc;
	intr_establish(ca->ca_ra.ra_intr[0].int_pri, &sc->sc_ih);

	/*
	 * Get MAC address from card if 'local-mac-address' property exists.
	 * Otherwise, use the machine's builtin MAC.
	 */
	if (getprop(ca->ca_ra.ra_node, "local-mac-address",
	    sc->sc_arpcom.ac_enaddr, ETHER_ADDR_LEN) <= 0) {
		myetheraddr(sc->sc_arpcom.ac_enaddr);
	}

	printf(" pri %d: address %s rev %d\n", pri,
	    ether_sprintf(sc->sc_arpcom.ac_enaddr), sc->sc_rev);

	sc->sc_mii.mii_ifp = ifp;
	sc->sc_mii.mii_readreg = hme_mii_read;
	sc->sc_mii.mii_writereg = hme_mii_write;
	sc->sc_mii.mii_statchg = hme_mii_statchg;
	ifmedia_init(&sc->sc_mii.mii_media, IFM_IMASK, hme_mediachange,
	    hme_mediastatus);
	mii_phy_probe(self, &sc->sc_mii, 0xffffffff);

	if (LIST_FIRST(&sc->sc_mii.mii_phys) == NULL) {
		ifmedia_add(&sc->sc_mii.mii_media, IFM_ETHER | IFM_NONE,
		    0, NULL);
		ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER | IFM_NONE);
	}
	else
		ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER | IFM_AUTO);

	bcopy(sc->sc_dev.dv_xname, ifp->if_xname, IFNAMSIZ);
	ifp->if_softc = sc;
	ifp->if_start = hmestart;
	ifp->if_ioctl = hmeioctl;
	ifp->if_watchdog = hmewatchdog;
	ifp->if_flags =
		IFF_BROADCAST | IFF_SIMPLEX | IFF_NOTRAILERS | IFF_MULTICAST;

	/* Attach the interface. */
	if_attach(ifp);
	ether_ifattach(ifp);

#if NBPFILTER > 0
	bpfattach(&ifp->if_bpf, ifp, DLT_EN10MB, sizeof(struct ether_header));
#endif
}

/*
 * Start output on interface.
 * We make two assumptions here:
 *  1) that the current priority is set to splnet _before_ this code
 *     is called *and* is returned to the appropriate priority after
 *     return
 *  2) that the IFF_OACTIVE flag is checked before this code is called
 *     (i.e. that the output part of the interface is idle)
 */
void
hmestart(ifp)
	struct ifnet *ifp;
{
	struct hme_softc *sc = ifp->if_softc;
	struct mbuf *m;
	int bix, len;

	if ((ifp->if_flags & (IFF_RUNNING | IFF_OACTIVE)) != IFF_RUNNING)
		return;

	bix = sc->sc_last_td;

	for (;;) {
		IF_DEQUEUE(&ifp->if_snd, m);
		if (m == 0)
			break;
#if NBPFILTER > 0
		/*
		 * If BPF is listening on this interface, let it see the
		 * packet before we commit it to the wire.
		 */
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m);
#endif

		/*
		 * Copy the mbuf chain into the transmit buffer.
		 */
		len = hme_put(sc, bix, m);

		/*
		 * Initialize transmit registers and start transmission.
		 */
		sc->sc_desc->hme_txd[bix].tx_flags =
		    HME_TXD_OWN | HME_TXD_SOP | HME_TXD_EOP |
		    (len & HME_TXD_SIZE);
		sc->sc_txr->tx_pnding = TXR_TP_DMAWAKEUP;

		if (++bix == HME_TX_RING_SIZE)
			bix = 0;

		if (++sc->sc_no_td == HME_TX_RING_SIZE) {
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}
	}

	sc->sc_last_td = bix;
}

#define MAX_STOP_TRIES	16

void
hmestop(sc)
	struct hme_softc *sc;
{
	int tries = 0;

	sc->sc_gr->reset = GR_RESET_ALL;
	while (sc->sc_gr->reset && (tries != MAX_STOP_TRIES))
		DELAY(20);
	if (tries == MAX_STOP_TRIES)
		printf("%s: stop failed\n", sc->sc_dev.dv_xname);
	sc->sc_mii.mii_media_status &= ~IFM_ACTIVE;
}

/*
 * Reset interface.
 */
void
hmereset(sc)
	struct hme_softc *sc;
{
	int s;

	s = splnet();
	hmestop(sc);
	hmeinit(sc);
	splx(s);
}

/*
 * Device timeout/watchdog routine. Entered if the device neglects to generate
 * an interrupt after a transmit has been started on it.
 */
void
hmewatchdog(ifp)
	struct ifnet *ifp;
{
	struct hme_softc *sc = ifp->if_softc;

	log(LOG_ERR, "%s: device timeout\n", sc->sc_dev.dv_xname);
	++sc->sc_arpcom.ac_if.if_oerrors;

	hmereset(sc);
}

int
hmeioctl(ifp, cmd, data)
	struct ifnet *ifp;
	u_long cmd;
	caddr_t data;
{
	struct hme_softc *sc = ifp->if_softc;
	struct ifaddr *ifa = (struct ifaddr *)data;
	struct ifreq *ifr = (struct ifreq *)data;
	int s, error = 0;

	s = splimp();

	if ((error = ether_ioctl(ifp, &sc->sc_arpcom, cmd, data)) > 0) {
		splx(s);
		return error;
	}

	switch (cmd) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			hmeinit(sc);
			arp_ifinit(&sc->sc_arpcom, ifa);
			break;
#endif /* INET */
#ifdef NS
		/* XXX - This code is probably wrong. */
		case AF_NS:
		    {
			struct ns_addr *ina = &IA_SNS(ifa)->sns_addr;

			if (ns_nullhost(*ina))
				ina->x_host = 
				    *(union ns_host *)(sc->sc_arpcom.ac_enaddr);
			else
				bcopy(ina->x_host.c_host,
				    sc->sc_arpcom.ac_enaddr,
				    sizeof(sc->sc_arpcom.ac_enaddr));
			/* Set new address. */
			hmeinit(sc);
			break;
		    }
#endif /* NS */
		default:
			hmeinit(sc);
			break;
		}
		break;

	case SIOCSIFFLAGS:
		if ((ifp->if_flags & IFF_UP) == 0 &&
		    (ifp->if_flags & IFF_RUNNING) != 0) {
			/*
			 * If interface is marked down and it is running, then
			 * stop it.
			 */
			hmestop(sc);
			ifp->if_flags &= ~IFF_RUNNING;
		} else if ((ifp->if_flags & IFF_UP) != 0 &&
			   (ifp->if_flags & IFF_RUNNING) == 0) {
			/*
			 * If interface is marked up and it is stopped, then
			 * start it.
			 */
			hmeinit(sc);
		} else {
			/*
			 * Reset the interface to pick up changes in any other
			 * flags that affect hardware registers.
			 */
			hmestop(sc);
			hmeinit(sc);
		}
#ifdef IEDEBUG   
		if (ifp->if_flags & IFF_DEBUG)
			sc->sc_debug = IED_ALL;
		else
			sc->sc_debug = 0;
#endif
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		error = (cmd == SIOCADDMULTI) ?
			ether_addmulti(ifr, &sc->sc_arpcom):
			ether_delmulti(ifr, &sc->sc_arpcom);

		if (error == ENETRESET) {
			/*
			 * Multicast list has changed; set the hardware filter
			 * accordingly.
			 */
			hme_mcreset(sc);
			error = 0;
		}
		break;
	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr,  &sc->sc_mii.mii_media, cmd);
		break;
	default:
		error = EINVAL;
	}
	splx(s);
	return error;
}

void
hme_meminit(sc)
	struct hme_softc *sc;
{
	struct hme_desc *desc;
	int i;

	if (sc->sc_desc_dva == NULL)
		sc->sc_desc_dva = (struct hme_desc *) dvma_malloc(
		    sizeof(struct hme_desc), &sc->sc_desc, M_NOWAIT);
	if (sc->sc_bufs_dva == NULL)
		sc->sc_bufs_dva = (struct hme_bufs *) dvma_malloc(
		    sizeof(struct hme_bufs), &sc->sc_bufs, M_NOWAIT);

	desc = sc->sc_desc;

	/*
	 * Setup TX descriptors
	 */
	sc->sc_first_td = sc->sc_last_td = sc->sc_no_td = 0;
	for (i = 0; i < HME_TX_RING_SIZE; i++) {
		desc->hme_txd[i].tx_addr =
		    (u_int32_t) &sc->sc_bufs_dva->tx_buf[i][0];
		desc->hme_txd[i].tx_flags = 0;
	}

	/*
	 * Setup RX descriptors
	 */
	sc->sc_last_rd = 0;
	for (i = 0; i < HME_RX_RING_SIZE; i++) {
		desc->hme_rxd[i].rx_addr =
		    (u_int32_t) &sc->sc_bufs_dva->rx_buf[i][0];
		desc->hme_rxd[i].rx_flags = HME_RXD_OWN |
		    ((HME_RX_PKT_BUF_SZ - HME_RX_OFFSET) << 16);
	}
}

void
hmeinit(sc)
	struct hme_softc *sc;
{
	u_int32_t c;
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	struct hme_tcvr *tcvr = sc->sc_tcvr;
	struct hme_cr *cr = sc->sc_cr;
	struct hme_gr *gr = sc->sc_gr;
	struct hme_txr *txr = sc->sc_txr;
	struct hme_rxr *rxr = sc->sc_rxr;

	hme_poll_stop(sc);
	hmestop(sc);

	hme_meminit(sc);

	tcvr->int_mask = 0xffff;

	c = tcvr->cfg;
	if (sc->sc_flags & HME_FLAG_FENABLE)
		tcvr->cfg = c & ~(TCVR_CFG_BENABLE);
	else
		tcvr->cfg = c | TCVR_CFG_BENABLE;

	hme_reset_tx(sc);
	hme_reset_rx(sc);

	cr->rand_seed = sc->sc_arpcom.ac_enaddr[5] |
	    ((sc->sc_arpcom.ac_enaddr[4] << 8) & 0x3f00);
	cr->mac_addr0 = (sc->sc_arpcom.ac_enaddr[0] << 8) |
			   sc->sc_arpcom.ac_enaddr[1];
	cr->mac_addr1 = (sc->sc_arpcom.ac_enaddr[2] << 8) |
			   sc->sc_arpcom.ac_enaddr[3];
	cr->mac_addr2 = (sc->sc_arpcom.ac_enaddr[4] << 8) |
			   sc->sc_arpcom.ac_enaddr[5];

	cr->jsize = HME_DEFAULT_JSIZE;
	cr->ipkt_gap1 = HME_DEFAULT_IPKT_GAP1;
	cr->ipkt_gap2 = HME_DEFAULT_IPKT_GAP2;

	rxr->rx_ring = (u_int32_t)&sc->sc_desc_dva->hme_rxd[0];
	txr->tx_ring = (u_int32_t)&sc->sc_desc_dva->hme_txd[0];

	if (sc->sc_burst & SBUS_BURST_64)
		gr->cfg = GR_CFG_BURST64;
	else if (sc->sc_burst & SBUS_BURST_32)
		gr->cfg = GR_CFG_BURST32;
	else if (sc->sc_burst & SBUS_BURST_16)
		gr->cfg = GR_CFG_BURST16;
	else {
		printf("%s: burst size unknown\n", sc->sc_dev.dv_xname);
		gr->cfg = 0;
	}

	gr->imask = GR_IMASK_SENTFRAME | GR_IMASK_TXPERR |
	              GR_IMASK_GOTFRAME | GR_IMASK_RCNTEXP;

	txr->tx_rsize = (HME_TX_RING_SIZE >> TXR_RSIZE_SHIFT) - 1;
	txr->cfg |= TXR_CFG_DMAENABLE;

	c = RXR_CFG_DMAENABLE | (HME_RX_OFFSET << 3) | (HME_RX_CSUMLOC << 16);
#if HME_RX_RING_SIZE == 32
	c |= RXR_CFG_RINGSIZE32;
#elif HME_RX_RING_SIZE == 64
	c |= RXR_CFG_RINGSIZE64;
#elif HME_RX_RING_SIZE == 128
	c |= RXR_CFG_RINGSIZE128;
#elif HME_RX_RING_SIZE == 256
	c |= RXR_CFG_RINGSIZE256;
#else
#error "HME_RX_RING_SIZE must be 32, 64, 128, or 256."
#endif
	rxr->cfg = c;
	DELAY(20);
	if (c != rxr->cfg)	/* the receiver sometimes misses bits */
	    printf("%s: setting rxreg->cfg failed.\n", sc->sc_dev.dv_xname);

	cr->rx_cfg = 0;
	hme_mcreset(sc);
	DELAY(10);

	cr->tx_cfg |= CR_TXCFG_DGIVEUP;

	c = CR_XCFG_ODENABLE;
	if (sc->sc_flags & HME_FLAG_LANCE)
		c |= (HME_DEFAULT_IPKT_GAP0 << 5) | CR_XCFG_LANCE;
	cr->xif_cfg = c;

	cr->tx_cfg |= CR_TXCFG_ENABLE;	/* enable tx */
	cr->rx_cfg |= CR_RXCFG_ENABLE;	/* enable rx */

	mii_mediachg(&sc->sc_mii);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;
	ifp->if_timer = 0;
}

static void
hme_poll_stop(sc)
	struct hme_softc *sc;
{
	struct hme_tcvr *tcvr = sc->sc_tcvr;

	/* if not polling, or polling not enabled, we're done. */
	if ((sc->sc_flags & (HME_FLAG_POLLENABLE | HME_FLAG_POLL)) != 
	    (HME_FLAG_POLLENABLE | HME_FLAG_POLL))
		return;

	/* Turn off MIF interrupts, and diable polling */
	tcvr->int_mask = 0xffff;
        tcvr->cfg &= ~(TCVR_CFG_PENABLE);
	sc->sc_flags &= ~(HME_FLAG_POLL);
	DELAY(200);
}

#define RESET_TRIES	32

static void
hme_reset_tx(sc)
	struct hme_softc *sc;
{
	int tries = RESET_TRIES;
	struct hme_cr *cr = sc->sc_cr;

	cr->tx_swreset = 0;
	while (tries-- && (cr->tx_swreset & 1))
		DELAY(20);

	if (!tries)
		printf("%s: reset tx failed\n", sc->sc_dev.dv_xname);
}

static void
hme_reset_rx(sc)
	struct hme_softc *sc;
{
	int tries = RESET_TRIES;
	struct hme_cr *cr = sc->sc_cr;

	cr->rx_swreset = 0;
	while (tries-- && (cr->rx_swreset & 1))
		DELAY(20);

	if (!tries)
		printf("%s: reset rx failed\n", sc->sc_dev.dv_xname);
}

/*
 * mif interrupt
 */
static int
hme_mint(sc, why)
	struct hme_softc *sc;
	u_int32_t why;
{
	printf("%s: link status changed\n", sc->sc_dev.dv_xname);
	hme_poll_stop(sc);
	return 1;
}

/*
 * transmit interrupt
 */
static int
hme_tint(sc)
	struct hme_softc *sc;
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	struct hme_cr *cr = sc->sc_cr;
	int bix;
	struct hme_txd txd;

	/*
	 * Get collision counters
	 */
	ifp->if_collisions += cr->ex_ctr + cr->lt_ctr + cr->fc_ctr + cr->nc_ctr;
	cr->ex_ctr = 0;
	cr->lt_ctr = 0;
	cr->fc_ctr = 0;
	cr->nc_ctr = 0;

	bix = sc->sc_first_td;

	for (;;) {
		if (sc->sc_no_td <= 0)
			break;

		bcopy(&sc->sc_desc->hme_txd[bix], &txd, sizeof(txd));

		if (txd.tx_flags & HME_TXD_OWN)
			break;

		ifp->if_flags &= ~IFF_OACTIVE;
		ifp->if_opackets++;

		if (++bix == HME_TX_RING_SIZE)
			bix = 0;

		--sc->sc_no_td;
	}

	sc->sc_first_td = bix;

	hmestart(ifp);

	if (sc->sc_no_td == 0)
		ifp->if_timer = 0;

	return 1;
}

static int
hme_rint(sc)
	struct hme_softc *sc;
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	int bix, len;
	struct hme_rxd rxd;

	bix = sc->sc_last_rd;

	for (;;) {
		bcopy(&sc->sc_desc->hme_rxd[bix], &rxd, sizeof(rxd));
		len = rxd.rx_flags >> 16;

		if (rxd.rx_flags & HME_RXD_OWN)
			break;

		if (rxd.rx_flags & HME_RXD_OVERFLOW)
			ifp->if_ierrors++;
		else
			hme_read(sc, bix, len);

		rxd.rx_flags = HME_RXD_OWN |
		    ((HME_RX_PKT_BUF_SZ - HME_RX_OFFSET) << 16);
		bcopy(&rxd, &sc->sc_desc->hme_rxd[bix], sizeof(rxd));

		if (++bix == HME_RX_RING_SIZE)
			bix = 0;
	}

	sc->sc_last_rd = bix;

	return 1;
}

/*
 * error interrupt
 */
static int
hme_eint(sc, why)
	struct hme_softc *sc;
	u_int32_t why;
{
	if (why & GR_STAT_ALL_ERRORS) {
		printf("%s: stat=%b, resetting.\n", sc->sc_dev.dv_xname,
		    why, GR_STAT_BITS);
		hmereset(sc);
	}

	return 1;
}

/*
 * Interrupt handler
 */
int
hmeintr(v)
	void *v;
{
	struct hme_softc *sc = (struct hme_softc *)v;
	struct hme_gr *gr = sc->sc_gr;
	u_int32_t why;
	int r = 0;

	why = gr->stat;

	if (why & GR_STAT_ALL_ERRORS)
		r |= hme_eint(sc, why);

	if (why & GR_STAT_MIFIRQ)
		r |= hme_mint(sc, why);

	if (why & (GR_STAT_TXALL | GR_STAT_HOSTTOTX))
		r |= hme_tint(sc);

	if (why & GR_STAT_RXTOHOST)
		r |= hme_rint(sc);

	return (r);
}

static struct mbuf *
hme_get(sc, idx, totlen)
	struct hme_softc *sc;
	int idx, totlen;
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	struct mbuf *m;
	struct mbuf *top, **mp;
	int len, pad, boff = 0;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return NULL;
	m->m_pkthdr.rcvif = ifp;
	m->m_pkthdr.len = totlen;
	pad = ALIGN(sizeof(struct ether_header)) - sizeof(struct ether_header);
	m->m_data += pad;
	len = MHLEN - pad;
	top = NULL;
	mp = &top;

	while (totlen > 0) {
		if (top) {
			MGET(m, M_DONTWAIT, MT_DATA);
			if (m == NULL) {
				m_freem(top);
				return NULL;
			}
			len = MLEN;
		}
		if (top && totlen >= MINCLSIZE) {
			MCLGET(m, M_DONTWAIT);
			if (m->m_flags & M_EXT)
				len = MCLBYTES;
		}
		m->m_len = len = min(totlen, len);
		bcopy(&sc->sc_bufs->rx_buf[idx][boff + HME_RX_OFFSET],
		    mtod(m, caddr_t), len);
		boff += len;
		totlen -= len;
		*mp = m;
		mp = &m->m_next;
	}

	return top;
}

static int
hme_put(sc, idx, m)
	struct hme_softc *sc;
	int idx;
	struct mbuf *m;
{
	struct mbuf *n;
	int len, tlen = 0, boff = 0;

	for (; m; m = n) {
		len = m->m_len;
		if (len == 0) {
			MFREE(m, n);
			continue;
		}
		bcopy(mtod(m, caddr_t), &sc->sc_bufs->tx_buf[idx][boff], len);
		boff += len;
		tlen += len;
		MFREE(m, n);
	}
	return tlen;
}

static void
hme_read(sc, idx, len)
	struct hme_softc *sc;
	int idx, len;
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	struct ether_header *eh;
	struct mbuf *m;

	if (len <= sizeof(struct ether_header) ||
	    len > ETHERMTU + sizeof(struct ether_header)) {
		printf("%s: invalid packet size %d; dropping\n",
		    ifp->if_xname, len);
		ifp->if_ierrors++;
		return;
	}

	/* Pull packet off interface. */
	m = hme_get(sc, idx, len);
	if (m == NULL) {
		ifp->if_ierrors++;
		return;
	}

	ifp->if_ipackets++;

	/* We assume that the header fit entirely in one mbuf. */
	eh = mtod(m, struct ether_header *);

#if NBPFILTER > 0
	/*
	 * Check if there's a BPF listener on this interface.
	 * If so, hand off the raw packet to BPF.
	 */
	if (ifp->if_bpf)
		bpf_mtap(ifp->if_bpf, m);
#endif
	/* Pass the packet up, with the ether header sort-of removed. */
	m_adj(m, sizeof(struct ether_header));
	ether_input(ifp, eh, m);
}

/*
 * Program the multicast receive filter.
 */
static void
hme_mcreset(sc)
	struct hme_softc *sc;
{
	struct arpcom *ac = &sc->sc_arpcom;
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	struct hme_cr *cr = sc->sc_cr;
	u_int32_t crc;
	u_int16_t hash[4];
	u_int8_t octet;
	int i, j;
	struct ether_multi *enm;
	struct ether_multistep step;

	if (ifp->if_flags & IFF_PROMISC) {
		cr->rx_cfg |= CR_RXCFG_PMISC;
		return;
	}
	else
		cr->rx_cfg &= ~CR_RXCFG_PMISC;

	if (ifp->if_flags & IFF_ALLMULTI) {
		cr->htable3 = 0xffff;
		cr->htable2 = 0xffff;
		cr->htable1 = 0xffff;
		cr->htable0 = 0xffff;
		cr->rx_cfg |= CR_RXCFG_HENABLE;
		return;
	}

	hash[3] = hash[2] = hash[1] = hash[0] = 0;

	ETHER_FIRST_MULTI(step, ac, enm);
	while (enm != NULL) {
		if (bcmp(enm->enm_addrlo, enm->enm_addrhi, ETHER_ADDR_LEN)) {
			/*
			 * We must listen to a range of multicast
			 * addresses.  For now, just accept all
			 * multicasts, rather than trying to set only
			 * those filter bits needed to match the range.
			 * (At this time, the only use of address
			 * ranges is for IP multicast routing, for
			 * which the range is big enough to require
			 * all bits set.)
			 */
			cr->htable3 = 0xffff;
			cr->htable2 = 0xffff;
			cr->htable1 = 0xffff;
			cr->htable0 = 0xffff;
			cr->rx_cfg |= CR_RXCFG_HENABLE;
			ifp->if_flags |= IFF_ALLMULTI;
			return;
		}

		crc = 0xffffffff;

		for (i = 0; i < ETHER_ADDR_LEN; i++) {
			octet = enm->enm_addrlo[i];

			for (j = 0; j < 8; j++) {
				if ((crc & 1) ^ (octet & 1)) {
					crc >>= 1;
					crc ^= MC_POLY_LE;
				}
				else
					crc >>= 1;
				octet >>= 1;
			}
		}

		crc >>=26;
		hash[crc >> 4] |= 1 << (crc & 0xf);
		ETHER_NEXT_MULTI(step, enm);
	}
	cr->htable3 = hash[3];
	cr->htable2 = hash[2];
	cr->htable1 = hash[1];
	cr->htable0 = hash[0];
	cr->rx_cfg |= CR_RXCFG_HENABLE;
	ifp->if_flags &= ~IFF_ALLMULTI;
}

/*
 * Writing to the serial BitBang, is a matter of putting the bit
 * into the data register, then strobing the clock.
 */
static void
hme_tcvr_bb_writeb(sc, b)
	struct hme_softc *sc;
	int b;
{
	sc->sc_tcvr->bb_data = b & 0x1;
	sc->sc_tcvr->bb_clock = 0;
	sc->sc_tcvr->bb_clock = 1;
}

/*
 * Read a bit from a PHY, if the PHY is not our internal or external
 * phy addr, just return all zero's.
 */
static int
hme_tcvr_bb_readb(sc, phy)
	struct hme_softc *sc;
	int phy;
{
	int ret;

	sc->sc_tcvr->bb_clock = 0;
	DELAY(10);

	if (phy == TCVR_PHYADDR_ITX)
		ret = sc->sc_tcvr->cfg & TCVR_CFG_MDIO0;
	else if (phy == TCVR_PHYADDR_ETX)
		ret = sc->sc_tcvr->cfg & TCVR_CFG_MDIO1;
	else
		ret = 0;

	sc->sc_tcvr->bb_clock = 1;

	return ((ret) ? 1 : 0);
}

static void
hme_mii_write(self, phy, reg, val)
	struct device *self;
	int phy, reg, val;
{
	struct hme_softc *sc = (struct hme_softc *)self;
	struct hme_tcvr *tcvr = sc->sc_tcvr;
	int tries = 16, i;

	if (sc->sc_flags & HME_FLAG_FENABLE) {
		tcvr->frame = (FRAME_WRITE | phy << 23) |
			      ((reg & 0xff) << 18) |
			      (val & 0xffff);
		while (!(tcvr->frame & 0x10000) && (tries != 0)) {
			tries--;
			DELAY(200);
		}
		if (!tries)
			printf("%s: mii_write failed\n", sc->sc_dev.dv_xname);
		return;
	}

	tcvr->bb_oenab = 1;

	for (i = 0; i < 32; i++)
		hme_tcvr_bb_writeb(sc, 1);

	hme_tcvr_bb_writeb(sc, (MII_COMMAND_START >> 1) & 1);
	hme_tcvr_bb_writeb(sc, MII_COMMAND_START & 1);
	hme_tcvr_bb_writeb(sc, (MII_COMMAND_WRITE >> 1) & 1);
	hme_tcvr_bb_writeb(sc, MII_COMMAND_WRITE & 1);

	for (i = 4; i >= 0; i--)
		hme_tcvr_bb_writeb(sc, (phy >> i) & 1);

	for (i = 4; i >= 0; i--)
		hme_tcvr_bb_writeb(sc, (reg >> i) & 1);

	for (i = 15; i >= 0; i--)
		hme_tcvr_bb_writeb(sc, (reg >> i) & 1);

	tcvr->bb_oenab = 0;
}

static int
hme_mii_read(self, phy, reg)
	struct device *self;
	int phy, reg;
{
	struct hme_softc *sc = (struct hme_softc *)self;
	struct hme_tcvr *tcvr = sc->sc_tcvr;
	int tries = 16, i, ret;

	/* Use the frame if possible */
	if (sc->sc_flags & HME_FLAG_FENABLE) {
		tcvr->frame = (FRAME_READ | phy << 23) |
			      ((reg & 0xff) << 18);
		while (!(tcvr->frame & 0x10000) && (tries != 0)) {
			tries--;
			DELAY(20);
		}
		if (!tries) {
			printf("%s: mii_read failed\n", sc->sc_dev.dv_xname);
			return 0;
		}
		return (tcvr->frame & 0xffff);
	}

	tcvr->bb_oenab = 1;

	for (i = 0; i < 32; i++)		/* make bitbang idle */
		hme_tcvr_bb_writeb(sc, 1);

	hme_tcvr_bb_writeb(sc, (MII_COMMAND_START >> 1) & 1);
	hme_tcvr_bb_writeb(sc, MII_COMMAND_START & 1);
	hme_tcvr_bb_writeb(sc, (MII_COMMAND_READ >> 1) & 1);
	hme_tcvr_bb_writeb(sc, MII_COMMAND_READ & 1);

	for (i = 4; i >= 0; i--)
		hme_tcvr_bb_writeb(sc, (phy >> i) & 1);

	for (i = 4; i >= 0; i--)
		hme_tcvr_bb_writeb(sc, (reg >> i) & 1);

	tcvr->bb_oenab = 0;	                /* turn off bitbang intrs */

	hme_tcvr_bb_readb(sc, phy);		/* ignore... */

	for (i = 15; i >= 15; i--)		/* read value */
		ret |= hme_tcvr_bb_readb(sc, phy) << i;

	hme_tcvr_bb_readb(sc, phy);			/* ignore... */
	hme_tcvr_bb_readb(sc, phy);			/* ignore... */
	hme_tcvr_bb_readb(sc, phy);			/* ignore... */

	return ret;
}

static int
hme_mediachange(ifp)
	struct ifnet *ifp;
{
	if (ifp->if_flags & IFF_UP)
		hmeinit(ifp->if_softc);
	return (0);
}

static void
hme_mediastatus(ifp, ifmr)
	struct ifnet *ifp;
	struct ifmediareq *ifmr;
{
	struct hme_softc *sc = (struct hme_softc *)ifp->if_softc;

	mii_pollstat(&sc->sc_mii);
	ifmr->ifm_active = sc->sc_mii.mii_media_active;
	ifmr->ifm_status = sc->sc_mii.mii_media_status;
}

static void
hme_mii_statchg(self)
	struct device *self;
{
	struct hme_softc *sc = (struct hme_softc *)sc;
	struct hme_cr *cr = sc->sc_cr;

	if (sc->sc_mii.mii_media_active & IFM_FDX)
		cr->tx_cfg |= CR_TXCFG_FULLDPLX;
	else
		cr->tx_cfg &= ~CR_TXCFG_FULLDPLX;
}
