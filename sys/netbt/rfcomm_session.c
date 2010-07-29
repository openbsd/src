/*	$OpenBSD: rfcomm_session.c,v 1.8 2010/07/29 14:40:47 blambert Exp $	*/
/*	$NetBSD: rfcomm_session.c,v 1.14 2008/08/06 15:01:24 plunky Exp $	*/

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
#include <sys/pool.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/types.h>

#include <netbt/bluetooth.h>
#include <netbt/hci.h>
#include <netbt/l2cap.h>
#include <netbt/rfcomm.h>

/******************************************************************************
 *
 * RFCOMM Multiplexer Sessions sit directly on L2CAP channels, and can
 * multiplex up to 30 incoming and 30 outgoing connections.
 * Only one Multiplexer is allowed between any two devices.
 */

static void rfcomm_session_timeout(void *);
static void rfcomm_session_recv_sabm(struct rfcomm_session *, int);
static void rfcomm_session_recv_disc(struct rfcomm_session *, int);
static void rfcomm_session_recv_ua(struct rfcomm_session *, int);
static void rfcomm_session_recv_dm(struct rfcomm_session *, int);
static void rfcomm_session_recv_uih(struct rfcomm_session *, int, int, struct mbuf *, int);
static void rfcomm_session_recv_mcc(struct rfcomm_session *, struct mbuf *);
static void rfcomm_session_recv_mcc_test(struct rfcomm_session *, int, struct mbuf *);
static void rfcomm_session_recv_mcc_fcon(struct rfcomm_session *, int);
static void rfcomm_session_recv_mcc_fcoff(struct rfcomm_session *, int);
static void rfcomm_session_recv_mcc_msc(struct rfcomm_session *, int, struct mbuf *);
static void rfcomm_session_recv_mcc_rpn(struct rfcomm_session *, int, struct mbuf *);
static void rfcomm_session_recv_mcc_rls(struct rfcomm_session *, int, struct mbuf *);
static void rfcomm_session_recv_mcc_pn(struct rfcomm_session *, int, struct mbuf *);
static void rfcomm_session_recv_mcc_nsc(struct rfcomm_session *, int, struct mbuf *);

/* L2CAP callbacks */
static void rfcomm_session_connecting(void *);
static void rfcomm_session_connected(void *);
static void rfcomm_session_disconnected(void *, int);
static void *rfcomm_session_newconn(void *, struct sockaddr_bt *, struct sockaddr_bt *);
static void rfcomm_session_complete(void *, int);
static void rfcomm_session_linkmode(void *, int);
static void rfcomm_session_input(void *, struct mbuf *);

static const struct btproto rfcomm_session_proto = {
	rfcomm_session_connecting,
	rfcomm_session_connected,
	rfcomm_session_disconnected,
	rfcomm_session_newconn,
	rfcomm_session_complete,
	rfcomm_session_linkmode,
	rfcomm_session_input,
};

struct rfcomm_session_list
	rfcomm_session_active = LIST_HEAD_INITIALIZER(rfcomm_session_active);

struct rfcomm_session_list
	rfcomm_session_listen = LIST_HEAD_INITIALIZER(rfcomm_session_listen);

struct pool rfcomm_credit_pool;

/*
 * RFCOMM System Parameters (see section 5.3)
 */
int rfcomm_mtu_default = 127;	/* bytes */
int rfcomm_ack_timeout = 20;	/* seconds */
int rfcomm_mcc_timeout = 20;	/* seconds */

/*
 * Reversed CRC table as per TS 07.10 Annex B.3.5
 */
static const uint8_t crctable[256] = {	/* reversed, 8-bit, poly=0x07 */
	0x00, 0x91, 0xe3, 0x72, 0x07, 0x96, 0xe4, 0x75,
	0x0e, 0x9f, 0xed, 0x7c, 0x09, 0x98, 0xea, 0x7b,
	0x1c, 0x8d, 0xff, 0x6e, 0x1b, 0x8a, 0xf8, 0x69,
	0x12, 0x83, 0xf1, 0x60, 0x15, 0x84, 0xf6, 0x67,

	0x38, 0xa9, 0xdb, 0x4a, 0x3f, 0xae, 0xdc, 0x4d,
	0x36, 0xa7, 0xd5, 0x44, 0x31, 0xa0, 0xd2, 0x43,
	0x24, 0xb5, 0xc7, 0x56, 0x23, 0xb2, 0xc0, 0x51,
	0x2a, 0xbb, 0xc9, 0x58, 0x2d, 0xbc, 0xce, 0x5f,

	0x70, 0xe1, 0x93, 0x02, 0x77, 0xe6, 0x94, 0x05,
	0x7e, 0xef, 0x9d, 0x0c, 0x79, 0xe8, 0x9a, 0x0b,
	0x6c, 0xfd, 0x8f, 0x1e, 0x6b, 0xfa, 0x88, 0x19,
	0x62, 0xf3, 0x81, 0x10, 0x65, 0xf4, 0x86, 0x17,

	0x48, 0xd9, 0xab, 0x3a, 0x4f, 0xde, 0xac, 0x3d,
	0x46, 0xd7, 0xa5, 0x34, 0x41, 0xd0, 0xa2, 0x33,
	0x54, 0xc5, 0xb7, 0x26, 0x53, 0xc2, 0xb0, 0x21,
	0x5a, 0xcb, 0xb9, 0x28, 0x5d, 0xcc, 0xbe, 0x2f,

	0xe0, 0x71, 0x03, 0x92, 0xe7, 0x76, 0x04, 0x95,
	0xee, 0x7f, 0x0d, 0x9c, 0xe9, 0x78, 0x0a, 0x9b,
	0xfc, 0x6d, 0x1f, 0x8e, 0xfb, 0x6a, 0x18, 0x89,
	0xf2, 0x63, 0x11, 0x80, 0xf5, 0x64, 0x16, 0x87,

	0xd8, 0x49, 0x3b, 0xaa, 0xdf, 0x4e, 0x3c, 0xad,
	0xd6, 0x47, 0x35, 0xa4, 0xd1, 0x40, 0x32, 0xa3,
	0xc4, 0x55, 0x27, 0xb6, 0xc3, 0x52, 0x20, 0xb1,
	0xca, 0x5b, 0x29, 0xb8, 0xcd, 0x5c, 0x2e, 0xbf,

	0x90, 0x01, 0x73, 0xe2, 0x97, 0x06, 0x74, 0xe5,
	0x9e, 0x0f, 0x7d, 0xec, 0x99, 0x08, 0x7a, 0xeb,
	0x8c, 0x1d, 0x6f, 0xfe, 0x8b, 0x1a, 0x68, 0xf9,
	0x82, 0x13, 0x61, 0xf0, 0x85, 0x14, 0x66, 0xf7,

	0xa8, 0x39, 0x4b, 0xda, 0xaf, 0x3e, 0x4c, 0xdd,
	0xa6, 0x37, 0x45, 0xd4, 0xa1, 0x30, 0x42, 0xd3,
	0xb4, 0x25, 0x57, 0xc6, 0xb3, 0x22, 0x50, 0xc1,
	0xba, 0x2b, 0x59, 0xc8, 0xbd, 0x2c, 0x5e, 0xcf
};

