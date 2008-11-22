/*	$OpenBSD: hci_event.c,v 1.8 2008/11/22 04:42:58 uwe Exp $	*/
/*	$NetBSD: hci_event.c,v 1.18 2008/04/24 11:38:37 ad Exp $	*/

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
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/proc.h>
#include <sys/systm.h>

#include <netbt/bluetooth.h>
#include <netbt/hci.h>
#include <netbt/sco.h>

static void hci_event_inquiry_result(struct hci_unit *, struct mbuf *);
static void hci_event_rssi_result(struct hci_unit *, struct mbuf *);
static void hci_event_command_status(struct hci_unit *, struct mbuf *);
static void hci_event_command_compl(struct hci_unit *, struct mbuf *);
static void hci_event_con_compl(struct hci_unit *, struct mbuf *);
static void hci_event_discon_compl(struct hci_unit *, struct mbuf *);
static void hci_event_con_req(struct hci_unit *, struct mbuf *);
static void hci_event_num_compl_pkts(struct hci_unit *, struct mbuf *);
static void hci_event_auth_compl(struct hci_unit *, struct mbuf *);
static void hci_event_encryption_change(struct hci_unit *, struct mbuf *);
static void hci_event_change_con_link_key_compl(struct hci_unit *, struct mbuf *);
static void hci_event_read_clock_offset_compl(struct hci_unit *, struct mbuf *);
static void hci_cmd_read_bdaddr(struct hci_unit *, struct mbuf *);
static void hci_cmd_read_buffer_size(struct hci_unit *, struct mbuf *);
static void hci_cmd_read_local_features(struct hci_unit *, struct mbuf *);
static void hci_cmd_read_local_ver(struct hci_unit *, struct mbuf *);
static void hci_cmd_read_local_commands(struct hci_unit *, struct mbuf *);
static void hci_cmd_reset(struct hci_unit *, struct mbuf *);
static void hci_cmd_create_con(struct hci_unit *unit, uint8_t status);

#ifdef BLUETOOTH_DEBUG
int bluetooth_debug;

static const char *hci_eventnames[] = {
/* 0x00 */ "NULL",
/* 0x01 */ "INQUIRY COMPLETE",
/* 0x02 */ "INQUIRY RESULT",
/* 0x03 */ "CONN COMPLETE",
/* 0x04 */ "CONN REQ",
/* 0x05 */ "DISCONN COMPLETE",
/* 0x06 */ "AUTH COMPLETE",
/* 0x07 */ "REMOTE NAME REQ COMPLETE",
/* 0x08 */ "ENCRYPTION CHANGE",
/* 0x09 */ "CHANGE CONN LINK KEY COMPLETE",
/* 0x0a */ "MASTER LINK KEY COMPLETE",
/* 0x0b */ "READ REMOTE FEATURES COMPLETE",
/* 0x0c */ "READ REMOTE VERSION INFO COMPLETE",
/* 0x0d */ "QoS SETUP COMPLETE",
/* 0x0e */ "COMMAND COMPLETE",
/* 0x0f */ "COMMAND STATUS",
/* 0x10 */ "HARDWARE ERROR",
/* 0x11 */ "FLUSH OCCUR",
/* 0x12 */ "ROLE CHANGE",
/* 0x13 */ "NUM COMPLETED PACKETS",
/* 0x14 */ "MODE CHANGE",
/* 0x15 */ "RETURN LINK KEYS",
/* 0x16 */ "PIN CODE REQ",
/* 0x17 */ "LINK KEY REQ",
/* 0x18 */ "LINK KEY NOTIFICATION",
/* 0x19 */ "LOOPBACK COMMAND",
/* 0x1a */ "DATA BUFFER OVERFLOW",
/* 0x1b */ "MAX SLOT CHANGE",
/* 0x1c */ "READ CLOCK OFFSET COMPLETE",
/* 0x1d */ "CONN PKT TYPE CHANGED",
/* 0x1e */ "QOS VIOLATION",
/* 0x1f */ "PAGE SCAN MODE CHANGE",
/* 0x20 */ "PAGE SCAN REP MODE CHANGE",
/* 0x21 */ "FLOW SPECIFICATION COMPLETE",
/* 0x22 */ "RSSI RESULT",
/* 0x23 */ "READ REMOTE EXT FEATURES",
/* 0x24 */ "UNKNOWN",
/* 0x25 */ "UNKNOWN",
/* 0x26 */ "UNKNOWN",
/* 0x27 */ "UNKNOWN",
/* 0x28 */ "UNKNOWN",
/* 0x29 */ "UNKNOWN",
/* 0x2a */ "UNKNOWN",
/* 0x2b */ "UNKNOWN",
/* 0x2c */ "SCO CON COMPLETE",
/* 0x2d */ "SCO CON CHANGED",
/* 0x2e */ "SNIFF SUBRATING",
/* 0x2f */ "EXTENDED INQUIRY RESULT",
/* 0x30 */ "ENCRYPTION KEY REFRESH",
/* 0x31 */ "IO CAPABILITY REQUEST",
/* 0x32 */ "IO CAPABILITY RESPONSE",
/* 0x33 */ "USER CONFIRM REQUEST",
/* 0x34 */ "USER PASSKEY REQUEST",
/* 0x35 */ "REMOTE OOB DATA REQUEST",
/* 0x36 */ "SIMPLE PAIRING COMPLETE",
/* 0x37 */ "UNKNOWN",
/* 0x38 */ "LINK SUPERVISION TIMEOUT CHANGED",
/* 0x39 */ "ENHANCED FLUSH COMPLETE",
/* 0x3a */ "UNKNOWN",
/* 0x3b */ "USER PASSKEY NOTIFICATION",
/* 0x3c */ "KEYPRESS NOTIFICATION",
/* 0x3d */ "REMOTE HOST FEATURES NOTIFICATION",
};

