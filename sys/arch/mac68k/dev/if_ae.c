/*	$OpenBSD: if_ae.c,v 1.25 2005/01/15 05:24:09 brad Exp $	*/
/*	$NetBSD: if_ae.c,v 1.62 1997/04/24 16:52:05 scottr Exp $	*/

/*
 * Device driver for National Semiconductor DS8390/WD83C690 based ethernet
 * adapters.
 *
 * Copyright (c) 1994, 1995 Charles M. Hannum.  All rights reserved.
 *
 * Copyright (C) 1993, David Greenman.  This software may be used, modified,
 * copied, distributed, and sold, in both source and binary form provided that
 * the above copyright and these terms are retained.  Under no circumstances is
 * the author responsible for the proper functioning of this software, nor does
 * the author assume any responsibility for damages incurred with its use.
 */

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/device.h>

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

#if NBPFILTER > 0
#include <net/bpf.h>
#include <net/bpfdesc.h>
#endif

#include <machine/bus.h>
#include <machine/viareg.h>

#include <dev/ic/dp8390reg.h>
#include "if_aereg.h"
#include "if_aevar.h"

#define INTERFACE_NAME_LEN	32

#define inline			/* XXX for debugging porpoises */

static inline void ae_rint(struct ae_softc *);
static inline void ae_xmit(struct ae_softc *);
static inline int ae_ring_copy( struct ae_softc *, int, caddr_t, int);

#define	ETHER_MIN_LEN	64
#define ETHER_MAX_LEN	1518
#define	ETHER_ADDR_LEN	6


#define NIC_GET(sc, reg)	(bus_space_read_1((sc)->sc_regt,	\
				    (sc)->sc_regh,			\
				    ((sc)->sc_reg_map[reg])))
#define NIC_PUT(sc, reg, val)	(bus_space_write_1((sc)->sc_regt,	\
				    (sc)->sc_regh,			\
				    ((sc)->sc_reg_map[reg]), (val)))
  
struct cfdriver ae_cd = {
	NULL, "ae", DV_IFNET
};
  
int
ae_size_card_memory(bst, bsh, ofs)
	bus_space_tag_t bst;
	bus_space_handle_t bsh;
	int ofs;
{
	int i1, i2, i3, i4, i8;

	/*
	 * banks; also assume it will generally mirror in upper banks
	 * if not installed.
	 */
	i1 = (8192 * 0);
	i2 = (8192 * 1);
	i3 = (8192 * 2);
	i4 = (8192 * 3);
	i8 = (8192 * 4);

	bus_space_write_2(bst, bsh, ofs + i8, 0x8888);
	bus_space_write_2(bst, bsh, ofs + i4, 0x4444);
	bus_space_write_2(bst, bsh, ofs + i3, 0x3333);
	bus_space_write_2(bst, bsh, ofs + i2, 0x2222);
	bus_space_write_2(bst, bsh, ofs + i1, 0x1111);

	/*
	 * 1) If the memory range is decoded completely, it does not
	 *    matter what we write first: High tags written into
	 *    the void are lost.
	 * 2) If the memory range is not decoded completely (banks are
	 *    mirrored), high tags are overwritten by lower ones.
	 * 3) Lazy implementation of pathological cases - none found yet.
	 */

	if (bus_space_read_2(bst, bsh, ofs + i1) == 0x1111 &&
	    bus_space_read_2(bst, bsh, ofs + i2) == 0x2222 &&
	    bus_space_read_2(bst, bsh, ofs + i3) == 0x3333 &&
	    bus_space_read_2(bst, bsh, ofs + i4) == 0x4444 &&
	    bus_space_read_2(bst, bsh, ofs + i8) == 0x8888)
		return 8192 * 8;

	if (bus_space_read_2(bst, bsh, ofs + i1) == 0x1111 &&
	    bus_space_read_2(bst, bsh, ofs + i2) == 0x2222 &&
	    bus_space_read_2(bst, bsh, ofs + i3) == 0x3333 &&
	    bus_space_read_2(bst, bsh, ofs + i4) == 0x4444)
		return 8192 * 4;

	if ((bus_space_read_2(bst, bsh, ofs + i1) == 0x1111 &&
	    bus_space_read_2(bst, bsh, ofs + i2) == 0x2222) ||
	    (bus_space_read_2(bst, bsh, ofs + i1) == 0x3333 &&
	    bus_space_read_2(bst, bsh, ofs + i2) == 0x4444))
		return 8192 * 2;

	if (bus_space_read_2(bst, bsh, ofs + i1) == 0x1111 ||
	    bus_space_read_2(bst, bsh, ofs + i1) == 0x4444)
		return 8192;

	return 0;
}

