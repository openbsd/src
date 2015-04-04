/*	$OpenBSD: adjacency.c,v 1.4 2015/04/04 15:09:47 renato Exp $ */

/*
 * Copyright (c) 2009 Michele Marchetto <michele@openbsd.org>
 * Copyright (c) 2005 Claudio Jeker <claudio@openbsd.org>
 * Copyright (c) 2004, 2005, 2008 Esben Norby <norby@openbsd.org>
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
#include <sys/socket.h>
#include <arpa/inet.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ldpd.h"
#include "ldpe.h"
#include "control.h"
#include "log.h"

extern struct ldpd_conf        *leconf;

void	 adj_itimer(int, short, void *);
char	*print_hello_src(struct hello_source *);

void	 tnbr_hello_timer(int, short, void *);
void	 tnbr_start_hello_timer(struct tnbr *);
void	 tnbr_stop_hello_timer(struct tnbr *);

struct adj *
adj_new(struct nbr *nbr, struct hello_source *source, struct in_addr addr)
{
	struct adj	*adj;

	log_debug("adj_new: LSR ID %s, %s", inet_ntoa(nbr->id),
	    print_hello_src(source));

	if ((adj = calloc(1, sizeof(*adj))) == NULL)
		fatal("adj_new");

	adj->nbr = nbr;
	memcpy(&adj->source, source, sizeof(*source));
	adj->addr = addr;

	evtimer_set(&adj->inactivity_timer, adj_itimer, adj);

	LIST_INSERT_HEAD(&nbr->adj_list, adj, nbr_entry);

	switch (source->type) {
	case HELLO_LINK:
		LIST_INSERT_HEAD(&source->link.iface->adj_list, adj,
		    iface_entry);
		break;
	case HELLO_TARGETED:
		source->target->adj = adj;
		break;
	}

	return (adj);
}

void
adj_del(struct adj *adj)
{
	log_debug("adj_del: LSR ID %s, %s", inet_ntoa(adj->nbr->id),
	    print_hello_src(&adj->source));

	adj_stop_itimer(adj);

	LIST_REMOVE(adj, nbr_entry);

	/* last adjacency deleted */
	if (LIST_EMPTY(&adj->nbr->adj_list))
		nbr_del(adj->nbr);

	free(adj);
}

struct adj *
adj_find(struct nbr *nbr, struct hello_source *source)
{
	struct adj *adj;

	LIST_FOREACH(adj, &nbr->adj_list, nbr_entry) {
		if (adj->source.type != source->type)
			continue;

		switch (source->type) {
		case HELLO_LINK:
			if (adj->source.link.src_addr.s_addr ==
			    source->link.src_addr.s_addr)
				return (adj);
			break;
		case HELLO_TARGETED:
			if (adj->source.target == source->target)
				return (adj);
			break;
		}
	}

	return (NULL);
}

char *
print_hello_src(struct hello_source *src)
{
	static char buffer[64];

	switch (src->type) {
	case HELLO_LINK:
		snprintf(buffer, sizeof(buffer), "iface %s",
		    src->link.iface->name);
		break;
	case HELLO_TARGETED:
		snprintf(buffer, sizeof(buffer), "source %s",
		    inet_ntoa(src->target->addr));
		break;
	}

	return (buffer);
}

/* adjacency timers */

/* ARGSUSED */
void
adj_itimer(int fd, short event, void *arg)
{
	struct adj *adj = arg;

	log_debug("adj_itimer: LDP ID %s", inet_ntoa(adj->nbr->id));

	switch (adj->source.type) {
	case HELLO_LINK:
		LIST_REMOVE(adj, iface_entry);
		break;
	case HELLO_TARGETED:
		adj->source.target->adj = NULL;
		if (!(adj->source.target->flags & F_TNBR_CONFIGURED)) {
			LIST_REMOVE(adj->source.target, entry);
			tnbr_del(adj->source.target);
		}
		break;
	}

	adj_del(adj);
}

void
adj_start_itimer(struct adj *adj)
{
	struct timeval	tv;

	timerclear(&tv);
	tv.tv_sec = adj->holdtime;

	if (evtimer_add(&adj->inactivity_timer, &tv) == -1)
		fatal("adj_start_itimer");
}

void
adj_stop_itimer(struct adj *adj)
{
	if (evtimer_pending(&adj->inactivity_timer, NULL) &&
	    evtimer_del(&adj->inactivity_timer) == -1)
		fatal("adj_stop_itimer");
}

