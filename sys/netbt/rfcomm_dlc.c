/*	$OpenBSD: rfcomm_dlc.c,v 1.5 2009/11/21 13:05:32 guenther Exp $	*/
/*	$NetBSD: rfcomm_dlc.c,v 1.6 2008/08/06 15:01:24 plunky Exp $	*/

/*-
 * Copyright (c) 2006 Itronix Inc.
 * All rights reserved.
 *
 * Written by Iain Hibbert for Itronix Inc.
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

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/proc.h>
#include <sys/systm.h>

#include <netbt/bluetooth.h>
#include <netbt/hci.h>
#include <netbt/l2cap.h>
#include <netbt/rfcomm.h>

/*
 * rfcomm_dlc_lookup(rfcomm_session, dlci)
 *
 * Find DLC on session with matching dlci
 */
struct rfcomm_dlc *
rfcomm_dlc_lookup(struct rfcomm_session *rs, int dlci)
{
	struct rfcomm_dlc *dlc;

	LIST_FOREACH(dlc, &rs->rs_dlcs, rd_next) {
		if (dlc->rd_dlci == dlci)
			break;
	}

	return dlc;
}

/*
 * rfcomm_dlc_newconn(rfcomm_session, dlci)
 *
 * handle a new dlc request (since its called from a couple of places)
 */
struct rfcomm_dlc *
rfcomm_dlc_newconn(struct rfcomm_session *rs, int dlci)
{
	struct rfcomm_session *ls;
	struct rfcomm_dlc *new, *dlc, *any, *best;
	struct sockaddr_bt laddr, raddr, addr;
	int chan;

	/*
	 * Search amongst the listening DLC community for the best match for
	 * address & channel. We keep listening DLC's hanging on listening
	 * sessions in a last first order, so scan the entire bunch and keep
	 * a note of the best address and BDADDR_ANY matches in order to find
	 * the oldest and most specific match.
	 */
	l2cap_sockaddr(rs->rs_l2cap, &laddr);
	l2cap_peeraddr(rs->rs_l2cap, &raddr);
	chan = RFCOMM_CHANNEL(dlci);
	new = NULL;

	any = best = NULL;
	LIST_FOREACH(ls, &rfcomm_session_listen, rs_next) {
		l2cap_sockaddr(ls->rs_l2cap, &addr);

		if (addr.bt_psm != laddr.bt_psm)
			continue;

		if (bdaddr_same(&laddr.bt_bdaddr, &addr.bt_bdaddr)) {
			LIST_FOREACH(dlc, &ls->rs_dlcs, rd_next) {
				if (dlc->rd_laddr.bt_channel == chan)
					best = dlc;
			}
		}

		if (bdaddr_any(&addr.bt_bdaddr)) {
			LIST_FOREACH(dlc, &ls->rs_dlcs, rd_next) {
				if (dlc->rd_laddr.bt_channel == chan)
					any = dlc;
			}
		}
	}

	dlc = best ? best : any;

	/* XXX
	 * Note that if this fails, we could have missed a chance to open
	 * a connection - really need to rewrite the strategy for storing
	 * listening DLC's so all can be checked in turn..
	 */
	if (dlc != NULL)
		new = (*dlc->rd_proto->newconn)(dlc->rd_upper, &laddr, &raddr);

	if (new == NULL) {
		rfcomm_session_send_frame(rs, RFCOMM_FRAME_DM, dlci);
		return NULL;
	}

	new->rd_dlci = dlci;
	new->rd_mtu = rfcomm_mtu_default;
	new->rd_mode = dlc->rd_mode;

	memcpy(&new->rd_laddr, &laddr, sizeof(struct sockaddr_bt));
	new->rd_laddr.bt_channel = chan;

	memcpy(&new->rd_raddr, &raddr, sizeof(struct sockaddr_bt));
	new->rd_raddr.bt_channel = chan;

	new->rd_session = rs;
	new->rd_state = RFCOMM_DLC_WAIT_CONNECT;
	LIST_INSERT_HEAD(&rs->rs_dlcs, new, rd_next);

	return new;
}

/*
 * rfcomm_dlc_close(dlc, error)
 *
 * detach DLC from session and clean up
 */