static const char *
hci_eventstr(unsigned int event)
{

	if (event < (sizeof(hci_eventnames) / sizeof(*hci_eventnames)))
		return hci_eventnames[event];

	switch (event) {
	case HCI_EVENT_BT_LOGO:		/* 0xfe */
		return "BT_LOGO";

	case HCI_EVENT_VENDOR:		/* 0xff */
		return "VENDOR";
	}

	return "UNKNOWN";
}
#endif	/* BLUETOOTH_DEBUG */

/*
 * process HCI Events
 *
 * We will free the mbuf at the end, no need for any sub
 * functions to handle that. We kind of assume that the
 * device sends us valid events.
 */
void
hci_event(struct mbuf *m, struct hci_unit *unit)
{
	hci_event_hdr_t hdr;

	KASSERT(m->m_flags & M_PKTHDR);

	KASSERT(m->m_pkthdr.len >= sizeof(hdr));
	m_copydata(m, 0, sizeof(hdr), (caddr_t)&hdr);
	m_adj(m, sizeof(hdr));

	KASSERT(hdr.type == HCI_EVENT_PKT);

	DPRINTFN(1, "(%s) event %s\n",
	    device_xname(unit->hci_dev), hci_eventstr(hdr.event));

	switch(hdr.event) {
	case HCI_EVENT_COMMAND_STATUS:
		hci_event_command_status(unit, m);
		break;

	case HCI_EVENT_COMMAND_COMPL:
		hci_event_command_compl(unit, m);
		break;

	case HCI_EVENT_NUM_COMPL_PKTS:
		hci_event_num_compl_pkts(unit, m);
		break;

	case HCI_EVENT_INQUIRY_RESULT:
		hci_event_inquiry_result(unit, m);
		break;

	case HCI_EVENT_RSSI_RESULT:
		hci_event_rssi_result(unit, m);
		break;

	case HCI_EVENT_CON_COMPL:
		hci_event_con_compl(unit, m);
		break;

	case HCI_EVENT_DISCON_COMPL:
		hci_event_discon_compl(unit, m);
		break;

	case HCI_EVENT_CON_REQ:
		hci_event_con_req(unit, m);
		break;

	case HCI_EVENT_AUTH_COMPL:
		hci_event_auth_compl(unit, m);
		break;

	case HCI_EVENT_ENCRYPTION_CHANGE:
		hci_event_encryption_change(unit, m);
		break;

	case HCI_EVENT_CHANGE_CON_LINK_KEY_COMPL:
		hci_event_change_con_link_key_compl(unit, m);
		break;

	case HCI_EVENT_READ_CLOCK_OFFSET_COMPL:
		hci_event_read_clock_offset_compl(unit, m);
		break;

	default:
		break;
	}

	m_freem(m);
}

/*
 * Command Status
 *
 * Restart command queue and post-process any pending commands
 */
static void
hci_event_command_status(struct hci_unit *unit, struct mbuf *m)
{
	hci_command_status_ep ep;

	KASSERT(m->m_pkthdr.len >= sizeof(ep));
	m_copydata(m, 0, sizeof(ep), (caddr_t)&ep);
	m_adj(m, sizeof(ep));

	ep.opcode = letoh16(ep.opcode);

	DPRINTFN(1, "(%s) opcode (%03x|%04x) status = 0x%x num_cmd_pkts = %d\n",
		device_xname(unit->hci_dev),
		HCI_OGF(ep.opcode), HCI_OCF(ep.opcode),
		ep.status,
		ep.num_cmd_pkts);

	hci_num_cmds(unit, ep.num_cmd_pkts);

	/*
	 * post processing of pending commands
	 */
	switch(ep.opcode) {
	case HCI_CMD_CREATE_CON:
		hci_cmd_create_con(unit, ep.status);
		break;

 	default:
		if (ep.status == 0)
			break;

		DPRINTFN(1,
		    "CommandStatus opcode (%03x|%04x) failed (status=0x%02x)\n",
		    device_xname(unit->hci_dev),
		    HCI_OGF(ep.opcode), HCI_OCF(ep.opcode),
		    ep.status);

 		break;
	}
}

