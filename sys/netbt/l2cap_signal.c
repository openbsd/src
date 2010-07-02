/*	$OpenBSD: l2cap_signal.c,v 1.6 2010/07/02 02:40:16 blambert Exp $	*/
/*	$NetBSD: l2cap_signal.c,v 1.9 2007/11/10 23:12:23 plunky Exp $	*/

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

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/systm.h>

#include <netbt/bluetooth.h>
#include <netbt/hci.h>
#include <netbt/l2cap.h>

/*******************************************************************************
 *
 *	L2CAP Signal processing
 */

static void l2cap_recv_command_rej(struct mbuf *, struct hci_link *);
static void l2cap_recv_connect_req(struct mbuf *, struct hci_link *);
static void l2cap_recv_connect_rsp(struct mbuf *, struct hci_link *);
static void l2cap_recv_config_req(struct mbuf *, struct hci_link *);
static void l2cap_recv_config_rsp(struct mbuf *, struct hci_link *);
static void l2cap_recv_disconnect_req(struct mbuf *, struct hci_link *);
static void l2cap_recv_disconnect_rsp(struct mbuf *, struct hci_link *);
static void l2cap_recv_info_req(struct mbuf *, struct hci_link *);
static int l2cap_send_signal(struct hci_link *, uint8_t, uint8_t, uint16_t, void *);
static int l2cap_send_command_rej(struct hci_link *, uint8_t, uint32_t, ...);

/*
 * process incoming signal packets (CID 0x0001). Can contain multiple
 * requests/responses.
 */
void
l2cap_recv_signal(struct mbuf *m, struct hci_link *link)
{
	l2cap_cmd_hdr_t cmd;

	for(;;) {
		if (m->m_pkthdr.len == 0)
			goto finish;

		if (m->m_pkthdr.len < sizeof(cmd))
			goto reject;

		m_copydata(m, 0, sizeof(cmd), (caddr_t)&cmd);
		cmd.length = letoh16(cmd.length);

		if (m->m_pkthdr.len < sizeof(cmd) + cmd.length)
			goto reject;

		DPRINTFN(2, "(%s) code %d, ident %d, len %d\n",
			device_xname(link->hl_unit->hci_dev),
			cmd.code, cmd.ident, cmd.length);

		switch (cmd.code) {
		case L2CAP_COMMAND_REJ:
			if (cmd.length > sizeof(l2cap_cmd_rej_cp))
				goto finish;

			l2cap_recv_command_rej(m, link);
			break;

		case L2CAP_CONNECT_REQ:
			if (cmd.length != sizeof(l2cap_con_req_cp))
				goto reject;

			l2cap_recv_connect_req(m, link);
			break;

		case L2CAP_CONNECT_RSP:
			if (cmd.length != sizeof(l2cap_con_rsp_cp))
				goto finish;

			l2cap_recv_connect_rsp(m, link);
			break;

		case L2CAP_CONFIG_REQ:
			l2cap_recv_config_req(m, link);
			break;

		case L2CAP_CONFIG_RSP:
			l2cap_recv_config_rsp(m, link);
			break;

		case L2CAP_DISCONNECT_REQ:
			if (cmd.length != sizeof(l2cap_discon_req_cp))
				goto reject;

			l2cap_recv_disconnect_req(m, link);
			break;

		case L2CAP_DISCONNECT_RSP:
			if (cmd.length != sizeof(l2cap_discon_rsp_cp))
				goto finish;

			l2cap_recv_disconnect_rsp(m, link);
			break;

		case L2CAP_ECHO_REQ:
			m_adj(m, sizeof(cmd) + cmd.length);
			l2cap_send_signal(link, L2CAP_ECHO_RSP, cmd.ident,
					0, NULL);
			break;

		case L2CAP_ECHO_RSP:
			m_adj(m, sizeof(cmd) + cmd.length);
			break;

		case L2CAP_INFO_REQ:
			if (cmd.length != sizeof(l2cap_info_req_cp))
				goto reject;

			l2cap_recv_info_req(m, link);
			break;

		case L2CAP_INFO_RSP:
			m_adj(m, sizeof(cmd) + cmd.length);
			break;

		default:
			goto reject;
		}
	}

#ifdef DIAGNOSTIC
	panic("impossible!");
#endif

reject:
	l2cap_send_command_rej(link, cmd.ident, L2CAP_REJ_NOT_UNDERSTOOD);
finish:
	m_freem(m);
}

/*
 * Process Received Command Reject. For now we dont try to recover gracefully
 * from this, it probably means that the link is garbled or the other end is
 * insufficiently capable of handling normal traffic. (not *my* fault, no way!)
 */
