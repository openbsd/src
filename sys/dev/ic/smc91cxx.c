/*	$OpenBSD: smc91cxx.c,v 1.18 2004/05/12 06:35:10 tedu Exp $	*/
/*	$NetBSD: smc91cxx.c,v 1.11 1998/08/08 23:51:41 mycroft Exp $	*/

/*-
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*      
 * Copyright (c) 1996 Gardner Buchanan <gbuchanan@shl.com>
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
 *	This product includes software developed by Gardner Buchanan.
 * 4. The name of Gardner Buchanan may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
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
 *   from FreeBSD Id: if_sn.c,v 1.4 1996/03/18 15:47:16 gardner Exp
 */      

/*
 * Core driver for the SMC 91Cxx family of Ethernet chips.
 *
 * Memory allocation interrupt logic is drived from an SMC 91C90 driver
 * written for NetBSD/amiga by Michael Hitch.
 */

#include "bpfilter.h"

#include <sys/param.h> 
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/syslog.h>
#include <sys/socket.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/ioctl.h> 
#include <sys/errno.h>
#if NRND > 0
#include <sys/rnd.h>
#endif

#include <machine/bus.h>
#include <machine/intr.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h> 

#ifdef INET
#include <netinet/in.h> 
#include <netinet/if_ether.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#endif

#ifdef NS
#include <netns/ns.h>
#include <netns/ns_if.h>
#endif

#if defined(CCITT) && defined(LLC)
#include <sys/socketvar.h>
#include <netccitt/x25.h>
#include <netccitt/pk.h>
#include <netccitt/pk_var.h>
#include <netccitt/pk_extern.h>
#endif

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#include <dev/ic/smc91cxxreg.h>
#include <dev/ic/smc91cxxvar.h>

/* XXX Hardware padding doesn't work yet(?) */
#define	SMC91CXX_SW_PAD

const char *smc91cxx_idstrs[] = {
	NULL,				/* 0 */
	NULL,				/* 1 */
	NULL,				/* 2 */
	"SMC91C90/91C92",		/* 3 */
	"SMC91C94",			/* 4 */
	"SMC91C95",			/* 5 */
	NULL,				/* 6 */
	"SMC91C100",			/* 7 */
	NULL,				/* 8 */
	NULL,				/* 9 */
	NULL,				/* 10 */
	NULL,				/* 11 */
	NULL,				/* 12 */
	NULL,				/* 13 */
	NULL,				/* 14 */
	NULL,				/* 15 */
};

/* Supported media types. */
const int smc91cxx_media[] = {
	IFM_ETHER|IFM_10_T,
	IFM_ETHER|IFM_10_5,
};
#define	NSMC91CxxMEDIA	(sizeof(smc91cxx_media) / sizeof(smc91cxx_media[0]))

struct cfdriver sm_cd = {
	NULL, "sm", DV_IFNET
};

int	smc91cxx_mediachange(struct ifnet *);
void	smc91cxx_mediastatus(struct ifnet *, struct ifmediareq *);

int	smc91cxx_set_media(struct smc91cxx_softc *, int);

void	smc91cxx_read(struct smc91cxx_softc *);
void	smc91cxx_reset(struct smc91cxx_softc *);
void	smc91cxx_start(struct ifnet *);
void	smc91cxx_resume(struct smc91cxx_softc *);
void	smc91cxx_watchdog(struct ifnet *);
int	smc91cxx_ioctl(struct ifnet *, u_long, caddr_t);

int	smc91cxx_enable(struct smc91cxx_softc *);
void	smc91cxx_disable(struct smc91cxx_softc *);

static __inline int ether_cmp(void *, void *);
static __inline int
ether_cmp(va, vb)
	void *va, *vb;
{
	u_int8_t *a = va;
	u_int8_t *b = vb;

	return ((a[5] != b[5]) || (a[4] != b[4]) || (a[3] != b[3]) ||
		(a[2] != b[2]) || (a[1] != b[1]) || (a[0] != b[0]));
}

void
smc91cxx_attach(sc, myea)
	struct smc91cxx_softc *sc;
	u_int8_t *myea;
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	bus_space_tag_t bst = sc->sc_bst;
	bus_space_handle_t bsh = sc->sc_bsh;
	u_int16_t tmp;
	int i, aui;
#ifdef SMC_DEBUG
	const char *idstr;
#endif

	/* Make sure the chip is stopped. */
	smc91cxx_stop(sc);