void
rfcomm_dlc_close(struct rfcomm_dlc *dlc, int err)
{
	struct rfcomm_session *rs;
	struct rfcomm_credit *credit;

	KASSERT(dlc->rd_state != RFCOMM_DLC_CLOSED);

	/* Clear credit history */
	rs = dlc->rd_session;
	SIMPLEQ_FOREACH(credit, &rs->rs_credits, rc_next)
		if (credit->rc_dlc == dlc)
			credit->rc_dlc = NULL;

	timeout_del(&dlc->rd_timeout);

	LIST_REMOVE(dlc, rd_next);
	dlc->rd_session = NULL;
	dlc->rd_state = RFCOMM_DLC_CLOSED;

	(*dlc->rd_proto->disconnected)(dlc->rd_upper, err);

	/*
	 * It is the responsibility of the party who sends the last
	 * DISC(dlci) to disconnect the session, but we will schedule
	 * an expiry just in case that doesnt happen..
	 */
	if (LIST_EMPTY(&rs->rs_dlcs)) {
		if (rs->rs_state == RFCOMM_SESSION_LISTEN)
			rfcomm_session_free(rs);
		else
			timeout_add_sec(&rs->rs_timeout, rfcomm_ack_timeout);
	}
}

/*
 * rfcomm_dlc_timeout(dlc)
 *
 * DLC timeout function is schedUled when we sent any of SABM,
 * DISC, MCC_MSC, or MCC_PN and should be cancelled when we get
 * the relevant response. There is nothing to do but shut this
 * DLC down.
 */
void
rfcomm_dlc_timeout(void *arg)
{
	struct rfcomm_dlc *dlc = arg;

	mutex_enter(&bt_lock);

	if (dlc->rd_state != RFCOMM_DLC_CLOSED)
		rfcomm_dlc_close(dlc, ETIMEDOUT);
	else if (dlc->rd_flags & RFCOMM_DLC_DETACH)
		free(dlc, M_BLUETOOTH);

	mutex_exit(&bt_lock);
}

/*
 * rfcomm_dlc_setmode(rfcomm_dlc)
 *
 * Set link mode for DLC.  This is only called when the session is
 * already open, so we don't need to worry about any previous mode
 * settings.
 */
int
rfcomm_dlc_setmode(struct rfcomm_dlc *dlc)
{
	int mode = 0;

	KASSERT(dlc->rd_session != NULL);
	KASSERT(dlc->rd_session->rs_state == RFCOMM_SESSION_OPEN);

	DPRINTF("dlci %d, auth %s, encrypt %s, secure %s\n", dlc->rd_dlci,
		(dlc->rd_mode & RFCOMM_LM_AUTH ? "yes" : "no"),
		(dlc->rd_mode & RFCOMM_LM_ENCRYPT ? "yes" : "no"),
		(dlc->rd_mode & RFCOMM_LM_SECURE ? "yes" : "no"));

	if (dlc->rd_mode & RFCOMM_LM_AUTH)
		mode |= L2CAP_LM_AUTH;

	if (dlc->rd_mode & RFCOMM_LM_ENCRYPT)
		mode |= L2CAP_LM_ENCRYPT;

	if (dlc->rd_mode & RFCOMM_LM_SECURE)
		mode |= L2CAP_LM_SECURE;

	return l2cap_setlinkmode(dlc->rd_session->rs_l2cap, mode);
}

/*
 * rfcomm_dlc_connect(rfcomm_dlc)
 *
 * initiate DLC connection (session is already connected)
 */
int
rfcomm_dlc_connect(struct rfcomm_dlc *dlc)
{
	struct rfcomm_mcc_pn pn;
	int err = 0;

	KASSERT(dlc->rd_session != NULL);
	KASSERT(dlc->rd_session->rs_state == RFCOMM_SESSION_OPEN);
	KASSERT(dlc->rd_state == RFCOMM_DLC_WAIT_SESSION);

	/*
	 * If we have not already sent a PN on the session, we must send
	 * a PN to negotiate Credit Flow Control, and this setting will
	 * apply to all future connections for this session. We ask for
	 * this every time, in order to establish initial credits.
	 */
	memset(&pn, 0, sizeof(pn));
	pn.dlci = dlc->rd_dlci;
	pn.priority = dlc->rd_dlci | 0x07;
	pn.mtu = htole16(dlc->rd_mtu);

	pn.flow_control = 0xf0;
	dlc->rd_rxcred = (dlc->rd_rxsize / dlc->rd_mtu);
	dlc->rd_rxcred = min(dlc->rd_rxcred, RFCOMM_CREDITS_DEFAULT);
	pn.credits = dlc->rd_rxcred;

	err = rfcomm_session_send_mcc(dlc->rd_session, 1,
					RFCOMM_MCC_PN, &pn, sizeof(pn));
	if (err)
		return err;

	dlc->rd_state = RFCOMM_DLC_WAIT_CONNECT;
	timeout_add_sec(&dlc->rd_timeout, rfcomm_mcc_timeout);

	return 0;
}