/*
 * Command Complete
 *
 * Restart command queue and handle the completed command
 */
static void
hci_event_command_compl(struct hci_unit *unit, struct mbuf *m)
{
	hci_command_compl_ep ep;
	hci_status_rp rp;

	KASSERT(m->m_pkthdr.len >= sizeof(ep));
	m_copydata(m, 0, sizeof(ep), (caddr_t)&ep);
	m_adj(m, sizeof(ep));

	DPRINTFN(1, "(%s) opcode (%03x|%04x) num_cmd_pkts = %d\n",
	    device_xname(unit->hci_dev), HCI_OGF(letoh16(ep.opcode)),
	    HCI_OCF(letoh16(ep.opcode)), ep.num_cmd_pkts);

	hci_num_cmds(unit, ep.num_cmd_pkts);

	/*
	 * I am not sure if this is completely correct, it is not guaranteed
	 * that a command_complete packet will contain the status though most
	 * do seem to.
	 */
	m_copydata(m, 0, sizeof(rp), (caddr_t)&rp);
	if (rp.status > 0)
		printf("%s: CommandComplete opcode (%03x|%04x) failed (status=0x%02x)\n",
		    device_xname(unit->hci_dev), HCI_OGF(letoh16(ep.opcode)),
		    HCI_OCF(letoh16(ep.opcode)), rp.status);

	/*
	 * post processing of completed commands
	 */
	switch(letoh16(ep.opcode)) {
	case HCI_CMD_READ_BDADDR:
		hci_cmd_read_bdaddr(unit, m);
		break;

	case HCI_CMD_READ_BUFFER_SIZE:
		hci_cmd_read_buffer_size(unit, m);
		break;

	case HCI_CMD_READ_LOCAL_FEATURES:
		hci_cmd_read_local_features(unit, m);
		break;

	case HCI_CMD_READ_LOCAL_VER:
		hci_cmd_read_local_ver(unit, m);
		break;

	case HCI_CMD_READ_LOCAL_COMMANDS:
		hci_cmd_read_local_commands(unit, m);
		break;

	case HCI_CMD_RESET:
		hci_cmd_reset(unit, m);
		break;

	default:
		break;
	}
}

/*
 * Number of Completed Packets
 *
 * This is sent periodically by the Controller telling us how many
 * buffers are now freed up and which handle was using them. From
 * this we determine which type of buffer it was and add the qty
 * back into the relevant packet counter, then restart output on
 * links that have halted.
 */
static void
hci_event_num_compl_pkts(struct hci_unit *unit, struct mbuf *m)
{
	hci_num_compl_pkts_ep ep;
	struct hci_link *link, *next;
	uint16_t handle, num;
	int num_acl = 0, num_sco = 0;

	KASSERT(m->m_pkthdr.len >= sizeof(ep));
	m_copydata(m, 0, sizeof(ep), (caddr_t)&ep);
	m_adj(m, sizeof(ep));

	while (ep.num_con_handles--) {
		m_copydata(m, 0, sizeof(handle), (caddr_t)&handle);
		m_adj(m, sizeof(handle));
		handle = letoh16(handle);

		m_copydata(m, 0, sizeof(num), (caddr_t)&num);
		m_adj(m, sizeof(num));
		num = letoh16(num);

		link = hci_link_lookup_handle(unit, handle);
		if (link) {
			if (link->hl_type == HCI_LINK_ACL) {
				num_acl += num;
				hci_acl_complete(link, num);
			} else {
				num_sco += num;
				hci_sco_complete(link, num);
			}
		} else {
			/* XXX need to issue Read_Buffer_Size or Reset? */
			printf("%s: unknown handle %d! "
			    "(losing track of %d packet buffer%s)\n",
			    device_xname(unit->hci_dev), handle,
			    num, (num == 1 ? "" : "s"));
		}
	}

	/*
	 * Move up any queued packets. When a link has sent data, it will move
	 * to the back of the queue - technically then if a link had something
	 * to send and there were still buffers available it could get started
	 * twice but it seemed more important to to handle higher loads fairly
	 * than worry about wasting cycles when we are not busy.
	 */

	unit->hci_num_acl_pkts += num_acl;
	unit->hci_num_sco_pkts += num_sco;

	link = TAILQ_FIRST(&unit->hci_links);
	while (link && (unit->hci_num_acl_pkts > 0 || unit->hci_num_sco_pkts > 0)) {
		next = TAILQ_NEXT(link, hl_next);

		if (link->hl_type == HCI_LINK_ACL) {
			if (unit->hci_num_acl_pkts > 0 && link->hl_txqlen > 0)
				hci_acl_start(link);
		} else {
			if (unit->hci_num_sco_pkts > 0 && link->hl_txqlen > 0)
				hci_sco_start(link);
		}

		link = next;
	}
}

/*
 * Inquiry Result
 *
 * keep a note of devices seen, so we know which unit to use
 * on outgoing connections
 */