#define FCS(f, d)	crctable[(f) ^ (d)]

/*
 * rfcomm_init()
 *
 * initialize the "credit pool".
 */
void
rfcomm_init(void)
{
	pool_init(&rfcomm_credit_pool, 0, 0, 0, 0, "rfcomm_credit", NULL);
}

/*
 * rfcomm_session_alloc(list, sockaddr)
 *
 * allocate a new session and fill in the blanks, then
 * attach session to front of specified list (active or listen)
 */
struct rfcomm_session *
rfcomm_session_alloc(struct rfcomm_session_list *list,
			struct sockaddr_bt *laddr)
{
	struct rfcomm_session *rs;
	int err;

	rs = malloc(sizeof(*rs), M_BLUETOOTH, M_NOWAIT | M_ZERO);
	if (rs == NULL)
		return NULL;

	rs->rs_state = RFCOMM_SESSION_CLOSED;

	timeout_set(&rs->rs_timeout, rfcomm_session_timeout, rs);

	SIMPLEQ_INIT(&rs->rs_credits);
	LIST_INIT(&rs->rs_dlcs);

	err = l2cap_attach(&rs->rs_l2cap, &rfcomm_session_proto, rs);
	if (err) {
		free(rs, M_BLUETOOTH);
		return NULL;
	}

	(void)l2cap_getopt(rs->rs_l2cap, SO_L2CAP_OMTU, &rs->rs_mtu);

	if (laddr->bt_psm == L2CAP_PSM_ANY)
		laddr->bt_psm = L2CAP_PSM_RFCOMM;

	(void)l2cap_bind(rs->rs_l2cap, laddr);

	LIST_INSERT_HEAD(list, rs, rs_next);

	return rs;
}

/*
 * rfcomm_session_free(rfcomm_session)
 *
 * release a session, including any cleanup
 */
void
rfcomm_session_free(struct rfcomm_session *rs)
{
	struct rfcomm_credit *credit;

	KASSERT(rs != NULL);
	KASSERT(LIST_EMPTY(&rs->rs_dlcs));

	rs->rs_state = RFCOMM_SESSION_CLOSED;

	/*
	 * If the callout is already invoked we have no way to stop it,
	 * but it will call us back right away (there are no DLC's) so
	 * not to worry.
	 */
	timeout_del(&rs->rs_timeout);
	if (timeout_triggered(&rs->rs_timeout))
		return;

	/*
	 * Take care that rfcomm_session_disconnected() doesnt call
	 * us back either as it will do if the l2cap_channel has not
	 * been closed when we detach it..
	 */
	if (rs->rs_flags & RFCOMM_SESSION_FREE)
		return;

	rs->rs_flags |= RFCOMM_SESSION_FREE;

	/* throw away any remaining credit notes */
	while ((credit = SIMPLEQ_FIRST(&rs->rs_credits)) != NULL) {
		SIMPLEQ_REMOVE_HEAD(&rs->rs_credits, rc_next);
		pool_put(&rfcomm_credit_pool, credit);
	}

	KASSERT(SIMPLEQ_EMPTY(&rs->rs_credits));

	/* Goodbye! */
	LIST_REMOVE(rs, rs_next);
	l2cap_detach(&rs->rs_l2cap);
	free(rs, M_BLUETOOTH);
}

/*
 * rfcomm_session_lookup(sockaddr, sockaddr)
 *
 * Find active rfcomm session matching src and dest addresses
 * when src is BDADDR_ANY match any local address
 */
struct rfcomm_session *
rfcomm_session_lookup(struct sockaddr_bt *src, struct sockaddr_bt *dest)
{
	struct rfcomm_session *rs;
	struct sockaddr_bt addr;

	LIST_FOREACH(rs, &rfcomm_session_active, rs_next) {
		if (rs->rs_state == RFCOMM_SESSION_CLOSED)
			continue;

		l2cap_sockaddr(rs->rs_l2cap, &addr);

		if (bdaddr_same(&src->bt_bdaddr, &addr.bt_bdaddr) == 0)
			if (bdaddr_any(&src->bt_bdaddr) == 0)
				continue;

		l2cap_peeraddr(rs->rs_l2cap, &addr);

		if (addr.bt_psm != dest->bt_psm)
			continue;

		if (bdaddr_same(&dest->bt_bdaddr, &addr.bt_bdaddr))
			break;
	}

	return rs;
}

/*
 * rfcomm_session_timeout(rfcomm_session)
 *
 * Session timeouts are scheduled when a session is left or
 * created with no DLCs, and when SABM(0) or DISC(0) are
 * sent.
 *
 * So, if it is in an open state with DLC's attached then
 * we leave it alone, otherwise the session is lost.
 */
static void
rfcomm_session_timeout(void *arg)
{
	struct rfcomm_session *rs = arg;
	struct rfcomm_dlc *dlc;

	KASSERT(rs != NULL);

	mutex_enter(&bt_lock);

	if (rs->rs_state != RFCOMM_SESSION_OPEN) {
		DPRINTF("timeout\n");
		rs->rs_state = RFCOMM_SESSION_CLOSED;

		while (!LIST_EMPTY(&rs->rs_dlcs)) {
			dlc = LIST_FIRST(&rs->rs_dlcs);

			rfcomm_dlc_close(dlc, ETIMEDOUT);
		}
	}

	if (LIST_EMPTY(&rs->rs_dlcs)) {
		DPRINTF("expiring\n");
		rfcomm_session_free(rs);
	}
	mutex_exit(&bt_lock);
}

/***********************************************************************
 *
 *	RFCOMM Session L2CAP protocol callbacks
 *
 */

static void
rfcomm_session_connecting(void *arg)
{
	/* struct rfcomm_session *rs = arg; */

	DPRINTF("Connecting\n");
}

static void
rfcomm_session_connected(void *arg)
{
	struct rfcomm_session *rs = arg;

	DPRINTF("Connected\n");

	/*
	 * L2CAP is open.
	 *
	 * If we are initiator, we can send our SABM(0)
	 * a timeout should be active?
	 *
	 * We must take note of the L2CAP MTU because currently
	 * the L2CAP implementation can only do Basic Mode.
	 */
	l2cap_getopt(rs->rs_l2cap, SO_L2CAP_OMTU, &rs->rs_mtu);

	rs->rs_mtu -= 6; /* (RFCOMM overhead could be this big) */
	if (rs->rs_mtu < RFCOMM_MTU_MIN) {
		rfcomm_session_disconnected(rs, EINVAL);
		return;
	}

	if (IS_INITIATOR(rs)) {
		int err;

		err = rfcomm_session_send_frame(rs, RFCOMM_FRAME_SABM, 0);
		if (err)
			rfcomm_session_disconnected(rs, err);

		timeout_add_sec(&rs->rs_timeout, rfcomm_ack_timeout);
	}
}

static void
rfcomm_session_disconnected(void *arg, int err)
{
	struct rfcomm_session *rs = arg;
	struct rfcomm_dlc *dlc;

	DPRINTF("Disconnected\n");

	rs->rs_state = RFCOMM_SESSION_CLOSED;

	while (!LIST_EMPTY(&rs->rs_dlcs)) {
		dlc = LIST_FIRST(&rs->rs_dlcs);

		rfcomm_dlc_close(dlc, err);
	}

	rfcomm_session_free(rs);
}

