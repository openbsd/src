/*	$OpenBSD: pfkey.c,v 1.4 2004/01/27 14:12:28 henning Exp $ */

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
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "bgpd.h"

#define	PFKEY2_CHUNK sizeof(u_int64_t)
#define	ROUNDUP(x) (((x) + (PFKEY2_CHUNK - 1)) & ~(PFKEY2_CHUNK - 1))
#define	IOV_CNT	8

static u_int32_t	sadb_msg_seq = 1;

int	pfkey_reply(int, u_int32_t *);
int	pfkey_send(int, uint8_t, struct sockaddr *, struct sockaddr *,
    u_int32_t, char *);

int
pfkey_send(int sd, uint8_t mtype, struct sockaddr *src, struct sockaddr *dst,
    u_int32_t spi, char *key)
{
	struct sadb_msg		smsg;
	struct sadb_sa		sa;
	struct sadb_address	sa_src, sa_dst;
	struct sadb_key		sa_key;
	struct sadb_spirange	sa_spirange;
	struct iovec		iov[IOV_CNT];
	ssize_t			n;
	int			klen = 0;
	int			len = 0;
	int			i;
	int			iov_cnt;
	char			realkey[TCP_SIGN_KEY_LEN];
	char			s[3];
	struct sockaddr_storage	ssrc, sdst;

	/* we need clean sockaddr... no ports set */
	bzero(&ssrc, sizeof(ssrc));
	if (src->sa_family == AF_INET) {
		((struct sockaddr_in *)&ssrc)->sin_addr.s_addr =
		    ((struct sockaddr_in *)src)->sin_addr.s_addr;
		ssrc.ss_len = sizeof(struct sockaddr_in);
		ssrc.ss_family = AF_INET;
	} else if (src->sa_family == AF_INET6) {
		memcpy(&((struct sockaddr_in6 *)&ssrc)->sin6_addr,
		    &((struct sockaddr_in6 *)src)->sin6_addr,
		    sizeof(struct in6_addr));
		ssrc.ss_len = sizeof(struct sockaddr_in6);
		ssrc.ss_family = AF_INET6;
	}

	bzero(&sdst, sizeof(sdst));
	if (dst->sa_family == AF_INET) {
		((struct sockaddr_in *)&sdst)->sin_addr.s_addr =
		    ((struct sockaddr_in *)dst)->sin_addr.s_addr;
		sdst.ss_len = sizeof(struct sockaddr_in);
		sdst.ss_family = AF_INET;
	} else if (dst->sa_family == AF_INET6) {
		memcpy(&((struct sockaddr_in6 *)&sdst)->sin6_addr,
		    &((struct sockaddr_in6 *)dst)->sin6_addr,
		    sizeof(struct in6_addr));
		sdst.ss_len = sizeof(struct sockaddr_in6);
		sdst.ss_family = AF_INET6;
	}

	bzero(&smsg, sizeof(smsg));
	smsg.sadb_msg_version = PF_KEY_V2;
	smsg.sadb_msg_seq = sadb_msg_seq++;
	smsg.sadb_msg_pid = getpid();
	smsg.sadb_msg_len = sizeof(smsg) / 8;
	smsg.sadb_msg_type = mtype;
	smsg.sadb_msg_satype = SADB_X_SATYPE_TCPSIGNATURE;

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
		bzero(&sa, sizeof(sa));
		sa.sadb_sa_exttype = SADB_EXT_SA;
		sa.sadb_sa_len = sizeof(sa) / 8;
		sa.sadb_sa_replay = 0;
		sa.sadb_sa_spi = spi;
		sa.sadb_sa_state = SADB_SASTATE_MATURE;

		bzero(&sa_key, sizeof(sa_key));
		klen = strlen(key) / 2;
		sa_key.sadb_key_exttype = SADB_EXT_KEY_AUTH;
		sa_key.sadb_key_len = (sizeof(sa_key) +
		    ((klen + 7) / 8) * 8) / 8;
		sa_key.sadb_key_bits = 8 * klen;

		for (i = 0; i < klen; i++) {
			s[0] = key[2*i];
			s[1] = key[2*i + 1];
			s[2] = 0;
			if (!isxdigit(s[0]) || !isxdigit(s[1])) {
				log_warnx("tcpmd5 must be specified in hex");
				return (-1);
			}
			realkey[i] = strtoul(s, NULL, 16);
		}
		break;
	}

	bzero(&sa_src, sizeof(sa_src));
	sa_src.sadb_address_exttype = SADB_EXT_ADDRESS_SRC;
	sa_src.sadb_address_len = (sizeof(sa_src) + ROUNDUP(ssrc.ss_len)) / 8;

	bzero(&sa_dst, sizeof(sa_dst));
	sa_dst.sadb_address_exttype = SADB_EXT_ADDRESS_DST;
	sa_dst.sadb_address_len = (sizeof(sa_dst) + ROUNDUP(sdst.ss_len)) / 8;

	iov_cnt = 0;

	/* msghdr */
	iov[iov_cnt].iov_base = &smsg;
	iov[iov_cnt].iov_len = sizeof(smsg);
	iov_cnt++;

	switch (mtype) {
	case SADB_ADD:
	case SADB_UPDATE:
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
		/* auth key */
		iov[iov_cnt].iov_base = &sa_key;
		iov[iov_cnt].iov_len = sizeof(sa_key);
		iov_cnt++;
		iov[iov_cnt].iov_base = realkey;
		iov[iov_cnt].iov_len = ((klen + 7) / 8) * 8;
		smsg.sadb_msg_len += sa_key.sadb_key_len;
		iov_cnt++;
		break;
	}

	len = smsg.sadb_msg_len * 8;
	if ((n = writev(sd, iov, iov_cnt)) == -1) {
		log_warn("writev (%d/%d)", iov_cnt, len);
		return (-1);
	}

	if (n != len) {
		log_warn("writev: should=%d has=%d", len, n);
		return (-1);
	}

	return (0);
}

