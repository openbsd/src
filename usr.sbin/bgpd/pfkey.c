/*	$OpenBSD: pfkey.c,v 1.52 2018/09/20 11:06:04 benno Exp $ */

/*
 * Copyright (c) 2003, 2004 Henning Brauer <henning@openbsd.org>
 * Copyright (c) 2003, 2004 Markus Friedl <markus@openbsd.org>
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
#include <net/pfkeyv2.h>
#include <netinet/ip_ipsp.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "bgpd.h"
#include "session.h"
#include "log.h"

#define	PFKEY2_CHUNK sizeof(u_int64_t)
#define	ROUNDUP(x) (((x) + (PFKEY2_CHUNK - 1)) & ~(PFKEY2_CHUNK - 1))
#define	IOV_CNT	20

static u_int32_t	sadb_msg_seq = 0;
static u_int32_t	pid = 0; /* should pid_t but pfkey needs u_int32_t */
static int		fd;

int	pfkey_reply(int, u_int32_t *);
int	pfkey_send(int, uint8_t, uint8_t, uint8_t,
	    struct bgpd_addr *, struct bgpd_addr *,
	    u_int32_t, uint8_t, int, char *, uint8_t, int, char *,
	    uint16_t, uint16_t);
int	pfkey_sa_add(struct bgpd_addr *, struct bgpd_addr *, u_int8_t, char *,
	    u_int32_t *);
int	pfkey_sa_remove(struct bgpd_addr *, struct bgpd_addr *, u_int32_t *);
int	pfkey_md5sig_establish(struct peer *);
int	pfkey_md5sig_remove(struct peer *);
int	pfkey_ipsec_establish(struct peer *);
int	pfkey_ipsec_remove(struct peer *);

#define pfkey_flow(fd, satype, cmd, dir, from, to, sport, dport) \
	pfkey_send(fd, satype, cmd, dir, from, to, \
	    0, 0, 0, NULL, 0, 0, NULL, sport, dport)