static void
hci_event_inquiry_result(struct hci_unit *unit, struct mbuf *m)
{
	hci_inquiry_result_ep ep;
	hci_inquiry_response ir;
	struct hci_memo *memo;

	KASSERT(m->m_pkthdr.len >= sizeof(ep));
	m_copydata(m, 0, sizeof(ep), (caddr_t)&ep);
	m_adj(m, sizeof(ep));

	DPRINTFN(1, "(%s) %d response%s\n", device_xname(unit->hci_dev),
	    ep.num_responses, (ep.num_responses == 1 ? "" : "s"));

	while(ep.num_responses--) {
		KASSERT(m->m_pkthdr.len >= sizeof(ir));
		m_copydata(m, 0, sizeof(ir), (caddr_t)&ir);
		m_adj(m, sizeof(ir));

		DPRINTFN(1, "bdaddr %02x:%02x:%02x:%02x:%02x:%02x\n",
			ir.bdaddr.b[5], ir.bdaddr.b[4], ir.bdaddr.b[3],
			ir.bdaddr.b[2], ir.bdaddr.b[1], ir.bdaddr.b[0]);

		memo = hci_memo_new(unit, &ir.bdaddr);
		if (memo != NULL) {
			memo->page_scan_rep_mode = ir.page_scan_rep_mode;
			memo->page_scan_mode = ir.page_scan_mode;
			memo->clock_offset = ir.clock_offset;
		}
	}
}

/*
 * Inquiry Result with RSSI
 *
 * as above but different packet when RSSI result is enabled
 */
static void
hci_event_rssi_result(struct hci_unit *unit, struct mbuf *m)
{
	hci_rssi_result_ep ep;
	hci_rssi_response rr;
	struct hci_memo *memo;

	KASSERT(m->m_pkthdr.len >= sizeof(ep));
	m_copydata(m, 0, sizeof(ep), (caddr_t)&ep);
	m_adj(m, sizeof(ep));

	DPRINTFN(1, "%d response%s\n", ep.num_responses,
				(ep.num_responses == 1 ? "" : "s"));

	while(ep.num_responses--) {
		KASSERT(m->m_pkthdr.len >= sizeof(rr));
		m_copydata(m, 0, sizeof(rr), (caddr_t)&rr);
		m_adj(m, sizeof(rr));

		DPRINTFN(1, "bdaddr %02x:%02x:%02x:%02x:%02x:%02x\n",
			rr.bdaddr.b[5], rr.bdaddr.b[4], rr.bdaddr.b[3],
			rr.bdaddr.b[2], rr.bdaddr.b[1], rr.bdaddr.b[0]);

		memo = hci_memo_new(unit, &rr.bdaddr);
		if (memo != NULL) {
			memo->page_scan_rep_mode = rr.page_scan_rep_mode;
			memo->page_scan_mode = 0;
			memo->clock_offset = rr.clock_offset;
		}
	}
}

/*
 * Connection Complete
 *
 * Sent to us when a connection is made. If there is no link
 * structure already allocated for this, we must have changed
 * our mind, so just disconnect.
 */
static void
hci_event_con_compl(struct hci_unit *unit, struct mbuf *m)
{
	hci_con_compl_ep ep;
	hci_write_link_policy_settings_cp cp;
	struct hci_link *link;
	int err;

	KASSERT(m->m_pkthdr.len >= sizeof(ep));
	m_copydata(m, 0, sizeof(ep), (caddr_t)&ep);
	m_adj(m, sizeof(ep));

	DPRINTFN(1, "(%s) %s connection complete for "
		"%02x:%02x:%02x:%02x:%02x:%02x status %#x\n",
		device_xname(unit->hci_dev),
		(ep.link_type == HCI_LINK_ACL ? "ACL" : "SCO"),
		ep.bdaddr.b[5], ep.bdaddr.b[4], ep.bdaddr.b[3],
		ep.bdaddr.b[2], ep.bdaddr.b[1], ep.bdaddr.b[0],
		ep.status);

	link = hci_link_lookup_bdaddr(unit, &ep.bdaddr, ep.link_type);

	if (ep.status) {
		if (link != NULL) {
			switch (ep.status) {
			case 0x04: /* "Page Timeout" */
				err = EHOSTDOWN;
				break;

			case 0x08: /* "Connection Timed Out" */
			case 0x10: /* "Connection Accept Timeout Exceeded" */
				err = ETIMEDOUT;
				break;

			case 0x16: /* "Connection Terminated by Local Host" */
				err = 0;
				break;

			default:
				err = ECONNREFUSED;
				break;
			}

			hci_link_free(link, err);
		}

		return;
	}

	if (link == NULL) {
		hci_discon_cp dp;

		dp.con_handle = ep.con_handle;
		dp.reason = 0x13; /* "Remote User Terminated Connection" */

		hci_send_cmd(unit, HCI_CMD_DISCONNECT, &dp, sizeof(dp));
		return;
	}

	/* XXX could check auth_enable here */

	if (ep.encryption_mode)
		link->hl_flags |= (HCI_LINK_AUTH | HCI_LINK_ENCRYPT);

	link->hl_state = HCI_LINK_OPEN;
	link->hl_handle = HCI_CON_HANDLE(letoh16(ep.con_handle));

	if (ep.link_type == HCI_LINK_ACL) {
		cp.con_handle = ep.con_handle;
		cp.settings = htole16(unit->hci_link_policy);
		err = hci_send_cmd(unit, HCI_CMD_WRITE_LINK_POLICY_SETTINGS,
						&cp, sizeof(cp));
		if (err)
			printf("%s: Warning, could not write link policy\n",
			    device_xname(unit->hci_dev));

		err = hci_send_cmd(unit, HCI_CMD_READ_CLOCK_OFFSET,
				    &cp.con_handle, sizeof(cp.con_handle));
		if (err)
			printf("%s: Warning, could not read clock offset\n",
			    device_xname(unit->hci_dev));

		err = hci_acl_setmode(link);
		if (err == EINPROGRESS)
			return;

		hci_acl_linkmode(link);
	} else {
		(*link->hl_sco->sp_proto->connected)(link->hl_sco->sp_upper);
	}
}

