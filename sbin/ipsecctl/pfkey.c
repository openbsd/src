/*	$OpenBSD: pfkey.c,v 1.2 2005/04/04 22:22:55 hshoexer Exp $	*/
/*
 * Copyright (c) 2003, 2004 Henning Brauer <henning@openbsd.org>
 * Copyright (c) 2003, 2004 Markus Friedl <markus@openbsd.org>
 * Copyright (c) 2004, 2005 Hans-Joerg Hoexer <hshoexer@openbsd.org>
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
#include <sys/queue.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip_ipsp.h>
#include <net/pfkeyv2.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "ipsecctl.h"

#define PFKEYV2_CHUNK sizeof(u_int64_t)
#define ROUNDUP(x) (((x) + (PFKEYV2_CHUNK - 1)) & ~(PFKEYV2_CHUNK - 1))
#define IOV_CNT 20

static int	fd;
static u_int32_t sadb_msg_seq = 1;

static int pfkey_flow(int, u_int8_t, u_int8_t, u_int8_t, struct ipsec_addr *,
		    struct ipsec_addr *, struct ipsec_addr *,
		    struct ipsec_auth);
static int	pfkey_reply(int);
int		pfkey_ipsec_flush(void);
int		pfkey_ipsec_establish(struct ipsec_rule *);
int		pfkey_init(void);

static int
pfkey_flow(int sd, u_int8_t satype, u_int8_t action, u_int8_t direction,
    struct ipsec_addr *src, struct ipsec_addr *dst, struct ipsec_addr *peer,
    struct ipsec_auth auth)
{
	struct sadb_msg	 smsg;
	struct sadb_address sa_src, sa_dst, sa_peer, sa_smask, sa_dmask;
	struct sadb_protocol sa_flowtype, sa_protocol;
	struct sadb_ident *sa_srcid, *sa_dstid;
	struct sockaddr_storage ssrc, sdst, speer, smask, dmask;
	struct iovec	 iov[IOV_CNT];
	ssize_t		 n;
	int		 iov_cnt, len, ret = 0;

	sa_srcid = sa_dstid = NULL;

	bzero(&ssrc, sizeof(ssrc));
	bzero(&smask, sizeof(smask));
	switch (src->af) {
	case AF_INET:
		((struct sockaddr_in *)&ssrc)->sin_addr = src->v4;
		ssrc.ss_len = sizeof(struct sockaddr_in);
		ssrc.ss_family = AF_INET;
		((struct sockaddr_in *)&smask)->sin_addr = src->v4mask.mask;
		break;
	case AF_INET6:
	default:
		warnx("unsupported address family %d", src->af);
		return -1;
	}
	smask.ss_family = ssrc.ss_family;
	smask.ss_len = ssrc.ss_len;

	bzero(&sdst, sizeof(sdst));
	bzero(&dmask, sizeof(dmask));
	switch (dst->af) {
	case AF_INET:
		((struct sockaddr_in *)&sdst)->sin_addr = dst->v4;
		sdst.ss_len = sizeof(struct sockaddr_in);
		sdst.ss_family = AF_INET;
		((struct sockaddr_in *)&dmask)->sin_addr = dst->v4mask.mask;
		break;
	case AF_INET6:
	default:
		warnx("unsupported address family %d", dst->af);
		return -1;
	}
	dmask.ss_family = sdst.ss_family;
	dmask.ss_len = sdst.ss_len;

	bzero(&speer, sizeof(speer));
	switch (peer->af) {
	case AF_INET:
		((struct sockaddr_in *)&speer)->sin_addr = peer->v4;
		speer.ss_len = sizeof(struct sockaddr_in);
		speer.ss_family = AF_INET;
		break;
	case AF_INET6:
	default:
		warnx("unsupported address family %d", peer->af);
		return -1;
	}

	bzero(&smsg, sizeof(smsg));
	smsg.sadb_msg_version = PF_KEY_V2;
	smsg.sadb_msg_seq = sadb_msg_seq++;
	smsg.sadb_msg_pid = getpid();
	smsg.sadb_msg_len = sizeof(smsg) / 8;
	smsg.sadb_msg_type = action;
	smsg.sadb_msg_satype = satype;

	bzero(&sa_flowtype, sizeof(sa_flowtype));
	sa_flowtype.sadb_protocol_exttype = SADB_X_EXT_FLOW_TYPE;
	sa_flowtype.sadb_protocol_len = sizeof(sa_flowtype) / 8;
	sa_flowtype.sadb_protocol_direction = direction;
	sa_flowtype.sadb_protocol_proto = SADB_X_FLOW_TYPE_REQUIRE;

	bzero(&sa_protocol, sizeof(sa_protocol));
	sa_protocol.sadb_protocol_exttype = SADB_X_EXT_PROTOCOL;
	sa_protocol.sadb_protocol_len = sizeof(sa_protocol) / 8;
	sa_protocol.sadb_protocol_direction = 0;
	sa_protocol.sadb_protocol_proto = IPPROTO_IP;

	bzero(&sa_src, sizeof(sa_src));
	sa_src.sadb_address_exttype = SADB_X_EXT_SRC_FLOW;
	sa_src.sadb_address_len = (sizeof(sa_src) + ROUNDUP(ssrc.ss_len)) / 8;

	bzero(&sa_smask, sizeof(sa_smask));
	sa_smask.sadb_address_exttype = SADB_X_EXT_SRC_MASK;
	sa_smask.sadb_address_len =
	    (sizeof(sa_smask) + ROUNDUP(smask.ss_len)) / 8;

	bzero(&sa_dst, sizeof(sa_dst));
	sa_dst.sadb_address_exttype = SADB_X_EXT_DST_FLOW;
	sa_dst.sadb_address_len = (sizeof(sa_dst) + ROUNDUP(sdst.ss_len)) / 8;

	bzero(&sa_dmask, sizeof(sa_dmask));
	sa_dmask.sadb_address_exttype = SADB_X_EXT_DST_MASK;
	sa_dmask.sadb_address_len =
	    (sizeof(sa_dmask) + ROUNDUP(dmask.ss_len)) / 8;

	bzero(&sa_peer, sizeof(sa_peer));
	sa_peer.sadb_address_exttype = SADB_EXT_ADDRESS_DST;
	sa_peer.sadb_address_len =
	    (sizeof(sa_peer) + ROUNDUP(speer.ss_len)) / 8;

	if (auth.srcid) {
		len = ROUNDUP(strlen(auth.srcid) + 1) + sizeof(*sa_srcid);

		sa_srcid = calloc(len, sizeof(u_int8_t));
		if (sa_srcid == NULL)
			err(1, "calloc");

		sa_srcid->sadb_ident_type = auth.idtype;
		sa_srcid->sadb_ident_len = len / 8;
		sa_srcid->sadb_ident_exttype = SADB_EXT_IDENTITY_SRC;

		strlcpy((char *)(sa_srcid + 1), auth.srcid,
		    ROUNDUP(strlen(auth.srcid) + 1));
	}
	if (auth.dstid) {
		len = ROUNDUP(strlen(auth.dstid) + 1) + sizeof(*sa_dstid);

		sa_dstid = calloc(len, sizeof(u_int8_t));
		if (sa_dstid == NULL)
			err(1, "calloc");

		sa_dstid->sadb_ident_type = auth.idtype;
		sa_dstid->sadb_ident_len = len / 8;
		sa_dstid->sadb_ident_exttype = SADB_EXT_IDENTITY_DST;

		strlcpy((char *)(sa_dstid + 1), auth.dstid,
		    ROUNDUP(strlen(auth.dstid) + 1));
	}

	iov_cnt = 0;

	/* header */
	iov[iov_cnt].iov_base = &smsg;
	iov[iov_cnt].iov_len = sizeof(smsg);
	iov_cnt++;

	/* remote peer */
	iov[iov_cnt].iov_base = &sa_peer;
	iov[iov_cnt].iov_len = sizeof(sa_peer);
	iov_cnt++;
	iov[iov_cnt].iov_base = &speer;
	iov[iov_cnt].iov_len = ROUNDUP(speer.ss_len);
	smsg.sadb_msg_len += sa_peer.sadb_address_len;
	iov_cnt++;

	/* add flow type */
	iov[iov_cnt].iov_base = &sa_flowtype;
	iov[iov_cnt].iov_len = sizeof(sa_flowtype);
	smsg.sadb_msg_len += sa_flowtype.sadb_protocol_len;
	iov_cnt++;

	/* add protocol */
	iov[iov_cnt].iov_base = &sa_protocol;
	iov[iov_cnt].iov_len = sizeof(sa_protocol);
	smsg.sadb_msg_len += sa_protocol.sadb_protocol_len;
	iov_cnt++;

	/* add flow masks */
	iov[iov_cnt].iov_base = &sa_smask;
	iov[iov_cnt].iov_len = sizeof(sa_smask);
	iov_cnt++;
	iov[iov_cnt].iov_base = &smask;
	iov[iov_cnt].iov_len = ROUNDUP(smask.ss_len);
	smsg.sadb_msg_len += sa_smask.sadb_address_len;
	iov_cnt++;

	iov[iov_cnt].iov_base = &sa_dmask;
	iov[iov_cnt].iov_len = sizeof(sa_dmask);
	iov_cnt++;
	iov[iov_cnt].iov_base = &dmask;
	iov[iov_cnt].iov_len = ROUNDUP(dmask.ss_len);
	smsg.sadb_msg_len += sa_dmask.sadb_address_len;
	iov_cnt++;

	/* dest addr */
	iov[iov_cnt].iov_base = &sa_dst;
	iov[iov_cnt].iov_len = sizeof(sa_dst);
	iov_cnt++;
	iov[iov_cnt].iov_base = &sdst;
	iov[iov_cnt].iov_len = ROUNDUP(sdst.ss_len);
	smsg.sadb_msg_len += sa_dst.sadb_address_len;
	iov_cnt++;

	/* src addr */
	iov[iov_cnt].iov_base = &sa_src;
	iov[iov_cnt].iov_len = sizeof(sa_src);
	iov_cnt++;
	iov[iov_cnt].iov_base = &ssrc;
	iov[iov_cnt].iov_len = ROUNDUP(ssrc.ss_len);
	smsg.sadb_msg_len += sa_src.sadb_address_len;
	iov_cnt++;

	if (sa_srcid) {
		/* src identity */
		iov[iov_cnt].iov_base = sa_srcid;
		iov[iov_cnt].iov_len = sa_srcid->sadb_ident_len * 8;
		smsg.sadb_msg_len += sa_srcid->sadb_ident_len;
		iov_cnt++;
	}
	if (sa_dstid) {
		/* dst identity */
		iov[iov_cnt].iov_base = sa_dstid;
		iov[iov_cnt].iov_len = sa_dstid->sadb_ident_len * 8;
		smsg.sadb_msg_len += sa_dstid->sadb_ident_len;
		iov_cnt++;
	}
	len = smsg.sadb_msg_len * 8;
	if ((n = writev(sd, iov, iov_cnt)) == -1) {
		warn("writev failed");
		ret = -1;
		goto out;
	}
	if (n != len) {
		warnx("short write");
		ret = -1;
	}