/*
 * Do bus-independent setup.
 */
int
aesetup(sc)
	struct ae_softc *sc;
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	int i;

	sc->cr_proto = ED_CR_RD2;

	/* Allocate one xmit buffer if < 16k, two buffers otherwise. */
	if ((sc->mem_size < 16384) ||
	    (sc->sc_flags & AE_FLAGS_NO_DOUBLE_BUFFERING))
		sc->txb_cnt = 1;
	else
		sc->txb_cnt = 2;

	sc->tx_page_start = 0;
	sc->rec_page_start = sc->tx_page_start + sc->txb_cnt * ED_TXBUF_SIZE;
	sc->rec_page_stop = sc->tx_page_start + (sc->mem_size >> ED_PAGE_SHIFT);
	sc->mem_ring = sc->rec_page_start << ED_PAGE_SHIFT;

	/* Now zero memory and verify that it is clear. */
	bus_space_set_region_2(sc->sc_buft, sc->sc_bufh,
	    0, 0, sc->mem_size / 2);

	for (i = 0; i < sc->mem_size; ++i) {
		if (bus_space_read_1(sc->sc_buft, sc->sc_bufh, i)) {
printf(": failed to clear shared memory - check configuration\n");
			return 1;
		}
	}

	/* Set interface to stopped condition (reset). */
	aestop(sc);

	/* Initialize ifnet structure. */
	bcopy(sc->sc_dev.dv_xname, ifp->if_xname, IFNAMSIZ);
	ifp->if_softc = sc;
	ifp->if_start = aestart;
	ifp->if_ioctl = aeioctl;
	if (!ifp->if_watchdog)
		ifp->if_watchdog = aewatchdog;
	ifp->if_flags =
	    IFF_BROADCAST | IFF_SIMPLEX | IFF_NOTRAILERS | IFF_MULTICAST;

	/* Attach the interface. */
	if_attach(ifp);
	ether_ifattach(ifp);

	/* Print additional info when attached. */
	printf(": address %s, ", ether_sprintf(sc->sc_arpcom.ac_enaddr));

	printf("type %s, %dKB memory\n", sc->type_str, sc->mem_size / 1024);

	return 0;
}

/*
 * Reset interface.
 */
void
aereset(sc)
	struct ae_softc *sc;
{
	int     s;

	s = splnet();
	aestop(sc);
	aeinit(sc);
	splx(s);
}

/*
 * Take interface offline.
 */
void
aestop(sc)
	struct ae_softc *sc;
{
	int     n = 5000;

	/* Stop everything on the interface, and select page 0 registers. */
	NIC_PUT(sc, ED_P0_CR, sc->cr_proto | ED_CR_PAGE_0 | ED_CR_STP);

	/*
	 * Wait for interface to enter stopped state, but limit # of checks to
	 * 'n' (about 5ms).  It shouldn't even take 5us on modern DS8390's, but
	 * just in case it's an old one.
	 */
	while (((NIC_GET(sc, ED_P0_ISR) & ED_ISR_RST) == 0) && --n);
}

/*
 * Device timeout/watchdog routine.  Entered if the device neglects to generate
 * an interrupt after a transmit has been started on it.
 */

void
aewatchdog(ifp)
	struct ifnet *ifp;
{
	struct ae_softc *sc = ifp->if_softc;

	log(LOG_ERR, "%s: device timeout\n", sc->sc_dev.dv_xname);
	++sc->sc_arpcom.ac_if.if_oerrors;

	aereset(sc);
}

/*
 * Initialize device.
 */