static void *
rfcomm_session_newconn(void *arg, struct sockaddr_bt *laddr,
				struct sockaddr_bt *raddr)
{
	struct rfcomm_session *new, *rs = arg;

	DPRINTF("New Connection\n");

	/*
	 * Incoming session connect request. We should return a new
	 * session pointer if this is acceptable. The L2CAP layer
	 * passes local and remote addresses, which we must check as
	 * only one RFCOMM session is allowed between any two devices
	 */
	new = rfcomm_session_lookup(laddr, raddr);
	if (new != NULL)
		return NULL;

	new = rfcomm_session_alloc(&rfcomm_session_active, laddr);
	if (new == NULL)
		return NULL;

	new->rs_mtu = rs->rs_mtu;
	new->rs_state = RFCOMM_SESSION_WAIT_CONNECT;

	/*
	 * schedule an expiry so that if nothing comes of it we
	 * can punt.
	 */
	timeout_add_sec(&new->rs_timeout, rfcomm_mcc_timeout);

	return new->rs_l2cap;
}

static void
rfcomm_session_complete(void *arg, int count)
{
	struct rfcomm_session *rs = arg;
	struct rfcomm_credit *credit;
	struct rfcomm_dlc *dlc;

	/*
	 * count L2CAP packets are 'complete', meaning that they are cleared
	 * our buffers (for best effort) or arrived safe (for guaranteed) so
	 * we can take it off our list and pass the message on, so that
	 * eventually the data can be removed from the sockbuf
	 */
	while (count-- > 0) {
		credit = SIMPLEQ_FIRST(&rs->rs_credits);
#ifdef DIAGNOSTIC
		if (credit == NULL) {
			printf("%s: too many packets completed!\n", __func__);
			break;
		}
#endif
		dlc = credit->rc_dlc;
		if (dlc != NULL) {
			dlc->rd_pending--;
			(*dlc->rd_proto->complete)
					(dlc->rd_upper, credit->rc_len);

			/*
			 * if not using credit flow control, we may push
			 * more data now
			 */
			if ((rs->rs_flags & RFCOMM_SESSION_CFC) == 0
			    && dlc->rd_state == RFCOMM_DLC_OPEN) {
				rfcomm_dlc_start(dlc);
			}

			/*
			 * When shutdown is indicated, we are just waiting to
			 * clear outgoing data.
			 */
			if ((dlc->rd_flags & RFCOMM_DLC_SHUTDOWN)
			    && dlc->rd_txbuf == NULL && dlc->rd_pending == 0) {
				dlc->rd_state = RFCOMM_DLC_WAIT_DISCONNECT;
				rfcomm_session_send_frame(rs, RFCOMM_FRAME_DISC,
							    dlc->rd_dlci);
				timeout_add_sec(&dlc->rd_timeout,
				    rfcomm_ack_timeout);
			}
		}

		SIMPLEQ_REMOVE_HEAD(&rs->rs_credits, rc_next);
		pool_put(&rfcomm_credit_pool, credit);
	}

	/*
	 * If session is closed, we are just waiting to clear the queue
	 */
	if (rs->rs_state == RFCOMM_SESSION_CLOSED) {
		if (SIMPLEQ_EMPTY(&rs->rs_credits))
			l2cap_disconnect(rs->rs_l2cap, 0);
	}
}

/*
 * Link Mode changed
 *
 * This is called when a mode change is complete. Proceed with connections
 * where appropriate, or pass the new mode to any active DLCs.
 */
static void
rfcomm_session_linkmode(void *arg, int new)
{
	struct rfcomm_session *rs = arg;
	struct rfcomm_dlc *dlc, *next;
	int err, mode = 0;

	DPRINTF("auth %s, encrypt %s, secure %s\n",
		(new & L2CAP_LM_AUTH ? "on" : "off"),
		(new & L2CAP_LM_ENCRYPT ? "on" : "off"),
		(new & L2CAP_LM_SECURE ? "on" : "off"));

	if (new & L2CAP_LM_AUTH)
		mode |= RFCOMM_LM_AUTH;

	if (new & L2CAP_LM_ENCRYPT)
		mode |= RFCOMM_LM_ENCRYPT;

	if (new & L2CAP_LM_SECURE)
		mode |= RFCOMM_LM_SECURE;

	next = LIST_FIRST(&rs->rs_dlcs);
	while ((dlc = next) != NULL) {
		next = LIST_NEXT(dlc, rd_next);

		switch (dlc->rd_state) {
		case RFCOMM_DLC_WAIT_SEND_SABM:	/* we are connecting */
			if ((mode & dlc->rd_mode) != dlc->rd_mode) {
				rfcomm_dlc_close(dlc, ECONNABORTED);
			} else {
				err = rfcomm_session_send_frame(rs,
					    RFCOMM_FRAME_SABM, dlc->rd_dlci);
				if (err) {
					rfcomm_dlc_close(dlc, err);
				} else {
					dlc->rd_state = RFCOMM_DLC_WAIT_RECV_UA;
					timeout_add_sec(&dlc->rd_timeout,
					    rfcomm_ack_timeout);
					break;
				}
			}

			/*
			 * If we aborted the connection and there are no more DLCs
			 * on the session, it is our responsibility to disconnect.
			 */
			if (!LIST_EMPTY(&rs->rs_dlcs))
				break;

			rs->rs_state = RFCOMM_SESSION_WAIT_DISCONNECT;
			rfcomm_session_send_frame(rs, RFCOMM_FRAME_DISC, 0);
			timeout_add_sec(&rs->rs_timeout, rfcomm_ack_timeout);
			break;

		case RFCOMM_DLC_WAIT_SEND_UA: /* they are connecting */
			if ((mode & dlc->rd_mode) != dlc->rd_mode) {
				rfcomm_session_send_frame(rs,
					    RFCOMM_FRAME_DM, dlc->rd_dlci);
				rfcomm_dlc_close(dlc, ECONNABORTED);
				break;
			}

			err = rfcomm_session_send_frame(rs,
					    RFCOMM_FRAME_UA, dlc->rd_dlci);
			if (err) {
				rfcomm_session_send_frame(rs,
						RFCOMM_FRAME_DM, dlc->rd_dlci);
				rfcomm_dlc_close(dlc, err);
				break;
			}

			err = rfcomm_dlc_open(dlc);
			if (err) {
				rfcomm_session_send_frame(rs,
						RFCOMM_FRAME_DM, dlc->rd_dlci);
				rfcomm_dlc_close(dlc, err);
				break;
			}

			break;

		case RFCOMM_DLC_WAIT_RECV_UA:
		case RFCOMM_DLC_OPEN: /* already established */
			(*dlc->rd_proto->linkmode)(dlc->rd_upper, mode);
			break;

		default:
			break;
		}
	}
}

/*
 * Receive data from L2CAP layer for session. There is always exactly one
 * RFCOMM frame contained in each L2CAP frame.
 */
