/*	$OpenBSD: l2cap_misc.c,v 1.2 2007/06/01 02:46:11 uwe Exp $	*/
/*	$NetBSD: l2cap_misc.c,v 1.3 2007/04/21 06:15:23 plunky Exp $	*/

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
#include <sys/systm.h>

#include <netbt/bluetooth.h>
#include <netbt/hci.h>
#include <netbt/l2cap.h>

struct l2cap_channel_list
	l2cap_active_list = LIST_HEAD_INITIALIZER(l2cap_active_list);
struct l2cap_channel_list
	l2cap_listen_list = LIST_HEAD_INITIALIZER(l2cap_listen_list);

struct pool l2cap_req_pool;
struct pool l2cap_pdu_pool;

const l2cap_qos_t l2cap_default_qos = {
	0,			/* flags */
	L2CAP_QOS_BEST_EFFORT,	/* service type */
	0x00000000,		/* token rate */
	0x00000000,		/* token bucket size */
	0x00000000,		/* peak bandwidth */
	0xffffffff,		/* latency */
	0xffffffff		/* delay variation */
};

/*
 * L2CAP request timeouts
 */
int l2cap_response_timeout = 30;		/* seconds */
int l2cap_response_extended_timeout = 180;	/* seconds */

void
l2cap_init(void)
{
	pool_init(&l2cap_req_pool, sizeof(struct l2cap_req), 0, 0, 0,
	    "l2cap_req", NULL);
	pool_init(&l2cap_pdu_pool, sizeof(struct l2cap_pdu), 0, 0, 0,
	    "l2cap_pdu", NULL);
}

/*
 * Set Link Mode on channel
 */
int
l2cap_setmode(struct l2cap_channel *chan)
{

	KASSERT(chan != NULL);
	KASSERT(chan->lc_link != NULL);

	DPRINTF("CID #%d, auth %s, encrypt %s, secure %s\n", chan->lc_lcid,
		(chan->lc_mode & L2CAP_LM_AUTH ? "yes" : "no"),
		(chan->lc_mode & L2CAP_LM_ENCRYPT ? "yes" : "no"),
		(chan->lc_mode & L2CAP_LM_SECURE ? "yes" : "no"));

	if (chan->lc_mode & L2CAP_LM_AUTH)
		chan->lc_link->hl_flags |= HCI_LINK_AUTH_REQ;

	if (chan->lc_mode & L2CAP_LM_ENCRYPT)
		chan->lc_link->hl_flags |= HCI_LINK_ENCRYPT_REQ;

	if (chan->lc_mode & L2CAP_LM_SECURE)
		chan->lc_link->hl_flags |= HCI_LINK_SECURE_REQ;

	return hci_acl_setmode(chan->lc_link);
}

/*
 * Allocate a new Request structure & ID and set the timer going
 */
int
l2cap_request_alloc(struct l2cap_channel *chan, uint8_t code)
{
	struct hci_link *link = chan->lc_link;
	struct l2cap_req *req;
	int next_id;

	if (link == NULL)
		return ENETDOWN;

	/* find next ID (0 is not allowed) */
	next_id = link->hl_lastid + 1;
	if (next_id > 0xff)
		next_id = 1;

	/* Ouroboros check */
	req = TAILQ_FIRST(&link->hl_reqs);
	if (req && req->lr_id == next_id)
		return ENFILE;

	req = pool_get(&l2cap_req_pool, PR_NOWAIT);
	if (req == NULL)
		return ENOMEM;

	req->lr_id = link->hl_lastid = next_id;

	req->lr_code = code;
	req->lr_chan = chan;
	req->lr_link = link;

	timeout_set(&req->lr_rtx, l2cap_rtx, req);
	timeout_add(&req->lr_rtx, l2cap_response_timeout*hz);

	TAILQ_INSERT_TAIL(&link->hl_reqs, req, lr_next);

	return 0;
}

/*
 * Find a running request for this link
 */
struct l2cap_req *
l2cap_request_lookup(struct hci_link *link, uint8_t id)
{
	struct l2cap_req *req;

	TAILQ_FOREACH(req, &link->hl_reqs, lr_next) {
		if (req->lr_id == id)
			return req;
	}

	return NULL;
}

/*
 * Halt and free a request
 */
void
l2cap_request_free(struct l2cap_req *req)
{
	struct hci_link *link = req->lr_link;

	timeout_del(&req->lr_rtx);
	if (timeout_triggered(&req->lr_rtx))
		return;

	TAILQ_REMOVE(&link->hl_reqs, req, lr_next);
	pool_put(&l2cap_req_pool, req);
}

/*
 * Response Timeout eXpired
 *
 * No response to our request, so deal with it as best we can.
 *
 * XXX should try again at least with ertx?
 */
void
l2cap_rtx(void *arg)
{
	struct l2cap_req *req = arg;
	struct l2cap_channel *chan;
	int s;

	s = splsoftnet();

	chan = req->lr_chan;
	l2cap_request_free(req);

	DPRINTF("cid %d, ident %d\n", (chan ? chan->lc_lcid : 0), req->lr_id);

	if (chan && chan->lc_state != L2CAP_CLOSED)
		l2cap_close(chan, ETIMEDOUT);

	splx(s);
}

/*
 * Allocate next available CID to channel. We keep a single
 * ordered list of channels, so find the first gap.
 *
 * If this turns out to be not enough (!), could use a
 * list per HCI unit..
 */
int
l2cap_cid_alloc(struct l2cap_channel *chan)
{
	struct l2cap_channel *used, *prev = NULL;
	uint16_t cid = L2CAP_FIRST_CID;

	if (chan->lc_lcid != L2CAP_NULL_CID || chan->lc_state != L2CAP_CLOSED)
		return EISCONN;

	LIST_FOREACH(used, &l2cap_active_list, lc_ncid) {
		if (used->lc_lcid > cid)
			break;	/* found our gap */

		KASSERT(used->lc_lcid == cid);
		cid++;

		if (cid == L2CAP_LAST_CID)
			return ENFILE;

		prev = used;	/* for insert after */
	}

	chan->lc_lcid = cid;

	if (prev)
		LIST_INSERT_AFTER(prev, chan, lc_ncid);
	else
		LIST_INSERT_HEAD(&l2cap_active_list, chan, lc_ncid);

	return 0;
}

/*
 * Find channel with CID
 */
struct l2cap_channel *
l2cap_cid_lookup(uint16_t cid)
{
	struct l2cap_channel *chan;

	LIST_FOREACH(chan, &l2cap_active_list, lc_ncid) {
		if (chan->lc_lcid == cid)
			return chan;

		if (chan->lc_lcid > cid)
			return NULL;
	}

	return NULL;
}