void
aeinit(sc)
	struct ae_softc *sc;
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	u_int8_t mcaf[8];
	int i;

	/*
	 * Initialize the NIC in the exact order outlined in the NS manual.
	 * This init procedure is "mandatory"...don't change what or when
	 * things happen.
	 */

	/* Reset transmitter flags. */
	ifp->if_timer = 0;

	sc->txb_inuse = 0;
	sc->txb_new = 0;
	sc->txb_next_tx = 0;

	/* Set interface for page 0, remote DMA complete, stopped. */
	NIC_PUT(sc, ED_P0_CR, sc->cr_proto | ED_CR_PAGE_0 | ED_CR_STP);

	if (sc->use16bit) {
		/*
		 * Set FIFO threshold to 8, No auto-init Remote DMA, byte
		 * order=80x86, word-wide DMA xfers,
		 */
		NIC_PUT(sc, ED_P0_DCR, ED_DCR_FT1 | ED_DCR_WTS | ED_DCR_LS);
	} else {
		/* Same as above, but byte-wide DMA xfers. */
		NIC_PUT(sc, ED_P0_DCR, ED_DCR_FT1 | ED_DCR_LS);
	}

	/* Clear remote byte count registers. */
	NIC_PUT(sc, ED_P0_RBCR0, 0);
	NIC_PUT(sc, ED_P0_RBCR1, 0);

	/* Tell RCR to do nothing for now. */
	NIC_PUT(sc, ED_P0_RCR, ED_RCR_MON);

	/* Place NIC in internal loopback mode. */
	NIC_PUT(sc, ED_P0_TCR, ED_TCR_LB0);

	/* Initialize receive buffer ring. */
	NIC_PUT(sc, ED_P0_TPSR, sc->rec_page_start);
	NIC_PUT(sc, ED_P0_PSTART, sc->rec_page_start);

	NIC_PUT(sc, ED_P0_PSTOP, sc->rec_page_stop);
	NIC_PUT(sc, ED_P0_BNRY, sc->rec_page_start);

	/*
	 * Clear all interrupts.  A '1' in each bit position clears the
	 * corresponding flag.
	 */
	NIC_PUT(sc, ED_P0_ISR, 0xff);

	/*
	 * Enable the following interrupts: receive/transmit complete,
	 * receive/transmit error, and Receiver OverWrite.
	 *
	 * Counter overflow and Remote DMA complete are *not* enabled.
	 */
	NIC_PUT(sc, ED_P0_IMR,
	    ED_IMR_PRXE | ED_IMR_PTXE | ED_IMR_RXEE | ED_IMR_TXEE |
	    ED_IMR_OVWE);

	/* Program command register for page 1. */
	NIC_PUT(sc, ED_P0_CR, sc->cr_proto | ED_CR_PAGE_1 | ED_CR_STP);

	/* Copy out our station address. */
	for (i = 0; i < ETHER_ADDR_LEN; ++i)
		NIC_PUT(sc, ED_P1_PAR0 + i, sc->sc_arpcom.ac_enaddr[i]);

	/* Set multicast filter on chip. */
	ae_getmcaf(&sc->sc_arpcom, mcaf);
	for (i = 0; i < 8; i++)
		NIC_PUT(sc, ED_P1_MAR0 + i, mcaf[i]);

	/*
	 * Set current page pointer to one page after the boundary pointer, as
	 * recommended in the National manual.
	 */
	sc->next_packet = sc->rec_page_start + 1;
	NIC_PUT(sc, ED_P1_CURR, sc->next_packet);

	/* Program command register for page 0. */
	NIC_PUT(sc, ED_P1_CR, sc->cr_proto | ED_CR_PAGE_0 | ED_CR_STP);

	i = ED_RCR_AB | ED_RCR_AM;
	if (ifp->if_flags & IFF_PROMISC) {
		/*
		 * Set promiscuous mode.  Multicast filter was set earlier so
		 * that we should receive all multicast packets.
		 */
		i |= ED_RCR_PRO | ED_RCR_AR | ED_RCR_SEP;
	}
	NIC_PUT(sc, ED_P0_RCR, i);

	/* Take interface out of loopback. */
	NIC_PUT(sc, ED_P0_TCR, 0);

	/* Fire up the interface. */
	NIC_PUT(sc, ED_P0_CR, sc->cr_proto | ED_CR_PAGE_0 | ED_CR_STA);

	/* Set 'running' flag, and clear output active flag. */
	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	/* ...and attempt to start output. */
	aestart(ifp);
}

/*
 * This routine actually starts the transmission on the interface.
 */