int
pfkey_reply(int sd, u_int32_t *spip)
{
	struct sadb_msg hdr, *msg;
	struct sadb_ext *ext;
	struct sadb_sa *sa;
	u_int8_t *data;
	ssize_t len;

	if (recv(sd, &hdr, sizeof(hdr), MSG_PEEK) != sizeof(hdr)) {
		log_warn("pfkey peek");
		return (-1);
	}
	if (hdr.sadb_msg_errno != 0) {
		errno = hdr.sadb_msg_errno;
		log_warn("pfkey");
		return (-1);
	}
	if (spip == NULL)
		return (0);
	if (hdr.sadb_msg_type != SADB_GETSPI)
		return (-1);
	len = hdr.sadb_msg_len * PFKEY2_CHUNK;
	if ((data = malloc(len)) == NULL) {
		log_warn("pfkey malloc");
		return (-1);
	}
	if (read(sd, data, len) != len) {
		log_warn("pfkey read");
		return (-1);
	}
	msg = (struct sadb_msg *)data;
	for (ext = (struct sadb_ext *)(msg + 1);
	    (size_t)((u_int8_t *)ext - (u_int8_t *)msg) <
	    msg->sadb_msg_len * PFKEY2_CHUNK;
	    ext = (struct sadb_ext *)((u_int8_t *)ext +
	    ext->sadb_ext_len * PFKEY2_CHUNK)) {
		if (ext->sadb_ext_type == SADB_EXT_SA) {
			sa = (struct sadb_sa *) ext;
			*spip = sa->sadb_sa_spi;
			break;
		}
	}
	memset(data, len, 0);
	free(data);
	return (0);
}

int
pfkey_setkey(struct sockaddr *src, struct sockaddr *dst, char *key)
{
	u_int32_t	spi = 0;
	int		sd;
	int		ret = -1;

	if ((sd = socket(PF_KEY, SOCK_RAW, PF_KEY_V2)) == -1) {
		if (errno == EPROTONOSUPPORT)
			log_warnx("no kernel support for PF_KEY");
		else
			log_warn("socket");
		return (-1);
	}
	if (pfkey_send(sd, SADB_GETSPI, src, dst, 0, NULL) < 0)
		goto done;
	if (pfkey_reply(sd, &spi) < 0)
		goto done;
	if (pfkey_send(sd, SADB_UPDATE, src, dst, spi, key) < 0)
		goto done;
	if (pfkey_reply(sd, NULL) < 0)
		goto done;
	ret = 0;
done:
	close(sd);
	return (ret);
}