#ifdef SMC_DEBUG
	SMC_SELECT_BANK(sc, 3);
	tmp = bus_space_read_2(bst, bsh, REVISION_REG_W);
	idstr = smc91cxx_idstrs[RR_ID(tmp)];
	printf("%s: ", sc->sc_dev.dv_xname);
	if (idstr != NULL)
		printf("%s, ", idstr);
	else
		printf("unknown chip id %d, ", RR_ID(tmp));
	printf("revision %d\n", RR_REV(tmp));
#endif

	/* Read the station address from the chip. */
	SMC_SELECT_BANK(sc, 1);
	if (myea == NULL) {
		for (i = 0; i < ETHER_ADDR_LEN; i += 2) {
			tmp = bus_space_read_2(bst, bsh, IAR_ADDR0_REG_W + i);
			sc->sc_arpcom.ac_enaddr[i + 1] = (tmp >>8) & 0xff;
			sc->sc_arpcom.ac_enaddr[i] = tmp & 0xff;
		}
	} else {
		bcopy(myea, sc->sc_arpcom.ac_enaddr, ETHER_ADDR_LEN);
	}

	printf(": address %s, ",
	    ether_sprintf(sc->sc_arpcom.ac_enaddr));

	/* ..and default media. */
	tmp = bus_space_read_2(bst, bsh, CONFIG_REG_W);
	printf("utp/aui (default %s)\n", (aui = (tmp & CR_AUI_SELECT)) ?
	    "aui" : "utp");

	/* Initialize the ifnet structure. */
	bcopy(sc->sc_dev.dv_xname, ifp->if_xname, IFNAMSIZ);
	ifp->if_softc = sc;
	ifp->if_start = smc91cxx_start;
	ifp->if_ioctl = smc91cxx_ioctl;
	ifp->if_watchdog = smc91cxx_watchdog;
	ifp->if_flags =
	    IFF_BROADCAST | IFF_SIMPLEX | IFF_NOTRAILERS | IFF_MULTICAST;
	IFQ_SET_READY(&ifp->if_snd);

	/* Attach the interface. */
	if_attach(ifp);
	ether_ifattach(ifp);

	/* Initialize the media structures. */
	ifmedia_init(&sc->sc_media, 0, smc91cxx_mediachange,
	    smc91cxx_mediastatus);
	for (i = 0; i < NSMC91CxxMEDIA; i++)
		ifmedia_add(&sc->sc_media, smc91cxx_media[i], 0, NULL);
	ifmedia_set(&sc->sc_media, IFM_ETHER | (aui ? IFM_10_5 : IFM_10_T));

#if NRND > 0
	rnd_attach_source(&sc->rnd_source, sc->sc_dev.dv_xname, RND_TYPE_NET);
#endif
}

/*
 * Change media according to request.
 */
int
smc91cxx_mediachange(ifp)
	struct ifnet *ifp;
{
	struct smc91cxx_softc *sc = ifp->if_softc;

	return (smc91cxx_set_media(sc, sc->sc_media.ifm_media));
}

int
smc91cxx_set_media(sc, media)
	struct smc91cxx_softc *sc;
	int media;
{
	bus_space_tag_t bst = sc->sc_bst;
	bus_space_handle_t bsh = sc->sc_bsh;
	u_int16_t tmp;

	/*
	 * If the interface is not currently powered on, just return.
	 * When it is enabled later, smc91cxx_init() will properly set
	 * up the media for us.
	 */
	if (sc->sc_enabled == 0)
		return (0);

	if (IFM_TYPE(media) != IFM_ETHER)
		return (EINVAL);

	switch (IFM_SUBTYPE(media)) {
	case IFM_10_T:
	case IFM_10_5:
		SMC_SELECT_BANK(sc, 1);
		tmp = bus_space_read_2(bst, bsh, CONFIG_REG_W);
		if (IFM_SUBTYPE(media) == IFM_10_5)
			tmp |= CR_AUI_SELECT;
		else
			tmp &= ~CR_AUI_SELECT;
		bus_space_write_2(bst, bsh, CONFIG_REG_W, tmp);
		delay(20000);	/* XXX is this needed? */
		break;

	default:
		return (EINVAL);
	}

	return (0);
}

/*
 * Notify the world which media we're using.
 */
