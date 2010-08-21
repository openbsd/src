/* $OpenBSD: if_aoe.c,v 1.2 2010/08/21 06:50:42 blambert Exp $ */
/*
 * Copyright (c) 2008 Ted Unangst <tedu@openbsd.org>
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
#include <sys/queue.h>
#include <sys/workq.h>
#include <sys/socket.h>
#include <sys/mbuf.h>
#include <net/if.h>
#include <net/if_aoe.h>

#ifdef AOE_DEBUG
#define	DPRINTF(x)	printf x
#else
#define	DPRINTF(x)	/* nothing */
#endif

struct aoe_handler_head aoe_handlers = TAILQ_HEAD_INITIALIZER(aoe_handlers);

void
aoe_input(struct ifnet *ifp, struct mbuf *m) 
{
	struct aoe_packet *ap;
	struct aoe_handler *q = NULL;

	splassert(IPL_NET);

	ap = mtod(m, struct aoe_packet *);
	DPRINTF(("aoe packet %d %d\n", htons(ap->major), ap->minor));

	TAILQ_FOREACH(q, &aoe_handlers, next) {
		if (q->ifp == ifp) {
			if (ap->major == q->major && ap->minor == q->minor)
				break;
		}
	}
	if (!q) {
		DPRINTF(("no q\n"));
		m_freem(m);
		return;
	}

	workq_queue_task(NULL, &q->task, 0, q->fn, q, m);
}
