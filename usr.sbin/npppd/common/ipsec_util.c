/*-
 * Copyright 2007, 2009
 *	Internet Initiative Japan Inc.  All rights reserved.
 */
/* $Id: ipsec_util.c,v 1.3 2010/07/02 21:20:57 yasuoka Exp $ */
/*@file IPsec related utility functions */
/*
 * RFC 2367 PF_KEY Key Management API, Version 2
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <net/pfkeyv2.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ipsec_util.h"
#include "ipsec_util_local.h"

/**
 * Delete the IPsec-SA for transport-mode ESP that matches specified sock and
 * peer.
 * <p>
 * For deleting IPsec-SA for NAT-T, port numbers and protocol must
 * be specified.</p>
 *
 * @param sock	localy bounded address of the IPsec-SA.
 * @param peer	remote address of the IPsec-SA.
 * @param proto	protocol of IPsec-SA.  Specify this only if IPsec-SA is for
 * NAT-T peer.
 * @param dir	IPsec-SA's direction by choosing
 * 	{@link ::IPSEC_UTIL_DIRECTION_IN}, {@link ::IPSEC_UTIL_DIRECTION_OUT}
 *	or {@link ::IPSEC_UTIL_DIRECTION_BOTH}
 * @return	0 if the function success, otherwise return non-zero value;
 */
int
ipsec_util_purge_transport_sa(struct sockaddr *sock, struct sockaddr *peer,
    int proto, int dir)
{
	int key_sock;
	struct timeval tv;
	struct sadb_del_args del_in, del_out;

	/*
	 * Assumes address family is (AF_INET|AF_INET6) and has valid length
	 */
	if (sock == NULL || peer == NULL ||
	    !sockaddr_is_valid(peer) || !sockaddr_is_valid(peer))
		return -1;

	if ((key_sock = socket(PF_KEY, SOCK_RAW, PF_KEY_V2)) < 0)
		return -1;

	tv = KEYSOCK_RCVTIMEO;
	if (setsockopt(key_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) != 0)
		goto fail;

	del_in.is_valid = del_out.is_valid = 0;
	if (delete_prepare(key_sock, sock, peer, proto, &del_in, &del_out) != 0)
		goto fail;

	if (del_in.is_valid && (dir & IPSEC_UTIL_DIRECTION_IN) != 0) {
		if (send_sadb_delete(key_sock, &del_in))
			goto fail;
	}
	if (del_out.is_valid && (dir & IPSEC_UTIL_DIRECTION_OUT) != 0) {
		if (send_sadb_delete(key_sock, &del_out))
			goto fail;
	}
	close(key_sock);

	return 0;

fail:
	close(key_sock);

	return -1;
}

/***********************************************************************
 * private functions
 ***********************************************************************/
static void
ipsec_util_prepare(void)
{

	/*
	 * for sadb_msg_seq.  As RFC 2367, it must be used to uniquely
	 * identify request to a proccess.
	 */
	while (++ipsec_util_seq == 0)
		/* empty */;

	if (ipsec_util_pid == -1)
		ipsec_util_pid = getpid();
}

/*
 * Find IPsec-SA to delete using SADB_DUMP
 */
