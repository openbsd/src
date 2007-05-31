/*	$OpenBSD: bt_input.c,v 1.4 2007/05/31 23:50:19 uwe Exp $	*/
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
#include <sys/mbuf.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <sys/timeout.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/netisr.h>

#include <netbt/bluetooth.h>
#include <netbt/bt_var.h>
#include <netbt/hci.h>

extern void hci_intr(void *);	/* XXX */

struct ifqueue btintrq;

void
bt_init(void)
{
	btintrq.ifq_maxlen = IFQ_MAXLEN;
}

void
btintr(void)
{
	struct hci_unit *unit;
	struct mbuf *m;
	int s;

	TAILQ_FOREACH(unit, &hci_unit_list, hci_next) {
		hci_intr(unit);
	}

	for (;;) {
		s = splnet();
		IF_DEQUEUE(&btintrq, m);
		splx(s);

		if (m == NULL)
			break;

#if 0
		ng_btsocket_hci_raw_data_input(m);
#endif
	}
}
