/*	$OpenBSD: lsack.c,v 1.4 2005/02/02 19:29:15 henning Exp $ */

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
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <arpa/inet.h>

#include <stdlib.h>
#include <strings.h>

#include "ospfd.h"
#include "ospf.h"
#include "log.h"
#include "ospfe.h"

int	 start_ls_ack_tx_timer_now(struct iface *);

/* link state acknowledgement packet handling */
int
send_ls_ack(struct iface *iface, struct in_addr addr, void *data, int len)
{
	struct sockaddr_in	 dst;
	char			*buf;
	char			*ptr;
	int			 ret = 0;

	log_debug("send_ls_ack: interface %s addr %s",
	    iface->name, inet_ntoa(addr));

	if (iface->passive)
		return (0);

	/* XXX use buffer API instead for better decoupling */
	if ((ptr = buf = calloc(1, READ_BUF_SIZE)) == NULL)
		fatal("send_ls_ack");

	dst.sin_family = AF_INET;
	dst.sin_len = sizeof(struct sockaddr_in);
	dst.sin_addr.s_addr = addr.s_addr;

	/* OSPF header */
	gen_ospf_hdr(ptr, iface, PACKET_TYPE_LS_ACK);
	ptr += sizeof(struct ospf_hdr);

	/* LS ack(s) */
	memcpy(ptr, data, len);		/* XXX size check ??? */
	ptr += len;

	/* update authentication and calculate checksum */
	auth_gen(buf, ptr - buf, iface);

	if ((ret = send_packet(iface, buf, (ptr - buf), &dst)) == -1)
		log_warnx("send_ls_ack: error sending packet on "
		    "interface %s", iface->name);

	free(buf);
	return (ret);
}

void
recv_ls_ack(struct nbr *nbr, char *buf, u_int16_t len)
{
	struct lsa_hdr	 lsa_hdr;

	log_debug("recv_ls_ack: neighbor ID %s", inet_ntoa(nbr->id));

	switch (nbr->state) {
	case NBR_STA_DOWN:
	case NBR_STA_ATTEMPT:
	case NBR_STA_INIT:
	case NBR_STA_2_WAY:
	case NBR_STA_XSTRT:
	case NBR_STA_SNAP:
		log_debug("recv_ls_ack: packet ignored in state %s, "
		    "neighbor ID %s", nbr_state_name(nbr->state),
		    inet_ntoa(nbr->id));
		break;
	case NBR_STA_XCHNG:
	case NBR_STA_LOAD:
	case NBR_STA_FULL:
		log_debug("recv_ls_ack: state %s, neighbor ID %s",
		    nbr_state_name(nbr->state), inet_ntoa(nbr->id));

		while (len >= sizeof(lsa_hdr)) {
			memcpy(&lsa_hdr, buf, sizeof(lsa_hdr));

			if (lsa_hdr_check(nbr, &lsa_hdr))
				ls_retrans_list_del(nbr, &lsa_hdr);

			buf += sizeof(lsa_hdr);
			len -= sizeof(lsa_hdr);
		}
		if (len > 0) {
			log_warnx("recv_ls_ack: bad packet size, "
			    "neighbor ID %s", inet_ntoa(nbr->id));
			return;
		}
		break;
	default:
		fatalx("recv_ls_ack: unknown neighbor state");
	}

	return;
}

int
lsa_hdr_check(struct nbr *nbr, struct lsa_hdr *lsa_hdr)
{
	/* invalid age */
	if ((ntohs(lsa_hdr->age) < 1) || (ntohs(lsa_hdr->age) > MAX_AGE)) {
		log_debug("lsa_hdr_check: invalid age, neighbor ID %s",
		     inet_ntoa(nbr->id));
		return (0);
	}

	/* invalid type */
	switch (lsa_hdr->type) {
	case LSA_TYPE_ROUTER:
	case LSA_TYPE_NETWORK:
	case LSA_TYPE_SUM_NETWORK:
	case LSA_TYPE_SUM_ROUTER:
	case LSA_TYPE_EXTERNAL:
		break;
	default:
		log_debug("lsa_hdr_check: invalid LSA type %d, neighbor ID %s",
		    lsa_hdr->type, inet_ntoa(nbr->id));
		return (0);
	}

	/* invalid sequence number */
	if (ntohl(lsa_hdr->seq_num) == 0x80000000) {
		log_debug("ls_hdr_check: invalid seq num, neighbor ID %s",
			inet_ntoa(nbr->id));
		return (0);
	}

	return (1);
}