/*
 * Disconnection Complete
 *
 * This is sent in response to a disconnection request, but also if
 * the remote device goes out of range.
 */
static void
hci_event_discon_compl(struct hci_unit *unit, struct mbuf *m)
{
	hci_discon_compl_ep ep;
	struct hci_link *link;

	KASSERT(m->m_pkthdr.len >= sizeof(ep));
	m_copydata(m, 0, sizeof(ep), (caddr_t)&ep);
	m_adj(m, sizeof(ep));

	ep.con_handle = letoh16(ep.con_handle);

	DPRINTFN(1, "(%s) handle #%d, status=0x%x\n",
	    device_xname(unit->hci_dev), ep.con_handle, ep.status);

	link = hci_link_lookup_handle(unit, HCI_CON_HANDLE(ep.con_handle));
	if (link)
		hci_link_free(link, ENOENT); /* XXX NetBSD used ENOLINK here */
}

/*
 * Connect Request
 *
 * We check upstream for appropriate listeners and accept connections
 * that are wanted.
 */
static void
hci_event_con_req(struct hci_unit *unit, struct mbuf *m)
{
	hci_con_req_ep ep;
	hci_accept_con_cp ap;
	hci_reject_con_cp rp;
	struct hci_link *link;

	KASSERT(m->m_pkthdr.len >= sizeof(ep));
	m_copydata(m, 0, sizeof(ep), (caddr_t)&ep);
	m_adj(m, sizeof(ep));

	DPRINTFN(1, "(%s) bdaddr %2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x "
		"class %2.2x%2.2x%2.2x type %s\n",
	    device_xname(unit->hci_dev),
	    ep.bdaddr.b[5], ep.bdaddr.b[4], ep.bdaddr.b[3],
	    ep.bdaddr.b[2], ep.bdaddr.b[1], ep.bdaddr.b[0],
	    ep.uclass[0], ep.uclass[1], ep.uclass[2],
	    ep.link_type == HCI_LINK_ACL ? "ACL" : "SCO");

	if (ep.link_type == HCI_LINK_ACL)
		link = hci_acl_newconn(unit, &ep.bdaddr);
	else
		link = hci_sco_newconn(unit, &ep.bdaddr);

	if (link == NULL) {
		memset(&rp, 0, sizeof(rp));
		bdaddr_copy(&rp.bdaddr, &ep.bdaddr);
		rp.reason = 0x0f;	/* Unacceptable BD_ADDR */

		hci_send_cmd(unit, HCI_CMD_REJECT_CON, &rp, sizeof(rp));
	} else {
		memset(&ap, 0, sizeof(ap));
		bdaddr_copy(&ap.bdaddr, &ep.bdaddr);
		if (unit->hci_link_policy & HCI_LINK_POLICY_ENABLE_ROLE_SWITCH)
			ap.role = HCI_ROLE_MASTER;
		else
			ap.role = HCI_ROLE_SLAVE;

		hci_send_cmd(unit, HCI_CMD_ACCEPT_CON, &ap, sizeof(ap));
	}
}

/*
 * Auth Complete
 *
 * Authentication has been completed on an ACL link. We can notify the
 * upper layer protocols unless further mode changes are pending.
 */
static void
hci_event_auth_compl(struct hci_unit *unit, struct mbuf *m)
{
	hci_auth_compl_ep ep;
	struct hci_link *link;
	int err;

	KASSERT(m->m_pkthdr.len >= sizeof(ep));
	m_copydata(m, 0, sizeof(ep), (caddr_t)&ep);
	m_adj(m, sizeof(ep));

	ep.con_handle = HCI_CON_HANDLE(letoh16(ep.con_handle));

	DPRINTFN(1, "(%s) handle #%d, status=0x%x\n",
	    device_xname(unit->hci_dev), ep.con_handle, ep.status);

	link = hci_link_lookup_handle(unit, ep.con_handle);
	if (link == NULL || link->hl_type != HCI_LINK_ACL)
		return;

	if (ep.status == 0) {
		link->hl_flags |= HCI_LINK_AUTH;

		if (link->hl_state == HCI_LINK_WAIT_AUTH)
			link->hl_state = HCI_LINK_OPEN;

		err = hci_acl_setmode(link);
		if (err == EINPROGRESS)
			return;
	}

	hci_acl_linkmode(link);
}

