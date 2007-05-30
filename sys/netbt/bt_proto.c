/*	$OpenBSD: bt_proto.c,v 1.2 2007/05/30 08:10:03 uwe Exp $	*/
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
#include <netbt/hci.h>
#include <netbt/bt_var.h>

struct domain btdomain;

struct protosw btsw[] = {
	{ SOCK_RAW, &btdomain, BTPROTO_HCI,
	  PR_ATOMIC | PR_ADDR,
	  NULL/*input*/, NULL/*output*/, NULL/*ctlinput*/,
	  NULL/*hci_ctloutput*/, NULL/*hci_usrreq*/, NULL/*init*/,
	  NULL/*fasttimo*/, NULL/*slowtimo*/, NULL/*drain*/,
	  NULL/*sysctl*/
	},
#if 0
	{ SOCK_RAW, &btdomain, BLUETOOTH_PROTO_L2CAP,
	  PR_ATOMIC | PR_ADDR,
	  NULL/*input*/, NULL/*output*/, NULL/*ctlinput*/,
	  NULL/*ctloutput*/, l2cap_raw_usrreq, l2cap_raw_init,
	  NULL/*fasttimo*/, NULL/*slowtimo*/, NULL/*drain*/,
	  NULL/*sysctl*/
	},
	{ SOCK_SEQPACKET, &btdomain, BLUETOOTH_PROTO_L2CAP,
	  PR_ATOMIC | PR_CONNREQUIRED,
	  NULL/*input*/, NULL/*output*/, NULL/*ctlinput*/,
	  l2cap_ctloutput, l2cap_usrreq, l2cap_init,
	  NULL/*fasttimo*/, NULL/*slowtimo*/, NULL/*drain*/,
	  NULL/*sysctl*/
	},
	{ SOCK_STREAM, &btdomain, BLUETOOTH_PROTO_RFCOMM,
	  PR_ATOMIC | PR_CONNREQUIRED,
	  NULL/*input*/, NULL/*output*/, NULL/*ctlinput*/,
	  rfcomm_ctloutput, rfcomm_usrreq, rfcomm_init,
	  NULL/*fasttimo*/, NULL/*slowtimo*/, NULL/*drain*/,
	  NULL/*sysctl*/
	}
#endif
};

struct domain btdomain = {
	AF_BLUETOOTH, "bluetooth",
	bt_init, NULL/*externalize*/, NULL/*dispose*/,
	btsw, &btsw[sizeof(btsw) / sizeof(btsw[0])], NULL,
	NULL/*rtattach*/, 32, sizeof(struct sockaddr_bt),
	NULL/*ifattach*/, NULL/*ifdetach*/
};
