/* $NetBSD: awi.c,v 1.8 1999/11/09 14:58:07 sommerfeld Exp $ */
/* $OpenBSD: awi.c,v 1.1 1999/12/16 02:56:56 deraadt Exp $ */

/*-
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Bill Sommerfeld
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
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
 * Driver for AMD 802.11 firmware.
 * Uses am79c930 chip driver to talk to firmware running on the am79c930.
 *
 * More-or-less a generic ethernet-like if driver, with 802.11 gorp added.
 */

/*
 * todo:
 *	- flush tx queue on resynch.
 *	- clear oactive on "down".
 *	- rewrite copy-into-mbuf code
 *	- mgmt state machine gets stuck retransmitting assoc requests.
 *	- multicast filter.
 *	- fix device reset so it's more likely to work
 *	- show status goo through ifmedia.
 *
 * more todo:
 *	- deal with more 802.11 frames.
 *		- send reassoc request
 *		- deal with reassoc response
 *		- send/deal with disassociation
 *	- deal with "full" access points (no room for me).
 *	- power save mode
 *
 * later:
 *	- SSID preferences
 *	- need ioctls for poking at the MIBs
 *	- implement ad-hoc mode (including bss creation).
 *	- decide when to do "ad hoc" vs. infrastructure mode (IFF_LINK flags?)
 *		(focus on inf. mode since that will be needed for ietf)
 *	- deal with DH vs. FH versions of the card
 *	- deal with faster cards (2mb/s)
 *	- ?WEP goo (mmm, rc4) (it looks not particularly useful).
 *	- ifmedia revision.
 *	- common 802.11 mibish things.
 *	- common 802.11 media layer.
 */

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/syslog.h>
#include <sys/select.h>
#include <sys/device.h>
#if NRND > 0
#include <sys/rnd.h>
#endif

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>

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

#if NBPFILTER > 0
#include <net/bpf.h>
#include <net/bpfdesc.h>
#endif

#include <machine/cpu.h>
#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/ic/am79c930reg.h>
#include <dev/ic/am79c930var.h>
#include <dev/ic/awireg.h>
#include <dev/ic/awivar.h>

#ifndef ETHER_CRC_LEN
#define ETHER_CRC_LEN 4
#endif

void awi_insane __P((struct awi_softc *sc));
int awi_intlock __P((struct awi_softc *sc));
void awi_intunlock __P((struct awi_softc *sc));
void awi_intrinit __P((struct awi_softc *sc));
u_int8_t awi_read_intst __P((struct awi_softc *sc));
void awi_stop __P((struct awi_softc *sc));
void awi_flush __P((struct awi_softc *sc));
void awi_init __P((struct awi_softc *sc));
void awi_set_mc __P((struct awi_softc *sc));
void awi_rxint __P((struct awi_softc *));
void awi_txint __P((struct awi_softc *));
void awi_tx_packet __P((struct awi_softc *, int, struct mbuf *));

void awi_rcv __P((struct awi_softc *, struct mbuf *, u_int32_t, u_int8_t));
void awi_rcv_mgt __P((struct awi_softc *, struct mbuf *, u_int32_t, u_int8_t));
void awi_rcv_data __P((struct awi_softc *, struct mbuf *));
void awi_rcv_ctl __P((struct awi_softc *, struct mbuf *));

int awi_enable __P((struct awi_softc *sc));
void awi_disable __P((struct awi_softc *sc));

void awi_zero __P((struct awi_softc *, u_int32_t, u_int32_t));

void awi_cmd __P((struct awi_softc *, u_int8_t));
void awi_cmd_test_if __P((struct awi_softc *));
void awi_cmd_get_mib __P((struct awi_softc *sc, u_int8_t, u_int8_t, u_int8_t));
void awi_cmd_txinit __P((struct awi_softc *sc));
void awi_cmd_scan __P((struct awi_softc *sc));
void awi_scan_next __P((struct awi_softc *sc));
void awi_try_sync __P((struct awi_softc *sc));
void awi_cmd_set_ss __P((struct awi_softc *sc));
void awi_cmd_set_promisc __P((struct awi_softc *sc));
void awi_cmd_set_allmulti __P((struct awi_softc *sc));
void awi_cmd_set_infra __P((struct awi_softc *sc));
void awi_cmd_set_notap __P((struct awi_softc *sc));
void awi_cmd_get_myaddr __P((struct awi_softc *sc));


void awi_cmd_scan_done __P((struct awi_softc *sc, u_int8_t));
void awi_cmd_sync_done __P((struct awi_softc *sc, u_int8_t));
void awi_cmd_set_ss_done __P((struct awi_softc *sc, u_int8_t));
void awi_cmd_set_allmulti_done __P((struct awi_softc *sc, u_int8_t));
void awi_cmd_set_promisc_done __P((struct awi_softc *sc, u_int8_t));
void awi_cmd_set_infra_done __P((struct awi_softc *sc, u_int8_t));
void awi_cmd_set_notap_done __P((struct awi_softc *sc, u_int8_t));
void awi_cmd_get_myaddr_done __P((struct awi_softc *sc, u_int8_t));

void awi_reset __P((struct awi_softc *));
void awi_init_1 __P((struct awi_softc *));
void awi_init_2 __P((struct awi_softc *, u_int8_t));
void awi_mibdump __P((struct awi_softc *, u_int8_t));
void awi_init_read_bufptrs_done __P((struct awi_softc *, u_int8_t));
void awi_init_4 __P((struct awi_softc *, u_int8_t));
void awi_init_5 __P((struct awi_softc *, u_int8_t));
void awi_init_6 __P((struct awi_softc *, u_int8_t));
void awi_running __P((struct awi_softc *));

void awi_init_txdescr __P((struct awi_softc *));
void awi_init_txd __P((struct awi_softc *, int, int, int, int));

void awi_watchdog __P((struct ifnet *));
void awi_start __P((struct ifnet *));
int awi_ioctl __P((struct ifnet *, u_long, caddr_t));
void awi_dump_rxchain __P((struct awi_softc *, char *, u_int32_t *));

void awi_send_frame __P((struct awi_softc *, struct mbuf *));
void awi_send_authreq __P((struct awi_softc *));
void awi_send_assocreq __P((struct awi_softc *));
void awi_parse_tlv __P((u_int8_t *base, u_int8_t *end, u_int8_t **vals, u_int8_t *lens, size_t nattr));

u_int8_t *awi_add_rates __P((struct awi_softc *, struct mbuf *, u_int8_t *));
u_int8_t *awi_add_ssid __P((struct awi_softc *, struct mbuf *, u_int8_t *));
void * awi_init_hdr __P((struct awi_softc *, struct mbuf *, int, int));

void awi_hexdump __P((char *tag, u_int8_t *data, int len));
void awi_card_hexdump __P((struct awi_softc *, char *tag, u_int32_t offset, int len));

int awi_drop_output __P((struct ifnet *, struct mbuf *,
    struct sockaddr *, struct rtentry *));
void awi_drop_input __P((struct ifnet *, struct mbuf *));
struct mbuf *awi_output_kludge __P((struct awi_softc *, struct mbuf *));
void awi_set_timer __P((struct awi_softc *));
void awi_restart_scan __P((struct awi_softc *));

struct awi_rxd
{
	u_int32_t next;
	u_int16_t len;
	u_int8_t state, rate, rssi, index;
	u_int32_t frame;
	u_int32_t rxts;
};

void awi_copy_rxd __P((struct awi_softc *, u_int32_t, struct awi_rxd *));
u_int32_t awi_parse_rxd __P((struct awi_softc *, u_int32_t, struct awi_rxd *));

static const u_int8_t snap_magic[] = { 0xaa, 0xaa, 3, 0, 0, 0 };

int awi_scan_keepalive = 10;

/* 
 * attach (called by bus-specific front end)
 *
 *	look for banner message
 *	wait for selftests to complete (up to 2s??? eeee.)
 *		(do this with a timeout!!??!!)
 *	on timeout completion:
 *		issue test_interface command.
 *	get_mib command  to locate TX buffer.
 *	set_mib command to set any non-default variables.
 *	init tx first.
 * 	init rx second with enable receiver command
 *
 *	mac mgmt portion executes sync command to start BSS
 *
 */

/*
 * device shutdown routine.
 */

/*
 * device appears to be insane.  rather than hanging, whap device upside
 * the head on next timeout.
 */

void
awi_insane(sc)
	struct awi_softc *sc;
{
	struct ifnet *ifp = sc->sc_ifp;
	printf("%s: device timeout\n", sc->sc_dev.dv_xname);

	/* whap device on next timeout. */
	sc->sc_state = AWI_ST_INSANE;
	ifp->if_timer = 1;
}

void
awi_set_timer (sc)
	struct awi_softc *sc;
{
	if (sc->sc_tx_timer || sc->sc_scan_timer ||
	    sc->sc_mgt_timer || sc->sc_cmd_timer)
		sc->sc_ifp->if_timer = 1;
}


/*
 * Copy m0 into the given TX descriptor and give the descriptor to the
 * device so it starts transmiting..
 */

void
awi_tx_packet (sc, txd, m0)
	struct awi_softc *sc;
	int txd;
	struct mbuf *m0;
{
	u_int32_t frame = sc->sc_txd[txd].frame;
	u_int32_t len = sc->sc_txd[txd].len;
	struct mbuf *m;

	for (m = m0; m != NULL; m = m->m_next) {
		u_int32_t nmove;
		nmove = min(len, m->m_len);
		awi_write_bytes (sc, frame, m->m_data, nmove);
		if (nmove != m->m_len) {
			printf("%s: large frame truncated\n",
			    sc->sc_dev.dv_xname);
			break;
		}
		frame += nmove;
		len -= nmove;
	}
	
	awi_init_txd (sc,
	    txd,
	    AWI_TXD_ST_OWN,
	    frame - sc->sc_txd[txd].frame,
	    AWI_RATE_1MBIT);

#if 0
	awi_card_hexdump (sc, "txd to go", sc->sc_txd[txd].descr,
	    AWI_TXD_SIZE);
#endif

}

/*
 * XXX KLUDGE XXX
 *
 * Convert ethernet-formatted frame into 802.11 data frame
 * for infrastructure mode.
 */

struct mbuf *
awi_output_kludge (sc, m0)
	struct awi_softc *sc;
	struct mbuf *m0;
{
	u_int8_t *framehdr;
	u_int8_t *llchdr;
	u_int8_t dstaddr[ETHER_ADDR_LEN];
	struct awi_mac_header *amhdr;
	u_int16_t etype;
	struct ether_header *eh = mtod(m0, struct ether_header *);
	
#if 0
	awi_hexdump("etherframe", m0->m_data, m0->m_len);
#endif

