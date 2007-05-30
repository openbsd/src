/*	$OpenBSD: l2cap_lower.c,v 1.1 2007/05/30 03:42:53 uwe Exp $	*/
/*	$NetBSD: l2cap_lower.c,v 1.6 2007/04/21 06:15:23 plunky Exp $	*/

/*-
 * Copyright (c) 2005 Iain Hibbert.
 * Copyright (c) 2006 Itronix Inc.
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
 * 3. The name of Itronix Inc. may not be used to endorse
 *    or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ITRONIX INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL ITRONIX INC. BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/systm.h>

#include <netbt/bluetooth.h>
#include <netbt/hci.h>
#include <netbt/l2cap.h>

/****************************************************************************
 *
 *	L2CAP Channel Lower Layer interface
 */

/*
 * L2CAP channel is disconnected, could be:
 *
 * HCI layer received "Disconnect Complete" event for ACL link
 * some Request timed out
 * Config failed
 * Other end reported invalid CID
 * Normal disconnection
 * Change link mode failed
 */
void
l2cap_close(struct l2cap_channel *chan, int err)
{
	struct l2cap_pdu *pdu;
	struct l2cap_req *req, *n;

	if (chan->lc_state == L2CAP_CLOSED)
		return;

	/*
	 * Since any potential PDU could be half sent we just let it go,
	 * but disassociate ourselves from it as links deal with ownerless
	 * PDU's in any case.  We could try harder to flush unsent packets
	 * but maybe its better to leave them in the queue?
	 */
	TAILQ_FOREACH(pdu, &chan->lc_link->hl_txq, lp_next) {
		if (pdu->lp_chan == chan)
			pdu->lp_chan = NULL;
	}

	/*
	 * and clear any outstanding requests..
	 */
	req = TAILQ_FIRST(&chan->lc_link->hl_reqs);
	while (req != NULL) {
		n = TAILQ_NEXT(req, lr_next);
		if (req->lr_chan == chan)
			l2cap_request_free(req);

		req = n;
	}

	chan->lc_pending = 0;
	chan->lc_state = L2CAP_CLOSED;
	hci_acl_close(chan->lc_link, err);
	chan->lc_link = NULL;

	(*chan->lc_proto->disconnected)(chan->lc_upper, err);
}

/*
 * Process incoming L2CAP frame from ACL link. We take off the B-Frame
 * header (which is present in all packets), verify the data length
 * and distribute the rest of the frame to the relevant channel
 * handler.
 */
void
l2cap_recv_frame(struct mbuf *m, struct hci_link *link)
{
	struct l2cap_channel *chan;
	l2cap_hdr_t hdr;

	m_copydata(m, 0, sizeof(hdr), (caddr_t)&hdr);
	m_adj(m, sizeof(hdr));

	hdr.length = letoh16(hdr.length);
	hdr.dcid = letoh16(hdr.dcid);

	DPRINTFN(5, "(%s) received packet (%d bytes)\n",
		    link->hl_unit->hci_devname, hdr.length);

	if (hdr.length != m->m_pkthdr.len)
		goto failed;

	if (hdr.dcid == L2CAP_SIGNAL_CID) {
		l2cap_recv_signal(m, link);
		return;
	}

	if (hdr.dcid == L2CAP_CLT_CID) {
		m_freem(m);	/* TODO */
		return;
	}

	chan = l2cap_cid_lookup(hdr.dcid);
	if (chan != NULL && chan->lc_link == link
	    && chan->lc_state == L2CAP_OPEN) {
		(*chan->lc_proto->input)(chan->lc_upper, m);
		return;
	}

	DPRINTF("(%s) dropping %d L2CAP data bytes for unknown CID #%d\n",
		link->hl_unit->hci_devname, hdr.length, hdr.dcid);

failed:
	m_freem(m);
}

/*
 * Start another L2CAP packet on its way. This is called from l2cap_send
 * (when no PDU is pending) and hci_acl_start (when PDU has been placed on
 * device queue). Thus we can have more than one PDU waiting at the device
 * if space is available but no single channel will hog the link.
 */
int
l2cap_start(struct l2cap_channel *chan)
{
	struct mbuf *m;
	int err = 0;

	if (chan->lc_state != L2CAP_OPEN)
		return 0;

	if (IF_IS_EMPTY(&chan->lc_txq)) {
		DPRINTFN(5, "no data, pending = %d\n", chan->lc_pending);
		/*
		 * If we are just waiting for the queue to flush
		 * and it has, we may disconnect..
		 */
		if (chan->lc_flags & L2CAP_SHUTDOWN
		    && chan->lc_pending == 0) {
			chan->lc_state = L2CAP_WAIT_DISCONNECT;
			err = l2cap_send_disconnect_req(chan);
			if (err)
				l2cap_close(chan, err);
		}

		return err;
	}

	/*
	 * We could check QoS/RFC mode here and optionally not send
	 * the packet if we are not ready for any reason
	 *
	 * Also to support flush timeout then we might want to start
	 * the timer going? (would need to keep some kind of record
	 * of packets sent, possibly change it so that we allocate
	 * the l2cap_pdu and fragment the packet, then hand it down
	 * and get it back when its completed). Hm.
	 */

	IF_DEQUEUE(&chan->lc_txq, m);

	KASSERT(chan->lc_link != NULL);
	KASSERT(m != NULL);

	DPRINTFN(5, "CID #%d sending packet (%d bytes)\n",
		chan->lc_lcid, m->m_pkthdr.len);

	chan->lc_pending++;
	return hci_acl_send(m, chan->lc_link, chan);
}
