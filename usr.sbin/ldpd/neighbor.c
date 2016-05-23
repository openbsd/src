/*	$OpenBSD: neighbor.c,v 1.59 2016/05/23 15:59:55 renato Exp $ */

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
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <net/if.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <event.h>
#include <unistd.h>

#include "ldpd.h"
#include "ldp.h"
#include "ldpe.h"
#include "control.h"
#include "log.h"
#include "lde.h"

void	nbr_send_labelmappings(struct nbr *);
int	nbr_act_session_operational(struct nbr *);

static __inline int nbr_id_compare(struct nbr *, struct nbr *);
static __inline int nbr_pid_compare(struct nbr *, struct nbr *);

RB_HEAD(nbr_id_head, nbr);
RB_GENERATE(nbr_id_head, nbr, id_tree, nbr_id_compare)
RB_HEAD(nbr_pid_head, nbr);
RB_GENERATE(nbr_pid_head, nbr, pid_tree, nbr_pid_compare)

static __inline int
nbr_id_compare(struct nbr *a, struct nbr *b)
{
	return (ntohl(a->id.s_addr) - ntohl(b->id.s_addr));
}

static __inline int
nbr_pid_compare(struct nbr *a, struct nbr *b)
{
	return (a->peerid - b->peerid);
}

struct nbr_id_head nbrs_by_id = RB_INITIALIZER(&nbrs_by_id);
struct nbr_pid_head nbrs_by_pid = RB_INITIALIZER(&nbrs_by_pid);

u_int32_t	peercnt = 1;

extern struct ldpd_conf		*leconf;
extern struct ldpd_sysdep	 sysdep;

struct {
	int		state;
	enum nbr_event	event;
	enum nbr_action	action;
	int		new_state;
} nbr_fsm_tbl[] = {
    /* current state	event that happened	action to take		resulting state */
/* Passive Role */
    {NBR_STA_PRESENT,	NBR_EVT_MATCH_ADJ,	NBR_ACT_NOTHING,	NBR_STA_INITIAL},
    {NBR_STA_INITIAL,	NBR_EVT_INIT_RCVD,	NBR_ACT_PASSIVE_INIT,	NBR_STA_OPENREC},
    {NBR_STA_OPENREC,	NBR_EVT_KEEPALIVE_RCVD,	NBR_ACT_SESSION_EST,	NBR_STA_OPER},
/* Active Role */
    {NBR_STA_PRESENT,	NBR_EVT_CONNECT_UP,	NBR_ACT_CONNECT_SETUP,	NBR_STA_INITIAL},
    {NBR_STA_INITIAL,	NBR_EVT_INIT_SENT,	NBR_ACT_NOTHING,	NBR_STA_OPENSENT},
    {NBR_STA_OPENSENT,	NBR_EVT_INIT_RCVD,	NBR_ACT_KEEPALIVE_SEND,	NBR_STA_OPENREC},
/* Session Maintenance */
    {NBR_STA_OPER,	NBR_EVT_PDU_RCVD,	NBR_ACT_RST_KTIMEOUT,	0},
    {NBR_STA_OPER,	NBR_EVT_PDU_SENT,	NBR_ACT_RST_KTIMER,	0},
/* Session Close */
    {NBR_STA_PRESENT,	NBR_EVT_CLOSE_SESSION,	NBR_ACT_NOTHING,	0},
    {NBR_STA_SESSION,	NBR_EVT_CLOSE_SESSION,	NBR_ACT_CLOSE_SESSION,	NBR_STA_PRESENT},
    {-1,		NBR_EVT_NOTHING,	NBR_ACT_NOTHING,	0},
};

const char * const nbr_event_names[] = {
	"NOTHING",
	"ADJACENCY MATCHED",
	"CONNECTION UP",
	"SESSION CLOSE",
	"INIT RECEIVED",
	"KEEPALIVE RECEIVED",
	"PDU RECEIVED",
	"PDU SENT",
	"INIT SENT"
};

