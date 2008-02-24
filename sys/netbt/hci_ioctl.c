/*	$OpenBSD: hci_ioctl.c,v 1.2 2008/02/24 21:34:48 uwe Exp $	*/
/*	$NetBSD: hci_ioctl.c,v 1.7 2007/11/28 20:16:12 plunky Exp $	*/

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
#include <sys/domain.h>
#include <sys/ioctl.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/proc.h>
#include <sys/systm.h>

#include <netbt/bluetooth.h>
#include <netbt/hci.h>
#include <netbt/l2cap.h>
#include <netbt/rfcomm.h>

#ifdef BLUETOOTH_DEBUG
#define BDADDR(bd)	(bd).b[5], (bd).b[4], (bd).b[3],	\
			(bd).b[2], (bd).b[1], (bd).b[0]

static void
hci_dump(void)
{
	struct hci_unit *unit;
	struct hci_link *link;
	struct l2cap_channel *chan;
	struct rfcomm_session *rs;
	struct rfcomm_dlc *dlc;

	printf("HCI:\n");
	TAILQ_FOREACH(unit, &hci_unit_list, hci_next) {
		printf("UNIT %s: flags 0x%4.4x, "
			"num_cmd=%d, num_acl=%d, num_sco=%d\n",
			device_xname(unit->hci_dev), unit->hci_flags,
			unit->hci_num_cmd_pkts,
			unit->hci_num_acl_pkts,
			unit->hci_num_sco_pkts);
		TAILQ_FOREACH(link, &unit->hci_links, hl_next) {
			printf("+HANDLE #%d: %s "
			    "raddr=%2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x, "
			    "state %d, refcnt %d\n",
			    link->hl_handle,
			    (link->hl_type == HCI_LINK_ACL ? "ACL":"SCO"),
			    BDADDR(link->hl_bdaddr),
			    link->hl_state, link->hl_refcnt);
		}
	}

	printf("L2CAP:\n");
	LIST_FOREACH(chan, &l2cap_active_list, lc_ncid) {
		printf("CID #%d state %d, psm=0x%4.4x, "
		    "laddr=%2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x, "
		    "raddr=%2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x\n",
		    chan->lc_lcid, chan->lc_state, chan->lc_raddr.bt_psm,
		    BDADDR(chan->lc_laddr.bt_bdaddr),
		    BDADDR(chan->lc_raddr.bt_bdaddr));
	}

	LIST_FOREACH(chan, &l2cap_listen_list, lc_ncid) {
		printf("LISTEN psm=0x%4.4x, "
		    "laddr=%2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x\n",
		    chan->lc_laddr.bt_psm,
		    BDADDR(chan->lc_laddr.bt_bdaddr));
	}

	printf("RFCOMM:\n");
	LIST_FOREACH(rs, &rfcomm_session_active, rs_next) {
		chan = rs->rs_l2cap;
		printf("SESSION: state=%d, flags=0x%4.4x, psm 0x%4.4x "
		    "laddr=%2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x, "
		    "raddr=%2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x\n",
		    rs->rs_state, rs->rs_flags, chan->lc_raddr.bt_psm,
		    BDADDR(chan->lc_laddr.bt_bdaddr),
		    BDADDR(chan->lc_raddr.bt_bdaddr));
		LIST_FOREACH(dlc, &rs->rs_dlcs, rd_next) {
			printf("+DLC channel=%d, dlci=%d, "
			    "state=%d, flags=0x%4.4x, rxcred=%d, rxsize=%ld, "
			    "txcred=%d, pending=%d, txqlen=%d\n",
			    dlc->rd_raddr.bt_channel, dlc->rd_dlci,
			    dlc->rd_state, dlc->rd_flags,
			    dlc->rd_rxcred, (unsigned long)dlc->rd_rxsize,
			    dlc->rd_txcred, dlc->rd_pending,
			    (dlc->rd_txbuf ? dlc->rd_txbuf->m_pkthdr.len : 0));
		}
	}

	LIST_FOREACH(rs, &rfcomm_session_listen, rs_next) {
		chan = rs->rs_l2cap;
		printf("LISTEN: psm 0x%4.4x, "
		    "laddr=%2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x\n",
		    chan->lc_laddr.bt_psm,
		    BDADDR(chan->lc_laddr.bt_bdaddr));
		LIST_FOREACH(dlc, &rs->rs_dlcs, rd_next)
			printf("+DLC channel=%d\n", dlc->rd_laddr.bt_channel);
	}
}

#undef BDADDR
#endif

