/*	$OpenBSD: neighbor.c,v 1.18 2010/06/30 01:47:11 claudio Exp $ */

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
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>

#include <ctype.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <event.h>
#include <unistd.h>

#include "ldpd.h"
#include "ldp.h"
#include "ldpe.h"
#include "log.h"
#include "lde.h"

int	nbr_establish_connection(struct nbr *);
void	nbr_send_labelmappings(struct nbr *);
int	nbr_act_session_operational(struct nbr *);

LIST_HEAD(nbr_head, nbr);

struct nbr_table {
	struct nbr_head		*hashtbl;
	u_int32_t		 hashmask;
} nbrtable;

#define NBR_HASH(x)		\
	&nbrtable.hashtbl[(x) & nbrtable.hashmask]

u_int32_t	peercnt = NBR_CNTSTART;

extern struct ldpd_conf	*leconf;

struct {
	int		state;
	enum nbr_event	event;
	enum nbr_action	action;
	int		new_state;
} nbr_fsm_tbl[] = {
    /* current state	event that happened	action to take		resulting state */
/* Discovery States */
    {NBR_STA_DOWN,	NBR_EVT_HELLO_RCVD,	NBR_ACT_STRT_ITIMER,	NBR_STA_PRESENT},
    {NBR_STA_UP,	NBR_EVT_HELLO_RCVD,	NBR_ACT_RST_ITIMER,	0},
/* Passive Role */
    {NBR_STA_PRESENT,	NBR_EVT_SESSION_UP,	NBR_ACT_SESSION_EST,	NBR_STA_INITIAL},
    {NBR_STA_INITIAL,	NBR_EVT_INIT_RCVD,	NBR_ACT_INIT_SEND,	NBR_STA_OPENREC},
    {NBR_STA_OPENREC,	NBR_EVT_KEEPALIVE_RCVD,	NBR_ACT_STRT_KTIMER,	NBR_STA_OPER},
/* Active Role */
    {NBR_STA_PRESENT,	NBR_EVT_INIT_SENT,	NBR_ACT_NOTHING,	NBR_STA_OPENSENT},
    {NBR_STA_OPENSENT,	NBR_EVT_INIT_RCVD,	NBR_ACT_KEEPALIVE_SEND,	NBR_STA_OPER},
/* Session Maintenance */
    {NBR_STA_OPER,	NBR_EVT_PDU_RCVD,	NBR_ACT_RST_KTIMEOUT,	0},
/* Session Close */
    {NBR_STA_SESSION,	NBR_EVT_CLOSE_SESSION,	NBR_ACT_CLOSE_SESSION,	NBR_STA_PRESENT},
    {NBR_STA_UP,	NBR_EVT_DOWN,		NBR_ACT_CLOSE_SESSION,	},
    {-1,		NBR_EVT_NOTHING,	NBR_ACT_NOTHING,	0},
};

const char * const nbr_event_names[] = {
	"NOTHING",
	"HELLO RECEIVED",
	"SESSION UP",
	"SESSION CLOSE",
	"INIT RECEIVED",
	"KEEPALIVE RECEIVED",
	"PDU RECEIVED",
	"INIT SENT",
	"DOWN"
};

const char * const nbr_action_names[] = {
	"NOTHING",
	"START INACTIVITY TIMER",
	"RESET INACTIVITY TIMER",
	"RESET KEEPALIVE TIMEOUT",
	"START KEEPALIVE TIMER",
	"RESET KEEPALIVE TIMER",
	"START NEIGHBOR SESSION",
	"SEND INIT",
	"SEND KEEPALIVE",
	"CLOSE SESSION"
};