	bcopy(eh->ether_dhost, dstaddr, sizeof(dstaddr));
	etype = eh->ether_type;

	m_adj(m0, sizeof(struct ether_header));
	
	M_PREPEND(m0, sizeof(struct awi_mac_header) + 8, M_DONTWAIT);
	
	if (m0 == NULL) {
		printf("oops, prepend failed\n");
		return NULL;
	}
	
	if (m0->m_len < 32) {
		printf("oops, prepend only left %d bytes\n", m0->m_len);
		m_freem(m0);
		return NULL;
	}
	framehdr = mtod(m0, u_int8_t *);
	amhdr = mtod(m0, struct awi_mac_header *);

	amhdr->awi_fc = IEEEWL_FC_VERS |
	    IEEEWL_FC_TYPE_DATA<<IEEEWL_FC_TYPE_SHIFT;
	amhdr->awi_f2 = IEEEWL_FC2_TODS;

	bcopy(dstaddr, amhdr->awi_addr3, ETHER_ADDR_LEN); /* ether DST */
	bcopy(sc->sc_active_bss.bss_id, amhdr->awi_addr1, ETHER_ADDR_LEN);
	bcopy(sc->sc_my_addr, amhdr->awi_addr2, ETHER_ADDR_LEN);
	amhdr->awi_duration = 0;
	amhdr->awi_seqctl = 0;
	llchdr = (u_int8_t *) (amhdr + 1);
	bcopy(snap_magic, llchdr, 6);
	bcopy(&etype, llchdr+6, 2);
	
	return m0;
}
/*
 * device start routine
 *
 * loop while there are free tx buffer descriptors and mbufs in the queue:
 *	-> copy mbufs to tx buffer and free mbufs.
 *	-> mark txd as good to go	       (OWN bit set, all others clear)
 */

void
awi_start(ifp)
	struct ifnet *ifp;
{
	struct awi_softc *sc = ifp->if_softc;
	struct mbuf *m0;
	int opending;
	
	if ((ifp->if_flags & IFF_RUNNING) == 0) {
		printf("%s: start called while not running\n",
		    sc->sc_dev.dv_xname);
		return;
	}
	
	/*
	 * loop through send queue, setting up tx descriptors
	 * until we either run out of stuff to send, or descriptors
	 * to send them in.
	 */
	opending = sc->sc_txpending;
	
	while (sc->sc_txpending < sc->sc_ntxd) {
		/*
		 * Grab a packet off the queue.
		 */
		IF_DEQUEUE (&sc->sc_mgtq, m0);

		if (m0 == NULL) {
			/* XXX defer sending if not synched yet? */
			IF_DEQUEUE (&ifp->if_snd, m0);
			if (m0 == NULL) 
				break;
#if NBPFILTER > 0
			/*
			 * Pass packet to bpf if there is a listener.
			 */
			if (ifp->if_bpf)
				bpf_mtap(ifp->if_bpf, m0);
#endif
			/*
			 * We've got an ethernet-format frame.
			 * we need to mangle it into 802.11 form..
			 */
			m0 = awi_output_kludge(sc, m0);
			if (m0 == NULL)
				continue;
		}
		
		awi_tx_packet(sc, sc->sc_txnext, m0);

		sc->sc_txpending++;
		sc->sc_txnext = (sc->sc_txnext + 1) % sc->sc_ntxd;
		
		m_freem(m0);
	}
	if (sc->sc_txpending >= sc->sc_ntxd) {
		/* no more slots available.. */
		ifp->if_flags |= IFF_OACTIVE;
	}
	if (sc->sc_txpending != opending) {
		/* set watchdog timer in case unit flakes out */
		if (sc->sc_tx_timer == 0)
			sc->sc_tx_timer = 5;
		awi_set_timer(sc);
	}
}

int
awi_enable(sc)
	struct awi_softc *sc;
{
	if (sc->sc_enabled == 0) {
		if ((sc->sc_enable != NULL) && ((*sc->sc_enable)(sc) != 0)) {
			printf("%s: device enable failed\n",
			    sc->sc_dev.dv_xname);
			return (EIO);
		}
		awi_init(sc);
	}
	sc->sc_enabled = 1;
	return 0;
}

void
awi_disable(sc)
	struct awi_softc *sc;
{
	if (sc->sc_enabled != 0 && sc->sc_disable != NULL) {
		(*sc->sc_disable)(sc);
		sc->sc_enabled = 0;
	}
}



int
awi_intlock(sc)
	struct awi_softc *sc;
{
	int i, j;
	u_int8_t lockout;
	
	DELAY(5);
	for (j=0; j<10; j++) {
		for (i=0; i<AWI_LOCKOUT_SPIN; i++) {
			lockout = awi_read_1(sc, AWI_LOCKOUT_HOST);
			if (!lockout)
				break;
			DELAY(5);
		}
		if (lockout)
			break;
		awi_write_1 (sc, AWI_LOCKOUT_MAC, 1);
		lockout = awi_read_1(sc, AWI_LOCKOUT_HOST);

		if (!lockout)
			break;
		/* oops, lost the race.. try again */
		awi_write_1 (sc, AWI_LOCKOUT_MAC, 0);
	}
		
	if (lockout) {
		awi_insane(sc);
		return 0;
	}
	return 1;
}

void
awi_intunlock(sc)
	struct awi_softc *sc;
{
	awi_write_1 (sc, AWI_LOCKOUT_MAC, 0);
}

void
awi_intrinit(sc)
	struct awi_softc *sc;
{
	u_int8_t intmask;

	am79c930_gcr_setbits(&sc->sc_chip, AM79C930_GCR_ENECINT);
	
	intmask = AWI_INT_GROGGY|AWI_INT_SCAN_CMPLT|
	    AWI_INT_TX|AWI_INT_RX|AWI_INT_CMD;

	intmask = ~intmask;

	if (!awi_intlock(sc))
		return;
	
	awi_write_1(sc, AWI_INTMASK, intmask);
	awi_write_1(sc, AWI_INTMASK2, 0); 

	awi_intunlock(sc);
}

void awi_hexdump (char *tag, u_int8_t *data, int len) 
{
	int i;
	
	printf("%s:", tag);
	for (i=0; i<len; i++) {
		printf(" %02x", data[i]);
	}
	printf("\n");
}

void awi_card_hexdump (sc, tag, offset, len)
	struct awi_softc *sc;
	char *tag;
	u_int32_t offset;
	int len;
{
	int i;
	
	printf("%s:", tag);
	for (i=0; i<len; i++) {
		printf(" %02x", awi_read_1(sc, offset+i));
	}
	printf("\n");
}

u_int8_t
awi_read_intst(sc)
	struct awi_softc *sc;
{
	u_int8_t state;
	
	if (!awi_intlock(sc))
		return 0;

	/* we have int lock.. */

	state = awi_read_1 (sc, AWI_INTSTAT);
	awi_write_1(sc, AWI_INTSTAT, 0);
	
	awi_intunlock(sc);

	return state;
}

		
void
awi_parse_tlv (u_int8_t *base, u_int8_t *end, u_int8_t **vals, u_int8_t *lens, size_t nattr)
{
	u_int8_t tag, len;

	int i;
	
	for (i=0; i<nattr; i++) {
		vals[i] = NULL;
		lens[i] = 0;
	}
	
	while (base < end) {
		tag = base[0];
		len = base[1];

		base += 2;
		
		if (tag < nattr) {
			lens[tag] = len;
			vals[tag] = base;
		}
		base += len;
	}
}

void
awi_send_frame (sc, m)
	struct awi_softc *sc;
	struct mbuf *m;
{
	IF_ENQUEUE(&sc->sc_mgtq, m);

	awi_start(sc->sc_ifp);
}

void *
awi_init_hdr (sc, m, f1, f2)
	struct awi_softc *sc;
	struct mbuf *m;
	int f1;
	int f2;
{
	struct awi_mac_header *amhp;
	
	/*
	 * initialize 802.11 mac header in mbuf, return pointer to next byte..
	 */
	
	amhp = mtod(m, struct awi_mac_header *);

	amhp->awi_fc = f1;
	amhp->awi_f2 = f2;
	amhp->awi_duration = 0;

	bcopy(sc->sc_active_bss.bss_id, amhp->awi_addr1, ETHER_ADDR_LEN);
	bcopy(sc->sc_my_addr, amhp->awi_addr2, ETHER_ADDR_LEN);
	bcopy(sc->sc_active_bss.bss_id, amhp->awi_addr3, ETHER_ADDR_LEN);

	amhp->awi_seqctl = 0;

	return amhp+1;
}



u_int8_t *
awi_add_rates (sc, m, ptr)
	struct awi_softc *sc;
	struct mbuf *m;
	u_int8_t *ptr;
{
	*ptr++ = 1;		/* XXX */
	*ptr++ = 1;		/* XXX */
	*ptr++ = 0x82;		/* XXX */
	return ptr;
}

u_int8_t *
awi_add_ssid (sc, m, ptr)
	struct awi_softc *sc;
	struct mbuf *m;
	u_int8_t *ptr;
{
	int len = sc->sc_active_bss.sslen;
	*ptr++ = 0;		/* XXX */
	*ptr++ = len;
	bcopy(sc->sc_active_bss.ssid, ptr, len);
	ptr += len;
	return ptr;
}



void
awi_send_authreq (sc)
	struct awi_softc *sc;
{
	struct mbuf *m;
	struct awi_auth_hdr *amahp;
	u_int8_t *tlvptr;
	
	MGETHDR(m, M_DONTWAIT, MT_DATA);
	
	/*
	 * form an "association request" message.
	 */

	/* 
	 * auth alg number.  2 bytes.  = 0
	 * auth txn seq number = 2 bytes = 1
	 *  status code	       = 2 bytes = 0
	 *  challenge text	(not present)
	 */

	if (m == 0)
		return;		/* we'll try again later.. */

	amahp = awi_init_hdr (sc, m,
	    (IEEEWL_FC_VERS | 
	    (IEEEWL_FC_TYPE_MGT << IEEEWL_FC_TYPE_SHIFT) |
	    (IEEEWL_SUBTYPE_AUTH << IEEEWL_FC_SUBTYPE_SHIFT)),
	    0);

	amahp->awi_algno[0] = 0;
	amahp->awi_algno[1] = 0;
	amahp->awi_seqno[0] = 1;
	amahp->awi_seqno[1] = 0;
	amahp->awi_status[0] = 0;
	amahp->awi_status[1] = 0;
	
