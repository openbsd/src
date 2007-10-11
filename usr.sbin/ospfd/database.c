/*	$OpenBSD: database.c,v 1.20 2007/10/11 08:21:29 claudio Exp $ */

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
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ospfd.h"
#include "ospf.h"
#include "log.h"
#include "ospfe.h"

extern struct ospfd_conf	*oeconf;

void	db_sum_list_next(struct nbr *);

/* database description packet handling */
int
send_db_description(struct nbr *nbr)
{
	struct sockaddr_in	 dst;
	struct db_dscrp_hdr	 dd_hdr;
	struct lsa_entry	*le, *nle;
	struct buf		*buf;
	int			 ret = 0;
	u_int8_t		 bits = 0;

	if ((buf = buf_open(nbr->iface->mtu - sizeof(struct ip))) == NULL)
		fatal("send_db_description");

	/* OSPF header */
	if (gen_ospf_hdr(buf, nbr->iface, PACKET_TYPE_DD))
		goto fail;

	/* reserve space for database description header */
	if (buf_reserve(buf, sizeof(dd_hdr)) == NULL)
		goto fail;

	switch (nbr->state) {
	case NBR_STA_DOWN:
	case NBR_STA_ATTEMPT:
	case NBR_STA_INIT:
	case NBR_STA_2_WAY:
	case NBR_STA_SNAP:
		log_debug("send_db_description: cannot send packet in state %s,"
		    " neighbor ID %s", nbr_state_name(nbr->state),
		    inet_ntoa(nbr->id));
		ret = -1;
		goto done;
	case NBR_STA_XSTRT:
		bits |= OSPF_DBD_MS | OSPF_DBD_M | OSPF_DBD_I;
		nbr->dd_more = 1;
		break;
	case NBR_STA_XCHNG:
		if (nbr->dd_master)
			bits |= OSPF_DBD_MS;
		else
			bits &= ~OSPF_DBD_MS;

		if (TAILQ_EMPTY(&nbr->db_sum_list)) {
			bits &= ~OSPF_DBD_M;
			nbr->dd_more = 0;
		} else {
			bits |= OSPF_DBD_M;
			nbr->dd_more = 1;
		}

		bits &= ~OSPF_DBD_I;

		/* build LSA list, keep space for a possible md5 sum */
		for (le = TAILQ_FIRST(&nbr->db_sum_list); le != NULL &&
		    buf->wpos + sizeof(struct lsa_hdr) < buf->max -
		    MD5_DIGEST_LENGTH; le = nle) {
			nbr->dd_end = nle = TAILQ_NEXT(le, entry);
			if (buf_add(buf, le->le_lsa, sizeof(struct lsa_hdr)))
				goto fail;
		}
		break;
	case NBR_STA_LOAD:
	case NBR_STA_FULL:
		if (nbr->dd_master)
			bits |= OSPF_DBD_MS;
		else
			bits &= ~OSPF_DBD_MS;
		bits &= ~OSPF_DBD_M;
		bits &= ~OSPF_DBD_I;

		nbr->dd_more = 0;
		break;
	default:
		fatalx("send_db_description: unknown neighbor state");
	}

	/* set destination */
	dst.sin_family = AF_INET;
	dst.sin_len = sizeof(struct sockaddr_in);

	switch (nbr->iface->type) {
	case IF_TYPE_POINTOPOINT:
		inet_aton(AllSPFRouters, &dst.sin_addr);
		dd_hdr.iface_mtu = htons(nbr->iface->mtu);
		break;
	case IF_TYPE_BROADCAST:
		dst.sin_addr = nbr->addr;
		dd_hdr.iface_mtu = htons(nbr->iface->mtu);
		break;
	case IF_TYPE_NBMA:
	case IF_TYPE_POINTOMULTIPOINT:
		/* XXX not supported */
		break;
	case IF_TYPE_VIRTUALLINK:
		dst.sin_addr = nbr->iface->dst;
		dd_hdr.iface_mtu = 0;
		break;
	default:
		fatalx("send_db_description: unknown interface type");
	}

	dd_hdr.opts = oeconf->options;
	dd_hdr.bits = bits;
	dd_hdr.dd_seq_num = htonl(nbr->dd_seq_num);

	memcpy(buf_seek(buf, sizeof(struct ospf_hdr), sizeof(dd_hdr)),
	    &dd_hdr, sizeof(dd_hdr));

	/* update authentication and calculate checksum */
	if (auth_gen(buf, nbr->iface))
		goto fail;

	/* transmit packet */
	ret = send_packet(nbr->iface, buf->buf, buf->wpos, &dst);
done:
	buf_free(buf);
	return (ret);
fail:
	log_warn("send_db_description");
	buf_free(buf);
	return (-1);
}