int
nbr_fsm(struct nbr *nbr, enum nbr_event event)
{
	struct timeval	now;
	int		old_state;
	int		new_state = 0;
	int		i, ret = 0;

	old_state = nbr->state;
	for (i = 0; nbr_fsm_tbl[i].state != -1; i++)
		if ((nbr_fsm_tbl[i].state & old_state) &&
		    (nbr_fsm_tbl[i].event == event)) {
			new_state = nbr_fsm_tbl[i].new_state;
			break;
		}

	if (nbr_fsm_tbl[i].state == -1) {
		/* event outside of the defined fsm, ignore it. */
		log_warnx("nbr_fsm: neighbor ID %s, "
		    "event %s not expected in state %s",
		    inet_ntoa(nbr->id), nbr_event_names[event],
		    nbr_state_name(old_state));
		return (0);
	}

	ret = 0;

	switch (nbr_fsm_tbl[i].action) {
	case NBR_ACT_RST_ITIMER:
		nbr_reset_itimer(nbr);
		break;
	case NBR_ACT_STRT_ITIMER:
		nbr_start_itimer(nbr);
		break;
	case NBR_ACT_RST_KTIMEOUT:
		nbr_reset_ktimeout(nbr);
		break;
	case NBR_ACT_RST_KTIMER:
		nbr_reset_ktimer(nbr);
		break;
	case NBR_ACT_STRT_KTIMER:
		nbr_act_session_operational(nbr);
		nbr_start_ktimer(nbr);
		nbr_start_ktimeout(nbr);
		send_address(nbr, NULL);
		nbr_send_labelmappings(nbr);
		break;
	case NBR_ACT_SESSION_EST:
		ret = nbr_act_session_establish(nbr, 0);
		break;
	case NBR_ACT_INIT_SEND:
		send_init(nbr);
		send_keepalive(nbr);
		break;
	case NBR_ACT_KEEPALIVE_SEND:
		nbr_act_session_operational(nbr);
		nbr_start_ktimer(nbr);
		nbr_start_ktimeout(nbr);
		send_keepalive(nbr);
		send_address(nbr, NULL);
		nbr_send_labelmappings(nbr);
		break;
	case NBR_ACT_CLOSE_SESSION:
		ldpe_imsg_compose_lde(IMSG_NEIGHBOR_DOWN, nbr->peerid, 0,
		    NULL, 0);
		session_close(nbr);
		nbr_start_idtimer(nbr);
		break;
	case NBR_ACT_NOTHING:
		/* do nothing */
		break;
	}

	if (ret) {
		log_warnx("nbr_fsm: error changing state for neighbor ID %s, "
		    "event %s, state %s", inet_ntoa(nbr->id),
		    nbr_event_names[event], nbr_state_name(old_state));
		return (-1);
	}

	if (new_state != 0)
		nbr->state = new_state;

	if (old_state != nbr->state) {
		log_debug("nbr_fsm: event %s resulted in action %s and "
		    "changing state for neighbor ID %s from %s to %s",
		    nbr_event_names[event],
		    nbr_action_names[nbr_fsm_tbl[i].action],
		    inet_ntoa(nbr->id), nbr_state_name(old_state),
		    nbr_state_name(nbr->state));

		if (nbr->state == NBR_STA_OPER) {
			gettimeofday(&now, NULL);
			nbr->uptime = now.tv_sec;
		}
	}

	return (ret);
}

void
nbr_init(u_int32_t hashsize)
{
	u_int32_t        hs, i;

	for (hs = 1; hs < hashsize; hs <<= 1)
		;
	nbrtable.hashtbl = calloc(hs, sizeof(struct nbr_head));
	if (nbrtable.hashtbl == NULL)
		fatal("nbr_init");

	for (i = 0; i < hs; i++)
		LIST_INIT(&nbrtable.hashtbl[i]);

	nbrtable.hashmask = hs - 1;
}

struct nbr *
nbr_new(u_int32_t nbr_id, u_int16_t lspace, struct iface *iface)
{
	struct nbr_head	*head;
	struct nbr	*nbr;

	if ((nbr = calloc(1, sizeof(*nbr))) == NULL)
		fatal("nbr_new");
	if ((nbr->rbuf = calloc(1, sizeof(struct ibuf_read))) == NULL)
		fatal("nbr_new");

	nbr->state = NBR_STA_DOWN;
	nbr->id.s_addr = nbr_id;
	nbr->lspace = lspace;

	/* get next unused peerid */
	while (nbr_find_peerid(++peercnt))
		;
	nbr->peerid = peercnt;
	head = NBR_HASH(nbr->peerid);
	LIST_INSERT_HEAD(head, nbr, hash);

	/* add to peer list */
	nbr->iface = iface;
	LIST_INSERT_HEAD(&iface->nbr_list, nbr, entry);

	TAILQ_INIT(&nbr->mapping_list);
	TAILQ_INIT(&nbr->withdraw_list);
	TAILQ_INIT(&nbr->request_list);
	TAILQ_INIT(&nbr->release_list);
	TAILQ_INIT(&nbr->abortreq_list);

	/* set event structures */
	evtimer_set(&nbr->inactivity_timer, nbr_itimer, nbr);
	evtimer_set(&nbr->keepalive_timeout, nbr_ktimeout, nbr);
	evtimer_set(&nbr->keepalive_timer, nbr_ktimer, nbr);
	evtimer_set(&nbr->initdelay_timer, nbr_idtimer, nbr);

	return (nbr);
}