int
hci_ioctl(unsigned long cmd, void *data, struct proc *p)
{
	struct btreq *btr = data;
	struct hci_unit *unit;
	int err = 0;

	DPRINTFN(1, "cmd %#lx\n", cmd);

	switch(cmd) {
#ifdef BLUETOOTH_DEBUG
	case SIOCBTDUMP:
		hci_dump();
		return 0;
#endif
	/*
	 * Get unit info based on address rather than name
	 */
	case SIOCGBTINFOA:
		unit = hci_unit_lookup(&btr->btr_bdaddr);
		if (unit == NULL)
			return ENXIO;

		break;

	/*
	 * The remaining ioctl's all use the same btreq structure and
	 * index on the name of the device, so we look that up first.
	 */
	case SIOCNBTINFO:
		/* empty name means give the first unit */
		if (btr->btr_name[0] == '\0') {
			unit = NULL;
			break;
		}

		/* else fall through and look it up */
	case SIOCGBTINFO:
	case SIOCSBTFLAGS:
	case SIOCSBTPOLICY:
	case SIOCSBTPTYPE:
	case SIOCGBTSTATS:
	case SIOCZBTSTATS:
	case SIOCSBTSCOMTU:
		TAILQ_FOREACH(unit, &hci_unit_list, hci_next) {
			if (strncmp(device_xname(unit->hci_dev), btr->btr_name,
			    HCI_DEVNAME_SIZE) == 0)
				break;
		}

		if (unit == NULL)
			return ENXIO;

		break;

	default:	/* not one of mine */
		return EPASSTHROUGH;
	}

	switch(cmd) {
	case SIOCNBTINFO:	/* get next info */
		if (unit)
			unit = TAILQ_NEXT(unit, hci_next);
		else
			unit = TAILQ_FIRST(&hci_unit_list);

		if (unit == NULL) {
			err = ENXIO;
			break;
		}

		/* and fall through to */
	case SIOCGBTINFO:	/* get unit info */
	case SIOCGBTINFOA:	/* get info by address */
		memset(btr, 0, sizeof(struct btreq));
		strlcpy(btr->btr_name, device_xname(unit->hci_dev), HCI_DEVNAME_SIZE);
		bdaddr_copy(&btr->btr_bdaddr, &unit->hci_bdaddr);

		btr->btr_flags = unit->hci_flags;

		btr->btr_num_cmd = unit->hci_num_cmd_pkts;
		btr->btr_num_acl = unit->hci_num_acl_pkts;
		btr->btr_num_sco = unit->hci_num_sco_pkts;
		btr->btr_acl_mtu = unit->hci_max_acl_size;
		btr->btr_sco_mtu = unit->hci_max_sco_size;

		btr->btr_packet_type = unit->hci_packet_type;
		btr->btr_link_policy = unit->hci_link_policy;
		break;

	case SIOCSBTFLAGS:	/* set unit flags (privileged) */
		err = suser(p, 0);
		if (err)
			break;

		if ((unit->hci_flags & BTF_UP)
		    && (btr->btr_flags & BTF_UP) == 0) {
			hci_disable(unit);
			unit->hci_flags &= ~BTF_UP;
		}

		unit->hci_flags |= (btr->btr_flags & BTF_INIT);

		if ((unit->hci_flags & BTF_UP) == 0
		    && (btr->btr_flags & BTF_UP)) {
			err = hci_enable(unit);
			if (err)
				break;

			unit->hci_flags |= BTF_UP;
		}

		btr->btr_flags = unit->hci_flags;
		break;

	case SIOCSBTPOLICY:	/* set unit link policy (privileged) */
		err = suser(p, 0);
		if (err)
			break;

		unit->hci_link_policy = btr->btr_link_policy;
		unit->hci_link_policy &= unit->hci_lmp_mask;
		btr->btr_link_policy = unit->hci_link_policy;
		break;

	case SIOCSBTPTYPE:	/* set unit packet types (privileged) */
		err = suser(p, 0);
		if (err)
			break;

		unit->hci_packet_type = btr->btr_packet_type;
		unit->hci_packet_type &= unit->hci_acl_mask;
		btr->btr_packet_type = unit->hci_packet_type;
		break;

	case SIOCGBTSTATS:	/* get unit statistics */
		(*unit->hci_if->get_stats)(unit->hci_dev, &btr->btr_stats, 0);
		break;

	case SIOCZBTSTATS:	/* get & reset unit statistics */
		err = suser(p, 0);
		if (err)
			break;

		(*unit->hci_if->get_stats)(unit->hci_dev, &btr->btr_stats, 1);
		break;

	case SIOCSBTSCOMTU:	/* set sco_mtu value for unit */
		/*
		 * This is a temporary ioctl and may not be supported
		 * in the future. The need is that if SCO packets are
		 * sent to USB bluetooth controllers that are not an
		 * integer number of frame sizes, the USB bus locks up.
		 */
		err = suser(p, 0);
		if (err)
			break;

		unit->hci_max_sco_size = btr->btr_sco_mtu;
		break;

	default:
		err = EFAULT;
		break;
	}

	return err;
}
