/*	$OpenBSD: interface.c,v 1.5 2005/02/09 16:32:32 claudio Exp $ */

/*
 * Copyright (c) 2005 Claudio Jeker <claudio@openbsd.org>
 * Copyright (c) 2004, 2005 Esben Norby <norby@openbsd.org>
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
#include <unistd.h>
#include <string.h>
#include <event.h>

#include "ospfd.h"
#include "ospf.h"
#include "log.h"
#include "ospfe.h"

void		 if_hello_timer(int, short, void *);
int		 if_start_hello_timer(struct iface *);
int		 if_stop_hello_timer(struct iface *);
int		 if_stop_wait_timer(struct iface *);
void		 if_wait_timer(int, short, void *);
int		 if_start_wait_timer(struct iface *);
int		 if_stop_wait_timer(struct iface *);
struct nbr	*if_elect(struct nbr *, struct nbr *);
int		 if_set_mcast_ttl(int, u_int8_t);
int		 if_set_tos(int, int);
int		 if_set_mcast_loop(int);

struct {
	int			state;
	enum iface_event	event;
	enum iface_action	action;
	int			new_state;
} iface_fsm[] = {
    /* current state	event that happened	action to take	resulting state */
    {IF_STA_DOWN,	IF_EVT_UP,		IF_ACT_STRT,	0},
    {IF_STA_WAITING,	IF_EVT_BACKUP_SEEN,	IF_ACT_ELECT,	0},
    {IF_STA_WAITING,	IF_EVT_WTIMER,		IF_ACT_ELECT,	0},
    {IF_STA_WAITING,	IF_EVT_NBR_CHNG,	0,		0},
    {IF_STA_MULTI,	IF_EVT_NBR_CHNG,	IF_ACT_ELECT,	0},
    {IF_STA_ANY,	IF_EVT_NBR_CHNG,	0,		0},
    {IF_STA_ANY,	IF_EVT_DOWN,		IF_ACT_RST,	IF_STA_DOWN},
    {IF_STA_ANY,	IF_EVT_LOOP,		IF_ACT_RST,	IF_STA_LOOPBACK},
    {IF_STA_LOOPBACK,	IF_EVT_UNLOOP,		0,		IF_STA_DOWN},
    {-1,		0,			-1,		0},
};

const char * const if_action_names[] = {
	"START",
	"ELECT",
	"RESET"
};

const char * const if_type_names[] = {
	"POINTOPOINT",
	"BROADCAST",
	"NBMA",
	"POINTOMULTIPOINT",
	"VIRTUALLINK"
};

const char * const if_auth_names[] = {
	"NONE",
	"SIMPLE",
	"CRYPT"
};

int
if_fsm(struct iface *iface, enum iface_event event)
{
	int			old_state;
	int			new_state = 0;
	int			i, ret = 0;

	old_state = iface->state;

	for (i = 0; iface_fsm[i].state != -1; i++)
		if ((iface_fsm[i].state & old_state) &&
		    (iface_fsm[i].event == event)) {
			new_state = iface_fsm[i].new_state;
			break;
		}

	if (iface_fsm[i].state == -1) {
		/* XXX event outside of the defined fsm, ignore it. */
		log_debug("fsm_if: interface %s, "
		    "event %s not expected in state %s", iface->name,
		    nbr_event_name(event), if_state_name(old_state));
		return (0);
	}

	switch (iface_fsm[i].action) {
	case IF_ACT_STRT:
		ret = if_act_start(iface);
		break;
	case IF_ACT_ELECT:
		ret = if_act_elect(iface);
		break;
	case IF_ACT_RST:
		ret = if_act_reset(iface);
		break;
	default:
		/* do nothing */
		break;
	}

	if (ret) {
		log_debug("fsm_if: error changing state for interface %s, "
		    "event %s, state %s", iface->name, if_event_name(event),
		    if_state_name(old_state));
		return (-1);
	}

	if (new_state != 0)
		iface->state = new_state;

	log_debug("fsm_if: event %s resulted in action %s and changing "
	    "state for interface %s from %s to %s",
	    if_event_name(event), if_action_name(iface_fsm[i].action),
	    iface->name, if_state_name(old_state), if_state_name(iface->state));

	return (ret);
}