int
pfkey_send(int sd, uint8_t satype, uint8_t mtype, uint8_t dir,
    struct bgpd_addr *src, struct bgpd_addr *dst, u_int32_t spi,
    uint8_t aalg, int alen, char *akey, uint8_t ealg, int elen, char *ekey,
    uint16_t sport, uint16_t dport)
{
	struct sadb_msg		smsg;
	struct sadb_sa		sa;
	struct sadb_address	sa_src, sa_dst, sa_peer, sa_smask, sa_dmask;
	struct sadb_key		sa_akey, sa_ekey;
	struct sadb_spirange	sa_spirange;
	struct sadb_protocol	sa_flowtype, sa_protocol;
	struct iovec		iov[IOV_CNT];
	ssize_t			n;
	int			len = 0;
	int			iov_cnt;
	struct sockaddr_storage	ssrc, sdst, speer, smask, dmask;
	struct sockaddr		*saptr;

	if (!pid)
		pid = getpid();

	/* we need clean sockaddr... no ports set */
	bzero(&ssrc, sizeof(ssrc));
	bzero(&smask, sizeof(smask));
	if ((saptr = addr2sa(src, 0)))
		memcpy(&ssrc, saptr, sizeof(ssrc));
	switch (src->aid) {
	case AID_INET:
		memset(&((struct sockaddr_in *)&smask)->sin_addr, 0xff, 32/8);
		break;
	case AID_INET6:
		memset(&((struct sockaddr_in6 *)&smask)->sin6_addr, 0xff,
		    128/8);
		break;
	case AID_UNSPEC:
		ssrc.ss_len = sizeof(struct sockaddr);
		break;
	default:
		return (-1);
	}
	smask.ss_family = ssrc.ss_family;
	smask.ss_len = ssrc.ss_len;

	bzero(&sdst, sizeof(sdst));
	bzero(&dmask, sizeof(dmask));
	if ((saptr = addr2sa(dst, 0)))
		memcpy(&sdst, saptr, sizeof(sdst));
	switch (dst->aid) {
	case AID_INET:
		memset(&((struct sockaddr_in *)&dmask)->sin_addr, 0xff, 32/8);
		break;
	case AID_INET6:
		memset(&((struct sockaddr_in6 *)&dmask)->sin6_addr, 0xff,
		    128/8);
		break;
	case AID_UNSPEC:
		sdst.ss_len = sizeof(struct sockaddr);
		break;
	default:
		return (-1);
	}
	dmask.ss_family = sdst.ss_family;
	dmask.ss_len = sdst.ss_len;

	bzero(&smsg, sizeof(smsg));
	smsg.sadb_msg_version = PF_KEY_V2;
	smsg.sadb_msg_seq = ++sadb_msg_seq;
	smsg.sadb_msg_pid = pid;
	smsg.sadb_msg_len = sizeof(smsg) / 8;
	smsg.sadb_msg_type = mtype;
	smsg.sadb_msg_satype = satype;

	switch (mtype) {
	case SADB_GETSPI:
		bzero(&sa_spirange, sizeof(sa_spirange));
		sa_spirange.sadb_spirange_exttype = SADB_EXT_SPIRANGE;
		sa_spirange.sadb_spirange_len = sizeof(sa_spirange) / 8;
		sa_spirange.sadb_spirange_min = 0x100;
		sa_spirange.sadb_spirange_max = 0xffffffff;
		sa_spirange.sadb_spirange_reserved = 0;
		break;
	case SADB_ADD:
	case SADB_UPDATE:
	case SADB_DELETE:
		bzero(&sa, sizeof(sa));
		sa.sadb_sa_exttype = SADB_EXT_SA;
		sa.sadb_sa_len = sizeof(sa) / 8;
		sa.sadb_sa_replay = 0;
		sa.sadb_sa_spi = htonl(spi);
		sa.sadb_sa_state = SADB_SASTATE_MATURE;
		break;
	case SADB_X_ADDFLOW:
	case SADB_X_DELFLOW:
		bzero(&sa_flowtype, sizeof(sa_flowtype));
		sa_flowtype.sadb_protocol_exttype = SADB_X_EXT_FLOW_TYPE;
		sa_flowtype.sadb_protocol_len = sizeof(sa_flowtype) / 8;
		sa_flowtype.sadb_protocol_direction = dir;
		sa_flowtype.sadb_protocol_proto = SADB_X_FLOW_TYPE_REQUIRE;

		bzero(&sa_protocol, sizeof(sa_protocol));
		sa_protocol.sadb_protocol_exttype = SADB_X_EXT_PROTOCOL;
		sa_protocol.sadb_protocol_len = sizeof(sa_protocol) / 8;
		sa_protocol.sadb_protocol_direction = 0;
		sa_protocol.sadb_protocol_proto = 6;
		break;
	}

	bzero(&sa_src, sizeof(sa_src));
	sa_src.sadb_address_exttype = SADB_EXT_ADDRESS_SRC;
	sa_src.sadb_address_len = (sizeof(sa_src) + ROUNDUP(ssrc.ss_len)) / 8;

	bzero(&sa_dst, sizeof(sa_dst));
	sa_dst.sadb_address_exttype = SADB_EXT_ADDRESS_DST;
	sa_dst.sadb_address_len = (sizeof(sa_dst) + ROUNDUP(sdst.ss_len)) / 8;

	sa.sadb_sa_auth = aalg;
	sa.sadb_sa_encrypt = SADB_X_EALG_AES; /* XXX */

	switch (mtype) {
	case SADB_ADD:
	case SADB_UPDATE:
		bzero(&sa_akey, sizeof(sa_akey));
		sa_akey.sadb_key_exttype = SADB_EXT_KEY_AUTH;
		sa_akey.sadb_key_len = (sizeof(sa_akey) +
		    ((alen + 7) / 8) * 8) / 8;
		sa_akey.sadb_key_bits = 8 * alen;

		bzero(&sa_ekey, sizeof(sa_ekey));
		sa_ekey.sadb_key_exttype = SADB_EXT_KEY_ENCRYPT;
		sa_ekey.sadb_key_len = (sizeof(sa_ekey) +
		    ((elen + 7) / 8) * 8) / 8;
		sa_ekey.sadb_key_bits = 8 * elen;

		break;
	case SADB_X_ADDFLOW:
	case SADB_X_DELFLOW:
		/* sa_peer always points to the remote machine */
		if (dir == IPSP_DIRECTION_IN) {
			speer = ssrc;
			sa_peer = sa_src;
		} else {
			speer = sdst;
			sa_peer = sa_dst;
		}
		sa_peer.sadb_address_exttype = SADB_EXT_ADDRESS_DST;
		sa_peer.sadb_address_len =
		    (sizeof(sa_peer) + ROUNDUP(speer.ss_len)) / 8;

		/* for addflow we also use src/dst as the flow destination */
		sa_src.sadb_address_exttype = SADB_X_EXT_SRC_FLOW;
		sa_dst.sadb_address_exttype = SADB_X_EXT_DST_FLOW;

		bzero(&smask, sizeof(smask));
		switch (src->aid) {
		case AID_INET:
			smask.ss_len = sizeof(struct sockaddr_in);
			smask.ss_family = AF_INET;
			memset(&((struct sockaddr_in *)&smask)->sin_addr,
			    0xff, 32/8);
			if (sport) {
				((struct sockaddr_in *)&ssrc)->sin_port =
				    htons(sport);
				((struct sockaddr_in *)&smask)->sin_port =
				    htons(0xffff);
			}
			break;
		case AID_INET6:
			smask.ss_len = sizeof(struct sockaddr_in6);
			smask.ss_family = AF_INET6;
			memset(&((struct sockaddr_in6 *)&smask)->sin6_addr,
			    0xff, 128/8);
			if (sport) {
				((struct sockaddr_in6 *)&ssrc)->sin6_port =
				    htons(sport);
				((struct sockaddr_in6 *)&smask)->sin6_port =
				    htons(0xffff);
			}
			break;
		}
		bzero(&dmask, sizeof(dmask));
		switch (dst->aid) {
		case AID_INET:
			dmask.ss_len = sizeof(struct sockaddr_in);
			dmask.ss_family = AF_INET;
			memset(&((struct sockaddr_in *)&dmask)->sin_addr,
			    0xff, 32/8);
			if (dport) {
				((struct sockaddr_in *)&sdst)->sin_port =
				    htons(dport);
				((struct sockaddr_in *)&dmask)->sin_port =
				    htons(0xffff);
			}
			break;
		case AID_INET6:
			dmask.ss_len = sizeof(struct sockaddr_in6);
			dmask.ss_family = AF_INET6;
			memset(&((struct sockaddr_in6 *)&dmask)->sin6_addr,
			    0xff, 128/8);
			if (dport) {
				((struct sockaddr_in6 *)&sdst)->sin6_port =
				    htons(dport);
				((struct sockaddr_in6 *)&dmask)->sin6_port =
				    htons(0xffff);
			}
			break;
		}

		bzero(&sa_smask, sizeof(sa_smask));
		sa_smask.sadb_address_exttype = SADB_X_EXT_SRC_MASK;
		sa_smask.sadb_address_len =
		    (sizeof(sa_smask) + ROUNDUP(smask.ss_len)) / 8;

		bzero(&sa_dmask, sizeof(sa_dmask));
		sa_dmask.sadb_address_exttype = SADB_X_EXT_DST_MASK;
		sa_dmask.sadb_address_len =
		    (sizeof(sa_dmask) + ROUNDUP(dmask.ss_len)) / 8;
		break;
	}

	iov_cnt = 0;

	/* msghdr */
	iov[iov_cnt].iov_base = &smsg;
	iov[iov_cnt].iov_len = sizeof(smsg);
	iov_cnt++;

	switch (mtype) {
	case SADB_ADD:
	case SADB_UPDATE:
	case SADB_DELETE:
		/* SA hdr */
		iov[iov_cnt].iov_base = &sa;
		iov[iov_cnt].iov_len = sizeof(sa);
		smsg.sadb_msg_len += sa.sadb_sa_len;
		iov_cnt++;
		break;
	case SADB_GETSPI:
		/* SPI range */
		iov[iov_cnt].iov_base = &sa_spirange;
		iov[iov_cnt].iov_len = sizeof(sa_spirange);
		smsg.sadb_msg_len += sa_spirange.sadb_spirange_len;
		iov_cnt++;
		break;
	case SADB_X_ADDFLOW:
		/* sa_peer always points to the remote machine */
		iov[iov_cnt].iov_base = &sa_peer;
		iov[iov_cnt].iov_len = sizeof(sa_peer);
		iov_cnt++;
		iov[iov_cnt].iov_base = &speer;
		iov[iov_cnt].iov_len = ROUNDUP(speer.ss_len);
		smsg.sadb_msg_len += sa_peer.sadb_address_len;
		iov_cnt++;

		/* FALLTHROUGH */
	case SADB_X_DELFLOW:
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
		break;
	}

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

	switch (mtype) {
	case SADB_ADD:
	case SADB_UPDATE:
		if (alen) {
			/* auth key */
			iov[iov_cnt].iov_base = &sa_akey;
			iov[iov_cnt].iov_len = sizeof(sa_akey);
			iov_cnt++;
			iov[iov_cnt].iov_base = akey;
			iov[iov_cnt].iov_len = ((alen + 7) / 8) * 8;
			smsg.sadb_msg_len += sa_akey.sadb_key_len;
			iov_cnt++;
		}
		if (elen) {
			/* encryption key */
			iov[iov_cnt].iov_base = &sa_ekey;
			iov[iov_cnt].iov_len = sizeof(sa_ekey);
			iov_cnt++;
			iov[iov_cnt].iov_base = ekey;
			iov[iov_cnt].iov_len = ((elen + 7) / 8) * 8;
			smsg.sadb_msg_len += sa_ekey.sadb_key_len;
			iov_cnt++;
		}
		break;
	}

	len = smsg.sadb_msg_len * 8;
	do {
		n = writev(sd, iov, iov_cnt);
	} while (n == -1 && (errno == EAGAIN || errno == EINTR));

	if (n == -1) {
		log_warn("writev (%d/%d)", iov_cnt, len);
		return (-1);
	}

	return (0);
}