static void
rfcomm_session_input(void *arg, struct mbuf *m)
{
	struct rfcomm_session *rs = arg;
	int dlci, len, type, pf;
	uint8_t fcs, b;

	KASSERT(m != NULL);
	KASSERT(rs != NULL);

	/*
	 * UIH frames: FCS is only calculated on address and control fields
	 * For other frames: FCS is calculated on address, control and length
	 * Length may extend to two octets
	 */
	fcs = 0xff;

	if (m->m_pkthdr.len < 4) {
		DPRINTF("short frame (%d), discarded\n", m->m_pkthdr.len);
		goto done;
	}

	/* address - one octet */
	m_copydata(m, 0, 1, &b);
	m_adj(m, 1);
	fcs = FCS(fcs, b);
	dlci = RFCOMM_DLCI(b);

	/* control - one octet */
	m_copydata(m, 0, 1, &b);
	m_adj(m, 1);
	fcs = FCS(fcs, b);
	type = RFCOMM_TYPE(b);
	pf = RFCOMM_PF(b);

	/* length - may be two octets */
	m_copydata(m, 0, 1, &b);
	m_adj(m, 1);
	if (type != RFCOMM_FRAME_UIH)
		fcs = FCS(fcs, b);
	len = (b >> 1) & 0x7f;

	if (RFCOMM_EA(b) == 0) {
		if (m->m_pkthdr.len < 2) {
			DPRINTF("short frame (%d, EA = 0), discarded\n",
				m->m_pkthdr.len);
			goto done;
		}

		m_copydata(m, 0, 1, &b);
		m_adj(m, 1);
		if (type != RFCOMM_FRAME_UIH)
			fcs = FCS(fcs, b);

		len |= (b << 7);
	}

	/* FCS byte is last octet in frame */
	m_copydata(m, m->m_pkthdr.len - 1, 1, &b);
	m_adj(m, -1);
	fcs = FCS(fcs, b);

	if (fcs != 0xcf) {
		DPRINTF("Bad FCS value (%#2.2x), frame discarded\n", fcs);
		goto done;
	}

	DPRINTFN(10, "dlci %d, type %2.2x, len = %d\n", dlci, type, len);

	switch (type) {
	case RFCOMM_FRAME_SABM:
		if (pf)
			rfcomm_session_recv_sabm(rs, dlci);
		break;

	case RFCOMM_FRAME_DISC:
		if (pf)
			rfcomm_session_recv_disc(rs, dlci);
		break;

	case RFCOMM_FRAME_UA:
		if (pf)
			rfcomm_session_recv_ua(rs, dlci);
		break;

	case RFCOMM_FRAME_DM:
		rfcomm_session_recv_dm(rs, dlci);
		break;

	case RFCOMM_FRAME_UIH:
		rfcomm_session_recv_uih(rs, dlci, pf, m, len);
		return;	/* (no release) */

	default:
		UNKNOWN(type);
		break;
	}

done:
	m_freem(m);
}

/***********************************************************************
 *
 *	RFCOMM Session receive processing
 */

/*
 * rfcomm_session_recv_sabm(rfcomm_session, dlci)
 *
 * Set Asyncrhonous Balanced Mode - open the channel.
 */
static void
rfcomm_session_recv_sabm(struct rfcomm_session *rs, int dlci)
{
	struct rfcomm_dlc *dlc;
	int err;

	DPRINTFN(5, "SABM(%d)\n", dlci);

	if (dlci == 0) {	/* Open Session */
		rs->rs_state = RFCOMM_SESSION_OPEN;
		rfcomm_session_send_frame(rs, RFCOMM_FRAME_UA, 0);
		LIST_FOREACH(dlc, &rs->rs_dlcs, rd_next) {
			if (dlc->rd_state == RFCOMM_DLC_WAIT_SESSION)
				rfcomm_dlc_connect(dlc);
		}
		return;
	}

	if (rs->rs_state != RFCOMM_SESSION_OPEN) {
		DPRINTF("session was not even open!\n");
		return;
	}

	/* validate direction bit */
	if ((IS_INITIATOR(rs) && !RFCOMM_DIRECTION(dlci))
	    || (!IS_INITIATOR(rs) && RFCOMM_DIRECTION(dlci))) {
		DPRINTF("Invalid direction bit on DLCI\n");
		return;
	}

	/*
	 * look for our DLC - this may exist if we received PN
	 * already, or we may have to fabricate a new one.
	 */
	dlc = rfcomm_dlc_lookup(rs, dlci);
	if (dlc == NULL) {
		dlc = rfcomm_dlc_newconn(rs, dlci);
		if (dlc == NULL)
			return;	/* (DM is sent) */
	}

	/*
	 * ..but if this DLC is not waiting to connect, they did
	 * something wrong, ignore it.
	 */
	if (dlc->rd_state != RFCOMM_DLC_WAIT_CONNECT)
		return;

	/* set link mode */
	err = rfcomm_dlc_setmode(dlc);
	if (err == EINPROGRESS) {
		dlc->rd_state = RFCOMM_DLC_WAIT_SEND_UA;
		(*dlc->rd_proto->connecting)(dlc->rd_upper);
		return;
	}
	if (err)
		goto close;

	err = rfcomm_session_send_frame(rs, RFCOMM_FRAME_UA, dlci);
	if (err)
		goto close;

	/* and mark it open */
	err = rfcomm_dlc_open(dlc);
	if (err)
		goto close;

	return;

close:
	rfcomm_dlc_close(dlc, err);
}

/*
 * Receive Disconnect Command
 */
static void
rfcomm_session_recv_disc(struct rfcomm_session *rs, int dlci)
{
	struct rfcomm_dlc *dlc;

	DPRINTFN(5, "DISC(%d)\n", dlci);

	if (dlci == 0) {
		/*
		 * Disconnect Session
		 *
		 * We set the session state to CLOSED so that when
		 * the UA frame is clear the session will be closed
		 * automatically. We wont bother to close any DLC's
		 * just yet as there should be none. In the unlikely
		 * event that something is left, it will get flushed
		 * out as the session goes down.
		 */
		rfcomm_session_send_frame(rs, RFCOMM_FRAME_UA, 0);
		rs->rs_state = RFCOMM_SESSION_CLOSED;
		return;
	}

	dlc = rfcomm_dlc_lookup(rs, dlci);
	if (dlc == NULL) {
		rfcomm_session_send_frame(rs, RFCOMM_FRAME_DM, dlci);
		return;
	}

	rfcomm_dlc_close(dlc, ECONNRESET);
	rfcomm_session_send_frame(rs, RFCOMM_FRAME_UA, dlci);
}

/*
 * Receive Unnumbered Acknowledgement Response
 *
 * This should be a response to a DISC or SABM frame that we
 * have previously sent. If unexpected, ignore it.
 */