static void
l2cap_recv_command_rej(struct mbuf *m, struct hci_link *link)
{
	struct l2cap_req *req;
	struct l2cap_channel *chan;
	l2cap_cmd_hdr_t cmd;
	l2cap_cmd_rej_cp cp;

	m_copydata(m, 0, sizeof(cmd), (caddr_t)&cmd);
	m_adj(m, sizeof(cmd));

	cmd.length = letoh16(cmd.length);

	m_copydata(m, 0, cmd.length, (caddr_t)&cp);
	m_adj(m, cmd.length);

	req = l2cap_request_lookup(link, cmd.ident);
	if (req == NULL)
		return;

	switch (letoh16(cp.reason)) {
	case L2CAP_REJ_NOT_UNDERSTOOD:
		/*
		 * I dont know what to do, just move up the timeout
		 */
		timeout_add(&req->lr_rtx, 0);
		break;

	case L2CAP_REJ_MTU_EXCEEDED:
		/*
		 * I didnt send any commands over L2CAP_MTU_MINIMUM size, but..
		 *
		 * XXX maybe we should resend this, instead?
		 */
		link->hl_mtu = letoh16(cp.data[0]);
		timeout_add(&req->lr_rtx, 0);
		break;

	case L2CAP_REJ_INVALID_CID:
		/*
		 * Well, if they dont have such a channel then our channel is
		 * most likely closed. Make it so.
		 */
		chan = req->lr_chan;
		l2cap_request_free(req);
		if (chan != NULL && chan->lc_state != L2CAP_CLOSED)
			l2cap_close(chan, ECONNABORTED);

		break;

	default:
		UNKNOWN(letoh16(cp.reason));
		break;
	}
}

/*
 * Process Received Connect Request. Find listening channel matching
 * psm & addr and ask upper layer for a new channel.
 */
static void
l2cap_recv_connect_req(struct mbuf *m, struct hci_link *link)
{
	struct sockaddr_bt laddr, raddr;
	struct l2cap_channel *chan, *new;
	l2cap_cmd_hdr_t cmd;
	l2cap_con_req_cp cp;
	int err;

	/* extract cmd */
	m_copydata(m, 0, sizeof(cmd), (caddr_t)&cmd);
	m_adj(m, sizeof(cmd));

	/* extract request */
	m_copydata(m, 0, sizeof(cp), (caddr_t)&cp);
	m_adj(m, sizeof(cp));

	cp.scid = letoh16(cp.scid);
	cp.psm = letoh16(cp.psm);

	memset(&laddr, 0, sizeof(struct sockaddr_bt));
	laddr.bt_len = sizeof(struct sockaddr_bt);
	laddr.bt_family = AF_BLUETOOTH;
	laddr.bt_psm = cp.psm;
	bdaddr_copy(&laddr.bt_bdaddr, &link->hl_unit->hci_bdaddr);

	memset(&raddr, 0, sizeof(struct sockaddr_bt));
	raddr.bt_len = sizeof(struct sockaddr_bt);
	raddr.bt_family = AF_BLUETOOTH;
	raddr.bt_psm = cp.psm;
	bdaddr_copy(&raddr.bt_bdaddr, &link->hl_bdaddr);

	LIST_FOREACH(chan, &l2cap_listen_list, lc_ncid) {
		if (chan->lc_laddr.bt_psm != laddr.bt_psm
		    && chan->lc_laddr.bt_psm != L2CAP_PSM_ANY)
			continue;

		if (!bdaddr_same(&laddr.bt_bdaddr, &chan->lc_laddr.bt_bdaddr)
		    && bdaddr_any(&chan->lc_laddr.bt_bdaddr) == 0)
			continue;

		new= (*chan->lc_proto->newconn)(chan->lc_upper, &laddr, &raddr);
		if (new == NULL)
			continue;

		err = l2cap_cid_alloc(new);
		if (err) {
			l2cap_send_connect_rsp(link, cmd.ident,
						0, cp.scid,
						L2CAP_NO_RESOURCES);

			(*new->lc_proto->disconnected)(new->lc_upper, err);
			return;
		}

		new->lc_link = hci_acl_open(link->hl_unit, &link->hl_bdaddr);
		KASSERT(new->lc_link == link);

		new->lc_rcid = cp.scid;
		new->lc_ident = cmd.ident;

		memcpy(&new->lc_laddr, &laddr, sizeof(struct sockaddr_bt));
		memcpy(&new->lc_raddr, &raddr, sizeof(struct sockaddr_bt));

		new->lc_mode = chan->lc_mode;

		err = l2cap_setmode(new);
		if (err == EINPROGRESS) {
			new->lc_state = L2CAP_WAIT_SEND_CONNECT_RSP;
			(*new->lc_proto->connecting)(new->lc_upper);
			return;
		}
		if (err) {
			new->lc_state = L2CAP_CLOSED;
			hci_acl_close(link, err);
			new->lc_link = NULL;

			l2cap_send_connect_rsp(link, cmd.ident,
						0, cp.scid,
						L2CAP_NO_RESOURCES);

			(*new->lc_proto->disconnected)(new->lc_upper, err);
			return;
		}

		err = l2cap_send_connect_rsp(link, cmd.ident,
					      new->lc_lcid, new->lc_rcid,
					      L2CAP_SUCCESS);
		if (err) {
			l2cap_close(new, err);
			return;
		}

		new->lc_state = L2CAP_WAIT_CONFIG;
		new->lc_flags |= (L2CAP_WAIT_CONFIG_REQ | L2CAP_WAIT_CONFIG_RSP);
		err = l2cap_send_config_req(new);
		if (err)
			l2cap_close(new, err);

		return;
	}

	l2cap_send_connect_rsp(link, cmd.ident,
				0, cp.scid,
				L2CAP_PSM_NOT_SUPPORTED);
}

