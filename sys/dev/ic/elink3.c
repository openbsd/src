/*	$NetBSD: elink3.c,v 1.7 1996/05/14 22:22:05 thorpej Exp $	*/

/*
 * Copyright (c) 1994 Herb Peyerl <hpeyerl@beer.org>
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
 *      This product includes software developed by Herb Peyerl.
 * 4. The name of Herb Peyerl may not be used to endorse or promote products
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
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/syslog.h>
#include <sys/select.h>
#include <sys/device.h>

#include <net/if.h>
#include <net/netisr.h>
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

#if NBPFILTER > 0
#include <net/bpf.h>
#include <net/bpfdesc.h>
#endif

#include <machine/cpu.h>
#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/ic/elink3var.h>
#include <dev/ic/elink3reg.h>

#define ETHER_MIN_LEN	64
#define ETHER_MAX_LEN   1518
#define ETHER_ADDR_LEN  6

struct cfdriver ep_cd = {
	NULL, "ep", DV_IFNET
};

static void eptxstat __P((struct ep_softc *));
static int epstatus __P((struct ep_softc *));
void epinit __P((struct ep_softc *));
int epioctl __P((struct ifnet *, u_long, caddr_t));
void epstart __P((struct ifnet *));
void epwatchdog __P((struct ifnet *));
void epreset __P((struct ep_softc *));
void epread __P((struct ep_softc *));
struct mbuf *epget __P((struct ep_softc *, int));
void epmbuffill __P((void *));
void epmbufempty __P((struct ep_softc *));
void epsetfilter __P((struct ep_softc *));
void epsetlink __P((struct ep_softc *));

static int epbusyeeprom __P((struct ep_softc *));

void
epconfig(sc, conn)
	struct ep_softc *sc;
	u_int16_t conn;
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	bus_chipset_tag_t bc = sc->sc_bc;
	bus_io_handle_t ioh = sc->sc_ioh;
	u_int16_t i;

	sc->ep_connectors = 0;
	if (conn & IS_AUI) {
		printf("aui");
		sc->ep_connectors |= AUI;
	}
	if (conn & IS_BNC) {
		if (sc->ep_connectors)
			printf("/");
		printf("bnc");
		sc->ep_connectors |= BNC;
	}
	if (conn & IS_UTP) {
		if (sc->ep_connectors)
			printf("/");
		printf("utp");
		sc->ep_connectors |= UTP;
	}
	if (!sc->ep_connectors)
		printf("no connectors!");

	/*
	 * Read the station address from the eeprom
	 */
	for (i = 0; i < 3; i++) {
		u_int16_t x;
		if (epbusyeeprom(sc))
			return;
		bus_io_write_2(bc, ioh, EP_W0_EEPROM_COMMAND, READ_EEPROM | i);
		if (epbusyeeprom(sc))
			return;
		x = bus_io_read_2(bc, ioh, EP_W0_EEPROM_DATA);
		sc->sc_arpcom.ac_enaddr[(i << 1)] = x >> 8;
		sc->sc_arpcom.ac_enaddr[(i << 1) + 1] = x;
	}

	printf(" address %s\n", ether_sprintf(sc->sc_arpcom.ac_enaddr));

	bcopy(sc->sc_dev.dv_xname, ifp->if_xname, IFNAMSIZ);
	ifp->if_softc = sc;
	ifp->if_start = epstart;
	ifp->if_ioctl = epioctl;
	ifp->if_watchdog = epwatchdog;
	ifp->if_flags =
	    IFF_BROADCAST | IFF_SIMPLEX | IFF_NOTRAILERS | IFF_MULTICAST;

	if_attach(ifp);
	ether_ifattach(ifp);

#if NBPFILTER > 0
	bpfattach(&sc->sc_arpcom.ac_if.if_bpf, ifp, DLT_EN10MB,
		  sizeof(struct ether_header));
#endif

	sc->tx_start_thresh = 20;	/* probably a good starting point. */
}

/*
 * The order in here seems important. Otherwise we may not receive
 * interrupts. ?!
 */