int
pfkey_read(int sd, struct sadb_msg *h)
{
	struct sadb_msg hdr;

	if (recv(sd, &hdr, sizeof(hdr), MSG_PEEK) != sizeof(hdr)) {
		if (errno == EAGAIN || errno == EINTR)
			return (0);
		log_warn("pfkey peek");
		return (-1);
	}

	/* XXX: Only one message can be outstanding. */
	if (hdr.sadb_msg_seq == sadb_msg_seq &&
	    hdr.sadb_msg_pid == pid) {
		if (h)
			bcopy(&hdr, h, sizeof(hdr));
		return (0);
	}

	/* not ours, discard */
	if (read(sd, &hdr, sizeof(hdr)) == -1) {
		if (errno == EAGAIN || errno == EINTR)
			return (0);
		log_warn("pfkey read");
		return (-1);
	}

	return (1);
}

int
pfkey_reply(int sd, u_int32_t *spi)
{
	struct sadb_msg hdr, *msg;
	struct sadb_ext *ext;
	struct sadb_sa *sa;
	u_int8_t *data;
	ssize_t len;
	int rv;

	do {
		rv = pfkey_read(sd, &hdr);
		if (rv == -1)
			return (-1);
	} while (rv);

	if (hdr.sadb_msg_errno != 0) {
		errno = hdr.sadb_msg_errno;
		if (errno == ESRCH)
			return (0);
		else {
			log_warn("pfkey");
			return (-1);
		}
	}
	if ((data = reallocarray(NULL, hdr.sadb_msg_len, PFKEY2_CHUNK))
	    == NULL) {
		log_warn("pfkey malloc");
		return (-1);
	}
	len = hdr.sadb_msg_len * PFKEY2_CHUNK;
	if (read(sd, data, len) != len) {
		log_warn("pfkey read");
		freezero(data, len);
		return (-1);
	}

	if (hdr.sadb_msg_type == SADB_GETSPI) {
		if (spi == NULL) {
			freezero(data, len);
			return (0);
		}

		msg = (struct sadb_msg *)data;
		for (ext = (struct sadb_ext *)(msg + 1);
		    (size_t)((u_int8_t *)ext - (u_int8_t *)msg) <
		    msg->sadb_msg_len * PFKEY2_CHUNK;
		    ext = (struct sadb_ext *)((u_int8_t *)ext +
		    ext->sadb_ext_len * PFKEY2_CHUNK)) {
			if (ext->sadb_ext_type == SADB_EXT_SA) {
				sa = (struct sadb_sa *) ext;
				*spi = ntohl(sa->sadb_sa_spi);
				break;
			}
		}
	}
	freezero(data, len);
	return (0);
}

