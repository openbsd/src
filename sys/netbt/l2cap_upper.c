/*	$OpenBSD: l2cap_upper.c,v 1.2 2007/10/01 16:39:30 krw Exp $	*/
/*	$NetBSD: l2cap_upper.c,v 1.8 2007/04/29 20:23:36 msaitoh Exp $	*/

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
#include <sys/mbuf.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/systm.h>

#include <netbt/bluetooth.h>
#include <netbt/hci.h>
#include <netbt/l2cap.h>

/*******************************************************************************
 *
 *	L2CAP Channel - Upper Protocol API
 */

/*
 * l2cap_attach(handle, btproto, upper)
 *
 *	attach new l2cap_channel to handle, populate
 *	with reasonable defaults
 */
int
l2cap_attach(struct l2cap_channel **handle,
		const struct btproto *proto, void *upper)
{
	struct l2cap_channel *chan;

	KASSERT(handle != NULL);
	KASSERT(proto != NULL);
	KASSERT(upper != NULL);

	chan = malloc(sizeof(*chan), M_BLUETOOTH, M_NOWAIT | M_ZERO);
	if (chan == NULL)
		return ENOMEM;

	chan->lc_proto = proto;
	chan->lc_upper = upper;

	chan->lc_state = L2CAP_CLOSED;

	chan->lc_lcid = L2CAP_NULL_CID;
	chan->lc_rcid = L2CAP_NULL_CID;

	chan->lc_laddr.bt_len = sizeof(struct sockaddr_bt);
	chan->lc_laddr.bt_family = AF_BLUETOOTH;
	chan->lc_laddr.bt_psm = L2CAP_PSM_ANY;

	chan->lc_raddr.bt_len = sizeof(struct sockaddr_bt);
	chan->lc_raddr.bt_family = AF_BLUETOOTH;
	chan->lc_raddr.bt_psm = L2CAP_PSM_ANY;

	chan->lc_imtu = L2CAP_MTU_DEFAULT;
	chan->lc_omtu = L2CAP_MTU_DEFAULT;
	chan->lc_flush = L2CAP_FLUSH_TIMO_DEFAULT;

	memcpy(&chan->lc_iqos, &l2cap_default_qos, sizeof(l2cap_qos_t));
	memcpy(&chan->lc_oqos, &l2cap_default_qos, sizeof(l2cap_qos_t));

	*handle = chan;
	return 0;
}

/*
 * l2cap_bind(l2cap_channel, sockaddr)
 *
 *	set local address of channel
 */
int
l2cap_bind(struct l2cap_channel *chan, struct sockaddr_bt *addr)
{

	memcpy(&chan->lc_laddr, addr, sizeof(struct sockaddr_bt));
	return 0;
}

/*
 * l2cap_sockaddr(l2cap_channel, sockaddr)
 *
 *	get local address of channel
 */
int
l2cap_sockaddr(struct l2cap_channel *chan, struct sockaddr_bt *addr)
{

	memcpy(addr, &chan->lc_laddr, sizeof(struct sockaddr_bt));
	return 0;
}

/*
 * l2cap_connect(l2cap_channel, sockaddr)
 *
 *	Initiate a connection to destination. This corresponds to
 *	"Open Channel Request" in the L2CAP specification and will
 *	result in one of the following:
 *
 *		proto->connected(upper)
 *		proto->disconnected(upper, error)
 *
 *	and, optionally
 *		proto->connecting(upper)
 */
int
l2cap_connect(struct l2cap_channel *chan, struct sockaddr_bt *dest)
{
	struct hci_unit *unit;
	int err;

	memcpy(&chan->lc_raddr, dest, sizeof(struct sockaddr_bt));

	if (L2CAP_PSM_INVALID(chan->lc_raddr.bt_psm))
		return EINVAL;

	if (bdaddr_any(&chan->lc_raddr.bt_bdaddr))
		return EDESTADDRREQ;

	/* set local address if it needs setting */
	if (bdaddr_any(&chan->lc_laddr.bt_bdaddr)) {
		err = hci_route_lookup(&chan->lc_laddr.bt_bdaddr,
					&chan->lc_raddr.bt_bdaddr);
		if (err)
			return err;
	}

	unit = hci_unit_lookup(&chan->lc_laddr.bt_bdaddr);
	if (unit == NULL)
		return EHOSTUNREACH;

	/* attach to active list */
	err = l2cap_cid_alloc(chan);
	if (err)
		return err;

	/* open link to remote device */
	chan->lc_link = hci_acl_open(unit, &chan->lc_raddr.bt_bdaddr);
	if (chan->lc_link == NULL)
		return EHOSTUNREACH;

	/* set the link mode */
	err = l2cap_setmode(chan);
	if (err == EINPROGRESS) {
		chan->lc_state = L2CAP_WAIT_SEND_CONNECT_REQ;
		(*chan->lc_proto->connecting)(chan->lc_upper);
		return 0;
	}
	if (err)
		goto fail;

	/*
	 * We can queue a connect request now even though the link may
	 * not yet be open; Our mode setting is assured, and the queue
	 * will be started automatically at the right time.
	 */
	chan->lc_state = L2CAP_WAIT_RECV_CONNECT_RSP;
	err = l2cap_send_connect_req(chan);
	if (err)
		goto fail;

	return 0;

fail:
	chan->lc_state = L2CAP_CLOSED;
	hci_acl_close(chan->lc_link, err);
	chan->lc_link = NULL;
	return err;
}