static inline void
ae_xmit(sc)
	struct ae_softc *sc;
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	u_short len;

	len = sc->txb_len[sc->txb_next_tx];

	/* Set NIC for page 0 register access. */
	NIC_PUT(sc, ED_P0_CR, sc->cr_proto | ED_CR_PAGE_0 | ED_CR_STA);

	/* Set TX buffer start page. */
	NIC_PUT(sc, ED_P0_TPSR, sc->tx_page_start +
	    sc->txb_next_tx * ED_TXBUF_SIZE);

	/* Set TX length. */
	NIC_PUT(sc, ED_P0_TBCR0, len);
	NIC_PUT(sc, ED_P0_TBCR1, len >> 8);

	/* Set page 0, remote DMA complete, transmit packet, and *start*. */
	NIC_PUT(sc, ED_P0_CR, sc->cr_proto | ED_CR_PAGE_0 | ED_CR_TXP | ED_CR_STA);

	/* Point to next transmit buffer slot and wrap if necessary. */
	sc->txb_next_tx++;
	if (sc->txb_next_tx == sc->txb_cnt)
		sc->txb_next_tx = 0;

	/* Set a timer just in case we never hear from the board again. */
	ifp->if_timer = 2;
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
aestart(ifp)
	struct ifnet *ifp;
{
	struct ae_softc *sc = ifp->if_softc;
	struct mbuf *m0;
	int buffer;
	int len;

	if ((ifp->if_flags & (IFF_RUNNING | IFF_OACTIVE)) != IFF_RUNNING)
		return;

outloop:
	/* See if there is room to put another packet in the buffer. */
	if (sc->txb_inuse == sc->txb_cnt) {
		/* No room.  Indicate this to the outside world and exit. */
		ifp->if_flags |= IFF_OACTIVE;
		return;
	}
	IF_DEQUEUE(&ifp->if_snd, m0);
	if (m0 == 0)
		return;

	/* We need to use m->m_pkthdr.len, so require the header */
	if ((m0->m_flags & M_PKTHDR) == 0)
		panic("aestart: no header mbuf");

#if NBPFILTER > 0
	/* Tap off here if there is a BPF listener. */
	if (ifp->if_bpf)
		bpf_mtap(ifp->if_bpf, m0);
#endif

	/* txb_new points to next open buffer slot. */
	buffer = (sc->txb_new * ED_TXBUF_SIZE) << ED_PAGE_SHIFT;

	len = ae_put(sc, m0, buffer);
#ifdef DIAGNOSTIC
	if (len != m0->m_pkthdr.len)
		printf("aestart: len %d != m0->m_pkthdr.len %d.\n",
			len, m0->m_pkthdr.len);
#endif
	len = m0->m_pkthdr.len;

	m_freem(m0);
	sc->txb_len[sc->txb_new] = max(len, ETHER_MIN_LEN);

	/* Start the first packet transmitting. */
	if (sc->txb_inuse == 0)
		ae_xmit(sc);

	/* Point to next buffer slot and wrap if necessary. */
	if (++sc->txb_new == sc->txb_cnt)
		sc->txb_new = 0;

	sc->txb_inuse++;

	/* Loop back to the top to possibly buffer more packets. */
	goto outloop;
}

/*
 * Ethernet interface receiver interrupt.
 */
static inline void
ae_rint(sc)
	struct ae_softc *sc;
{
	u_char boundary, current;
	u_short len;
	u_char nlen;
	u_int8_t *lenp;
	struct ae_ring packet_hdr;
	int packet_ptr;

loop:
	/* Set NIC to page 1 registers to get 'current' pointer. */
	NIC_PUT(sc, ED_P0_CR, sc->cr_proto | ED_CR_PAGE_1 | ED_CR_STA);

	/*
	 * 'sc->next_packet' is the logical beginning of the ring-buffer - i.e.
	 * it points to where new data has been buffered.  The 'CURR' (current)
	 * register points to the logical end of the ring-buffer - i.e. it
	 * points to where additional new data will be added.  We loop here
	 * until the logical beginning equals the logical end (or in other
	 * words, until the ring-buffer is empty).
	 */
	current = NIC_GET(sc, ED_P1_CURR);
	if (sc->next_packet == current)
		return;

	/* Set NIC to page 0 registers to update boundary register. */
	NIC_PUT(sc, ED_P1_CR, sc->cr_proto | ED_CR_PAGE_0 | ED_CR_STA);