void
smc91cxx_mediastatus(ifp, ifmr)
	struct ifnet *ifp;
	struct ifmediareq *ifmr;
{
	struct smc91cxx_softc *sc = ifp->if_softc;
	bus_space_tag_t bst = sc->sc_bst;
	bus_space_handle_t bsh = sc->sc_bsh;
	u_int16_t tmp;

	if (sc->sc_enabled == 0) {
		ifmr->ifm_active = IFM_ETHER | IFM_NONE;
		ifmr->ifm_status = 0;
		return;
	}

	SMC_SELECT_BANK(sc, 1);
	tmp = bus_space_read_2(bst, bsh, CONFIG_REG_W);
	ifmr->ifm_active =
	    IFM_ETHER | ((tmp & CR_AUI_SELECT) ? IFM_10_5 : IFM_10_T);
}

/*
 * Reset and initialize the chip.
 */
void
smc91cxx_init(sc)
	struct smc91cxx_softc *sc;
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	bus_space_tag_t bst = sc->sc_bst;
	bus_space_handle_t bsh = sc->sc_bsh;
	u_int16_t tmp;
	int s, i;

	s = splnet();

	/*
	 * This resets the registers mostly to defaults, but doesn't
	 * affect the EEPROM.  After the reset cycle, we pause briefly
	 * for the chip to recover.
	 *
	 * XXX how long are we really supposed to delay?  --thorpej
	 */
	SMC_SELECT_BANK(sc, 0);
	bus_space_write_2(bst, bsh, RECV_CONTROL_REG_W, RCR_SOFTRESET);
	delay(100);
	bus_space_write_2(bst, bsh, RECV_CONTROL_REG_W, 0);
	delay(200);

	bus_space_write_2(bst, bsh, TXMIT_CONTROL_REG_W, 0);

	/* Set the Ethernet address. */
	SMC_SELECT_BANK(sc, 1);
	for (i = 0; i < ETHER_ADDR_LEN; i++ )
		bus_space_write_1(bst, bsh, IAR_ADDR0_REG_W + i,
		    sc->sc_arpcom.ac_enaddr[i]);

	/*
	 * Set the control register to automatically release successfully
	 * transmitted packets (making the best use of our limited memory)
	 * and enable the EPH interrupt on certain TX errors.
	 */
	bus_space_write_2(bst, bsh, CONTROL_REG_W, (CTR_AUTO_RELEASE |
	    CTR_TE_ENABLE | CTR_CR_ENABLE | CTR_LE_ENABLE));

	/*
	 * Reset the MMU and wait for it to be un-busy.
	 */
	SMC_SELECT_BANK(sc, 2);
	bus_space_write_2(bst, bsh, MMU_CMD_REG_W, MMUCR_RESET);
	while (bus_space_read_2(bst, bsh, MMU_CMD_REG_W) & MMUCR_BUSY)
		/* XXX bound this loop! */ ;

	/*
	 * Disable all interrupts.
	 */
	bus_space_write_1(bst, bsh, INTR_MASK_REG_B, 0);

	/*
	 * Set current media.
	 */
	smc91cxx_set_media(sc, sc->sc_media.ifm_cur->ifm_media);

	/*
	 * Set the receive filter.  We want receive enable and auto
	 * strip of CRC from received packet.  If we are in promisc. mode,
	 * then set that bit as well.
	 *
	 * XXX Initialize multicast filter.  For now, we just accept
	 * XXX all multicast.
	 */
	SMC_SELECT_BANK(sc, 0);

	tmp = RCR_ENABLE | RCR_STRIP_CRC | RCR_ALMUL;
	if (ifp->if_flags & IFF_PROMISC)
		tmp |= RCR_PROMISC;

	bus_space_write_2(bst, bsh, RECV_CONTROL_REG_W, tmp);

	/*
	 * Set transmitter control to "enabled".
	 */
	tmp = TCR_ENABLE;

#ifndef SMC91CXX_SW_PAD
	/*
	 * Enable hardware padding of transmitted packets.
	 * XXX doesn't work?
	 */
	tmp |= TCR_PAD_ENABLE;
#endif

	bus_space_write_2(bst, bsh, TXMIT_CONTROL_REG_W, tmp);

	/*
	 * Now, enable interrupts.
	 */
	SMC_SELECT_BANK(sc, 2);

	bus_space_write_1(bst, bsh, INTR_MASK_REG_B,
	    IM_EPH_INT | IM_RX_OVRN_INT | IM_RCV_INT | IM_TX_INT);

	/* Interface is now running, with no output active. */
	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	/*
	 * Attempt to start any pending transmission.
	 */
	smc91cxx_start(ifp);

	splx(s);
}