void
epinit(sc)
	register struct ep_softc *sc;
{
	register struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	bus_chipset_tag_t bc = sc->sc_bc;
	bus_io_handle_t ioh = sc->sc_ioh;
	int i;

	while (bus_io_read_2(bc, ioh, EP_STATUS) & S_COMMAND_IN_PROGRESS)
		;

	if (sc->bustype != EP_BUS_PCI) {
		GO_WINDOW(0);
		bus_io_write_2(bc, ioh, EP_W0_CONFIG_CTRL, 0);
		bus_io_write_2(bc, ioh, EP_W0_CONFIG_CTRL, ENABLE_DRQ_IRQ);
	}

	if (sc->bustype == EP_BUS_PCMCIA) {
#ifdef EP_COAX_DEFAULT
		bus_io_write_2(bc, ioh, EP_W0_ADDRESS_CFG,3<<14);
#else
		bus_io_write_2(bc, ioh, EP_W0_ADDRESS_CFG,0<<14);
#endif
		bus_io_write_2(bc, ioh, EP_W0_RESOURCE_CFG, 0x3f00);
	}

	GO_WINDOW(2);
	for (i = 0; i < 6; i++)	/* Reload the ether_addr. */
		bus_io_write_1(bc, ioh, EP_W2_ADDR_0 + i,
		    sc->sc_arpcom.ac_enaddr[i]);

	bus_io_write_2(bc, ioh, EP_COMMAND, RX_RESET);
	bus_io_write_2(bc, ioh, EP_COMMAND, TX_RESET);

	GO_WINDOW(1);		/* Window 1 is operating window */
	for (i = 0; i < 31; i++)
		bus_io_read_1(bc, ioh, EP_W1_TX_STATUS);

	bus_io_write_2(bc, ioh, EP_COMMAND, SET_RD_0_MASK | S_CARD_FAILURE | 
				S_RX_COMPLETE | S_TX_COMPLETE | S_TX_AVAIL);
	bus_io_write_2(bc, ioh, EP_COMMAND, SET_INTR_MASK | S_CARD_FAILURE |
				S_RX_COMPLETE | S_TX_COMPLETE | S_TX_AVAIL);

	/*
	 * Attempt to get rid of any stray interrupts that occured during
	 * configuration.  On the i386 this isn't possible because one may
	 * already be queued.  However, a single stray interrupt is
	 * unimportant.
	 */
	bus_io_write_2(bc, ioh, EP_COMMAND, ACK_INTR | 0xff);

	epsetfilter(sc);
	epsetlink(sc);

	bus_io_write_2(bc, ioh, EP_COMMAND, RX_ENABLE);
	bus_io_write_2(bc, ioh, EP_COMMAND, TX_ENABLE);

	epmbuffill(sc);

	/* Interface is now `running', with no output active. */
	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	/* Attempt to start output, if any. */
	epstart(ifp);
}

void
epsetfilter(sc)
	register struct ep_softc *sc;
{
	register struct ifnet *ifp = &sc->sc_arpcom.ac_if;

	GO_WINDOW(1);		/* Window 1 is operating window */
	bus_io_write_2(sc->sc_bc, sc->sc_ioh, EP_COMMAND, SET_RX_FILTER |
	    FIL_INDIVIDUAL | FIL_BRDCST |
	    ((ifp->if_flags & IFF_MULTICAST) ? FIL_MULTICAST : 0 ) |
	    ((ifp->if_flags & IFF_PROMISC) ? FIL_PROMISC : 0 ));
}