	do {
		/* Get pointer to this buffer's header structure. */
		packet_ptr = sc->mem_ring +
		    ((sc->next_packet - sc->rec_page_start) << ED_PAGE_SHIFT);

		/*
		 * The byte count includes a 4 byte header that was added by
		 * the NIC.
		 */
		bus_space_read_region_1(sc->sc_buft, sc->sc_bufh,
		    packet_ptr, &packet_hdr, sizeof(struct ae_ring));
		lenp = (u_int8_t *)&packet_hdr.count; /* sigh. */
		len = lenp[0] | (lenp[1] << 8);
		packet_hdr.count = len;

		/*
		 * Try do deal with old, buggy chips that sometimes duplicate
		 * the low byte of the length into the high byte.  We do this
		 * by simply ignoring the high byte of the length and always
		 * recalculating it.
		 *
		 * NOTE: sc->next_packet is pointing at the current packet.
		 */
		if (packet_hdr.next_packet >= sc->next_packet)
			nlen = (packet_hdr.next_packet - sc->next_packet);
		else
			nlen = ((packet_hdr.next_packet - sc->rec_page_start) +
			    (sc->rec_page_stop - sc->next_packet));
		--nlen;
		if ((len & ED_PAGE_MASK) + sizeof(packet_hdr) > ED_PAGE_SIZE)
			--nlen;
		len = (len & ED_PAGE_MASK) | (nlen << ED_PAGE_SHIFT);
#ifdef DIAGNOSTIC
		if (len != packet_hdr.count) {
			printf("%s: length does not match next packet pointer\n",
			    sc->sc_dev.dv_xname);
			printf("%s: len %04x nlen %04x start %02x first %02x curr %02x next %02x stop %02x\n",
			    sc->sc_dev.dv_xname, packet_hdr.count, len,
			    sc->rec_page_start, sc->next_packet, current,
			    packet_hdr.next_packet, sc->rec_page_stop);
		}
#endif

		/*
		 * Be fairly liberal about what we allow as a "reasonable"
		 * length so that a [crufty] packet will make it to BPF (and
		 * can thus be analyzed).  Note that all that is really
		 * important is that we have a length that will fit into one
		 * mbuf cluster or less; the upper layer protocols can then
		 * figure out the length from their own length field(s).
		 */
		if (len <= MCLBYTES &&
		    packet_hdr.next_packet >= sc->rec_page_start &&
		    packet_hdr.next_packet < sc->rec_page_stop) {
			/* Go get packet. */
			aeread(sc, packet_ptr + sizeof(struct ae_ring),
			    len - sizeof(struct ae_ring));
			++sc->sc_arpcom.ac_if.if_ipackets;
		} else {
			/* Really BAD.  The ring pointers are corrupted. */
			log(LOG_ERR,
			    "%s: NIC memory corrupt - invalid packet length %d\n",
			    sc->sc_dev.dv_xname, len);
			++sc->sc_arpcom.ac_if.if_ierrors;
			aereset(sc);
			return;
		}

		/* Update next packet pointer. */
		sc->next_packet = packet_hdr.next_packet;

		/*
		 * Update NIC boundary pointer - being careful to keep it one
		 * buffer behind (as recommended by NS databook).
		 */
		boundary = sc->next_packet - 1;
		if (boundary < sc->rec_page_start)
			boundary = sc->rec_page_stop - 1;
		NIC_PUT(sc, ED_P0_BNRY, boundary);
	} while (sc->next_packet != current);

	goto loop;
}

