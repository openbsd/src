/*	$OpenBSD: pfkey.c,v 1.24 2005/10/16 19:52:19 hshoexer Exp $	*/
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
#include "pfkey.h"

#define ROUNDUP(x) (((x) + (PFKEYV2_CHUNK - 1)) & ~(PFKEYV2_CHUNK - 1))
#define IOV_CNT 20

static int	fd;
static u_int32_t sadb_msg_seq = 1;

static int	pfkey_flow(int, u_int8_t, u_int8_t, u_int8_t,
		    struct ipsec_addr *, struct ipsec_addr *,
		    struct ipsec_addr *, struct ipsec_auth *, u_int8_t);
static int	pfkey_sa(int, u_int8_t, u_int8_t, u_int32_t,
		    struct ipsec_addr *, struct ipsec_addr *,
		    struct ipsec_transforms *, struct ipsec_key *,
		    struct ipsec_key *);
static int	pfkey_reply(int);
int		pfkey_parse(struct sadb_msg *, struct ipsec_rule *);
int		pfkey_ipsec_flush(void);
int		pfkey_ipsec_establish(int, struct ipsec_rule *);
int		pfkey_init(void);

static int
pfkey_flow(int sd, u_int8_t satype, u_int8_t action, u_int8_t direction,
    struct ipsec_addr *src, struct ipsec_addr *dst, struct ipsec_addr *peer,
    struct ipsec_auth *auth, u_int8_t flowtype)
{
	struct sadb_msg		 smsg;
	struct sadb_address	 sa_src, sa_dst, sa_peer, sa_smask, sa_dmask;
	struct sadb_protocol	 sa_flowtype, sa_protocol;
	struct sadb_ident	*sa_srcid, *sa_dstid;
	struct sockaddr_storage	 ssrc, sdst, speer, smask, dmask;
	struct iovec	 	 iov[IOV_CNT];
	ssize_t		 	 n;
	int			 iov_cnt, len, ret = 0;

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
	if (peer) {
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

	switch (flowtype) {
	case TYPE_USE:
		sa_flowtype.sadb_protocol_proto = SADB_X_FLOW_TYPE_USE;
		break;
	case TYPE_REQUIRE:
		sa_flowtype.sadb_protocol_proto = SADB_X_FLOW_TYPE_REQUIRE;
		break;
	default:
		warnx("unsupported flowtype %d", flowtype);
		return -1;
	}

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

	if (auth && auth->srcid) {
		len = ROUNDUP(strlen(auth->srcid) + 1) + sizeof(*sa_srcid);

		sa_srcid = calloc(len, sizeof(u_int8_t));
		if (sa_srcid == NULL)
			err(1, "pfkey_flow: calloc");

		sa_srcid->sadb_ident_type = auth->idtype;
		sa_srcid->sadb_ident_len = len / 8;
		sa_srcid->sadb_ident_exttype = SADB_EXT_IDENTITY_SRC;

		strlcpy((char *)(sa_srcid + 1), auth->srcid,
		    ROUNDUP(strlen(auth->srcid) + 1));
	}
	if (auth && auth->dstid) {
		len = ROUNDUP(strlen(auth->dstid) + 1) + sizeof(*sa_dstid);

		sa_dstid = calloc(len, sizeof(u_int8_t));
		if (sa_dstid == NULL)
			err(1, "pfkey_flow: calloc");

		sa_dstid->sadb_ident_type = auth->idtype;
		sa_dstid->sadb_ident_len = len / 8;
		sa_dstid->sadb_ident_exttype = SADB_EXT_IDENTITY_DST;

		strlcpy((char *)(sa_dstid + 1), auth->dstid,
		    ROUNDUP(strlen(auth->dstid) + 1));
	}

	iov_cnt = 0;

	/* header */
	iov[iov_cnt].iov_base = &smsg;
	iov[iov_cnt].iov_len = sizeof(smsg);
	iov_cnt++;

	/* add flow type */
	iov[iov_cnt].iov_base = &sa_flowtype;
	iov[iov_cnt].iov_len = sizeof(sa_flowtype);
	smsg.sadb_msg_len += sa_flowtype.sadb_protocol_len;
	iov_cnt++;

	/* remote peer */
	if (peer) {
		iov[iov_cnt].iov_base = &sa_peer;
		iov[iov_cnt].iov_len = sizeof(sa_peer);
		iov_cnt++;
		iov[iov_cnt].iov_base = &speer;
		iov[iov_cnt].iov_len = ROUNDUP(speer.ss_len);
		smsg.sadb_msg_len += sa_peer.sadb_address_len;
		iov_cnt++;
	}

	/* src addr */
	iov[iov_cnt].iov_base = &sa_src;
	iov[iov_cnt].iov_len = sizeof(sa_src);
	iov_cnt++;
	iov[iov_cnt].iov_base = &ssrc;
	iov[iov_cnt].iov_len = ROUNDUP(ssrc.ss_len);
	smsg.sadb_msg_len += sa_src.sadb_address_len;
	iov_cnt++;

	/* src mask */
	iov[iov_cnt].iov_base = &sa_smask;
	iov[iov_cnt].iov_len = sizeof(sa_smask);
	iov_cnt++;
	iov[iov_cnt].iov_base = &smask;
	iov[iov_cnt].iov_len = ROUNDUP(smask.ss_len);
	smsg.sadb_msg_len += sa_smask.sadb_address_len;
	iov_cnt++;

	/* dest addr */
	iov[iov_cnt].iov_base = &sa_dst;
	iov[iov_cnt].iov_len = sizeof(sa_dst);
	iov_cnt++;
	iov[iov_cnt].iov_base = &sdst;
	iov[iov_cnt].iov_len = ROUNDUP(sdst.ss_len);
	smsg.sadb_msg_len += sa_dst.sadb_address_len;
	iov_cnt++;

	/* dst mask */
	iov[iov_cnt].iov_base = &sa_dmask;
	iov[iov_cnt].iov_len = sizeof(sa_dmask);
	iov_cnt++;
	iov[iov_cnt].iov_base = &dmask;
	iov[iov_cnt].iov_len = ROUNDUP(dmask.ss_len);
	smsg.sadb_msg_len += sa_dmask.sadb_address_len;
	iov_cnt++;

	/* add protocol */
	iov[iov_cnt].iov_base = &sa_protocol;
	iov[iov_cnt].iov_len = sizeof(sa_protocol);
	smsg.sadb_msg_len += sa_protocol.sadb_protocol_len;
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
pfkey_sa(int sd, u_int8_t satype, u_int8_t action, u_int32_t spi, struct
    ipsec_addr *src, struct ipsec_addr *dst, struct ipsec_transforms *xfs,
    struct ipsec_key *authkey, struct ipsec_key *enckey)
{
	struct sadb_msg		smsg;
	struct sadb_sa		sa;
	struct sadb_address	sa_src, sa_dst;
	struct sadb_key		sa_authkey, sa_enckey;
	struct sockaddr_storage	ssrc, sdst;
	struct iovec	 	iov[IOV_CNT];
	ssize_t		 	n;
	int		 	iov_cnt, len, ret = 0;

	bzero(&ssrc, sizeof(ssrc));
	switch (src->af) {
	case AF_INET:
		((struct sockaddr_in *)&ssrc)->sin_addr = src->v4;
		ssrc.ss_len = sizeof(struct sockaddr_in);
		ssrc.ss_family = AF_INET;
		break;
	case AF_INET6:
	default:
		warnx("unsupported address family %d", src->af);
		return -1;
	}

	bzero(&sdst, sizeof(sdst));
	switch (dst->af) {
	case AF_INET:
		((struct sockaddr_in *)&sdst)->sin_addr = dst->v4;
		sdst.ss_len = sizeof(struct sockaddr_in);
		sdst.ss_family = AF_INET;
		break;
	case AF_INET6:
	default:
		warnx("unsupported address family %d", dst->af);
		return -1;
	}

	bzero(&smsg, sizeof(smsg));
	smsg.sadb_msg_version = PF_KEY_V2;
	smsg.sadb_msg_seq = sadb_msg_seq++;
	smsg.sadb_msg_pid = getpid();
	smsg.sadb_msg_len = sizeof(smsg) / 8;
	smsg.sadb_msg_type = action;
	smsg.sadb_msg_satype = satype;

	bzero(&sa, sizeof(sa));
	sa.sadb_sa_len = sizeof(sa) / 8;
	sa.sadb_sa_exttype = SADB_EXT_SA;
	sa.sadb_sa_spi = htonl(spi);
	sa.sadb_sa_state = SADB_SASTATE_MATURE;

	if (xfs && xfs->authxf) {
		switch (xfs->authxf->id) {
		case AUTHXF_NONE:
			break;
		case AUTHXF_HMAC_MD5:
			sa.sadb_sa_auth = SADB_AALG_MD5HMAC;
			break;
		case AUTHXF_HMAC_RIPEMD160:
			sa.sadb_sa_auth = SADB_X_AALG_RIPEMD160HMAC;
			break;
		case AUTHXF_HMAC_SHA1:
			sa.sadb_sa_auth = SADB_AALG_SHA1HMAC;
			break;
		case AUTHXF_HMAC_SHA2_256:
			sa.sadb_sa_auth = SADB_X_AALG_SHA2_256;
			break;
		case AUTHXF_HMAC_SHA2_384:
			sa.sadb_sa_auth = SADB_X_AALG_SHA2_384;
			break;
		case AUTHXF_HMAC_SHA2_512:
			sa.sadb_sa_auth = SADB_X_AALG_SHA2_512;
			break;
		case AUTHXF_MD5:
			sa.sadb_sa_auth = SADB_X_AALG_MD5;
			break;
		case AUTHXF_SHA1:
			sa.sadb_sa_auth = SADB_X_AALG_SHA1;
			break;
		default:
			warnx("unsupported authentication algorithm %d",
			    xfs->authxf->id);
		}
	}
	if (xfs && xfs->encxf) {
		switch (xfs->encxf->id) {
		case ENCXF_NONE:
			break;
		case ENCXF_3DES_CBC:
			sa.sadb_sa_encrypt = SADB_EALG_3DESCBC;
			break;
		case ENCXF_DES_CBC:
			sa.sadb_sa_encrypt = SADB_EALG_DESCBC;
			break;
		case ENCXF_AES:
			sa.sadb_sa_encrypt = SADB_X_EALG_AES;
			break;
		case ENCXF_AESCTR:
			sa.sadb_sa_encrypt = SADB_X_EALG_AESCTR;
			break;
		case ENCXF_BLOWFISH:
			sa.sadb_sa_encrypt = SADB_X_EALG_BLF;
			break;
		case ENCXF_CAST128:
			sa.sadb_sa_encrypt = SADB_X_EALG_CAST;
			break;
		case ENCXF_NULL:
			sa.sadb_sa_encrypt = SADB_EALG_NULL;
			break;
		case ENCXF_SKIPJACK:
			sa.sadb_sa_encrypt = SADB_X_EALG_SKIPJACK;
			break;
		default:
			warnx("unsupported encryption algorithm %d",
			    xfs->encxf->id);
		}
	}

	bzero(&sa_src, sizeof(sa_src));
	sa_src.sadb_address_len = (sizeof(sa_src) + ROUNDUP(ssrc.ss_len)) / 8;
	sa_src.sadb_address_exttype = SADB_EXT_ADDRESS_SRC;

	bzero(&sa_dst, sizeof(sa_dst));
	sa_dst.sadb_address_len = (sizeof(sa_dst) + ROUNDUP(sdst.ss_len)) / 8;
	sa_dst.sadb_address_exttype = SADB_EXT_ADDRESS_DST;

	if (action == SADB_ADD && !authkey && !enckey) { /* XXX ENCNULL */
		warnx("no key specified");
		return -1;
	}
	if (authkey) {
		bzero(&sa_authkey, sizeof(sa_authkey));
		sa_authkey.sadb_key_len = (sizeof(sa_authkey) +
		    ((authkey->len + 7) / 8) * 8) / 8;
		sa_authkey.sadb_key_exttype = SADB_EXT_KEY_AUTH;
		sa_authkey.sadb_key_bits = 8 * authkey->len;
	}
	if (enckey) {
		bzero(&sa_enckey, sizeof(sa_enckey));
		sa_enckey.sadb_key_len = (sizeof(sa_enckey) +
		    ((enckey->len + 7) / 8) * 8) / 8;
		sa_enckey.sadb_key_exttype = SADB_EXT_KEY_ENCRYPT;
		sa_enckey.sadb_key_bits = 8 * enckey->len;
	}

	iov_cnt = 0;

	/* header */
	iov[iov_cnt].iov_base = &smsg;
	iov[iov_cnt].iov_len = sizeof(smsg);
	iov_cnt++;

	/* sa */
	iov[iov_cnt].iov_base = &sa;
	iov[iov_cnt].iov_len = sizeof(sa);
	smsg.sadb_msg_len += sa.sadb_sa_len;
	iov_cnt++;

	/* src addr */
	iov[iov_cnt].iov_base = &sa_src;
	iov[iov_cnt].iov_len = sizeof(sa_src);
	iov_cnt++;
	iov[iov_cnt].iov_base = &ssrc;
	iov[iov_cnt].iov_len = ROUNDUP(ssrc.ss_len);
	smsg.sadb_msg_len += sa_src.sadb_address_len;
	iov_cnt++;

	/* dst addr */
	iov[iov_cnt].iov_base = &sa_dst;
	iov[iov_cnt].iov_len = sizeof(sa_dst);
	iov_cnt++;
	iov[iov_cnt].iov_base = &sdst;
	iov[iov_cnt].iov_len = ROUNDUP(sdst.ss_len);
	smsg.sadb_msg_len += sa_dst.sadb_address_len;
	iov_cnt++;

	if (authkey) {
		/* authentication key */
		iov[iov_cnt].iov_base = &sa_authkey;
		iov[iov_cnt].iov_len = sizeof(sa_authkey);
		iov_cnt++;
		iov[iov_cnt].iov_base = authkey->data;
		iov[iov_cnt].iov_len = ((authkey->len + 7) / 8) * 8;
		smsg.sadb_msg_len += sa_authkey.sadb_key_len;
		iov_cnt++;
	}
	if (enckey) {
		/* encryption key */
		iov[iov_cnt].iov_base = &sa_enckey;
		iov[iov_cnt].iov_len = sizeof(sa_enckey);
		iov_cnt++;
		iov[iov_cnt].iov_base = enckey->data;
		iov[iov_cnt].iov_len = ((enckey->len + 7) / 8) * 8;
		smsg.sadb_msg_len += sa_enckey.sadb_key_len;
		iov_cnt++;
	}

	len = smsg.sadb_msg_len * 8;
	if ((n = writev(sd, iov, iov_cnt)) == -1) {
		warn("writev failed");
		ret = -1;
	} else if (n != len) {
		warnx("short write");
		ret = -1;
	}

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
		warn("PF_KEY failed");
		return -1;
	}
	len = hdr.sadb_msg_len * PFKEYV2_CHUNK;
	if ((data = malloc(len)) == NULL)
		err(1, "pfkey_reply: malloc");
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
pfkey_parse(struct sadb_msg *msg, struct ipsec_rule *rule)
{
	struct sadb_ext		*ext;
	struct sadb_address	*saddr;
	struct sadb_protocol	*sproto;
	struct sadb_ident	*sident;
	struct sockaddr		*sa;
	int			 len;

	switch (msg->sadb_msg_satype) {
	case SADB_SATYPE_ESP:
		rule->proto = IPSEC_ESP;
		break;
	case SADB_SATYPE_AH:
		rule->proto = IPSEC_AH;
		break;
	case SADB_X_SATYPE_IPCOMP:
	default:
		return (1);
	}

	for (ext = (struct sadb_ext *)(msg + 1);
	    (size_t)((u_int8_t *)ext - (u_int8_t *)msg) <
	    msg->sadb_msg_len * PFKEYV2_CHUNK && ext->sadb_ext_len > 0;
	    ext = (struct sadb_ext *)((u_int8_t *)ext +
	    ext->sadb_ext_len * PFKEYV2_CHUNK)) {
		switch (ext->sadb_ext_type) {
		case SADB_EXT_ADDRESS_SRC:
#ifdef notyet
			saddr = (struct sadb_address *)ext;
			sa = (struct sockaddr *)(saddr + 1);

			rule->local = calloc(1, sizeof(struct ipsec_addr));
			if (rule->local == NULL)
				err(1, "pfkey_parse: malloc");

			switch (sa->sa_family) {
			case AF_INET:
				bcopy(&((struct sockaddr_in *)sa)->sin_addr,
				    &rule->local->v4, sizeof(struct in_addr));
				memset(&rule->local->v4mask, 0xff,
				    sizeof(u_int32_t));
				rule->local->af = AF_INET;
				break;
			default:
				return (1);
			}
#endif
			break;


		case SADB_EXT_ADDRESS_DST:
			saddr = (struct sadb_address *)ext;
			sa = (struct sockaddr *)(saddr + 1);

			rule->peer = calloc(1, sizeof(struct ipsec_addr));
			if (rule->peer == NULL)
				err(1, "pfkey_parse: malloc");

			switch (sa->sa_family) {
			case AF_INET:
				bcopy(&((struct sockaddr_in *)sa)->sin_addr,
				    &rule->peer->v4, sizeof(struct in_addr));
				memset(&rule->peer->v4mask, 0xff,
				    sizeof(u_int32_t));
				rule->peer->af = AF_INET;
				break;
			default:
				return (1);
			}
			break;

		case SADB_EXT_IDENTITY_SRC:
			sident = (struct sadb_ident *)ext;
			len = (sident->sadb_ident_len * sizeof(uint64_t)) -
			    sizeof(struct sadb_ident);

			if (rule->auth == NULL) {
				rule->auth = calloc(1, sizeof(struct
				    ipsec_auth));
				if (rule->auth == NULL)
					err(1, "pfkey_parse: calloc");
			}

			rule->auth->srcid = calloc(1, len);
			if (rule->auth->srcid == NULL)
				err(1, "pfkey_parse: calloc");

			strlcpy(rule->auth->srcid, (char *)(sident + 1), len);
			break;

		case SADB_EXT_IDENTITY_DST:
			sident = (struct sadb_ident *)ext;
			len = (sident->sadb_ident_len * sizeof(uint64_t)) -
			    sizeof(struct sadb_ident);

			if (rule->auth == NULL) {
				rule->auth = calloc(1, sizeof(struct
				    ipsec_auth));
				if (rule->auth == NULL)
					err(1, "pfkey_parse: calloc");
			}

			rule->auth->dstid = calloc(1, len);
			if (rule->auth->dstid == NULL)
				err(1, "pfkey_parse: calloc");

			strlcpy(rule->auth->dstid, (char *)(sident + 1), len);
			break;

		case SADB_X_EXT_PROTOCOL:
			/* XXX nothing yet? */
			break;

		case SADB_X_EXT_FLOW_TYPE:
			sproto = (struct sadb_protocol *)ext;

			switch (sproto->sadb_protocol_direction) {
			case IPSP_DIRECTION_IN:
				rule->direction = IPSEC_IN;
				break;
			case IPSP_DIRECTION_OUT:
				rule->direction = IPSEC_OUT;
				break;
			default:
				return (1);
			}
			switch (sproto->sadb_protocol_proto) {
			case SADB_X_FLOW_TYPE_USE:
				rule->flowtype = TYPE_USE;
				break;
			case SADB_X_FLOW_TYPE_ACQUIRE:
				rule->flowtype = TYPE_ACQUIRE;
				break;
			case SADB_X_FLOW_TYPE_REQUIRE:
				rule->flowtype = TYPE_REQUIRE;
				break;
			case SADB_X_FLOW_TYPE_DENY:
				rule->flowtype = TYPE_DENY;
				break;
			case SADB_X_FLOW_TYPE_BYPASS:
				rule->flowtype = TYPE_BYPASS;
				break;
			case SADB_X_FLOW_TYPE_DONTACQ:
				rule->flowtype = TYPE_DONTACQ;
				break;
			default:
				rule->flowtype = TYPE_UNKNOWN;
				break;
			}
			break;

		case SADB_X_EXT_SRC_FLOW:
			saddr = (struct sadb_address *)ext;
			sa = (struct sockaddr *)(saddr + 1);

			if (rule->src == NULL) {
				rule->src = calloc(1,
				    sizeof(struct ipsec_addr));
				if (rule->src == NULL)
					err(1, "pfkey_parse: calloc");
			}

			switch (sa->sa_family) {
			case AF_INET:
				bcopy(&((struct sockaddr_in *)sa)->sin_addr,
				    &rule->src->v4, sizeof(struct in_addr));
				rule->src->af = AF_INET;
				break;
			default:
				return (1);
			}
			break;

		case SADB_X_EXT_DST_FLOW:
			saddr = (struct sadb_address *)ext;
			sa = (struct sockaddr *)(saddr + 1);

			if (rule->dst == NULL) {
				rule->dst = calloc(1,
				    sizeof(struct ipsec_addr));
				if (rule->dst == NULL)
					err(1, "pfkey_parse: calloc");
			}

			switch (sa->sa_family) {
			case AF_INET:
				bcopy(&((struct sockaddr_in *)sa)->sin_addr,
				    &rule->dst->v4, sizeof(struct in_addr));
				rule->dst->af = AF_INET;
				break;

			default:
				return (1);
			}
			break;


		case SADB_X_EXT_SRC_MASK:
			saddr = (struct sadb_address *)ext;
			sa = (struct sockaddr *)(saddr + 1);

			if (rule->src == NULL) {
				rule->src = calloc(1,
				    sizeof(struct ipsec_addr));
				if (rule->src == NULL)
					err(1, "pfkey_parse: calloc");
			}

			switch (sa->sa_family) {
			case AF_INET:
				bcopy(&((struct sockaddr_in *)sa)->sin_addr,
				    &rule->src->v4mask.mask,
				    sizeof(struct in_addr));
				rule->src->af = AF_INET;
				break;

			default:
				return (1);
			}
			break;

		case SADB_X_EXT_DST_MASK:
			saddr = (struct sadb_address *)ext;
			sa = (struct sockaddr *)(saddr + 1);

			if (rule->dst == NULL) {
				rule->dst = calloc(1,
				    sizeof(struct ipsec_addr));
				if (rule->dst == NULL)
					err(1, "pfkey_parse: calloc");
			}

			switch (sa->sa_family) {
			case AF_INET:
				bcopy(&((struct sockaddr_in *)sa)->sin_addr,
				    &rule->dst->v4mask.mask,
				    sizeof(struct in_addr));
				rule->dst->af = AF_INET;
				break;

			default:
				return (1);
			}
			break;
		
		default:
			return (1);
		}
	}

	return (0);
}

int
pfkey_ipsec_establish(int action, struct ipsec_rule *r)
{
	int		ret;
	u_int8_t	satype, direction;

	if (r->type == RULE_FLOW) {
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

		switch (action) {
		case ACTION_ADD:
			ret = pfkey_flow(fd, satype, SADB_X_ADDFLOW, direction,
			    r->src, r->dst, r->peer, r->auth, r->flowtype);
			break;
		case ACTION_DELETE:
			/* No peer for flow deletion. */
			ret = pfkey_flow(fd, satype, SADB_X_DELFLOW, direction,
			    r->src, r->dst, NULL, NULL, r->flowtype);
			break;
		default:
			return -1;
		}
	} else if (r->type == RULE_SA) {
		switch (r->proto) {
		case IPSEC_AH:
			satype = SADB_SATYPE_AH;
			break;
		case IPSEC_ESP:
			satype = SADB_SATYPE_ESP;
			break;
		case IPSEC_TCPMD5:
			satype = SADB_X_SATYPE_TCPSIGNATURE;
			break;
		default:
			return -1;
		}
		switch (action) {
		case ACTION_ADD:
			ret = pfkey_sa(fd, satype, SADB_ADD, r->spi,
			    r->src, r->dst, r->xfs, r->authkey, r->enckey);
			break;
		case ACTION_DELETE:
			ret = pfkey_sa(fd, satype, SADB_DELETE, r->spi,
			    r->src, r->dst, r->xfs, NULL, NULL);
			break;
		default:
			return -1;
		}
	} else
		return -1;

	if (ret < 0)
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
		err(1, "pfkey_init: failed to open PF_KEY socket");

	return 0;
}
