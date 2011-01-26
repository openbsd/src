/*	$OpenBSD: timer.c,v 1.4 2011/01/26 17:07:59 reyk Exp $	*/

/*
 * Copyright (c) 2010 Reyk Floeter <reyk@vantronix.net>
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
#include <sys/socket.h>
#include <sys/uio.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <event.h>

#include "iked.h"

struct timer_cbarg {
	int		 tmr_active;
	struct event	 tmr_ev; 
	struct iked	*tmr_env;
	struct timeval	 tmr_first;
	struct timeval	 tmr_last;
	struct timeval	 tmr_tv;
	int		(*tmr_initcb)(struct iked *, struct iked_policy *);
} timer_initiator;

void	 timer_initiator_cb(int, short, void *);

#define IKED_TIMER_INITIATOR_INITIAL	2
#define IKED_TIMER_INITIATOR_INTERVAL	60

void
timer_register_initiator(struct iked *env,
    int (*cb)(struct iked *, struct iked_policy *))
{
	struct timer_cbarg	*tmr;

	timer_unregister_initiator(env);

	if (env->sc_passive)
		return;

	tmr = &timer_initiator;
	gettimeofday(&tmr->tmr_first, NULL);
	gettimeofday(&tmr->tmr_last, NULL);

	tmr->tmr_env = env;
	tmr->tmr_initcb = cb;
	tmr->tmr_active = 1;
	evtimer_set(&tmr->tmr_ev, timer_initiator_cb, tmr);

	tmr->tmr_tv.tv_sec = IKED_TIMER_INITIATOR_INITIAL;
	tmr->tmr_tv.tv_usec = 0;
	evtimer_add(&tmr->tmr_ev, &tmr->tmr_tv);
}

void
timer_unregister_initiator(struct iked *env)
{
	struct timer_cbarg	*tmr;

	tmr = &timer_initiator;
	if (!tmr->tmr_active)
		return;

	event_del(&tmr->tmr_ev);
	bzero(tmr, sizeof(*tmr));
}

void
timer_initiator_cb(int fd, short event, void *arg)
{
	struct timer_cbarg	*tmr = arg;
	struct iked		*env = tmr->tmr_env;
	struct iked_policy	*pol;

	gettimeofday(&tmr->tmr_last, NULL);

	TAILQ_FOREACH(pol, &env->sc_policies, pol_entry) {
		if ((pol->pol_flags & IKED_POLICY_ACTIVE) == 0)
			continue;
		if (sa_peer_lookup(pol, &pol->pol_peer.addr) != NULL) {
			log_debug("%s: \"%s\" is already active",
			    __func__, pol->pol_name);
			continue;
		}

		log_debug("%s: initiating \"%s\"", __func__, pol->pol_name);

		if (tmr->tmr_initcb != NULL) {
			/* Ignore error but what should we do on failure? */
			(void)tmr->tmr_initcb(env, pol);
		}
	}

	tmr->tmr_tv.tv_sec = IKED_TIMER_INITIATOR_INTERVAL;
	tmr->tmr_tv.tv_usec = 0;
	evtimer_add(&tmr->tmr_ev, &tmr->tmr_tv);
}