void
nbr_del(struct nbr *nbr)
{
	session_close(nbr);

	if (evtimer_pending(&nbr->inactivity_timer, NULL))
		evtimer_del(&nbr->inactivity_timer);
	if (evtimer_pending(&nbr->keepalive_timer, NULL))
		evtimer_del(&nbr->keepalive_timer);
	if (evtimer_pending(&nbr->keepalive_timeout, NULL))
		evtimer_del(&nbr->keepalive_timeout);
	if (evtimer_pending(&nbr->initdelay_timer, NULL))
		evtimer_del(&nbr->initdelay_timer);

	nbr_mapping_list_clr(nbr, &nbr->mapping_list);

	LIST_REMOVE(nbr, entry);
	LIST_REMOVE(nbr, hash);

	free(nbr->rbuf);
	free(nbr);
}

struct nbr *
nbr_find_peerid(u_int32_t peerid)
{
	struct nbr_head	*head;
	struct nbr	*nbr;

	head = NBR_HASH(peerid);

	LIST_FOREACH(nbr, head, hash) {
		if (nbr->peerid == peerid)
			return (nbr);
	}

	return (NULL);
}

struct nbr *
nbr_find_ip(struct iface *iface, u_int32_t rtr_id)
{
	struct nbr	*nbr = NULL;

	LIST_FOREACH(nbr, &iface->nbr_list, entry) {
		if (nbr->addr.s_addr == rtr_id)
			return (nbr);
	}

	return (NULL);
}

struct nbr *
nbr_find_ldpid(struct iface *iface, u_int32_t rtr_id, u_int16_t lspace)
{
	struct nbr	*nbr = NULL;

	LIST_FOREACH(nbr, &iface->nbr_list, entry) {
		if (nbr->id.s_addr == rtr_id && nbr->lspace == lspace)
			return (nbr);
	}

	return (NULL);
}

/* timers */

/* Inactivity timer: timeout based on hellos */
/* ARGSUSED */
void
nbr_itimer(int fd, short event, void *arg)
{
	struct nbr *nbr = arg;

	log_debug("nbr_itimer: neighbor ID %s peerid %lu", inet_ntoa(nbr->id),
	    nbr->peerid);

	nbr_del(nbr);
}

void
nbr_start_itimer(struct nbr *nbr)
{
	struct timeval	tv;

	timerclear(&tv);
	tv.tv_sec = nbr->holdtime;

	if (evtimer_add(&nbr->inactivity_timer, &tv) == -1)
		fatal("nbr_start_itimer");
}

void
nbr_stop_itimer(struct nbr *nbr)
{
	if (evtimer_del(&nbr->inactivity_timer) == -1)
		fatal("nbr_stop_itimer");
}

void
nbr_reset_itimer(struct nbr *nbr)
{
	struct timeval	tv;

	timerclear(&tv);
	tv.tv_sec = nbr->holdtime;

	if (evtimer_add(&nbr->inactivity_timer, &tv) == -1)
		fatal("nbr_reset_itimer");
}

/* Keepalive timer: timer to send keepalive message to neighbors */

void
nbr_ktimer(int fd, short event, void *arg)
{
	struct nbr	*nbr = arg;
	struct timeval	 tv;

	send_keepalive(nbr);

	timerclear(&tv);
	tv.tv_sec = (time_t)(nbr->keepalive / KEEPALIVE_PER_PERIOD);
	if (evtimer_add(&nbr->keepalive_timer, &tv) == -1)
		fatal("nbr_ktimer");
}

