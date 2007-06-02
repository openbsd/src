/*	$OpenBSD: hci_socket.c,v 1.3 2007/06/02 01:46:01 uwe Exp $	*/
/*	$NetBSD: hci_socket.c,v 1.10 2007/03/31 18:17:13 plunky Exp $	*/

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

/* load symbolic names */
#ifdef BLUETOOTH_DEBUG
#define PRUREQUESTS
#define PRCOREQUESTS
#endif

#include <sys/param.h>
#include <sys/domain.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/proc.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/systm.h>

#include <netbt/bluetooth.h>
#include <netbt/hci.h>

/*******************************************************************************
 *
 * HCI SOCK_RAW Sockets - for control of Bluetooth Devices
 *
 */

/*
 * the raw HCI protocol control block
 */
struct hci_pcb {
	struct socket		*hp_socket;	/* socket */
	unsigned int		hp_flags;	/* flags */
	bdaddr_t		hp_laddr;	/* local address */
	bdaddr_t		hp_raddr;	/* remote address */
	struct hci_filter	hp_efilter;	/* user event filter */
	struct hci_filter	hp_pfilter;	/* user packet filter */
	LIST_ENTRY(hci_pcb)	hp_next;	/* next HCI pcb */
};

/* hp_flags */
#define HCI_PRIVILEGED		(1<<0)	/* no security filter for root */
#define HCI_DIRECTION		(1<<1)	/* direction control messages */
#define HCI_PROMISCUOUS		(1<<2)	/* listen to all units */

LIST_HEAD(hci_pcb_list, hci_pcb) hci_pcb = LIST_HEAD_INITIALIZER(hci_pcb);

/* sysctl defaults */
int hci_sendspace = HCI_CMD_PKT_SIZE;
int hci_recvspace = 4096;

/*
 * Security filter routines for unprivileged users.
 *	Allow all but a few critical events, and only permit read commands.
 */

static int
hci_security_check_opcode(uint16_t opcode)
{

	switch (opcode) {
	/* Link control */
	case HCI_CMD_INQUIRY:
		return sizeof(hci_inquiry_cp);
	case HCI_CMD_REMOTE_NAME_REQ:
		return sizeof(hci_remote_name_req_cp);
	case HCI_CMD_READ_REMOTE_FEATURES:
		return sizeof(hci_read_remote_features_cp);
	case HCI_CMD_READ_REMOTE_EXTENDED_FEATURES:
		return sizeof(hci_read_remote_extended_features_cp);
	case HCI_CMD_READ_REMOTE_VER_INFO:
		return sizeof(hci_read_remote_ver_info_cp);
	case HCI_CMD_READ_CLOCK_OFFSET:
		return sizeof(hci_read_clock_offset_cp);
	case HCI_CMD_READ_LMP_HANDLE:
		return sizeof(hci_read_lmp_handle_cp);

	/* Link policy */
	case HCI_CMD_ROLE_DISCOVERY:
		return sizeof(hci_role_discovery_cp);
	case HCI_CMD_READ_LINK_POLICY_SETTINGS:
		return sizeof(hci_read_link_policy_settings_cp);
	case HCI_CMD_READ_DEFAULT_LINK_POLICY_SETTINGS:
		return 0;	/* No command parameters */

	/* Host controller and baseband */
	case HCI_CMD_READ_PIN_TYPE:
	case HCI_CMD_READ_LOCAL_NAME:
	case HCI_CMD_READ_CON_ACCEPT_TIMEOUT:
	case HCI_CMD_READ_PAGE_TIMEOUT:
	case HCI_CMD_READ_SCAN_ENABLE:
	case HCI_CMD_READ_PAGE_SCAN_ACTIVITY:
	case HCI_CMD_READ_INQUIRY_SCAN_ACTIVITY:
	case HCI_CMD_READ_AUTH_ENABLE:
	case HCI_CMD_READ_ENCRYPTION_MODE:
	case HCI_CMD_READ_UNIT_CLASS:
	case HCI_CMD_READ_VOICE_SETTING:
		return 0;	/* No command parameters */
	case HCI_CMD_READ_AUTO_FLUSH_TIMEOUT:
		return sizeof(hci_read_auto_flush_timeout_cp);
	case HCI_CMD_READ_NUM_BROADCAST_RETRANS:
	case HCI_CMD_READ_HOLD_MODE_ACTIVITY:
		return 0;	/* No command parameters */
	case HCI_CMD_READ_XMIT_LEVEL:
		return sizeof(hci_read_xmit_level_cp);
	case HCI_CMD_READ_SCO_FLOW_CONTROL:
		return 0;	/* No command parameters */
	case HCI_CMD_READ_LINK_SUPERVISION_TIMEOUT:
		return sizeof(hci_read_link_supervision_timeout_cp);
	case HCI_CMD_READ_NUM_SUPPORTED_IAC:
	case HCI_CMD_READ_IAC_LAP:
	case HCI_CMD_READ_PAGE_SCAN_PERIOD:
	case HCI_CMD_READ_PAGE_SCAN:
	case HCI_CMD_READ_INQUIRY_SCAN_TYPE:
	case HCI_CMD_READ_INQUIRY_MODE:
	case HCI_CMD_READ_PAGE_SCAN_TYPE:
	case HCI_CMD_READ_AFH_ASSESSMENT:
		return 0;	/* No command parameters */

	/* Informational */
	case HCI_CMD_READ_LOCAL_VER:
	case HCI_CMD_READ_LOCAL_COMMANDS:
	case HCI_CMD_READ_LOCAL_FEATURES:
		return 0;	/* No command parameters */
	case HCI_CMD_READ_LOCAL_EXTENDED_FEATURES:
		return sizeof(hci_read_local_extended_features_cp);
	case HCI_CMD_READ_BUFFER_SIZE:
	case HCI_CMD_READ_COUNTRY_CODE:
	case HCI_CMD_READ_BDADDR:
		return 0;	/* No command parameters */

	/* Status */
	case HCI_CMD_READ_FAILED_CONTACT_CNTR:
		return sizeof(hci_read_failed_contact_cntr_cp);
	case HCI_CMD_READ_LINK_QUALITY:
		return sizeof(hci_read_link_quality_cp);
	case HCI_CMD_READ_RSSI:
		return sizeof(hci_read_rssi_cp);
	case HCI_CMD_READ_AFH_CHANNEL_MAP:
		return sizeof(hci_read_afh_channel_map_cp);
	case HCI_CMD_READ_CLOCK:
		return sizeof(hci_read_clock_cp);

	/* Testing */
	case HCI_CMD_READ_LOOPBACK_MODE:
		return 0;	/* No command parameters */
	}

	return -1;	/* disallowed */
}