int
pfkey_sa_add(struct bgpd_addr *src, struct bgpd_addr *dst, u_int8_t keylen,
    char *key, u_int32_t *spi)
{
	if (pfkey_send(fd, SADB_X_SATYPE_TCPSIGNATURE, SADB_GETSPI, 0,
	    src, dst, 0, 0, 0, NULL, 0, 0, NULL, 0, 0) < 0)
		return (-1);
	if (pfkey_reply(fd, spi) < 0)
		return (-1);
	if (pfkey_send(fd, SADB_X_SATYPE_TCPSIGNATURE, SADB_UPDATE, 0,
		src, dst, *spi, 0, keylen, key, 0, 0, NULL, 0, 0) < 0)
		return (-1);
	if (pfkey_reply(fd, NULL) < 0)
		return (-1);
	return (0);
}

int
pfkey_sa_remove(struct bgpd_addr *src, struct bgpd_addr *dst, u_int32_t *spi)
{
	if (pfkey_send(fd, SADB_X_SATYPE_TCPSIGNATURE, SADB_DELETE, 0,
	    src, dst, *spi, 0, 0, NULL, 0, 0, NULL, 0, 0) < 0)
		return (-1);
	if (pfkey_reply(fd, NULL) < 0)
		return (-1);
	*spi = 0;
	return (0);
}

