/*	$OpenBSD: hme.c,v 1.4 1998/07/17 21:33:07 jason Exp $	*/

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
#include <sparc/dev/stp2002var.h>
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

static void	hme_tcvr_write	    __P((struct hme_softc *, int reg,
					u_short val));
static int	hme_tcvr_read	    __P((struct hme_softc *, int reg));
static void	hme_tcvr_bb_write   __P((struct hme_softc *, int reg,
					u_short val));
static int	hme_tcvr_bb_read    __P((struct hme_softc *, int reg));
static void	hme_tcvr_bb_writeb  __P((struct hme_softc *, int b));
static int	hme_tcvr_bb_readb   __P((struct hme_softc *));
static void	hme_tcvr_check	    __P((struct hme_softc *));
static int	hme_tcvr_reset	    __P((struct hme_softc *));

static void	hme_poll_stop	__P((struct hme_softc *sc));

static int	hme_mint	__P((struct hme_softc *, u_int32_t));
static int	hme_eint	__P((struct hme_softc *, u_int32_t));

static void	hme_auto_negotiate __P((struct hme_softc *));
static void	hme_manual_negotiate __P((struct hme_softc *));
static void	hme_negotiate_watchdog __P((void *));
static void	hme_print_link_mode __P((struct hme_softc *));
static void	hme_set_initial_advertisement	__P((struct hme_softc *));

static void	hme_reset_rx		__P((struct hme_softc *));
static void	hme_reset_tx		__P((struct hme_softc *));
static void	hme_tx_dmawakeup	__P((void *v));

static void hme_mcreset __P((struct hme_softc *));

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
	    strcmp("SUNW,hme", ra->ra_name)) {
		return (0);
	}
	return (1);
}

void    
hmeattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct confargs *ca = aux;
	struct hme_softc *sc = (struct hme_softc *)self;
	struct ifnet *ifp = &sc->sc_stp.stp_arpcom.ac_if;
	int pri;
	/* XXX the following declaration should be elsewhere */
	extern void myetheraddr __P((u_char *));

	if (ca->ca_ra.ra_nintr != 1) {
		printf(": expected 1 interrupt, got %d\n", ca->ca_ra.ra_nintr);
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

	sc->sc_burst = ((struct sbus_softc *)parent)->sc_burst;

	sc->sc_stp.stp_tx_dmawakeup = hme_tx_dmawakeup;
	stp2002_meminit(&sc->sc_stp);

	hme_set_initial_advertisement(sc);

	sbus_establish(&sc->sc_sd, &sc->sc_dev);

	sc->sc_ih.ih_fun = hmeintr;
	sc->sc_ih.ih_arg = sc;
	intr_establish(ca->ca_ra.ra_intr[0].int_pri, &sc->sc_ih);

	/*
	 * Get MAC address from card if 'local-mac-address' property exists.
	 * Otherwise, use the machine's builtin MAC.
	 */
	if (getprop(ca->ca_ra.ra_node, "local-mac-address",
			sc->sc_stp.stp_arpcom.ac_enaddr, ETHER_ADDR_LEN) <= 0) {
		myetheraddr(sc->sc_stp.stp_arpcom.ac_enaddr);
	}

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

	sc->sc_an_state = HME_TIMER_DONE;
	sc->sc_an_ticks = 0;

	printf(" pri %d: address %s rev %d\n", pri,
		ether_sprintf(sc->sc_stp.stp_arpcom.ac_enaddr), sc->sc_rev);

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
	struct hme_softc *sc = (struct hme_softc *)ifp->if_softc;

	stp2002_start(&sc->sc_stp);
}

