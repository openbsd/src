/*	$NetBSD: if_qn.c,v 1.3 1995/12/24 02:30:02 mycroft Exp $	*/

/*
 * Copyright (c) 1995 Mika Kortelainen
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
 *      This product includes software developed by  Mika Kortelainen
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
 *
 * Thanks for Aspecs Oy (Finland) for the data book for the NIC used
 * in this card and also many thanks for the Resource Management Force
 * (QuickNet card manufacturer) and especially Daniel Koch for providing
 * me with the necessary 'inside' information to write the driver.
 *
 * This is partly based on other code:
 * - if_ed.c: basic function structure for Ethernet driver and now also
 *	qn_put() is done similarly, i.e. no extra packet buffers.
 *
 *	Device driver for National Semiconductor DS8390/WD83C690 based ethernet
 *	adapters.
 *
 *	Copyright (c) 1994, 1995 Charles M. Hannum.  All rights reserved.
 *
 *	Copyright (C) 1993, David Greenman.  This software may be used,
 *	modified, copied, distributed, and sold, in both source and binary
 *	form provided that the above copyright and these terms are retained.
 *	Under no circumstances is the author responsible for the proper
 *	functioning of this software, nor does the author assume any
 *	responsibility for damages incurred with its use.
 *
 * - if_es.c: used as an example of -current driver
 *
 *	Copyright (c) 1995 Michael L. Hitch
 *	All rights reserved.
 *
 * - if_fe.c: some ideas for error handling for qn_rint() which might
 *	have fixed those random lock ups, too.
 *
 *	All Rights Reserved, Copyright (C) Fujitsu Limited 1995
 *
 *
 * TODO:
 * - add multicast support
 */

#include "qn.h"
#if NQN > 0

#define QN_DEBUG
#define QN_DEBUG1_no /* hides some old tests */

#include "bpfilter.h"

/*
 * Fujitsu MB86950 Ethernet Controller (as used in the QuickNet QN2000
 * Ethernet card)
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/buf.h>
#include <sys/device.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/ioctl.h>
#include <sys/errno.h>

#include <net/if.h>
#include <net/netisr.h>
#include <net/route.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#endif

#ifdef NS
#include <netns/ns.h>
#include <netns/ns_if.h>
#endif

#include <machine/cpu.h>
#include <machine/mtpr.h>
#include <amiga/amiga/device.h>
#include <amiga/amiga/isr.h>
#include <amiga/dev/zbusvar.h>
#include <amiga/dev/if_qnreg.h>


#define ETHER_MIN_LEN	60
#define ETHER_MAX_LEN	1514
#define ETHER_HDR_SIZE	14
#define	NIC_R_MASK	(R_INT_PKT_RDY | R_INT_ALG_ERR |\
			 R_INT_CRC_ERR | R_INT_OVR_FLO)
#define	MAX_PACKETS	30 /* max number of packets read per interrupt */

/*
 * Ethernet software status per interface
 *
 * Each interface is referenced by a network interface structure,
 * qn_if, which the routing code uses to locate the interface.
 * This structure contains the output queue for the interface, its address, ...
 */
struct	qn_softc {
	struct	device sc_dev;
	struct	isr sc_isr;
	struct	arpcom sc_arpcom;	/* Common ethernet structures */
	u_char	volatile *sc_base;
	u_char	volatile *sc_nic_base;
	u_short	volatile *nic_fifo;
	u_short	volatile *nic_r_status;
	u_short	volatile *nic_t_status;
	u_short	volatile *nic_r_mask;
	u_short	volatile *nic_t_mask;
	u_short	volatile *nic_r_mode;
	u_short	volatile *nic_t_mode;
	u_short	volatile *nic_reset;
	u_short	volatile *nic_len;
	u_char	transmit_pending;
#if NBPFILTER > 0
	caddr_t	sc_bpf;
#endif
} qn_softc[NQN];

#if NBPFILTER > 0
#include <net/bpf.h>
#include <net/bpfdesc.h>
#endif