void
recv_db_description(struct nbr *nbr, char *buf, u_int16_t len)
{
	struct db_dscrp_hdr	 dd_hdr;
	int			 dupe = 0;

	if (len < sizeof(dd_hdr)) {
		log_warnx("recv_dd_description: "
		    "bad packet size, neighbor ID %s", inet_ntoa(nbr->id));
		return;
	}
	memcpy(&dd_hdr, buf, sizeof(dd_hdr));
	buf += sizeof(dd_hdr);
	len -= sizeof(dd_hdr);

	/* db description packet sanity checks */
	if (ntohs(dd_hdr.iface_mtu) > nbr->iface->mtu) {
		log_warnx("recv_dd_description: invalid MTU %d sent by "
		    "neighbor ID %s, expected %d", ntohs(dd_hdr.iface_mtu),
		    inet_ntoa(nbr->id), nbr->iface->mtu);
		return;
	}

	if (nbr->last_rx_options == dd_hdr.opts &&
	    nbr->last_rx_bits == dd_hdr.bits &&
	    ntohl(dd_hdr.dd_seq_num) == nbr->dd_seq_num - nbr->dd_master ?
	    1 : 0) {
			log_debug("recv_db_description: dupe");
			dupe = 1;
	}

	switch (nbr->state) {
	case NBR_STA_DOWN:
	case NBR_STA_ATTEMPT:
	case NBR_STA_2_WAY:
	case NBR_STA_SNAP:
		log_debug("recv_db_description: packet ignored in state %s, "
		    "neighbor ID %s", nbr_state_name(nbr->state),
		    inet_ntoa(nbr->id));
		return;
	case NBR_STA_INIT:
		/* evaluate dr and bdr after issuing a 2-Way event */
		nbr_fsm(nbr, NBR_EVT_2_WAY_RCVD);
		if_fsm(nbr->iface, IF_EVT_NBR_CHNG);
		if (nbr->state != NBR_STA_XSTRT)
			return;
		/* FALLTHROUGH */
	case NBR_STA_XSTRT:
		if (dupe)
			return;
		/*
		 * check bits: either I,M,MS or only M
		 */
		if (dd_hdr.bits == (OSPF_DBD_I | OSPF_DBD_M | OSPF_DBD_MS)) {
			/* if nbr Router ID is larger than own -> slave */
			if ((ntohl(nbr->id.s_addr)) >
			    ntohl(ospfe_router_id())) {
				/* slave */
				nbr->dd_master = 0;
				nbr->dd_seq_num = ntohl(dd_hdr.dd_seq_num);

				/* event negotiation done */
				nbr_fsm(nbr, NBR_EVT_NEG_DONE);
			}
		} else if (!(dd_hdr.bits & (OSPF_DBD_I | OSPF_DBD_MS))) {
			/* M only case: we are master */
			if (ntohl(dd_hdr.dd_seq_num) != nbr->dd_seq_num) {
				log_warnx("recv_db_description: invalid "
				    "seq num, mine %x his %x",
				    nbr->dd_seq_num, ntohl(dd_hdr.dd_seq_num));
				nbr_fsm(nbr, NBR_EVT_SEQ_NUM_MIS);
				return;
			}
			nbr->dd_seq_num++;

			/* this packet may already have data so pass it on */
			if (len > 0) {
				nbr->dd_pending++;
				ospfe_imsg_compose_rde(IMSG_DD, nbr->peerid,
				    0, buf, len);
			}

			/* event negotiation done */
			nbr_fsm(nbr, NBR_EVT_NEG_DONE);

		} else {
			/* ignore packet */
			log_debug("recv_db_description: packet ignored in "
			    "state %s (bad flags), neighbor ID %s",
			    nbr_state_name(nbr->state), inet_ntoa(nbr->id));
		}
		break;
	case NBR_STA_XCHNG:
	case NBR_STA_LOAD:
	case NBR_STA_FULL:
		if (dd_hdr.bits & OSPF_DBD_I ||
		    !(dd_hdr.bits & OSPF_DBD_MS) == !nbr->dd_master) {
			log_warnx("recv_db_description: seq num mismatch, "
			    "bad flags");
			nbr_fsm(nbr, NBR_EVT_SEQ_NUM_MIS);
			return;
		}

		if (nbr->last_rx_options != dd_hdr.opts) {
			log_warnx("recv_db_description: seq num mismatch, "
			    "bad options");
			nbr_fsm(nbr, NBR_EVT_SEQ_NUM_MIS);
			return;
		}

		if (dupe) {
			if (!nbr->dd_master)
				/* retransmit */
				start_db_tx_timer(nbr);
			return;
		}

		if (nbr->state != NBR_STA_XCHNG) {
			log_warnx("recv_db_description: invalid "
			    "seq num, mine %x his %x",
			    nbr->dd_seq_num, ntohl(dd_hdr.dd_seq_num));
			nbr_fsm(nbr, NBR_EVT_SEQ_NUM_MIS);
			return;
		}

		/* sanity check dd seq number */
		if (nbr->dd_master) {
			/* master */
			if (ntohl(dd_hdr.dd_seq_num) != nbr->dd_seq_num) {
				log_warnx("recv_db_description: invalid "
				    "seq num, mine %x his %x",
				    nbr->dd_seq_num, ntohl(dd_hdr.dd_seq_num));
				nbr_fsm(nbr, NBR_EVT_SEQ_NUM_MIS);
				return;
			}
			nbr->dd_seq_num++;
		} else {
			/* slave */
			if (ntohl(dd_hdr.dd_seq_num) != nbr->dd_seq_num + 1) {
				log_warnx("recv_db_description: invalid "
				    "seq num, mine %x his %x",
				    nbr->dd_seq_num, ntohl(dd_hdr.dd_seq_num));
				nbr_fsm(nbr, NBR_EVT_SEQ_NUM_MIS);
				return;
			}
			nbr->dd_seq_num = ntohl(dd_hdr.dd_seq_num);
		}

		/* forward to RDE and let it decide which LSAs to request */
		if (len > 0) {
			nbr->dd_pending++;
			ospfe_imsg_compose_rde(IMSG_DD, nbr->peerid, 0,
			    buf, len);
		}

		/* next packet */
		db_sum_list_next(nbr);
		start_db_tx_timer(nbr);

		if (!(dd_hdr.bits & OSPF_DBD_M) &&
		    TAILQ_EMPTY(&nbr->db_sum_list))
			if (!nbr->dd_master || !nbr->dd_more)
				nbr_fsm(nbr, NBR_EVT_XCHNG_DONE);
		break;
	default:
		fatalx("recv_db_description: unknown neighbor state");
	}

	nbr->last_rx_options = dd_hdr.opts;
	nbr->last_rx_bits = dd_hdr.bits;
}