/*
 * Process Received Connect Response.
 */
static void
l2cap_recv_connect_rsp(struct mbuf *m, struct hci_link *link)
{
	l2cap_cmd_hdr_t cmd;
	l2cap_con_rsp_cp cp;
	struct l2cap_req *req;
	struct l2cap_channel *chan;

	m_copydata(m, 0, sizeof(cmd), (caddr_t)&cmd);
	m_adj(m, sizeof(cmd));

	m_copydata(m, 0, sizeof(cp), (caddr_t)&cp);
	m_adj(m, sizeof(cp));

	cp.scid = letoh16(cp.scid);
	cp.dcid = letoh16(cp.dcid);
	cp.result = letoh16(cp.result);

	req = l2cap_request_lookup(link, cmd.ident);
	if (req == NULL || req->lr_code != L2CAP_CONNECT_REQ)
		return;

	chan = req->lr_chan;
	if (chan != NULL && chan->lc_lcid != cp.scid)
		return;

	if (chan == NULL || chan->lc_state != L2CAP_WAIT_RECV_CONNECT_RSP) {
		l2cap_request_free(req);
		return;
	}

	switch (cp.result) {
	case L2CAP_SUCCESS:
		/*
		 * Ok, at this point we have a connection to the other party. We
		 * could indicate upstream that we are ready for business and
		 * wait for a "Configure Channel Request" but I'm not so sure
		 * that is required in our case - we will proceed directly to
		 * sending our config request. We set two state bits because in
		 * the config state we are waiting for requests and responses.
		 */
		l2cap_request_free(req);
		chan->lc_rcid = cp.dcid;
		chan->lc_state = L2CAP_WAIT_CONFIG;
		chan->lc_flags |= (L2CAP_WAIT_CONFIG_REQ | L2CAP_WAIT_CONFIG_RSP);
		l2cap_send_config_req(chan);
		break;

	case L2CAP_PENDING:
		/* XXX dont release request, should start eRTX timeout? */
		(*chan->lc_proto->connecting)(chan->lc_upper);
		break;

	case L2CAP_PSM_NOT_SUPPORTED:
	case L2CAP_SECURITY_BLOCK:
	case L2CAP_NO_RESOURCES:
	default:
		l2cap_request_free(req);
		l2cap_close(chan, ECONNREFUSED);
		break;
	}
}

/*
 * Process Received Config Request.
 */
