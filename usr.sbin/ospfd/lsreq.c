/*	$OpenBSD: lsreq.c,v 1.4 2005/02/10 14:05:48 claudio Exp $ */

/*
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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>

#include "ospfd.h"
#include "ospf.h"
#include "log.h"
#include "ospfe.h"

extern struct imsgbuf		*ibuf_rde;

/* link state request packet handling */
int
send_ls_req(struct nbr *nbr)
{
	struct sockaddr_in	 dst;
	struct ls_req_hdr	*ls_req_hdr;
	struct lsa_entry	*le, *nle;
	char			*buf = NULL;
	char			*ptr;
	int			 ret = 0;

	log_debug("send_ls_req: neighbor ID %s", inet_ntoa(nbr->id));

	if (nbr->iface->passive)
		return (0);

	/* XXX use buffer API instead for better decoupling */
	if ((ptr = buf = calloc(1, READ_BUF_SIZE)) == NULL)
		fatal("send_ls_req");

	/* set destination */
	dst.sin_family = AF_INET;
	dst.sin_len = sizeof(struct sockaddr_in);

	switch (nbr->iface->type) {
	case IF_TYPE_POINTOPOINT:
		inet_aton(AllSPFRouters, &dst.sin_addr);
		break;
	case IF_TYPE_BROADCAST:
	case IF_TYPE_NBMA:
	case IF_TYPE_POINTOMULTIPOINT:
	case IF_TYPE_VIRTUALLINK:
		dst.sin_addr.s_addr = nbr->addr.s_addr;
		break;
	default:
		fatalx("send_ls_req: unknown interface type");
	}

	/* OSPF header */
	gen_ospf_hdr(ptr, nbr->iface, PACKET_TYPE_LS_REQUEST);
	ptr += sizeof(struct ospf_hdr);

	/* LSA header(s) */
	for (le = TAILQ_FIRST(&nbr->ls_req_list); le != NULL &&
	    (ptr - buf) < nbr->iface->mtu - PACKET_HDR; le = nle) {
		nbr->ls_req = nle = TAILQ_NEXT(le, entry);
		ls_req_hdr = (struct ls_req_hdr *)ptr;
		ls_req_hdr->type = htonl(le->le_lsa->type);
		ls_req_hdr->ls_id = le->le_lsa->ls_id;
		ls_req_hdr->adv_rtr = le->le_lsa->adv_rtr;
		ptr += sizeof(*ls_req_hdr);
	}

	/* update authentication and calculate checksum */
	auth_gen(buf, ptr - buf, nbr->iface);

	if ((ret = send_packet(nbr->iface, buf, (ptr - buf), &dst)) == -1)
		log_warnx("send_ls_req: error sending packet on "
		    "interface %s", nbr->iface->name);
	free(buf);
	return (ret);
}

void
recv_ls_req(struct nbr *nbr, char *buf, u_int16_t len)
{
	log_debug("recv_ls_req: neighbor ID %s", inet_ntoa(nbr->id));

	switch (nbr->state) {
	case NBR_STA_DOWN:
	case NBR_STA_ATTEMPT:
	case NBR_STA_INIT:
	case NBR_STA_2_WAY:
	case NBR_STA_XSTRT:
	case NBR_STA_SNAP:
		log_debug("recv_ls_req: packet ignored in state %s, "
		    "neighbor ID %s", nbr_state_name(nbr->state),
		    inet_ntoa(nbr->id));
		nbr_fsm(nbr, NBR_EVT_ADJ_OK);
		break;
	case NBR_STA_XCHNG:
	case NBR_STA_LOAD:
	case NBR_STA_FULL:
		imsg_compose(ibuf_rde, IMSG_LS_REQ, nbr->peerid, 0, -1,
		    buf, len);
		break;
	default:
		fatalx("recv_ls_req: unknown neighbor state");
	}
}