static int
hci_security_check_event(uint8_t event)
{

	switch (event) {
	case HCI_EVENT_RETURN_LINK_KEYS:
	case HCI_EVENT_LINK_KEY_NOTIFICATION:
	case HCI_EVENT_VENDOR:
		return -1;	/* disallowed */
	}

	return 0;	/* ok */
}

/*
 * When command packet reaches the device, we can drop
 * it from the socket buffer (called from hci_output_acl)
 */
void
hci_drop(void *arg)
{
	struct socket *so = arg;

	sbdroprecord(&so->so_snd);
	sowwakeup(so);
}

/*
 * HCI socket is going away and has some pending packets. We let them
 * go by design, but remove the context pointer as it will be invalid
 * and we no longer need to be notified.
 */
static void
hci_cmdwait_flush(struct socket *so)
{
	struct hci_unit *unit;
	struct socket *ctx;
	struct mbuf *m;

	DPRINTF("flushing %p\n", so);

	TAILQ_FOREACH(unit, &hci_unit_list, hci_next) {
		IF_POLL(&unit->hci_cmdwait, m);
		while (m != NULL) {
			ctx = M_GETCTX(m, struct socket *);
			if (ctx == so)
				M_SETCTX(m, NULL);

			m = m->m_nextpkt;
		}
	}
}

/*
 * HCI send packet
 *     This came from userland, so check it out.
 */