static int
delete_prepare(int key_sock, struct sockaddr *sock, struct sockaddr *peer,
    int proto, struct sadb_del_args *in, struct sadb_del_args *out)
{
	int sz, dump_end, res_count;
	struct sadb_msg req_msg = {
		.sadb_msg_version = PF_KEY_V2,
		.sadb_msg_type = SADB_DUMP,
		.sadb_msg_satype = SADB_SATYPE_ESP,
		.sadb_msg_len = PFKEY_UNIT64(sizeof(struct sadb_msg))
	}, *res_msg;
	u_char buffer[2048];

	/* Dump the SADB to search the SA that matches sock/peer.  */
	ipsec_util_prepare();
	req_msg.sadb_msg_seq = ipsec_util_seq;
	req_msg.sadb_msg_pid = ipsec_util_pid;
	sz = send(key_sock, &req_msg, sizeof(req_msg), 0);
	if (sz <= 0)
		return -1;

	for (res_count = 0, dump_end = 0; !dump_end;) {
		int off = 0;
		uint32_t spi;
		struct sadb_ext *res_ext;
		struct sadb_address *res_src, *res_dst;

		sz = recv(key_sock, buffer, sizeof(buffer), 0);
		if (sz == 0 && res_count == 0)
			return 0;	/* empty */
		if (sz <= 0)
			return -1;
		if (sz < sizeof(struct sadb_msg))
			return -1;
		res_msg = (struct sadb_msg *)buffer;
		if (res_msg->sadb_msg_errno != 0) {
			if (res_msg->sadb_msg_errno == ENOENT)
				return 0;
			return -1;
		}

		dump_end = (res_msg->sadb_msg_seq == 0)? 1 : 0;
		if (res_msg->sadb_msg_version != req_msg.sadb_msg_version ||
		    res_msg->sadb_msg_type != req_msg.sadb_msg_type ||
		    res_msg->sadb_msg_pid != req_msg.sadb_msg_pid)
			continue;
		res_count++;

		spi = 0; res_src = res_dst = NULL;
		for (off = sizeof(struct sadb_msg); off < sz;) {
			res_ext = (struct sadb_ext *)(buffer + off);
			off += PFKEY_UNUNIT64(res_ext->sadb_ext_len);

			switch (res_ext->sadb_ext_type) {
			case SADB_EXT_SA:
				if (((struct sadb_sa *)res_ext)->sadb_sa_state
				    != SADB_SASTATE_MATURE)
					break;
				spi = ((struct sadb_sa *)res_ext)->sadb_sa_spi;
				break;

			case SADB_EXT_ADDRESS_SRC:
				res_src = (struct sadb_address *)res_ext;
				break;

			case SADB_EXT_ADDRESS_DST:
				res_dst = (struct sadb_address *)res_ext;
				break;
			}
		}
		if (res_src == NULL || res_dst == NULL || spi == 0)
			continue;

		if (address_compar(res_src, sock, proto) == 0 &&
		    address_compar(res_dst, peer, proto) == 0) {
			(void)sadb_del_args_init(out, spi, res_src, res_dst,
			    proto);
			/* continue anyway */
		} else
		if (address_compar(res_src, peer, proto) == 0 &&
		    address_compar(res_dst, sock, proto) == 0) {
			(void)sadb_del_args_init(in, spi, res_src, res_dst,
			    proto);
			/* continue anyway */
		}
	}

	return 0;
}

static int
send_sadb_delete(int key_sock, struct sadb_del_args *args)
{
	int i;

	for (i = 0; i < args->spiidx; i++) {
		int iovidx, sz;
		struct iovec iov[10];
		struct msghdr msg;
		struct sadb_msg req_msg = {
			.sadb_msg_version = PF_KEY_V2,
			.sadb_msg_type = SADB_DELETE,
			.sadb_msg_satype = SADB_SATYPE_ESP
		}, *res_msg;
		struct sadb_sa sa;
		u_char buffer[1024];

		ipsec_util_prepare();
		iovidx = 0;
		req_msg.sadb_msg_seq = ipsec_util_seq;
		req_msg.sadb_msg_pid = ipsec_util_pid;
		req_msg.sadb_msg_len = PFKEY_UNIT64(sizeof(req_msg)
		    + sizeof(struct sadb_sa)
		    + PFKEY_UNUNIT64(args->src.sadb_address_len)
		    + PFKEY_UNUNIT64(args->dst.sadb_address_len));
		iov[iovidx].iov_base = &req_msg;
		iov[iovidx].iov_len = sizeof(req_msg);
		iovidx++;

		sa.sadb_sa_exttype = SADB_EXT_SA;
		sa.sadb_sa_len = PFKEY_UNIT64(sizeof(struct sadb_sa));
		sa.sadb_sa_spi = args->spi[i];
		iov[iovidx].iov_base = &sa;
		iov[iovidx].iov_len = sizeof(sa);
		iovidx++;

		iov[iovidx].iov_base = &args->src;
		iov[iovidx].iov_len = sizeof(args->src);
		iovidx++;
		iov[iovidx].iov_base = &args->src_sa;
		iov[iovidx].iov_len =
		    PFKEY_ALIGN8(((struct sockaddr *)&args->src_sa)->sa_len);
		iovidx++;

		iov[iovidx].iov_base = &args->dst;
		iov[iovidx].iov_len = sizeof(args->dst);
		iovidx++;
		iov[iovidx].iov_base = &args->dst_sa;
		iov[iovidx].iov_len =
		    PFKEY_ALIGN8(((struct sockaddr *)&args->dst_sa)->sa_len);
		iovidx++;

		memset(&msg, 0, sizeof(msg));
		msg.msg_iov = iov;
		msg.msg_iovlen = iovidx;

		if ((sz = sendmsg(key_sock, &msg, 0)) <= 0)
			return 1;

		if ((sz = recv(key_sock, buffer, sizeof(buffer), 0)) <
		    sizeof(struct sadb_msg))
			return 1;

		res_msg = (struct sadb_msg *)buffer;
		if (res_msg->sadb_msg_pid != req_msg.sadb_msg_pid ||
		    res_msg->sadb_msg_version != req_msg.sadb_msg_version ||
		    res_msg->sadb_msg_type != req_msg.sadb_msg_type ||
		    res_msg->sadb_msg_errno != 0)
			return 1;
	}

	return 0;
}

