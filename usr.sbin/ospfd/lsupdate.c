/*	$OpenBSD: lsupdate.c,v 1.3 2005/02/02 19:15:07 henning Exp $ */

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
#include <strings.h>

#include "ospf.h"
#include "ospfd.h"
#include "log.h"
#include "ospfe.h"
#include "rde.h"

extern struct ospfd_conf	*oeconf;
extern struct imsgbuf		*ibuf_rde;

/* link state update packet handling */
int
lsa_flood(struct iface *iface, struct nbr *originator, struct lsa_hdr *lsa_hdr,
    void *data, u_int16_t len)
{
	struct in_addr		 addr;
	struct nbr		*nbr;
	struct lsa_entry	*le;
	int			 queued = 0, dont_ack = 0;
	int			 r;

	if (iface->passive)
		return (0);

	LIST_FOREACH(nbr, &iface->nbr_list, entry) {
		if (nbr == iface->self)
			continue;
		if (!(nbr->state & NBR_STA_FLOOD))
			continue;

		le = ls_retrans_list_get(nbr, lsa_hdr);
		if (le)
			ls_retrans_list_free(nbr, le);

		if (!(nbr->state & NBR_STA_FULL) &&
		    (le = ls_req_list_get(nbr, lsa_hdr)) != NULL) {
			r = lsa_newer(lsa_hdr, le->le_lsa);
			if (r > 0) {
				/* to flood LSA is newer than requested */
				ls_req_list_free(nbr, le);
				/* new needs to be flooded */
			} else if (r < 0) {
				/* to flood LSA is older than requested */
				continue;
			} else {
				/* LSA are equal */
				ls_req_list_free(nbr, le);
				continue;
			}
		}

		if (nbr == originator) {
			dont_ack++;
			continue;
		}

		ls_retrans_list_add(nbr, data);	/* XXX error handling */
		queued = 1;
	}
	if (!queued)
		return (0);

	if (iface == originator->iface) {
		if (iface->dr == originator || iface->bdr == originator)
			return (0);
		if (iface->state & IF_STA_BACKUP)
			return (0);
		dont_ack++;
	}

	/* flood LSA but first set correct destination */
	switch (iface->type) {
	case IF_TYPE_POINTOPOINT:
		inet_aton(AllSPFRouters, &addr);
		send_ls_update(iface, addr, data, len);
		break;
	case IF_TYPE_BROADCAST:
		if (iface->state & IF_STA_DRORBDR)
			inet_aton(AllSPFRouters, &addr);
		else
			inet_aton(AllDRouters, &addr);
		send_ls_update(iface, addr, data, len);
		break;
	case IF_TYPE_NBMA:
	case IF_TYPE_POINTOMULTIPOINT:
	case IF_TYPE_VIRTUALLINK:
		LIST_FOREACH(nbr, &iface->nbr_list, entry) {
			if (nbr == iface->self)
				continue;
			if (!(nbr->state & NBR_STA_FLOOD))
				continue;
			if (!TAILQ_EMPTY(&nbr->ls_retrans_list)) {
				le = TAILQ_LAST(&nbr->ls_retrans_list,
				    lsa_head);
				if (lsa_hdr->type != le->le_lsa->type ||
				    lsa_hdr->ls_id != le->le_lsa->ls_id ||
				    lsa_hdr->adv_rtr != le->le_lsa->adv_rtr)
					continue;
			}
			send_ls_update(iface, nbr->addr, data, len);
		}
		break;
	default:
		fatalx("lsa_flood: unknown interface type");
	}


	return (dont_ack == 2);
}

int
send_ls_update(struct iface *iface, struct in_addr addr, void *data, int len)
{
	struct sockaddr_in	 dst;
	char			*buf;
	char			*ptr;
	u_int32_t		 nlsa;
	u_int16_t		 age;
	int			 ret = 0;

	log_debug("send_ls_update: interface %s addr %s",
	    iface->name, inet_ntoa(addr));

	if (iface->passive)
		return (0);

	/* XXX use buffer API instead for better decoupling */
	if ((ptr = buf = calloc(1, READ_BUF_SIZE)) == NULL)
		fatal("send_ls_update");

	/* set destination */
	dst.sin_family = AF_INET;
	dst.sin_len = sizeof(struct sockaddr_in);
	dst.sin_addr.s_addr = addr.s_addr;

	/* OSPF header */
	gen_ospf_hdr(ptr, iface, PACKET_TYPE_LS_UPDATE);
	ptr += sizeof(struct ospf_hdr);

	nlsa = htonl(1);
	memcpy(ptr, &nlsa, sizeof(nlsa));
	ptr += sizeof(nlsa);

	memcpy(ptr, data, len);		/* XXX */

	/* age LSA befor sending it out */
	memcpy(&age, ptr, sizeof(age));
	age = ntohs(age);
	if ((age += iface->transfer_delay) >= MAX_AGE) age = MAX_AGE;
	age = ntohs(age);
	memcpy(ptr, &age, sizeof(age));

	ptr += len;

	/* update authentication and calculate checksum */
	auth_gen(buf, ptr - buf, iface);

	if ((ret = send_packet(iface, buf, (ptr - buf), &dst)) == -1)
		log_warnx("send_ls_update: error sending packet on "
		    "interface %s", iface->name);

	free(buf);
	return (ret);
}