/*
 * rfcomm_dlc_open(rfcomm_dlc)
 *
 * send "Modem Status Command" and mark DLC as open.
 */
int
rfcomm_dlc_open(struct rfcomm_dlc *dlc)
{
	struct rfcomm_mcc_msc msc;
	int err;

	KASSERT(dlc->rd_session != NULL);
	KASSERT(dlc->rd_session->rs_state == RFCOMM_SESSION_OPEN);

	memset(&msc, 0, sizeof(msc));
	msc.address = RFCOMM_MKADDRESS(1, dlc->rd_dlci);
	msc.modem = dlc->rd_lmodem & 0xfe;	/* EA = 0 */
	msc.brk =	0x00	   | 0x01;	/* EA = 1 */

	err = rfcomm_session_send_mcc(dlc->rd_session, 1,
				RFCOMM_MCC_MSC, &msc, sizeof(msc));
	if (err)
		return err;

	timeout_add_sec(&dlc->rd_timeout, rfcomm_mcc_timeout);

	dlc->rd_state = RFCOMM_DLC_OPEN;
	(*dlc->rd_proto->connected)(dlc->rd_upper);

	return 0;
}

/*
 * rfcomm_dlc_start(rfcomm_dlc)
 *
 * Start sending data (and/or credits) for DLC. Our strategy is to
 * send anything we can down to the l2cap layer. When credits run
 * out, data will naturally bunch up. When not using credit flow
 * control, we limit the number of packets we have pending to reduce
 * flow control lag.
 * We should deal with channel priority somehow.
 */
void
rfcomm_dlc_start(struct rfcomm_dlc *dlc)
{
	struct rfcomm_session *rs = dlc->rd_session;
	struct mbuf *m;
	int len, credits;

	KASSERT(rs != NULL);
	KASSERT(rs->rs_state == RFCOMM_SESSION_OPEN);
	KASSERT(dlc->rd_state == RFCOMM_DLC_OPEN);

	for (;;) {
		credits = 0;
		len = dlc->rd_mtu;
		if (rs->rs_flags & RFCOMM_SESSION_CFC) {
			credits = (dlc->rd_rxsize / dlc->rd_mtu);
			credits -= dlc->rd_rxcred;
			credits = min(credits, RFCOMM_CREDITS_MAX);

			if (credits > 0)
				len--;

			if (dlc->rd_txcred == 0)
				len = 0;
		} else {
			if (rs->rs_flags & RFCOMM_SESSION_RFC)
				break;

			if (dlc->rd_rmodem & RFCOMM_MSC_FC)
				break;

			if (dlc->rd_pending > RFCOMM_CREDITS_DEFAULT)
				break;
		}

		if (dlc->rd_txbuf == NULL)
			len = 0;

		if (len == 0) {
			if (credits == 0)
				break;

			/*
			 * No need to send small numbers of credits on their
			 * own unless the other end hasn't many left.
			 */
			if (credits < RFCOMM_CREDITS_DEFAULT
			    && dlc->rd_rxcred > RFCOMM_CREDITS_DEFAULT)
				break;

			m = NULL;
		} else {
			/*
			 * take what data we can from (front of) txbuf
			 */
			m = dlc->rd_txbuf;
			if (len < m->m_pkthdr.len) {
				dlc->rd_txbuf = m_split(m, len, M_DONTWAIT);
				if (dlc->rd_txbuf == NULL) {
					dlc->rd_txbuf = m;
					break;
				}
			} else {
				dlc->rd_txbuf = NULL;
				len = m->m_pkthdr.len;
			}
		}

		DPRINTFN(10, "dlci %d send %d bytes, %d credits, rxcred = %d\n",
			dlc->rd_dlci, len, credits, dlc->rd_rxcred);

		if (rfcomm_session_send_uih(rs, dlc, credits, m)) {
			printf("%s: lost %d bytes on DLCI %d\n",
				__func__, len, dlc->rd_dlci);

			break;
		}

		dlc->rd_pending++;

		if (rs->rs_flags & RFCOMM_SESSION_CFC) {
			if (len > 0)
				dlc->rd_txcred--;

			if (credits > 0)
				dlc->rd_rxcred += credits;
		}
	}
}
