/*	$OpenBSD: hello.c,v 1.26 2015/07/21 04:40:56 renato Exp $ */

/*
 * Copyright (c) 2009 Michele Marchetto <michele@openbsd.org>
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
#include <sys/uio.h>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <net/if_dl.h>

#include <errno.h>
#include <event.h>
#include <stdlib.h>
#include <string.h>

#include "ldpd.h"
#include "ldp.h"
#include "log.h"
#include "ldpe.h"

extern struct ldpd_conf        *leconf;

int	tlv_decode_hello_prms(char *, u_int16_t, u_int16_t *, u_int16_t *);
int	tlv_decode_opt_hello_prms(char *, u_int16_t, struct in_addr *,
	    u_int32_t *);
int	gen_hello_prms_tlv(struct ibuf *buf, u_int16_t, u_int16_t);
int	gen_opt4_hello_prms_tlv(struct ibuf *, u_int16_t, u_int32_t);

int
send_hello(enum hello_type type, struct iface *iface, struct tnbr *tnbr)
{
	struct sockaddr_in	 dst;
	struct ibuf		*buf;
	u_int16_t		 size, holdtime = 0, flags = 0;
	int			 fd = 0;

	dst.sin_port = htons(LDP_PORT);
	dst.sin_family = AF_INET;
	dst.sin_len = sizeof(struct sockaddr_in);

	switch (type) {
	case HELLO_LINK:
		inet_aton(AllRouters, &dst.sin_addr);
		holdtime = iface->hello_holdtime;
		flags = 0;
		fd = iface->discovery_fd;
		break;
	case HELLO_TARGETED:
		dst.sin_addr.s_addr = tnbr->addr.s_addr;
		holdtime = tnbr->hello_holdtime;
		flags = TARGETED_HELLO;
		if (tnbr->flags & F_TNBR_CONFIGURED)
			flags |= REQUEST_TARG_HELLO;
		fd = tnbr->discovery_fd;
		break;
	}

	if ((buf = ibuf_open(LDP_MAX_LEN)) == NULL)
		fatal("send_hello");

	size = LDP_HDR_SIZE + sizeof(struct ldp_msg) +
	    sizeof(struct hello_prms_tlv) +
	    sizeof(struct hello_prms_opt4_tlv);

	gen_ldp_hdr(buf, size);

	size -= LDP_HDR_SIZE;

	gen_msg_tlv(buf, MSG_TYPE_HELLO, size);

	gen_hello_prms_tlv(buf, holdtime, flags);
	gen_opt4_hello_prms_tlv(buf, TLV_TYPE_IPV4TRANSADDR, ldpe_router_id());

	send_packet(fd, iface, buf->buf, buf->wpos, &dst);
	ibuf_free(buf);

	return (0);
}

void
recv_hello(struct iface *iface, struct in_addr src, char *buf, u_int16_t len)
{
	struct ldp_msg		 hello;
	struct ldp_hdr		 ldp;
	struct adj		*adj;
	struct nbr		*nbr;
	struct in_addr		 lsr_id;
	struct in_addr		 transport_addr;
	u_int32_t		 conf_number;
	u_int16_t		 holdtime, flags;
	int			 r;
	struct hello_source	 source;
	struct tnbr		*tnbr = NULL;

	bcopy(buf, &ldp, sizeof(ldp));
	buf += LDP_HDR_SIZE;
	len -= LDP_HDR_SIZE;

	bcopy(buf, &hello, sizeof(hello));
	buf += sizeof(struct ldp_msg);
	len -= sizeof(struct ldp_msg);

	lsr_id.s_addr = ldp.lsr_id;

	r = tlv_decode_hello_prms(buf, len, &holdtime, &flags);
	if (r == -1) {
		log_debug("recv_hello: neighbor %s: failed to decode params",
		    inet_ntoa(lsr_id));
		return;
	}

	bzero(&source, sizeof(source));
	if (flags & TARGETED_HELLO) {
		tnbr = tnbr_find(src);

		/* remove the dynamic tnbr if the 'R' bit was cleared */
		if (tnbr && (tnbr->flags & F_TNBR_DYNAMIC) &&
		    !((flags & REQUEST_TARG_HELLO))) {
			tnbr->flags &= ~F_TNBR_DYNAMIC;
			tnbr = tnbr_check(tnbr);
		}

		if (!tnbr) {
			if (!((flags & REQUEST_TARG_HELLO) &&
			    leconf->flags & LDPD_FLAG_TH_ACCEPT))
				return;

			tnbr = tnbr_new(leconf, src);
			tnbr->flags |= F_TNBR_DYNAMIC;
			tnbr_init(leconf, tnbr);
			LIST_INSERT_HEAD(&leconf->tnbr_list, tnbr, entry);
		}

		source.type = HELLO_TARGETED;
		source.target = tnbr;
	} else {
		if (ldp.lspace_id != 0) {
			log_debug("recv_hello: invalid label space "
			    "ID %u, interface %s", ldp.lspace_id,
			    iface->name);
			return;
		}
		source.type = HELLO_LINK;
		source.link.iface = iface;
		source.link.src_addr.s_addr = src.s_addr;
	}

	buf += r;
	len -= r;

	r = tlv_decode_opt_hello_prms(buf, len, &transport_addr,
	    &conf_number);
	if (r == -1) {
		log_debug("recv_hello: neighbor %s: failed to decode "
		    "optional params", inet_ntoa(lsr_id));
		return;
	}
	if (transport_addr.s_addr == INADDR_ANY)
		transport_addr.s_addr = src.s_addr;

	if (r != len) {
		log_debug("recv_hello: neighbor %s: unexpected data in message",
		    inet_ntoa(lsr_id));
		return;
	}

	nbr = nbr_find_ldpid(ldp.lsr_id);
	if (!nbr) {
		/* create new adjacency and new neighbor */
		nbr = nbr_new(lsr_id, transport_addr);
		adj = adj_new(nbr, &source, transport_addr);
	} else {
		adj = adj_find(nbr, &source);
		if (!adj) {
			/* create new adjacency for existing neighbor */
			adj = adj_new(nbr, &source, transport_addr);

			if (nbr->addr.s_addr != transport_addr.s_addr)
				log_warnx("recv_hello: neighbor %s: multiple "
				    "adjacencies advertising different "
				    "transport addresses", inet_ntoa(lsr_id));
		}
	}

	/* always update the holdtime to properly handle runtime changes */
	switch (source.type) {
	case HELLO_LINK:
		if (holdtime == 0)
			holdtime = LINK_DFLT_HOLDTIME;

		adj->holdtime = min(iface->hello_holdtime, holdtime);
		break;
	case HELLO_TARGETED:
		if (holdtime == 0)
			holdtime = TARGETED_DFLT_HOLDTIME;

		adj->holdtime = min(tnbr->hello_holdtime, holdtime);
	}

	if (adj->holdtime != INFINITE_HOLDTIME)
		adj_start_itimer(adj);
	else
		adj_stop_itimer(adj);

	if (nbr->state == NBR_STA_PRESENT && nbr_session_active_role(nbr) &&
	    !nbr_pending_connect(nbr) && !nbr_pending_idtimer(nbr))
		nbr_establish_connection(nbr);
}