void
recv_ls_update(struct nbr *nbr, char *buf, u_int16_t len)
{
	struct lsa_hdr		 lsa;
	u_int32_t		 nlsa;

	log_debug("recv_ls_update: neighbor ID %s", inet_ntoa(nbr->id));

	if (len < sizeof(nlsa)) {
		log_warnx("recv_ls_update: bad packet size, neighbor ID %s",
		    inet_ntoa(nbr->id));
		return;
	}
	memcpy(&nlsa, buf, sizeof(nlsa));
	nlsa = ntohl(nlsa);
	buf += sizeof(nlsa);
	len -= sizeof(nlsa);

	switch (nbr->state) {
	case NBR_STA_DOWN:
	case NBR_STA_ATTEMPT:
	case NBR_STA_INIT:
	case NBR_STA_2_WAY:
	case NBR_STA_XSTRT:
	case NBR_STA_SNAP:
		log_debug("recv_ls_update: packet ignored in state %s, "
		    "neighbor ID %s", nbr_state_name(nbr->state),
		    inet_ntoa(nbr->id));
		break;
	case NBR_STA_XCHNG:
	case NBR_STA_LOAD:
	case NBR_STA_FULL:
		for (; nlsa > 0 && len > 0; nlsa--) {
			if (len < sizeof(lsa)) {
				log_warnx("recv_ls_update: bad packet size, "
				    "neighbor ID %s", inet_ntoa(nbr->id));
				return;
			}
			memcpy(&lsa, buf, sizeof(lsa));
			if (len < ntohs(lsa.len)) {
				log_warnx("recv_ls_update: bad packet size, "
				    "neighbor ID %s", inet_ntoa(nbr->id));
				return;
			}
			imsg_compose(ibuf_rde, IMSG_LS_UPD, nbr->peerid, 0, -1,
			    buf, ntohs(lsa.len));
			buf += ntohs(lsa.len);
			len -= ntohs(lsa.len);
		}
		if (nlsa > 0 || len > 0) {
			log_warnx("recv_ls_update: bad packet size, "
			    "neighbor ID %s", inet_ntoa(nbr->id));
			return;
		}
		break;
	default:
		fatalx("recv_ls_update: unknown neighbor state");
	}

	return;
}

/* link state retransmit list */
int
ls_retrans_list_add(struct nbr *nbr, struct lsa_hdr *lsa)
{
	struct lsa_entry	*le;
	struct lsa_ref		*ref;

	ref = lsa_cache_get(lsa);
	if (ref == NULL)
		return (1);

	if ((le = calloc(1, sizeof(*le))) == NULL)
		fatal("ls_retrans_list_add");

	le->le_ref = ref;
	TAILQ_INSERT_TAIL(&nbr->ls_retrans_list, le, entry);

	return (0);
}

int
ls_retrans_list_del(struct nbr *nbr, struct lsa_hdr *lsa_hdr)
{
	struct lsa_entry	*le;

	le = ls_retrans_list_get(nbr, lsa_hdr);
	if (le == NULL)
		return (1);
	if (lsa_hdr->seq_num == le->le_ref->hdr.seq_num &&
	    lsa_hdr->ls_chksum == le->le_ref->hdr.ls_chksum) {
		ls_retrans_list_free(nbr, le);
		return (0);
	}

	log_warnx("ls_retrans_list_del: invalid LS ack received, neigbor %s",
	     inet_ntoa(nbr->id));

	return (1);
}

struct lsa_entry *
ls_retrans_list_get(struct nbr *nbr, struct lsa_hdr *lsa_hdr)
{
	struct lsa_entry	*le;

	TAILQ_FOREACH(le, &nbr->ls_retrans_list, entry) {
		if ((lsa_hdr->type == le->le_ref->hdr.type) &&
		    (lsa_hdr->ls_id == le->le_ref->hdr.ls_id) &&
		    (lsa_hdr->adv_rtr == le->le_ref->hdr.adv_rtr))
			return (le);
	}
	return (NULL);
}