/*
 * Encryption Change
 *
 * The encryption status has changed. Basically, we note the change
 * then notify the upper layer protocol unless further mode changes
 * are pending.
 * Note that if encryption gets disabled when it has been requested,
 * we will attempt to enable it again.. (its a feature not a bug :)
 */
static void
hci_event_encryption_change(struct hci_unit *unit, struct mbuf *m)
{
	hci_encryption_change_ep ep;
	struct hci_link *link;
	int err;

	KASSERT(m->m_pkthdr.len >= sizeof(ep));
	m_copydata(m, 0, sizeof(ep), (caddr_t)&ep);
	m_adj(m, sizeof(ep));

	ep.con_handle = HCI_CON_HANDLE(letoh16(ep.con_handle));

	DPRINTFN(1, "(%s) handle #%d, status=0x%x, encryption_enable=0x%x\n",
	    device_xname(unit->hci_dev), ep.con_handle, ep.status,
	    ep.encryption_enable);

	link = hci_link_lookup_handle(unit, ep.con_handle);
	if (link == NULL || link->hl_type != HCI_LINK_ACL)
		return;

	if (ep.status == 0) {
		if (ep.encryption_enable == 0)
			link->hl_flags &= ~HCI_LINK_ENCRYPT;
		else
			link->hl_flags |= (HCI_LINK_AUTH | HCI_LINK_ENCRYPT);

		if (link->hl_state == HCI_LINK_WAIT_ENCRYPT)
			link->hl_state = HCI_LINK_OPEN;

		err = hci_acl_setmode(link);
		if (err == EINPROGRESS)
			return;
	}

	hci_acl_linkmode(link);
}

/*
 * Change Connection Link Key Complete
 *
 * Link keys are handled in userland but if we are waiting to secure
 * this link, we should notify the upper protocols. A SECURE request
 * only needs a single key change, so we can cancel the request.
 */
static void
hci_event_change_con_link_key_compl(struct hci_unit *unit, struct mbuf *m)
{
	hci_change_con_link_key_compl_ep ep;
	struct hci_link *link;
	int err;

	KASSERT(m->m_pkthdr.len >= sizeof(ep));
	m_copydata(m, 0, sizeof(ep), (caddr_t)&ep);
	m_adj(m, sizeof(ep));

	ep.con_handle = HCI_CON_HANDLE(letoh16(ep.con_handle));

	DPRINTFN(1, "(%s) handle #%d, status=0x%x\n",
	    device_xname(unit->hci_dev), ep.con_handle, ep.status);

	link = hci_link_lookup_handle(unit, ep.con_handle);
	if (link == NULL || link->hl_type != HCI_LINK_ACL)
		return;

	link->hl_flags &= ~HCI_LINK_SECURE_REQ;

	if (ep.status == 0) {
		link->hl_flags |= (HCI_LINK_AUTH | HCI_LINK_SECURE);

		if (link->hl_state == HCI_LINK_WAIT_SECURE)
			link->hl_state = HCI_LINK_OPEN;

		err = hci_acl_setmode(link);
		if (err == EINPROGRESS)
			return;
	}

	hci_acl_linkmode(link);
}

/*
 * Read Clock Offset Complete
 *
 * We keep a note of the clock offset of remote devices when a
 * link is made, in order to facilitate reconnections to the device
 */
static void
hci_event_read_clock_offset_compl(struct hci_unit *unit, struct mbuf *m)
{
	hci_read_clock_offset_compl_ep ep;
	struct hci_link *link;

	KASSERT(m->m_pkthdr.len >= sizeof(ep));
	m_copydata(m, 0, sizeof(ep), (caddr_t)&ep);
	m_adj(m, sizeof(ep));

	DPRINTFN(1, "handle #%d, offset=%u, status=0x%x\n",
	    letoh16(ep.con_handle), letoh16(ep.clock_offset), ep.status);

	ep.con_handle = HCI_CON_HANDLE(letoh16(ep.con_handle));
	link = hci_link_lookup_handle(unit, ep.con_handle);

	if (ep.status != 0 || link == NULL)
		return;

	link->hl_clock = ep.clock_offset;
}

/*
 * process results of read_bdaddr command_complete event
 */