/*
 * l2cap_peeraddr(l2cap_channel, sockaddr)
 *
 *	get remote address of channel
 */
int
l2cap_peeraddr(struct l2cap_channel *chan, struct sockaddr_bt *addr)
{

	memcpy(addr, &chan->lc_raddr, sizeof(struct sockaddr_bt));
	return 0;
}

/*
 * l2cap_disconnect(l2cap_channel, linger)
 *
 *	Initiate L2CAP disconnection. This corresponds to
 *	"Close Channel Request" in the L2CAP specification
 *	and will result in a call to
 *
 *		proto->disconnected(upper, error)
 *
 *	when the disconnection is complete. If linger is set,
 *	the call will not be made until data has flushed from
 *	the queue.
 */
int
l2cap_disconnect(struct l2cap_channel *chan, int linger)
{
	int err = 0;

	if (chan->lc_state == L2CAP_CLOSED
	    || chan->lc_state == L2CAP_WAIT_DISCONNECT)
		return EINVAL;

	chan->lc_flags |= L2CAP_SHUTDOWN;

	/*
	 * no need to do anything unless the queue is empty or
	 * we are not lingering..
	 */
	if ((IF_IS_EMPTY(&chan->lc_txq) && chan->lc_pending == 0)
	    || linger == 0) {
		chan->lc_state = L2CAP_WAIT_DISCONNECT;
		err = l2cap_send_disconnect_req(chan);
		if (err)
			l2cap_close(chan, err);
	}
	return err;
}

/*
 * l2cap_detach(handle)
 *
 *	Detach l2cap channel from handle & close it down
 */
int
l2cap_detach(struct l2cap_channel **handle)
{
	struct l2cap_channel *chan;

	chan = *handle;
	*handle = NULL;

	if (chan->lc_state != L2CAP_CLOSED)
		l2cap_close(chan, 0);

	if (chan->lc_lcid != L2CAP_NULL_CID) {
		LIST_REMOVE(chan, lc_ncid);
		chan->lc_lcid = L2CAP_NULL_CID;
	}

	IF_PURGE(&chan->lc_txq);

	/*
	 * Could implement some kind of delayed expunge to make sure that the
	 * CID is really dead before it becomes available for reuse?
	 */

	free(chan, M_BLUETOOTH);
	return 0;
}

/*
 * l2cap_listen(l2cap_channel)
 *
 *	Use this channel as a listening post (until detached). This will
 *	result in calls to:
 *
 *		proto->newconn(upper, laddr, raddr)
 *
 *	for incoming connections matching the psm and local address of the
 *	channel (NULL psm/address are permitted and match any protocol/device).
 *
 *	The upper layer should create and return a new channel.
 *
 *	You cannot use this channel for anything else subsequent to this call
 */
int
l2cap_listen(struct l2cap_channel *chan)
{
	struct l2cap_channel *used, *prev = NULL;

	if (chan->lc_lcid != L2CAP_NULL_CID)
		return EINVAL;

	if (chan->lc_laddr.bt_psm != L2CAP_PSM_ANY
	    && L2CAP_PSM_INVALID(chan->lc_laddr.bt_psm))
		return EADDRNOTAVAIL;

	/*
	 * This CID is irrelevant, as the channel is not stored on the active
	 * list and the socket code does not allow operations on listening
	 * sockets, but we set it so the detach code knows to LIST_REMOVE the
	 * channel.
	 */
	chan->lc_lcid = L2CAP_SIGNAL_CID;

	/*
	 * The list of listening channels is stored in an order such that new
	 * listeners dont usurp current listeners, but that specific listening
	 * takes precedence over promiscuous, and the connect request code can
	 * easily use the first matching entry.
	 */
	LIST_FOREACH(used, &l2cap_listen_list, lc_ncid) {
		if (used->lc_laddr.bt_psm < chan->lc_laddr.bt_psm)
			break;

		if (used->lc_laddr.bt_psm == chan->lc_laddr.bt_psm
			&& bdaddr_any(&used->lc_laddr.bt_bdaddr)
			&& !bdaddr_any(&chan->lc_laddr.bt_bdaddr))
			break;

		prev = used;
	}

	if (prev == NULL)
		LIST_INSERT_HEAD(&l2cap_listen_list, chan, lc_ncid);
	else
		LIST_INSERT_AFTER(prev, chan, lc_ncid);

	return 0;
}