out:
	if (sa_srcid)
		free(sa_srcid);
	if (sa_dstid)
		free(sa_dstid);

	return ret;
}

static int
pfkey_reply(int sd)
{
	struct sadb_msg	 hdr;
	ssize_t		 len;
	u_int8_t	*data;

	if (recv(sd, &hdr, sizeof(hdr), MSG_PEEK) != sizeof(hdr)) {
		warnx("short read");
		return -1;
	}
	if (hdr.sadb_msg_errno != 0) {
		errno = hdr.sadb_msg_errno;
		if (errno == ESRCH)
			return 0;
		else {
			warn("PF_KEY returned error");
			return -1;
		}
	}
	len = hdr.sadb_msg_len * PFKEYV2_CHUNK;
	if ((data = malloc(len)) == NULL)
		err(1, NULL);
	if (read(sd, data, len) != len) {
		warn("PF_KEY short read");
		bzero(data, len);
		free(data);
		return -1;
	}
	bzero(data, len);
	free(data);

	return 0;
}

int
pfkey_ipsec_establish(struct ipsec_rule *r)
{
	u_int8_t	satype;
	u_int8_t	direction;

	switch (r->proto) {
	case IPSEC_ESP:
		satype = SADB_SATYPE_ESP;
		break;
	case IPSEC_AH:
		satype = SADB_SATYPE_AH;
		break;
	case IPSEC_COMP:
	default:
		return -1;
	}

	switch (r->direction) {
	case IPSEC_IN:
		direction = IPSP_DIRECTION_IN;
		break;
	case IPSEC_OUT:
		direction = IPSP_DIRECTION_OUT;
		break;
	default:
		return -1;
	}

	if (pfkey_flow(fd, satype, SADB_X_ADDFLOW, direction, r->src, r->dst,
	    r->peer, r->auth) < 0)
		return -1;
	if (pfkey_reply(fd) < 0)
		return -1;

	return 0;
}

