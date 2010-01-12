/*	$OpenBSD: filter.c,v 1.7 2010/01/12 03:20:51 mcbride Exp $ */

/*
 * Copyright (c) 2004, 2005 Camiel Dobbelaar, <cd@sentia.nl>
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

#include <syslog.h>

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/pfvar.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "filter.h"

/* From netinet/in.h, but only _KERNEL_ gets them. */
#define satosin(sa)	((struct sockaddr_in *)(sa))
#define satosin6(sa)	((struct sockaddr_in6 *)(sa))

enum { TRANS_FILTER = 0, TRANS_NAT, TRANS_RDR, TRANS_SIZE };

int prepare_rule(u_int32_t, struct sockaddr *, struct sockaddr *,
    u_int16_t, u_int8_t);
int server_lookup4(struct sockaddr_in *, struct sockaddr_in *,
    struct sockaddr_in *, u_int8_t);
int server_lookup6(struct sockaddr_in6 *, struct sockaddr_in6 *,
    struct sockaddr_in6 *, u_int8_t);

static struct pfioc_rule	pfr;
static struct pfioc_trans	pft;
static struct pfioc_trans_e	pfte;
static int dev, rule_log;
static char *qname;

int
add_filter(u_int32_t id, u_int8_t dir, struct sockaddr *src,
    struct sockaddr *dst, u_int16_t d_port, u_int8_t proto)
{
	if (!src || !dst || !d_port || !proto) {
		errno = EINVAL;
		return (-1);
	}

	if (prepare_rule(id, src, dst, d_port, proto) == -1)
		return (-1);

	pfr.rule.direction = dir;
	if (ioctl(dev, DIOCADDRULE, &pfr) == -1)
		return (-1);

	return (0);
}

int
add_rdr(u_int32_t id, struct sockaddr *src, struct sockaddr *dst,
    u_int16_t d_port, struct sockaddr *rdr, u_int16_t rdr_port, u_int8_t proto)
{
	if (!src || !dst || !d_port || !rdr || !rdr_port || !proto ||
	    (src->sa_family != rdr->sa_family)) {
		errno = EINVAL;
		return (-1);
	}

	if (prepare_rule(id, src, dst, d_port, proto) == -1)
		return (-1);

	if (rdr->sa_family == AF_INET) {
		memcpy(&pfr.rule.rdr.addr.v.a.addr.v4,
		    &satosin(rdr)->sin_addr.s_addr, 4);
		memset(&pfr.rule.rdr.addr.v.a.mask.addr8, 255, 4);
	} else {
		memcpy(&pfr.rule.rdr.addr.v.a.addr.v6,
		    &satosin6(rdr)->sin6_addr.s6_addr, 16);
		memset(&pfr.rule.rdr.addr.v.a.mask.addr8, 255, 16);
	}

	pfr.rule.rdr.proxy_port[0] = rdr_port;
	if (ioctl(dev, DIOCADDRULE, &pfr) == -1)
		return (-1);

	return (0);
}

int
do_commit(void)
{
	if (ioctl(dev, DIOCXCOMMIT, &pft) == -1)
		return (-1);

	return (0);
}

int
do_rollback(void)
{
	if (ioctl(dev, DIOCXROLLBACK, &pft) == -1)
		return (-1);
	
	return (0);
}

void
init_filter(char *opt_qname, int opt_verbose)
{
	struct pf_status status;

	qname = opt_qname;

	if (opt_verbose == 1)
		rule_log = PF_LOG;
	else if (opt_verbose == 2)
		rule_log = PF_LOG_ALL;

	dev = open("/dev/pf", O_RDWR);	
	if (dev == -1) {
		syslog(LOG_ERR, "can't open /dev/pf");
		exit(1);
	}
	if (ioctl(dev, DIOCGETSTATUS, &status) == -1) {
		syslog(LOG_ERR, "DIOCGETSTATUS");
		exit(1);
	}
	if (!status.running) {
		syslog(LOG_ERR, "pf is disabled");
		exit(1);
	}
}

int
prepare_commit(u_int32_t id)
{
	char an[PF_ANCHOR_NAME_SIZE];

	memset(&pft, 0, sizeof pft);
	memset(&pfte, 0, sizeof pfte);
	pft.size = 1;
	pft.esize = sizeof pfte;
	pft.array = &pfte;

	snprintf(an, PF_ANCHOR_NAME_SIZE, "%s/%d.%d", FTP_PROXY_ANCHOR,
	    getpid(), id);
	strlcpy(pfte.anchor, an, PF_ANCHOR_NAME_SIZE);
	pfte.type = PF_TRANS_RULESET;

	if (ioctl(dev, DIOCXBEGIN, &pft) == -1)
		return (-1);

	return (0);
}
	
int
prepare_rule(u_int32_t id, struct sockaddr *src,
    struct sockaddr *dst, u_int16_t d_port, u_int8_t proto)
{
	char an[PF_ANCHOR_NAME_SIZE];

	if ((src->sa_family != AF_INET && src->sa_family != AF_INET6) ||
	    (src->sa_family != dst->sa_family)) {
	    	errno = EPROTONOSUPPORT;
		return (-1);
	}