int
pfkey_md5sig_establish(struct peer *p)
{
	sleep(1);

	if (!p->auth.spi_out)
		if (pfkey_sa_add(&p->auth.local_addr, &p->conf.remote_addr,
		    p->conf.auth.md5key_len, p->conf.auth.md5key,
		    &p->auth.spi_out) == -1)
			return (-1);
	if (!p->auth.spi_in)
		if (pfkey_sa_add(&p->conf.remote_addr, &p->auth.local_addr,
		    p->conf.auth.md5key_len, p->conf.auth.md5key,
		    &p->auth.spi_in) == -1)
			return (-1);

	p->auth.established = 1;
	return (0);
}

int
pfkey_md5sig_remove(struct peer *p)
{
	if (p->auth.spi_out)
		if (pfkey_sa_remove(&p->auth.local_addr, &p->conf.remote_addr,
		    &p->auth.spi_out) == -1)
			return (-1);
	if (p->auth.spi_in)
		if (pfkey_sa_remove(&p->conf.remote_addr, &p->auth.local_addr,
		    &p->auth.spi_in) == -1)
			return (-1);

	p->auth.established = 0;
	return (0);
}

int
pfkey_ipsec_establish(struct peer *p)
{
	uint8_t satype = SADB_SATYPE_ESP;

	switch (p->auth.method) {
	case AUTH_IPSEC_IKE_ESP:
		satype = SADB_SATYPE_ESP;
		break;
	case AUTH_IPSEC_IKE_AH:
		satype = SADB_SATYPE_AH;
		break;
	case AUTH_IPSEC_MANUAL_ESP:
	case AUTH_IPSEC_MANUAL_AH:
		satype = p->auth.method == AUTH_IPSEC_MANUAL_ESP ?
		    SADB_SATYPE_ESP : SADB_SATYPE_AH;
		if (pfkey_send(fd, satype, SADB_ADD, 0,
		    &p->auth.local_addr, &p->conf.remote_addr,
		    p->auth.spi_out,
		    p->conf.auth.auth_alg_out,
		    p->conf.auth.auth_keylen_out,
		    p->conf.auth.auth_key_out,
		    p->conf.auth.enc_alg_out,
		    p->conf.auth.enc_keylen_out,
		    p->conf.auth.enc_key_out,
		    0, 0) < 0)
			return (-1);
		if (pfkey_reply(fd, NULL) < 0)
			return (-1);
		if (pfkey_send(fd, satype, SADB_ADD, 0,
		    &p->conf.remote_addr, &p->auth.local_addr,
		    p->auth.spi_in,
		    p->conf.auth.auth_alg_in,
		    p->conf.auth.auth_keylen_in,
		    p->conf.auth.auth_key_in,
		    p->conf.auth.enc_alg_in,
		    p->conf.auth.enc_keylen_in,
		    p->conf.auth.enc_key_in,
		    0, 0) < 0)
			return (-1);
		if (pfkey_reply(fd, NULL) < 0)
			return (-1);
		break;
	default:
		return (-1);
		break;
	}

	if (pfkey_flow(fd, satype, SADB_X_ADDFLOW, IPSP_DIRECTION_OUT,
	    &p->auth.local_addr, &p->conf.remote_addr, 0, BGP_PORT) < 0)
		return (-1);
	if (pfkey_reply(fd, NULL) < 0)
		return (-1);

	if (pfkey_flow(fd, satype, SADB_X_ADDFLOW, IPSP_DIRECTION_OUT,
	    &p->auth.local_addr, &p->conf.remote_addr, BGP_PORT, 0) < 0)
		return (-1);
	if (pfkey_reply(fd, NULL) < 0)
		return (-1);

	if (pfkey_flow(fd, satype, SADB_X_ADDFLOW, IPSP_DIRECTION_IN,
	    &p->conf.remote_addr, &p->auth.local_addr, 0, BGP_PORT) < 0)
		return (-1);
	if (pfkey_reply(fd, NULL) < 0)
		return (-1);

	if (pfkey_flow(fd, satype, SADB_X_ADDFLOW, IPSP_DIRECTION_IN,
	    &p->conf.remote_addr, &p->auth.local_addr, BGP_PORT, 0) < 0)
		return (-1);
	if (pfkey_reply(fd, NULL) < 0)
		return (-1);

	p->auth.established = 1;
	return (0);
}