const char * const nbr_action_names[] = {
	"NOTHING",
	"RESET KEEPALIVE TIMEOUT",
	"START NEIGHBOR SESSION",
	"RESET KEEPALIVE TIMER",
	"SETUP NEIGHBOR CONNECTION",
	"SEND INIT AND KEEPALIVE",
	"SEND KEEPALIVE",
	"CLOSE SESSION"
};

int
nbr_fsm(struct nbr *nbr, enum nbr_event event)
{
	struct timeval	now;
	int		old_state;
	int		new_state = 0;
	int		i;

	old_state = nbr->state;
	for (i = 0; nbr_fsm_tbl[i].state != -1; i++)
		if ((nbr_fsm_tbl[i].state & old_state) &&
		    (nbr_fsm_tbl[i].event == event)) {
			new_state = nbr_fsm_tbl[i].new_state;
			break;
		}

	if (nbr_fsm_tbl[i].state == -1) {
		/* event outside of the defined fsm, ignore it. */
		log_warnx("%s: neighbor ID %s, event %s not expected in "
		    "state %s", __func__, inet_ntoa(nbr->id),
		    nbr_event_names[event], nbr_state_name(old_state));
		return (0);
	}

	if (new_state != 0)
		nbr->state = new_state;

	if (old_state != nbr->state) {
		log_debug("%s: event %s resulted in action %s and "
		    "changing state for neighbor ID %s from %s to %s",
		    __func__, nbr_event_names[event],
		    nbr_action_names[nbr_fsm_tbl[i].action],
		    inet_ntoa(nbr->id), nbr_state_name(old_state),
		    nbr_state_name(nbr->state));

		if (nbr->state == NBR_STA_OPER) {
			gettimeofday(&now, NULL);
			nbr->uptime = now.tv_sec;
		}
	}

	switch (nbr_fsm_tbl[i].action) {
	case NBR_ACT_RST_KTIMEOUT:
		nbr_start_ktimeout(nbr);
		break;
	case NBR_ACT_RST_KTIMER:
		nbr_start_ktimer(nbr);
		break;
	case NBR_ACT_SESSION_EST:
		nbr_act_session_operational(nbr);
		nbr_start_ktimer(nbr);
		nbr_start_ktimeout(nbr);
		send_address(nbr, NULL, 0);
		nbr_send_labelmappings(nbr);
		break;
	case NBR_ACT_CONNECT_SETUP:
		nbr->tcp = tcp_new(nbr->fd, nbr);

		/* trigger next state */
		send_init(nbr);
		nbr_fsm(nbr, NBR_EVT_INIT_SENT);
		break;
	case NBR_ACT_PASSIVE_INIT:
		send_init(nbr);
		send_keepalive(nbr);
		break;
	case NBR_ACT_KEEPALIVE_SEND:
		nbr_start_ktimeout(nbr);
		send_keepalive(nbr);
		break;
	case NBR_ACT_CLOSE_SESSION:
		ldpe_imsg_compose_lde(IMSG_NEIGHBOR_DOWN, nbr->peerid, 0,
		    NULL, 0);
		session_close(nbr);
		break;
	case NBR_ACT_NOTHING:
		/* do nothing */
		break;
	}

	return (0);
}

struct nbr *
nbr_new(struct in_addr id, struct in_addr addr)
{
	struct nbr		*nbr;
	struct nbr_params	*nbrp;

	log_debug("%s: LSR ID %s", __func__, inet_ntoa(id));

	if ((nbr = calloc(1, sizeof(*nbr))) == NULL)
		fatal(__func__);

	LIST_INIT(&nbr->adj_list);
	nbr->state = NBR_STA_PRESENT;
	nbr->id.s_addr = id.s_addr;
	nbr->laddr.s_addr = leconf->trans_addr.s_addr;
	nbr->raddr.s_addr = addr.s_addr;
	nbr->peerid = 0;

	if (RB_INSERT(nbr_id_head, &nbrs_by_id, nbr) != NULL)
		fatalx("nbr_new: RB_INSERT(nbrs_by_id) failed");

	TAILQ_INIT(&nbr->mapping_list);
	TAILQ_INIT(&nbr->withdraw_list);
	TAILQ_INIT(&nbr->request_list);
	TAILQ_INIT(&nbr->release_list);
	TAILQ_INIT(&nbr->abortreq_list);

	/* set event structures */
	evtimer_set(&nbr->keepalive_timeout, nbr_ktimeout, nbr);
	evtimer_set(&nbr->keepalive_timer, nbr_ktimer, nbr);
	evtimer_set(&nbr->initdelay_timer, nbr_idtimer, nbr);

	/* init pfkey - remove old if any, load new ones */
	pfkey_remove(nbr);
	nbrp = nbr_params_find(leconf, nbr->raddr);
	if (nbrp && pfkey_establish(nbr, nbrp) == -1)
		fatalx("pfkey setup failed");

	return (nbr);
}