static void
rfcomm_session_recv_ua(struct rfcomm_session *rs, int dlci)
{
	struct rfcomm_dlc *dlc;

	DPRINTFN(5, "UA(%d)\n", dlci);

	if (dlci == 0) {
		switch (rs->rs_state) {
		case RFCOMM_SESSION_WAIT_CONNECT:	/* We sent SABM */
			timeout_del(&rs->rs_timeout);
			rs->rs_state = RFCOMM_SESSION_OPEN;
			LIST_FOREACH(dlc, &rs->rs_dlcs, rd_next) {
				if (dlc->rd_state == RFCOMM_DLC_WAIT_SESSION)
					rfcomm_dlc_connect(dlc);
			}
			break;

		case RFCOMM_SESSION_WAIT_DISCONNECT:	/* We sent DISC */
			timeout_del(&rs->rs_timeout);
			rs->rs_state = RFCOMM_SESSION_CLOSED;
			l2cap_disconnect(rs->rs_l2cap, 0);
			break;

		default:
			DPRINTF("Received spurious UA(0)!\n");
			break;
		}

		return;
	}

	/*
	 * If we have no DLC on this dlci, we may have aborted
	 * without shutting down properly, so check if the session
	 * needs disconnecting.
	 */
	dlc = rfcomm_dlc_lookup(rs, dlci);
	if (dlc == NULL)
		goto check;

	switch (dlc->rd_state) {
	case RFCOMM_DLC_WAIT_RECV_UA:		/* We sent SABM */
		rfcomm_dlc_open(dlc);
		return;

	case RFCOMM_DLC_WAIT_DISCONNECT:	/* We sent DISC */
		rfcomm_dlc_close(dlc, 0);
		break;

	default:
		DPRINTF("Received spurious UA(%d)!\n", dlci);
		return;
	}

check:	/* last one out turns out the light */
	if (LIST_EMPTY(&rs->rs_dlcs)) {
		rs->rs_state = RFCOMM_SESSION_WAIT_DISCONNECT;
		rfcomm_session_send_frame(rs, RFCOMM_FRAME_DISC, 0);
		timeout_add_sec(&rs->rs_timeout, rfcomm_ack_timeout);
	}
}

/*
 * Receive Disconnected Mode Response
 *
 * If this does not apply to a known DLC then we may ignore it.
 */
static void
rfcomm_session_recv_dm(struct rfcomm_session *rs, int dlci)
{
	struct rfcomm_dlc *dlc;

	DPRINTFN(5, "DM(%d)\n", dlci);

	dlc = rfcomm_dlc_lookup(rs, dlci);
	if (dlc == NULL)
		return;

	if (dlc->rd_state == RFCOMM_DLC_WAIT_CONNECT)
		rfcomm_dlc_close(dlc, ECONNREFUSED);
	else
		rfcomm_dlc_close(dlc, ECONNRESET);
}

/*
 * Receive Unnumbered Information with Header check (MCC or data packet)
 */
static void
rfcomm_session_recv_uih(struct rfcomm_session *rs, int dlci,
			int pf, struct mbuf *m, int len)
{
	struct rfcomm_dlc *dlc;
	uint8_t credits = 0;

	DPRINTFN(10, "UIH(%d)\n", dlci);

	if (dlci == 0) {
		rfcomm_session_recv_mcc(rs, m);
		return;
	}

	if (m->m_pkthdr.len != len + pf) {
		DPRINTF("Bad Frame Length (%d), frame discarded\n",
			    m->m_pkthdr.len);

		goto discard;
	}

	dlc = rfcomm_dlc_lookup(rs, dlci);
	if (dlc == NULL) {
		DPRINTF("UIH received for non existent DLC, discarded\n");
		rfcomm_session_send_frame(rs, RFCOMM_FRAME_DM, dlci);
		goto discard;
	}

	if (dlc->rd_state != RFCOMM_DLC_OPEN) {
		DPRINTF("non-open DLC (state = %d), discarded\n",
				dlc->rd_state);
		goto discard;
	}

	/* if PF is set, credits were included */
	if (rs->rs_flags & RFCOMM_SESSION_CFC) {
		if (pf != 0) {
			if (m->m_pkthdr.len < sizeof(credits)) {
				DPRINTF("Bad PF value, UIH discarded\n");
				goto discard;
			}

			m_copydata(m, 0, sizeof(credits), &credits);
			m_adj(m, sizeof(credits));

			dlc->rd_txcred += credits;

			if (credits > 0 && dlc->rd_txbuf != NULL)
				rfcomm_dlc_start(dlc);
		}

		if (len == 0)
			goto discard;

		if (dlc->rd_rxcred == 0) {
			DPRINTF("Credit limit reached, UIH discarded\n");
			goto discard;
		}

		if (len > dlc->rd_rxsize) {
			DPRINTF("UIH frame exceeds rxsize, discarded\n");
			goto discard;
		}

		dlc->rd_rxcred--;
		dlc->rd_rxsize -= len;
	}

	(*dlc->rd_proto->input)(dlc->rd_upper, m);
	return;

discard:
	m_freem(m);
}

/*
 * Receive Multiplexer Control Command
 */
static void
rfcomm_session_recv_mcc(struct rfcomm_session *rs, struct mbuf *m)
{
	int type, cr, len;
	uint8_t b;

	/*
	 * Extract MCC header.
	 *
	 * Fields are variable length using extension bit = 1 to signify the
	 * last octet in the sequence.
	 *
	 * Only single octet types are defined in TS 07.10/RFCOMM spec
	 *
	 * Length can realistically only use 15 bits (max RFCOMM MTU)
	 */
	if (m->m_pkthdr.len < sizeof(b)) {
		DPRINTF("Short MCC header, discarded\n");
		goto release;
	}

	m_copydata(m, 0, sizeof(b), &b);
	m_adj(m, sizeof(b));

	if (RFCOMM_EA(b) == 0) {	/* verify no extensions */
		DPRINTF("MCC type EA = 0, discarded\n");
		goto release;
	}

	type = RFCOMM_MCC_TYPE(b);
	cr = RFCOMM_CR(b);

	len = 0;
	do {
		if (m->m_pkthdr.len < sizeof(b)) {
			DPRINTF("Short MCC header, discarded\n");
			goto release;
		}

		m_copydata(m, 0, sizeof(b), &b);
		m_adj(m, sizeof(b));

		len = (len << 7) | (b >> 1);
		len = min(len, RFCOMM_MTU_MAX);
	} while (RFCOMM_EA(b) == 0);

	if (len != m->m_pkthdr.len) {
		DPRINTF("Incorrect MCC length, discarded\n");
		goto release;
	}

	DPRINTFN(2, "MCC %s type %2.2x (%d bytes)\n",
		(cr ? "command" : "response"), type, len);

	/*
	 * pass to command handler
	 */
	switch(type) {
	case RFCOMM_MCC_TEST:	/* Test */
		rfcomm_session_recv_mcc_test(rs, cr, m);
		break;

	case RFCOMM_MCC_FCON:	/* Flow Control On */
		rfcomm_session_recv_mcc_fcon(rs, cr);
		break;

	case RFCOMM_MCC_FCOFF:	/* Flow Control Off */
		rfcomm_session_recv_mcc_fcoff(rs, cr);
		break;

	case RFCOMM_MCC_MSC:	/* Modem Status Command */
		rfcomm_session_recv_mcc_msc(rs, cr, m);
		break;

	case RFCOMM_MCC_RPN:	/* Remote Port Negotiation */
		rfcomm_session_recv_mcc_rpn(rs, cr, m);
		break;

	case RFCOMM_MCC_RLS:	/* Remote Line Status */
		rfcomm_session_recv_mcc_rls(rs, cr, m);
		break;

	case RFCOMM_MCC_PN:	/* Parameter Negotiation */
		rfcomm_session_recv_mcc_pn(rs, cr, m);
		break;

	case RFCOMM_MCC_NSC:	/* Non Supported Command */
		rfcomm_session_recv_mcc_nsc(rs, cr, m);
		break;

	default:
		b = RFCOMM_MKMCC_TYPE(cr, type);
		rfcomm_session_send_mcc(rs, 0, RFCOMM_MCC_NSC, &b, sizeof(b));
	}

release:
	m_freem(m);
}