int
pfkey_ipsec_remove(struct peer *p)
{
	uint8_t satype;

	switch (p->auth.method) {
	case AUTH_IPSEC_IKE_ESP:
		satype = SADB_SATYPE_ESP;
		break;
	case AUTH_IPSEC_IKE_AH:
		satype = SADB_SATYPE_AH;
		break;
	case AUTH_IPSEC_MANUAL_ESP:
	case AUTH_IPSEC_MANUAL_AH:
		satype = p->auth.method == AUTH_IPSEC_MANUAL_ESP ?
		    SADB_SATYPE_ESP : SADB_SATYPE_AH;
		if (pfkey_send(fd, satype, SADB_DELETE, 0,
		    &p->auth.local_addr, &p->conf.remote_addr,
		    p->auth.spi_out, 0, 0, NULL, 0, 0, NULL,
		    0, 0) < 0)
			return (-1);
		if (pfkey_reply(fd, NULL) < 0)
			return (-1);

		if (pfkey_send(fd, satype, SADB_DELETE, 0,
		    &p->conf.remote_addr, &p->auth.local_addr,
		    p->auth.spi_in, 0, 0, NULL, 0, 0, NULL,
		    0, 0) < 0)
			return (-1);
		if (pfkey_reply(fd, NULL) < 0)
			return (-1);
		break;
	default:
		return (-1);
		break;
	}

	if (pfkey_flow(fd, satype, SADB_X_DELFLOW, IPSP_DIRECTION_OUT,
	    &p->auth.local_addr, &p->conf.remote_addr, 0, BGP_PORT) < 0)
		return (-1);
	if (pfkey_reply(fd, NULL) < 0)
		return (-1);

	if (pfkey_flow(fd, satype, SADB_X_DELFLOW, IPSP_DIRECTION_OUT,
	    &p->auth.local_addr, &p->conf.remote_addr, BGP_PORT, 0) < 0)
		return (-1);
	if (pfkey_reply(fd, NULL) < 0)
		return (-1);

	if (pfkey_flow(fd, satype, SADB_X_DELFLOW, IPSP_DIRECTION_IN,
	    &p->conf.remote_addr, &p->auth.local_addr, 0, BGP_PORT) < 0)
		return (-1);
	if (pfkey_reply(fd, NULL) < 0)
		return (-1);

	if (pfkey_flow(fd, satype, SADB_X_DELFLOW, IPSP_DIRECTION_IN,
	    &p->conf.remote_addr, &p->auth.local_addr, BGP_PORT, 0) < 0)
		return (-1);
	if (pfkey_reply(fd, NULL) < 0)
		return (-1);

	p->auth.established = 0;
	return (0);
}

int
pfkey_establish(struct peer *p)
{
	/*
	 * make sure we keep copies of everything we need to
	 * remove SAs and flows later again, even if the
	 * info in p->conf changed due to reload.
	 * We need: SPIs, method, local_addr, remote_addr.
	 * remote_addr cannot change, so no copy.
	 */
	memcpy(&p->auth.local_addr, &p->conf.local_addr,
	    sizeof(p->auth.local_addr));
	p->auth.method = p->conf.auth.method;
	p->auth.spi_in = p->conf.auth.spi_in;
	p->auth.spi_out = p->conf.auth.spi_out;

	if (!p->auth.method)
		return (0);
	else if (p->auth.method == AUTH_MD5SIG)
		return (pfkey_md5sig_establish(p));
	else
		return (pfkey_ipsec_establish(p));
}

int
pfkey_remove(struct peer *p)
{
	if (!p->auth.established)
		return (0);
	else if (p->auth.method == AUTH_MD5SIG)
		return (pfkey_md5sig_remove(p));
	else
		return (pfkey_ipsec_remove(p));
}

int
pfkey_init(struct bgpd_sysdep *sysdep)
{
	if ((fd = socket(PF_KEY, SOCK_RAW | SOCK_CLOEXEC | SOCK_NONBLOCK,
	    PF_KEY_V2)) == -1) {
		if (errno == EPROTONOSUPPORT) {
			log_warnx("PF_KEY not available, disabling ipsec");
			sysdep->no_pfkey = 1;
			return (-1);
		} else
			fatal("pfkey setup failed");
	}
	return (fd);
}
