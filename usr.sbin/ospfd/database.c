/*	$OpenBSD: database.c,v 1.5 2005/02/10 14:05:48 claudio Exp $ */

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
	struct db_dscrp_hdr	*dd_hdr;
	struct lsa_hdr		*lsa_hdr;
	struct lsa_entry	*le, *nle;
	char			*buf;
	char			*ptr;
	int			 ret = 0;

	log_debug("send_db_description: neighbor ID %s, seq_num %x",
	    inet_ntoa(nbr->id), nbr->dd_seq_num);

	if (nbr->iface->passive)
		return (0);

	if ((ptr = buf = calloc(1, READ_BUF_SIZE)) == NULL)
		fatal("send_db_description");

	/* OSPF header */
	gen_ospf_hdr(ptr, nbr->iface, PACKET_TYPE_DD);
	ptr += sizeof(struct ospf_hdr);

	/* database description header */
	dd_hdr = (struct db_dscrp_hdr *)ptr;
	dd_hdr->opts = oeconf->options;
	dd_hdr->dd_seq_num = htonl(nbr->dd_seq_num);

	ptr += sizeof(*dd_hdr);

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
		log_debug("send_db_description: state %s, neighbor ID %s",
		    nbr_state_name(nbr->state), inet_ntoa(nbr->id));

		nbr->options |= OSPF_DBD_MS | OSPF_DBD_M | OSPF_DBD_I;
		break;
	case NBR_STA_XCHNG:
		log_debug("send_db_description: state %s, neighbor ID %s",
		    nbr_state_name(nbr->state), inet_ntoa(nbr->id));

		if (nbr->master) {
			/* master */
			nbr->options |= OSPF_DBD_MS;
		} else {
			/* slave */
			nbr->options &= ~OSPF_DBD_MS;
		}

		if (TAILQ_EMPTY(&nbr->db_sum_list))
			nbr->options &= ~OSPF_DBD_M;
		else
			nbr->options |= OSPF_DBD_M;

		nbr->options &= ~OSPF_DBD_I;

		/* build LSA list */
		lsa_hdr = (struct lsa_hdr *)ptr;

		for (le = TAILQ_FIRST(&nbr->db_sum_list); (le != NULL) &&
		    ((ptr - buf) < nbr->iface->mtu - PACKET_HDR); le = nle) {
			nbr->dd_end = nle = TAILQ_NEXT(le, entry);
			memcpy(ptr, le->le_lsa, sizeof(struct lsa_hdr));
			ptr += sizeof(*lsa_hdr);
		}
		break;
	case NBR_STA_LOAD:
	case NBR_STA_FULL:
		log_debug("send_db_description: state %s, neighbor ID %s",
		    nbr_state_name(nbr->state), inet_ntoa(nbr->id));

		if (nbr->master) {
			/* master */
			nbr->options |= OSPF_DBD_MS;
		} else {
			/* slave */
			nbr->options &= ~OSPF_DBD_MS;
		}
		nbr->options &= ~OSPF_DBD_M;
		nbr->options &= ~OSPF_DBD_I;

		break;
	default:
		log_debug("send_db_description: unknown neighbor state, "
		    "neighbor ID %s", inet_ntoa(nbr->id));
		ret = -1;
		goto done;
		break;
	}

	/* set destination */
	dst.sin_family = AF_INET;
	dst.sin_len = sizeof(struct sockaddr_in);

	switch (nbr->iface->type) {
	case IF_TYPE_POINTOPOINT:
		inet_aton(AllSPFRouters, &dst.sin_addr);
		dd_hdr->iface_mtu = htons(nbr->iface->mtu);
		break;
	case IF_TYPE_BROADCAST:
		dst.sin_addr = nbr->addr;
		dd_hdr->iface_mtu = htons(nbr->iface->mtu);
		break;
	case IF_TYPE_NBMA:
	case IF_TYPE_POINTOMULTIPOINT:
	case IF_TYPE_VIRTUALLINK:
		dst.sin_addr = nbr->addr;
		dd_hdr->iface_mtu = 0;
		break;
	default:
		fatalx("send_db_description: unknown interface type");
	}

	dd_hdr->bits = nbr->options;

	/* update authentication and calculate checksum */
	auth_gen(buf, ptr - buf, nbr->iface);

	/* transmit packet */
	if ((ret = send_packet(nbr->iface, buf, (ptr - buf), &dst)) == -1)
		log_warnx("send_db_description: error sending packet on "
		    "interface %s", nbr->iface->name);

done:
	free(buf);

	return (ret);
}