/* link state request list */
void
ls_req_list_add(struct nbr *nbr, struct lsa_hdr *lsa)
{
	struct lsa_entry	*le;

	if (lsa == NULL)
		fatalx("ls_req_list_add: no LSA header");

	if ((le = calloc(1, sizeof(*le))) == NULL)
		fatal("ls_req_list_add");

	TAILQ_INSERT_TAIL(&nbr->ls_req_list, le, entry);
	le->le_lsa = lsa;
	nbr->ls_req_cnt++;
}

struct lsa_entry *
ls_req_list_get(struct nbr *nbr, struct lsa_hdr *lsa_hdr)
{
	struct lsa_entry	*le;

	TAILQ_FOREACH(le, &nbr->ls_req_list, entry) {
		if ((lsa_hdr->type == le->le_lsa->type) &&
		    (lsa_hdr->ls_id == le->le_lsa->ls_id) &&
		    (lsa_hdr->adv_rtr == le->le_lsa->adv_rtr))
			return (le);
	}
	return (NULL);
}

void
ls_req_list_free(struct nbr *nbr, struct lsa_entry *le)
{
	if (nbr->ls_req == le) {
		nbr->ls_req = TAILQ_NEXT(le, entry);
	}

	TAILQ_REMOVE(&nbr->ls_req_list, le, entry);
	free(le->le_lsa);
	free(le);
	nbr->ls_req_cnt--;

	/* received all requested LSA(s), send a new LS req */
	if (nbr->ls_req != NULL &&
	    nbr->ls_req == TAILQ_FIRST(&nbr->ls_req_list)) {
		start_ls_req_tx_timer(nbr);
	}

	if (ls_req_list_empty(nbr) && nbr->dd_pending == 0)
		nbr_fsm(nbr, NBR_EVT_LOAD_DONE);
}

void
ls_req_list_clr(struct nbr *nbr)
{
	struct lsa_entry	*le;

	while ((le = TAILQ_FIRST(&nbr->ls_req_list)) != NULL) {
		TAILQ_REMOVE(&nbr->ls_req_list, le, entry);
		free(le->le_lsa);
		free(le);
	}

	nbr->ls_req_cnt = 0;
	nbr->ls_req = NULL;
}

bool
ls_req_list_empty(struct nbr *nbr)
{
	return (TAILQ_EMPTY(&nbr->ls_req_list));
}

/* timers */
void
ls_req_tx_timer(int fd, short event, void *arg)
{
	struct nbr *nbr = arg;
	struct timeval tv;

	log_debug("ls_req_tx_timer: neighbor ID %s", inet_ntoa(nbr->id));

	switch (nbr->state) {
	case NBR_STA_DOWN:
	case NBR_STA_ATTEMPT:
	case NBR_STA_INIT:
	case NBR_STA_2_WAY:
	case NBR_STA_SNAP:
	case NBR_STA_XSTRT:
	case NBR_STA_XCHNG:
		return;
	case NBR_STA_LOAD:
		send_ls_req(nbr);
		break;
	case NBR_STA_FULL:
		return;
	default:
		log_debug("ls_req_tx_timer: unknown neighbor state, "
		    "neighbor ID %s", inet_ntoa(nbr->id));
		break;
	}

	/* reschedule lsreq_tx_timer */
	if (nbr->state == NBR_STA_LOAD) {
		timerclear(&tv);
		tv.tv_sec = nbr->iface->rxmt_interval;
		log_debug("ls_req_tx_timer: reschedule neighbor ID %s",
		    inet_ntoa(nbr->id));
		evtimer_add(&nbr->lsreq_tx_timer, &tv);
	}
}

int
start_ls_req_tx_timer(struct nbr *nbr)
{
	struct timeval tv;

	if (nbr == nbr->iface->self)
		return (0);

	log_debug("start_ls_req_tx_timer: neighbor ID %s", inet_ntoa(nbr->id));
	timerclear(&tv);

	return (evtimer_add(&nbr->lsreq_tx_timer, &tv));
}

int
stop_ls_req_tx_timer(struct nbr *nbr)
{
	if (nbr == nbr->iface->self)
		return (0);

	log_debug("stop_ls_req_tx_timer: neighbor ID %s", inet_ntoa(nbr->id));

	return (evtimer_del(&nbr->lsreq_tx_timer));
}