/*
 * Start output on an interface.
 * Must be called at splnet or interrupt level.
 */
void
smc91cxx_start(ifp)
	struct ifnet *ifp;
{
	struct smc91cxx_softc *sc = ifp->if_softc;
	bus_space_tag_t bst = sc->sc_bst;
	bus_space_handle_t bsh = sc->sc_bsh;
	u_int len;
	struct mbuf *m, *top;
	u_int16_t length, npages;
	u_int8_t packetno;
	int timo, pad;

	if ((ifp->if_flags & (IFF_RUNNING|IFF_OACTIVE)) != IFF_RUNNING)
		return;

 again:
	/*
	 * Peek at the next packet.
	 */
	IFQ_POLL(&ifp->if_snd, m);
	if (m == NULL)
		return;

	/*
	 * Compute the frame length and set pad to give an overall even
	 * number of bytes.  Below, we assume that the packet length
	 * is even.
	 */
	for (len = 0, top = m; m != NULL; m = m->m_next)
		len += m->m_len;
	pad = (len & 1);

	/*
	 * We drop packets that are too large.  Perhaps we should
	 * truncate them instead?
	 */
	if ((len + pad) > (ETHER_MAX_LEN - ETHER_CRC_LEN)) {
		printf("%s: large packet discarded\n", sc->sc_dev.dv_xname);
		ifp->if_oerrors++;
		IFQ_DEQUEUE(&ifp->if_snd, m);
		m_freem(m);
		goto readcheck;
	}

#ifdef SMC91CXX_SW_PAD
	/*
	 * Not using hardware padding; pad to ETHER_MIN_LEN.
	 */
	if (len < (ETHER_MIN_LEN - ETHER_CRC_LEN))
		pad = ETHER_MIN_LEN - ETHER_CRC_LEN - len;
#endif

	length = pad + len;

	/*
	 * The MMU has a 256 byte page size.  The MMU expects us to
	 * ask for "npages - 1".  We include space for the status word,
	 * byte count, and control bytes in the allocation request.
	 */
	npages = (length + 6) >> 8;

	/*
	 * Now allocate the memory.
	 */
	SMC_SELECT_BANK(sc, 2);
	bus_space_write_2(bst, bsh, MMU_CMD_REG_W, MMUCR_ALLOC | npages);

	timo = MEMORY_WAIT_TIME;
	do {
		if (bus_space_read_1(bst, bsh, INTR_STAT_REG_B) & IM_ALLOC_INT)
			break;
		delay(1);
	} while (--timo);

	packetno = bus_space_read_1(bst, bsh, ALLOC_RESULT_REG_B);

	if (packetno & ARR_FAILED || timo == 0) {
		/*
		 * No transmit memory is available.  Record the number
		 * of requestd pages and enable the allocation completion
		 * interrupt.  Set up the watchdog timer in case we miss
		 * the interrupt.  Mark the interface as active so that
		 * no one else attempts to transmit while we're allocating
		 * memory.
		 */
		bus_space_write_1(bst, bsh, INTR_MASK_REG_B,
		    bus_space_read_1(bst, bsh, INTR_MASK_REG_B) | IM_ALLOC_INT);

		ifp->if_timer = 5;
		ifp->if_flags |= IFF_OACTIVE;

		return;
	}

	/*
	 * We have a packet number - set the data window.
	 */
	bus_space_write_1(bst, bsh, PACKET_NUM_REG_B, packetno);

	/*
	 * Point to the beginning of the packet.
	 */
	bus_space_write_2(bst, bsh, POINTER_REG_W, PTR_AUTOINC /* | 0x0000 */);

	/*
	 * Send the packet length (+6 for stats, length, and control bytes)
	 * and the status word (set to zeros).
	 */
	bus_space_write_2(bst, bsh, DATA_REG_W, 0);
	bus_space_write_1(bst, bsh, DATA_REG_B, (length + 6) & 0xff);
	bus_space_write_1(bst, bsh, DATA_REG_B, ((length + 6) >> 8) & 0xff);

	/*
	 * Get the packet from the kernel.  This will include the Ethernet
	 * frame header, MAC address, etc.
	 */
	IFQ_DEQUEUE(&ifp->if_snd, m);

