/*	$OpenBSD: interface.c,v 1.39 2016/05/23 18:40:15 renato Exp $ */

/*
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
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <net/if_types.h>
#include <fcntl.h>
#include <ctype.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <event.h>

#include "ldpd.h"
#include "ldp.h"
#include "log.h"
#include "ldpe.h"

extern struct ldpd_conf        *leconf;

void		 if_hello_timer(int, short, void *);
void		 if_start_hello_timer(struct iface *);
void		 if_stop_hello_timer(struct iface *);

struct iface *
if_new(struct kif *kif)
{
	struct iface		*iface;

	if ((iface = calloc(1, sizeof(*iface))) == NULL)
		fatal("if_new: calloc");

	iface->state = IF_STA_DOWN;

	LIST_INIT(&iface->addr_list);
	LIST_INIT(&iface->adj_list);

	strlcpy(iface->name, kif->ifname, sizeof(iface->name));

	/* get type */
	if (kif->flags & IFF_POINTOPOINT)
		iface->type = IF_TYPE_POINTOPOINT;
	if (kif->flags & IFF_BROADCAST &&
	    kif->flags & IFF_MULTICAST)
		iface->type = IF_TYPE_BROADCAST;

	/* get index and flags */
	iface->ifindex = kif->ifindex;
	iface->flags = kif->flags;
	iface->linkstate = kif->link_state;
	iface->if_type = kif->if_type;

	return (iface);
}

void
if_del(struct iface *iface)
{
	struct if_addr		*if_addr;

	if (iface->state == IF_STA_ACTIVE)
		if_reset(iface);

	log_debug("%s: interface %s", __func__, iface->name);

	while ((if_addr = LIST_FIRST(&iface->addr_list)) != NULL) {
		LIST_REMOVE(if_addr, entry);
		free(if_addr);
	}

	free(iface);
}

struct iface *
if_lookup(struct ldpd_conf *xconf, unsigned short ifindex)
{
	struct iface *iface;

	LIST_FOREACH(iface, &xconf->iface_list, entry)
		if (iface->ifindex == ifindex)
			return (iface);

	return (NULL);
}

struct if_addr *
if_addr_new(struct kaddr *ka)
{
	struct if_addr	*if_addr;

	if ((if_addr = calloc(1, sizeof(*if_addr))) == NULL)
		fatal(__func__);

	if_addr->addr = ka->addr;
	if_addr->mask = ka->mask;
	if_addr->dstbrd = ka->dstbrd;

	return (if_addr);
}

struct if_addr *
if_addr_lookup(struct if_addr_head *addr_list, struct kaddr *ka)
{
	struct if_addr *if_addr;

	LIST_FOREACH(if_addr, addr_list, entry)
		if (if_addr->addr.s_addr == ka->addr.s_addr &&
		    if_addr->mask.s_addr == ka->mask.s_addr &&
		    if_addr->dstbrd.s_addr == ka->dstbrd.s_addr)
			return (if_addr);

	return (NULL);
}

void
if_addr_add(struct kaddr *ka)
{
	struct iface		*iface;
	struct if_addr		*if_addr;
	struct nbr		*nbr;

	if (if_addr_lookup(&global.addr_list, ka) == NULL) {
		if_addr = if_addr_new(ka);

		LIST_INSERT_HEAD(&global.addr_list, if_addr, entry);
		RB_FOREACH(nbr, nbr_id_head, &nbrs_by_id) {
			if (nbr->state != NBR_STA_OPER)
				continue;

			send_address(nbr, if_addr, 0);
		}
	}

	iface = if_lookup(leconf, ka->ifindex);
	if (iface &&
	    if_addr_lookup(&iface->addr_list, ka) == NULL) {
		if_addr = if_addr_new(ka);
		LIST_INSERT_HEAD(&iface->addr_list, if_addr, entry);
		if_update(iface);
	}
}

void
if_addr_del(struct kaddr *ka)
{
	struct iface		*iface;
	struct if_addr		*if_addr;
	struct nbr		*nbr;

	iface = if_lookup(leconf, ka->ifindex);
	if (iface) {
		if_addr = if_addr_lookup(&iface->addr_list, ka);
		if (if_addr) {
			LIST_REMOVE(if_addr, entry);
			free(if_addr);
			if_update(iface);
		}
	}

	if_addr = if_addr_lookup(&global.addr_list, ka);
	if (if_addr) {
		RB_FOREACH(nbr, nbr_id_head, &nbrs_by_id) {
			if (nbr->state != NBR_STA_OPER)
				continue;
			send_address(nbr, if_addr, 1);
		}
		LIST_REMOVE(if_addr, entry);
		free(if_addr);
	}
}

int
if_start(struct iface *iface)
{
	struct in_addr		 addr;
	struct timeval		 now;

	log_debug("%s: %s", __func__, iface->name);

	gettimeofday(&now, NULL);
	iface->uptime = now.tv_sec;

	inet_aton(AllRouters, &addr);
	if (if_join_group(iface, &addr))
		return (-1);

	send_hello(HELLO_LINK, iface, NULL);

	evtimer_set(&iface->hello_timer, if_hello_timer, iface);
	if_start_hello_timer(iface);
	return (0);
}

