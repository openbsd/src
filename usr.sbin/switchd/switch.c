/*	$OpenBSD: switch.c,v 1.3 2016/09/18 13:17:40 rzalamena Exp $	*/

/*
 * Copyright (c) 2013-2016 Reyk Floeter <reyk@openbsd.org>
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
#include <sys/socket.h>
#include <sys/uio.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <imsg.h>
#include <event.h>

#include "switchd.h"

void	 switch_timer(struct switchd *, void *);

static __inline int
	 switch_cmp(struct switch_control *, struct switch_control *);
static __inline int
	 switch_maccmp(struct macaddr *, struct macaddr *);

void
switch_init(struct switchd *sc)
{
	RB_INIT(&sc->sc_switches);
}

int
switch_dispatch_control(int fd, struct privsep_proc *p, struct imsg *imsg)
{
	struct privsep		*ps = p->p_ps;
	struct switchd		*sc = ps->ps_env;
	struct switch_control	*sw;
	struct macaddr		*mac;
	struct iovec		 iov[2];

	switch (imsg->hdr.type) {
	case IMSG_CTL_SHOW_SUM:
		IMSG_SIZE_CHECK(imsg, &fd);

		RB_FOREACH(sw, switch_head, &sc->sc_switches) {
			iov[0].iov_base = imsg->data;
			iov[0].iov_len = IMSG_DATA_SIZE(imsg);
			iov[1].iov_base = sw;
			iov[1].iov_len = sizeof(*sw);

			proc_composev(ps, PROC_CONTROL,
			    IMSG_CTL_SWITCH, iov, 2);

			RB_FOREACH(mac, macaddr_head, &sw->sw_addrcache) {
				iov[0].iov_base = imsg->data;
				iov[0].iov_len = IMSG_DATA_SIZE(imsg);
				iov[1].iov_base = mac;
				iov[1].iov_len = sizeof(*mac);

				proc_composev(ps, PROC_CONTROL,
				    IMSG_CTL_MAC, iov, 2);
			}
		}

		proc_compose(ps, PROC_CONTROL,
		    IMSG_CTL_END, imsg->data, IMSG_DATA_SIZE(imsg));
		return (0);
	default:
		break;
	}

	return (-1);
}

struct switch_control *
switch_get(struct switch_connection *con)
{
	struct switchd		*sc = con->con_sc;
	struct switch_control	 key;

	memcpy(&key.sw_addr, &con->con_peer, sizeof(key.sw_addr));

	con->con_switch = RB_FIND(switch_head, &sc->sc_switches, &key);
	return (con->con_switch);
}

struct switch_control *
switch_add(struct switch_connection *con)
{
	struct switchd		*sc = con->con_sc;
	struct switch_control	*sw, *oldsw;
	static unsigned int	 id = 0;

	/* Connection already has an associated switch */
	if (con->con_switch != NULL)
		return (NULL);

	if ((sw = calloc(1, sizeof(*sw))) == NULL)
		return (NULL);

	memcpy(&sw->sw_addr, &con->con_peer, sizeof(sw->sw_addr));
	sw->sw_id = ++id;
	RB_INIT(&sw->sw_addrcache);

	if ((oldsw =
	    RB_INSERT(switch_head, &sc->sc_switches, sw)) != NULL) {
		free(sw);
		sw = oldsw;
	} else {
		timer_set(sc, &sw->sw_timer, switch_timer, sw);
		timer_add(sc, &sw->sw_timer, sc->sc_cache_timeout);
	}

	con->con_switch = sw;
	return (con->con_switch);
}

void
switch_timer(struct switchd *sc, void *arg)
{
	struct switch_control	*sw = arg;
	struct macaddr		*mac, *next;
	struct timeval		 tv;
	unsigned int		 cnt = 0;

	getmonotime(&tv);

	for (mac = RB_MIN(macaddr_head, &sw->sw_addrcache);
	    mac != NULL; mac = next) {
		next = RB_NEXT(macaddr_head, &sw->sw_addrcache, mac);

		/* Simple monotonic timeout */
		if ((tv.tv_sec - mac->mac_age) >= sc->sc_cache_timeout) {
			RB_REMOVE(macaddr_head, &sw->sw_addrcache, mac);
			sw->sw_cachesize--;
			free(mac);
			cnt++;
		}
	}
	if (cnt)
		log_debug("%s: flushed %d mac from switch %u after timeout",
		    __func__, cnt, sw->sw_id);

	timer_add(sc, &sw->sw_timer, sc->sc_cache_timeout);
}

void
switch_remove(struct switchd *sc, struct switch_control *sw)
{
	struct macaddr	*mac, *next;

	if (sw == NULL)
		return;

	timer_del(sc, &sw->sw_timer);

	for (mac = RB_MIN(macaddr_head, &sw->sw_addrcache);
	    mac != NULL; mac = next) {
		next = RB_NEXT(macaddr_head, &sw->sw_addrcache, mac);
		RB_REMOVE(macaddr_head, &sw->sw_addrcache, mac);
		sw->sw_cachesize--;
		free(mac);
	}
	RB_REMOVE(switch_head, &sc->sc_switches, sw);

	log_debug("%s: switch %u removed", __func__, sw->sw_id);

	free(sw);
}

struct macaddr *
switch_learn(struct switchd *sc, struct switch_control *sw,
    uint8_t *ea, uint32_t port)
{
	struct macaddr	*mac, *oldmac = NULL;
	struct timeval	 tv;

	if ((mac = oldmac = switch_cached(sw, ea)) != NULL)
		goto update;

	if (sw->sw_cachesize >= sc->sc_cache_max)
		return (NULL);

	if ((mac = calloc(1, sizeof(*mac))) == NULL)
		return (NULL);

	memcpy(&mac->mac_addr, ea, sizeof(mac->mac_addr));

	if (RB_INSERT(macaddr_head, &sw->sw_addrcache, mac) != NULL)
		fatalx("cache corrupted");
	sw->sw_cachesize++;

 update:
	getmonotime(&tv);
	mac->mac_port = port;
	mac->mac_age = tv.tv_sec;

	log_debug("%s: %s mac %s on switch %u port %u",
	    __func__, oldmac == NULL ? "learned new" : "updated",
	    print_ether(ea), sw->sw_id, port);

	return (mac);
}

struct macaddr *
switch_cached(struct switch_control *sw, uint8_t *ea)
{
	struct macaddr	 key;
	memcpy(&key.mac_addr, ea, sizeof(key.mac_addr));
	return (RB_FIND(macaddr_head, &sw->sw_addrcache, &key));
}

static __inline int
switch_cmp(struct switch_control *a, struct switch_control *b)
{
	int		diff = 0;

	diff = sockaddr_cmp((struct sockaddr *)&a->sw_addr,
	    (struct sockaddr *)&b->sw_addr, 128);
	if (!diff)
		diff = socket_getport(&a->sw_addr) -
		    socket_getport(&b->sw_addr);

	return (diff);
}

static __inline int
switch_maccmp(struct macaddr *a, struct macaddr *b)
{
	return (memcmp(a->mac_addr, b->mac_addr, sizeof(a->mac_addr)));
}

RB_GENERATE(switch_head, switch_control, sw_entry, switch_cmp);
RB_GENERATE(macaddr_head, macaddr, mac_entry, switch_maccmp);