static int
hci_send(struct hci_pcb *pcb, struct mbuf *m, bdaddr_t *addr)
{
	struct hci_unit *unit;
	struct mbuf *m0;
	hci_cmd_hdr_t hdr;
	int err;

	KASSERT(m != NULL);
	KASSERT(addr != NULL);

	/* wants at least a header to start with */
	if (m->m_pkthdr.len < sizeof(hdr)) {
		err = EMSGSIZE;
		goto bad;
	}
	m_copydata(m, 0, sizeof(hdr), (caddr_t)&hdr);

	/* only allows CMD packets to be sent */
	if (hdr.type != HCI_CMD_PKT) {
		err = EINVAL;
		goto bad;
	}

	/* validates packet length */
	if (m->m_pkthdr.len != sizeof(hdr) + hdr.length) {
		err = EMSGSIZE;
		goto bad;
	}

	/* security checks for unprivileged users */
	if ((pcb->hp_flags & HCI_PRIVILEGED) == 0
	    && hci_security_check_opcode(letoh16(hdr.opcode)) != hdr.length) {
		err = EPERM;
		goto bad;
	}

	/* finds destination */
	unit = hci_unit_lookup(addr);
	if (unit == NULL) {
		err = ENETDOWN;
		goto bad;
	}

	/* makes a copy for precious to keep */
	m0 = m_copym(m, 0, M_COPYALL, M_DONTWAIT);
	if (m0 == NULL) {
		err = ENOMEM;
		goto bad;
	}
	sbappendrecord(&pcb->hp_socket->so_snd, m0);
	M_SETCTX(m, pcb->hp_socket);	/* enable drop callback */

	DPRINTFN(2, "(%s) opcode (%03x|%04x)\n", unit->hci_devname,
		HCI_OGF(letoh16(hdr.opcode)), HCI_OCF(letoh16(hdr.opcode)));

	/* Sendss it */
	if (unit->hci_num_cmd_pkts == 0)
		IF_ENQUEUE(&unit->hci_cmdwait, m);
	else
		hci_output_cmd(unit, m);

	return 0;

bad:
	DPRINTF("packet (%d bytes) not sent (error %d)\n",
			m->m_pkthdr.len, err);
	if (m) m_freem(m);
	return err;
}

/*
 * User Request.
 * up is socket
 * m is either
 *	optional mbuf chain containing message
 *	ioctl command (PRU_CONTROL)
 * nam is either
 *	optional mbuf chain containing an address
 *	ioctl data (PRU_CONTROL)
 *      optionally, protocol number (PRU_ATTACH)
 * ctl is optional mbuf chain containing socket options
 * l is pointer to process requesting action (if any)
 *
 * we are responsible for disposing of m and ctl if
 * they are mbuf chains
 */
int
hci_usrreq(struct socket *up, int req, struct mbuf *m,
    struct mbuf *nam, struct mbuf *ctl)
{
	struct hci_pcb *pcb = (struct hci_pcb *)up->so_pcb;
	struct sockaddr_bt *sa;
	int err = 0;

#ifdef notyet			/* XXX */
	DPRINTFN(2, "%s\n", prurequests[req]);
#endif

	switch(req) {
	case PRU_CONTROL:
		return hci_ioctl((unsigned long)m, (void *)nam, curproc);

	case PRU_ATTACH:
		if (pcb)
			return EINVAL;

		err = soreserve(up, hci_sendspace, hci_recvspace);
		if (err)
			return err;

		pcb = malloc(sizeof *pcb, M_PCB, M_NOWAIT);
		if (pcb == NULL)
			return ENOMEM;
		bzero(pcb, sizeof *pcb);

		up->so_pcb = pcb;
		pcb->hp_socket = up;

		if (curproc == NULL || suser(curproc, 0) == 0)
			pcb->hp_flags |= HCI_PRIVILEGED;

		/*
		 * Set default user filter. By default, socket only passes
		 * Command_Complete and Command_Status Events.
		 */
		hci_filter_set(HCI_EVENT_COMMAND_COMPL, &pcb->hp_efilter);
		hci_filter_set(HCI_EVENT_COMMAND_STATUS, &pcb->hp_efilter);
		hci_filter_set(HCI_EVENT_PKT, &pcb->hp_pfilter);

		LIST_INSERT_HEAD(&hci_pcb, pcb, hp_next);

		return 0;
	}

	/* anything after here *requires* a pcb */
	if (pcb == NULL) {
		err = EINVAL;
		goto release;
	}