void
hme_tx_dmawakeup(v)
	void *v;
{
	struct hme_softc *sc = (struct hme_softc *)v;

	sc->sc_txr->tx_pnding = TXR_TP_DMAWAKEUP;
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
	++sc->sc_stp.stp_arpcom.ac_if.if_oerrors;

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

	if ((error = ether_ioctl(ifp, &sc->sc_stp.stp_arpcom, cmd, data)) > 0) {
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
			arp_ifinit(&sc->sc_stp.stp_arpcom, ifa);
			break;
#endif /* INET */
#ifdef NS
		/* XXX - This code is probably wrong. */
		case AF_NS:
		    {
			struct ns_addr *ina = &IA_SNS(ifa)->sns_addr;

			if (ns_nullhost(*ina))
				ina->x_host = 
				    *(union ns_host *)(sc->sc_stp.stp_arpcom.ac_enaddr);
			else
				bcopy(ina->x_host.c_host,
				    sc->sc_stp.stp_arpcom.ac_enaddr,
				    sizeof(sc->sc_stp.stp_arpcom.ac_enaddr));
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
		sc->sc_promisc = ifp->if_flags & IFF_PROMISC;
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
			ether_addmulti(ifr, &sc->sc_stp.stp_arpcom):
			ether_delmulti(ifr, &sc->sc_stp.stp_arpcom);

		if (error == ENETRESET) {
			/*
			 * Multicast list has changed; set the hardware filter
			 * accordingly.
			 */
			hme_mcreset(sc);
			error = 0;
		}
		break;
	default:
		error = EINVAL;
	}
	splx(s);
	return error;
}

void
hmeinit(sc)
	struct hme_softc *sc;
{
	u_int32_t c;
	struct hme_tcvr *tcvr = sc->sc_tcvr;
	struct hme_cr *cr = sc->sc_cr;
	struct hme_gr *gr = sc->sc_gr;
	struct hme_txr *txr = sc->sc_txr;
	struct hme_rxr *rxr = sc->sc_rxr;

	hme_poll_stop(sc);
	hmestop(sc);

	stp2002_meminit(&sc->sc_stp);

	tcvr->int_mask = 0xffff;

	c = tcvr->cfg;
	if (sc->sc_flags & HME_FLAG_FENABLE)
		tcvr->cfg = c & ~(TCVR_CFG_BENABLE);
	else
		tcvr->cfg = c | TCVR_CFG_BENABLE;

	hme_tcvr_check(sc);
	switch (sc->sc_tcvr_type) {
	case HME_TCVR_NONE:
		printf("%s: no transceiver type!\n", sc->sc_dev.dv_xname);
		return;
	case HME_TCVR_INTERNAL:
		cr->xif_cfg = 0;
		break;
	case HME_TCVR_EXTERNAL:
		cr->xif_cfg = CR_XCFG_MIIDISAB;
		break;
	}
	hme_tcvr_reset(sc);

	hme_reset_tx(sc);
	hme_reset_rx(sc);

	cr->rand_seed = sc->sc_stp.stp_arpcom.ac_enaddr[5] |
	    ((sc->sc_stp.stp_arpcom.ac_enaddr[4] << 8) & 0x3f00);
	cr->mac_addr0 = (sc->sc_stp.stp_arpcom.ac_enaddr[0] << 8) |
			   sc->sc_stp.stp_arpcom.ac_enaddr[1];
	cr->mac_addr1 = (sc->sc_stp.stp_arpcom.ac_enaddr[2] << 8) |
			   sc->sc_stp.stp_arpcom.ac_enaddr[3];
	cr->mac_addr2 = (sc->sc_stp.stp_arpcom.ac_enaddr[4] << 8) |
			   sc->sc_stp.stp_arpcom.ac_enaddr[5];

	cr->jsize = HME_DEFAULT_JSIZE;
	cr->ipkt_gap1 = HME_DEFAULT_IPKT_GAP1;
	cr->ipkt_gap2 = HME_DEFAULT_IPKT_GAP2;
	cr->htable3 = 0;
	cr->htable2 = 0;
	cr->htable1 = 0;
	cr->htable0 = 0;

	rxr->rx_ring = sc->sc_stp.stp_rx_dvma;
	txr->tx_ring = sc->sc_stp.stp_tx_dvma;

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

	txr->tx_rsize = (STP_TX_RING_SIZE >> TXR_RSIZE_SHIFT) - 1;
	txr->cfg |= TXR_CFG_DMAENABLE;

	c = RXR_CFG_DMAENABLE | (STP_RX_OFFSET << 3) | (STP_RX_CSUMLOC << 16);
#if STP_RX_RING_SIZE == 32
	c |= RXR_CFG_RINGSIZE32;
#elif STP_RX_RING_SIZE == 64
	c |= RXR_CFG_RINGSIZE64;
#elif STP_RX_RING_SIZE == 128
	c |= RXR_CFG_RINGSIZE128;
#elif STP_RX_RING_SIZE == 256
	c |= RXR_CFG_RINGSIZE256;
#else
#error "STP_RX_RING_SIZE must be 32, 64, 128, or 256."
#endif
	rxr->cfg = c;
	DELAY(20);
	if (c != rxr->cfg)	/* the receiver sometimes misses bits */
	    printf("%s: setting rxreg->cfg failed.\n", sc->sc_dev.dv_xname);
	
	cr->rx_cfg = CR_RXCFG_HENABLE;
	DELAY(10);

	c = CR_TXCFG_DGIVEUP;
	if (sc->sc_flags & HME_FLAG_FULL)
		c |= CR_TXCFG_FULLDPLX;
	cr->tx_cfg = c;

	c = CR_XCFG_ODENABLE;
	if (sc->sc_flags & HME_FLAG_LANCE)
		c |= (HME_DEFAULT_IPKT_GAP0 << 5) | CR_XCFG_LANCE;
	if (sc->sc_tcvr_type == HME_TCVR_EXTERNAL)
		c |= CR_XCFG_MIIDISAB;
	cr->xif_cfg = c;

	cr->tx_cfg |= CR_TXCFG_ENABLE;	/* enable tx */
	cr->rx_cfg |= CR_RXCFG_ENABLE;	/* enable rx */

	hme_auto_negotiate(sc);
}

static void
hme_set_initial_advertisement(sc)
	struct hme_softc *sc;
{
	hmestop(sc);
	sc->sc_tcvr->int_mask = 0xffff;
	if (sc->sc_flags & HME_FLAG_FENABLE)
		sc->sc_tcvr->cfg &= ~(TCVR_CFG_BENABLE);
	else
		sc->sc_tcvr->cfg |= TCVR_CFG_BENABLE;

	hme_tcvr_check(sc);
	switch (sc->sc_tcvr_type) {
	case HME_TCVR_NONE:
		return;
	case HME_TCVR_INTERNAL:
		sc->sc_cr->xif_cfg = 0;
		break;
	case HME_TCVR_EXTERNAL:
		sc->sc_cr->xif_cfg = CR_XCFG_MIIDISAB;
		break;
	}
	if (hme_tcvr_reset(sc))
		return;

	/* grab the supported modes and advertised modes */
	sc->sc_sw.bmsr = hme_tcvr_read(sc, DP83840_BMSR);
	sc->sc_sw.anar = hme_tcvr_read(sc, DP83840_ANAR);

	/* If 10BaseT Half duplex supported, advertise it, and so on... */
	if (sc->sc_sw.bmsr & BMSR_10BASET_HALF)
		sc->sc_sw.anar |= ANAR_10;
	else
		sc->sc_sw.anar &= ~(ANAR_10);

	if (sc->sc_sw.bmsr & BMSR_10BASET_FULL)
		sc->sc_sw.anar |= ANAR_10_FD;
	else
		sc->sc_sw.anar &= ~(ANAR_10_FD);

	if (sc->sc_sw.bmsr & BMSR_100BASETX_HALF)
		sc->sc_sw.anar |= ANAR_TX;
	else
		sc->sc_sw.anar &= ~(ANAR_TX);

	if (sc->sc_sw.bmsr & BMSR_100BASETX_FULL)
		sc->sc_sw.anar |= ANAR_TX_FD;
	else
		sc->sc_sw.anar &= ~(ANAR_TX_FD);

	/* Inform card about what it should advertise */
	hme_tcvr_write(sc, DP83840_ANAR, sc->sc_sw.anar);
}

#define XCVR_RESET_TRIES	16
#define XCVR_UNISOLATE_TRIES	32

static int
hme_tcvr_reset(sc)
	struct hme_softc *sc;
{
	struct hme_tcvr *tcvr = sc->sc_tcvr;
	u_int32_t cfg;
	int result, tries = XCVR_RESET_TRIES;

	cfg = tcvr->cfg;
	if (sc->sc_tcvr_type == HME_TCVR_EXTERNAL) {
		tcvr->cfg = cfg & ~(TCVR_CFG_PSELECT);
		sc->sc_tcvr_type = HME_TCVR_INTERNAL;
		sc->sc_phyaddr = TCVR_PHYADDR_ITX;
		hme_tcvr_write(sc, DP83840_BMCR,
			(BMCR_LOOPBACK | BMCR_PDOWN | BMCR_ISOLATE));
		result = hme_tcvr_read(sc, DP83840_BMCR);
		if (result == TCVR_FAILURE) {
			printf("%s: tcvr_reset failed\n", sc->sc_dev.dv_xname);
			return -1;
		}
		tcvr->cfg = cfg | TCVR_CFG_PSELECT;
		sc->sc_tcvr_type = HME_TCVR_EXTERNAL;
		sc->sc_phyaddr = TCVR_PHYADDR_ETX;
	}
	else {
		if (cfg & TCVR_CFG_MDIO1) {
			tcvr->cfg = cfg | TCVR_CFG_PSELECT;
			hme_tcvr_write(sc, DP83840_BMCR,
				(BMCR_LOOPBACK | BMCR_PDOWN | BMCR_ISOLATE));
			result = hme_tcvr_read(sc, DP83840_BMCR);
			if (result == TCVR_FAILURE) {
				printf("%s: tcvr_reset failed\n",
					sc->sc_dev.dv_xname);
				return -1;
			}
			tcvr->cfg = cfg & ~(TCVR_CFG_PSELECT);
			sc->sc_tcvr_type = HME_TCVR_INTERNAL;
			sc->sc_phyaddr = TCVR_PHYADDR_ITX;
		}
	}

	hme_tcvr_write(sc, DP83840_BMCR, BMCR_RESET);

	while (--tries) {
		result = hme_tcvr_read(sc, DP83840_BMCR);
		if (result == TCVR_FAILURE)
			return -1;
		sc->sc_sw.bmcr = result;
		if (!(result & BMCR_RESET))
			break;
		DELAY(200);
	}
	if (!tries) {
		printf("%s: bmcr reset failed\n", sc->sc_dev.dv_xname);
		return -1;
	}

	sc->sc_sw.bmsr = hme_tcvr_read(sc, DP83840_BMSR);
	sc->sc_sw.phyidr1 = hme_tcvr_read(sc, DP83840_PHYIDR1);
	sc->sc_sw.phyidr2 = hme_tcvr_read(sc, DP83840_PHYIDR2);
	sc->sc_sw.anar = hme_tcvr_read(sc, DP83840_BMSR);

	sc->sc_sw.bmcr &= ~(BMCR_ISOLATE);
	hme_tcvr_write(sc, DP83840_BMCR, sc->sc_sw.bmcr);

	tries = XCVR_UNISOLATE_TRIES;
	while (--tries) {
		result = hme_tcvr_read(sc, DP83840_BMCR);
		if (result == TCVR_FAILURE)
			return -1;
		if (!(result & BMCR_ISOLATE))
			break;
		DELAY(200);
	}
	if (!tries) {
		printf("%s: bmcr unisolate failed\n", sc->sc_dev.dv_xname);
		return -1;
	}

	result = hme_tcvr_read(sc, DP83840_PCR);
	hme_tcvr_write(sc, DP83840_PCR, (result | PCR_CIM_DIS));
	return 0;
}


/*
 * We need to know whether we are using an internal or external transceiver.
 */
static void
hme_tcvr_check(sc)
	struct hme_softc *sc;
{
	struct hme_tcvr *tcvr = sc->sc_tcvr;
	u_int32_t cfg = tcvr->cfg;

	/* polling? */
	if (sc->sc_flags & HME_FLAG_POLL) {
		if (sc->sc_tcvr_type == HME_TCVR_INTERNAL) {
			hme_poll_stop(sc);
			sc->sc_phyaddr = TCVR_PHYADDR_ETX;
			sc->sc_tcvr_type = HME_TCVR_EXTERNAL;
			cfg &= ~(TCVR_CFG_PENABLE);
			cfg |= TCVR_CFG_PSELECT;
			tcvr->cfg = cfg;
		}
		else {
			if (!(tcvr->status >> 16)) {
				hme_poll_stop(sc);
				sc->sc_phyaddr = TCVR_PHYADDR_ITX;
				sc->sc_tcvr_type = HME_TCVR_INTERNAL;
				cfg &= ~(TCVR_CFG_PSELECT);
				tcvr->cfg = cfg;
			}
		}
	}
	else {
		u_int32_t cfg2 = tcvr->cfg;

		if (cfg2 & TCVR_CFG_MDIO1) {
			tcvr->cfg = cfg | TCVR_CFG_PSELECT;
			sc->sc_phyaddr = TCVR_PHYADDR_ETX;
			sc->sc_tcvr_type = HME_TCVR_EXTERNAL;
		}
		else {
			if (cfg2 & TCVR_CFG_MDIO0) {
				tcvr->cfg = cfg & ~(TCVR_CFG_PSELECT);
				sc->sc_phyaddr = TCVR_PHYADDR_ITX;
				sc->sc_tcvr_type = HME_TCVR_INTERNAL;
			}
			else {
				sc->sc_tcvr_type = HME_TCVR_NONE;
			}
		}
	}
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

#define XCVR_WRITE_TRIES	16

static void
hme_tcvr_write(sc, reg, val)
	struct hme_softc *sc;
	int reg;
	u_short val;
{
	struct hme_tcvr *tcvr = sc->sc_tcvr;
	int tries = XCVR_WRITE_TRIES;

	/* Use the bitbang? */
	if (! (sc->sc_flags & HME_FLAG_FENABLE))
		return hme_tcvr_bb_write(sc, reg, val);

	/* No, good... we just write to the tcvr frame */
	tcvr->frame = (FRAME_WRITE | sc->sc_phyaddr << 23) |
		      ((reg & 0xff) << 18) |
		      (val & 0xffff);
	while (!(tcvr->frame & 0x10000) && (tries != 0)) {
		tries--;
		DELAY(200);
	}

	if (!tries)
		printf("%s: tcvr_write failed\n", sc->sc_dev.dv_xname);
}

#define XCVR_READ_TRIES	16

static int
hme_tcvr_read(sc, reg)
	struct hme_softc *sc;
	int reg;
{
	struct hme_tcvr *tcvr = sc->sc_tcvr;
	int tries = XCVR_READ_TRIES;

	if (sc->sc_tcvr_type == HME_TCVR_NONE) {
		printf("%s: no transceiver type\n", sc->sc_dev.dv_xname);
		return TCVR_FAILURE;
	}

	/* Use the bitbang? */
	if (! (sc->sc_flags & HME_FLAG_FENABLE))
		return hme_tcvr_bb_read(sc, reg);

	/* No, good... we just write/read to the tcvr frame */
	tcvr->frame = (FRAME_READ | sc->sc_phyaddr << 23) |
		      ((reg & 0xff) << 18);
	while (!(tcvr->frame & 0x10000) && (tries != 0)) {
		tries--;
		DELAY(200);
	}

	if (!tries) {
		printf("%s: tcvr_write failed\n", sc->sc_dev.dv_xname);
		return TCVR_FAILURE;
	}
	return (tcvr->frame & 0xffff);
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

static int
hme_tcvr_bb_readb(sc)
	struct hme_softc *sc;
{
	int ret;

	sc->sc_tcvr->bb_clock = 0;
	DELAY(10);
	if (sc->sc_tcvr_type == HME_TCVR_INTERNAL)
		ret = sc->sc_tcvr->cfg & TCVR_CFG_MDIO0;
	else
		ret = sc->sc_tcvr->cfg & TCVR_CFG_MDIO1;
	sc->sc_tcvr->bb_clock = 1;
	return ((ret) ? 1 : 0);
}

static void
hme_tcvr_bb_write(sc, reg, val)
	struct hme_softc *sc;
	int reg;
	u_short val;
{
	struct hme_tcvr *tcvr = sc->sc_tcvr;
	int i;

	tcvr->bb_oenab = 1;	                /* turn on bitbang intrs */

	for (i = 0; i < 32; i++)		/* make bitbang idle */
		hme_tcvr_bb_writeb(sc, 1);

	hme_tcvr_bb_writeb(sc, 0);		/* 0101 signals a write */
	hme_tcvr_bb_writeb(sc, 1);
	hme_tcvr_bb_writeb(sc, 0);
	hme_tcvr_bb_writeb(sc, 1);

	for (i = 4; i >= 0; i--)		/* send PHY addr */
		hme_tcvr_bb_writeb(sc, ((sc->sc_phyaddr & 0xff) >> i) & 0x1);

	for (i = 4; i >= 0; i--)		/* send register num */
		hme_tcvr_bb_writeb(sc, ((reg & 0xff) >> i) & 0x1);

	hme_tcvr_bb_writeb(sc, 1);		/* get ready for data */
	hme_tcvr_bb_writeb(sc, 0);

	for (i = 15; i >= 0; i--)		/* send new value */
		hme_tcvr_bb_writeb(sc, (val >> i) & 0x1);

	tcvr->bb_oenab = 0;	                /* turn off bitbang intrs */
}

static int
hme_tcvr_bb_read(sc, reg)
	struct hme_softc *sc;
	int reg;
{
	struct hme_tcvr *tcvr = sc->sc_tcvr;
	int ret = 0, i;

	tcvr->bb_oenab = 1;                	/* turn on bitbang intrs */

	for (i = 0; i < 32; i++)		/* make bitbang idle */
		hme_tcvr_bb_writeb(sc, 1);

	hme_tcvr_bb_writeb(sc, 0);		/* 0110 signals a read */
	hme_tcvr_bb_writeb(sc, 1);
	hme_tcvr_bb_writeb(sc, 1);
	hme_tcvr_bb_writeb(sc, 0);

	for (i = 4; i >= 0; i--)		/* send PHY addr */
		hme_tcvr_bb_writeb(sc, ((sc->sc_phyaddr & 0xff) >> i) & 0x1);

	for (i = 4; i >= 0; i--)		/* send register num */
		hme_tcvr_bb_writeb(sc, ((reg & 0xff) >> i) & 0x1);

	tcvr->bb_oenab = 0;	                /* turn off bitbang intrs */

	hme_tcvr_bb_readb(sc);			/* ignore... */

	for (i = 15; i >= 15; i--)		/* read value */
		ret |= hme_tcvr_bb_readb(sc) << i;

	hme_tcvr_bb_readb(sc);			/* ignore... */
	hme_tcvr_bb_readb(sc);			/* ignore... */
	hme_tcvr_bb_readb(sc);			/* ignore... */

	return ret;
}

static void
hme_auto_negotiate(sc)
	struct hme_softc *sc;
{
	int tries;

	/* grab all of the registers */
	sc->sc_sw.bmsr =	hme_tcvr_read(sc, DP83840_BMSR);
	sc->sc_sw.bmcr =	hme_tcvr_read(sc, DP83840_BMCR);
	sc->sc_sw.phyidr1 =	hme_tcvr_read(sc, DP83840_PHYIDR1);
	sc->sc_sw.phyidr2 =	hme_tcvr_read(sc, DP83840_PHYIDR2);
	sc->sc_sw.anar =	hme_tcvr_read(sc, DP83840_ANAR);

	/* can this board autonegotiate? No, do it manually */
	if (! (sc->sc_sw.bmsr & BMSR_ANC))
		hme_manual_negotiate(sc);

	/* advertise -everything- supported */
	if (sc->sc_sw.bmsr & BMSR_10BASET_HALF)
		sc->sc_sw.anar |= ANAR_10;
	else
		sc->sc_sw.anar &= ~(ANAR_10);

	if (sc->sc_sw.bmsr & BMSR_10BASET_FULL)
		sc->sc_sw.anar |= ANAR_10_FD;
	else
		sc->sc_sw.anar &= ~(ANAR_10_FD);

	if (sc->sc_sw.bmsr & BMSR_100BASETX_HALF)
		sc->sc_sw.anar |= ANAR_TX;
	else
		sc->sc_sw.anar &= ~(ANAR_TX);

	if (sc->sc_sw.bmsr & BMSR_100BASETX_FULL)
		sc->sc_sw.anar |= ANAR_TX_FD;
	else
		sc->sc_sw.anar &= ~(ANAR_TX_FD);

	hme_tcvr_write(sc, DP83840_ANAR, sc->sc_sw.anar);

	/* Start autonegoiation */
	sc->sc_sw.bmcr |= BMCR_ANE;	/* enable auto-neg */
	hme_tcvr_write(sc, DP83840_BMCR, sc->sc_sw.bmcr);
	sc->sc_sw.bmcr |= BMCR_RAN;	/* force a restart */
	hme_tcvr_write(sc, DP83840_BMCR, sc->sc_sw.bmcr);

	/* BMCR_RAN clears itself when it has started negotiation... */
	tries = 64;
	while (--tries) {
		int r = hme_tcvr_read(sc, DP83840_BMCR);
		if (r == TCVR_FAILURE)
			return;
		sc->sc_sw.bmcr = r;
		if (! (sc->sc_sw.bmcr & BMCR_RAN))
			break;
		DELAY(100);
	}
	if (!tries) {
		printf("%s: failed to start auto-negotiation\n",
			sc->sc_dev.dv_xname);
		hme_manual_negotiate(sc);
		return;
	}
	sc->sc_an_state = HME_TIMER_AUTONEG;
	sc->sc_an_ticks = 0;
	timeout(hme_negotiate_watchdog, sc, (12 * hz)/10);
}

static void
hme_manual_negotiate(sc)
	struct hme_softc *sc;
{
	printf("%s: Starting manual negotiation... not yet!\n",
		sc->sc_dev.dv_xname);
}

/*
 * If auto-negotiating, check to see if it has completed successfully.  If so,
 * wait for a link up.  If it completed unsucessfully, try the manual process.
 */
static void
hme_negotiate_watchdog(arg)
	void *arg;
{
	struct hme_softc *sc = (struct hme_softc *)arg;
	struct ifnet *ifp = &sc->sc_stp.stp_arpcom.ac_if;

	sc->sc_an_ticks++;
	switch (sc->sc_an_state) {
	    case HME_TIMER_DONE:
		return;
	    case HME_TIMER_AUTONEG:
		printf("%s: tick: autoneg...\n", sc->sc_dev.dv_xname);
		sc->sc_sw.bmsr = hme_tcvr_read(sc, DP83840_BMSR);
		if (sc->sc_sw.bmsr & BMSR_ANCOMPLETE) {
			sc->sc_an_state = HME_TIMER_LINKUP;
			sc->sc_an_ticks = 0;
			timeout(hme_negotiate_watchdog, sc, (12 * hz)/10);
			return;
		}
		if (sc->sc_an_ticks > 10) {
			printf("%s: auto-negotiation failed.\n",
				sc->sc_dev.dv_xname);
			return;
		}
		timeout(hme_negotiate_watchdog, sc, (12 * hz)/10);
		break;
	    case HME_TIMER_LINKUP:
		printf("%s: tick: linkup..\n", sc->sc_dev.dv_xname);
		ifp->if_flags |= IFF_RUNNING;
		ifp->if_flags &= ~IFF_OACTIVE;
		ifp->if_timer = 0;
		hmestart(ifp);
		sc->sc_sw.bmsr = hme_tcvr_read(sc, DP83840_BMSR);
		if (sc->sc_sw.bmsr & BMSR_LINKSTATUS) {
			sc->sc_an_state = HME_TIMER_DONE;
			sc->sc_an_ticks = 0;
			hme_print_link_mode(sc);
			return;
		}
		if ((sc->sc_an_ticks % 10) == 0) {
			printf("%s: link down...\n", sc->sc_dev.dv_xname);
			timeout(hme_negotiate_watchdog, sc, (12 * hz)/10);
			return;
		}
	}
}

static void 
hme_print_link_mode(sc)
	struct hme_softc *sc;
{
	sc->sc_sw.anlpar = hme_tcvr_read(sc, DP83840_ANLPAR);
	printf("%s: %s transceiver up %dMb/s %s duplex\n",
	    sc->sc_dev.dv_xname,
	    (sc->sc_tcvr_type == HME_TCVR_EXTERNAL) ? "external" : "internal",
	    (sc->sc_sw.anlpar & (ANLPAR_TX_FD | ANLPAR_TX)) ? 100 : 10,
	    (sc->sc_sw.anlpar & (ANLPAR_TX_FD | ANLPAR_10_FD)) ? "full" : "half");
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
	sc->sc_sw.bmcr = hme_tcvr_read(sc, DP83840_BMCR);
	sc->sc_sw.anlpar = hme_tcvr_read(sc, DP83840_ANLPAR);

	printf("%s: link status changed\n", sc->sc_dev.dv_xname);
	if (sc->sc_sw.anlpar & ANLPAR_TX_FD) {
		sc->sc_sw.bmcr |= (BMCR_SPEED | BMCR_DUPLEX);
	} else if (sc->sc_sw.anlpar & ANLPAR_TX) {
		sc->sc_sw.bmcr |= BMCR_SPEED;
	} else if (sc->sc_sw.anlpar & ANLPAR_10_FD) {
		sc->sc_sw.bmcr |= BMCR_DUPLEX;
	} /* else 10Mb half duplex... */
	hme_tcvr_write(sc, DP83840_BMCR, sc->sc_sw.bmcr);
	hme_print_link_mode(sc);
	hme_poll_stop(sc);
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
	if (why &  GR_STAT_RFIFOVF) {	/* probably dma error */
		printf("%s: receive fifo overflow\n", sc->sc_dev.dv_xname);
		hmereset(sc);
	}

	if (why & GR_STAT_STSTERR) {
		printf("%s: SQE test failed: resetting\n", sc->sc_dev.dv_xname);
		hmereset(sc);
	}

	if (why & GR_STAT_TFIFO_UND) {	/* probably dma error */
		printf("%s: tx fifo underrun\n", sc->sc_dev.dv_xname);
		hmereset(sc);
	}

	if (why & GR_STAT_MAXPKTERR) {	/* driver bug */
		printf("%s: tx max packet size error\n", sc->sc_dev.dv_xname);
		hmereset(sc);
	}

	if (why & GR_STAT_NORXD) {	/* driver bug */
		printf("%s: out of receive descriptors\n", sc->sc_dev.dv_xname);
		hmereset(sc);
	}

	if (why & GR_STAT_EOPERR) {
		printf("%s: eop not set in tx descriptor\n",
		    sc->sc_dev.dv_xname);
		hmereset(sc);
	}

	if (why & (GR_STAT_RXERR | GR_STAT_RXPERR | GR_STAT_RXTERR)) {
		printf("%s: rx dma error < ", sc->sc_dev.dv_xname);
		if (why & GR_STAT_RXERR)
			printf("Generic ");
		if (why & GR_STAT_RXPERR);
			printf("Parity ");
		if (why & GR_STAT_RXTERR)
			printf("RxTag ");
		printf(" >\n");
		hmereset(sc);
	}

	if (why &
	    (GR_STAT_TXEACK|GR_STAT_TXLERR|GR_STAT_TXPERR|GR_STAT_TXTERR)) {
		printf("%s: rx dma error < ", sc->sc_dev.dv_xname);
		if (why & GR_STAT_TXEACK)
			printf("Generic ");
		if (why & GR_STAT_TXLERR);
			printf("Late ");
		if (why & GR_STAT_TXPERR)
			printf("Parity ");
		if (why & GR_STAT_TXTERR);
			printf("TxTag ");
		printf(" >\n");
		hmereset(sc);
	}

	if (why & (GR_STAT_SLVERR | GR_STAT_SLVPERR)) {
		printf("%s: sbus %s error accessing registers\n",
			sc->sc_dev.dv_xname,
			(why & GR_STAT_SLVPERR) ? "parity" : "generic");
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
		r |= stp2002_tint(&sc->sc_stp);

	if (why & GR_STAT_RXTOHOST)
		r |= stp2002_rint(&sc->sc_stp);

	return (r);
}

/*
 * Program the multicast receive filter.
 */
static void
hme_mcreset(sc)
	struct hme_softc *sc;
{
	struct arpcom *ac = &sc->sc_stp.stp_arpcom;
	struct ifnet *ifp = &sc->sc_stp.stp_arpcom.ac_if;
	struct hme_cr *cr = sc->sc_cr;
	struct ether_multi *enm;
	struct ether_multistep step;
	u_int16_t hash[4];
	u_int32_t crc;
	int i, j;

	if (ifp->if_flags & IFF_ALLMULTI) {
		cr->htable3 = 0xffff;
		cr->htable2 = 0xffff;
		cr->htable1 = 0xffff;
		cr->htable0 = 0xffff;
	}
	else if (ifp->if_flags & IFF_PROMISC) {
		cr->rx_cfg |= CR_RXCFG_PMISC;
	}
	else {
		cr->htable3 = cr->htable2 = cr->htable1 = cr->htable0 = 0;
		hash[3] = hash[2] = hash[1] = hash[0] = 0;
		crc = 0xffffffffU;

		ETHER_FIRST_MULTI(step, ac, enm);
		while (enm != NULL) {

			for (i = 0; i < ETHER_ADDR_LEN; i++) {
				u_int8_t octet = enm->enm_addrlo[i];

				for (j = 0; j < 8; j++) {
					u_int8_t bit;

					bit = (octet << j) & 1;
					crc >>= 1;
					if ((bit ^ crc) & 1)
						crc = crc ^ MC_POLY_LE;
				}
			}

			crc >>=26;
			hash[crc >> 4] |= 1 << (crc & 0x0f);
			ETHER_NEXT_MULTI(step, enm);
		}
		cr->htable3 = hash[3];
		cr->htable2 = hash[2];
		cr->htable1 = hash[1];
		cr->htable0 = hash[0];
	}
}