void
nbr_del(struct nbr *nbr)
{
	log_debug("%s: LSR ID %s", __func__, inet_ntoa(nbr->id));

	nbr_fsm(nbr, NBR_EVT_CLOSE_SESSION);
	pfkey_remove(nbr);

	if (event_pending(&nbr->ev_connect, EV_WRITE, NULL))
		event_del(&nbr->ev_connect);
	nbr_stop_ktimer(nbr);
	nbr_stop_ktimeout(nbr);
	nbr_stop_idtimer(nbr);

	mapping_list_clr(&nbr->mapping_list);
	mapping_list_clr(&nbr->withdraw_list);
	mapping_list_clr(&nbr->request_list);
	mapping_list_clr(&nbr->release_list);
	mapping_list_clr(&nbr->abortreq_list);

	if (nbr->peerid)
		RB_REMOVE(nbr_pid_head, &nbrs_by_pid, nbr);
	RB_REMOVE(nbr_id_head, &nbrs_by_id, nbr);

	free(nbr);
}

void
nbr_update_peerid(struct nbr *nbr)
{
	if (nbr->peerid)
		RB_REMOVE(nbr_pid_head, &nbrs_by_pid, nbr);

	/* get next unused peerid */
	while (nbr_find_peerid(++peercnt))
		;
	nbr->peerid = peercnt;

	if (RB_INSERT(nbr_pid_head, &nbrs_by_pid, nbr) != NULL)
		fatalx("nbr_new: RB_INSERT(nbrs_by_pid) failed");
}

struct nbr *
nbr_find_peerid(u_int32_t peerid)
{
	struct nbr	n;
	n.peerid = peerid;
	return (RB_FIND(nbr_pid_head, &nbrs_by_pid, &n));
}

struct nbr *
nbr_find_ldpid(u_int32_t rtr_id)
{
	struct nbr	n;
	n.id.s_addr = rtr_id;
	return (RB_FIND(nbr_id_head, &nbrs_by_id, &n));
}

int
nbr_session_active_role(struct nbr *nbr)
{
	if (ntohl(nbr->laddr.s_addr) > ntohl(nbr->raddr.s_addr))
		return (1);

	return (0);
}

/* timers */

/* Keepalive timer: timer to send keepalive message to neighbors */

void
nbr_ktimer(int fd, short event, void *arg)
{
	struct nbr	*nbr = arg;

	send_keepalive(nbr);
	nbr_start_ktimer(nbr);
}

void
nbr_start_ktimer(struct nbr *nbr)
{
	struct timeval	 tv;

	/* send three keepalives per period */
	timerclear(&tv);
	tv.tv_sec = (time_t)(nbr->keepalive / KEEPALIVE_PER_PERIOD);
	if (evtimer_add(&nbr->keepalive_timer, &tv) == -1)
		fatal(__func__);
}

void
nbr_stop_ktimer(struct nbr *nbr)
{
	if (evtimer_pending(&nbr->keepalive_timer, NULL) &&
	    evtimer_del(&nbr->keepalive_timer) == -1)
		fatal(__func__);
}

/* Keepalive timeout: if the nbr hasn't sent keepalive */