/***********************************************************************
 * Utility functions
 ***********************************************************************/
static inline int
address_compar(struct sadb_address *sadb, struct sockaddr *sa, int proto)
{
	u_short porta, portb;
	int cmp;
	struct sockaddr *sb = (struct sockaddr *)(sadb + 1);

	if ((cmp = sa->sa_family - sb->sa_family) != 0) return cmp;
	if ((cmp = sa->sa_len - sb->sa_len) != 0) return cmp;
	if (proto != 0 &&
	    (cmp = proto - sadb->sadb_address_proto) != 0) return cmp;

	switch (sa->sa_family) {
	case AF_INET:
		if (sadb->sadb_address_prefixlen != sizeof(struct in_addr) << 3)
			return -1;
		if ((cmp = memcmp(&((struct sockaddr_in *)sa)->sin_addr,
		    &((struct sockaddr_in *)sb)->sin_addr,
		    sizeof(struct in_addr))) != 0)
			return cmp;
		porta = ((struct sockaddr_in *)sa)->sin_port;
		portb = ((struct sockaddr_in *)sb)->sin_port;
		break;

	case AF_INET6:
		if (sadb->sadb_address_prefixlen != sizeof(struct in6_addr) << 3)
			return -1;
		if ((cmp = memcmp(&((struct sockaddr_in6 *)sa)->sin6_addr,
		    &((struct sockaddr_in6 *)sb)->sin6_addr,
		    sizeof(struct in6_addr))) != 0)
			return cmp;
		porta = ((struct sockaddr_in6 *)sa)->sin6_port;
		portb = ((struct sockaddr_in6 *)sb)->sin6_port;
		break;

	default:
		return -1;
	}
	if (porta == 0) {
		if (ntohs(portb) != 500 && portb != 0)
			return porta - portb;
	} else {
		if ((cmp = porta - portb) != 0) return cmp;
	}

	return 0;
}


static int
sadb_del_args_init(struct sadb_del_args *args, uint32_t spi,
    struct sadb_address *src, struct sadb_address *dst, int proto)
{
	if (!args->is_valid) {
		memset(args, 0, sizeof(struct sadb_del_args));

		args->src = *src;
		args->dst = *dst;
		args->src.sadb_address_prefixlen =
		    args->dst.sadb_address_prefixlen = 0;
#define	SADB2SA(_base) ((struct sockaddr *)((_base) + 1))
		memcpy(&args->src_sa, SADB2SA(src), SADB2SA(src)->sa_len);
		memcpy(&args->dst_sa, SADB2SA(dst), SADB2SA(dst)->sa_len);
#undef SADB2SA
		if (proto != 0) {
			args->src.sadb_address_proto = proto;
			args->dst.sadb_address_proto = proto;
		}
		args->is_valid = 1;
	}
	if (args->spiidx < countof(args->spi)) {
		args->spi[args->spiidx++] = spi;
		return 0;
	}

	return 1;
}

static int
sockaddr_is_valid(struct sockaddr *sa)
{
	return
	    ((sa->sa_family == AF_INET &&
		    sa->sa_len == sizeof(struct sockaddr_in)) ||
	    (sa->sa_family == AF_INET6 &&
		    sa->sa_len == sizeof(struct sockaddr_in6)))? 1 : 0;
}
