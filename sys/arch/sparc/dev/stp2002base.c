/*	$OpenBSD: stp2002base.c,v 1.1 1998/07/17 21:33:11 jason Exp $	*/

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
 * Driver for the STP2002QFP chips found in both the hme and qec+be ethernet
 * controllers.
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

#include <sparc/dev/stp2002var.h>

static struct mbuf *stp2002_get __P((struct stp_base *, int, int));
static int stp2002_put __P((struct stp_base *, int, struct mbuf *));
static void stp2002_read __P((struct stp_base *, int, int));

/*
 * Allocate and initialize buffers and descriptor rings
 */
void
stp2002_meminit(stp)
	struct stp_base *stp;
{
	struct stp_desc *desc;
	int i;

	if (stp->stp_desc_dva == NULL)
		stp->stp_desc_dva = (struct stp_desc *) dvma_malloc(
		    sizeof(struct stp_desc), &(stp->stp_desc), M_NOWAIT);

	if (stp->stp_bufs_dva == NULL)
		stp->stp_bufs_dva = (struct stp_bufs *) dvma_malloc(
		    sizeof(struct stp_bufs) + STP_RX_ALIGN_SIZE,
		    &(stp->stp_bufs),
		    M_NOWAIT); /* XXX must be aligned on 64 byte boundary */


	stp->stp_rx_dvma = (u_long) stp->stp_desc_dva +   
	    (((u_long) &stp->stp_desc->stp_rxd[0]) - ((u_long)stp->stp_desc));  
	stp->stp_tx_dvma = (u_long) stp->stp_desc_dva +   
	    (((u_long) &stp->stp_desc->stp_txd[0]) - ((u_long)stp->stp_desc));

	desc = stp->stp_desc;

	/* setup tx descriptors */
	stp->stp_first_td = stp->stp_last_td = stp->stp_no_td = 0;
	for (i = 0; i < STP_TX_RING_SIZE; i++)
		desc->stp_txd[i].tx_flags = 0;

	/* setup rx descriptors */
	stp->stp_last_rd = 0;
	for (i = 0; i < STP_RX_RING_SIZE; i++) {
		desc->stp_rxd[i].rx_addr = (u_long) stp->stp_bufs_dva +
		    (((u_long) &(stp->stp_bufs->rx_buf[i])) -
		    ((u_long) stp->stp_bufs));
		desc->stp_rxd[i].rx_flags =
		    STP_RXFLAG_OWN |
		    ((STP_RX_PKT_BUF_SZ - STP_RX_OFFSET) << 16);
	}
}

/*
 * Pull data off an interface.
 * Len is the length of data, with local net header stripped.
 * We copy the data into mbufs.  When full cluster sized units are present,
 * we copy into clusters.
 */
static struct mbuf *
stp2002_get(stp, idx, totlen)
	struct stp_base *stp;
	int idx, totlen;
{
	struct ifnet *ifp = &stp->stp_arpcom.ac_if;
	struct mbuf *m;
	struct mbuf *top, **mp;
	int len, pad, boff = 0;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return (NULL);
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
		bcopy(&(stp->stp_bufs->rx_buf[idx][boff + STP_RX_OFFSET]),
			mtod(m, caddr_t), len);
		boff += len;
		totlen -= len;
		*mp = m;
		mp = &m->m_next;
	}

	return (top);
}

static int
stp2002_put(stp, idx, m)
	struct stp_base *stp;
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
		bcopy(mtod(m, caddr_t), &(stp->stp_bufs->tx_buf[idx][boff]), len);
		boff += len;
		tlen += len;
		MFREE(m, n);
	}
	return tlen;
}

/*
 * receive interrupt
 */
int
stp2002_rint(stp)
	struct stp_base *stp;
{
	struct ifnet *ifp = &stp->stp_arpcom.ac_if;
	int bix, len;
	struct stp_rxd rxd;

	bix = stp->stp_last_rd;

	/* Process all buffers with valid data. */
	for (;;) {
		bcopy(&(stp->stp_desc->stp_rxd[bix]), &rxd, sizeof(rxd));
		len = rxd.rx_flags >> 16;

		if (rxd.rx_flags & STP_RXFLAG_OWN)
			break;

		if (rxd.rx_flags & STP_RXFLAG_OVERFLOW)
			ifp->if_ierrors++;
		else
			stp2002_read(stp, bix, len);

		rxd.rx_flags = STP_RXFLAG_OWN |
				((STP_RX_PKT_BUF_SZ - STP_RX_OFFSET) << 16);
		bcopy(&rxd, &(stp->stp_desc->stp_rxd[bix]), sizeof(rxd));

		if (++bix == STP_RX_RING_SIZE)
			bix = 0;
	}

	stp->stp_last_rd = bix;

	return 1;
}

/*
 * transmit interrupt
 */
int
stp2002_tint(stp)
	struct stp_base *stp;
{
	struct ifnet *ifp = &stp->stp_arpcom.ac_if;
	int bix;
	struct stp_txd txd;

	bix = stp->stp_first_td;

	for (;;) {
		if (stp->stp_no_td <= 0)
			break;

		bcopy(&(stp->stp_desc->stp_txd[bix]), &txd, sizeof(txd));

		if (txd.tx_flags & STP_TXFLAG_OWN)
			break;

		ifp->if_flags &= ~IFF_OACTIVE;
		ifp->if_opackets++;

		if (++bix == STP_TX_RING_SIZE)
			bix = 0;

		--stp->stp_no_td;
	}

	stp->stp_first_td = bix;

	stp2002_start(stp);

	if (stp->stp_no_td == 0)
		ifp->if_timer = 0;

	return 1;
}

/*
 * Start output on interface.
 * We make two assumptions here:
 *  1) that the current priority is set to splnet _before_ this code
 *     is called *and* is returned to the appropriate priority after
 *     return
 *  2) that the IFF_OACTIVE flag is checked before this code is called
 *     (i.e. that the output part of the interface is idle)
 *
 * This function returns 1 if at least one packet has been placed in the
 * descriptor ring, and 0 otherwise.  The intent is that upon return, the
 * caller can conditionally wake up the DMA engine.
 */
void
stp2002_start(stp)
	struct stp_base *stp;
{
	struct ifnet *ifp = &stp->stp_arpcom.ac_if;
	struct mbuf *m;
	int bix, len;

	if ((ifp->if_flags & (IFF_RUNNING | IFF_OACTIVE)) != IFF_RUNNING)
		return;

	bix = stp->stp_last_td;

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
		len = stp2002_put(stp, bix, m);

		/*
		 * Initialize transmit registers and start transmission
		 */
		stp->stp_desc->stp_txd[bix].tx_addr = (u_long) stp->stp_bufs_dva +
			(((u_long) &(stp->stp_bufs->tx_buf[bix])) -
			((u_long) stp->stp_bufs));
		stp->stp_desc->stp_txd[bix].tx_flags =
			STP_TXFLAG_OWN | STP_TXFLAG_SOP | STP_TXFLAG_EOP |
			(len & STP_TXFLAG_SIZE);

		(*stp->stp_tx_dmawakeup)(ifp->if_softc);

		if (++bix == STP_TX_RING_SIZE)
			bix = 0;

		if (++stp->stp_no_td == STP_TX_RING_SIZE) {
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}
	}

	stp->stp_last_td = bix;
}

static void
stp2002_read(stp, idx, len)
	struct stp_base *stp;
	int idx, len;
{
	struct ifnet *ifp = &stp->stp_arpcom.ac_if;
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
	m = stp2002_get(stp, idx, len);
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