struct iface *
if_new(char *name, unsigned int idx)
{
	struct sockaddr_in	*sain;
	struct iface		*iface = NULL;
	struct ifreq		*ifr;
	int			 s;

	if ((iface = calloc(1, sizeof(*iface))) == NULL)
		errx(1, "if_new: calloc");

	iface->state = IF_STA_DOWN;
	iface->passive = true;

	LIST_INIT(&iface->nbr_list);
	TAILQ_INIT(&iface->ls_ack_list);

	evtimer_set(&iface->lsack_tx_timer, ls_ack_tx_timer, iface);

	strlcpy(iface->name, name, sizeof(iface->name));

	if ((ifr = calloc(1, sizeof(*ifr))) == NULL)
		errx(1, "if_new: calloc");

	/* set up ifreq */
	strlcpy(ifr->ifr_name, name, sizeof(ifr->ifr_name));
	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
		errx(1, "if_new: socket");

	/* get type */
	if (ioctl(s, SIOCGIFFLAGS, (caddr_t)ifr) < 0)
		errx(1, "if_new: cannot get type");
	if ((ifr->ifr_flags & IFF_POINTOPOINT))
		iface->type = IF_TYPE_POINTOPOINT;

	if ((ifr->ifr_flags & IFF_BROADCAST) &&
	    (ifr->ifr_flags & IFF_MULTICAST))
		iface->type = IF_TYPE_BROADCAST;

	iface->flags = ifr->ifr_flags;

	/* get address */
	if (ioctl(s, SIOCGIFADDR, (caddr_t)ifr) < 0)
		errx(1, "if_new: cannot get address");
	sain = (struct sockaddr_in *) &ifr->ifr_addr;
	iface->addr = sain->sin_addr;

	/* get mask */
	if (ioctl(s, SIOCGIFNETMASK, (caddr_t)ifr) < 0)
		errx(1, "if_new: cannot get mask");
	sain = (struct sockaddr_in *) &ifr->ifr_addr;
	iface->mask = sain->sin_addr;

	/* get mtu */
	if (ioctl(s, SIOCGIFMTU, (caddr_t)ifr) < 0)
		errx(1, "if_new: cannot get mtu");

	iface->mtu = ifr->ifr_mtu;
	iface->ifindex = idx;

	/* set event handlers for interface */
	evtimer_set(&iface->hello_timer, if_hello_timer, iface);
	evtimer_set(&iface->wait_timer, if_wait_timer, iface);

	free(ifr);
	close(s);

	return (iface);
}

int
if_del(struct iface *iface)
{
	struct nbr	*nbr = NULL;

	log_debug("if_del: interface %s", iface->name);

	/* clear lists etc */
	iface->self = NULL; /* trick neighbor.c code to remove self too */
	while ((nbr = LIST_FIRST(&iface->nbr_list)) != NULL) {
		LIST_REMOVE(nbr, entry);
		nbr_del(nbr);
	}

	ls_ack_list_clr(iface);

	return (-1);
}

int
if_init(struct ospfd_conf *xconf)
{
	struct area	*area = NULL;
	struct iface	*iface = NULL;

	if ((xconf->ospf_socket = socket(AF_INET, SOCK_RAW,
	    IPPROTO_OSPF)) == -1) {
		log_warn("if_init: error creating socket");
		return (-1);
	}

	/* set some defaults */
	if (if_set_mcast_ttl(xconf->ospf_socket,
	    IP_DEFAULT_MULTICAST_TTL) == -1)
		return (-1);

	if (if_set_mcast_loop(xconf->ospf_socket) == -1)
		return (-1);

	if (if_set_tos(xconf->ospf_socket, IPTOS_PREC_INTERNETCONTROL) == -1)
		return (-1);


	LIST_FOREACH(area, &xconf->area_list, entry) {
		LIST_FOREACH(iface, &area->iface_list, entry) {
			switch (iface->type) {
			case IF_TYPE_POINTOPOINT:
				break;
			case IF_TYPE_BROADCAST:
				/* all bcast interfaces use the same socket */
				iface->fd = xconf->ospf_socket;
				break;
			case IF_TYPE_NBMA:
				break;
			case IF_TYPE_POINTOMULTIPOINT:
				break;
			case IF_TYPE_VIRTUALLINK:
				break;
			default:
				fatalx("if_init: unknown interface type");
			}
		}
	}

	return (0);
}