/* Ethernet interface interrupt processor. */
int
aeintr(void *arg)
{
	struct ae_softc *sc = (struct ae_softc *)arg;
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	u_char isr;

	/* Set NIC to page 0 registers. */
	NIC_PUT(sc, ED_P0_CR, sc->cr_proto | ED_CR_PAGE_0 | ED_CR_STA);

	isr = NIC_GET(sc, ED_P0_ISR);
	if (!isr)
		return (0);

	/* Loop until there are no more new interrupts. */
	for (;;) {
		/*
		 * Reset all the bits that we are 'acknowledging' by writing a
		 * '1' to each bit position that was set.
		 * (Writing a '1' *clears* the bit.)
		 */
		NIC_PUT(sc, ED_P0_ISR, isr);

		/*
		 * Handle transmitter interrupts.  Handle these first because
		 * the receiver will reset the board under some conditions.
		 */
		if (isr & (ED_ISR_PTX | ED_ISR_TXE)) {
			u_char  collisions = NIC_GET(sc, ED_P0_NCR) & 0x0f;

			/*
			 * Check for transmit error.  If a TX completed with an
			 * error, we end up throwing the packet away.  Really
			 * the only error that is possible is excessive
			 * collisions, and in this case it is best to allow the
			 * automatic mechanisms of TCP to backoff the flow.  Of
			 * course, with UDP we're screwed, but this is expected
			 * when a network is heavily loaded.
			 */
			(void) NIC_GET(sc, ED_P0_TSR);
			if (isr & ED_ISR_TXE) {
				/*
				 * Excessive collisions (16).
				 */
				if ((NIC_GET(sc, ED_P0_TSR) & ED_TSR_ABT)
				    && (collisions == 0)) {
					/*
					 * When collisions total 16, the P0_NCR
					 * will indicate 0, and the TSR_ABT is
					 * set.
					 */
					collisions = 16;
				}

				/* Update output errors counter. */
				++ifp->if_oerrors;
			} else {
				/*
				 * Update total number of successfully
				 * transmitted packets.
				 */
				++ifp->if_opackets;
			}

			/* Done with the buffer. */
			sc->txb_inuse--;

			/* Clear watchdog timer. */
			ifp->if_timer = 0;
			ifp->if_flags &= ~IFF_OACTIVE;

			/*
			 * Add in total number of collisions on last
			 * transmission.
			 */
			ifp->if_collisions += collisions;

			/*
			 * Decrement buffer in-use count if not zero (can only
			 * be zero if a transmitter interrupt occurred while not
			 * actually transmitting).
			 * If data is ready to transmit, start it transmitting,
			 * otherwise defer until after handling receiver.
			 */
			if (sc->txb_inuse > 0)
				ae_xmit(sc);
		}

		/* Handle receiver interrupts. */
		if (isr & (ED_ISR_PRX | ED_ISR_RXE | ED_ISR_OVW)) {
			/*
			 * Overwrite warning.  In order to make sure that a
			 * lockup of the local DMA hasn't occurred, we reset
			 * and re-init the NIC.  The NSC manual suggests only a
			 * partial reset/re-init is necessary - but some chips
			 * seem to want more.  The DMA lockup has been seen
			 * only with early rev chips - Methinks this bug was
			 * fixed in later revs.  -DG
			 */
			if (isr & ED_ISR_OVW) {
				++ifp->if_ierrors;
#ifdef DIAGNOSTIC
				log(LOG_WARNING,
				    "%s: warning - receiver ring buffer overrun\n",
				    sc->sc_dev.dv_xname);
#endif
				/* Stop/reset/re-init NIC. */
				aereset(sc);
			} else {
				/*
				 * Receiver Error.  One or more of: CRC error,
				 * frame alignment error FIFO overrun, or
				 * missed packet.
				 */
				if (isr & ED_ISR_RXE) {
					++ifp->if_ierrors;
#ifdef AE_DEBUG
					printf("%s: receive error %x\n",
					    sc->sc_dev.dv_xname,
					    NIC_GET(sc, ED_P0_RSR));
#endif
				}

				/*
				 * Go get the packet(s)
				 * XXX - Doing this on an error is dubious
				 * because there shouldn't be any data to get
				 * (we've configured the interface to not
				 * accept packets with errors).
				 */
				ae_rint(sc);
			}
		}

		/*
		 * If it looks like the transmitter can take more data, attempt
		 * to start output on the interface.  This is done after
		 * handling the receiver to give the receiver priority.
		 */
		aestart(ifp);

		/*
		 * Return NIC CR to standard state: page 0, remote DMA
		 * complete, start (toggling the TXP bit off, even if was just
		 * set in the transmit routine, is *okay* - it is 'edge'
		 * triggered from low to high).
		 */
		NIC_PUT(sc, ED_P0_CR, sc->cr_proto | ED_CR_PAGE_0 | ED_CR_STA);

		/*
		 * If the Network Talley Counters overflow, read them to reset
		 * them.  It appears that old 8390's won't clear the ISR flag
		 * otherwise - resulting in an infinite loop.
		 */
		if (isr & ED_ISR_CNT) {
			(void)NIC_GET(sc, ED_P0_CNTR0);
			(void)NIC_GET(sc, ED_P0_CNTR1);
			(void)NIC_GET(sc, ED_P0_CNTR2);
		}

		isr = NIC_GET(sc, ED_P0_ISR);
		if (!isr)
			break;
	}
	return (1);
}