	/*
	 * form an "authentication" message.
	 */

	tlvptr = (u_int8_t *)(amahp+1);

	tlvptr = awi_add_ssid(sc, m, tlvptr);
	tlvptr = awi_add_rates(sc, m, tlvptr);

	m->m_len = tlvptr - mtod(m, u_int8_t *);

	if (sc->sc_ifp->if_flags & IFF_DEBUG) {
		printf("%s: sending auth request\n",
		    sc->sc_dev.dv_xname);
		awi_hexdump("frame", m->m_data, m->m_len);
	}
	
	awi_send_frame(sc, m);

	sc->sc_mgt_timer = 2;
	awi_set_timer(sc);
}

void
awi_send_assocreq (sc)
	struct awi_softc *sc;
{
	struct mbuf *m;
	struct awi_assoc_hdr *amahp;
	u_int8_t *tlvptr;
	
	MGETHDR(m, M_DONTWAIT, MT_DATA);
	
	/*
	 * form an "association request" message.
	 */

	if (m == 0)
		return;		/* we'll try again later.. */

	/* 
	 * cap info (2 bytes)
	 * listen interval	(2 bytes)
	 * ssid			(variable)
	 * supported rates	(variable)
	 */

	amahp = awi_init_hdr (sc, m,
	    IEEEWL_FC_TYPE_MGT, IEEEWL_SUBTYPE_ASSOCREQ);

	amahp->awi_cap_info[0] = 4; /* XXX magic (CF-pollable) */
	amahp->awi_cap_info[1] = 0;
	amahp->awi_li[0] = 1;
	amahp->awi_li[1] = 0;

	tlvptr = (u_int8_t *)(amahp+1);

	tlvptr = awi_add_ssid(sc, m, tlvptr);
	tlvptr = awi_add_rates(sc, m, tlvptr);

	m->m_len = tlvptr - mtod(m, u_int8_t *);


	if (sc->sc_ifp->if_flags & IFF_DEBUG) {
		printf("%s: sending assoc request\n",
		    sc->sc_dev.dv_xname);
		awi_hexdump("frame", m->m_data, m->m_len);
	}

	awi_send_frame(sc, m);

	sc->sc_mgt_timer = 2;
	awi_set_timer(sc);	
}

#if 0
void
awi_send_reassocreq (sc)
{

	/*
	 * form an "reassociation request" message.
	 */

	/* 2 bytes frame control
	   00100000 00000000
	   2 bytes goo
	   00000000 00000000
	   address 1: bssid
	   address 2: my address
	   address 3: bssid
	   2 bytes seq/ctl
	   00000000 00000000

	   cap info (2 bytes)
	   listen interval		(2 bytes)
	   current ap address		(6 bytes)
	   ssid				(variable)
	   supported rates		(va
	*/
}

#endif

void
awi_rcv_ctl (sc, m) 
	struct awi_softc *sc;
	struct mbuf *m;
{
	printf("%s: ctl\n", sc->sc_dev.dv_xname);
}

void
awi_rcv_data (sc, m) 
	struct awi_softc *sc;
	struct mbuf *m;
{
	struct ifnet *ifp = sc->sc_ifp;
	u_int8_t *llc;
	u_int8_t *to, *from;
	struct awi_mac_header *amhp;
	
	sc->sc_scan_timer = awi_scan_keepalive;	/* user data is as good
				   as a beacon as a keepalive.. */

	amhp = mtod(m, struct awi_mac_header *);
	
	/*
	 * we have: 4 bytes useless goo.
	 *	    3 x 6 bytes MAC addresses.
	 *	    2 bytes goo.
	 *	    802.x LLC header, SNAP header, and data.
	 *
	 * for now, we fake up a "normal" ethernet header and feed
	 * this to the appropriate input routine.
	 */

	llc = (u_int8_t *)(amhp+1);
	
	if (amhp->awi_f2 & IEEEWL_FC2_TODS) {
		printf("drop packet to DS\n");
		goto drop;
	}

	to = amhp->awi_addr1;
	if (amhp->awi_f2 & IEEEWL_FC2_FROMDS)
		from = amhp->awi_addr3;
	else
		from = amhp->awi_addr2;
	if (memcmp (llc, snap_magic, 6) != 0)
		goto drop;

	/* XXX overwrite llc with "from" address */
	/* XXX overwrite llc-6 with "to" address */
	bcopy(from, llc, ETHER_ADDR_LEN);
	bcopy(to, llc-6, ETHER_ADDR_LEN);
	
	m_adj(m, sizeof(struct awi_mac_header) + sizeof(struct awi_llc_header) 
	    - sizeof(struct ether_header));

#if NBPFILTER > 0
	/*
	 * Pass packet to bpf if there is a listener.
	 */
	if (ifp->if_bpf)
		bpf_mtap(ifp->if_bpf, m);
#endif

#if __NetBSD_Version__ > 104010000
	m->m_flags |= M_HASFCS;
	(*ifp->if_input)(ifp, m);
#else
	{
		struct ether_header *eh;
		eh = mtod(m, struct ether_header *);
		m_adj(m, sizeof(*eh));
		m_adj(m, -ETHER_CRC_LEN);
		ether_input(ifp, eh, m);
	}
#endif	
	return;
 drop:
	m_freem(m);
}

void
awi_rcv_mgt (sc, m, rxts, rssi) 
	struct awi_softc *sc;
	struct mbuf *m;
	u_int32_t rxts;
	u_int8_t rssi;
{
	u_int8_t subtype;
	u_int8_t *framehdr, *mgthdr, *end, *timestamp;
	struct awi_auth_hdr *auhp;
	struct ifnet *ifp = sc->sc_ifp;
	
#define IEEEWL_MGT_NATTR		10 /* XXX */
	u_int8_t *attr[IEEEWL_MGT_NATTR];
	u_int8_t attrlen[IEEEWL_MGT_NATTR];	
	u_int8_t *addr1, *addr2, *addr3;
	u_int8_t *sa, *da, *bss;
	
	framehdr = mtod(m, u_int8_t *);

	/*
	 * mgt frame:
	 *  2 bytes frame goo
	 *  2 bytes duration
	 *  6 bytes a1
	 *  6 bytes a2
	 *  6 bytes a3
	 *  2 bytes seq control.
	 * --
	 * 24 bytes goo.
	 */
	
	subtype = (framehdr[IEEEWL_FC] & IEEEWL_FC_SUBTYPE_MASK)
	    >> IEEEWL_FC_SUBTYPE_SHIFT;

	addr1 = framehdr + 4;	/* XXX */
	addr2 = addr1+ETHER_ADDR_LEN;
	addr3 = addr2+ETHER_ADDR_LEN;

	/* XXX look at to/from DS bits here!! */
	da = addr1;
	sa = addr3;
	bss = addr2;
	
	framehdr = mtod(m, u_int8_t *);
	end = framehdr + m->m_len;
	end -= 4;	/* trim TLV */
		
	mgthdr = framehdr + 24;	/* XXX magic */
	
	switch (subtype) {

	case IEEEWL_SUBTYPE_ASSOCRESP:
		/*
		 * this acknowledges that the AP will be forwarding traffic
		 * for us..
		 *
		 * contains:
		 *	cap info
		 *	status code
		 *	AId
		 *	supported rates.
		 */
		if (ifp->if_flags & IFF_DEBUG) {
			printf("%s: got assoc resp\n",
			    sc->sc_dev.dv_xname);
			awi_hexdump("assocresp", m->m_data, m->m_len);
		}
		awi_drvstate (sc, AWI_DRV_INFASSOC);
		sc->sc_state = AWI_ST_RUNNING;
		sc->sc_mgt_timer = AWI_ASSOC_REFRESH;
		awi_set_timer(sc);
		if (sc->sc_new_bss) {
			printf("%s: associated with %s, SSID: %s\n",
			    sc->sc_dev.dv_xname,
			    ether_sprintf(sc->sc_active_bss.bss_id),
			    sc->sc_active_bss.ssid);
			sc->sc_new_bss = 0;
		}
		
		/* XXX set media status to "i see carrier" */
		break;
		
	case IEEEWL_SUBTYPE_REASSOCRESP:
		/*
		 * this indicates that we've moved from one AP to another
		 * within the same DS.
		 */
		printf("reassoc_resp\n");

		break;
		
	case IEEEWL_SUBTYPE_PROBEREQ:
		/* discard */
		break;

	case IEEEWL_SUBTYPE_PROBERESP:
		/*
		 * 8 bytes timestamp.
		 * 2 bytes beacon intvl.
		 * 2 bytes cap info.
		 * then tlv data..
		 */
		timestamp = mgthdr;

		if (ifp->if_flags & IFF_DEBUG) {
			printf("%s: got probe resp\n",
			    sc->sc_dev.dv_xname);
			awi_hexdump("proberesp", m->m_data, m->m_len);
		}
		/* now, into the tlv goo.. */
		mgthdr += 12;	/* XXX magic */
		awi_parse_tlv (mgthdr, end, attr, attrlen, IEEEWL_MGT_NATTR);

		if (attr[IEEEWL_MGT_TLV_SSID] &&
		    attr[IEEEWL_MGT_TLV_FHPARMS] &&
		    attrlen[IEEEWL_MGT_TLV_SSID] < AWI_SSID_LEN) { 
			struct awi_bss_binding *bp = NULL;
			int i;
			
			for (i=0; i< sc->sc_nbindings; i++) {
				struct awi_bss_binding *bp1 =
				    &sc->sc_bindings[i];
				if (memcmp(bp1->bss_id, bss, ETHER_ADDR_LEN) == 0) {
					bp = bp1;
					break;
				}
			}

			if (bp == NULL && sc->sc_nbindings < NBND) {
				bp = &sc->sc_bindings[sc->sc_nbindings++];
			}
			if (bp != NULL) {
				u_int8_t *fhparms =
				    attr[IEEEWL_MGT_TLV_FHPARMS];
				
				bp->sslen = attrlen[IEEEWL_MGT_TLV_SSID];
				
				bcopy(attr[IEEEWL_MGT_TLV_SSID], bp->ssid,
				      bp->sslen);
				bp->ssid[bp->sslen] = 0;

				bcopy(bss, bp->bss_id, ETHER_ADDR_LEN);
				
				/* XXX more magic numbers.. */
				bp->dwell_time = fhparms[0] | (fhparms[1]<<8);
				bp->chanset = fhparms[2];
				bp->pattern = fhparms[3];
				bp->index = fhparms[4];
				bp->rssi = rssi;
				bp->rxtime = rxts;
				bcopy(timestamp, bp->bss_timestamp, 8);
			}
		}
		
		break;

	case IEEEWL_SUBTYPE_BEACON:
		if ((ifp->if_flags & (IFF_DEBUG|IFF_LINK2)) ==
		    (IFF_DEBUG|IFF_LINK2)) {
			printf("%s: beacon from %s\n",
			    sc->sc_dev.dv_xname,
			    ether_sprintf(addr2));
			awi_hexdump("beacon", m->m_data, m->m_len);
		}
		/*
		 * Note that AP is still alive so we don't have to go looking
		 * for one for a while.
		 *
		 * XXX Beacons from other AP's should be recorded for
		 * potential use if we lose this AP..  (also, may want
		 * to notice if rssi of new AP is significantly
		 * stronger than old one and jump ship..)
		 */
		if ((sc->sc_state >= AWI_ST_SYNCED) &&
		    (memcmp (addr2, sc->sc_active_bss.bss_id,
			ETHER_ADDR_LEN) == 0)) {
			sc->sc_scan_timer = awi_scan_keepalive;
			awi_set_timer(sc);
		}
		
		break;
		
	case IEEEWL_SUBTYPE_DISSOC:
		printf("dissoc\n");

		break;
		
	case IEEEWL_SUBTYPE_AUTH:
		if (ifp->if_flags & IFF_DEBUG) {
			printf("%s: got auth\n",
			    sc->sc_dev.dv_xname);
			awi_hexdump("auth", m->m_data, m->m_len);
		}
		/*
		 * woohoo!  somebody likes us!
		 */

		auhp = (struct awi_auth_hdr *)mgthdr;

		if ((auhp->awi_status[0] == 0) && (auhp->awi_status[1] == 0))
		{
			awi_drvstate (sc, AWI_DRV_INFAUTH);
			sc->sc_state = AWI_ST_AUTHED;
			awi_send_assocreq (sc);
		}
		break;
		
	case IEEEWL_SUBTYPE_DEAUTH:
		if (ifp->if_flags & IFF_DEBUG) {
			printf("%s: got deauth\n",
			    sc->sc_dev.dv_xname);
			awi_hexdump("deauth", m->m_data, m->m_len);
		}
		sc->sc_state = AWI_ST_SYNCED;
		sc->sc_new_bss = 1;
		awi_send_authreq(sc);
		break;
	default:
		printf("unk mgt subtype %x\n", subtype);
		break;
	}
	m_freem(m);		/* done.. */
}