void
epsetlink(sc)
	register struct ep_softc *sc;
{
	register struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	bus_chipset_tag_t bc = sc->sc_bc;
	bus_io_handle_t ioh = sc->sc_ioh;

	/*
	 * you can `ifconfig (link0|-link0) ep0' to get the following
	 * behaviour:
	 *	-link0	disable AUI/UTP. enable BNC.
	 *	link0	disable BNC. enable AUI.
	 *	link1	if the card has a UTP connector, and link0 is
	 *		set too, then you get the UTP port.
	 */
	GO_WINDOW(4);
	bus_io_write_2(bc, ioh, EP_W4_MEDIA_TYPE, DISABLE_UTP);
	if (!(ifp->if_flags & IFF_LINK0) && (sc->ep_connectors & BNC)) {
		if (sc->bustype == EP_BUS_PCMCIA) {
			GO_WINDOW(0);
			bus_io_write_2(bc, ioh, EP_W0_ADDRESS_CFG,3<<14);
			GO_WINDOW(1);
		}
		bus_io_write_2(bc, ioh, EP_COMMAND, START_TRANSCEIVER);
		delay(1000);
	}
	if (ifp->if_flags & IFF_LINK0) {
		bus_io_write_2(bc, ioh, EP_COMMAND, STOP_TRANSCEIVER);
		delay(1000);
		if ((ifp->if_flags & IFF_LINK1) && (sc->ep_connectors & UTP)) {
			if (sc->bustype == EP_BUS_PCMCIA) {
				GO_WINDOW(0);
				bus_io_write_2(bc, ioh,
				    EP_W0_ADDRESS_CFG,0<<14);
				GO_WINDOW(4);
			}
			bus_io_write_2(bc, ioh, EP_W4_MEDIA_TYPE, ENABLE_UTP);
		}
	}
	GO_WINDOW(1);
}

/*
 * Start outputting on the interface.
 * Always called as splnet().
 */
void
epstart(ifp)
	struct ifnet *ifp;
{
	register struct ep_softc *sc = ifp->if_softc;
	bus_chipset_tag_t bc = sc->sc_bc;
	bus_io_handle_t ioh = sc->sc_ioh;
	struct mbuf *m, *m0;
	int sh, len, pad;

	/* Don't transmit if interface is busy or not running */
	if ((ifp->if_flags & (IFF_RUNNING|IFF_OACTIVE)) != IFF_RUNNING)
		return;

startagain:
	/* Sneak a peek at the next packet */
	m0 = ifp->if_snd.ifq_head;
	if (m0 == 0)
		return;

	/* We need to use m->m_pkthdr.len, so require the header */
	if ((m0->m_flags & M_PKTHDR) == 0)
		panic("epstart: no header mbuf");
	len = m0->m_pkthdr.len;

	pad = (4 - len) & 3;

	/*
	 * The 3c509 automatically pads short packets to minimum ethernet
	 * length, but we drop packets that are too large. Perhaps we should
	 * truncate them instead?
	 */
	if (len + pad > ETHER_MAX_LEN) {
		/* packet is obviously too large: toss it */
		++ifp->if_oerrors;
		IF_DEQUEUE(&ifp->if_snd, m0);
		m_freem(m0);
		goto readcheck;
	}

	if (bus_io_read_2(bc, ioh, EP_W1_FREE_TX) < len + pad + 4) {
		bus_io_write_2(bc, ioh, EP_COMMAND,
		    SET_TX_AVAIL_THRESH | (len + pad + 4));
		/* not enough room in FIFO */
		ifp->if_flags |= IFF_OACTIVE;
		return;
	} else {
		bus_io_write_2(bc, ioh, EP_COMMAND,
		    SET_TX_AVAIL_THRESH | 2044);
	}

	IF_DEQUEUE(&ifp->if_snd, m0);
	if (m0 == 0)		/* not really needed */
		return;

	bus_io_write_2(bc, ioh, EP_COMMAND, SET_TX_START_THRESH |
	    (len / 4 + sc->tx_start_thresh));

#if NBPFILTER > 0
	if (ifp->if_bpf)
		bpf_mtap(ifp->if_bpf, m0);
#endif

	/*
	 * Do the output at splhigh() so that an interrupt from another device
	 * won't cause a FIFO underrun.
	 */
	sh = splhigh();

