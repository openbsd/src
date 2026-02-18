/*
 * Copyright (c) 2026 Christoph Liebender <christoph@liebender.dev>
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

#include "relayd.h"

#define PROXY_V2_CMD_PROXY 0x01

#define PROXY_V2_FAM_UNSPEC 0x00
#define PROXY_V2_FAM_TCP4   0x11
#define PROXY_V2_FAM_UDP4   0x12
#define PROXY_V2_FAM_TCP6   0x21
#define PROXY_V2_FAM_UDP6   0x22

static const u_int8_t PROXY_V2_SIG[12] = {
	0x0D, 0x0A, 0x0D, 0x0A, 0x00, 0x0D, 0x0A, 0x51, 0x55, 0x49, 0x54, 0x0A
};

struct proxy_v2_hdr {
	u_int8_t sig[12];
	u_int8_t ver_cmd;
	u_int8_t fam;
	u_int16_t len;
};

union proxy_v2_addr {
	struct {
		u_int32_t src_addr;
		u_int32_t dst_addr;
		in_port_t src_port;
		in_port_t dst_port;
	} ipv4_addr;
	struct {
		u_int8_t  src_addr[16];
		u_int8_t  dst_addr[16];
		in_port_t src_port;
		in_port_t dst_port;
	} ipv6_addr;
};

int
proxy_protocol_v1(struct rsession *con, struct evbuffer *dstout)
{
	char ibuf[128], obuf[128];
	const char *proxyproto;
	int ret;

	bzero(&ibuf, sizeof(ibuf));
	bzero(&obuf, sizeof(obuf));

	if (print_host(&con->se_in.ss, ibuf, sizeof(ibuf)) == NULL ||
	    print_host(&con->se_sockname, obuf, sizeof(obuf)) == NULL)
		return -1;

	if (con->se_relay->rl_conf.flags & F_UDP)
		proxyproto = "UNKNOWN";
	else {
		switch (con->se_in.ss.ss_family) {
		case AF_INET:
			proxyproto = "TCP4";
			break;
		case AF_INET6:
			proxyproto = "TCP6";
			break;
		default:
			proxyproto = "UNKNOWN";
			break;
		}
	}

	ret = evbuffer_add_printf(dstout,
		"PROXY %s %s %s %d %d\r\n", proxyproto, ibuf, obuf,
		ntohs(con->se_in.port), ntohs(con->se_relay->rl_conf.port));

	return ret == -1 ? -1 : 0;
}

int
proxy_protocol_v2(struct rsession *con, struct evbuffer *dstout)
{
	union proxy_v2_addr 	       addr;
	struct proxy_v2_hdr 	       hdr;
	const struct relay_config     *conf = &con->se_relay->rl_conf;
	const struct sockaddr_storage *srcss = &con->se_in.ss;
	const struct sockaddr_storage *dstss = &con->se_sockname;
	int 			       error;
	in_port_t		       srcport = con->se_in.port;
	in_port_t		       dstport = conf->port;
	u_int16_t 		       len;

	bcopy(PROXY_V2_SIG, hdr.sig, sizeof(hdr.sig));
	hdr.ver_cmd = 0x20 | PROXY_V2_CMD_PROXY;

	switch (dstss->ss_family) {
	case AF_INET:
		hdr.fam = (conf->flags & F_UDP) ?
			PROXY_V2_FAM_UDP4 : PROXY_V2_FAM_TCP4;
		len = sizeof(addr.ipv4_addr);
		addr.ipv4_addr.src_addr =
		    ((const struct sockaddr_in *)srcss)->sin_addr.s_addr;
		addr.ipv4_addr.dst_addr =
		    ((const struct sockaddr_in *)dstss)->sin_addr.s_addr;
		addr.ipv4_addr.src_port = srcport;
		addr.ipv4_addr.dst_port = dstport;
		break;
	case AF_INET6:
		hdr.fam = (conf->flags & F_UDP) ?
			PROXY_V2_FAM_UDP6 : PROXY_V2_FAM_TCP6;
		len = sizeof(addr.ipv6_addr);
		bcopy(&((const struct sockaddr_in6 *)srcss)->sin6_addr,
		    addr.ipv6_addr.src_addr, sizeof(addr.ipv6_addr.src_addr));
		bcopy(&((const struct sockaddr_in6 *)dstss)->sin6_addr,
		    addr.ipv6_addr.dst_addr, sizeof(addr.ipv6_addr.dst_addr));
		addr.ipv6_addr.src_port = srcport;
		addr.ipv6_addr.dst_port = dstport;
		break;
	default:
		hdr.fam = 0x00;
		len = 0;
		break;
	}

	hdr.len = htons(len);

	if ((error = evbuffer_add(dstout, &hdr, sizeof(hdr))) != 0)
		return error;

	if (len > 0 && (error = evbuffer_add(dstout, &addr, len)) != 0)
		return error;

	return 0;
}