int
if_shutdown(struct ospfd_conf *xconf)
{
	int	ret = 0;

	ret = close(xconf->ospf_socket);

	return (ret);
}

/* timers */
void
if_hello_timer(int fd, short event, void *arg)
{
	struct iface *iface = arg;
	struct timeval tv;

	send_hello(iface);

	/* reschedule hello_timer */
	if (!iface->passive) {
		timerclear(&tv);
		tv.tv_sec = iface->hello_interval;
		evtimer_add(&iface->hello_timer, &tv);
	}
}

int
if_start_hello_timer(struct iface *iface)
{
	struct timeval tv;

	timerclear(&tv);
	return (evtimer_add(&iface->hello_timer, &tv));
}

int
if_stop_hello_timer(struct iface *iface)
{
	return (evtimer_del(&iface->hello_timer));
}

void
if_wait_timer(int fd, short event, void *arg)
{
	struct iface *iface = arg;

	if_fsm(iface, IF_EVT_WTIMER);
}

int
if_start_wait_timer(struct iface *iface)
{
	struct timeval	tv;

	timerclear(&tv);
	tv.tv_sec = iface->dead_interval;
	return (evtimer_add(&iface->wait_timer, &tv));
}

int
if_stop_wait_timer(struct iface *iface)
{
	return (evtimer_del(&iface->wait_timer));
}

/* actions */
int
if_act_start(struct iface *iface)
{
	struct in_addr		 addr;

	if (iface->passive) {
		log_debug("if_act_start: cannot start passive interface %s",
		    iface->name);
		return (-1);
	}

	/* init the dummy local neighbor */
	if (iface->self == NULL)
		iface->self = nbr_new(ospfe_router_id(), iface, 1);

	/* up interface */
		/* ... */

	switch (iface->type) {
	case IF_TYPE_POINTOPOINT:
	case IF_TYPE_POINTOMULTIPOINT:
	case IF_TYPE_VIRTUALLINK:
		log_debug("if_act_start: type %s not supported, interface %s",
		    if_type_name(iface->type), iface->name);
		return (-1);
	case IF_TYPE_BROADCAST:
		inet_aton(AllSPFRouters, &addr);
		if (if_join_group(iface, &addr)) {
			log_warnx("if_act_start: error joining group %s, "
			    "interface %s", inet_ntoa(addr), iface->name);
		}
		if (iface->priority == 0) {
			iface->state = IF_STA_DROTHER;
			if (if_start_hello_timer(iface))
				log_warnx("if_act_start: cannot schedule hello "
				    "timer, interface %s", iface->name);
		} else {
			iface->state = IF_STA_WAITING;
			if (if_start_wait_timer(iface))
				log_warnx("if_act_start: cannot schedule wait "
				    "timer, interface %s", iface->name);
		}
		break;
	case IF_TYPE_NBMA:
		log_debug("if_act_start: type %s not supported, interface %s",
		    if_type_name(iface->type), iface->name);
		return (-1);
	default:
		fatalx("if_act_start: unknown interface type");
	}

	return (0);
}

struct nbr *
if_elect(struct nbr *a, struct nbr *b)
{
	if (a->priority > b->priority)
		return (a);
	if (a->priority < b->priority)
		return (b);
	if (ntohl(a->id.s_addr) > ntohl(b->id.s_addr))
		return (a);
	return (b);
}