	bus_io_write_2(bc, ioh, EP_W1_TX_PIO_WR_1, len);
	bus_io_write_2(bc, ioh, EP_W1_TX_PIO_WR_1,
	    0xffff);	/* Second dword meaningless */
	if (EP_IS_BUS_32(sc->bustype)) {
		for (m = m0; m; ) {
			if (m->m_len > 3)
				bus_io_write_multi_4(bc, ioh,
				    EP_W1_TX_PIO_WR_1, mtod(m, u_int32_t *),
				    m->m_len / 4);
			if (m->m_len & 3)
				bus_io_write_multi_1(bc, ioh,
				    EP_W1_TX_PIO_WR_1,
				    mtod(m, u_int8_t *) + (m->m_len & ~3),
				    m->m_len & 3);
			MFREE(m, m0);
			m = m0;
		}
	} else {
		for (m = m0; m; ) {
			if (m->m_len > 1)
				bus_io_write_multi_2(bc, ioh,
				    EP_W1_TX_PIO_WR_1, mtod(m, u_int16_t *),
				    m->m_len / 2);
			if (m->m_len & 1)
				bus_io_write_1(bc, ioh, EP_W1_TX_PIO_WR_1,
				     *(mtod(m, u_int8_t *) + m->m_len - 1));
			MFREE(m, m0);
			m = m0;
		}
	}
	while (pad--)
		bus_io_write_1(bc, ioh, EP_W1_TX_PIO_WR_1, 0);

	splx(sh);

	++ifp->if_opackets;

readcheck:
	if ((bus_io_read_2(bc, ioh, EP_W1_RX_STATUS) & ERR_INCOMPLETE) == 0) {
		/* We received a complete packet. */
		u_int16_t status = bus_io_read_2(bc, ioh, EP_STATUS);

		if ((status & S_INTR_LATCH) == 0) {
			/*
			 * No interrupt, read the packet and continue
			 * Is  this supposed to happen? Is my motherboard 
			 * completely busted?
			 */
			epread(sc);
		}
		else
			/* Got an interrupt, return so that it gets serviced. */
			return;
	}
	else {
		/* Check if we are stuck and reset [see XXX comment] */
		if (epstatus(sc)) {
			if (ifp->if_flags & IFF_DEBUG)
				printf("%s: adapter reset\n",
				       sc->sc_dev.dv_xname);
			epreset(sc);
		}
	}

	goto startagain;
}


/*
 * XXX: The 3c509 card can get in a mode where both the fifo status bit
 *	FIFOS_RX_OVERRUN and the status bit ERR_INCOMPLETE are set
 *	We detect this situation and we reset the adapter.
 *	It happens at times when there is a lot of broadcast traffic
 *	on the cable (once in a blue moon).
 */
static int
epstatus(sc)
	register struct ep_softc *sc;
{
	bus_chipset_tag_t bc = sc->sc_bc;
	bus_io_handle_t ioh = sc->sc_ioh;
	u_int16_t fifost;

	/*
	 * Check the FIFO status and act accordingly
	 */
	GO_WINDOW(4);
	fifost = bus_io_read_2(bc, ioh, EP_W4_FIFO_DIAG);
	GO_WINDOW(1);

	if (fifost & FIFOS_RX_UNDERRUN) {
		if (sc->sc_arpcom.ac_if.if_flags & IFF_DEBUG)
			printf("%s: RX underrun\n", sc->sc_dev.dv_xname);
		epreset(sc);
		return 0;
	}

	if (fifost & FIFOS_RX_STATUS_OVERRUN) {
		if (sc->sc_arpcom.ac_if.if_flags & IFF_DEBUG)
			printf("%s: RX Status overrun\n", sc->sc_dev.dv_xname);
		return 1;
	}

	if (fifost & FIFOS_RX_OVERRUN) {
		if (sc->sc_arpcom.ac_if.if_flags & IFF_DEBUG)
			printf("%s: RX overrun\n", sc->sc_dev.dv_xname);
		return 1;
	}

	if (fifost & FIFOS_TX_OVERRUN) {
		if (sc->sc_arpcom.ac_if.if_flags & IFF_DEBUG)
			printf("%s: TX overrun\n", sc->sc_dev.dv_xname);
		epreset(sc);
		return 0;
	}

	return 0;
}