void
nbr_start_ktimer(struct nbr *nbr)
{
	struct timeval	tv;

	timerclear(&tv);

	/* XXX: just to be sure it will send three keepalives per period */
	tv.tv_sec = (time_t)(nbr->keepalive / KEEPALIVE_PER_PERIOD);

	if (evtimer_add(&nbr->keepalive_timer, &tv) == -1)
		fatal("nbr_start_ktimer");
}

void
nbr_stop_ktimer(struct nbr *nbr)
{
	if (evtimer_del(&nbr->keepalive_timer) == -1)
		fatal("nbr_stop_ktimer");
}

void
nbr_reset_ktimer(struct nbr *nbr)
{
	struct timeval	tv;

	timerclear(&tv);

	/* XXX: just to be sure it will send three keepalives per period */
	tv.tv_sec = (time_t)(nbr->keepalive / KEEPALIVE_PER_PERIOD);

	if (evtimer_add(&nbr->keepalive_timer, &tv) == -1)
		fatal("nbr_reset_ktimer");
}

/* Keepalive timeout: if the nbr hasn't sent keepalive */

void
nbr_ktimeout(int fd, short event, void *arg)
{
	struct nbr *nbr = arg;

	log_debug("nbr_ktimeout: neighbor ID %s peerid %lu", inet_ntoa(nbr->id),
	    nbr->peerid);

	send_notification_nbr(nbr, S_KEEPALIVE_TMR, 0, 0);
	/* XXX race, send_notification_nbr() has no chance to be sent */
	session_close(nbr);
}

void
nbr_start_ktimeout(struct nbr *nbr)
{
	struct timeval	tv;

	timerclear(&tv);
	tv.tv_sec = nbr->keepalive;

	if (evtimer_add(&nbr->keepalive_timeout, &tv) == -1)
		fatal("nbr_start_ktimeout");
}

void
nbr_stop_ktimeout(struct nbr *nbr)
{
	if (evtimer_del(&nbr->keepalive_timeout) == -1)
		fatal("nbr_stop_ktimeout");
}

void
nbr_reset_ktimeout(struct nbr *nbr)
{
	struct timeval	tv;

	timerclear(&tv);
	tv.tv_sec = nbr->keepalive;

	if (evtimer_add(&nbr->keepalive_timeout, &tv) == -1)
		fatal("nbr_reset_ktimeout");
}

/* Init delay timer: timer to retry to iniziatize session */

void
nbr_idtimer(int fd, short event, void *arg)
{
	struct nbr *nbr = arg;

	log_debug("nbr_idtimer: neighbor ID %s peerid %lu", inet_ntoa(nbr->id),
	    nbr->peerid);

	nbr_act_session_establish(nbr, 1);
}

void
nbr_start_idtimer(struct nbr *nbr)
{
	struct timeval	tv;

	timerclear(&tv);
	tv.tv_sec = INIT_DELAY_TMR;

	if (evtimer_add(&nbr->initdelay_timer, &tv) == -1)
		fatal("nbr_start_idtimer");
}

void
nbr_stop_idtimer(struct nbr *nbr)
{
	if (evtimer_del(&nbr->initdelay_timer) == -1)
		fatal("nbr_stop_idtimer");
}

int
nbr_pending_idtimer(struct nbr *nbr)
{
	if (evtimer_pending(&nbr->initdelay_timer, NULL))
		return (1);

	return (0);
}


void
nbr_reset_idtimer(struct nbr *nbr)
{
	struct timeval	tv;

	timerclear(&tv);
	tv.tv_sec = INIT_DELAY_TMR;

	if (evtimer_add(&nbr->initdelay_timer, &tv) == -1)
		fatal("nbr_reset_idtimer");
}

int
nbr_establish_connection(struct nbr *nbr)
{
	struct sockaddr_in	in;

	bzero(&in, sizeof(in));
	in.sin_family = AF_INET;
	in.sin_port = htons(LDP_PORT);
	in.sin_addr.s_addr = nbr->addr.s_addr;

	nbr->fd = socket(AF_INET, SOCK_STREAM, 0);
	if (nbr->fd == -1) {
		log_debug("nbr_establish_connection: error while "
		    "creating socket");
		return (-1);
	}

	if (connect(nbr->fd, (struct sockaddr *)&in, sizeof(in)) == -1) {
		log_debug("nbr_establish_connection: error while "
		    "connecting to %s", inet_ntoa(nbr->addr));
		nbr_start_idtimer(nbr);
		close(nbr->fd);
		return (-1);
	}

	return (0);
}