static void
l2cap_recv_config_req(struct mbuf *m, struct hci_link *link)
{
	uint8_t buf[L2CAP_MTU_MINIMUM];
	l2cap_cmd_hdr_t cmd;
	l2cap_cfg_req_cp cp;
	l2cap_cfg_opt_t opt;
	l2cap_cfg_opt_val_t val;
	l2cap_cfg_rsp_cp rp;
	struct l2cap_channel *chan;
	int left, len;

	m_copydata(m, 0, sizeof(cmd), (caddr_t)&cmd);
	m_adj(m, sizeof(cmd));
	left = letoh16(cmd.length);

	if (left < sizeof(cp))
		goto reject;

	m_copydata(m, 0, sizeof(cp), (caddr_t)&cp);
	m_adj(m, sizeof(cp));
	left -= sizeof(cp);

	cp.dcid = letoh16(cp.dcid);
	cp.flags = letoh16(cp.flags);

	chan = l2cap_cid_lookup(cp.dcid);
	if (chan == NULL || chan->lc_link != link
	    || chan->lc_state != L2CAP_WAIT_CONFIG
	    || (chan->lc_flags & L2CAP_WAIT_CONFIG_REQ) == 0) {
		/* XXX we should really accept reconfiguration requests */
		l2cap_send_command_rej(link, cmd.ident, L2CAP_REJ_INVALID_CID,
					L2CAP_NULL_CID, cp.dcid);
		goto out;
	}

	/* ready our response packet */
	rp.scid = htole16(chan->lc_rcid);
	rp.flags = 0;	/* "No Continuation" */
	rp.result = L2CAP_SUCCESS;
	len = sizeof(rp);

	/*
	 * Process the packet. We build the return packet on the fly adding any
	 * unacceptable parameters as we go. As we can only return one result,
	 * unknown option takes precedence so we start our return packet anew
	 * and ignore option values thereafter as they will be re-sent.
	 *
	 * Since we do not support enough options to make overflowing the min
	 * MTU size an issue in normal use, we just reject config requests that
	 * make that happen. This could be because options are repeated or the
	 * packet is corrupted in some way.
	 *
	 * If unknown option types threaten to overflow the packet, we just
	 * ignore them. We can deny them next time.
	 */
	while (left > 0) {
		if (left < sizeof(opt))
			goto reject;

		m_copydata(m, 0, sizeof(opt), (caddr_t)&opt);
		m_adj(m, sizeof(opt));
		left -= sizeof(opt);

		if (left < opt.length)
			goto reject;

		switch(opt.type & L2CAP_OPT_HINT_MASK) {
		case L2CAP_OPT_MTU:
			if (rp.result == L2CAP_UNKNOWN_OPTION)
				break;

			if (opt.length != L2CAP_OPT_MTU_SIZE)
				goto reject;

			m_copydata(m, 0, L2CAP_OPT_MTU_SIZE, (caddr_t)&val);
			val.mtu = letoh16(val.mtu);

			/*
			 * XXX how do we know what the minimum acceptable MTU is
			 * for a channel? Spec says some profiles have a higher
			 * minimum but I have no way to find that out at this
			 * juncture..
			 */
			if (val.mtu < L2CAP_MTU_MINIMUM) {
				if (len + sizeof(opt) + L2CAP_OPT_MTU_SIZE > sizeof(buf))
					goto reject;

				rp.result = L2CAP_UNACCEPTABLE_PARAMS;
				memcpy(buf + len, &opt, sizeof(opt));
				len += sizeof(opt);
				val.mtu = htole16(L2CAP_MTU_MINIMUM);
				memcpy(buf + len, &val, L2CAP_OPT_MTU_SIZE);
				len += L2CAP_OPT_MTU_SIZE;
			} else
				chan->lc_omtu = val.mtu;

			break;

		case L2CAP_OPT_FLUSH_TIMO:
			if (rp.result == L2CAP_UNKNOWN_OPTION)
				break;

			if (opt.length != L2CAP_OPT_FLUSH_TIMO_SIZE)
				goto reject;

			/*
			 * I think that this is informational only - he is
			 * informing us of the flush timeout he will be using.
			 * I dont think this affects us in any significant way,
			 * so just ignore this value for now.
			 */
			break;

		case L2CAP_OPT_QOS:
			if (rp.result == L2CAP_UNKNOWN_OPTION)
				break;

			if (opt.length != L2CAP_OPT_QOS_SIZE)
				goto reject;

			m_copydata(m, 0, L2CAP_OPT_QOS_SIZE, (caddr_t)&val);
			if (val.qos.service_type == L2CAP_QOS_NO_TRAFFIC ||
			    val.qos.service_type == L2CAP_QOS_BEST_EFFORT)
				/*
				 * In accordance with the spec, we choose to
				 * ignore the fields an provide no response.
				 */
				break;

			if (len + sizeof(opt) + L2CAP_OPT_QOS_SIZE > sizeof(buf))
				goto reject;

			if (val.qos.service_type != L2CAP_QOS_GUARANTEED) {
				/*
				 * Instead of sending an "unacceptable
				 * parameters" response, treat this as an
				 * unknown option and include the option
				 * value in the response.
				 */
				rp.result = L2CAP_UNKNOWN_OPTION;
			} else {
				/*
				 * According to the spec, we must return
				 * specific values for wild card parameters.
				 * I don't know what to return without lying,
				 * so return "unacceptable parameters" and
				 * specify the preferred service type as
				 * "Best Effort".
				 */
				rp.result = L2CAP_UNACCEPTABLE_PARAMS;
				val.qos.service_type = L2CAP_QOS_BEST_EFFORT;
			}

			memcpy(buf + len, &opt, sizeof(opt));
			len += sizeof(opt);
			memcpy(buf + len, &val, L2CAP_OPT_QOS_SIZE);
			len += L2CAP_OPT_QOS_SIZE;
			break;

		default:
			/* ignore hints */
			if (opt.type & L2CAP_OPT_HINT_BIT)
				break;

			/* unknown options supersede all else */
			if (rp.result != L2CAP_UNKNOWN_OPTION) {
				rp.result = L2CAP_UNKNOWN_OPTION;
				len = sizeof(rp);
			}

			/* ignore if it doesn't fit */
			if (len + sizeof(opt) > sizeof(buf))
				break;

			/* return unknown option type, but no data */
			buf[len++] = opt.type;
			buf[len++] = 0;
			break;
		}

		m_adj(m, opt.length);
		left -= opt.length;
	}

	rp.result = htole16(rp.result);
	memcpy(buf, &rp, sizeof(rp));
	l2cap_send_signal(link, L2CAP_CONFIG_RSP, cmd.ident, len, buf);

	if ((cp.flags & L2CAP_OPT_CFLAG_BIT) == 0
	    && rp.result == letoh16(L2CAP_SUCCESS)) {

		chan->lc_flags &= ~L2CAP_WAIT_CONFIG_REQ;

		if ((chan->lc_flags & L2CAP_WAIT_CONFIG_RSP) == 0) {
			chan->lc_state = L2CAP_OPEN;
			/* XXX how to distinguish REconfiguration? */
			(*chan->lc_proto->connected)(chan->lc_upper);
		}
	}
	return;

reject:
	l2cap_send_command_rej(link, cmd.ident, L2CAP_REJ_NOT_UNDERSTOOD);
out:
	m_adj(m, left);
}