/*
 * process TEST command/response
 */
static void
rfcomm_session_recv_mcc_test(struct rfcomm_session *rs, int cr, struct mbuf *m)
{
	void *data;
	int len;

	if (cr == 0)	/* ignore ack */
		return;

	/*
	 * we must send all the data they included back as is
	 */

	len = m->m_pkthdr.len;
	if (len > RFCOMM_MTU_MAX)
		return;

	data = malloc(len, M_BLUETOOTH, M_NOWAIT);
	if (data == NULL)
		return;

	m_copydata(m, 0, len, data);
	rfcomm_session_send_mcc(rs, 0, RFCOMM_MCC_TEST, data, len);
	free(data, M_BLUETOOTH);
}

/*
 * process Flow Control ON command/response
 */
static void
rfcomm_session_recv_mcc_fcon(struct rfcomm_session *rs, int cr)
{

	if (cr == 0)	/* ignore ack */
		return;

	rs->rs_flags |= RFCOMM_SESSION_RFC;
	rfcomm_session_send_mcc(rs, 0, RFCOMM_MCC_FCON, NULL, 0);
}

/*
 * process Flow Control OFF command/response
 */
static void
rfcomm_session_recv_mcc_fcoff(struct rfcomm_session *rs, int cr)
{

	if (cr == 0)	/* ignore ack */
		return;

	rs->rs_flags &= ~RFCOMM_SESSION_RFC;
	rfcomm_session_send_mcc(rs, 0, RFCOMM_MCC_FCOFF, NULL, 0);
}

/*
 * process Modem Status Command command/response
 */
static void
rfcomm_session_recv_mcc_msc(struct rfcomm_session *rs, int cr, struct mbuf *m)
{
	struct rfcomm_mcc_msc msc;	/* (3 octets) */
	struct rfcomm_dlc *dlc;
	int len = 0;

	/* [ADDRESS] */
	if (m->m_pkthdr.len < sizeof(msc.address))
		return;

	m_copydata(m, 0, sizeof(msc.address), &msc.address);
	m_adj(m, sizeof(msc.address));
	len += sizeof(msc.address);

	dlc = rfcomm_dlc_lookup(rs, RFCOMM_DLCI(msc.address));

	if (cr == 0) {	/* ignore acks */
		if (dlc != NULL)
			timeout_del(&dlc->rd_timeout);

		return;
	}

	if (dlc == NULL) {
		rfcomm_session_send_frame(rs, RFCOMM_FRAME_DM,
						RFCOMM_DLCI(msc.address));
		return;
	}

	/* [SIGNALS] */
	if (m->m_pkthdr.len < sizeof(msc.modem))
		return;

	m_copydata(m, 0, sizeof(msc.modem), &msc.modem);
	m_adj(m, sizeof(msc.modem));
	len += sizeof(msc.modem);

	dlc->rd_rmodem = msc.modem;
	/* XXX how do we signal this upstream? */

	if (RFCOMM_EA(msc.modem) == 0) {
		if (m->m_pkthdr.len < sizeof(msc.brk))
			return;

		m_copydata(m, 0, sizeof(msc.brk), &msc.brk);
		m_adj(m, sizeof(msc.brk));
		len += sizeof(msc.brk);

		/* XXX how do we signal this upstream? */
	}

	rfcomm_session_send_mcc(rs, 0, RFCOMM_MCC_MSC, &msc, len);
}

/*
 * process Remote Port Negotiation command/response
 */
static void
rfcomm_session_recv_mcc_rpn(struct rfcomm_session *rs, int cr, struct mbuf *m)
{
	struct rfcomm_mcc_rpn rpn;
	uint16_t mask;

	if (cr == 0)	/* ignore ack */
		return;

	/* default values */
	rpn.bit_rate = RFCOMM_RPN_BR_9600;
	rpn.line_settings = RFCOMM_RPN_8_N_1;
	rpn.flow_control = RFCOMM_RPN_FLOW_NONE;
	rpn.xon_char = RFCOMM_RPN_XON_CHAR;
	rpn.xoff_char = RFCOMM_RPN_XOFF_CHAR;

	if (m->m_pkthdr.len == sizeof(rpn)) {
		m_copydata(m, 0, sizeof(rpn), (caddr_t)&rpn);
		rpn.param_mask = RFCOMM_RPN_PM_ALL;
	} else if (m->m_pkthdr.len == 1) {
		m_copydata(m, 0, 1, (caddr_t)&rpn);
		rpn.param_mask = letoh16(rpn.param_mask);
	} else {
		DPRINTF("Bad RPN length (%d)\n", m->m_pkthdr.len);
		return;
	}

	mask = 0;

	if (rpn.param_mask & RFCOMM_RPN_PM_RATE)
		mask |= RFCOMM_RPN_PM_RATE;

	if (rpn.param_mask & RFCOMM_RPN_PM_DATA
	    && RFCOMM_RPN_DATA_BITS(rpn.line_settings) == RFCOMM_RPN_DATA_8)
		mask |= RFCOMM_RPN_PM_DATA;

	if (rpn.param_mask & RFCOMM_RPN_PM_STOP
	    && RFCOMM_RPN_STOP_BITS(rpn.line_settings) == RFCOMM_RPN_STOP_1)
		mask |= RFCOMM_RPN_PM_STOP;

	if (rpn.param_mask & RFCOMM_RPN_PM_PARITY
	    && RFCOMM_RPN_PARITY(rpn.line_settings) == RFCOMM_RPN_PARITY_NONE)
		mask |= RFCOMM_RPN_PM_PARITY;

	if (rpn.param_mask & RFCOMM_RPN_PM_XON
	    && rpn.xon_char == RFCOMM_RPN_XON_CHAR)
		mask |= RFCOMM_RPN_PM_XON;

	if (rpn.param_mask & RFCOMM_RPN_PM_XOFF
	    && rpn.xoff_char == RFCOMM_RPN_XOFF_CHAR)
		mask |= RFCOMM_RPN_PM_XOFF;

	if (rpn.param_mask & RFCOMM_RPN_PM_FLOW
	    && rpn.flow_control == RFCOMM_RPN_FLOW_NONE)
		mask |= RFCOMM_RPN_PM_FLOW;

	rpn.param_mask = htole16(mask);

	rfcomm_session_send_mcc(rs, 0, RFCOMM_MCC_RPN, &rpn, sizeof(rpn));
}

/*
 * process Remote Line Status command/response
 */