static void
hci_cmd_read_bdaddr(struct hci_unit *unit, struct mbuf *m)
{
	hci_read_bdaddr_rp rp;

	KASSERT(m->m_pkthdr.len >= sizeof(rp));
	m_copydata(m, 0, sizeof(rp), (caddr_t)&rp);
	m_adj(m, sizeof(rp));

	if (rp.status > 0)
		return;

	if ((unit->hci_flags & BTF_INIT_BDADDR) == 0)
		return;

	bdaddr_copy(&unit->hci_bdaddr, &rp.bdaddr);

	unit->hci_flags &= ~BTF_INIT_BDADDR;

	wakeup(&unit->hci_init);
}

/*
 * process results of read_buffer_size command_complete event
 */
static void
hci_cmd_read_buffer_size(struct hci_unit *unit, struct mbuf *m)
{
	hci_read_buffer_size_rp rp;

	KASSERT(m->m_pkthdr.len >= sizeof(rp));
	m_copydata(m, 0, sizeof(rp), (caddr_t)&rp);
	m_adj(m, sizeof(rp));

	if (rp.status > 0)
		return;

	if ((unit->hci_flags & BTF_INIT_BUFFER_SIZE) == 0)
		return;

	unit->hci_max_acl_size = letoh16(rp.max_acl_size);
	unit->hci_num_acl_pkts = letoh16(rp.num_acl_pkts);
	unit->hci_max_sco_size = rp.max_sco_size;
	unit->hci_num_sco_pkts = letoh16(rp.num_sco_pkts);

	unit->hci_flags &= ~BTF_INIT_BUFFER_SIZE;

	wakeup(&unit->hci_init);
}

/*
 * process results of read_local_features command_complete event
 */
static void
hci_cmd_read_local_features(struct hci_unit *unit, struct mbuf *m)
{
	hci_read_local_features_rp rp;

	KASSERT(m->m_pkthdr.len >= sizeof(rp));
	m_copydata(m, 0, sizeof(rp), (caddr_t)&rp);
	m_adj(m, sizeof(rp));

	if (rp.status > 0)
		return;

	if ((unit->hci_flags & BTF_INIT_FEATURES) == 0)
		return;

	unit->hci_lmp_mask = 0;

	if (rp.features[0] & HCI_LMP_ROLE_SWITCH)
		unit->hci_lmp_mask |= HCI_LINK_POLICY_ENABLE_ROLE_SWITCH;

	if (rp.features[0] & HCI_LMP_HOLD_MODE)
		unit->hci_lmp_mask |= HCI_LINK_POLICY_ENABLE_HOLD_MODE;

	if (rp.features[0] & HCI_LMP_SNIFF_MODE)
		unit->hci_lmp_mask |= HCI_LINK_POLICY_ENABLE_SNIFF_MODE;

	if (rp.features[1] & HCI_LMP_PARK_MODE)
		unit->hci_lmp_mask |= HCI_LINK_POLICY_ENABLE_PARK_MODE;

	/* ACL packet mask */
	unit->hci_acl_mask = HCI_PKT_DM1 | HCI_PKT_DH1;

	if (rp.features[0] & HCI_LMP_3SLOT)
		unit->hci_acl_mask |= HCI_PKT_DM3 | HCI_PKT_DH3;

	if (rp.features[0] & HCI_LMP_5SLOT)
		unit->hci_acl_mask |= HCI_PKT_DM5 | HCI_PKT_DH5;

	if ((rp.features[3] & HCI_LMP_EDR_ACL_2MBPS) == 0)
		unit->hci_acl_mask |= HCI_PKT_2MBPS_DH1
				    | HCI_PKT_2MBPS_DH3
				    | HCI_PKT_2MBPS_DH5;

	if ((rp.features[3] & HCI_LMP_EDR_ACL_3MBPS) == 0)
		unit->hci_acl_mask |= HCI_PKT_3MBPS_DH1
				    | HCI_PKT_3MBPS_DH3
				    | HCI_PKT_3MBPS_DH5;

	if ((rp.features[4] & HCI_LMP_3SLOT_EDR_ACL) == 0)
		unit->hci_acl_mask |= HCI_PKT_2MBPS_DH3
				    | HCI_PKT_3MBPS_DH3;

	if ((rp.features[5] & HCI_LMP_5SLOT_EDR_ACL) == 0)
		unit->hci_acl_mask |= HCI_PKT_2MBPS_DH5
				    | HCI_PKT_3MBPS_DH5;

	unit->hci_packet_type = unit->hci_acl_mask;

	/* SCO packet mask */
	unit->hci_sco_mask = 0;
	if (rp.features[1] & HCI_LMP_SCO_LINK)
		unit->hci_sco_mask |= HCI_PKT_HV1;

	if (rp.features[1] & HCI_LMP_HV2_PKT)
		unit->hci_sco_mask |= HCI_PKT_HV2;

	if (rp.features[1] & HCI_LMP_HV3_PKT)
		unit->hci_sco_mask |= HCI_PKT_HV3;

	if (rp.features[3] & HCI_LMP_EV3_PKT)
		unit->hci_sco_mask |= HCI_PKT_EV3;

	if (rp.features[4] & HCI_LMP_EV4_PKT)
		unit->hci_sco_mask |= HCI_PKT_EV4;

	if (rp.features[4] & HCI_LMP_EV5_PKT)
		unit->hci_sco_mask |= HCI_PKT_EV5;

	/* XXX what do 2MBPS/3MBPS/3SLOT eSCO mean? */

	unit->hci_flags &= ~BTF_INIT_FEATURES;

	wakeup(&unit->hci_init);

	DPRINTFN(1, "%s: lmp_mask %4.4x, acl_mask %4.4x, sco_mask %4.4x\n",
		device_xname(unit->hci_dev), unit->hci_lmp_mask,
		unit->hci_acl_mask, unit->hci_sco_mask);
}