/*
 * Process Received Config Response.
 */
static void
l2cap_recv_config_rsp(struct mbuf *m, struct hci_link *link)
{
	l2cap_cmd_hdr_t cmd;
	l2cap_cfg_rsp_cp cp;
	l2cap_cfg_opt_t opt;
	l2cap_cfg_opt_val_t val;
	struct l2cap_req *req;
	struct l2cap_channel *chan;
	int left;

	m_copydata(m, 0, sizeof(cmd), (caddr_t)&cmd);
	m_adj(m, sizeof(cmd));
	left = letoh16(cmd.length);

	if (left < sizeof(cp))
		goto out;

	m_copydata(m, 0, sizeof(cp), (caddr_t)&cp);
	m_adj(m, sizeof(cp));
	left -= sizeof(cp);

	cp.scid = letoh16(cp.scid);
	cp.flags = letoh16(cp.flags);
	cp.result = letoh16(cp.result);

	req = l2cap_request_lookup(link, cmd.ident);
	if (req == NULL || req->lr_code != L2CAP_CONFIG_REQ)
		goto out;

	chan = req->lr_chan;
	if (chan != NULL && chan->lc_lcid != cp.scid)
		goto out;

	l2cap_request_free(req);

	if (chan == NULL || chan->lc_state != L2CAP_WAIT_CONFIG
	    || (chan->lc_flags & L2CAP_WAIT_CONFIG_RSP) == 0)
		goto out;

	if ((cp.flags & L2CAP_OPT_CFLAG_BIT)) {
		l2cap_cfg_req_cp rp;

		/*
		 * They have more to tell us and want another ID to
		 * use, so send an empty config request
		 */
		if (l2cap_request_alloc(chan, L2CAP_CONFIG_REQ))
			goto discon;

		rp.dcid = htole16(cp.scid);
		rp.flags = 0;

		if (l2cap_send_signal(link, L2CAP_CONFIG_REQ, link->hl_lastid,
					sizeof(rp), &rp))
			goto discon;
	}

	switch(cp.result) {
	case L2CAP_SUCCESS:
		/*
		 * If continuation flag was not set, our config request was
		 * accepted. We may have to wait for their config request to
		 * complete, so check that but otherwise we are open
		 *
		 * There may be 'advisory' values in the packet but we just
		 * ignore those..
		 */
		if ((cp.flags & L2CAP_OPT_CFLAG_BIT) == 0) {
			chan->lc_flags &= ~L2CAP_WAIT_CONFIG_RSP;

			if ((chan->lc_flags & L2CAP_WAIT_CONFIG_REQ) == 0) {
				chan->lc_state = L2CAP_OPEN;
				/* XXX how to distinguish REconfiguration? */
				(*chan->lc_proto->connected)(chan->lc_upper);
			}
		}
		goto out;

	case L2CAP_UNACCEPTABLE_PARAMS:
		/*
		 * Packet contains unacceptable parameters with preferred values
		 */
		while (left > 0) {
			if (left < sizeof(opt))
				goto discon;

			m_copydata(m, 0, sizeof(opt), (caddr_t)&opt);
			m_adj(m, sizeof(opt));
			left -= sizeof(opt);

			if (left < opt.length)
				goto discon;

			switch (opt.type) {
			case L2CAP_OPT_MTU:
				if (opt.length != L2CAP_OPT_MTU_SIZE)
					goto discon;

				m_copydata(m, 0, L2CAP_OPT_MTU_SIZE, (caddr_t)&val);
				chan->lc_imtu = letoh16(val.mtu);
				if (chan->lc_imtu < L2CAP_MTU_MINIMUM)
					chan->lc_imtu = L2CAP_MTU_DEFAULT;
				break;

			case L2CAP_OPT_FLUSH_TIMO:
				if (opt.length != L2CAP_OPT_FLUSH_TIMO_SIZE)
					goto discon;

				/*
				 * Spec says: If we cannot honor proposed value,
				 * either disconnect or try again with original
				 * value. I can't really see why they want to
				 * interfere with OUR flush timeout in any case
				 * so we just punt for now.
				 */
				goto discon;

			case L2CAP_OPT_QOS:
				break;

			default:
				UNKNOWN(opt.type);
				goto discon;
			}

			m_adj(m, opt.length);
			left -= opt.length;
		}

		if ((cp.flags & L2CAP_OPT_CFLAG_BIT) == 0)
			l2cap_send_config_req(chan);	/* no state change */

		goto out;

	case L2CAP_REJECT:
		goto discon;

	case L2CAP_UNKNOWN_OPTION:
		/*
		 * Packet contains options not understood. Turn off unknown
		 * options by setting them to default values (means they will
		 * not be requested again).
		 *
		 * If our option was already off then fail (paranoia?)
		 *
		 * XXX Should we consider that options were set for a reason?
		 */
		while (left > 0) {
			if (left < sizeof(opt))
				goto discon;

			m_copydata(m, 0, sizeof(opt), (caddr_t)&opt);
			m_adj(m, sizeof(opt));
			left -= sizeof(opt);

			if (left < opt.length)
				goto discon;

			m_adj(m, opt.length);
			left -= opt.length;

			switch(opt.type) {
			case L2CAP_OPT_MTU:
				if (chan->lc_imtu == L2CAP_MTU_DEFAULT)
					goto discon;

				chan->lc_imtu = L2CAP_MTU_DEFAULT;
				break;

			case L2CAP_OPT_FLUSH_TIMO:
				if (chan->lc_flush == L2CAP_FLUSH_TIMO_DEFAULT)
					goto discon;

				chan->lc_flush = L2CAP_FLUSH_TIMO_DEFAULT;
				break;

			case L2CAP_OPT_QOS:
				break;

			default:
				UNKNOWN(opt.type);
				goto discon;
			}
		}

		if ((cp.flags & L2CAP_OPT_CFLAG_BIT) == 0)
			l2cap_send_config_req(chan);	/* no state change */

		goto out;

	default:
		UNKNOWN(cp.result);
		goto discon;
	}

	DPRINTF("how did I get here!?\n");

discon:
	l2cap_send_disconnect_req(chan);
	l2cap_close(chan, ECONNABORTED);

out:
	m_adj(m, left);
}

