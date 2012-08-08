/*	$OpenBSD: scheduler_backend.c,v 1.3 2012/08/08 08:50:42 eric Exp $	*/

/*
 * Copyright (c) 2012 Gilles Chehade <gilles@openbsd.org>
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
#include <sys/tree.h>
#include <sys/param.h>
#include <sys/socket.h>

#include <ctype.h>
#include <err.h>
#include <event.h>
#include <fcntl.h>
#include <imsg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "smtpd.h"
#include "log.h"

extern struct scheduler_backend scheduler_backend_ramqueue;

struct scheduler_backend *
scheduler_backend_lookup(const char *name)
{
	if (!strcmp(name, "ramqueue"))
		return &scheduler_backend_ramqueue;

	return NULL;
}

void
scheduler_info(struct scheduler_info *sched, struct envelope *evp)
{
	strlcpy(sched->destination, evp->dest.domain, sizeof sched->destination);

	sched->evpid    = evp->id;
	sched->type     = evp->type;
	sched->creation = evp->creation;
	sched->lasttry  = evp->lasttry;
	sched->expire   = evp->expire;
	sched->retry    = evp->retry;
}

time_t
scheduler_compute_schedule(struct scheduler_info *sched)
{
	time_t	delay;

	if (sched->retry == 0)
		delay = 0;
#if 0
		/* for testing scheduler sleep */
		delay == arc4random() % 30;
#endif
	else if (sched->retry < 4)
		delay = (sched->retry * 15 * 60);
	else if (sched->retry < 8)
		delay = ((sched->retry - 3) * 60 * 60);
	else
		delay = ((sched->retry - 7) * 24 * 60 * 60);

	return (sched->creation + delay);
}