/*
 * process results of read_local_ver command_complete event
 *
 * reading local supported commands is only supported from 1.2 spec
 */
static void
hci_cmd_read_local_ver(struct hci_unit *unit, struct mbuf *m)
{
	hci_read_local_ver_rp rp;

	KASSERT(m->m_pkthdr.len >= sizeof(rp));
	m_copydata(m, 0, sizeof(rp), (caddr_t)&rp);
	m_adj(m, sizeof(rp));

	if (rp.status != 0)
		return;

	if ((unit->hci_flags & BTF_INIT_COMMANDS) == 0)
		return;

	if (rp.hci_version < HCI_SPEC_V12) {
		unit->hci_flags &= ~BTF_INIT_COMMANDS;
		wakeup(&unit->hci_init);
		return;
	}

	hci_send_cmd(unit, HCI_CMD_READ_LOCAL_COMMANDS, NULL, 0);
}

/*
 * process results of read_local_commands command_complete event
 */
static void
hci_cmd_read_local_commands(struct hci_unit *unit, struct mbuf *m)
{
	hci_read_local_commands_rp rp;

	KASSERT(m->m_pkthdr.len >= sizeof(rp));
	m_copydata(m, 0, sizeof(rp), (caddr_t)&rp);
	m_adj(m, sizeof(rp));

	if (rp.status != 0)
		return;

	if ((unit->hci_flags & BTF_INIT_COMMANDS) == 0)
		return;

	unit->hci_flags &= ~BTF_INIT_COMMANDS;
	memcpy(unit->hci_cmds, rp.commands, HCI_COMMANDS_SIZE);

	wakeup(&unit->hci_init);
}

/*
 * process results of reset command_complete event
 *
 * This has killed all the connections, so close down anything we have left,
 * and reinitialise the unit.
 */
static void
hci_cmd_reset(struct hci_unit *unit, struct mbuf *m)
{
	hci_reset_rp rp;
	struct hci_link *link, *next;
	int acl;

	KASSERT(m->m_pkthdr.len >= sizeof(rp));
	m_copydata(m, 0, sizeof(rp), (caddr_t)&rp);
	m_adj(m, sizeof(rp));

	if (rp.status != 0)
		return;

	/*
	 * release SCO links first, since they may be holding
	 * an ACL link reference.
	 */
	for (acl = 0 ; acl < 2 ; acl++) {
		next = TAILQ_FIRST(&unit->hci_links);
		while ((link = next) != NULL) {
			next = TAILQ_NEXT(link, hl_next);
			if (acl || link->hl_type != HCI_LINK_ACL)
				hci_link_free(link, ECONNABORTED);
		}
	}

	unit->hci_num_acl_pkts = 0;
	unit->hci_num_sco_pkts = 0;

	if (hci_send_cmd(unit, HCI_CMD_READ_BDADDR, NULL, 0))
		return;

	if (hci_send_cmd(unit, HCI_CMD_READ_BUFFER_SIZE, NULL, 0))
		return;

	if (hci_send_cmd(unit, HCI_CMD_READ_LOCAL_FEATURES, NULL, 0))
		return;

	if (hci_send_cmd(unit, HCI_CMD_READ_LOCAL_VER, NULL, 0))
		return;
}

/*
 * process command_status event for create_con command
 *
 * a "Create Connection" command can sometimes fail to start for whatever
 * reason and the command_status event returns failure but we get no
 * indication of which connection failed (for instance in the case where
 * we tried to open too many connections all at once) So, we keep a flag
 * on the link to indicate pending status until the command_status event
 * is returned to help us decide which needs to be failed.
 *
 * since created links are inserted at the tail of hci_links, we know that
 * the first pending link we find will be the one that this command status
 * refers to.
 */
static void
hci_cmd_create_con(struct hci_unit *unit, uint8_t status)
{
	struct hci_link *link;

	TAILQ_FOREACH(link, &unit->hci_links, hl_next) {
		if ((link->hl_flags & HCI_LINK_CREATE_CON) == 0)
			continue;

		link->hl_flags &= ~HCI_LINK_CREATE_CON;

		switch(status) {
		case 0x00:	/* success */
			break;

		case 0x0c:	/* "Command Disallowed" */
			hci_link_free(link, EBUSY);
			break;

		default:	/* some other trouble */
			hci_link_free(link, /*EPROTO*/ECONNABORTED);
			break;
		}

		return;
	}
}