/*
 * Process Received Disconnect Request. We must validate scid and dcid
 * just in case but otherwise this connection is finished.
 */
static void
l2cap_recv_disconnect_req(struct mbuf *m, struct hci_link *link)
{
	l2cap_cmd_hdr_t cmd;
	l2cap_discon_req_cp cp;
	l2cap_discon_rsp_cp rp;
	struct l2cap_channel *chan;

	m_copydata(m, 0, sizeof(cmd), (caddr_t)&cmd);
	m_adj(m, sizeof(cmd));

	m_copydata(m, 0, sizeof(cp), (caddr_t)&cp);
	m_adj(m, sizeof(cp));

	cp.scid = letoh16(cp.scid);
	cp.dcid = letoh16(cp.dcid);

	chan = l2cap_cid_lookup(cp.dcid);
	if (chan == NULL || chan->lc_link != link || chan->lc_rcid != cp.scid) {
		l2cap_send_command_rej(link, cmd.ident, L2CAP_REJ_INVALID_CID,
					cp.dcid, cp.scid);
		return;
	}

	rp.dcid = htole16(chan->lc_lcid);
	rp.scid = htole16(chan->lc_rcid);
	l2cap_send_signal(link, L2CAP_DISCONNECT_RSP, cmd.ident,
				sizeof(rp), &rp);

	if (chan->lc_state != L2CAP_CLOSED)
		l2cap_close(chan, ECONNRESET);
}

/*
 * Process Received Disconnect Response. We must validate scid and dcid but
 * unless we were waiting for this signal, ignore it.
 */