void
nbr_ktimeout(int fd, short event, void *arg)
{
	struct nbr *nbr = arg;

	log_debug("%s: neighbor ID %s", __func__, inet_ntoa(nbr->id));

	session_shutdown(nbr, S_KEEPALIVE_TMR, 0, 0);
}

void
nbr_start_ktimeout(struct nbr *nbr)
{
	struct timeval	tv;

	timerclear(&tv);
	tv.tv_sec = nbr->keepalive;

	if (evtimer_add(&nbr->keepalive_timeout, &tv) == -1)
		fatal(__func__);
}

void
nbr_stop_ktimeout(struct nbr *nbr)
{
	if (evtimer_pending(&nbr->keepalive_timeout, NULL) &&
	    evtimer_del(&nbr->keepalive_timeout) == -1)
		fatal(__func__);
}

/* Init delay timer: timer to retry to iniziatize session */

void
nbr_idtimer(int fd, short event, void *arg)
{
	struct nbr *nbr = arg;

	log_debug("%s: neighbor ID %s", __func__, inet_ntoa(nbr->id));

	nbr_establish_connection(nbr);
}

void
nbr_start_idtimer(struct nbr *nbr)
{
	struct timeval	tv;

	timerclear(&tv);

	tv.tv_sec = INIT_DELAY_TMR;
	switch(nbr->idtimer_cnt) {
	default:
		/* do not further increase the counter */
		tv.tv_sec = MAX_DELAY_TMR;
		break;
	case 2:
		tv.tv_sec *= 2;
		/* FALLTHROUGH */
	case 1:
		tv.tv_sec *= 2;
		/* FALLTHROUGH */
	case 0:
		nbr->idtimer_cnt++;
		break;
	}

	if (evtimer_add(&nbr->initdelay_timer, &tv) == -1)
		fatal(__func__);
}

void
nbr_stop_idtimer(struct nbr *nbr)
{
	if (evtimer_pending(&nbr->initdelay_timer, NULL) &&
	    evtimer_del(&nbr->initdelay_timer) == -1)
		fatal(__func__);
}

int
nbr_pending_idtimer(struct nbr *nbr)
{
	if (evtimer_pending(&nbr->initdelay_timer, NULL))
		return (1);

	return (0);
}

int
nbr_pending_connect(struct nbr *nbr)
{
	if (event_initialized(&nbr->ev_connect) &&
	    event_pending(&nbr->ev_connect, EV_WRITE, NULL))
		return (1);

	return (0);
}

static void
nbr_connect_cb(int fd, short event, void *arg)
{
	struct nbr	*nbr = arg;
	int		 error;
	socklen_t	 len;

	len = sizeof(error);
	if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
		log_warn("%s: getsockopt SOL_SOCKET SO_ERROR", __func__);
		return;
	}

	if (error) {
		close(nbr->fd);
		errno = error;
		log_debug("%s: error while connecting to %s: %s", __func__,
		    inet_ntoa(nbr->raddr), strerror(errno));
		return;
	}

	nbr_fsm(nbr, NBR_EVT_CONNECT_UP);
}

