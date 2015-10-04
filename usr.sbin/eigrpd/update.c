/*	$OpenBSD: update.c,v 1.2 2015/10/04 23:00:10 renato Exp $ */

/*
 * Copyright (c) 2015 Renato Westphal <renato@openbsd.org>
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

#include <stdlib.h>
#include <netinet/in.h>
#include <netinet/ip6.h>

#include "eigrpd.h"
#include "eigrp.h"
#include "log.h"
#include "eigrpe.h"

extern struct eigrpd_conf	*econf;

/* update packet handling */

void
send_update(struct eigrp_iface *ei, struct nbr *nbr, uint32_t flags,
    int startup, struct rinfo_head *rinfo_list)
{
	struct eigrp		*eigrp = ei->eigrp;
	struct ibuf		*buf;
	struct rinfo_entry	*re;
	int			 size;
	int			 route_len;
	struct eigrp_hdr	*eigrp_hdr;

	/* don't exceed the interface's mtu */
	do {
		if ((buf = ibuf_dynamic(PKG_DEF_SIZE,
		    IP_MAXPACKET - sizeof(struct ip))) == NULL)
			fatal("send_update");

		/* EIGRP header */
		if (gen_eigrp_hdr(buf, EIGRP_OPC_UPDATE, flags,
		    eigrp->seq_num, eigrp->as))
			goto fail;

		if (rinfo_list == NULL)
			break;

		switch (eigrp->af) {
		case AF_INET:
			size = sizeof(struct ip);
			break;
		case AF_INET6:
			size = sizeof(struct ip6_hdr);
			break;
		default:
			fatalx("send_update: unknown af");
		}
		size += sizeof(struct eigrp_hdr);

		while ((re = TAILQ_FIRST(rinfo_list)) != NULL) {
			route_len = len_route_tlv(&re->rinfo);
			if (size + route_len > ei->iface->mtu) {
				rtp_send(ei, nbr, buf);
				break;
			}
			size += route_len;

			if (gen_route_tlv(buf, &re->rinfo))
				goto fail;
			TAILQ_REMOVE(rinfo_list, re, entry);
			free(re);
		}
	} while (!TAILQ_EMPTY(rinfo_list));

	/* set the EOT flag in the last startup update */
	if (startup) {
		if ((eigrp_hdr = ibuf_seek(buf, 0, sizeof(*eigrp_hdr))) == NULL)
                	fatalx("send_update: buf_seek failed");
		eigrp_hdr->flags = ntohl(eigrp_hdr->flags) | EIGRP_HDR_FLAG_EOT;
		eigrp_hdr->flags = htonl(eigrp_hdr->flags);
	}

	rtp_send(ei, nbr, buf);
	return;
fail:
	log_warnx("%s: failed to send message", __func__);
	if (rinfo_list)
		message_list_clr(rinfo_list);
	ibuf_free(buf);
	return;
}

void
recv_update(struct nbr *nbr, struct rinfo_head *rinfo_list, uint32_t flags)
{
	struct rinfo_entry	*re;

	rtp_ack_start_timer(nbr);

	if (flags & EIGRP_HDR_FLAG_INIT) {
		log_debug("%s: INIT flag is set", __func__);

		if (nbr->flags & F_EIGRP_NBR_PENDING)
			nbr_init(nbr);
		else if (!(flags & EIGRP_HDR_FLAG_RS))
			/*
			 * This is not in the draft, but apparently if a Cisco
			 * device sends an INIT Update it expects to receive
			 * an INIT Update as well, otherwise it triggers the
			 * "stuck in INIT state" error and discards subsequent
			 * packets.  However, there is an exception: when the
			 * "clear ip eigrp neighbors soft" command is issued
			 * on a Cisco device, the "Restart Flag" is also set
			 * in the EIGRP header. In this case the Cisco device
			 * doesn't expect to receive an INIT Update otherwise
			 * the adjacency will flap.  Unfortunately it looks
			 * like that there is some kind of initialization
			 * FSM implemented in the Cisco devices that is not
			 * documented in the draft.
			 */
			send_update(nbr->ei, nbr, EIGRP_HDR_FLAG_INIT,
			    0, NULL);

		/*
		 * The INIT flag instructs us to advertise all of our routes,
		 * even if the neighbor is not pending.
		 */
		eigrpe_imsg_compose_rde(IMSG_RECV_UPDATE_INIT, nbr->peerid,
		    0, NULL, 0);
		return;
	}

	TAILQ_FOREACH(re, rinfo_list, entry)
		eigrpe_imsg_compose_rde(IMSG_RECV_UPDATE, nbr->peerid,
		    0, &re->rinfo, sizeof(re->rinfo));
}