	/*
	 * Push the packet out to the card.
	 */
	for (top = m; m != NULL; m = m->m_next) {
		/* Words... */
		bus_space_write_multi_2(bst, bsh, DATA_REG_W,
		    mtod(m, u_int16_t *), m->m_len >> 1);

		/* ...and the remaining byte, if any. */
		if (m->m_len & 1)
			bus_space_write_1(bst, bsh, DATA_REG_B,
			  *(u_int8_t *)(mtod(m, u_int8_t *) + (m->m_len - 1)));
	}

#ifdef SMC91CXX_SW_PAD
	/*
	 * Push out padding.
	 */
	while (pad > 1) {
		bus_space_write_2(bst, bsh, DATA_REG_W, 0);
		pad -= 2;
	}
	if (pad)
		bus_space_write_1(bst, bsh, DATA_REG_B, 0);
#endif

	/*
	 * Push out control byte and unused packet byte.  The control byte
	 * is 0, meaning the packet is even lengthed and no special
	 * CRC handling is necessary.
	 */
	bus_space_write_2(bst, bsh, DATA_REG_W, 0);

	/*
	 * Enable transmit interrupts and let the chip go.  Set a watchdog
	 * in case we miss the interrupt.
	 */
	bus_space_write_1(bst, bsh, INTR_MASK_REG_B,
	    bus_space_read_1(bst, bsh, INTR_MASK_REG_B) |
	    IM_TX_INT | IM_TX_EMPTY_INT);

	bus_space_write_2(bst, bsh, MMU_CMD_REG_W, MMUCR_ENQUEUE);

	ifp->if_timer = 5;

#if NBPFILTER > 0
	/* Hand off a copy to the bpf. */
	if (ifp->if_bpf)
		bpf_mtap(ifp->if_bpf, top);
#endif

	ifp->if_opackets++;
	m_freem(top);

 readcheck:
	/*
	 * Check for incoming pcakets.  We don't want to overflow the small
	 * RX FIFO.  If nothing has arrived, attempt to queue another
	 * transmit packet.
	 */
	if (bus_space_read_2(bst, bsh, FIFO_PORTS_REG_W) & FIFO_REMPTY)
		goto again;
}

/*
 * Interrupt service routine.
 */