int
pfkey_ipsec_flush(void)
{
	struct sadb_msg smsg;
	struct iovec	iov[IOV_CNT];
	ssize_t		n;
	int		iov_cnt, len;

	bzero(&smsg, sizeof(smsg));
	smsg.sadb_msg_version = PF_KEY_V2;
	smsg.sadb_msg_seq = sadb_msg_seq++;
	smsg.sadb_msg_pid = getpid();
	smsg.sadb_msg_len = sizeof(smsg) / 8;
	smsg.sadb_msg_type = SADB_FLUSH;
	smsg.sadb_msg_satype = SADB_SATYPE_UNSPEC;

	iov_cnt = 0;

	iov[iov_cnt].iov_base = &smsg;
	iov[iov_cnt].iov_len = sizeof(smsg);
	iov_cnt++;

	len = smsg.sadb_msg_len * 8;
	if ((n = writev(fd, iov, iov_cnt)) == -1) {
		warn("writev failed");
		return -1;
	}
	if (n != len) {
		warnx("short write");
		return -1;
	}
	if (pfkey_reply(fd) < 0)
		return -1;

	return 0;
}

int
pfkey_init(void)
{
	if ((fd = socket(PF_KEY, SOCK_RAW, PF_KEY_V2)) == -1)
		err(1, "failed to open PF_KEY socket");

	return 0;
}