	switch(req) {
	case PRU_DISCONNECT:
		bdaddr_copy(&pcb->hp_raddr, BDADDR_ANY);

		/* XXX we cannot call soisdisconnected() here, as it sets
		 * SS_CANTRCVMORE and SS_CANTSENDMORE. The problem being,
		 * that soisconnected() does not clear these and if you
		 * try to reconnect this socket (which is permitted) you
		 * get a broken pipe when you try to write any data.
		 */
		up->so_state &= ~SS_ISCONNECTED;
		break;

	case PRU_ABORT:
		soisdisconnected(up);
		/* fall through to */
	case PRU_DETACH:
		if (up->so_snd.sb_mb != NULL)
			hci_cmdwait_flush(up);

		up->so_pcb = NULL;
		LIST_REMOVE(pcb, hp_next);
		free(pcb, M_PCB);
		return 0;

	case PRU_BIND:
		KASSERT(nam != NULL);
		sa = mtod(nam, struct sockaddr_bt *);

		if (sa->bt_len != sizeof(struct sockaddr_bt))
			return EINVAL;

		if (sa->bt_family != AF_BLUETOOTH)
			return EAFNOSUPPORT;

		bdaddr_copy(&pcb->hp_laddr, &sa->bt_bdaddr);

		if (bdaddr_any(&sa->bt_bdaddr))
			pcb->hp_flags |= HCI_PROMISCUOUS;
		else
			pcb->hp_flags &= ~HCI_PROMISCUOUS;

		return 0;

	case PRU_CONNECT:
		KASSERT(nam != NULL);
		sa = mtod(nam, struct sockaddr_bt *);

		if (sa->bt_len != sizeof(struct sockaddr_bt))
			return EINVAL;

		if (sa->bt_family != AF_BLUETOOTH)
			return EAFNOSUPPORT;

		if (hci_unit_lookup(&sa->bt_bdaddr) == NULL)
			return EADDRNOTAVAIL;

		bdaddr_copy(&pcb->hp_raddr, &sa->bt_bdaddr);
		soisconnected(up);
		return 0;

	case PRU_PEERADDR:
		KASSERT(nam != NULL);
		sa = mtod(nam, struct sockaddr_bt *);

		memset(sa, 0, sizeof(struct sockaddr_bt));
		nam->m_len =
		sa->bt_len = sizeof(struct sockaddr_bt);
		sa->bt_family = AF_BLUETOOTH;
		bdaddr_copy(&sa->bt_bdaddr, &pcb->hp_raddr);
		return 0;

	case PRU_SOCKADDR:
		KASSERT(nam != NULL);
		sa = mtod(nam, struct sockaddr_bt *);

		memset(sa, 0, sizeof(struct sockaddr_bt));
		nam->m_len =
		sa->bt_len = sizeof(struct sockaddr_bt);
		sa->bt_family = AF_BLUETOOTH;
		bdaddr_copy(&sa->bt_bdaddr, &pcb->hp_laddr);
		return 0;

	case PRU_SHUTDOWN:
		socantsendmore(up);
		break;

	case PRU_SEND:
		sa = NULL;
		if (nam) {
			sa = mtod(nam, struct sockaddr_bt *);

			if (sa->bt_len != sizeof(struct sockaddr_bt)) {
				err = EINVAL;
				goto release;
			}

			if (sa->bt_family != AF_BLUETOOTH) {
				err = EAFNOSUPPORT;
				goto release;
			}
		}

		if (ctl) /* have no use for this */
			m_freem(ctl);

		return hci_send(pcb, m, (sa ? &sa->bt_bdaddr : &pcb->hp_raddr));

	case PRU_SENSE:
		return 0;		/* (no sense - Doh!) */

	case PRU_RCVD:
	case PRU_RCVOOB:
		return EOPNOTSUPP;	/* (no release) */

	case PRU_ACCEPT:
	case PRU_CONNECT2:
	case PRU_LISTEN:
	case PRU_SENDOOB:
	case PRU_FASTTIMO:
	case PRU_SLOWTIMO:
	case PRU_PROTORCV:
	case PRU_PROTOSEND:
		err = EOPNOTSUPP;
		break;

	default:
		UNKNOWN(req);
		err = EOPNOTSUPP;
		break;
	}

release:
	if (m)
		m_freem(m);
	if (ctl)
		m_freem(ctl);
	return err;
}

/*
 * get/set socket options
 */