void
recv_db_description(struct nbr *nbr, char *buf, u_int16_t len)
{
	struct db_dscrp_hdr	 dd_hdr;
	int			 dupe = 0;

	log_debug("recv_db_description: neighbor ID %s, seq_num %x",
	    inet_ntoa(nbr->id), nbr->dd_seq_num);

	if (len < sizeof(dd_hdr)) {
		log_warnx("recv_dd_description: "
		    "bad packet size, neighbor ID %s", inet_ntoa(nbr->id));
		return;
	}
	memcpy(&dd_hdr, buf, sizeof(dd_hdr));
	buf += sizeof(dd_hdr);
	len -= sizeof(dd_hdr);


	/* db description packet sanity checks */
	if (ntohs(dd_hdr.iface_mtu) != nbr->iface->mtu) {
		log_warnx("recv_dd_description: invalid MTU, neighbor ID %s",
		    inet_ntoa(nbr->id));
		return;
	}

	if (nbr->last_rx_options == dd_hdr.opts &&
	    nbr->last_rx_bits == dd_hdr.bits &&
	    ntohl(dd_hdr.dd_seq_num) == nbr->dd_seq_num - nbr->master ? 1 : 0) {
			log_debug("recv_db_description: dupe");
			dupe = 1;
	}

	log_debug("recv_db_description: seq_num %x", ntohl(dd_hdr.dd_seq_num));
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
		log_debug("recv_db_description: state %s, neighbor ID %s",
		    nbr_state_name(nbr->state), inet_ntoa(nbr->id));

		/* evaluate dr and bdr before issuing a 2-Way event */
		if_fsm(nbr->iface, IF_EVT_NBR_CHNG);
		nbr_fsm(nbr, NBR_EVT_2_WAY_RCVD);
		if (nbr->state != NBR_STA_XSTRT)
			return;
		/* FALLTHROUGH */
	case NBR_STA_XSTRT:
		log_debug("recv_db_description: state %s, neighbor ID %s",
		    nbr_state_name(nbr->state), inet_ntoa(nbr->id));

		if (dupe)
			return;
		/*
		 * check bits: either I,M,MS or only M
		 * I,M,MS is checked here the M only case is a fall-through
		 */
		if (dd_hdr.bits == (OSPF_DBD_I | OSPF_DBD_M | OSPF_DBD_MS)) {
			/* if nbr Router ID is larger than own -> slave */
			if ((ntohl(nbr->id.s_addr)) >
			    ntohl(nbr->iface->rtr_id.s_addr)) {
				/* slave */
				log_debug("recv_db_description: slave, "
				    "neighbor ID %s", inet_ntoa(nbr->id));
				nbr->master = false;
				nbr->dd_seq_num = ntohl(dd_hdr.dd_seq_num);

				/* event negotiation done */
				nbr_fsm(nbr, NBR_EVT_NEG_DONE);
			}
		} else if (!(dd_hdr.bits & (OSPF_DBD_I | OSPF_DBD_MS))) {
			/* master */
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
		log_debug("recv_db_description: state %s, neighbor ID %s",
		    nbr_state_name(nbr->state), inet_ntoa(nbr->id));

		if (dd_hdr.bits & OSPF_DBD_I ||
		    !(dd_hdr.bits & OSPF_DBD_MS) == !nbr->master) {
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
			if (!nbr->master)
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
		if (nbr->master) {
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

		/* forward to RDE and let it decide which LSA's to request */
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
			if (!nbr->master || !(nbr->options & OSPF_DBD_M))
				nbr_fsm(nbr, NBR_EVT_XCHNG_DONE);
		break;
	default:
		fatalx("recv_db_description: unknown neighbor state");
	}

	nbr->last_rx_options = dd_hdr.opts;
	nbr->last_rx_bits = dd_hdr.bits;
	return;
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
void
db_tx_timer(int fd, short event, void *arg)
{
	struct nbr *nbr = arg;
	struct timeval tv;

	log_debug("db_tx_timer: neighbor ID %s", inet_ntoa(nbr->id));

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
	if (nbr->master) {
		timerclear(&tv);
		tv.tv_sec = nbr->iface->rxmt_interval;
		log_debug("db_tx_timer: reschedule neighbor ID %s",
		    inet_ntoa(nbr->id));
		evtimer_add(&nbr->db_tx_timer, &tv);
	}
}

int
start_db_tx_timer(struct nbr *nbr)
{
	struct timeval	tv;

	if (nbr == nbr->iface->self)
		return (0);

	log_debug("start_db_tx_timer: neighbor ID %s", inet_ntoa(nbr->id));

	timerclear(&tv);

	return (evtimer_add(&nbr->db_tx_timer, &tv));
}

int
stop_db_tx_timer(struct nbr *nbr)
{
	if (nbr == nbr->iface->self)
		return (0);

	log_debug("stop_db_tx_timer: neighbor ID %s", inet_ntoa(nbr->id));

	return (evtimer_del(&nbr->db_tx_timer));
}