int	qnmatch __P((struct device *, void *, void *));
void	qnattach __P((struct device *, struct device *, void *));
int	qnintr __P((struct qn_softc *sc));
int	qnioctl __P((struct ifnet *, u_long, caddr_t));
void	qnstart __P((struct ifnet *));
void	qnwatchdog __P((int));
void	qnreset __P((struct qn_softc *));
void	qninit __P((struct qn_softc *));
void	qnstop __P((struct qn_softc *));
static	u_short qn_put __P((u_short volatile *, struct mbuf *));
static	void qn_rint __P((struct qn_softc *, u_short));
static	void qn_flush __P((struct qn_softc *));
#ifdef QN_DEBUG1
static	void qn_dump __P((struct qn_softc *));
#endif

struct cfdriver qncd = {
	NULL, "qn", qnmatch, qnattach, DV_IFNET, sizeof(struct qn_softc)
};


int
qnmatch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct zbus_args *zap;

	zap = (struct zbus_args *)aux;

	/* RMF QuickNet QN2000 EtherNet card */
	if (zap->manid == 2011 && zap->prodid == 2)
		return (1);

	return (0);
}

/*
 * Interface exists: make available by filling in network interface
 * record.  System will initialize the interface when it is ready
 * to accept packets.
 */
void
qnattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct zbus_args *zap;
	struct qn_softc *sc = (struct qn_softc *)self;
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	int i;

	zap = (struct zbus_args *)aux;

	sc->sc_base = zap->va;
	sc->sc_nic_base = sc->sc_base + QUICKNET_NIC_BASE;
	sc->nic_fifo = (u_short volatile *)(sc->sc_nic_base + NIC_BMPR0);
	sc->nic_len = (u_short volatile *)(sc->sc_nic_base + NIC_BMPR2);
	sc->nic_t_status = (u_short volatile *)(sc->sc_nic_base + NIC_DLCR0);
	sc->nic_r_status = (u_short volatile *)(sc->sc_nic_base + NIC_DLCR2);
	sc->nic_t_mask = (u_short volatile *)(sc->sc_nic_base + NIC_DLCR1);
	sc->nic_r_mask = (u_short volatile *)(sc->sc_nic_base + NIC_DLCR3);
	sc->nic_t_mode = (u_short volatile *)(sc->sc_nic_base + NIC_DLCR4);
	sc->nic_r_mode = (u_short volatile *)(sc->sc_nic_base + NIC_DLCR5);
	sc->nic_reset = (u_short volatile *)(sc->sc_nic_base + NIC_DLCR6);
	sc->transmit_pending = 0;

	/*
	 * The ethernet address of the board (1st three bytes are the vendor
	 * address, the rest is the serial number of the board).
	 */
	sc->sc_arpcom.ac_enaddr[0] = 0x5c;
	sc->sc_arpcom.ac_enaddr[1] = 0x5c;
	sc->sc_arpcom.ac_enaddr[2] = 0x00;
	sc->sc_arpcom.ac_enaddr[3] = (zap->serno >> 16) & 0xff;
	sc->sc_arpcom.ac_enaddr[4] = (zap->serno >> 8) & 0xff;
	sc->sc_arpcom.ac_enaddr[5] = zap->serno & 0xff;

	/* set interface to stopped condition (reset) */
	qnstop(sc);

	ifp->if_unit = sc->sc_dev.dv_unit;
	ifp->if_name = qncd.cd_name;
	ifp->if_ioctl = qnioctl;
	ifp->if_watchdog = qnwatchdog;
	ifp->if_output = ether_output;
	ifp->if_start = qnstart;
	/* XXX IFF_MULTICAST */
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_NOTRAILERS;
	ifp->if_mtu = ETHERMTU;

	/* Attach the interface. */
	if_attach(ifp);
	ether_ifattach(ifp);

#ifdef QN_DEBUG
	printf(": hardware address %s\n", ether_sprintf(sc->sc_arpcom.ac_enaddr));
#endif

#if NBPFILTER > 0
	bpfattach(&sc->sc_bpf, ifp, DLT_EN10MB, sizeof(struct ether_header));