int
hci_ctloutput(int req, struct socket *so, int level,
		int optname, struct mbuf **opt)
{
	struct hci_pcb *pcb = (struct hci_pcb *)so->so_pcb;
	struct mbuf *m;
	int err = 0;

#ifdef notyet			/* XXX */
	DPRINTFN(2, "req %s\n", prcorequests[req]);
#endif

	if (pcb == NULL)
		return EINVAL;

	if (level != BTPROTO_HCI)
		return ENOPROTOOPT;

	switch(req) {
	case PRCO_GETOPT:
		m = m_get(M_WAIT, MT_SOOPTS);
		switch (optname) {
		case SO_HCI_EVT_FILTER:
			m->m_len = sizeof(struct hci_filter);
			memcpy(mtod(m, void *), &pcb->hp_efilter, m->m_len);
			break;

		case SO_HCI_PKT_FILTER:
			m->m_len = sizeof(struct hci_filter);
			memcpy(mtod(m, void *), &pcb->hp_pfilter, m->m_len);
			break;

		case SO_HCI_DIRECTION:
			m->m_len = sizeof(int);
			if (pcb->hp_flags & HCI_DIRECTION)
				*mtod(m, int *) = 1;
			else
				*mtod(m, int *) = 0;
			break;

		default:
			err = ENOPROTOOPT;
			m_freem(m);
			m = NULL;
			break;
		}
		*opt = m;
		break;

	case PRCO_SETOPT:
		m = *opt;
		if (m) switch (optname) {
		case SO_HCI_EVT_FILTER:	/* set event filter */
			m->m_len = min(m->m_len, sizeof(struct hci_filter));
			memcpy(&pcb->hp_efilter, mtod(m, void *), m->m_len);
			break;

		case SO_HCI_PKT_FILTER:	/* set packet filter */
			m->m_len = min(m->m_len, sizeof(struct hci_filter));
			memcpy(&pcb->hp_pfilter, mtod(m, void *), m->m_len);
			break;

		case SO_HCI_DIRECTION:	/* request direction ctl messages */
			if (*mtod(m, int *))
				pcb->hp_flags |= HCI_DIRECTION;
			else
				pcb->hp_flags &= ~HCI_DIRECTION;
			break;

		default:
			err = ENOPROTOOPT;
			break;
		}
		m_freem(m);
		break;

	default:
		err = ENOPROTOOPT;
		break;
	}

	return err;
}

/*
 * HCI mbuf tap routine
 *
 * copy packets to any raw HCI sockets that wish (and are
 * permitted) to see them
 */
void
hci_mtap(struct mbuf *m, struct hci_unit *unit)
{
	struct hci_pcb *pcb;
	struct mbuf *m0, *ctlmsg, **ctl;
	struct sockaddr_bt sa;
	uint8_t type;
	uint8_t event;
	uint16_t opcode;

	KASSERT(m->m_len >= sizeof(type));

	type = *mtod(m, uint8_t *);

	memset(&sa, 0, sizeof(sa));
	sa.bt_len = sizeof(struct sockaddr_bt);
	sa.bt_family = AF_BLUETOOTH;
	bdaddr_copy(&sa.bt_bdaddr, &unit->hci_bdaddr);

	LIST_FOREACH(pcb, &hci_pcb, hp_next) {
		/*
		 * filter according to source address
		 */
		if ((pcb->hp_flags & HCI_PROMISCUOUS) == 0
		    && bdaddr_same(&pcb->hp_laddr, &sa.bt_bdaddr) == 0)
			continue;

		/*
		 * filter according to packet type filter
		 */
		if (hci_filter_test(type, &pcb->hp_pfilter) == 0)
			continue;

		/*
		 * filter according to event/security filters
		 */
		switch(type) {
		case HCI_EVENT_PKT:
			KASSERT(m->m_len >= sizeof(hci_event_hdr_t));

			event = mtod(m, hci_event_hdr_t *)->event;

			if (hci_filter_test(event, &pcb->hp_efilter) == 0)
				continue;

			if ((pcb->hp_flags & HCI_PRIVILEGED) == 0
			    && hci_security_check_event(event) == -1)
				continue;
			break;

		case HCI_CMD_PKT:
			KASSERT(m->m_len >= sizeof(hci_cmd_hdr_t));

			opcode = letoh16(mtod(m, hci_cmd_hdr_t *)->opcode);

			if ((pcb->hp_flags & HCI_PRIVILEGED) == 0
			    && hci_security_check_opcode(opcode) == -1)
				continue;
			break;

		case HCI_ACL_DATA_PKT:
		case HCI_SCO_DATA_PKT:
		default:
			if ((pcb->hp_flags & HCI_PRIVILEGED) == 0)
				continue;

			break;
		}

		/*
		 * create control messages
		 */
		ctlmsg = NULL;
		ctl = &ctlmsg;
		if (pcb->hp_flags & HCI_DIRECTION) {
			int dir = m->m_flags & M_LINK0 ? 1 : 0;

			*ctl = sbcreatecontrol((void *)&dir, sizeof(dir),
			    SCM_HCI_DIRECTION, BTPROTO_HCI);

			if (*ctl != NULL)
				ctl = &((*ctl)->m_next);
		}

		/*
		 * copy to socket
		 */
		m0 = m_copym(m, 0, M_COPYALL, M_DONTWAIT);
		if (m0 && sbappendaddr(&pcb->hp_socket->so_rcv,
				(struct sockaddr *)&sa, m0, ctlmsg)) {
			sorwakeup(pcb->hp_socket);
		} else {
			m_freem(ctlmsg);
			m_freem(m0);
		}
	}
}