/*
 * Process an ioctl request.
 * XXX - This code needs some work - it looks pretty ugly.
 */
int
aeioctl(ifp, cmd, data)
	struct ifnet *ifp;
	u_long cmd;
	caddr_t data;
{
	struct ae_softc *sc = ifp->if_softc;
	struct ifaddr *ifa = (struct ifaddr *) data;
	struct ifreq *ifr = (struct ifreq *) data;
	int     s, error = 0;

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
			aeinit(sc);
			arp_ifinit(&sc->sc_arpcom, ifa);
			break;
#endif
		default:
			aeinit(sc);
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
			aestop(sc);
			ifp->if_flags &= ~IFF_RUNNING;
		} else
			if ((ifp->if_flags & IFF_UP) != 0 &&
			    (ifp->if_flags & IFF_RUNNING) == 0) {
				/*
				 * If interface is marked up and it is stopped, then
				 * start it.
				 */
				aeinit(sc);
			} else {
				/*
				 * Reset the interface to pick up changes in any other
				 * flags that affect hardware registers.
				 */
				aestop(sc);
				aeinit(sc);
			}
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		/* Update our multicast list. */
		error = (cmd == SIOCADDMULTI) ?
		    ether_addmulti(ifr, &sc->sc_arpcom) :
		    ether_delmulti(ifr, &sc->sc_arpcom);

		if (error == ENETRESET) {
			/*
			 * Multicast list has changed; set the hardware filter
			 * accordingly.
			 */
			if (ifp->if_flags & IFF_RUNNING) {
				aestop(sc);	/* XXX for ds_setmcaf? */
				aeinit(sc);
			}
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

/*
 * Retreive packet from shared memory and send to the next level up via
 * ether_input().  If there is a BPF listener, give a copy to BPF, too.
 */
void
aeread(sc, buf, len)
	struct ae_softc *sc;
	int buf;
	int len;
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	struct mbuf *m;

	/* Pull packet off interface. */
	m = aeget(sc, buf, len);
	if (m == 0) {
		ifp->if_ierrors++;
		return;
	}

	ifp->if_ipackets++;

#if NBPFILTER > 0
	/*
	 * Check if there's a BPF listener on this interface.
	 * If so, hand off the raw packet to bpf.
	 */
	if (ifp->if_bpf)
		bpf_mtap(ifp->if_bpf, m);
#endif

	ether_input_mbuf(ifp, m);
}

/*
 * Supporting routines.
 */

/*
 * Given a source and destination address, copy 'amount' of a packet from the
 * ring buffer into a linear destination buffer.  Takes into account ring-wrap.
 */
static inline int
ae_ring_copy(sc, src, dst, amount)
	struct ae_softc *sc;
	int src;
	caddr_t dst;
	int amount;
{
	bus_space_tag_t bst = sc->sc_buft;
	bus_space_handle_t bsh = sc->sc_bufh;
	int tmp_amount;

	/* Does copy wrap to lower addr in ring buffer? */
	if (src + amount > sc->mem_size) {
		tmp_amount = sc->mem_size - src;

		/* Copy amount up to end of NIC memory. */
		bus_space_read_region_1(bst, bsh, src, dst, tmp_amount);

		amount -= tmp_amount;
		src = sc->mem_ring;
		dst += tmp_amount;
	}
	bus_space_read_region_1(bst, bsh, src, dst, amount);

	return (src + amount);
}

/*
 * Copy data from receive buffer to end of mbuf chain allocate additional mbufs
 * as needed.  Return pointer to last mbuf in chain.
 * sc = ae info (softc)
 * src = pointer in ae ring buffer
 * dst = pointer to last mbuf in mbuf chain to copy to
 * amount = amount of data to copy
 */
struct mbuf *
aeget(sc, src, total_len)
	struct ae_softc *sc;
	int src;
	u_short total_len;
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	struct mbuf *top, **mp, *m;
	int len;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == 0)
		return 0;
	m->m_pkthdr.rcvif = ifp;
	m->m_pkthdr.len = total_len;
	len = MHLEN;
	top = 0;
	mp = &top;

	while (total_len > 0) {
		if (top) {
			MGET(m, M_DONTWAIT, MT_DATA);
			if (m == 0) {
				m_freem(top);
				return 0;
			}
			len = MLEN;
		}
		if (total_len >= MINCLSIZE) {
			MCLGET(m, M_DONTWAIT);
			if ((m->m_flags & M_EXT) == 0) {
				m_freem(top);
				return 0;
			}
			len = MCLBYTES;
		}
		m->m_len = len = min(total_len, len);
		src = ae_ring_copy(sc, src, mtod(m, caddr_t), len);
		total_len -= len;
		*mp = m;
		mp = &m->m_next;
	}

	return top;
}