static void
l2cap_recv_disconnect_rsp(struct mbuf *m, struct hci_link *link)
{
	l2cap_cmd_hdr_t cmd;
	l2cap_discon_rsp_cp cp;
	struct l2cap_req *req;
	struct l2cap_channel *chan;

	m_copydata(m, 0, sizeof(cmd), (caddr_t)&cmd);
	m_adj(m, sizeof(cmd));

	m_copydata(m, 0, sizeof(cp), (caddr_t)&cp);
	m_adj(m, sizeof(cp));

	cp.scid = letoh16(cp.scid);
	cp.dcid = letoh16(cp.dcid);

	req = l2cap_request_lookup(link, cmd.ident);
	if (req == NULL || req->lr_code != L2CAP_DISCONNECT_REQ)
		return;

	chan = req->lr_chan;
	if (chan == NULL
	    || chan->lc_lcid != cp.scid
	    || chan->lc_rcid != cp.dcid)
		return;

	l2cap_request_free(req);

	if (chan->lc_state != L2CAP_WAIT_DISCONNECT)
		return;

	l2cap_close(chan, 0);
}

/*
 * Process Received Info Request. We must respond but alas dont
 * support anything as yet so thats easy.
 */
static void
l2cap_recv_info_req(struct mbuf *m, struct hci_link *link)
{
	l2cap_cmd_hdr_t cmd;
	l2cap_info_req_cp cp;
	l2cap_info_rsp_cp rp;

	m_copydata(m, 0, sizeof(cmd), (caddr_t)&cmd);
	m_adj(m, sizeof(cmd));

	m_copydata(m, 0, sizeof(cp), (caddr_t)&cp);
	m_adj(m, sizeof(cp));

	switch(letoh16(cp.type)) {
	case L2CAP_CONNLESS_MTU:
	case L2CAP_EXTENDED_FEATURES:
	default:
		rp.type = cp.type;
		rp.result = htole16(L2CAP_NOT_SUPPORTED);

		l2cap_send_signal(link, L2CAP_INFO_RSP, cmd.ident,
					sizeof(rp), &rp);
		break;
	}
}

/*
 * Construct signal and wrap in C-Frame for link.
 */