/*
 * Do 802.11 receive processing.  "m" contains a receive frame;
 * rxts is the local receive timestamp
 */

void
awi_rcv (sc, m, rxts, rssi)
	struct awi_softc *sc;
	struct mbuf *m;
	u_int32_t rxts;
	u_int8_t rssi;
{
	u_int8_t *framehdr;
	u_int8_t framectl;
	
	framehdr = mtod(m, u_int8_t *);

	/*
	 * peek at first byte of frame header.
	 *  check version subfield (must be zero)
	 *  check type subfield (00 = mgt, 01 = ctl, 10 = data)
	 *  check subtype field (next four bits)
	 */

	/*
	 * Not counting WDS mode, the IEEE 802.11 frame header format
	 * has *three* MAC addresses. 
	 * (source, destination, and BSS).
	 *
	 * The BSS indicates which wireless "cable segment" we're part of;
	 * we discover this dynamically..
	 *
	 * Not content to put them in a fixed order, the exact
	 * ordering of these addresses depends on other attribute bits
	 * in the frame control word!
	 *
	 * an alternate presentation which is more self-consistent:
	 * address 1 is the "wireless destination" -- either the
	 * station address,
	 * for wireless->wireless traffic, or the BSS id of an AP.
	 *
	 * address 2 is the "wireless source" -- either the
	 * station address of a wireless node, or the BSS id of an AP.
	 *
	 * address 3 is the "other address" -- for STA->AP, the
	 * eventual destination; for AP->STA, the original source, and
	 * for ad-hoc mode, the BSS id..
	 */

	framectl = framehdr[IEEEWL_FC];
	
	if ((framectl & IEEEWL_FC_VERS_MASK) != IEEEWL_FC_VERS) {
		printf("wrong vers.  drop");
		goto drop;
	}

	switch (framectl & IEEEWL_FC_TYPE_MASK) {
	case IEEEWL_FC_TYPE_MGT << IEEEWL_FC_TYPE_SHIFT:
		awi_rcv_mgt (sc, m, rxts, rssi);
		m = 0;
		break;
		
	case IEEEWL_FC_TYPE_DATA << IEEEWL_FC_TYPE_SHIFT:
		awi_rcv_data (sc, m);
		m = 0;
		break;

	case IEEEWL_FC_TYPE_CTL << IEEEWL_FC_TYPE_SHIFT:
		awi_rcv_ctl (sc, m);
	default:
		goto drop;
	}

 drop:
	if (m) m_freem(m);
}

void
awi_copy_rxd (sc, cur, rxd)
	struct awi_softc *sc;
	u_int32_t cur;
	struct awi_rxd *rxd;
{
	if (sc->sc_ifp->if_flags & IFF_LINK0) {
		printf("%x: ", cur);
		awi_card_hexdump(sc, "rxd", cur, AWI_RXD_SIZE);
	}
	
	rxd->next = awi_read_4(sc, cur + AWI_RXD_NEXT);
	rxd->state = awi_read_1(sc, cur + AWI_RXD_HOST_DESC_STATE);
	rxd->len = awi_read_2 (sc, cur + AWI_RXD_LEN);
	rxd->rate = awi_read_1 (sc, cur + AWI_RXD_RATE);
	rxd->rssi = awi_read_1 (sc, cur + AWI_RXD_RSSI);		
	rxd->index = awi_read_1 (sc, cur + AWI_RXD_INDEX);
	rxd->frame = awi_read_4 (sc, cur + AWI_RXD_START_FRAME);
	rxd->rxts = awi_read_4 (sc, cur + AWI_RXD_LOCALTIME);

	/*
	 * only the low order bits of "frame" and "next" are valid.
	 * (the documentation doesn't mention this).
	 */
	rxd->frame &= 0xffff;
	rxd->next &= (0xffff | AWI_RXD_NEXT_LAST);

	/*
	 * XXX after masking, sanity check that rxd->frame and
	 * rxd->next lie within the receive area.
	 */
	if (sc->sc_ifp->if_flags & IFF_LINK0) {
		printf("nxt %x frame %x state %b len %d\n",
		    rxd->next, rxd->frame,
		    rxd->state, AWI_RXD_ST_BITS,
		    rxd->len);
	}
}
	

u_int32_t
awi_parse_rxd (sc, cur, rxd)
	struct awi_softc *sc;
	u_int32_t cur;
	struct awi_rxd *rxd;
{
	struct mbuf *top;
	struct ifnet *ifp = sc->sc_ifp;
	u_int32_t next;
	
	if ((rxd->state & AWI_RXD_ST_CONSUMED) == 0) {
		if (ifp->if_flags & IFF_LINK1) {
			int xx = awi_read_1(sc, rxd->frame);
			if (xx != (IEEEWL_FC_VERS |
			    (IEEEWL_FC_TYPE_MGT<<IEEEWL_FC_TYPE_SHIFT) |
			    (IEEEWL_SUBTYPE_BEACON << IEEEWL_FC_SUBTYPE_SHIFT))) {
				char bitbuf[64];
				printf("floosh: %d state ", sc->sc_flushpkt);
			 	snprintf(bitbuf, sizeof bitbuf, "%b",
					 rxd->state, AWI_RXD_ST_BITS);
				awi_card_hexdump(sc, bitbuf, rxd->frame,
						 rxd->len);
			}
			
		}
		if ((sc->sc_flushpkt == 0) &&
		    (sc->sc_nextpkt == NULL)) {
			MGETHDR(top, M_DONTWAIT, MT_DATA);
			
			if (top == NULL) {
				sc->sc_flushpkt = 1;
				sc->sc_m = NULL;
				sc->sc_mptr = NULL;
				sc->sc_mleft = 0;
			} else {
				if (rxd->len >= MINCLSIZE)
					MCLGET(top, M_DONTWAIT);
		
				top->m_pkthdr.rcvif = ifp;
				top->m_pkthdr.len = 0;
				top->m_len = 0;
		
				sc->sc_mleft = (top->m_flags & M_EXT) ?
				    MCLBYTES : MHLEN;
				sc->sc_mptr = mtod(top, u_int8_t *);
				sc->sc_m = top;
				sc->sc_nextpkt = top;
			}
		}
		if (sc->sc_flushpkt == 0) {
			/* copy data into mbuf */

			while (rxd->len > 0) {
				int nmove = min (rxd->len, sc->sc_mleft);

				awi_read_bytes (sc, rxd->frame, sc->sc_mptr,
				    nmove);

				rxd->len -= nmove;
				rxd->frame += nmove;
				sc->sc_mleft -= nmove;
				sc->sc_mptr += nmove;
				
				sc->sc_nextpkt->m_pkthdr.len += nmove;
				sc->sc_m->m_len += nmove;
						
				if ((rxd->len > 0) && (sc->sc_mleft == 0)) {
					struct mbuf *m1;
					
					/* Get next mbuf.. */
					MGET(m1, M_DONTWAIT, MT_DATA);
					if (m1 == NULL) {
						m_freem(sc->sc_nextpkt);
						sc->sc_nextpkt = NULL;
						sc->sc_flushpkt = 1;
						sc->sc_m = NULL;
						sc->sc_mptr = NULL;
						sc->sc_mleft = 0;
						break;
					}
					sc->sc_m->m_next = m1;
					sc->sc_m = m1;
					m1->m_len = 0;

					sc->sc_mleft = MLEN;
					sc->sc_mptr = mtod(m1, u_int8_t *);
				}
			}
		}
		if (rxd->state & AWI_RXD_ST_LF) {
			if (sc->sc_flushpkt) {
				sc->sc_flushpkt = 0;
			}
			else if (sc->sc_nextpkt != NULL) {
				struct mbuf *m = sc->sc_nextpkt;
				sc->sc_nextpkt = NULL;
				sc->sc_flushpkt = 0;
				sc->sc_m = NULL;
				sc->sc_mptr = NULL;
				sc->sc_mleft = 0;
				awi_rcv(sc, m, rxd->rxts, rxd->rssi);
			}
		}
	}
	rxd->state |= AWI_RXD_ST_CONSUMED;
	awi_write_1(sc, cur + AWI_RXD_HOST_DESC_STATE, rxd->state);
	next = cur;
	if ((rxd->next & AWI_RXD_NEXT_LAST) == 0) {
		rxd->state |= AWI_RXD_ST_OWN;
		awi_write_1(sc, cur + AWI_RXD_HOST_DESC_STATE, rxd->state);
		next = rxd->next;
	}
	return next;
}