int
nbr_establish_connection(struct nbr *nbr)
{
	struct sockaddr_in	 local_sa;
	struct sockaddr_in	 remote_sa;
	struct adj		*adj;
	struct nbr_params	*nbrp;
	int			 opt = 1;

	nbr->fd = socket(AF_INET, SOCK_STREAM|SOCK_NONBLOCK|SOCK_CLOEXEC, 0);
	if (nbr->fd == -1) {
		log_warn("%s: error while creating socket", __func__);
		return (-1);
	}

	nbrp = nbr_params_find(leconf, nbr->raddr);
	if (nbrp && nbrp->auth.method == AUTH_MD5SIG) {
		if (sysdep.no_pfkey || sysdep.no_md5sig) {
			log_warnx("md5sig configured but not available");
			return (-1);
		}
		if (setsockopt(nbr->fd, IPPROTO_TCP, TCP_MD5SIG,
		    &opt, sizeof(opt)) == -1) {
			log_warn("setsockopt md5sig");
			return (-1);
		}
	}

	bzero(&local_sa, sizeof(local_sa));
	local_sa.sin_family = AF_INET;
	local_sa.sin_port = htons(0);
	local_sa.sin_addr.s_addr = nbr->laddr.s_addr;

	if (bind(nbr->fd, (struct sockaddr *) &local_sa,
	    sizeof(struct sockaddr_in)) == -1) {
		log_warn("%s: error while binding socket to %s", __func__,
		    inet_ntoa(local_sa.sin_addr));
		close(nbr->fd);
		return (-1);
	}

	bzero(&remote_sa, sizeof(remote_sa));
	remote_sa.sin_family = AF_INET;
	remote_sa.sin_port = htons(LDP_PORT);
	remote_sa.sin_addr.s_addr = nbr->raddr.s_addr;

	/*
	 * Send an extra hello to guarantee that the remote peer has formed
	 * an adjacency as well.
	 */
	LIST_FOREACH(adj, &nbr->adj_list, nbr_entry)
		send_hello(adj->source.type, adj->source.link.iface,
		    adj->source.target);

	if (connect(nbr->fd, (struct sockaddr *)&remote_sa,
	    sizeof(remote_sa)) == -1) {
		if (errno == EINPROGRESS) {
			event_set(&nbr->ev_connect, nbr->fd, EV_WRITE,
			    nbr_connect_cb, nbr);
			event_add(&nbr->ev_connect, NULL);
			return (0);
		}
		log_warn("%s: error while connecting to %s", __func__,
		    inet_ntoa(nbr->raddr));
		close(nbr->fd);
		return (-1);
	}

	/* connection completed immediately */
	nbr_fsm(nbr, NBR_EVT_CONNECT_UP);

	return (0);
}

int
nbr_act_session_operational(struct nbr *nbr)
{
	nbr->idtimer_cnt = 0;

	/* this is necessary to avoid ipc synchronization issues */
	nbr_update_peerid(nbr);

	return (ldpe_imsg_compose_lde(IMSG_NEIGHBOR_UP, nbr->peerid, 0,
	    &nbr->id, sizeof(nbr->id)));
}

void
nbr_send_labelmappings(struct nbr *nbr)
{
	ldpe_imsg_compose_lde(IMSG_LABEL_MAPPING_FULL, nbr->peerid, 0,
	    NULL, 0);
}

struct nbr_params *
nbr_params_new(struct in_addr addr)
{
	struct nbr_params	*nbrp;

	if ((nbrp = calloc(1, sizeof(*nbrp))) == NULL)
		fatal(__func__);

	nbrp->addr.s_addr = addr.s_addr;
	nbrp->auth.method = AUTH_NONE;

	return (nbrp);
}

struct nbr_params *
nbr_params_find(struct ldpd_conf *xconf, struct in_addr addr)
{
	struct nbr_params *nbrp;

	LIST_FOREACH(nbrp, &xconf->nbrp_list, entry)
		if (nbrp->addr.s_addr == addr.s_addr)
			return (nbrp);

	return (NULL);
}

uint16_t
nbr_get_keepalive(struct in_addr addr)
{
	struct nbr_params	*nbrp;

	nbrp = nbr_params_find(leconf, addr);
	if (nbrp && (nbrp->flags & F_NBRP_KEEPALIVE))
		return (nbrp->keepalive);

	return (leconf->keepalive);
}

struct ctl_nbr *
nbr_to_ctl(struct nbr *nbr)
{
	static struct ctl_nbr	 nctl;
	struct timeval		 now;

	memcpy(&nctl.id, &nbr->id, sizeof(nctl.id));
	memcpy(&nctl.addr, &nbr->raddr, sizeof(nctl.addr));
	nctl.nbr_state = nbr->state;

	gettimeofday(&now, NULL);
	if (nbr->state == NBR_STA_OPER) {
		nctl.uptime = now.tv_sec - nbr->uptime;
	} else
		nctl.uptime = 0;

	return (&nctl);
}