int
if_act_elect(struct iface *iface)
{
	struct nbr	*nbr, *bdr = NULL, *dr = NULL;
	int		 round = 0;
	int		 changed = 0;
	char		 b1[16], b2[16], b3[16], b4[16];

	log_debug("if_act_elect: interface %s", iface->name);

start:
	/* elect backup designated router */
	LIST_FOREACH(nbr, &iface->nbr_list, entry) {
		if (nbr->priority == 0 || nbr == dr ||	/* not electable */
		    nbr->dr.s_addr == nbr->addr.s_addr)	/* don't elect DR */
			continue;
		if (bdr != NULL) {
			if (nbr->bdr.s_addr == nbr->addr.s_addr) {
				if (bdr->bdr.s_addr == bdr->addr.s_addr)
					bdr = if_elect(bdr, nbr);
				else
					bdr = nbr;
			} else if (bdr->bdr.s_addr != bdr->addr.s_addr)
					bdr = if_elect(bdr, nbr);
		} else
			bdr = nbr;
	}
	log_debug("if_act_elect: bdr %s", bdr ?
	    inet_ntop(AF_INET, &bdr->addr, b4, sizeof(b4)) : "none");
	/* elect designated router */
	LIST_FOREACH(nbr, &iface->nbr_list, entry) {
		if (nbr->priority == 0 || (nbr != dr &&
		    nbr->dr.s_addr != nbr->addr.s_addr))
			/* only DR may be elected check priority too */
			continue;
		if (dr == NULL || bdr == NULL)
			dr = nbr;
		else
			dr = if_elect(bdr, nbr);
	}
	log_debug("if_act_elect: dr %s", dr ?
	    inet_ntop(AF_INET, &dr->addr, b4, sizeof(b4)) : "none");
	if (dr == NULL) {
		/* no designate router found use backup DR */
		dr = bdr;
		bdr = NULL;
	}

	/*
	 * if we are involved in the election (e.g. new DR or no
	 * longer BDR) redo the election
	 */
	if (round == 0 &&
	    ((iface->self == dr && iface->self != iface->dr) ||
	    (iface->self != dr && iface->self == iface->dr) ||
	    (iface->self == bdr && iface->self != iface->bdr) ||
	    (iface->self != bdr && iface->self == iface->bdr))) {
		log_debug("if_act_elect: round two");
		round = 1;
		goto start;
	}

	log_debug("if_act_elect: interface %s old dr %s new dr %s, "
	    "old bdr %s new bdr %s", iface->name,
	    iface->dr ? inet_ntop(AF_INET, &iface->dr->addr, b1, sizeof(b1)) :
	    "none", dr ? inet_ntop(AF_INET, &dr->addr, b2, sizeof(b2)) : "none",
	    iface->bdr ? inet_ntop(AF_INET, &iface->bdr->addr, b3, sizeof(b3)) :
	    "none", bdr ? inet_ntop(AF_INET, &bdr->addr, b4, sizeof(b4)) :
	    "none");

	/*
	 * After the second round still DR or BDR change state to DR or BDR,
	 * etc.
	 */
	if (dr == iface->self)
		iface->state = IF_STA_DR;
	else if (bdr == iface->self)
		iface->state = IF_STA_BACKUP;
	else
		iface->state = IF_STA_DROTHER;

	/* TODO if iface is NBMA send all non eligible neighbors event Start */

	/*
	 * if DR or BDR changed issue a AdjOK? event for all neighbors > 2-Way
	 */
	if (iface->dr != dr || iface->bdr != bdr)
		changed = 1;

	iface->dr = dr;
	iface->bdr = bdr;

	if (changed)
		LIST_FOREACH(nbr, &iface->nbr_list, entry) {
			if (nbr->state & NBR_STA_BIDIR)
				nbr_fsm(nbr, NBR_EVT_ADJ_OK);
		}

	if (if_start_hello_timer(iface)) {
		log_warnx("if_act_elect: cannot schedule hello_timer");
		return (-1);
	}

	return (0);
}

int
if_act_reset(struct iface *iface)
{
	struct nbr		*nbr = NULL;
	struct in_addr		 addr;

	if (if_stop_hello_timer(iface)) {
		log_warnx("if_act_reset: error removing hello_timer, "
		    "interface %s", iface->name);
		return (-1);
	}

	if (if_stop_wait_timer(iface)) {
		log_warnx("if_act_reset: error removing wait_timer, "
		    "interface %s", iface->name);
		return (-1);
	}

	if (stop_ls_ack_tx_timer(iface)) {
		log_warnx("if_act_reset: error removing ls_ack_tx_timer, "
		    "interface %s", iface->name);
		return (-1);
	}

	switch (iface->type) {
	case IF_TYPE_POINTOPOINT:
		log_debug("if_act_reset: type %s not supported, interface %s",
		    if_type_name(iface->type), iface->name);
		return (-1);
	case IF_TYPE_BROADCAST:
		inet_aton(AllSPFRouters, &addr);
		if (if_leave_group(iface, &addr)) {
			log_warnx("if_act_reset: error leaving group %s, "
			    "interface %s", inet_ntoa(addr), iface->name);
		}
		break;
	case IF_TYPE_NBMA:
	case IF_TYPE_POINTOMULTIPOINT:
	case IF_TYPE_VIRTUALLINK:
		log_debug("if_act_reset: type %s not supported, interface %s",
		    if_type_name(iface->type), iface->name);
		return (-1);
	default:
		fatalx("if_act_reset: unknown interface type");
	}

	LIST_FOREACH(nbr, &iface->nbr_list, entry) {
		if (nbr_fsm(nbr, NBR_EVT_KILL_NBR)) {
			log_debug("if_act_reset: error killing neighbor %s",
			    inet_ntoa(nbr->id));
		}
	}

	while ((nbr = LIST_FIRST(&iface->nbr_list))) {
		LIST_REMOVE(nbr, entry);
		nbr_del(nbr);
	}

	iface->dr = NULL;
	iface->bdr = NULL;

	return (0);
}