void
awi_dump_rxchain (sc, what, descr)
	struct awi_softc *sc;
	char *what;
	u_int32_t *descr;
{
	u_int32_t cur, next;
	struct awi_rxd rxd;
	
	cur = *descr;

	if (cur & AWI_RXD_NEXT_LAST)
		return;
	
	do {
		awi_copy_rxd(sc, cur, &rxd);

		next = awi_parse_rxd(sc, cur, &rxd);
		if ((rxd.state & AWI_RXD_ST_OWN) && (next == cur)) {
			printf("%s: loop in rxd list?",
			    sc->sc_dev.dv_xname);
			break;
		}
		cur = next;
	} while (rxd.state & AWI_RXD_ST_OWN);

	*descr = cur;
}

void
awi_rxint (sc)
	struct awi_softc *sc;
{
	awi_dump_rxchain (sc, "mgt", &sc->sc_rx_mgt_desc);
	awi_dump_rxchain (sc, "data", &sc->sc_rx_data_desc);	
}

void
awi_init_txd (sc, tx, flag, len, rate)
	struct awi_softc *sc;
	int tx;
	int flag;
	int len;
	int rate;
{
	u_int32_t txdbase = sc->sc_txd[tx].descr;
	u_int32_t framebase = sc->sc_txd[tx].frame;
	u_int32_t nextbase = sc->sc_txd[(tx+1)%sc->sc_ntxd].descr;

	awi_write_4 (sc, txdbase + AWI_TXD_START, framebase);
	awi_write_4 (sc, txdbase + AWI_TXD_NEXT, nextbase);
	awi_write_4 (sc, txdbase + AWI_TXD_LENGTH, len);
	awi_write_1 (sc, txdbase + AWI_TXD_RATE, rate);
	/* zeroize tail end of txd */
	awi_write_4 (sc, txdbase + AWI_TXD_NDA, 0);
	awi_write_4 (sc, txdbase + AWI_TXD_NRA, 0);
	/* Init state last; firmware keys off of this to know when to start tx */
	awi_write_1 (sc, txdbase + AWI_TXD_STATE, flag);
}

void
awi_init_txdescr (sc)
	struct awi_softc *sc;
{
	int i;
	u_int32_t offset = sc->sc_txbase;

	sc->sc_txfirst = 0;
	sc->sc_txnext = 0;

	sc->sc_ntxd = sc->sc_txlen / (AWI_FRAME_SIZE + AWI_TXD_SIZE);
	if (sc->sc_ntxd > NTXD) {
		sc->sc_ntxd = NTXD;
		printf("oops, no, only %d\n", sc->sc_ntxd);
	}

	/* Allocate TXD's */
	for (i=0; i<sc->sc_ntxd; i++) {
		sc->sc_txd[i].descr = offset;
		offset += AWI_TXD_SIZE;
	}
	/* now, allocate buffer space to each txd.. */
	for (i=0; i<sc->sc_ntxd; i++) {
		sc->sc_txd[i].frame = offset;
		sc->sc_txd[i].len = AWI_FRAME_SIZE;
		offset += AWI_FRAME_SIZE;
		
	}
	
	/* now, initialize the TX descriptors into a circular linked list. */
	
	for (i= 0; i<sc->sc_ntxd; i++) {
		awi_init_txd(sc, i, 0, 0, 0);
	}
}

void
awi_txint (sc)
	struct awi_softc *sc;
{
	struct ifnet *ifp = sc->sc_ifp;
	int txfirst;
	
	sc->sc_tx_timer = 0;

	txfirst = sc->sc_txfirst;
	while (sc->sc_txpending > 0) {
		u_int8_t flags = awi_read_1 (sc, sc->sc_txd[txfirst].descr +
		    AWI_TXD_STATE);

		if (flags & AWI_TXD_ST_OWN)
			break;

		if (flags & AWI_TXD_ST_ERROR) {
			/* increment oerrs */;
		}

		txfirst = (txfirst + 1) % sc->sc_ntxd;
		sc->sc_txpending--;
	}

	sc->sc_txfirst = txfirst;

	if (sc->sc_txpending < sc->sc_ntxd)
		ifp->if_flags &= ~IFF_OACTIVE;
	
	/*
	 * see which descriptors are done..
	 */

	awi_start(sc->sc_ifp);
}




/*
 * device interrupt routine.
 *
 *	lock out MAC
 *	loop:
 *		look at intr status, DTRT.
 *
 *		on tx done, reclaim free buffers from tx, call start.
 *		on rx done, look at rx queue, copy to mbufs, mark as free,
 *			hand to ether media layer rx routine.
 *		on cmd done, call cmd cmpl continuation.
 *		
 */

int
awi_intr(arg)
	void *arg;
{
	struct awi_softc *sc = arg;
	int handled = 0;

	if (sc->sc_state == AWI_ST_OFF) {
		u_int8_t intstate = awi_read_intst (sc);
		return intstate != 0;
	}

	/* disable power down, (and implicitly ack interrupt) */
	am79c930_gcr_setbits(&sc->sc_chip, AM79C930_GCR_DISPWDN);
	awi_write_1(sc, AWI_DIS_PWRDN, 1);
	
	for (;;) {
		u_int8_t intstate = awi_read_intst (sc);

		if (!intstate)
			break;

		handled = 1;
		
		if (intstate & AWI_INT_RX)
			awi_rxint(sc);

		if (intstate & AWI_INT_TX) 
			awi_txint(sc);

		if (intstate & AWI_INT_CMD) {
			u_int8_t status;
			
			if (!(sc->sc_flags & AWI_FL_CMD_INPROG))
				printf("%s: no command in progress?\n",
				    sc->sc_dev.dv_xname);
			status = awi_read_1(sc, AWI_CMD_STATUS);
			awi_write_1 (sc, AWI_CMD, 0);
			sc->sc_cmd_timer = 0;
			sc->sc_flags &= ~AWI_FL_CMD_INPROG;
			
			if (sc->sc_completion)
				(*sc->sc_completion)(sc, status);
		}
		if (intstate & AWI_INT_SCAN_CMPLT) {
			if (sc->sc_flags & AWI_FL_CMD_INPROG) {
				panic("i can't take it any more");
			}
			/*
			 * scan completion heuristic..
			 */
			if ((sc->sc_nbindings >= NBND)
			    || ((sc->sc_scan_timer == 0) &&
				(sc->sc_nbindings > 0)))
				awi_try_sync(sc);
			else
				awi_scan_next(sc);
		}
		
	}
	/* reenable power down */
	am79c930_gcr_clearbits(&sc->sc_chip, AM79C930_GCR_DISPWDN);
	awi_write_1(sc, AWI_DIS_PWRDN, 0);

	return handled;
}

/*
 * flush tx queues..
 */

void
awi_flush(sc)
	struct awi_softc *sc;
{
	struct ifnet *ifp = sc->sc_ifp;
	struct mbuf *m;

	do {
		IF_DEQUEUE (&sc->sc_mgtq, m);
		m_freem(m);
	} while (m != NULL);
	
	do {
		IF_DEQUEUE (&ifp->if_snd, m);
		m_freem(m);
	} while (m != NULL);
}



/*
 * device stop routine
 */

void
awi_stop(sc)
	struct awi_softc *sc;
{
	struct ifnet *ifp = sc->sc_ifp;
	
	awi_flush(sc);

	/* Turn off timer.. */
	ifp->if_timer = 0;
	sc->sc_state = AWI_ST_OFF;
	(void) awi_read_intst (sc);
	/*
	 * XXX for pcmcia, there's no point in  disabling the device,
	 * as it's about to be powered off..
	 * for non-PCMCIA attachments, we should, however, stop
	 * the receiver and transmitter here.
	 */
}

/*
 * Watchdog routine, triggered by timer.
 * This does periodic maintainance-type tasks on the interface.
 */

void
awi_watchdog(ifp)
	struct ifnet *ifp;
{
	struct awi_softc *sc = ifp->if_softc;
	u_int8_t test;
	int i;

	if (sc->sc_state == AWI_ST_OFF) 
		/* nothing to do */
		return;
	else if (sc->sc_state == AWI_ST_INSANE) {
		awi_reset(sc);
		return;
	} else if (sc->sc_state == AWI_ST_SELFTEST) {
		/* check for selftest completion.. */
		test = awi_read_1(sc, AWI_SELFTEST);
		if ((test & 0xf0)  == 0xf0) { /* XXX magic numbers */
			if (test == AWI_SELFTEST_PASSED) {
				awi_init_1(sc);
			} else {
				printf("%s: selftest failed (code %x)\n",
				    sc->sc_dev.dv_xname, test);
				awi_reset(sc);
			}
		}
		sc->sc_selftest_tries++;
		/* still running.  try again on next tick */
		if (sc->sc_selftest_tries < 5) {
			ifp->if_timer = 1;
		} else {
			/*
			 * XXX should power down card, wait 1s, power it back
			 * up again..
			 */
			printf("%s: device failed to complete selftest (code %x)\n",
			    sc->sc_dev.dv_xname, test);
			ifp->if_timer = 0;
		}
		return;
	}
	

	/*
	 * command timer: if it goes to zero, device failed to respond.
	 * boot to the head.
	 */
	if (sc->sc_cmd_timer) {
		sc->sc_cmd_timer--;
		if (sc->sc_cmd_timer == 0) {
			sc->sc_flags &= ~AWI_FL_CMD_INPROG;
		
			printf("%s: timeout waiting for command completion\n",
			    sc->sc_dev.dv_xname);
			test = awi_read_1(sc, AWI_CMD_STATUS);
			printf("%s: cmd status: %x\n", sc->sc_dev.dv_xname, test);
			test = awi_read_1(sc, AWI_CMD);		
			printf("%s: cmd: %x\n", sc->sc_dev.dv_xname, test);
			awi_card_hexdump(sc, "CSB", AWI_CSB, 16);
			awi_reset(sc);
			return;
		}
	}
	/*
	 * Transmit timer.  If it goes to zero, device failed to deliver a
	 * tx complete interrupt.  boot to the head.
	 */
	if (sc->sc_tx_timer) {
		sc->sc_tx_timer--;
		if ((sc->sc_tx_timer == 0)  && (sc->sc_txpending)) {
			awi_card_hexdump(sc, "CSB", AWI_CSB, 16);
			printf("%s: transmit timeout\n", sc->sc_dev.dv_xname);
			awi_card_hexdump(sc, "last_txd", AWI_LAST_TXD, 5*4);
			for (i=0; i<sc->sc_ntxd; i++) {
				awi_card_hexdump(sc, "txd",
				    sc->sc_txd[i].descr, AWI_TXD_SIZE);
			}
			awi_reset(sc);
			return;
		}
	}
	/*
	 * Scan timer.
	 * When synched, this is used to notice when we've stopped
	 * receiving beacons and should attempt to resynch.
	 *
	 * When unsynched, this is used to notice if we've received an
	 * interesting probe response and should synch up.
	 */