static void
rfcomm_session_recv_mcc_rls(struct rfcomm_session *rs, int cr, struct mbuf *m)
{
	struct rfcomm_mcc_rls rls;

	if (cr == 0)	/* ignore ack */
		return;

	if (m->m_pkthdr.len != sizeof(rls)) {
		DPRINTF("Bad RLS length %d\n", m->m_pkthdr.len);
		return;
	}

	m_copydata(m, 0, sizeof(rls), (caddr_t)&rls);

	/*
	 * So far as I can tell, we just send back what
	 * they sent us. This signifies errors that seem
	 * irrelevent for RFCOMM over L2CAP.
	 */
	rls.address |= 0x03;	/* EA = 1, CR = 1 */
	rls.status &= 0x0f;	/* only 4 bits valid */

	rfcomm_session_send_mcc(rs, 0, RFCOMM_MCC_RLS, &rls, sizeof(rls));
}

/*
 * process Parameter Negotiation command/response
 */
static void
rfcomm_session_recv_mcc_pn(struct rfcomm_session *rs, int cr, struct mbuf *m)
{
	struct rfcomm_dlc *dlc;
	struct rfcomm_mcc_pn pn;
	int err;

	if (m->m_pkthdr.len != sizeof(pn)) {
		DPRINTF("Bad PN length %d\n", m->m_pkthdr.len);
		return;
	}

	m_copydata(m, 0, sizeof(pn), (caddr_t)&pn);

	pn.dlci &= 0x3f;
	pn.mtu = letoh16(pn.mtu);

	dlc = rfcomm_dlc_lookup(rs, pn.dlci);
	if (cr) {	/* Command */
		/*
		 * If there is no DLC present, this is a new
		 * connection so attempt to make one
		 */
		if (dlc == NULL) {
			dlc = rfcomm_dlc_newconn(rs, pn.dlci);
			if (dlc == NULL)
				return;	/* (DM is sent) */
		}

		/* accept any valid MTU, and offer it back */
		pn.mtu = min(pn.mtu, RFCOMM_MTU_MAX);
		pn.mtu = min(pn.mtu, rs->rs_mtu);
		pn.mtu = max(pn.mtu, RFCOMM_MTU_MIN);
		dlc->rd_mtu = pn.mtu;
		pn.mtu = htole16(pn.mtu);

		/* credits are only set before DLC is open */
		if (dlc->rd_state == RFCOMM_DLC_WAIT_CONNECT
		    && (pn.flow_control & 0xf0) == 0xf0) {
			rs->rs_flags |= RFCOMM_SESSION_CFC;
			dlc->rd_txcred = pn.credits & 0x07;

			dlc->rd_rxcred = (dlc->rd_rxsize / dlc->rd_mtu);
			dlc->rd_rxcred = min(dlc->rd_rxcred,
						RFCOMM_CREDITS_DEFAULT);

			pn.flow_control = 0xe0;
			pn.credits = dlc->rd_rxcred;
		} else {
			pn.flow_control = 0x00;
			pn.credits = 0x00;
		}

		/* unused fields must be ignored and set to zero */
		pn.ack_timer = 0;
		pn.max_retrans = 0;

		/* send our response */
		err = rfcomm_session_send_mcc(rs, 0,
					RFCOMM_MCC_PN, &pn, sizeof(pn));
		if (err)
			goto close;

	} else {	/* Response */
		/* ignore responses with no matching DLC */
		if (dlc == NULL)
			return;

		timeout_del(&dlc->rd_timeout);

		if (pn.mtu > RFCOMM_MTU_MAX || pn.mtu > dlc->rd_mtu) {
			dlc->rd_state = RFCOMM_DLC_WAIT_DISCONNECT;
			err = rfcomm_session_send_frame(rs, RFCOMM_FRAME_DISC,
							pn.dlci);
			if (err)
				goto close;

			timeout_add_sec(&dlc->rd_timeout, rfcomm_ack_timeout);
			return;
		}
		dlc->rd_mtu = pn.mtu;

		/* if DLC is not waiting to connect, we are done */
		if (dlc->rd_state != RFCOMM_DLC_WAIT_CONNECT)
			return;

		/* set initial credits according to RFCOMM spec */
		if ((pn.flow_control & 0xf0) == 0xe0) {
			rs->rs_flags |= RFCOMM_SESSION_CFC;
			dlc->rd_txcred = (pn.credits & 0x07);
		}

		timeout_add_sec(&dlc->rd_timeout, rfcomm_ack_timeout);

		/* set link mode */
		err = rfcomm_dlc_setmode(dlc);
		if (err == EINPROGRESS) {
			dlc->rd_state = RFCOMM_DLC_WAIT_SEND_SABM;
			(*dlc->rd_proto->connecting)(dlc->rd_upper);
			return;
		}
		if (err)
			goto close;

		/* we can proceed now */
		err = rfcomm_session_send_frame(rs, RFCOMM_FRAME_SABM, pn.dlci);
		if (err)
			goto close;

		dlc->rd_state = RFCOMM_DLC_WAIT_RECV_UA;
	}
	return;

close:
	rfcomm_dlc_close(dlc, err);
}

/*
 * process Non Supported Command command/response
 */
static void
rfcomm_session_recv_mcc_nsc(struct rfcomm_session *rs,
    int cr, struct mbuf *m)
{
	struct rfcomm_dlc *dlc, *next;

	/*
	 * Since we did nothing that is not mandatory,
	 * we just abort the whole session..
	 */

	next = LIST_FIRST(&rs->rs_dlcs);
	while ((dlc = next) != NULL) {
		next = LIST_NEXT(dlc, rd_next);
		rfcomm_dlc_close(dlc, ECONNABORTED);
	}

	rfcomm_session_free(rs);
}

/***********************************************************************
 *
 *	RFCOMM Session outward frame/uih/mcc building
 */

/*
 * SABM/DISC/DM/UA frames are all minimal and mostly identical.
 */
int
rfcomm_session_send_frame(struct rfcomm_session *rs, int type, int dlci)
{
	struct rfcomm_cmd_hdr *hdr;
	struct rfcomm_credit *credit;
	struct mbuf *m;
	uint8_t fcs, cr;

	credit = pool_get(&rfcomm_credit_pool, PR_NOWAIT);
	if (credit == NULL)
		return ENOMEM;

	m = m_gethdr(M_DONTWAIT, MT_DATA);
	if (m == NULL) {
		pool_put(&rfcomm_credit_pool, credit);
		return ENOMEM;
	}

	/*
	 * The CR (command/response) bit identifies the frame either as a
	 * commmand or a response and is used along with the DLCI to form
	 * the address. Commands contain the non-initiator address, whereas
	 * responses contain the initiator address, so the CR value is
	 * also dependent on the session direction.
	 */
	if (type == RFCOMM_FRAME_UA || type == RFCOMM_FRAME_DM)
		cr = IS_INITIATOR(rs) ? 0 : 1;
	else
		cr = IS_INITIATOR(rs) ? 1 : 0;

	hdr = mtod(m, struct rfcomm_cmd_hdr *);
	hdr->address = RFCOMM_MKADDRESS(cr, dlci);
	hdr->control = RFCOMM_MKCONTROL(type, 1);   /* PF = 1 */
	hdr->length = (0x00 << 1) | 0x01;	    /* len = 0x00, EA = 1 */

	fcs = 0xff;
	fcs = FCS(fcs, hdr->address);
	fcs = FCS(fcs, hdr->control);
	fcs = FCS(fcs, hdr->length);
	fcs = 0xff - fcs;	/* ones complement */
	hdr->fcs = fcs;

	m->m_pkthdr.len = m->m_len = sizeof(struct rfcomm_cmd_hdr);

	/* empty credit note */
	credit->rc_dlc = NULL;
	credit->rc_len = m->m_pkthdr.len;
	SIMPLEQ_INSERT_TAIL(&rs->rs_credits, credit, rc_next);

	DPRINTFN(5, "dlci %d type %2.2x (%d bytes, fcs=%#2.2x)\n",
		dlci, type, m->m_pkthdr.len, fcs);

	return l2cap_send(rs->rs_l2cap, m);
}