/*
 * l2cap_send(l2cap_channel, mbuf)
 *
 *	Output SDU on channel described by channel. This corresponds
 *	to "Send Data Request" in the L2CAP specification. The upper
 *	layer will be notified when SDU's have completed sending by a
 *	call to:
 *
 *		proto->complete(upper, n)
 *
 *	(currently n == 1)
 *
 *	Note: I'm not sure how this will work out, but I think that
 *	if outgoing Retransmission Mode or Flow Control Mode is
 *	negotiated then this call will not be made until the SDU has
 *	been acknowleged by the peer L2CAP entity. For 'Best Effort'
 *	it will be made when the packet has cleared the controller
 *	buffers.
 *
 *	We only support Basic mode so far, so encapsulate with a
 *	B-Frame header and start sending if we are not already
 */
int
l2cap_send(struct l2cap_channel *chan, struct mbuf *m)
{
	l2cap_hdr_t *hdr;
	int plen;

	if (chan->lc_state == L2CAP_CLOSED) {
		m_freem(m);
		return ENOTCONN;
	}

	plen = m->m_pkthdr.len;

	DPRINTFN(5, "send %d bytes on CID #%d (pending = %d)\n",
		plen, chan->lc_lcid, chan->lc_pending);

	/* Encapsulate with B-Frame */
	M_PREPEND(m, sizeof(l2cap_hdr_t), M_DONTWAIT);
	if (m == NULL)
		return ENOMEM;

	hdr = mtod(m, l2cap_hdr_t *);
	hdr->length = htole16(plen);
	hdr->dcid = htole16(chan->lc_rcid);

	/* Queue it on our list */
	IF_ENQUEUE(&chan->lc_txq, m);

	/* If we are not sending, then start doing so */
	if (chan->lc_pending == 0)
		return l2cap_start(chan);

	return 0;
}

/*
 * l2cap_setopt(l2cap_channel, opt, addr)
 *
 *	Apply configuration options to channel. This corresponds to
 *	"Configure Channel Request" in the L2CAP specification.
 *
 *	for SO_L2CAP_LM, the settings will take effect when the
 *	channel is established. If the channel is already open,
 *	a call to
 *		proto->linkmode(upper, new)
 *
 *	will be made when the change is complete.
 */
int
l2cap_setopt(struct l2cap_channel *chan, int opt, void *addr)
{
	int mode, err = 0;
	uint16_t mtu;

	switch (opt) {
	case SO_L2CAP_IMTU:	/* set Incoming MTU */
		mtu = *(uint16_t *)addr;
		if (mtu < L2CAP_MTU_MINIMUM)
			err = EINVAL;
		else if (chan->lc_state == L2CAP_CLOSED)
			chan->lc_imtu = mtu;
		else
			err = EBUSY;

		break;

	case SO_L2CAP_LM:	/* set link mode */
		mode = *(int *)addr;
		mode &= (L2CAP_LM_SECURE | L2CAP_LM_ENCRYPT | L2CAP_LM_AUTH);

		if (mode & L2CAP_LM_SECURE)
			mode |= L2CAP_LM_ENCRYPT;

		if (mode & L2CAP_LM_ENCRYPT)
			mode |= L2CAP_LM_AUTH;

		chan->lc_mode = mode;

		if (chan->lc_state == L2CAP_OPEN)
			err = l2cap_setmode(chan);

		break;

	case SO_L2CAP_OQOS:	/* set Outgoing QoS flow spec */
	case SO_L2CAP_FLUSH:	/* set Outgoing Flush Timeout */
	default:
		err = ENOPROTOOPT;
		break;
	}

	return err;
}

/*
 * l2cap_getopt(l2cap_channel, opt, addr)
 *
 *	Return configuration parameters.
 */
int
l2cap_getopt(struct l2cap_channel *chan, int opt, void *addr)
{

	switch (opt) {
	case SO_L2CAP_IMTU:	/* get Incoming MTU */
		*(uint16_t *)addr = chan->lc_imtu;
		return sizeof(uint16_t);

	case SO_L2CAP_OMTU:	/* get Outgoing MTU */
		*(uint16_t *)addr = chan->lc_omtu;
		return sizeof(uint16_t);

	case SO_L2CAP_IQOS:	/* get Incoming QoS flow spec */
		memcpy(addr, &chan->lc_iqos, sizeof(l2cap_qos_t));
		return sizeof(l2cap_qos_t);

	case SO_L2CAP_OQOS:	/* get Outgoing QoS flow spec */
		memcpy(addr, &chan->lc_oqos, sizeof(l2cap_qos_t));
		return sizeof(l2cap_qos_t);

	case SO_L2CAP_FLUSH:	/* get Flush Timeout */
		*(uint16_t *)addr = chan->lc_flush;
		return sizeof(uint16_t);

	case SO_L2CAP_LM:	/* get link mode */
		*(int *)addr = chan->lc_mode;
		return sizeof(int);

	default:
		break;
	}

	return 0;
}