	if (sc->sc_scan_timer) {
		sc->sc_scan_timer--;
		if (sc->sc_scan_timer == 0) {
			if (sc->sc_state == AWI_ST_SCAN) {
				/*
				 * XXX what if device fails to deliver
				 * a scan-completion interrupt?
				 */
			} else {
				printf("%s: no recent beacon from %s; rescanning\n",
				    sc->sc_dev.dv_xname,
				    ether_sprintf(sc->sc_active_bss.bss_id));
				awi_restart_scan(sc);
			}
		}
	}
	
	/*
	 * Management timer.  Used to know when to send auth
	 * requests and associate requests.
	 */
	if (sc->sc_mgt_timer) {
		sc->sc_mgt_timer--;
		if (sc->sc_mgt_timer == 0) {
			switch (sc->sc_state) 
			{
			case AWI_ST_SYNCED:
			case AWI_ST_RUNNING:
				sc->sc_state = AWI_ST_SYNCED;
				awi_send_authreq(sc);
				break;
			case AWI_ST_AUTHED:
				awi_send_assocreq(sc);
				break;
			default:
				printf("weird state for mgt timeout!\n");
				break;
			}
		}
	}
	awi_set_timer(sc);
}

void
awi_set_mc (sc)
	struct awi_softc  *sc;
{
	/* XXX not implemented yet.. */
}

/*
 * init routine
 */

/*
 * ioctl routine
 * SIOCSIFADDR sets IFF_UP
 * SIOCIFMTU
 * SIOCSIFFLAGS
 * SIOCADDMULTI/SIOCDELMULTI
 */
 
int
awi_ioctl(ifp, cmd, data)
	register struct ifnet *ifp;
	u_long cmd;
	caddr_t data;
{
	struct awi_softc *sc = ifp->if_softc;
	struct ifaddr *ifa = (struct ifaddr *)data;
	struct ifreq *ifr = (struct ifreq *)data;
	int s, error = 0;
	
	s = splnet();

	switch (cmd) {
	case SIOCSIFADDR:
		if ((error = awi_enable(sc)) != 0)
			break;

		ifp->if_flags |= IFF_UP;

		/* XXX other AF support: inet6, NS, ... */
		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			arp_ifinit(&sc->sc_ec, ifa);
			break;
#endif
		default:
			break;
		}
		break;
		
	case SIOCSIFFLAGS:
		if ((ifp->if_flags & IFF_UP) == 0 &&
		    (sc->sc_state != AWI_ST_OFF)) {
			/*
			 * If interface is marked down and it is enabled, then
			 * stop it.
			 */
			ifp->if_flags &= ~IFF_RUNNING;
			awi_stop(sc);
			awi_disable(sc);
		} else if ((ifp->if_flags & IFF_UP) != 0 &&
			   (ifp->if_flags & IFF_RUNNING) == 0) {
			/*
			 * If interface is marked up and it is stopped, then
			 * start it.
			 */
			if ((error = awi_enable(sc)) != 0)
				break;
		} else if ((ifp->if_flags & IFF_UP) != 0) {
			/*
			 * Deal with other flags that change hardware
			 * state, i.e. IFF_PROMISC.
			 */
			awi_set_mc(sc);
		}
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		error = (cmd == SIOCADDMULTI) ?
		    ether_addmulti(ifr, &sc->sc_ec) :
		    ether_delmulti(ifr, &sc->sc_ec);
		if (error == ENETRESET) {
			error = 0;
			awi_set_mc(sc);
		}
		break;

	default:
		error = EINVAL;
		break;
		
	}
	splx(s);
	return error;
	
}

int awi_activate (self, act)
	struct device *self;
	enum devact act;
{
	int s = splnet();
	panic("awi_activate");
	
#if 0
	switch (act) {
	case DVACT_ACTIVATE:
		rv = EOPNOTSUPP;
		break;

	case DVACT_DEACTIVATE:
#ifdef notyet
		/* First, kill off the interface. */
		if_detach(sc->sc_ethercom.ec_if);
#endif

		/* Now disable the interface. */
		awidisable(sc);
		break;
	}
#endif
	splx(s);
	
}

int
awi_drop_output (ifp, m0, dst, rt0)
	struct ifnet *ifp;
	struct mbuf *m0;
	struct sockaddr *dst;
	struct rtentry *rt0;
{
	m_freem(m0);
	return 0;
}

void
awi_drop_input (ifp, m0)
	struct ifnet *ifp;
	struct mbuf *m0;
{
	m_freem(m0);
}

int awi_attach (sc, macaddr)
	struct awi_softc *sc;
	u_int8_t *macaddr;
{
	struct ifnet *ifp = &sc->sc_ec.ac_if;
	u_int8_t version[AWI_BANNER_LEN];

	sc->sc_ifp = ifp;
	sc->sc_nextpkt = NULL;
	sc->sc_m = NULL;
	sc->sc_mptr = NULL;
	sc->sc_mleft = 0;
	sc->sc_flushpkt = 0;
	
	awi_read_bytes (sc, AWI_BANNER, version, AWI_BANNER_LEN);
	printf("%s: firmware %s\n", sc->sc_dev.dv_xname, version);

	bcopy(macaddr, sc->sc_my_addr, ETHER_ADDR_LEN);
	printf("%s: 802.11 address %s\n", sc->sc_dev.dv_xname,
	    ether_sprintf(sc->sc_my_addr));
	
	bcopy(sc->sc_dev.dv_xname, ifp->if_xname, IFNAMSIZ);
	ifp->if_softc = sc;
	ifp->if_start = awi_start;
	ifp->if_ioctl = awi_ioctl;
	ifp->if_watchdog = awi_watchdog;
	ifp->if_mtu = ETHERMTU;	
	/* XXX simplex may not be correct here.. */
	ifp->if_flags =
	    IFF_BROADCAST | IFF_SIMPLEX | IFF_NOTRAILERS | IFF_MULTICAST;

	sc->sc_mgtq.ifq_maxlen = 5;
	
	if_attach(ifp);
	ether_ifattach(ifp);
	ifp->if_hdrlen = 32;	/* 802.11 headers are bigger.. */

#if NBPFILTER > 0
	bpfattach(&ifp->if_bpf, ifp, DLT_EN10MB, sizeof(struct ether_header));
#endif
	return 0;
}

void
awi_zero (sc, from, to)
	struct awi_softc *sc;
	u_int32_t from, to;
{
	u_int32_t i;
	for (i=from; i<to; i++)
		awi_write_1(sc, i, 0);
}

void
awi_init (sc)
	struct awi_softc *sc;
{
	struct ifnet *ifp = sc->sc_ifp;

	sc->sc_scan_duration = 100; /* scan for 100ms */

	/*
	 * Maybe we should randomize these....
	 */
	sc->sc_scan_chanset = IEEEWL_FH_CHANSET_MIN;
	sc->sc_scan_pattern = IEEEWL_FH_PATTERN_MIN;
	
	sc->sc_flags &= ~AWI_FL_CMD_INPROG;

	ifp->if_flags &= ~(IFF_RUNNING|IFF_OACTIVE);
	ifp->if_timer = 0;
	
	sc->sc_cmd_timer = 0;
	sc->sc_tx_timer = 0;
	sc->sc_mgt_timer = 0;
	sc->sc_scan_timer = 0;
	
	sc->sc_nbindings = 0;

	/*
	 * this reset sequence doesn't seem to always do the trick.
	 * hard-power-cycling the card may do it..
	 */
	
	/*
	 * reset the hardware, just to be sure.
	 * (bring out the big hammer here..)
	 */
	/* XXX insert delay here? */
	
	am79c930_gcr_setbits (&sc->sc_chip, AM79C930_GCR_CORESET);
	delay(10);		/* XXX arbitrary value */

	/*
	 * clear control memory regions (firmware should do this but...)
	 */
	awi_zero(sc, AWI_LAST_TXD, AWI_BUFFERS);

	awi_drvstate(sc, AWI_DRV_RESET);
	sc->sc_selftest_tries = 0;

	/*
	 * release reset
	 */
	am79c930_gcr_clearbits (&sc->sc_chip, AM79C930_GCR_CORESET);
	delay(10);
	
	sc->sc_state = AWI_ST_SELFTEST;
	ifp->if_timer = 1;

}

void
awi_cmd (sc, opcode)
	struct awi_softc *sc;
	u_int8_t opcode;
{
	if (sc->sc_flags & AWI_FL_CMD_INPROG)
		panic("%s: command reentered", sc->sc_dev.dv_xname);
	
	sc->sc_flags |= AWI_FL_CMD_INPROG;
	
	/* issue test-interface command */
	awi_write_1(sc, AWI_CMD, opcode);

	awi_write_1(sc, AWI_CMD_STATUS, 0);

	sc->sc_cmd_timer = 2;
	awi_set_timer(sc);
}

void
awi_cmd_test_if (sc)
	struct awi_softc *sc;
{
	awi_cmd (sc, AWI_CMD_NOP);
}

void
awi_cmd_get_mib (sc, var, offset, len)
	struct awi_softc *sc;
	u_int8_t var;
	u_int8_t offset;
	u_int8_t len;
{
	awi_write_1(sc, AWI_CMD_PARAMS+AWI_CA_MIB_TYPE, var);
	awi_write_1(sc, AWI_CMD_PARAMS+AWI_CA_MIB_SIZE, len);
	awi_write_1(sc, AWI_CMD_PARAMS+AWI_CA_MIB_INDEX, offset);

	awi_cmd (sc, AWI_CMD_GET_MIB);
}