static int
l2cap_send_signal(struct hci_link *link, uint8_t code, uint8_t ident,
			uint16_t length, void *data)
{
	struct mbuf *m;
	l2cap_hdr_t *hdr;
	l2cap_cmd_hdr_t *cmd;

#ifdef DIAGNOSTIC
	if (link == NULL)
		return ENETDOWN;

	if (sizeof(l2cap_cmd_hdr_t) + length > link->hl_mtu)
		printf("(%s) exceeding L2CAP Signal MTU for link!\n",
		    device_xname(link->hl_unit->hci_dev));
#endif

	m = m_gethdr(M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return ENOMEM;

	hdr = mtod(m, l2cap_hdr_t *);
	cmd = (l2cap_cmd_hdr_t *)(hdr + 1);

	m->m_len = m->m_pkthdr.len = MHLEN;

	/* Command Data */
	if (length > 0)
		m_copyback(m, sizeof(*hdr) + sizeof(*cmd), length, data,
		    M_NOWAIT);

	/* Command Header */
	cmd->code = code;
	cmd->ident = ident;
	cmd->length = htole16(length);
	length += sizeof(*cmd);

	/* C-Frame Header */
	hdr->length = htole16(length);
	hdr->dcid = htole16(L2CAP_SIGNAL_CID);
	length += sizeof(*hdr);

	if (m->m_pkthdr.len != MAX(MHLEN, length)) {
		m_freem(m);
		return ENOMEM;
	}

	m->m_pkthdr.len = length;
	m->m_len = MIN(length, MHLEN);

	DPRINTFN(2, "(%s) code %d, ident %d, len %d\n",
		device_xname(link->hl_unit->hci_dev), code, ident, length);

	return hci_acl_send(m, link, NULL);
}

/*
 * Send Command Reject packet.
 */
static int
l2cap_send_command_rej(struct hci_link *link, uint8_t ident,
			uint32_t reason, ...)
{
	l2cap_cmd_rej_cp cp;
	int len = 0;
	va_list ap;

	va_start(ap, reason);

	cp.reason = htole16(reason);

	switch (reason) {
	case L2CAP_REJ_NOT_UNDERSTOOD:
		len = 2;
		break;

	case L2CAP_REJ_MTU_EXCEEDED:
		len = 4;
		cp.data[0] = va_arg(ap, int);		/* SigMTU */
		cp.data[0] = htole16(cp.data[0]);
		break;

	case L2CAP_REJ_INVALID_CID:
		len = 6;
		cp.data[0] = va_arg(ap, int);		/* dcid */
		cp.data[0] = htole16(cp.data[0]);
		cp.data[1] = va_arg(ap, int);		/* scid */
		cp.data[1] = htole16(cp.data[1]);
		break;

	default:
		UNKNOWN(reason);
		return EINVAL;
	}

	va_end(ap);

	return l2cap_send_signal(link, L2CAP_COMMAND_REJ, ident, len, &cp);
}

/*
 * Send Connect Request
 */
int
l2cap_send_connect_req(struct l2cap_channel *chan)
{
	l2cap_con_req_cp cp;
	int err;

	err = l2cap_request_alloc(chan, L2CAP_CONNECT_REQ);
	if (err)
		return err;

	cp.psm = htole16(chan->lc_raddr.bt_psm);
	cp.scid = htole16(chan->lc_lcid);

	return l2cap_send_signal(chan->lc_link, L2CAP_CONNECT_REQ,
				chan->lc_link->hl_lastid, sizeof(cp), &cp);
}

/*
 * Send Config Request
 *
 * For outgoing config request, we only put options in the packet if they
 * differ from the default and would have to be actioned. We dont support
 * enough option types to make overflowing SigMTU an issue so it can all
 * go in one packet.
 */
int
l2cap_send_config_req(struct l2cap_channel *chan)
{
	l2cap_cfg_req_cp *cp;
	l2cap_cfg_opt_t *opt;
	l2cap_cfg_opt_val_t *val;
	uint8_t *next, buf[L2CAP_MTU_MINIMUM];
	int err;

	err = l2cap_request_alloc(chan, L2CAP_CONFIG_REQ);
	if (err)
		return err;

	/* Config Header (4 octets) */
	cp = (l2cap_cfg_req_cp *)buf;
	cp->dcid = htole16(chan->lc_rcid);
	cp->flags = 0;	/* "No Continuation" */

	next = buf + sizeof(l2cap_cfg_req_cp);

	/* Incoming MTU (4 octets) */
	if (chan->lc_imtu != L2CAP_MTU_DEFAULT) {
		opt = (l2cap_cfg_opt_t *)next;
		opt->type = L2CAP_OPT_MTU;
		opt->length = L2CAP_OPT_MTU_SIZE;

		val = (l2cap_cfg_opt_val_t *)(opt + 1);
		val->mtu = htole16(chan->lc_imtu);

		next += sizeof(l2cap_cfg_opt_t) + L2CAP_OPT_MTU_SIZE;
	}

	/* Flush Timeout (4 octets) */
	if (chan->lc_flush != L2CAP_FLUSH_TIMO_DEFAULT) {
		opt = (l2cap_cfg_opt_t *)next;
		opt->type = L2CAP_OPT_FLUSH_TIMO;
		opt->length = L2CAP_OPT_FLUSH_TIMO_SIZE;

		val = (l2cap_cfg_opt_val_t *)(opt + 1);
		val->flush_timo = htole16(chan->lc_flush);

		next += sizeof(l2cap_cfg_opt_t) + L2CAP_OPT_FLUSH_TIMO_SIZE;
	}

	/* Outgoing QoS Flow (24 octets) */
	/* Retransmission & Flow Control (11 octets) */
	/*
	 * From here we need to start paying attention to SigMTU as we have
	 * possibly overflowed the minimum supported..
	 */

	return l2cap_send_signal(chan->lc_link, L2CAP_CONFIG_REQ,
				    chan->lc_link->hl_lastid, (int)(next - buf), buf);
}

/*
 * Send Disconnect Request
 */
int
l2cap_send_disconnect_req(struct l2cap_channel *chan)
{
	l2cap_discon_req_cp cp;
	int err;

	err = l2cap_request_alloc(chan, L2CAP_DISCONNECT_REQ);
	if (err)
		return err;

	cp.dcid = htole16(chan->lc_rcid);
	cp.scid = htole16(chan->lc_lcid);

	return l2cap_send_signal(chan->lc_link, L2CAP_DISCONNECT_REQ,
				    chan->lc_link->hl_lastid, sizeof(cp), &cp);
}

/*
 * Send Connect Response
 */
int
l2cap_send_connect_rsp(struct hci_link *link, uint8_t ident, uint16_t dcid,
    uint16_t scid, uint16_t result)
{
	l2cap_con_rsp_cp cp;

	memset(&cp, 0, sizeof(cp));
	cp.dcid = htole16(dcid);
	cp.scid = htole16(scid);
	cp.result = htole16(result);

	return l2cap_send_signal(link, L2CAP_CONNECT_RSP, ident, sizeof(cp), &cp);
}