int
gen_hello_prms_tlv(struct ibuf *buf, u_int16_t holdtime, u_int16_t flags)
{
	struct hello_prms_tlv	parms;

	bzero(&parms, sizeof(parms));
	parms.type = htons(TLV_TYPE_COMMONHELLO);
	parms.length = htons(sizeof(parms.holdtime) + sizeof(parms.flags));
	parms.holdtime = htons(holdtime);
	parms.flags = htons(flags);

	return (ibuf_add(buf, &parms, sizeof(parms)));
}

int
gen_opt4_hello_prms_tlv(struct ibuf *buf, u_int16_t type, u_int32_t value)
{
	struct hello_prms_opt4_tlv	parms;

	bzero(&parms, sizeof(parms));
	parms.type = htons(type);
	parms.length = htons(4);
	parms.value = value;

	return (ibuf_add(buf, &parms, sizeof(parms)));
}

int
tlv_decode_hello_prms(char *buf, u_int16_t len, u_int16_t *holdtime,
    u_int16_t *flags)
{
	struct hello_prms_tlv	tlv;

	if (len < sizeof(tlv))
		return (-1);
	bcopy(buf, &tlv, sizeof(tlv));

	if (ntohs(tlv.length) != sizeof(tlv) - TLV_HDR_LEN)
		return (-1);

	if (tlv.type != htons(TLV_TYPE_COMMONHELLO))
		return (-1);

	*holdtime = ntohs(tlv.holdtime);
	*flags = ntohs(tlv.flags);

	return (sizeof(tlv));
}

int
tlv_decode_opt_hello_prms(char *buf, u_int16_t len, struct in_addr *addr,
    u_int32_t *conf_number)
{
	struct tlv	tlv;
	int		cons = 0;
	u_int16_t	tlv_len;

	bzero(addr, sizeof(*addr));
	*conf_number = 0;

	while (len >= sizeof(tlv)) {
		bcopy(buf, &tlv, sizeof(tlv));
		tlv_len = ntohs(tlv.length);
		switch (ntohs(tlv.type)) {
		case TLV_TYPE_IPV4TRANSADDR:
			if (tlv_len != sizeof(u_int32_t))
				return (-1);
			bcopy(buf + TLV_HDR_LEN, addr, sizeof(u_int32_t));
			break;
		case TLV_TYPE_CONFIG:
			if (tlv_len != sizeof(u_int32_t))
				return (-1);
			bcopy(buf + TLV_HDR_LEN, conf_number,
			    sizeof(u_int32_t));
			break;
		default:
			/* if unknown flag set, ignore TLV */
			if (!(ntohs(tlv.type) & UNKNOWN_FLAG))
				return (-1);
			break;
		}
		buf += TLV_HDR_LEN + tlv_len;
		len -= TLV_HDR_LEN + tlv_len;
		cons += TLV_HDR_LEN + tlv_len;
	}

	return (cons);
}
