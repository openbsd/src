/*	$OpenBSD: rde_peer.c,v 1.1 2020/01/01 07:25:04 claudio Exp $ */

/*
 * Copyright (c) 2019 Claudio Jeker <claudio@openbsd.org>
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
#include <sys/types.h>
#include <sys/queue.h>

#include <netinet/in.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "bgpd.h"
#include "rde.h"

struct iq {
	SIMPLEQ_ENTRY(iq)	entry;
	struct imsg		imsg;
};

/*
 * move an imsg from src to dst, disconnecting any dynamic memory from src.
 */
static void
imsg_move(struct imsg *dst, struct imsg *src)
{
	*dst = *src;
	src->data = NULL;	/* allocation was moved */
}

/*
 * push an imsg onto the peer imsg queue.
 */
void
peer_imsg_push(struct rde_peer *peer, struct imsg *imsg)
{
	struct iq *iq;

	if ((iq = calloc(1, sizeof(*iq))) == NULL)
		fatal(NULL);
	imsg_move(&iq->imsg, imsg);
	SIMPLEQ_INSERT_TAIL(&peer->imsg_queue, iq, entry);
}

/*
 * pop first imsg from peer imsg queue and move it into imsg argument.
 * Returns 1 if an element is returned else 0.
 */
int
peer_imsg_pop(struct rde_peer *peer, struct imsg *imsg)
{
	struct iq *iq;

	iq = SIMPLEQ_FIRST(&peer->imsg_queue);
	if (iq == NULL)
		return 0;

	imsg_move(imsg, &iq->imsg);

	SIMPLEQ_REMOVE_HEAD(&peer->imsg_queue, entry);
	free(iq);

	return 1;
}

void
peer_imsg_queued(struct rde_peer *peer, void *arg)
{
	int *p = arg;

	*p = *p || !SIMPLEQ_EMPTY(&peer->imsg_queue);
}

/*
 * flush all imsg queued for a peer.
 */
void
peer_imsg_flush(struct rde_peer *peer)
{
	struct iq *iq;

	while ((iq = SIMPLEQ_FIRST(&peer->imsg_queue)) != NULL) {
		SIMPLEQ_REMOVE_HEAD(&peer->imsg_queue, entry);
		free(iq);
	}
}