struct ctl_iface *
if_to_ctl(struct iface *iface)
{
	static struct ctl_iface	 ictl;
	struct timeval		 tv, now, res;
	struct nbr		*nbr;

	memcpy(ictl.name, iface->name, sizeof(ictl.name));
	memcpy(&ictl.addr, &iface->addr, sizeof(ictl.addr));
	memcpy(&ictl.mask, &iface->mask, sizeof(ictl.mask));
	memcpy(&ictl.rtr_id, &iface->rtr_id, sizeof(ictl.rtr_id));
	memcpy(&ictl.area, &iface->area->id, sizeof(ictl.area));
	if (iface->dr) {
		memcpy(&ictl.dr_id, &iface->dr->id, sizeof(ictl.dr_id));
		memcpy(&ictl.dr_addr, &iface->dr->addr, sizeof(ictl.dr_addr));
	} else {
		bzero(&ictl.dr_id, sizeof(ictl.dr_id));
		bzero(&ictl.dr_addr, sizeof(ictl.dr_addr));
	}
	if (iface->bdr) {
		memcpy(&ictl.bdr_id, &iface->bdr->id, sizeof(ictl.bdr_id));
		memcpy(&ictl.bdr_addr, &iface->bdr->addr,
		    sizeof(ictl.bdr_addr));
	} else {
		bzero(&ictl.bdr_id, sizeof(ictl.bdr_id));
		bzero(&ictl.bdr_addr, sizeof(ictl.bdr_addr));
	}
	ictl.ifindex = iface->ifindex;
	ictl.state = iface->state;
	ictl.mtu = iface->mtu;
	ictl.nbr_cnt = 0;
	ictl.adj_cnt = 0;
	ictl.baudrate = iface->baudrate;
	ictl.dead_interval = iface->dead_interval;
	ictl.transfer_delay = iface->transfer_delay;
	ictl.hello_interval = iface->hello_interval;
	ictl.flags = iface->flags;
	ictl.metric = iface->metric;
	ictl.rxmt_interval = iface->rxmt_interval;
	ictl.type = iface->type;
	ictl.linkstate = iface->linkstate;
	ictl.priority = iface->priority;
	ictl.passive = iface->passive;

	gettimeofday(&now, NULL);
	if (evtimer_pending(&iface->hello_timer, &tv)) {
		timersub(&tv, &now, &res);
		ictl.hello_timer = res.tv_sec;
	} else
		ictl.hello_timer = -1;

	LIST_FOREACH(nbr, &iface->nbr_list, entry) {
		if (nbr == iface->self)
			continue;
		ictl.nbr_cnt++;
		if (nbr->state & NBR_STA_ADJFORM)
			ictl.adj_cnt++;
	}

	return (&ictl);
}

/* names */
const char *
if_state_name(int state)
{
	switch (state) {
	case IF_STA_DOWN:
		return ("DOWN");
	case IF_STA_LOOPBACK:
		return ("LOOPBACK");
	case IF_STA_WAITING:
		return ("WAITING");
	case IF_STA_POINTTOPOINT:
		return ("POINT-TO-POINT");
	case IF_STA_DROTHER:
		return ("DROTHER");
	case IF_STA_BACKUP:
		return ("BACKUP");
	case IF_STA_DR:
		return ("DR");
	default:
		return ("UNKNOWN");
	}
}

const char *
if_event_name(int event)
{
	return (if_event_names[event]);
}

const char *
if_action_name(int action)
{
	return (if_action_names[action]);
}

const char *
if_type_name(int type)
{
	return (if_type_names[type]);
}