/*
 * Compute the multicast address filter from the list of multicast addresses we
 * need to listen to.
 */
void
ae_getmcaf(ac, af)
	struct arpcom *ac;
	u_char *af;
{
	struct ifnet *ifp = &ac->ac_if;
	struct ether_multi *enm;
	u_char *cp, c;
	u_int32_t crc;
	int i, len;
	struct ether_multistep step;

	/*
	 * Set up multicast address filter by passing all multicast addresses
	 * through a crc generator, and then using the high order 6 bits as an
	 * index into the 64 bit logical address filter.  The high order bit
	 * selects the word, while the rest of the bits select the bit within
	 * the word.
	 */

	if (ifp->if_flags & IFF_PROMISC) {
		ifp->if_flags |= IFF_ALLMULTI;
		for (i = 0; i < 8; i++)
			af[i] = 0xff;
		return;
	}
	for (i = 0; i < 8; i++)
		af[i] = 0;
	ETHER_FIRST_MULTI(step, ac, enm);
	while (enm != NULL) {
		if (bcmp(enm->enm_addrlo, enm->enm_addrhi,
			sizeof(enm->enm_addrlo)) != 0) {
			/*
			 * We must listen to a range of multicast addresses.
			 * For now, just accept all multicasts, rather than
			 * trying to set only those filter bits needed to match
			 * the range.  (At this time, the only use of address
			 * ranges is for IP multicast routing, for which the
			 * range is big enough to require all bits set.)
			 */
			ifp->if_flags |= IFF_ALLMULTI;
			for (i = 0; i < 8; i++)
				af[i] = 0xff;
			return;
		}
		cp = enm->enm_addrlo;
		crc = 0xffffffff;
		for (len = sizeof(enm->enm_addrlo); --len >= 0;) {
			c = *cp++;
			for (i = 8; --i >= 0;) {
				if (((crc & 0x80000000) ? 1 : 0) ^ (c & 0x01)) {
					crc <<= 1;
					crc ^= 0x04c11db6 | 1;
				} else
					crc <<= 1;
				c >>= 1;
			}
		}
		/* Just want the 6 most significant bits. */
		crc >>= 26;

		/* Turn on the corresponding bit in the filter. */
		af[crc >> 3] |= 1 << (crc & 0x7);

		ETHER_NEXT_MULTI(step, enm);
	}
	ifp->if_flags &= ~IFF_ALLMULTI;
}

/*
 * Copy packet from mbuf to the board memory
 *
 * Currently uses an extra buffer/extra memory copy,
 * unless the whole packet fits in one mbuf.
 *
 */
int
ae_put(sc, m, buf)
	struct ae_softc *sc;
	struct mbuf *m;
	int buf;
{
	u_char *data, savebyte[2];
	int len, wantbyte;
	u_short totlen = 0;

	wantbyte = 0;

	for (; m ; m = m->m_next) {
		data = mtod(m, u_char *);
		len = m->m_len;
		totlen += len;
		if (len > 0) {
			/* Finish the last word. */
			if (wantbyte) {
				savebyte[1] = *data;
				bus_space_write_region_2(sc->sc_buft,
				    sc->sc_bufh, buf, savebyte, 1);
				buf += 2;
				data++;
				len--;
				wantbyte = 0;
			}
			/* Output contiguous words. */
			if (len > 1) {
				bus_space_write_region_2(sc->sc_buft,
				    sc->sc_bufh, buf, data, len >> 1);
				buf += len & ~1;
				data += len & ~1;
				len &= 1;
			}
			/* Save last byte, if necessary. */
			if (len == 1) {
				savebyte[0] = *data;
				wantbyte = 1;
			}
		}
	}

	if (wantbyte) {
		savebyte[1] = 0;
		bus_space_write_region_2(sc->sc_buft, sc->sc_bufh,
		    buf, savebyte, 1);
	}
	return (totlen);
}