static void
eptxstat(sc)
	register struct ep_softc *sc;
{
	bus_chipset_tag_t bc = sc->sc_bc;
	bus_io_handle_t ioh = sc->sc_ioh;
	int i;

	/*
	 * We need to read+write TX_STATUS until we get a 0 status
	 * in order to turn off the interrupt flag.
	 */
	while ((i = bus_io_read_1(bc, ioh, EP_W1_TX_STATUS)) & TXS_COMPLETE) {
		bus_io_write_1(bc, ioh, EP_W1_TX_STATUS, 0x0);

		if (i & TXS_JABBER) {
			++sc->sc_arpcom.ac_if.if_oerrors;
			if (sc->sc_arpcom.ac_if.if_flags & IFF_DEBUG)
				printf("%s: jabber (%x)\n",
				       sc->sc_dev.dv_xname, i);
			epreset(sc);
		} else if (i & TXS_UNDERRUN) {
			++sc->sc_arpcom.ac_if.if_oerrors;
			if (sc->sc_arpcom.ac_if.if_flags & IFF_DEBUG)
				printf("%s: fifo underrun (%x) @%d\n",
				       sc->sc_dev.dv_xname, i,
				       sc->tx_start_thresh);
			if (sc->tx_succ_ok < 100)
				    sc->tx_start_thresh = min(ETHER_MAX_LEN,
					    sc->tx_start_thresh + 20);
			sc->tx_succ_ok = 0;
			epreset(sc);
		} else if (i & TXS_MAX_COLLISION) {
			++sc->sc_arpcom.ac_if.if_collisions;
			bus_io_write_2(bc, ioh, EP_COMMAND, TX_ENABLE);
			sc->sc_arpcom.ac_if.if_flags &= ~IFF_OACTIVE;
		} else
			sc->tx_succ_ok = (sc->tx_succ_ok+1) & 127;
	}
}

int
epintr(arg)
	void *arg;
{
	register struct ep_softc *sc = arg;
	bus_chipset_tag_t bc = sc->sc_bc;
	bus_io_handle_t ioh = sc->sc_ioh;
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	u_int16_t status;
	int ret = 0;

	for (;;) {
		bus_io_write_2(bc, ioh, EP_COMMAND, C_INTR_LATCH);

		status = bus_io_read_2(bc, ioh, EP_STATUS);

		if ((status & (S_TX_COMPLETE | S_TX_AVAIL |
			       S_RX_COMPLETE | S_CARD_FAILURE)) == 0)
			break;

		ret = 1;

		/*
		 * Acknowledge any interrupts.  It's important that we do this
		 * first, since there would otherwise be a race condition.
		 * Due to the i386 interrupt queueing, we may get spurious
		 * interrupts occasionally.
		 */
		bus_io_write_2(bc, ioh, EP_COMMAND, ACK_INTR | status);

		if (status & S_RX_COMPLETE)
			epread(sc);
		if (status & S_TX_AVAIL) {
			sc->sc_arpcom.ac_if.if_flags &= ~IFF_OACTIVE;
			epstart(&sc->sc_arpcom.ac_if);
		}
		if (status & S_CARD_FAILURE) {
			printf("%s: adapter failure (%x)\n",
			       sc->sc_dev.dv_xname, status);
			epreset(sc);
			return (1);
		}
		if (status & S_TX_COMPLETE) {
			eptxstat(sc);
			epstart(ifp);
		}
	}	

	/* no more interrupts */
	return (ret);
}