/* targeted neighbors */

struct tnbr *
tnbr_new(struct ldpd_conf *xconf, struct in_addr addr, int configured)
{
	struct tnbr		*tnbr;

	if ((tnbr = calloc(1, sizeof(*tnbr))) == NULL)
		fatal("tnbr_new");

	tnbr->addr.s_addr = addr.s_addr;
	if (configured)
		tnbr->flags |= F_TNBR_CONFIGURED;
	else {
		tnbr->hello_holdtime = xconf->thello_holdtime;
		tnbr->hello_interval = xconf->thello_interval;
	}

	return (tnbr);
}

void
tnbr_del(struct tnbr *tnbr)
{
	tnbr_stop_hello_timer(tnbr);
	if (tnbr->adj)
		adj_del(tnbr->adj);
	free(tnbr);
}

void
tnbr_init(struct ldpd_conf *xconf, struct tnbr *tnbr)
{
	/* set event handlers for targeted neighbor */
	evtimer_set(&tnbr->hello_timer, tnbr_hello_timer, tnbr);

	tnbr->discovery_fd = xconf->ldp_ediscovery_socket;
	tnbr_start_hello_timer(tnbr);
}

struct tnbr *
tnbr_find(struct in_addr addr)
{
	struct tnbr *tnbr;

	LIST_FOREACH(tnbr, &leconf->tnbr_list, entry)
		if (addr.s_addr == tnbr->addr.s_addr)
			return (tnbr);

	return (NULL);
}

/* target neighbors timers */

/* ARGSUSED */
void
tnbr_hello_timer(int fd, short event, void *arg)
{
	struct tnbr *tnbr = arg;
	struct timeval tv;

	send_hello(HELLO_TARGETED, NULL, tnbr);

	/* reschedule hello_timer */
	timerclear(&tv);
	tv.tv_sec = tnbr->hello_interval;
	if (evtimer_add(&tnbr->hello_timer, &tv) == -1)
		fatal("tnbr_hello_timer");
}

void
tnbr_start_hello_timer(struct tnbr *tnbr)
{
	struct timeval tv;

	send_hello(HELLO_TARGETED, NULL, tnbr);

	timerclear(&tv);
	tv.tv_sec = tnbr->hello_interval;
	if (evtimer_add(&tnbr->hello_timer, &tv) == -1)
		fatal("tnbr_start_hello_timer");
}

void
tnbr_stop_hello_timer(struct tnbr *tnbr)
{
	if (evtimer_pending(&tnbr->hello_timer, NULL) &&
	    evtimer_del(&tnbr->hello_timer) == -1)
		fatal("tnbr_stop_hello_timer");
}

struct ctl_adj *
adj_to_ctl(struct adj *adj)
{
	static struct ctl_adj	 actl;

	actl.id.s_addr = adj->nbr->id.s_addr;
	actl.type = adj->source.type;
	switch (adj->source.type) {
	case HELLO_LINK:
		memcpy(actl.ifname, adj->source.link.iface->name,
		    sizeof(actl.ifname));
		break;
	case HELLO_TARGETED:
		actl.src_addr.s_addr = adj->source.target->addr.s_addr;
		break;
	}
	actl.holdtime = adj->holdtime;

	return (&actl);
}

void
ldpe_adj_ctl(struct ctl_conn *c)
{
	struct adj	*adj;
	struct iface	*iface;
	struct tnbr	*tnbr;
	struct ctl_adj	*actl;

	/* basic discovery mechanism */
	LIST_FOREACH(iface, &leconf->iface_list, entry)
		LIST_FOREACH(adj, &iface->adj_list, iface_entry) {
			actl = adj_to_ctl(adj);
			imsg_compose_event(&c->iev, IMSG_CTL_SHOW_DISCOVERY,
			    0, 0, -1, actl, sizeof(struct ctl_adj));
		}

	/* extended discovery mechanism */
	LIST_FOREACH(tnbr, &leconf->tnbr_list, entry)
		if (tnbr->adj) {
			actl = adj_to_ctl(tnbr->adj);
			imsg_compose_event(&c->iev, IMSG_CTL_SHOW_DISCOVERY,
			    0, 0, -1, actl, sizeof(struct ctl_adj));
		}

	imsg_compose_event(&c->iev, IMSG_CTL_END, 0, 0, -1, NULL, 0);
}
