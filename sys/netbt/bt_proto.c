/*	$OpenBSD: bt_proto.c,v 1.4 2007/06/24 20:55:27 uwe Exp $	*/
/*
 * Copyright (c) 2004 Alexander Yurchenko <grange@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/timeout.h>

#include <netbt/bluetooth.h>
#include <netbt/bt_var.h>
#include <netbt/hci.h>
#include <netbt/l2cap.h>
#include <netbt/rfcomm.h>
#include <netbt/sco.h>

struct domain btdomain;

struct protosw btsw[] = {
	{ SOCK_RAW, &btdomain, BTPROTO_HCI,
	  PR_ATOMIC | PR_ADDR,
	  NULL/*input*/, NULL/*output*/, NULL/*ctlinput*/,
	  hci_ctloutput, hci_usrreq, NULL/*init*/,
	  NULL/*fasttimo*/, NULL/*slowtimo*/, NULL/*drain*/,
	  NULL/*sysctl*/
	},
	{ SOCK_SEQPACKET, &btdomain, BTPROTO_SCO,
	  PR_ATOMIC | PR_CONNREQUIRED,
	  NULL/*input*/, NULL/*output*/, NULL/*ctlinput*/,
	  sco_ctloutput, sco_usrreq, NULL/*init*/,
	  NULL/*fasttimo*/, NULL/*slowtimo*/, NULL/*drain*/,
	  NULL/*sysctl*/
	},
	{ SOCK_SEQPACKET, &btdomain, BTPROTO_L2CAP,
	  PR_ATOMIC | PR_CONNREQUIRED,
	  NULL/*input*/, NULL/*output*/, NULL/*ctlinput*/,
	  l2cap_ctloutput, l2cap_usrreq, l2cap_init,
	  NULL/*fasttimo*/, NULL/*slowtimo*/, NULL/*drain*/,
	  NULL/*sysctl*/
	},
	{ SOCK_STREAM, &btdomain, BTPROTO_RFCOMM,
	  PR_CONNREQUIRED | PR_WANTRCVD,
	  NULL/*input*/, NULL/*output*/, NULL/*ctlinput*/,
	  rfcomm_ctloutput, rfcomm_usrreq, rfcomm_init,
	  NULL/*fasttimo*/, NULL/*slowtimo*/, NULL/*drain*/,
	  NULL/*sysctl*/
	}
};

struct domain btdomain = {
	AF_BLUETOOTH, "bluetooth",
	NULL/*init*/, NULL/*externalize*/, NULL/*dispose*/,
	btsw, &btsw[sizeof(btsw) / sizeof(btsw[0])], NULL,
	NULL/*rtattach*/, 32, sizeof(struct sockaddr_bt),
	NULL/*ifattach*/, NULL/*ifdetach*/
};