/*
 * rfcomm_session_send_uih(rfcomm_session, rfcomm_dlc, credits, mbuf)
 *
 * UIH frame is per DLC data or Multiplexer Control Commands
 * when no DLC is given. Data mbuf is optional (just credits
 * will be sent in that case)
 */
int
rfcomm_session_send_uih(struct rfcomm_session *rs, struct rfcomm_dlc *dlc,
			int credits, struct mbuf *m)
{
	struct rfcomm_credit *credit;
	struct mbuf *m0 = NULL;
	int err, len;
	uint8_t fcs, *hdr;

	KASSERT(rs != NULL);

	len = (m == NULL) ? 0 : m->m_pkthdr.len;
	KASSERT(!(credits == 0 && len == 0));

	/*
	 * Make a credit note for the completion notification
	 */
	credit = pool_get(&rfcomm_credit_pool, PR_NOWAIT);
	if (credit == NULL)
		goto nomem;

	credit->rc_len = len;
	credit->rc_dlc = dlc;

	/*
	 * Wrap UIH frame information around payload.
	 *
	 * [ADDRESS] [CONTROL] [LENGTH] [CREDITS] [...] [FCS]
	 *
	 * Address is one octet.
	 * Control is one octet.
	 * Length is one or two octets.
	 * Credits may be one octet.
	 *
	 * FCS is one octet and calculated on address and
	 *	control octets only.
	 *
	 * If there are credits to be sent, we will set the PF
	 * flag and include them in the frame.
	 */
	m0 = m_gethdr(M_DONTWAIT, MT_DATA);
	if (m0 == NULL)
		goto nomem;

	MH_ALIGN(m0, 5);	/* (max 5 header octets) */
	hdr = mtod(m0, uint8_t *);

	/* CR bit is set according to the initiator of the session */
	*hdr = RFCOMM_MKADDRESS((IS_INITIATOR(rs) ? 1 : 0),
				(dlc ? dlc->rd_dlci : 0));
	fcs = FCS(0xff, *hdr);
	hdr++;

	/* PF bit is set if credits are being sent */
	*hdr = RFCOMM_MKCONTROL(RFCOMM_FRAME_UIH, (credits > 0 ? 1 : 0));
	fcs = FCS(fcs, *hdr);
	hdr++;

	if (len < (1 << 7)) {
		*hdr++ = ((len << 1) & 0xfe) | 0x01;	/* 7 bits, EA = 1 */
	} else {
		*hdr++ = ((len << 1) & 0xfe);		/* 7 bits, EA = 0 */
		*hdr++ = ((len >> 7) & 0xff);		/* 8 bits, no EA */
	}

	if (credits > 0)
		*hdr++ = (uint8_t)credits;

	m0->m_len = hdr - mtod(m0, uint8_t *);

	/* Append payload */
	m0->m_next = m;
	m = NULL;

	m0->m_pkthdr.len = m0->m_len + len;

	/* Append FCS */
	fcs = 0xff - fcs;	/* ones complement */
	len = m0->m_pkthdr.len;
	m_copyback(m0, len, sizeof(fcs), &fcs, M_NOWAIT);
	if (m0->m_pkthdr.len != len + sizeof(fcs))
		goto nomem;

	DPRINTFN(10, "dlci %d, pktlen %d (%d data, %d credits), fcs=%#2.2x\n",
		dlc ? dlc->rd_dlci : 0, m0->m_pkthdr.len, credit->rc_len,
		credits, fcs);

	/*
	 * UIH frame ready to go..
	 */
	err = l2cap_send(rs->rs_l2cap, m0);
	if (err)
		goto fail;

	SIMPLEQ_INSERT_TAIL(&rs->rs_credits, credit, rc_next);
	return 0;

nomem:
	err = ENOMEM;

	if (m0 != NULL)
		m_freem(m0);

	if (m != NULL)
		m_freem(m);

fail:
	if (credit != NULL)
		pool_put(&rfcomm_credit_pool, credit);

	return err;
}

/*
 * send Multiplexer Control Command (or Response) on session
 */
int
rfcomm_session_send_mcc(struct rfcomm_session *rs, int cr,
			uint8_t type, void *data, int len)
{
	struct mbuf *m;
	uint8_t *hdr;
	int hlen;

	m = m_gethdr(M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return ENOMEM;

	hdr = mtod(m, uint8_t *);

	/*
	 * Technically the type field can extend past one octet, but none
	 * currently defined will do that.
	 */
	*hdr++ = RFCOMM_MKMCC_TYPE(cr, type);

	/*
	 * In the frame, the max length size is 2 octets (15 bits) whereas
	 * no max length size is specified for MCC commands. We must allow
	 * for 3 octets since for MCC frames we use 7 bits + EA in each.
	 *
	 * Only test data can possibly be that big.
	 *
	 * XXX Should we check this against the MTU?
	 */
	if (len < (1 << 7)) {
		*hdr++ = ((len << 1) & 0xfe) | 0x01;	/* 7 bits, EA = 1 */
	} else if (len < (1 << 14)) {
		*hdr++ = ((len << 1) & 0xfe);		/* 7 bits, EA = 0 */
		*hdr++ = ((len >> 6) & 0xfe) | 0x01;	/* 7 bits, EA = 1 */
	} else if (len < (1 << 15)) {
		*hdr++ = ((len << 1) & 0xfe);		/* 7 bits, EA = 0 */
		*hdr++ = ((len >> 6) & 0xfe);		/* 7 bits, EA = 0 */
		*hdr++ = ((len >> 13) & 0x02) | 0x01;	/* 1 bit,  EA = 1 */
	} else {
		DPRINTF("incredible length! (%d)\n", len);
		m_freem(m);
		return EMSGSIZE;
	}

	/*
	 * add command data (to same mbuf if possible)
	 */
	hlen = hdr - mtod(m, uint8_t *);

	if (len > 0) {
		m->m_pkthdr.len = m->m_len = MHLEN;
		m_copyback(m, hlen, len, data, M_NOWAIT);
		if (m->m_pkthdr.len != max(MHLEN, hlen + len)) {
			m_freem(m);
			return ENOMEM;
		}
	}

	m->m_pkthdr.len = hlen + len;
	m->m_len = min(MHLEN, m->m_pkthdr.len);

	DPRINTFN(5, "%s type %2.2x len %d\n",
		(cr ? "command" : "response"), type, m->m_pkthdr.len);

	return rfcomm_session_send_uih(rs, NULL, 0, m);
}