void
ls_retrans_list_free(struct nbr *nbr, struct lsa_entry *le)
{
	TAILQ_REMOVE(&nbr->ls_retrans_list, le, entry);

	lsa_cache_put(le->le_ref, nbr);
	free(le);
}

void
ls_retrans_list_clr(struct nbr *nbr)
{
	struct lsa_entry	*le;

	while ((le = TAILQ_FIRST(&nbr->ls_retrans_list)) != NULL)
		ls_retrans_list_free(nbr, le);
}

bool
ls_retrans_list_empty(struct nbr *nbr)
{
	return (TAILQ_EMPTY(&nbr->ls_retrans_list));
}

LIST_HEAD(lsa_cache_head, lsa_ref);

struct lsa_cache {
	struct lsa_cache_head	*hashtbl;
	u_int32_t		 hashmask;
} lsacache;

struct lsa_ref		*lsa_cache_look(struct lsa_hdr *);
struct lsa_cache_head	*lsa_cache_hash(struct lsa_hdr *);

void
lsa_cache_init(u_int32_t hashsize)
{
	u_int32_t        hs, i;

	for (hs = 1; hs < hashsize; hs <<= 1)
		;
	lsacache.hashtbl = calloc(hs, sizeof(struct lsa_cache_head));
	if (lsacache.hashtbl == NULL)
		fatal("lsa_cache_init");

	for (i = 0; i < hs; i++)
		LIST_INIT(&lsacache.hashtbl[i]);

	lsacache.hashmask = hs - 1;
}

struct lsa_ref *
lsa_cache_add(void *data, u_int16_t len)
{
	struct lsa_cache_head	*head;
	struct lsa_ref		*ref, *old;

	if ((ref = calloc(1, sizeof(*ref))) == NULL)
		fatal("lsa_cache_add");
	memcpy(&ref->hdr, data, sizeof(ref->hdr));

	if ((old = lsa_cache_look(&ref->hdr))) {
		free(ref);
		old->refcnt++;
		return (old);
	}

	if ((ref->data = malloc(len)) == NULL)
		fatal("lsa_cache_add");
	memcpy(ref->data, data, len);
	ref->stamp = time(NULL);
	ref->len = len;
	ref->refcnt = 1;

	head = lsa_cache_hash(&ref->hdr);
	LIST_INSERT_HEAD(head, ref, entry);
	return (ref);
}

struct lsa_ref *
lsa_cache_get(struct lsa_hdr *lsa_hdr)
{
	struct lsa_ref		*ref;

	ref = lsa_cache_look(lsa_hdr);
	if (ref)
		ref->refcnt++;

	return (ref);
}

void
lsa_cache_put(struct lsa_ref *ref, struct nbr *nbr)
{
	if (--ref->refcnt > 0)
		return;

	if (ntohs(ref->hdr.age) >= MAX_AGE)
		ospfe_imsg_compose_rde(IMSG_LS_MAXAGE, nbr->peerid, 0,
		    ref->data, sizeof(struct lsa_hdr));

	free(ref->data);
	LIST_REMOVE(ref, entry);
	free(ref);
}

struct lsa_ref *
lsa_cache_look(struct lsa_hdr *lsa_hdr)
{
	struct lsa_cache_head	*head;
	struct lsa_ref		*ref;

	head = lsa_cache_hash(lsa_hdr);
	LIST_FOREACH(ref, head, entry) {
		if (ref->hdr.type == lsa_hdr->type &&
		    ref->hdr.ls_id == lsa_hdr->ls_id &&
		    ref->hdr.adv_rtr == lsa_hdr->adv_rtr &&
		    ref->hdr.seq_num == lsa_hdr->seq_num &&
		    ref->hdr.ls_chksum == lsa_hdr->ls_chksum) {
			/* found match */
			return (ref);
		}
	}
	return (NULL);
}

struct lsa_cache_head *
lsa_cache_hash(struct lsa_hdr *lsa_hdr)
{
	u_int32_t	hash = 8271;

	hash ^= lsa_hdr->type;
	hash ^= lsa_hdr->ls_id;
	hash ^= lsa_hdr->adv_rtr;
	hash ^= lsa_hdr->seq_num;
	hash ^= lsa_hdr->ls_chksum;
	hash &= lsacache.hashmask;

	return (&lsacache.hashtbl[hash]);
}