void
epread(sc)
	register struct ep_softc *sc;
{
	bus_chipset_tag_t bc = sc->sc_bc;
	bus_io_handle_t ioh = sc->sc_ioh;
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	struct mbuf *m;
	struct ether_header *eh;
	int len;

	len = bus_io_read_2(bc, ioh, EP_W1_RX_STATUS);

again:
	if (ifp->if_flags & IFF_DEBUG) {
		int err = len & ERR_MASK;
		char *s = NULL;

		if (len & ERR_INCOMPLETE)
			s = "incomplete packet";
		else if (err == ERR_OVERRUN)
			s = "packet overrun";
		else if (err == ERR_RUNT)
			s = "runt packet";
		else if (err == ERR_ALIGNMENT)
			s = "bad alignment";
		else if (err == ERR_CRC)
			s = "bad crc";
		else if (err == ERR_OVERSIZE)
			s = "oversized packet";
		else if (err == ERR_DRIBBLE)
			s = "dribble bits";

		if (s)
			printf("%s: %s\n", sc->sc_dev.dv_xname, s);
	}

	if (len & ERR_INCOMPLETE)
		return;

	if (len & ERR_RX) {
		++ifp->if_ierrors;
		goto abort;
	}

	len &= RX_BYTES_MASK;	/* Lower 11 bits = RX bytes. */

	/* Pull packet off interface. */
	m = epget(sc, len);
	if (m == 0) {
		ifp->if_ierrors++;
		goto abort;
	}

	++ifp->if_ipackets;

	eh = mtod(m, struct ether_header *);

#if NBPFILTER > 0
	/*
	 * Check if there's a BPF listener on this interface.
	 * If so, hand off the raw packet to BPF.
	 */
	if (ifp->if_bpf) {
		bpf_mtap(ifp->if_bpf, m);

		/*
		 * Note that the interface cannot be in promiscuous mode if
		 * there are no BPF listeners.  And if we are in promiscuous
		 * mode, we have to check if this packet is really ours.
		 */
		if ((ifp->if_flags & IFF_PROMISC) &&
		    (eh->ether_dhost[0] & 1) == 0 && /* !mcast and !bcast */
		    bcmp(eh->ether_dhost, sc->sc_arpcom.ac_enaddr,
			    sizeof(eh->ether_dhost)) != 0) {
			m_freem(m);
			return;
		}
	}
#endif

	m_adj(m, sizeof(struct ether_header));
	ether_input(ifp, eh, m);

	/*
	 * In periods of high traffic we can actually receive enough
	 * packets so that the fifo overrun bit will be set at this point,
	 * even though we just read a packet. In this case we
	 * are not going to receive any more interrupts. We check for
	 * this condition and read again until the fifo is not full.
	 * We could simplify this test by not using epstatus(), but
	 * rechecking the RX_STATUS register directly. This test could
	 * result in unnecessary looping in cases where there is a new
	 * packet but the fifo is not full, but it will not fix the
	 * stuck behavior.
	 *
	 * Even with this improvement, we still get packet overrun errors
	 * which are hurting performance. Maybe when I get some more time
	 * I'll modify epread() so that it can handle RX_EARLY interrupts.
	 */
	if (epstatus(sc)) {
		len = bus_io_read_2(bc, ioh, EP_W1_RX_STATUS);
		/* Check if we are stuck and reset [see XXX comment] */
		if (len & ERR_INCOMPLETE) {
			if (ifp->if_flags & IFF_DEBUG)
				printf("%s: adapter reset\n",
				       sc->sc_dev.dv_xname);
			epreset(sc);
			return;
		}
		goto again;
	}

	return;

abort:
	bus_io_write_2(bc, ioh, EP_COMMAND, RX_DISCARD_TOP_PACK);
	while (bus_io_read_2(bc, ioh, EP_STATUS) & S_COMMAND_IN_PROGRESS)
		;
}