#endif

	sc->sc_isr.isr_intr = qnintr;
	sc->sc_isr.isr_arg = sc;
	sc->sc_isr.isr_ipl = 2;
	add_isr(&sc->sc_isr);
}

/*
 * Initialize device
 *
 */
void
qninit(sc)
	struct qn_softc *sc;
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	u_short i;
	static retry = 0;

	*sc->nic_r_mask   = NIC_R_MASK;
	*sc->nic_t_mode   = NO_LOOPBACK;

	if (sc->sc_arpcom.ac_if.if_flags & IFF_PROMISC) {
		*sc->nic_r_mode = PROMISCUOUS_MODE;
		log(LOG_INFO, "qn: Promiscuous mode (not tested)\n");
	} else
		*sc->nic_r_mode = NORMAL_MODE;

	/* Set physical ethernet address. */
	for (i = 0; i < ETHER_ADDR_LEN; i++)
		*((u_short volatile *)(sc->sc_nic_base+
				       QNET_HARDWARE_ADDRESS+2*i)) =
		    ((((u_short)sc->sc_arpcom.ac_enaddr[i]) << 8) |
		    sc->sc_arpcom.ac_enaddr[i]);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;
	sc->transmit_pending = 0;

	qn_flush(sc);

	/* QuickNet magic. Done ONLY once, otherwise a lockup occurs. */
	if (retry == 0) {
		*((u_short volatile *)(sc->sc_nic_base + QNET_MAGIC)) = 0;
		retry = 1;
	}

	/* Enable data link controller. */
	*sc->nic_reset = ENABLE_DLC;

	/* Attempt to start output, if any. */
	qnstart(ifp);
}

/*
 * Device timeout/watchdog routine.  Entered if the device neglects to
 * generate an interrupt after a transmit has been started on it.
 */
void
qnwatchdog(unit)
	int unit;
{
	struct qn_softc *sc = qncd.cd_devs[unit];

	log(LOG_INFO, "qn: device timeout (watchdog)\n");
	++sc->sc_arpcom.ac_if.if_oerrors;

	qnreset(sc);
}

/*
 * Flush card's buffer RAM.
 */
static void
qn_flush(sc)
	struct qn_softc *sc;
{
#if 1
	/* Read data until bus read error (i.e. buffer empty). */
	while (!(*sc->nic_r_status & R_BUS_RD_ERR))
		(void)(*sc->nic_fifo);
#else
	/* Read data twice to clear some internal pipelines. */
	(void)(*sc->nic_fifo);
	(void)(*sc->nic_fifo);
#endif

	/* Clear bus read error. */
	*sc->nic_r_status = R_BUS_RD_ERR;
}

/*
 * Reset the interface.
 *
 */
void
qnreset(sc)
	struct qn_softc *sc;
{
	int s;

	s = splnet();
	qnstop(sc);
	qninit(sc);
	splx(s);
}

/*
 * Take interface offline.
 */
void
qnstop(sc)
	struct qn_softc *sc;
{

	/* Stop the interface. */
	*sc->nic_reset    = DISABLE_DLC;
	delay(200);
	*sc->nic_t_status = CLEAR_T_ERR;
	*sc->nic_t_mask   = CLEAR_T_MASK;
	*sc->nic_r_status = CLEAR_R_ERR;
	*sc->nic_r_mask   = CLEAR_R_MASK;

	/* Turn DMA off */
	*((u_short volatile *)(sc->sc_nic_base + NIC_BMPR4)) = 0;

	/* Accept no packets. */
	*sc->nic_r_mode = 0;
	*sc->nic_t_mode = 0;

	qn_flush(sc);
}

/*
 * Start output on interface. Get another datagram to send
 * off the interface queue, and copy it to the
 * interface before starting the output.
 *
 * This assumes that it is called inside a critical section...
 *
 */
void
qnstart(ifp)
	struct ifnet *ifp;
{
	struct qn_softc *sc = qncd.cd_devs[ifp->if_unit];
	struct mbuf *m;
	u_short len;
	int timout = 60000;