/* link state ack list */
void
ls_ack_list_add(struct iface *iface, struct lsa_hdr *lsa)
{
	struct lsa_entry	*le;

	if (lsa == NULL)
		fatalx("ls_ack_list_add: no LSA header");

	if ((le = calloc(1, sizeof(*le))) == NULL)
		fatal("ls_ack_list_add");

	if (ls_ack_list_empty(iface))
		start_ls_ack_tx_timer(iface);

	TAILQ_INSERT_TAIL(&iface->ls_ack_list, le, entry);
	le->le_lsa = lsa;
	iface->ls_ack_cnt++;

	/* reschedule now if we have enough for a full packet */
	if (iface->ls_ack_cnt >
	    ((iface->mtu - PACKET_HDR) / sizeof(struct lsa_hdr))) {
		start_ls_ack_tx_timer_now(iface);
	}

}

void
ls_ack_list_free(struct iface *iface, struct lsa_entry *le)
{
	TAILQ_REMOVE(&iface->ls_ack_list, le, entry);
	free(le->le_lsa);
	free(le);

	iface->ls_ack_cnt--;
}

void
ls_ack_list_clr(struct iface *iface)
{
	struct lsa_entry	*le;

	while ((le = TAILQ_FIRST(&iface->ls_ack_list)) != NULL) {
		TAILQ_REMOVE(&iface->ls_ack_list, le, entry);
		free(le->le_lsa);
		free(le);
	}
	iface->ls_ack_cnt = 0;
}

bool
ls_ack_list_empty(struct iface *iface)
{
	return (TAILQ_EMPTY(&iface->ls_ack_list));
}

/* timers */
void
ls_ack_tx_timer(int fd, short event, void *arg)
{
	struct in_addr		 addr;
	struct iface		*iface = arg;
	struct lsa_hdr		*lsa_hdr;
	struct lsa_entry	*le, *nle;
	struct nbr		*nbr;
	char			*buf;
	char			*ptr;
	int			 cnt = 0;

	if ((buf = calloc(1, READ_BUF_SIZE)) == NULL)
		fatal("ls_ack_tx_timer");

	while (!ls_ack_list_empty(iface)) {
		ptr = buf;
		cnt = 0;
		for (le = TAILQ_FIRST(&iface->ls_ack_list); (le != NULL) &&
		    ((ptr - buf) < iface->mtu - PACKET_HDR); le = nle) {
			nle = TAILQ_NEXT(le, entry);
			memcpy(ptr, le->le_lsa, sizeof(struct lsa_hdr));
			ptr += sizeof(*lsa_hdr);
			ls_ack_list_free(iface, le);
			cnt++;
		}

		log_debug("ls_ack_tx_timer: sending %d ack(s), interface %s",
		    cnt, iface->name);

		/* send LS ack(s) but first set correct destination */
		switch (iface->type) {
		case IF_TYPE_POINTOPOINT:
			inet_aton(AllSPFRouters, &addr);
			send_ls_ack(iface, addr, buf, ptr - buf);
			break;
		case IF_TYPE_BROADCAST:
			if (iface->state & IF_STA_DRORBDR)
				inet_aton(AllSPFRouters, &addr);
			else
				inet_aton(AllDRouters, &addr);
			send_ls_ack(iface, addr, buf, ptr - buf);
			break;
		case IF_TYPE_NBMA:
		case IF_TYPE_POINTOMULTIPOINT:
		case IF_TYPE_VIRTUALLINK:
			LIST_FOREACH(nbr, &iface->nbr_list, entry) {
				if (nbr == iface->self)
					continue;
				if (!(nbr->state & NBR_STA_FLOOD))
					continue;
				send_ls_ack(iface, nbr->addr, ptr, ptr - buf);
			}
			break;
		default:
			fatalx("lsa_flood: unknown interface type");
		}
	}

	free(buf);
}

int
start_ls_ack_tx_timer(struct iface *iface)
{
	struct timeval tv;

	log_debug("start_ls_ack_tx_timer: interface %s", iface->name);
	timerclear(&tv);
	tv.tv_sec = iface->rxmt_interval / 2;
	return (evtimer_add(&iface->lsack_tx_timer, &tv));
}

int
start_ls_ack_tx_timer_now(struct iface *iface)
{
	struct timeval tv;

	log_debug("start_ls_ack_tx_timer_now: interface %s", iface->name);
	timerclear(&tv);

	return (evtimer_add(&iface->lsack_tx_timer, &tv));
}

int
stop_ls_ack_tx_timer(struct iface *iface)
{
	log_debug("stop_ls_ack_tx_timer: interface %s", iface->name);

	return (evtimer_del(&iface->lsack_tx_timer));
}