int
smc91cxx_intr(arg)
	void *arg;
{
	struct smc91cxx_softc *sc = arg;
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	bus_space_tag_t bst = sc->sc_bst;
	bus_space_handle_t bsh = sc->sc_bsh;
	u_int8_t mask, interrupts, status;
	u_int16_t packetno, tx_status, card_stats;

	if (sc->sc_enabled == 0)
		return (0);

	SMC_SELECT_BANK(sc, 2);

	/*
	 * Obtain the current interrupt mask.
	 */
	mask = bus_space_read_1(bst, bsh, INTR_MASK_REG_B);

	/*
	 * Get the set of interrupt which occurred and eliminate any
	 * which are not enabled.
	 */
	interrupts = bus_space_read_1(bst, bsh, INTR_STAT_REG_B);
	status = interrupts & mask;

	/* Ours? */
	if (status == 0)
		return (0);

	/*
	 * It's ours; disable all interrupts while we process them.
	 */
	bus_space_write_1(bst, bsh, INTR_MASK_REG_B, 0);

	/*
	 * Receive overrun interrupts.
	 */
	if (status & IM_RX_OVRN_INT) {
		bus_space_write_1(bst, bsh, INTR_ACK_REG_B, IM_RX_OVRN_INT);
		ifp->if_ierrors++;
	}

	/*
	 * Receive interrupts.
	 */
	if (status & IM_RCV_INT) {
#if 1 /* DIAGNOSTIC */
		packetno = bus_space_read_2(bst, bsh, FIFO_PORTS_REG_W);
		if (packetno & FIFO_REMPTY)
			printf("%s: receive interrupt on empty fifo\n",
			    sc->sc_dev.dv_xname);
		else
#endif
		smc91cxx_read(sc);
	}

	/*
	 * Memory allocation interrupts.
	 */
	if (status & IM_ALLOC_INT) {
		/* Disable this interrupt. */
		mask &= ~IM_ALLOC_INT;

		/*
		 * Release the just-allocated memory.  We will reallocate
		 * it through the normal start logic.
		 */
		while (bus_space_read_2(bst, bsh, MMU_CMD_REG_W) & MMUCR_BUSY)
			/* XXX bound this loop! */ ;
		bus_space_write_2(bst, bsh, MMU_CMD_REG_W, MMUCR_FREEPKT);

		ifp->if_flags &= ~IFF_OACTIVE;
		ifp->if_timer = 0;
	}

	/*
	 * Transmit complete interrupt.  Handle transmission error messages.
	 * This will only be called on error condition because of AUTO RELEASE
	 * mode.
	 */
	if (status & IM_TX_INT) {
		bus_space_write_1(bst, bsh, INTR_ACK_REG_B, IM_TX_INT);

		packetno = bus_space_read_2(bst, bsh, FIFO_PORTS_REG_W) &
		    FIFO_TX_MASK;

		/*
		 * Select this as the packet to read from.
		 */
		bus_space_write_1(bst, bsh, PACKET_NUM_REG_B, packetno);

		/*
		 * Position the pointer to the beginning of the packet.
		 */
		bus_space_write_2(bst, bsh, POINTER_REG_W,
		    PTR_AUTOINC | PTR_READ /* | 0x0000 */);

		/*
		 * Fetch the TX status word.  This will be a copy of
		 * the EPH_STATUS_REG_W at the time of the transmission
		 * failure.
		 */
		tx_status = bus_space_read_2(bst, bsh, DATA_REG_W);

		if (tx_status & EPHSR_TX_SUC)
			printf("%s: successful packet caused TX interrupt?!\n",
			    sc->sc_dev.dv_xname);
		else
			ifp->if_oerrors++;

		if (tx_status & EPHSR_LATCOL)
			ifp->if_collisions++;

		/*
		 * Some of these errors disable the transmitter; reenable it.
		 */
		SMC_SELECT_BANK(sc, 0);
#ifdef SMC91CXX_SW_PAD
		bus_space_write_2(bst, bsh, TXMIT_CONTROL_REG_W, TCR_ENABLE);
#else
		bus_space_write_2(bst, bsh, TXMIT_CONTROL_REG_W,
		    TCR_ENABLE | TCR_PAD_ENABLE);
#endif

		/*
		 * Kill the failed packet and wait for the MMU to unbusy.
		 */
		SMC_SELECT_BANK(sc, 2);
		while (bus_space_read_2(bst, bsh, MMU_CMD_REG_W) & MMUCR_BUSY)
			/* XXX bound this loop! */ ;
		bus_space_write_2(bst, bsh, MMU_CMD_REG_W, MMUCR_FREEPKT);

		ifp->if_timer = 0;
	}

	/*
	 * Transmit underrun interrupts.  We use this opportunity to
	 * update transmit statistics from the card.
	 */
	if (status & IM_TX_EMPTY_INT) {
		bus_space_write_1(bst, bsh, INTR_ACK_REG_B, IM_TX_EMPTY_INT);

		/* Disable this interrupt. */
		mask &= ~IM_TX_EMPTY_INT;

		SMC_SELECT_BANK(sc, 0);
		card_stats = bus_space_read_2(bst, bsh, COUNTER_REG_W);

		/* Single collisions. */
		ifp->if_collisions += card_stats & ECR_COLN_MASK;

		/* Multiple collisions. */
		ifp->if_collisions += (card_stats & ECR_MCOLN_MASK) >> 4;

		SMC_SELECT_BANK(sc, 2);

		ifp->if_timer = 0;
	}

	/*
	 * Other errors.  Reset the interface.
	 */
	if (status & IM_EPH_INT) {
		smc91cxx_stop(sc);
		smc91cxx_init(sc);
	}

	/*
	 * Attempt to queue more packets for transmission.
	 */
	smc91cxx_start(ifp);

	/*
	 * Reenable the interrupts we wish to receive now that processing
	 * is complete.
	 */
	mask |= bus_space_read_1(bst, bsh, INTR_MASK_REG_B);
	bus_space_write_1(bst, bsh, INTR_MASK_REG_B, mask);

#if NRND > 0
	if (status)
		rnd_add_uint32(&sc->rnd_source, status);
#endif

	return (1);
}

/*
 * Read a packet from the card and pass it up to the kernel.
 * NOTE!  WE EXPECT TO BE IN REGISTER WINDOW 2!
 */