	if ((ifp->if_flags & (IFF_RUNNING | IFF_OACTIVE)) != IFF_RUNNING)
		return;

	IF_DEQUEUE(&sc->sc_arpcom.ac_if.if_snd, m);
	if (m == 0)
		return;

#if NBPFILTER > 0
	/*
	 * If bpf is listening on this interface, let it
	 * see the packet before we commit it to the wire
	 *
	 * (can't give the copy in QuickNet card RAM to bpf, because
	 * that RAM is not visible to the host but is read from FIFO)
	 *
	 */
	log(LOG_INFO, "NBPFILTER... no-one has tested this with qn.\n");
	if (sc->sc_bpf)
		bpf_mtap(sc->sc_bpf, m);
#endif
	len = qn_put(sc->nic_fifo, m);
	m_freem(m);

	/*
	 * Really transmit the packet.
	 */

	/* Set packet length (byte-swapped). */
	len = ((len >> 8) & 0x0007) | TRANSMIT_START | ((len & 0x00ff) << 8);
	*sc->nic_len = len;

	/* Wait for the packet to really leave. */
	while (!(*sc->nic_t_status & T_TMT_OK) && --timout) {
		if ((timout % 10000) == 0)
			log(LOG_INFO, "qn: timout...\n");
	}

	if (timout == 0)
		/* Maybe we should try to recover from this one? */
		/* But now, let's just fall thru and hope the best... */
		log(LOG_INFO, "qn: transmit timout (fatal?)\n");

	sc->transmit_pending = 1;
	*sc->nic_t_mask = INT_TMT_OK | INT_SIXTEEN_COL;

	sc->sc_arpcom.ac_if.if_flags |= IFF_OACTIVE;
	ifp->if_timer = 2;
}

/*
 * Memory copy, copies word at a time
 */
static void inline
word_copy_from_card(card, b, len)
	u_short volatile *card;
	u_short *b, len;
{
	register u_short l = len/2;

	while (l--)
		*b++ = *card;
}

static void inline
word_copy_to_card(a, card, len)
	u_short *a, len;
	u_short volatile *card;
{
	register u_short l = len/2;

	while (l--)
		*card = *a++;
}

/*
 * Copy packet from mbuf to the board memory
 *
 * Uses an extra buffer/extra memory copy,
 * unless the whole packet fits in one mbuf.
 *
 */
static u_short
qn_put(addr, m)
	u_short volatile *addr;
	struct mbuf *m;
{
	u_char *data, savebyte[2];
	int len, wantbyte;
	u_short totlen;

	totlen = wantbyte = 0;