int
nbr_act_session_establish(struct nbr *nbr, int active)
{
	if (active) {
		if (nbr_establish_connection(nbr) < 0)
			return (-1);
	}

	evbuf_init(&nbr->wbuf, nbr->fd, session_write, nbr);
	event_set(&nbr->rev, nbr->fd, EV_READ | EV_PERSIST, session_read, nbr);
	event_add(&nbr->rev, NULL);

	if (active) {
		send_init(nbr);
		nbr_fsm(nbr, NBR_EVT_INIT_SENT);
	}


	return (0);
}

int
nbr_act_session_operational(struct nbr *nbr)
{
	struct lde_nbr	 rn;

	bzero(&rn, sizeof(rn));
	rn.id.s_addr = nbr->id.s_addr;
	rn.lspace = nbr->lspace;

	return (ldpe_imsg_compose_lde(IMSG_NEIGHBOR_UP, nbr->peerid, 0, &rn,
	    sizeof(rn)));
}

void
nbr_send_labelmappings(struct nbr *nbr)
{
	if (leconf->mode & MODE_ADV_UNSOLICITED) {
		ldpe_imsg_compose_lde(IMSG_LABEL_MAPPING_FULL, nbr->peerid, 0,
		    NULL, 0);
	}
}

void
nbr_mapping_add(struct nbr *nbr, struct mapping_head *mh, struct map *map)
{
	struct mapping_entry	*me;

	me = calloc(1, sizeof(*me));
	if (me == NULL)
		fatal("nbr_mapping_add");

	me->prefix = map->prefix.s_addr;
	me->prefixlen = map->prefixlen;
	me->label = map->label;

	TAILQ_INSERT_TAIL(mh, me, entry);
}

struct mapping_entry *
nbr_mapping_find(struct nbr *nbr, struct mapping_head *mh, struct map *map)
{
	struct mapping_entry	*me = NULL;

	TAILQ_FOREACH(me, mh, entry) {
		if (me->prefix == map->prefix.s_addr &&
		    me->prefixlen == map->prefixlen)
			return (me);
	}

	return (NULL);
}

void
nbr_mapping_del(struct nbr *nbr, struct mapping_head *mh, struct map *map)
{
	struct mapping_entry	*me;

	me = nbr_mapping_find(nbr, mh, map);
	if (me == NULL)
		return;

	TAILQ_REMOVE(mh, me, entry);
	free(me);
}

void
nbr_mapping_list_clr(struct nbr *nbr, struct mapping_head *mh)
{
	struct mapping_entry	*me;

	while ((me = TAILQ_FIRST(mh)) != NULL) {
		TAILQ_REMOVE(mh, me, entry);
		free(me);
	}
}

struct ctl_nbr *
nbr_to_ctl(struct nbr *nbr)
{
	static struct ctl_nbr	 nctl;
	struct timeval		 tv, now, res;

	memcpy(nctl.name, nbr->iface->name, sizeof(nctl.name));
	memcpy(&nctl.id, &nbr->id, sizeof(nctl.id));
	memcpy(&nctl.addr, &nbr->addr, sizeof(nctl.addr));

	nctl.nbr_state = nbr->state;
	nctl.iface_state = nbr->iface->state;

	gettimeofday(&now, NULL);
	if (evtimer_pending(&nbr->inactivity_timer, &tv)) {
		timersub(&tv, &now, &res);
		if (nbr->state & NBR_STA_DOWN)
			nctl.dead_timer = DEFAULT_NBR_TMOUT - res.tv_sec;
		else
			nctl.dead_timer = res.tv_sec;
	} else
		nctl.dead_timer = 0;

	if (nbr->state == NBR_STA_OPER) {
		nctl.uptime = now.tv_sec - nbr->uptime;
	} else
		nctl.uptime = 0;

	return (&nctl);
}