struct mbuf *
epget(sc, totlen)
	struct ep_softc *sc;
	int totlen;
{
	bus_chipset_tag_t bc = sc->sc_bc;
	bus_io_handle_t ioh = sc->sc_ioh;
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	struct mbuf *top, **mp, *m;
	int len, pad;
	int sh;

	m = sc->mb[sc->next_mb];
	sc->mb[sc->next_mb] = 0;
	if (m == 0) {
		MGETHDR(m, M_DONTWAIT, MT_DATA);
		if (m == 0)
			return 0;
	} else {
		/* If the queue is no longer full, refill. */
		if (sc->last_mb == sc->next_mb)
			timeout(epmbuffill, sc, 1);
		/* Convert one of our saved mbuf's. */
		sc->next_mb = (sc->next_mb + 1) % MAX_MBS;
		m->m_data = m->m_pktdat;
		m->m_flags = M_PKTHDR;
	}
	m->m_pkthdr.rcvif = ifp;
	m->m_pkthdr.len = totlen;
	pad = ALIGN(sizeof(struct ether_header)) - sizeof(struct ether_header);
	m->m_data += pad;
	len = MHLEN - pad;
	top = 0;
	mp = &top;

	/*
	 * We read the packet at splhigh() so that an interrupt from another
	 * device doesn't cause the card's buffer to overflow while we're
	 * reading it.  We may still lose packets at other times.
	 */
	sh = splhigh();

	while (totlen > 0) {
		if (top) {
			m = sc->mb[sc->next_mb];
			sc->mb[sc->next_mb] = 0;
			if (m == 0) {
				MGET(m, M_DONTWAIT, MT_DATA);
				if (m == 0) {
					splx(sh);
					m_freem(top);
					return 0;
				}
			} else {
				sc->next_mb = (sc->next_mb + 1) % MAX_MBS;
			}
			len = MLEN;
		}
		if (top && totlen >= MINCLSIZE) {
			MCLGET(m, M_DONTWAIT);
			if (m->m_flags & M_EXT)
				len = MCLBYTES;
		}
		len = min(totlen, len);
		if (EP_IS_BUS_32(sc->bustype)) {
			if (len > 3) {
				len &= ~3;
				bus_io_read_multi_4(bc, ioh,
				    EP_W1_RX_PIO_RD_1, mtod(m, u_int32_t *),
				    len / 4);
			} else
				bus_io_read_multi_1(bc, ioh,
				    EP_W1_RX_PIO_RD_1, mtod(m, u_int8_t *),
				    len);
		} else {
			if (len > 1) {
				len &= ~1;
				bus_io_read_multi_2(bc, ioh,
				    EP_W1_RX_PIO_RD_1, mtod(m, u_int16_t *),
				    len / 2);
			} else
				*(mtod(m, u_int8_t *)) =
				    bus_io_read_1(bc, ioh, EP_W1_RX_PIO_RD_1);
		}
		m->m_len = len;
		totlen -= len;
		*mp = m;
		mp = &m->m_next;
	}

	bus_io_write_2(bc, ioh, EP_COMMAND, RX_DISCARD_TOP_PACK);
	while (bus_io_read_2(bc, ioh, EP_STATUS) & S_COMMAND_IN_PROGRESS)
		;

	splx(sh);

	return top;
}

int
epioctl(ifp, cmd, data)
	register struct ifnet *ifp;
	u_long cmd;
	caddr_t data;
{
	struct ep_softc *sc = ifp->if_softc;
	struct ifaddr *ifa = (struct ifaddr *)data;
	struct ifreq *ifr = (struct ifreq *)data;
	int s, error = 0;

	s = splnet();

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
			epinit(sc);
			arp_ifinit(&sc->sc_arpcom, ifa);
			break;
#endif
		default:
			epinit(sc);
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
			epstop(sc);
			ifp->if_flags &= ~IFF_RUNNING;
		} else if ((ifp->if_flags & IFF_UP) != 0 &&
			   (ifp->if_flags & IFF_RUNNING) == 0) {
			/*
			 * If interface is marked up and it is stopped, then
			 * start it.
			 */
			epinit(sc);
		} else {
			/*
			 * deal with flags changes:
			 * IFF_MULTICAST, IFF_PROMISC,
			 * IFF_LINK0, IFF_LINK1,
			 */
			epsetfilter(sc);
			epsetlink(sc);
		}
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		error = (cmd == SIOCADDMULTI) ?
		    ether_addmulti(ifr, &sc->sc_arpcom) :
		    ether_delmulti(ifr, &sc->sc_arpcom);

		if (error == ENETRESET) {
			/*
			 * Multicast list has changed; set the hardware filter
			 * accordingly.
			 */
			epreset(sc);
			error = 0;
		}
		break;

	default:
		error = EINVAL;
		break;
	}

	splx(s);
	return (error);
}

void
epreset(sc)
	struct ep_softc *sc;
{
	int s;

	s = splnet();
	epstop(sc);
	epinit(sc);
	splx(s);
}

void
epwatchdog(ifp)
	struct ifnet *ifp;
{
	struct ep_softc *sc = ifp->if_softc;

	log(LOG_ERR, "%s: device timeout\n", sc->sc_dev.dv_xname);
	++sc->sc_arpcom.ac_if.if_oerrors;

	epreset(sc);
}