void
db_sum_list_add(struct nbr *nbr, struct lsa_hdr *lsa)
{
	struct lsa_entry	*le;

	if ((le = calloc(1, sizeof(*le))) == NULL)
		fatal("db_sum_list_add");

	TAILQ_INSERT_TAIL(&nbr->db_sum_list, le, entry);
	le->le_lsa = lsa;
}

void
db_sum_list_next(struct nbr *nbr)
{
	struct lsa_entry	*le;

	while ((le = TAILQ_FIRST(&nbr->db_sum_list)) != nbr->dd_end) {
		TAILQ_REMOVE(&nbr->db_sum_list, le, entry);
		free(le->le_lsa);
		free(le);
	}
}

void
db_sum_list_clr(struct nbr *nbr)
{
	nbr->dd_end = NULL;
	db_sum_list_next(nbr);
}

/* timers */
/* ARGSUSED */
void
db_tx_timer(int fd, short event, void *arg)
{
	struct nbr *nbr = arg;
	struct timeval tv;

	switch (nbr->state) {
	case NBR_STA_DOWN:
	case NBR_STA_ATTEMPT:
	case NBR_STA_INIT:
	case NBR_STA_2_WAY:
	case NBR_STA_SNAP:
		return ;
	case NBR_STA_XSTRT:
	case NBR_STA_XCHNG:
	case NBR_STA_LOAD:
	case NBR_STA_FULL:
		send_db_description(nbr);
		break;
	default:
		log_debug("db_tx_timer: unknown neighbor state, "
		    "neighbor ID %s", inet_ntoa(nbr->id));
		break;
	}

	/* reschedule db_tx_timer but only in master mode */
	if (nbr->dd_master) {
		timerclear(&tv);
		tv.tv_sec = nbr->iface->rxmt_interval;
		if (evtimer_add(&nbr->db_tx_timer, &tv) == -1)
			fatal("db_tx_timer");
	}
}

void
start_db_tx_timer(struct nbr *nbr)
{
	struct timeval	tv;

	if (nbr == nbr->iface->self)
		return;

	timerclear(&tv);
	if (evtimer_add(&nbr->db_tx_timer, &tv) == -1)
		fatal("start_db_tx_timer");
}

void
stop_db_tx_timer(struct nbr *nbr)
{
	if (nbr == nbr->iface->self)
		return;

	if (evtimer_del(&nbr->db_tx_timer) == -1)
		fatal("stop_db_tx_timer");
}