const char *
if_auth_name(int type)
{
	return (if_auth_names[type]);
}

/* misc */
int
if_set_mcast_ttl(int fd, u_int8_t ttl)
{
	if (setsockopt(fd, IPPROTO_IP, IP_MULTICAST_TTL,
	    (char *)&ttl, sizeof(ttl)) < 0) {
		log_warn("if_set_mcast_ttl: error setting "
		    "IP_MULTICAST_TTL to %d", ttl);
		return (-1);
	}

	return (0);
}

int
if_set_tos(int fd, int tos)
{
	if (setsockopt(fd, IPPROTO_IP, IP_TOS,
	    (int *)&tos, sizeof(tos)) < 0) {
		log_warn("if_set_tos: error setting IP_TOS to 0x%x", tos);
		return (-1);
	}

	return (0);
}

int
if_join_group(struct iface *iface, struct in_addr *addr)
{
	struct ip_mreq	 mreq;

	switch (iface->type) {
	case IF_TYPE_BROADCAST:
		mreq.imr_multiaddr.s_addr = addr->s_addr;
		mreq.imr_interface.s_addr = iface->addr.s_addr;

		if (setsockopt(iface->fd, IPPROTO_IP, IP_ADD_MEMBERSHIP,
		    (void *)&mreq, sizeof(mreq)) < 0) {
			log_debug("if_join_group: error IP_ADD_MEMBERSHIP, "
			    "interface %s", iface->name);
			return (-1);
		}
		break;
	case IF_TYPE_POINTOPOINT:
	case IF_TYPE_POINTOMULTIPOINT:
	case IF_TYPE_VIRTUALLINK:
	case IF_TYPE_NBMA:
		log_debug("if_join_group: type %s not supported, interface %s",
		    if_type_name(iface->type), iface->name);
		return (-1);
	default:
		fatalx("if_join_group: unknown interface type");
	}

	return (0);
}

int
if_leave_group(struct iface *iface, struct in_addr *addr)
{
	struct ip_mreq	 mreq;

	switch (iface->type) {
	case IF_TYPE_BROADCAST:
		mreq.imr_multiaddr.s_addr = addr->s_addr;
		mreq.imr_interface.s_addr = iface->addr.s_addr;

		if (setsockopt(iface->fd, IPPROTO_IP, IP_DROP_MEMBERSHIP,
		    (void *)&mreq, sizeof(mreq)) < 0) {
			log_debug("if_leave_group: error IP_DROP_MEMBERSHIP, "
			    "interface %s", iface->name);
			return (-1);
		}
		break;
	case IF_TYPE_POINTOPOINT:
	case IF_TYPE_POINTOMULTIPOINT:
	case IF_TYPE_VIRTUALLINK:
	case IF_TYPE_NBMA:
		log_debug("if_leave_group: type %s not supported, interface %s",
		    if_type_name(iface->type), iface->name);
		return (-1);
	default:
		fatalx("if_leave_group: unknown interface type");
	}

	return (0);
}

int
if_set_mcast(struct iface *iface)
{
	switch (iface->type) {
	case IF_TYPE_BROADCAST:
		if (setsockopt(iface->fd, IPPROTO_IP, IP_MULTICAST_IF,
		    (char *)&iface->addr.s_addr,
		    sizeof(iface->addr.s_addr)) < 0) {
			log_debug("if_set_mcast: error setting "
			    "IP_MULTICAST_IF, interface %s", iface->name);
			return (-1);
		}
		break;
	case IF_TYPE_POINTOPOINT:
	case IF_TYPE_POINTOMULTIPOINT:
	case IF_TYPE_VIRTUALLINK:
	case IF_TYPE_NBMA:
		log_debug("if_set_mcast: type %s not supported, interface %s",
		    if_type_name(iface->type), iface->name);
		return (-1);
	default:
		fatalx("if_set_mcast: unknown interface type");
	}

	return (0);
}

int
if_set_mcast_loop(int fd)
{
	u_int8_t	loop = 0;

	if (setsockopt(fd, IPPROTO_IP, IP_MULTICAST_LOOP,
	    (char *)&loop, sizeof(loop)) < 0) {
		log_warn("if_set_mcast_loop: error setting IP_MULTICAST_LOOP");
		return (-1);
	}

	return (0);
}