void
epstop(sc)
	register struct ep_softc *sc;
{
	bus_chipset_tag_t bc = sc->sc_bc;
	bus_io_handle_t ioh = sc->sc_ioh;

	bus_io_write_2(bc, ioh, EP_COMMAND, RX_DISABLE);
	bus_io_write_2(bc, ioh, EP_COMMAND, RX_DISCARD_TOP_PACK);
	while (bus_io_read_2(bc, ioh, EP_STATUS) & S_COMMAND_IN_PROGRESS)
		;
	bus_io_write_2(bc, ioh, EP_COMMAND, TX_DISABLE);
	bus_io_write_2(bc, ioh, EP_COMMAND, STOP_TRANSCEIVER);
	bus_io_write_2(bc, ioh, EP_COMMAND, RX_RESET);
	bus_io_write_2(bc, ioh, EP_COMMAND, TX_RESET);
	bus_io_write_2(bc, ioh, EP_COMMAND, C_INTR_LATCH);
	bus_io_write_2(bc, ioh, EP_COMMAND, SET_RD_0_MASK);
	bus_io_write_2(bc, ioh, EP_COMMAND, SET_INTR_MASK);
	bus_io_write_2(bc, ioh, EP_COMMAND, SET_RX_FILTER);

	epmbufempty(sc);
}

/*
 * We get eeprom data from the id_port given an offset into the
 * eeprom.  Basically; after the ID_sequence is sent to all of
 * the cards; they enter the ID_CMD state where they will accept
 * command requests. 0x80-0xbf loads the eeprom data.  We then
 * read the port 16 times and with every read; the cards check
 * for contention (ie: if one card writes a 0 bit and another
 * writes a 1 bit then the host sees a 0. At the end of the cycle;
 * each card compares the data on the bus; if there is a difference
 * then that card goes into ID_WAIT state again). In the meantime;
 * one bit of data is returned in the AX register which is conveniently
 * returned to us by bus_io_read_1().  Hence; we read 16 times getting one
 * bit of data with each read.
 *
 * NOTE: the caller must provide an i/o handle for ELINK_ID_PORT!
 */
u_int16_t
epreadeeprom(bc, ioh, offset)
	bus_chipset_tag_t bc;
	bus_io_handle_t ioh;
	int offset;
{
	u_int16_t data = 0;
	int i;

	bus_io_write_1(bc, ioh, 0, 0x80 + offset);
	delay(1000);
	for (i = 0; i < 16; i++)
		data = (data << 1) | (bus_io_read_2(bc, ioh, 0) & 1);
	return (data);
}

static int
epbusyeeprom(sc)
	struct ep_softc *sc;
{
	bus_chipset_tag_t bc = sc->sc_bc;
	bus_io_handle_t ioh = sc->sc_ioh;
	int i = 100, j;

	while (i--) {
		j = bus_io_read_2(bc, ioh, EP_W0_EEPROM_COMMAND);
		if (j & EEPROM_BUSY)
			delay(100);
		else
			break;
	}
	if (!i) {
		printf("\n%s: eeprom failed to come ready\n",
		    sc->sc_dev.dv_xname);
		return (1);
	}
	if (sc->bustype != EP_BUS_PCMCIA && j & EEPROM_TST_MODE) {
		printf("\n%s: erase pencil mark, or disable plug-n-play mode!\n",
		    sc->sc_dev.dv_xname);
		return (1);
	}
	return (0);
}

void
epmbuffill(v)
	void *v;
{
	struct ep_softc *sc = v;
	int s, i;

	s = splnet();
	i = sc->last_mb;
	do {
		if (sc->mb[i] == NULL)
			MGET(sc->mb[i], M_DONTWAIT, MT_DATA);
		if (sc->mb[i] == NULL)
			break;
		i = (i + 1) % MAX_MBS;
	} while (i != sc->next_mb);
	sc->last_mb = i;
	/* If the queue was not filled, try again. */
	if (sc->last_mb != sc->next_mb)
		timeout(epmbuffill, sc, 1);
	splx(s);
}

void
epmbufempty(sc)
	struct ep_softc *sc;
{
	int s, i;

	s = splnet();
	for (i = 0; i<MAX_MBS; i++) {
		if (sc->mb[i]) {
			m_freem(sc->mb[i]);
			sc->mb[i] = NULL;
		}
	}
	sc->last_mb = sc->next_mb = 0;
	untimeout(epmbuffill, sc);
	splx(s);
}