void
awi_cmd_txinit (sc) 
	struct awi_softc *sc;
{
	awi_write_4(sc, AWI_CMD_PARAMS+AWI_CA_TX_DATA, sc->sc_txbase);
	awi_write_4(sc, AWI_CMD_PARAMS+AWI_CA_TX_MGT, 0);
	awi_write_4(sc, AWI_CMD_PARAMS+AWI_CA_TX_BCAST, 0);
	awi_write_4(sc, AWI_CMD_PARAMS+AWI_CA_TX_PS, 0);
	awi_write_4(sc, AWI_CMD_PARAMS+AWI_CA_TX_CF, 0);
	
	awi_cmd (sc, AWI_CMD_INIT_TX);
}

int awi_max_chan = -1;
int awi_min_chan = 1000;
int awi_max_pattern = -1;
int awi_min_pattern = 1000;


/*
 * timeout-driven routine: complete device init once device has passed
 * selftest.
 */

void awi_init_1 (sc)
	struct awi_softc *sc;
{
	struct ifnet *ifp = sc->sc_ifp;
	
	awi_intrinit(sc);

	sc->sc_state = AWI_ST_IFTEST;

	if (ifp->if_flags & IFF_DEBUG) {
		awi_card_hexdump(sc, "init_1 CSB", AWI_CSB, 16);
		sc->sc_completion = awi_mibdump;
	} else
		sc->sc_completion = awi_init_2;

	sc->sc_curmib = 0;

	awi_cmd_test_if (sc);
}

void awi_mibdump (sc, status) 
	struct awi_softc *sc;
	u_int8_t status;
{
	u_int8_t mibblk[256];
	    
	if (status != AWI_STAT_OK) {
		printf("%s: pre-mibread failed (card unhappy?)\n",
		    sc->sc_dev.dv_xname);
		awi_reset(sc);
		return;
	}
	
	if (sc->sc_curmib != 0) {
		awi_read_bytes(sc, AWI_CMD_PARAMS+AWI_CA_MIB_DATA,
		    mibblk, 72);
		awi_hexdump("mib", mibblk, 72);
	}
	if (sc->sc_curmib > AWI_MIB_LAST) {
		awi_init_2 (sc, status);
	} else {
		sc->sc_completion = awi_mibdump;
		printf("mib %d\n", sc->sc_curmib);
		awi_cmd_get_mib (sc, sc->sc_curmib, 0, 30);
		sc->sc_curmib++;
		/* skip over reserved MIB's.. */
		if ((sc->sc_curmib == 1) || (sc->sc_curmib == 6))
			sc->sc_curmib++;
	}
}


/*
 * called on completion of test-interface command in first-stage init.
 */

void awi_init_2 (sc, status)
	struct awi_softc *sc;
	u_int8_t status;
{
	/* did it succeed? */
	if (status != AWI_STAT_OK) {
		printf("%s: nop failed (card unhappy?)\n",
		    sc->sc_dev.dv_xname);
		awi_reset(sc);
	}
	
	sc->sc_state = AWI_ST_MIB_GET;
	sc->sc_completion = awi_init_read_bufptrs_done;

	awi_cmd_get_mib (sc, AWI_MIB_LOCAL, 0, AWI_MIB_LOCAL_SIZE);
}

void awi_init_read_bufptrs_done (sc, status)
	struct awi_softc *sc;
	u_int8_t status;
{
	if (status != AWI_STAT_OK) {
		printf("%s: get_mib failed (card unhappy?)\n",
		    sc->sc_dev.dv_xname);
		awi_reset(sc);
	}
	
	sc->sc_txbase = awi_read_4 (sc,
	    AWI_CMD_PARAMS+AWI_CA_MIB_DATA+AWI_MIB_LOCAL_TXB_OFFSET);
	sc->sc_txlen = awi_read_4 (sc,
	    AWI_CMD_PARAMS+AWI_CA_MIB_DATA+AWI_MIB_LOCAL_TXB_SIZE);
	sc->sc_rxbase = awi_read_4 (sc,
	    AWI_CMD_PARAMS+AWI_CA_MIB_DATA+AWI_MIB_LOCAL_RXB_OFFSET);
	sc->sc_rxlen = awi_read_4 (sc,
	    AWI_CMD_PARAMS+AWI_CA_MIB_DATA+AWI_MIB_LOCAL_RXB_SIZE);
	/*
	 * XXX consider repartitioning buffer space to allow for
	 * more efficient usage.
	 * 6144: 3 txds, 1476 waste	(current partition)
	 * better splits:
	 * 4864: 3 txds, 196 waste
	 * 6400: 4 txds, 176 waste
	 * 7936: 5 txds, 156 waste
	 */

#if 0
	printf("tx offset: %x\n", sc->sc_txbase);
	printf("tx size: %x\n", sc->sc_txlen);
	printf("rx offset: %x\n", sc->sc_rxbase);
	printf("rx size: %x\n", sc->sc_rxlen);
#endif
	
	sc->sc_state = AWI_ST_MIB_SET;
	awi_cmd_set_notap(sc);
}

void awi_cmd_set_notap (sc)
	struct awi_softc *sc;
{
	awi_write_1(sc, AWI_CMD_PARAMS+AWI_CA_MIB_TYPE, AWI_MIB_LOCAL);
	awi_write_1(sc, AWI_CMD_PARAMS+AWI_CA_MIB_SIZE, 1);
	awi_write_1(sc, AWI_CMD_PARAMS+AWI_CA_MIB_INDEX,
	    AWI_MIB_LOCAL_ACTING_AS_AP);
	
	awi_write_1(sc, AWI_CMD_PARAMS+AWI_CA_MIB_DATA, 0);
	sc->sc_completion = awi_cmd_set_notap_done;
	awi_cmd (sc, AWI_CMD_SET_MIB);
}

void awi_cmd_set_notap_done (sc, status) 
	struct awi_softc *sc;
	u_int8_t status;
{
	if (status != AWI_STAT_OK) {
		int erroffset = awi_read_1 (sc, AWI_ERROR_OFFSET);
		printf("%s: set_infra failed (card unhappy?); erroffset %d\n",
		    sc->sc_dev.dv_xname,
		    erroffset);
		awi_reset(sc);
		return;
	}
	awi_cmd_set_infra (sc);
}

void awi_cmd_set_infra (sc)
	struct awi_softc *sc;
{

	awi_write_1(sc, AWI_CMD_PARAMS+AWI_CA_MIB_TYPE, AWI_MIB_LOCAL);
	awi_write_1(sc, AWI_CMD_PARAMS+AWI_CA_MIB_SIZE, 1);
	awi_write_1(sc, AWI_CMD_PARAMS+AWI_CA_MIB_INDEX,
	    AWI_MIB_LOCAL_INFRA_MODE);
	
	awi_write_1(sc, AWI_CMD_PARAMS+AWI_CA_MIB_DATA, 1);
	sc->sc_completion = awi_cmd_set_infra_done;
	awi_cmd (sc, AWI_CMD_SET_MIB);
}

void awi_cmd_set_infra_done (sc, status) 
	struct awi_softc *sc;
	u_int8_t status;
{
#if 0
	printf("set_infra done\n");
#endif
	if (status != AWI_STAT_OK) {
		int erroffset = awi_read_1 (sc, AWI_ERROR_OFFSET);
		printf("%s: set_infra failed (card unhappy?); erroffset %d\n",
		    sc->sc_dev.dv_xname,
		    erroffset);
		awi_reset(sc);
		return;
	}
#if 0
	printf("%s: set_infra done\n", sc->sc_dev.dv_xname);
#endif
	awi_cmd_set_allmulti (sc);
}

void awi_cmd_set_allmulti (sc)
	struct awi_softc *sc;
{
	awi_write_1(sc, AWI_CMD_PARAMS+AWI_CA_MIB_TYPE, AWI_MIB_LOCAL);
	awi_write_1(sc, AWI_CMD_PARAMS+AWI_CA_MIB_SIZE, 1);
	awi_write_1(sc, AWI_CMD_PARAMS+AWI_CA_MIB_INDEX,
	    AWI_MIB_LOCAL_FILTMULTI);
	awi_write_1(sc, AWI_CMD_PARAMS+AWI_CA_MIB_DATA, 0);
	sc->sc_completion = awi_cmd_set_allmulti_done;
	awi_cmd (sc, AWI_CMD_SET_MIB);
}

void awi_cmd_set_allmulti_done (sc, status) 
	struct awi_softc *sc;
	u_int8_t status;
{
	if (status != AWI_STAT_OK) {
		int erroffset = awi_read_1 (sc, AWI_ERROR_OFFSET);
		printf("%s: set_almulti_done failed (card unhappy?); erroffset %d\n",
		    sc->sc_dev.dv_xname,
		    erroffset);
		awi_reset(sc);
		return;
	}
	awi_cmd_set_promisc (sc);
}

void awi_cmd_set_promisc (sc)
	struct awi_softc *sc;
{
	awi_write_1(sc, AWI_CMD_PARAMS+AWI_CA_MIB_TYPE, AWI_MIB_MAC);
	awi_write_1(sc, AWI_CMD_PARAMS+AWI_CA_MIB_SIZE, 1);
	awi_write_1(sc, AWI_CMD_PARAMS+AWI_CA_MIB_INDEX,
	    AWI_MIB_MAC_PROMISC); 
	awi_write_1(sc, AWI_CMD_PARAMS+AWI_CA_MIB_DATA, 0); /* XXX */
	sc->sc_completion = awi_cmd_set_promisc_done;
	awi_cmd (sc, AWI_CMD_SET_MIB);
}

void awi_cmd_set_promisc_done (sc, status) 
	struct awi_softc *sc;
	u_int8_t status;
{
#if 0
	printf("set promisc_done\n");
#endif
	
	if (status != AWI_STAT_OK) {
		int erroffset = awi_read_1 (sc, AWI_ERROR_OFFSET);
		printf("%s: set_promisc_done failed (card unhappy?); erroffset %d\n",
		    sc->sc_dev.dv_xname,
		    erroffset);
		awi_reset(sc);
		return;
	}
#if 0
	printf("%s: set_promisc done\n", sc->sc_dev.dv_xname);
#endif

	awi_init_txdescr(sc);

	sc->sc_state = AWI_ST_TXINIT;
	sc->sc_completion = awi_init_4;
	awi_cmd_txinit(sc);
}

void
awi_init_4 (sc, status)
	struct awi_softc *sc;
	u_int8_t status;
{
#if 0
	printf("%s: awi_init_4, st %x\n", sc->sc_dev.dv_xname, status);
	awi_card_hexdump(sc, "init_4 CSB", AWI_CSB, 16);
#endif

	if (status != AWI_STAT_OK) {
		int erroffset = awi_read_1 (sc, AWI_ERROR_OFFSET);
		printf("%s: init_tx failed (card unhappy?); erroffset %d\n",
		    sc->sc_dev.dv_xname,
		    erroffset);
		awi_reset(sc);
		return;
	}

	sc->sc_state = AWI_ST_RXINIT;
	sc->sc_completion = awi_init_5;
	
	awi_cmd (sc, AWI_CMD_INIT_RX);
}