int
if_reset(struct iface *iface)
{
	struct in_addr		 addr;
	struct adj		*adj;

	log_debug("%s: %s", __func__, iface->name);

	while ((adj = LIST_FIRST(&iface->adj_list)) != NULL)
		adj_del(adj);

	if_stop_hello_timer(iface);

	/* try to cleanup */
	if (global.ldp_disc_socket != -1) {
		inet_aton(AllRouters, &addr);
		if_leave_group(iface, &addr);
	}

	return (0);
}

int
if_update(struct iface *iface)
{
	int			 link_ok, addr_ok = 0, socket_ok;
	int			 ret;

	link_ok = (iface->flags & IFF_UP) &&
	    LINK_STATE_IS_UP(iface->linkstate);

	addr_ok = !LIST_EMPTY(&iface->addr_list);

	if (global.ldp_disc_socket != -1)
		socket_ok = 1;
	else
		socket_ok = 0;

	if (iface->state == IF_STA_DOWN) {
		if (!link_ok || !addr_ok || !socket_ok)
			return (0);


		iface->state = IF_STA_ACTIVE;
		ret = if_start(iface);
	} else {
		if (link_ok && addr_ok && socket_ok)
			return (0);

		iface->state = IF_STA_DOWN;
		ret = if_reset(iface);
	}

	return (ret);
}

void
if_update_all(void)
{
	struct iface		*iface;

	LIST_FOREACH(iface, &leconf->iface_list, entry)
		if_update(iface);
}

/* timers */
/* ARGSUSED */
void
if_hello_timer(int fd, short event, void *arg)
{
	struct iface		*iface = arg;

	send_hello(HELLO_LINK, iface, NULL);
	if_start_hello_timer(iface);
}

void
if_start_hello_timer(struct iface *iface)
{
	struct timeval		 tv;

	timerclear(&tv);
	tv.tv_sec = iface->hello_interval;
	if (evtimer_add(&iface->hello_timer, &tv) == -1)
		fatal(__func__);
}

void
if_stop_hello_timer(struct iface *iface)
{
	if (evtimer_pending(&iface->hello_timer, NULL) &&
	    evtimer_del(&iface->hello_timer) == -1)
		fatal(__func__);
}

struct ctl_iface *
if_to_ctl(struct iface *iface)
{
	static struct ctl_iface	 ictl;
	struct timeval		 now;
	struct adj		*adj;

	memcpy(ictl.name, iface->name, sizeof(ictl.name));
	ictl.ifindex = iface->ifindex;
	ictl.state = iface->state;
	ictl.hello_holdtime = iface->hello_holdtime;
	ictl.hello_interval = iface->hello_interval;
	ictl.flags = iface->flags;
	ictl.type = iface->type;
	ictl.linkstate = iface->linkstate;
	ictl.if_type = iface->if_type;

	gettimeofday(&now, NULL);
	if (iface->state != IF_STA_DOWN &&
	    iface->uptime != 0) {
		ictl.uptime = now.tv_sec - iface->uptime;
	} else
		ictl.uptime = 0;

	ictl.adj_cnt = 0;
	LIST_FOREACH(adj, &iface->adj_list, iface_entry)
		ictl.adj_cnt++;

	return (&ictl);
}

/* misc */
int
if_join_group(struct iface *iface, struct in_addr *addr)
{
	struct ip_mreq		 mreq;
	struct if_addr		*if_addr;

	log_debug("%s: interface %s addr %s", __func__, iface->name,
	    inet_ntoa(*addr));

	if_addr = LIST_FIRST(&iface->addr_list);
	mreq.imr_multiaddr = *addr;
	mreq.imr_interface = if_addr->addr;

	if (setsockopt(global.ldp_disc_socket, IPPROTO_IP, IP_ADD_MEMBERSHIP,
	    (void *)&mreq, sizeof(mreq)) < 0) {
		log_warn("%s: error IP_ADD_MEMBERSHIP, interface %s address %s",
		     __func__, iface->name, inet_ntoa(*addr));
		return (-1);
	}
	return (0);
}

int
if_leave_group(struct iface *iface, struct in_addr *addr)
{
	struct ip_mreq		 mreq;
	struct if_addr		*if_addr;

	log_debug("%s: interface %s addr %s", __func__, iface->name,
	    inet_ntoa(*addr));

	if_addr = LIST_FIRST(&iface->addr_list);
	if (!if_addr)
		return (0);

	mreq.imr_multiaddr = *addr;
	mreq.imr_interface = if_addr->addr;

	if (setsockopt(global.ldp_disc_socket, IPPROTO_IP, IP_DROP_MEMBERSHIP,
	    (void *)&mreq, sizeof(mreq)) < 0) {
		log_warn("%s: error IP_DROP_MEMBERSHIP, interface %s "
		    "address %s", __func__, iface->name, inet_ntoa(*addr));
		return (-1);
	}

	return (0);
}