void
smc91cxx_read(sc)
	struct smc91cxx_softc *sc;
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	bus_space_tag_t bst = sc->sc_bst;
	bus_space_handle_t bsh = sc->sc_bsh;
	struct mbuf *m;
	u_int16_t status, packetno, packetlen;
	u_int8_t *data;

 again:
	/*
	 * Set data pointer to the beginning of the packet.  Since
	 * PTR_RCV is set, the packet number will be found automatically
	 * in FIFO_PORTS_REG_W, FIFO_RX_MASK.
	 */
	bus_space_write_2(bst, bsh, POINTER_REG_W,
	    PTR_READ | PTR_RCV | PTR_AUTOINC /* | 0x0000 */);

	/*
	 * First two words are status and packet length.
	 */
	status = bus_space_read_2(bst, bsh, DATA_REG_W);
	packetlen = bus_space_read_2(bst, bsh, DATA_REG_W);

	/*
	 * The packet length includes 3 extra words: status, length,
	 * and an extra word that includes the control byte.
	 */
	packetlen -= 6;

	/*
	 * Account for receive errors and discard.
	 */
	if (status & RS_ERRORS) {
		ifp->if_ierrors++;
		goto out;
	}

	/*
	 * Adjust for odd-length packet.
	 */
	if (status & RS_ODDFRAME)
		packetlen++;

	/*
	 * Allocate a header mbuf.
	 */
	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		goto out;

	m->m_pkthdr.rcvif = ifp;
	m->m_pkthdr.len = m->m_len = packetlen;

	/*
	 * Always put the packet in a cluster.
	 * XXX should chain small mbufs if less than threshold.
	 */
	MCLGET(m, M_DONTWAIT);
	if ((m->m_flags & M_EXT) == 0) {
		m_freem(m);
		ifp->if_ierrors++;
		printf("%s: can't allocate cluster for incoming packet\n",
		    sc->sc_dev.dv_xname);
		goto out;
	}

	/*
	 * Pull the packet off the interface.
	 */
	data = mtod(m, u_int8_t *);
	bus_space_read_multi_2(bst, bsh, DATA_REG_W, (u_int16_t *)data,
	    packetlen >> 1);
	if (packetlen & 1) {
		data += packetlen & ~1;
		*data = bus_space_read_1(bst, bsh, DATA_REG_B);
	}

	ifp->if_ipackets++;

#if NBPFILTER > 0
	/*
	 * Hand the packet off to bpf listeners.  If there's a bpf listener,
	 * we need to check if the packet is ours.
	 */
	if (ifp->if_bpf)
		bpf_mtap(ifp->if_bpf, m);
#endif

	m->m_pkthdr.len = m->m_len = packetlen;
	ether_input_mbuf(ifp, m);

 out:
	/*
	 * Tell the card to free the memory occupied by this packet.
	 */
	while (bus_space_read_2(bst, bsh, MMU_CMD_REG_W) & MMUCR_BUSY)
		/* XXX bound this loop! */ ;
	bus_space_write_2(bst, bsh, MMU_CMD_REG_W, MMUCR_RELEASE);

	/*
	 * Check for another packet.
	 */
	packetno = bus_space_read_2(bst, bsh, FIFO_PORTS_REG_W);
	if (packetno & FIFO_REMPTY)
		return;
	goto again;
}

/*
 * Process an ioctl request.
 */