void awi_init_5 (sc, status)
	struct awi_softc *sc;
	u_int8_t status;
{
#if 0
	struct ifnet *ifp = sc->sc_ifp;
#endif
	
#if 0
	printf("%s: awi_init_5, st %x\n", sc->sc_dev.dv_xname, status);
	awi_card_hexdump(sc, "init_5 CSB", AWI_CSB, 16);
#endif

	if (status != AWI_STAT_OK) {
		printf("%s: init_rx failed (card unhappy?)\n",
		    sc->sc_dev.dv_xname);
		awi_reset(sc);
		return;
	}

	sc->sc_rx_data_desc = awi_read_4(sc, AWI_CMD_PARAMS+AWI_CA_IRX_DATA_DESC);
	sc->sc_rx_mgt_desc = awi_read_4(sc, AWI_CMD_PARAMS+AWI_CA_IRX_PS_DESC);	

#if 0
	printf("%s: data desc %x, mgt desc %x\n", sc->sc_dev.dv_xname,
	    sc->sc_rx_data_desc, sc->sc_rx_mgt_desc);
#endif
	awi_restart_scan(sc);
}

void awi_restart_scan (sc)
	struct awi_softc *sc;
{
	if (sc->sc_ifp->if_flags & IFF_DEBUG) {
		printf("%s: starting scan\n", sc->sc_dev.dv_xname);
	}
	sc->sc_scan_timer = 2;
	sc->sc_mgt_timer = 0;
	awi_set_timer(sc);

	sc->sc_nbindings = 0;
	sc->sc_state = AWI_ST_SCAN;
	awi_drvstate (sc, AWI_DRV_INFSC);
	awi_cmd_scan (sc);
}

void
awi_cmd_scan (sc)
	struct awi_softc *sc;
{
	
	awi_write_2 (sc, AWI_CMD_PARAMS+AWI_CA_SCAN_DURATION,
	    sc->sc_scan_duration);
	awi_write_1 (sc, AWI_CMD_PARAMS+AWI_CA_SCAN_SET,
	    sc->sc_scan_chanset);
	awi_write_1 (sc, AWI_CMD_PARAMS+AWI_CA_SCAN_PATTERN,
	    sc->sc_scan_pattern);
	awi_write_1 (sc, AWI_CMD_PARAMS+AWI_CA_SCAN_IDX, 1);
	awi_write_1 (sc, AWI_CMD_PARAMS+AWI_CA_SCAN_SUSP, 0);	
	
	sc->sc_completion = awi_cmd_scan_done;
	awi_cmd (sc, AWI_CMD_SCAN);
}

void
awi_cmd_scan_done (sc, status)
	struct awi_softc *sc;
	u_int8_t status;
{
#if 0
	int erroffset;
#endif
	if (status == AWI_STAT_OK) {
		if (sc->sc_scan_chanset > awi_max_chan)
			awi_max_chan = sc->sc_scan_chanset;
		if (sc->sc_scan_chanset < awi_min_chan)
			awi_min_chan = sc->sc_scan_chanset;
		if (sc->sc_scan_pattern > awi_max_pattern)
			awi_max_pattern = sc->sc_scan_pattern;
		if (sc->sc_scan_pattern < awi_min_pattern)
			awi_min_pattern = sc->sc_scan_pattern;
		
		return;
	}
#if 0
	erroffset = awi_read_1 (sc, AWI_ERROR_OFFSET);
	printf("%s: scan failed; erroffset %d\n", sc->sc_dev.dv_xname,
	    erroffset);
#endif
	/* wait for response or scan timeout.. */
}

void
awi_scan_next (sc)
	struct awi_softc *sc;
{
	sc->sc_scan_pattern++;
	if (sc->sc_scan_pattern > IEEEWL_FH_PATTERN_MAX) {
		sc->sc_scan_pattern = IEEEWL_FH_PATTERN_MIN;		

		sc->sc_scan_chanset++;
		if (sc->sc_scan_chanset > IEEEWL_FH_CHANSET_MAX)
			sc->sc_scan_chanset = IEEEWL_FH_CHANSET_MIN;
	}
#if 0
	printf("scan: pattern %x chanset %x\n", sc->sc_scan_pattern,
	    sc->sc_scan_chanset);
#endif
	
	awi_cmd_scan(sc);
}

void
awi_try_sync (sc) 
	struct awi_softc *sc;
{
	int max_rssi = 0, best = 0;
	int i;
	struct awi_bss_binding *bp = NULL;
	
	awi_flush(sc);

	if (sc->sc_ifp->if_flags & IFF_DEBUG) {
		printf("%s: looking for best of %d\n",
		    sc->sc_dev.dv_xname, sc->sc_nbindings);
	}
	/* pick one with best rssi */
	for (i=0; i<sc->sc_nbindings; i++) {
		bp = &sc->sc_bindings[i];

		if (bp->rssi > max_rssi) {
			max_rssi = bp->rssi;
			best = i;
		}
	}

	if (bp == NULL) {
		printf("%s: no beacons seen\n", sc->sc_dev.dv_xname);
		awi_scan_next(sc);
		return;
	}

	if (sc->sc_ifp->if_flags & IFF_DEBUG) {
		printf("%s: best %d\n", sc->sc_dev.dv_xname, best);
	}
	sc->sc_scan_timer = awi_scan_keepalive;
	
	bp = &sc->sc_bindings[best];
	bcopy(bp, &sc->sc_active_bss, sizeof(*bp));
	sc->sc_new_bss = 1;
	
	awi_write_1 (sc, AWI_CMD_PARAMS+AWI_CA_SYNC_SET, bp->chanset);
	awi_write_1 (sc, AWI_CMD_PARAMS+AWI_CA_SYNC_PATTERN, bp->pattern);	
	awi_write_1 (sc, AWI_CMD_PARAMS+AWI_CA_SYNC_IDX, bp->index);	
	awi_write_1 (sc, AWI_CMD_PARAMS+AWI_CA_SYNC_STARTBSS, 0);

	awi_write_2 (sc, AWI_CMD_PARAMS+AWI_CA_SYNC_DWELL, bp->dwell_time);
	awi_write_2 (sc, AWI_CMD_PARAMS+AWI_CA_SYNC_MBZ, 0);

	awi_write_bytes (sc, AWI_CMD_PARAMS+AWI_CA_SYNC_TIMESTAMP,
	    bp->bss_timestamp, 8);
	awi_write_4 (sc, AWI_CMD_PARAMS+AWI_CA_SYNC_REFTIME, bp->rxtime);

	sc->sc_completion = awi_cmd_sync_done;
	
	awi_cmd (sc, AWI_CMD_SYNC);

}

void
awi_cmd_sync_done (sc, status)
	struct awi_softc *sc;
	u_int8_t status;
{
	if (status != AWI_STAT_OK) {
		int erroffset = awi_read_1 (sc, AWI_ERROR_OFFSET);
		printf("%s: sync_done failed (card unhappy?); erroffset %d\n",
		    sc->sc_dev.dv_xname,
		    erroffset);
		awi_reset(sc);
		return;
	}

	/*
	 * at this point, the card should be synchronized with the AP
	 * we heard from.  tell the card what BSS and ESS it's running in..
	 */
	
	awi_drvstate (sc, AWI_DRV_INFSY);
	if (sc->sc_ifp->if_flags & IFF_DEBUG) {
		printf("%s: sync done, setting bss/iss parameters\n",
		    sc->sc_dev.dv_xname);
		awi_hexdump ("bss", sc->sc_active_bss.bss_id, ETHER_ADDR_LEN);
		printf("ssid: %s\n", sc->sc_active_bss.ssid);
	}

	awi_cmd_set_ss (sc);
}


void awi_cmd_set_ss (sc)
	struct awi_softc *sc;
{
	awi_write_1(sc, AWI_CMD_PARAMS+AWI_CA_MIB_TYPE, AWI_MIB_MAC_MGT);
	awi_write_1(sc, AWI_CMD_PARAMS+AWI_CA_MIB_SIZE,
	    ETHER_ADDR_LEN + AWI_MIB_MGT_ESS_SIZE);
	awi_write_1(sc, AWI_CMD_PARAMS+AWI_CA_MIB_INDEX,
	    AWI_MIB_MGT_BSS_ID);
	
	awi_write_bytes(sc, AWI_CMD_PARAMS+AWI_CA_MIB_DATA,
	    sc->sc_active_bss.bss_id, ETHER_ADDR_LEN);
	awi_write_1(sc, AWI_CMD_PARAMS+AWI_CA_MIB_DATA+ETHER_ADDR_LEN,
	    0);			/* XXX */
	awi_write_1(sc, AWI_CMD_PARAMS+AWI_CA_MIB_DATA+ETHER_ADDR_LEN+1,
	    sc->sc_active_bss.sslen);
	awi_write_bytes(sc, AWI_CMD_PARAMS+AWI_CA_MIB_DATA+8,
	    sc->sc_active_bss.ssid, AWI_MIB_MGT_ESS_SIZE-2);

	sc->sc_completion = awi_cmd_set_ss_done;
	awi_cmd (sc, AWI_CMD_SET_MIB);
}

void awi_cmd_set_ss_done (sc, status) 
	struct awi_softc *sc;
	u_int8_t status;
{
	if (status != AWI_STAT_OK) {
		int erroffset = awi_read_1 (sc, AWI_ERROR_OFFSET);
		printf("%s: set_ss_done failed (card unhappy?); erroffset %d\n",
		    sc->sc_dev.dv_xname,
		    erroffset);
		awi_reset(sc);
		return;
	}
#if 0
	printf("%s: set_ss done\n", sc->sc_dev.dv_xname);
#endif

	awi_running (sc);
	
	/*
	 * now, we *should* be getting broadcast frames..
	 */
	sc->sc_state = AWI_ST_SYNCED;
	awi_send_authreq (sc);
	
}

void awi_running (sc)
	struct awi_softc *sc;
	
{
	struct ifnet *ifp = sc->sc_ifp;
	
	/*
	 * Who knows what it is to be running?
	 * Only he who is running knows..
	 */
	ifp->if_flags |= IFF_RUNNING;
	awi_start(ifp);
}


void awi_reset (sc)
	struct awi_softc *sc;
{
	printf("%s: reset\n", sc->sc_dev.dv_xname);

}
