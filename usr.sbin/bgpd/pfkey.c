/*	$OpenBSD: pfkey.c,v 1.1 2004/01/26 14:42:47 henning Exp $ */

/*
 * Copyright (c) 2003, 2004 Henning Brauer <henning@openbsd.org>
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

#define	ROUNDUP(x) (((x) + sizeof(u_int64_t) - 1) & ~(sizeof(u_int64_t) - 1))
#define	IOV_CNT	8

static u_int32_t	sadb_msg_seq = 1;

int	send_sa_msg(struct iovec *, int, int);

int
send_sa_msg(struct iovec *iov, int cnt, int len)
{
	struct sadb_msg	sm;
	int		sd;
	ssize_t		n;

	if ((sd = socket(PF_KEY, SOCK_RAW, PF_KEY_V2)) == -1) {
		if (errno == EPROTONOSUPPORT)
			log_warnx("no kernel support for PF_KEY");
		else
			log_warn("socket");
		return (-1);
	}

	if ((n = writev(sd, iov, cnt)) == -1) {
		log_warn("write");
		return (-1);
	}

	if (n != len) {
		log_warn("writev: should=%d has=%d", len, n);
		return (-1);
	}

	if (read(sd, &sm, sizeof(sm)) != sizeof(sm)) {
		log_warn("read");
		return (-1);
	}
	if (sm.sadb_msg_errno != 0) {
		errno = sm.sadb_msg_errno;
		log_warn("pfkey");
		return (-1);
	}
	close(sd);
	return (0);
}

int
pfkey_signature(struct sockaddr *src, struct sockaddr *dst, char *key)
{
	struct sadb_msg		smsg;
	struct sadb_sa		sa;
	struct sadb_address	sa_src, sa_dst;
	struct sadb_key		sa_key;
	struct iovec		iov[IOV_CNT];
	int			klen = 0;
	int			i;
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
	smsg.sadb_msg_type = SADB_ADD;
	smsg.sadb_msg_satype = SADB_X_SATYPE_TCPSIGNATURE;

	bzero(&sa, sizeof(sa));
	sa.sadb_sa_exttype = SADB_EXT_SA;
	sa.sadb_sa_len = sizeof(sa) / 8;
	sa.sadb_sa_replay = 0;
	sa.sadb_sa_spi = 0;
	sa.sadb_sa_state = SADB_SASTATE_MATURE;

	bzero(&sa_src, sizeof(sa_src));
	sa_src.sadb_address_exttype = SADB_EXT_ADDRESS_SRC;
	sa_src.sadb_address_len = (sizeof(sa_src) + ROUNDUP(ssrc.ss_len)) / 8;

	bzero(&sa_dst, sizeof(sa_dst));
	sa_dst.sadb_address_exttype = SADB_EXT_ADDRESS_DST;
	sa_dst.sadb_address_len = (sizeof(sa_dst) + ROUNDUP(sdst.ss_len)) / 8;

	bzero(&sa_key, sizeof(sa_key));
	klen = strlen(key) / 2;
	sa_key.sadb_key_exttype = SADB_EXT_KEY_AUTH;
	sa_key.sadb_key_len = (sizeof(sa_key) + ((klen + 7) / 8) * 8) / 8;
	sa_key.sadb_key_bits = 8 * klen;

	for (i = 0; i < klen; i++) {
		s[0] = key[2*i];
		s[1] = key[2*i + 1];
		s[2] = 0;
		if (!isxdigit(s[0]) || !isxdigit(s[1])) {
			log_warnx("espkey must be specified in hex");
			return (-1);
		}
		realkey[i] = strtoul(s, NULL, 16);
	}

	/* msghdr */
	iov[0].iov_base = &smsg;
	iov[0].iov_len = sizeof(smsg);

	/* SA hdr */
	iov[1].iov_base = &sa;
	iov[1].iov_len = sizeof(sa);
	smsg.sadb_msg_len += sa.sadb_sa_len;

	/* dest addr */
	iov[2].iov_base = &sa_dst;
	iov[2].iov_len = sizeof(sa_dst);
	iov[3].iov_base = &sdst;
	iov[3].iov_len = ROUNDUP(sdst.ss_len);
	smsg.sadb_msg_len += sa_dst.sadb_address_len;

	/* src addr */
	iov[4].iov_base = &sa_src;
	iov[4].iov_len = sizeof(sa_src);
	iov[5].iov_base = &ssrc;
	iov[5].iov_len = ROUNDUP(ssrc.ss_len);
	smsg.sadb_msg_len += sa_src.sadb_address_len;

	/* auth key */
	iov[6].iov_base = &sa_key;
	iov[6].iov_len = sizeof(sa_key);
	iov[7].iov_base = realkey;
	iov[7].iov_len = ((klen + 7) / 8) * 8;
	smsg.sadb_msg_len += sa_key.sadb_key_len;

	return (send_sa_msg(iov, IOV_CNT, smsg.sadb_msg_len * 8));
}