int
smc91cxx_ioctl(ifp, cmd, data)
	struct ifnet *ifp;
	u_long cmd;
	caddr_t data;
{
	struct smc91cxx_softc *sc = ifp->if_softc;
	struct ifaddr *ifa = (struct ifaddr *)data;
	struct ifreq *ifr = (struct ifreq *)data;
	int s, error = 0;

	s = splnet();

	switch (cmd) {
	case SIOCSIFADDR:
		if ((error = smc91cxx_enable(sc)) != 0)
			break;
		ifp->if_flags |= IFF_UP;
		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			smc91cxx_init(sc);
			arp_ifinit(&sc->sc_arpcom, ifa);
			break;
#endif
#ifdef NS
		case AF_NS:
		    {
			struct ns_addr *ina = &IA_SNS(ifa)->sns_addr;

			if (ns_nullhost(*ina))
				ina->x_host =
				    *(union ns_host *)LLADDR(ifp->if_sadl);
			else {
				bcopy(ina->x_host.c_host, LLADDR(ifp->if_sadl), 
				    ETHER_ADDR_LEN);
			}

			/*
			 * Set new address.  Reset, because the receiver
			 * has to be stopped before we can set the new
			 * MAC address.
			 */
			smc91cxx_reset(sc);
			break;
		    }
#endif
		default:
			smc91cxx_init(sc);
			break;
		}
		break;

#if defined(CCITT) && defined(LLC)
	case SIOCSIFCONF_X25:
		if ((error = smc91cxx_enable(sc)) != 0)
			break;
		ifp->if_flags |= IFF_UP;
		ifa->ifa_rtrequest = cons_rtrequest;	/* XXX */
		error = x25_llcglue(PRC_IFUP, ifa->ifa_addr);
		if (error == 0)
			smc91cxx_init(sc);
		break;
#endif

	case SIOCSIFFLAGS:
		if ((ifp->if_flags & IFF_UP) == 0 &&
		    (ifp->if_flags & IFF_RUNNING) != 0) {
			/*
			 * If interface is marked down and it is running,
			 * stop it.
			 */
			smc91cxx_stop(sc);
			ifp->if_flags &= ~IFF_RUNNING;
			smc91cxx_disable(sc);
		} else if ((ifp->if_flags & IFF_UP) != 0 &&
			   (ifp->if_flags & IFF_RUNNING) == 0) {
			/*
			 * If interface is marked up and it is stopped,
			 * start it.
			 */
			if ((error = smc91cxx_enable(sc)) != 0)
				break;
			smc91cxx_init(sc);
		} else if (sc->sc_enabled) {
			/*
			 * Reset the interface to pick up changes in any
			 * other flags that affect hardware registers.
			 */
			smc91cxx_reset(sc);
		}
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		if (sc->sc_enabled == 0) {
			error = EIO;
			break;
		}

		error = (cmd == SIOCADDMULTI) ?
		    ether_addmulti(ifr, &sc->sc_arpcom) :
		    ether_delmulti(ifr, &sc->sc_arpcom);
		if (error == ENETRESET) {
			/*
			 * Multicast list has changed; set the hardware
			 * filter accordingly.
			 */
			smc91cxx_reset(sc);
			error = 0;
		}
		break;

	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->sc_media, cmd);
		break;

	default:
		error = EINVAL;
		break;
	}

	splx(s);
	return (error);
}

/*
 * Reset the interface.
 */
void
smc91cxx_reset(sc)
	struct smc91cxx_softc *sc;
{
	int s;

	s = splnet();
	smc91cxx_stop(sc);
	smc91cxx_init(sc);
	splx(s);
}

/*
 * Watchdog timer.
 */
void
smc91cxx_watchdog(ifp)
	struct ifnet *ifp;
{
	struct smc91cxx_softc *sc = ifp->if_softc;

	log(LOG_ERR, "%s: device timeout\n", sc->sc_dev.dv_xname);
	++sc->sc_arpcom.ac_if.if_oerrors;

	smc91cxx_reset(sc);
}

/*
 * Stop output on the interface.
 */
void
smc91cxx_stop(sc)
	struct smc91cxx_softc *sc;
{
	bus_space_tag_t bst = sc->sc_bst;
	bus_space_handle_t bsh = sc->sc_bsh;

	/*
	 * Clear interrupt mask; disable all interrupts.
	 */
	SMC_SELECT_BANK(sc, 2);
	bus_space_write_1(bst, bsh, INTR_MASK_REG_B, 0);

	/*
	 * Disable transmitter and receiver.
	 */
	SMC_SELECT_BANK(sc, 0);
	bus_space_write_2(bst, bsh, RECV_CONTROL_REG_W, 0);
	bus_space_write_2(bst, bsh, TXMIT_CONTROL_REG_W, 0);

	/*
	 * Cancel watchdog timer.
	 */
	sc->sc_arpcom.ac_if.if_timer = 0;
}

/*
 * Enable power on the interface.
 */
int
smc91cxx_enable(sc)
	struct smc91cxx_softc *sc;
{

	if (sc->sc_enabled == 0 && sc->sc_enable != NULL) {
		if ((*sc->sc_enable)(sc) != 0) {
			printf("%s: device enable failed\n",
			    sc->sc_dev.dv_xname);
			return (EIO);
		}
	}

	sc->sc_enabled = 1;
	return (0);
}

/*
 * Disable power on the interface.
 */
void
smc91cxx_disable(sc)
	struct smc91cxx_softc *sc;
{

	if (sc->sc_enabled != 0 && sc->sc_disable != NULL) {
		(*sc->sc_disable)(sc);
		sc->sc_enabled = 0;
	}
}