	memset(&pfr, 0, sizeof pfr);
	snprintf(an, PF_ANCHOR_NAME_SIZE, "%s/%d.%d", FTP_PROXY_ANCHOR,
	    getpid(), id);
	strlcpy(pfr.anchor, an, PF_ANCHOR_NAME_SIZE);

	pfr.ticket = pfte.ticket;

	/* Generic for all rule types. */
	pfr.rule.af = src->sa_family;
	pfr.rule.proto = proto;
	pfr.rule.src.addr.type = PF_ADDR_ADDRMASK;
	pfr.rule.dst.addr.type = PF_ADDR_ADDRMASK;
	if (src->sa_family == AF_INET) {
		memcpy(&pfr.rule.src.addr.v.a.addr.v4,
		    &satosin(src)->sin_addr.s_addr, 4);
		memset(&pfr.rule.src.addr.v.a.mask.addr8, 255, 4);
		memcpy(&pfr.rule.dst.addr.v.a.addr.v4,
		    &satosin(dst)->sin_addr.s_addr, 4);
		memset(&pfr.rule.dst.addr.v.a.mask.addr8, 255, 4);
	} else {
		memcpy(&pfr.rule.src.addr.v.a.addr.v6,
		    &satosin6(src)->sin6_addr.s6_addr, 16);
		memset(&pfr.rule.src.addr.v.a.mask.addr8, 255, 16);
		memcpy(&pfr.rule.dst.addr.v.a.addr.v6,
		    &satosin6(dst)->sin6_addr.s6_addr, 16);
		memset(&pfr.rule.dst.addr.v.a.mask.addr8, 255, 16);
	}
	pfr.rule.dst.port_op = PF_OP_EQ;
	pfr.rule.dst.port[0] = htons(d_port);
	pfr.rule.action = PF_PASS;
	pfr.rule.quick = 1;
	pfr.rule.log = rule_log;
	pfr.rule.keep_state = 1;
	pfr.rule.flags = (proto == IPPROTO_TCP ? TH_SYN : 0);
	pfr.rule.flagset = (proto == IPPROTO_TCP ?
	    (TH_SYN|TH_ACK|TH_FIN|TH_RST) : 0);
	pfr.rule.max_states = 1;
	if (qname != NULL)
		strlcpy(pfr.rule.qname, qname, sizeof pfr.rule.qname);

	return (0);
}

int
server_lookup(struct sockaddr *client, struct sockaddr *proxy,
    struct sockaddr *server, u_int8_t proto)
{
	if (client->sa_family == AF_INET)
		return (server_lookup4(satosin(client), satosin(proxy),
		    satosin(server), proto));

	if (client->sa_family == AF_INET6)
		return (server_lookup6(satosin6(client), satosin6(proxy),
		    satosin6(server), proto));

	errno = EPROTONOSUPPORT;
	return (-1);
}

int
server_lookup4(struct sockaddr_in *client, struct sockaddr_in *proxy,
    struct sockaddr_in *server, u_int8_t proto)
{
	struct pfioc_natlook pnl;

	memset(&pnl, 0, sizeof pnl);
	pnl.direction = PF_OUT;
	pnl.af = AF_INET;
	pnl.proto = proto;
	memcpy(&pnl.saddr.v4, &client->sin_addr.s_addr, sizeof pnl.saddr.v4);
	memcpy(&pnl.daddr.v4, &proxy->sin_addr.s_addr, sizeof pnl.daddr.v4);
	pnl.sport = client->sin_port;
	pnl.dport = proxy->sin_port;

	if (ioctl(dev, DIOCNATLOOK, &pnl) == -1)
		return (-1);

	memset(server, 0, sizeof(struct sockaddr_in));
	server->sin_len = sizeof(struct sockaddr_in);
	server->sin_family = AF_INET;
	memcpy(&server->sin_addr.s_addr, &pnl.rdaddr.v4,
	    sizeof server->sin_addr.s_addr);
	server->sin_port = pnl.rdport;

	return (0);
}

int
server_lookup6(struct sockaddr_in6 *client, struct sockaddr_in6 *proxy,
    struct sockaddr_in6 *server, u_int8_t proto)
{
	struct pfioc_natlook pnl;

	memset(&pnl, 0, sizeof pnl);
	pnl.direction = PF_OUT;
	pnl.af = AF_INET6;
	pnl.proto = proto;
	memcpy(&pnl.saddr.v6, &client->sin6_addr.s6_addr, sizeof pnl.saddr.v6);
	memcpy(&pnl.daddr.v6, &proxy->sin6_addr.s6_addr, sizeof pnl.daddr.v6);
	pnl.sport = client->sin6_port;
	pnl.dport = proxy->sin6_port;
	
	if (ioctl(dev, DIOCNATLOOK, &pnl) == -1)
		return (-1);

	memset(server, 0, sizeof(struct sockaddr_in6));
	server->sin6_len = sizeof(struct sockaddr_in6);
	server->sin6_family = AF_INET6;
	memcpy(&server->sin6_addr.s6_addr, &pnl.rdaddr.v6,
	    sizeof server->sin6_addr);
	server->sin6_port = pnl.rdport;

	return (0);
}