	for (; m != NULL; m = m->m_next) {
		data = mtod(m, u_char *);
		len = m->m_len;
		totlen += len;
		if (len > 0) {
			/* Finish the last word. */
			if (wantbyte) {
				savebyte[1] = *data;
				*addr = *((u_short *)savebyte);
				data++;
				len--;
				wantbyte = 0;
			}
			/* Output contiguous words. */
			if (len > 1) {
				word_copy_to_card(data, addr, len);
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
		*addr = *((u_short *)savebyte);
	}

	if(totlen < ETHER_MIN_LEN) {
		/*
		 * Fill the rest of the packet with zeros.
		 * N.B.: This is required! Otherwise MB86950 fails.
		 */
		for(len = totlen + 1; len < ETHER_MIN_LEN; len += 2)
	  		*addr = (u_short)0x0000;
		totlen = ETHER_MIN_LEN;
	}

	return (totlen);
}

/*
 * Copy packet from board RAM.
 *
 * Trailers not supported.
 *
 */
static void
qn_get_packet(sc, len)
	struct qn_softc *sc;
	u_short len;
{
	register u_short volatile *nic_fifo_ptr = sc->nic_fifo;
	struct ether_header *eh;
	struct mbuf *m, *dst, *head = NULL;
	register u_short len1;
	u_short amount;

	/* Allocate header mbuf. */
	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		goto bad;

	/*
	 * Round len to even value.
	 */
	if (len & 1)
		len++;

	m->m_pkthdr.rcvif = &sc->sc_arpcom.ac_if;
	m->m_pkthdr.len = len;
	m->m_len = 0;
	head = m;

	eh = mtod(head, struct ether_header *);

	word_copy_from_card(nic_fifo_ptr,
	    mtod(head, u_short *),
	    sizeof(struct ether_header));

	head->m_len += sizeof(struct ether_header);
	len -= sizeof(struct ether_header);

	while (len > 0) {
		len1 = len;

		amount = M_TRAILINGSPACE(m);
		if (amount == 0) {
			/* Allocate another mbuf. */
			dst = m;
			MGET(m, M_DONTWAIT, MT_DATA);
			if (m == NULL)
				goto bad;

			if (len1 >= MINCLSIZE)
				MCLGET(m, M_DONTWAIT);

			m->m_len = 0;
			dst->m_next = m;

			amount = M_TRAILINGSPACE(m);
		}

		if (amount < len1)
			len1 = amount;

		word_copy_from_card(nic_fifo_ptr,
		    (u_short *)(mtod(m, caddr_t) + m->m_len),
		    len1);
		m->m_len += len1;
		len -= len1;
	}

#if NBPFILTER > 0
	log(LOG_INFO, "qn: Beware, an untested code section\n");
	if (sc->sc_bpf) {
		bpf_mtap(sc->sc_bpf, head);

		/*
		 * The interface cannot be in promiscuous mode if there are
		 * no BPF listeners. And in prom. mode we have to check
		 * if the packet is really ours...
		 */
		if ((sc->sc_arpcom.ac_if.if_flags & IFF_PROMISC) &&
		    (eh->ether_dhost[0] & 1) == 0 && /* not bcast or mcast */
		    bcmp(eh->ether_dhost,
        	        sc->sc_arpcom.ac_enaddr,
		        ETHER_ADDR_LEN) != 0) {
			m_freem(head);
			return;
		}
	}
#endif

	m_adj(head, sizeof(struct ether_header));
	ether_input(&sc->sc_arpcom.ac_if, eh, head);
	return;

bad:
	if (head) {
		m_freem(head);
		log(LOG_INFO, "qn_get_packet: mbuf alloc failed\n");
	}
}

/*
 * Ethernet interface receiver interrupt.
 */
static void
qn_rint(sc, rstat)
	struct qn_softc *sc;
	u_short rstat;
{
	int i;
	u_short len, status;

	/* Clear the status register. */
	*sc->nic_r_status = CLEAR_R_ERR;

	/*
	 * Was there some error?
	 * Some of them are senseless because they are masked off.
	 * XXX
	 */
	if (rstat & 0x0101) {
#ifdef QN_DEBUG
		log(LOG_INFO, "Overflow\n");
#endif
		++sc->sc_arpcom.ac_if.if_ierrors;
	}
	if (rstat & 0x0202) {
#ifdef QN_DEBUG
		log(LOG_INFO, "CRC Error\n");
#endif
		++sc->sc_arpcom.ac_if.if_ierrors;
	}
	if (rstat & 0x0404) {
#ifdef QN_DEBUG
		log(LOG_INFO, "Alignment error\n");
#endif
		++sc->sc_arpcom.ac_if.if_ierrors;
	}
	if (rstat & 0x0808) {
		/* Short packet (these may occur and are
		 * no reason to worry about - or maybe
		 * they are?).
		 */
#ifdef QN_DEBUG
		log(LOG_INFO, "Short packet\n");
#endif
		++sc->sc_arpcom.ac_if.if_ierrors;
	}
	if (rstat & 0x4040) {
#ifdef QN_DEBUG
		log(LOG_INFO, "Bus read error\n");
#endif
		++sc->sc_arpcom.ac_if.if_ierrors;
		qnreset(sc);
	}

	/*
	 * Read at most MAX_PACKETS packets per interrupt
	 */
	for (i = 0; i < MAX_PACKETS; i++) {
		if (*sc->nic_r_mode & RM_BUF_EMP)
			/* Buffer empty. */
			break;

		/*
		 * Read the first word: upper byte contains useful
		 * information.
		 */
		status = *sc->nic_fifo;
		if ((status & 0x7000) != 0x2000) {
			log(LOG_INFO, "qn: ERROR: status=%04x\n", status);
			continue;
		}

		/*
		 * Read packet length (byte-swapped).
		 * CRC is stripped off by the NIC.
		 */
		len = *sc->nic_fifo;
		len = ((len << 8) & 0xff00) | ((len >> 8) & 0x00ff);

#ifdef QN_DEBUG
		if (len > ETHER_MAX_LEN || len < ETHER_HDR_SIZE) {
			log(LOG_WARNING,
			    "%s: received a %s packet? (%u bytes)\n",
			    sc->sc_dev.dv_xname,
			    len < ETHER_HDR_SIZE ? "partial" : "big", len);
			++sc->sc_arpcom.ac_if.if_ierrors;
			continue;
		}
#endif
#ifdef QN_DEBUG
		if (len < ETHER_MIN_LEN)
			log(LOG_WARNING,
			    "%s: received a short packet? (%u bytes)\n",
			    sc->sc_dev.dv_xname, len);
#endif 

		/* Read the packet. */
		qn_get_packet(sc, len);

		++sc->sc_arpcom.ac_if.if_ipackets;
	}

#ifdef QN_DEBUG
	/* This print just to see whether MAX_PACKETS is large enough. */
	if (i == MAX_PACKETS)
		log(LOG_INFO, "used all the %d loops\n", MAX_PACKETS);
#endif
}

/*
 * Our interrupt routine
 */
int
qnintr(sc)
	struct qn_softc *sc;
{
	u_short tint, rint, tintmask;
	char return_tintmask = 0;

	/*
	 * If the driver has not been initialized, just return immediately.
	 * This also happens if there is no QuickNet board present.
	 */
	if (sc->sc_base == NULL)
		return (0);

	/* Get interrupt statuses and masks. */
	rint = (*sc->nic_r_status) & NIC_R_MASK;
	tintmask = *sc->nic_t_mask;
	tint = (*sc->nic_t_status) & tintmask;
	if (tint == 0 && rint == 0)
		return (0);

	/* Disable interrupts so that we won't miss anything. */
	*sc->nic_r_mask = CLEAR_R_MASK;
	*sc->nic_t_mask = CLEAR_T_MASK;

	/*
	 * Handle transmitter interrupts. Some of them are not asked for
	 * but do happen, anyway.
	 */

	if (tint != 0) {
		/* Clear transmit interrupt status. */
		*sc->nic_t_status = CLEAR_T_ERR;

		if (sc->transmit_pending && (tint & T_TMT_OK)) {
			sc->transmit_pending = 0;
			/*
			 * Update total number of successfully
			 * transmitted packets.
			 */
			sc->sc_arpcom.ac_if.if_opackets++;
		}

		if (tint & T_SIXTEEN_COL) {
			/*
			 * 16 collision (i.e., packet lost).
			 */
			log(LOG_INFO, "qn: 16 collision - packet lost\n");
#ifdef QN_DEBUG1
			qn_dump(sc);
#endif
			sc->sc_arpcom.ac_if.if_oerrors++;
			sc->sc_arpcom.ac_if.if_collisions += 16;
			sc->transmit_pending = 0;
		}

		if (sc->transmit_pending) {
			log(LOG_INFO, "qn:still pending...\n");

			/* Must return transmission interrupt mask. */
			return_tintmask = 1;
		} else {
			sc->sc_arpcom.ac_if.if_flags &= ~IFF_OACTIVE;

			/* Clear watchdog timer. */
			sc->sc_arpcom.ac_if.if_timer = 0;
		}
	} else
		return_tintmask = 1;

	/*
	 * Handle receiver interrupts.
	 */
	if (rint != 0)
		qn_rint(sc, rint);

	if ((sc->sc_arpcom.ac_if.if_flags & IFF_OACTIVE) == 0)
		qnstart(&sc->sc_arpcom.ac_if);
	else if (return_tintmask == 1)
		*sc->nic_t_mask = tintmask;

	/* Set receive interrupt mask back. */
	*sc->nic_r_mask = NIC_R_MASK;

	return (1);
}

/*
 * Process an ioctl request. This code needs some work - it looks pretty ugly.
 * I somehow think that this is quite a common excuse... ;-)
 */
int
qnioctl(ifp, command, data)
	register struct ifnet *ifp;
	u_long command;
	caddr_t data;
{
	struct qn_softc *sc = qncd.cd_devs[ifp->if_unit];
	register struct ifaddr *ifa = (struct ifaddr *)data;
#if 0
	struct ifreg *ifr = (struct ifreg *)data;
#endif
	int s, error = 0;

	s = splnet();

	switch (command) {

	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;

		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			qnstop(sc);
			qninit(sc);
			arp_ifinit(&sc->sc_arpcom, ifa);
			break;
#endif
#ifdef NS
		case AF_NS:
		    {
			register struct ns_addr *ina = &(IA_SNS(ifa)->sns_addr);

			if (ns_nullhost(*ina))
				ina->x_host =
				    *(union ns_host *)(sc->sc_arpcom.sc_enaddr);
			else
				bcopy(ina->x_host.c_host,
				    sc->sc_arpcom.ac_enaddr,
				    sizeof(sc->sc_arpcom.ac_enaddr));
			qnstop(sc);
			qninit(sc);
			break;
		    }
#endif
		default:
			log(LOG_INFO, "qn:sa_family:default (not tested)\n");
			qnstop(sc);
			qninit(sc);
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
#ifdef QN_DEBUG1
			qn_dump(sc);
#endif
			qnstop(sc);
			ifp->if_flags &= ~IFF_RUNNING;
		} else if ((ifp->if_flags & IFF_UP) != 0 &&
			   (ifp->if_flags & IFF_RUNNING) == 0) {
			/*
			 * If interface is marked up and it is stopped, then
			 * start it.
			 */
			qninit(sc);
		} else {
			/*
			 * Something else... we won't do anything so we won't
			 * break anything (hope so).
			 */
#ifdef QN_DEBUG1
			log(LOG_INFO, "Else branch...\n");
#endif
		}
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		log(LOG_INFO, "qnioctl: multicast not done yet\n");
#if 0
		error = (command == SIOCADDMULTI) ?
		    ether_addmulti(ifr, &sc->sc_arpcom) :
		    ether_delmulti(ifr, &sc->sc_arpcom);

		if (error == ENETRESET) {
			/*
			 * Multicast list has changed; set the hardware filter
			 * accordingly.
			 */
			log(LOG_INFO, "qnioctl: multicast not done yet\n");
			error = 0;
		}
#else
		error = EINVAL;
#endif
		break;

	default:
		log(LOG_INFO, "qnioctl: default\n");
		error = EINVAL;
	}

	splx(s);
	return (error);
}

/*
 * Dump some register information.
 */
#ifdef QN_DEBUG1
static void
qn_dump(sc)
	struct qn_softc *sc;
{

	log(LOG_INFO, "t_status  : %04x\n", *sc->nic_t_status);
	log(LOG_INFO, "t_mask    : %04x\n", *sc->nic_t_mask);
	log(LOG_INFO, "t_mode    : %04x\n", *sc->nic_t_mode);
	log(LOG_INFO, "r_status  : %04x\n", *sc->nic_r_status);
	log(LOG_INFO, "r_mask    : %04x\n", *sc->nic_r_mask);
	log(LOG_INFO, "r_mode    : %04x\n", *sc->nic_r_mode);
	log(LOG_INFO, "pending   : %02x\n", sc->transmit_pending);
	log(LOG_INFO, "if_flags  : %04x\n", sc->sc_arpcom.ac_if.if_flags);
}
#endif

#endif /* NQN > 0 */
