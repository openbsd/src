/*	$OpenBSD: relay_udp.c,v 1.7 2008/01/31 12:12:50 thib Exp $	*/

/*
 * Copyright (c) 2007 Reyk Floeter <reyk@openbsd.org>
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
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/tree.h>
#include <sys/hash.h>

#include <net/if.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <err.h>
#include <pwd.h>
#include <event.h>
#include <fnmatch.h>

#include <openssl/ssl.h>

#include "relayd.h"

extern volatile sig_atomic_t relay_sessions;
extern objid_t relay_conid;
extern int proc_id;
extern struct imsgbuf *ibuf_pfe;
extern int debug;

extern void	 relay_close(struct session *, const char *);
extern void	 relay_natlook(int, short, void *);
extern void	 relay_session(struct session *);
extern int	 relay_from_table(struct session *);
extern int	 relay_socket_af(struct sockaddr_storage *, in_port_t);
extern int	 relay_cmp_af(struct sockaddr_storage *,
		    struct sockaddr_storage *);

struct relayd *env = NULL;

int		 relay_udp_socket(struct sockaddr_storage *, in_port_t,
		    struct protocol *);
void		 relay_udp_request(struct session *);
void		 relay_udp_timeout(int, short, void *);

void		 relay_dns_log(struct session *, u_int8_t *);
int		 relay_dns_validate(struct relay *, struct sockaddr_storage *,
		    u_int8_t *, size_t, u_int32_t *);
int		 relay_dns_request(struct session *);
void		 relay_dns_response(struct session *, u_int8_t *, size_t);
int		 relay_dns_cmp(struct session *, struct session *);

void
relay_udp_privinit(struct relayd *x_env, struct relay *rlay)
{
	struct protocol		*proto = rlay->rl_proto;

	if (env == NULL)
		env = x_env;

	if (rlay->rl_conf.flags & F_SSL)
		fatalx("ssl over udp is not supported");
	rlay->rl_conf.flags |= F_UDP;

	switch (proto->type) {
	case RELAY_PROTO_DNS:
		proto->validate = relay_dns_validate;
		proto->request = relay_dns_request;
		proto->cmp = relay_dns_cmp;
		break;
	default:
		fatalx("unsupported udp protocol");
		break;
	}
}

int
relay_udp_bind(struct sockaddr_storage *ss, in_port_t port,
    struct protocol *proto)
{
	int s;

	if ((s = relay_udp_socket(ss, port, proto)) == -1)
		return (-1);

	if (bind(s, (struct sockaddr *)ss, ss->ss_len) == -1)
		goto bad;

	return (s);

 bad:
	close(s);
	return (-1);
}

int
relay_udp_socket(struct sockaddr_storage *ss, in_port_t port,
    struct protocol *proto)
{
	int s = -1, val;

	if (relay_socket_af(ss, port) == -1)
		goto bad;

	if ((s = socket(ss->ss_family, SOCK_DGRAM, IPPROTO_UDP)) == -1)
		goto bad;

	/*
	 * Socket options
	 */
	if (fcntl(s, F_SETFL, O_NONBLOCK) == -1)
		goto bad;
	if (proto->tcpflags & TCPFLAG_BUFSIZ) {
		val = proto->tcpbufsiz;
		if (setsockopt(s, SOL_SOCKET, SO_RCVBUF,
		    &val, sizeof(val)) == -1)
			goto bad;
		val = proto->tcpbufsiz;
		if (setsockopt(s, SOL_SOCKET, SO_SNDBUF,
		    &val, sizeof(val)) == -1)
			goto bad;
	}

	/*
	 * IP options
	 */
	if (proto->tcpflags & TCPFLAG_IPTTL) {
		val = (int)proto->tcpipttl;
		if (setsockopt(s, IPPROTO_IP, IP_TTL,
		    &val, sizeof(val)) == -1)
			goto bad;
	}
	if (proto->tcpflags & TCPFLAG_IPMINTTL) {
		val = (int)proto->tcpipminttl;
		if (setsockopt(s, IPPROTO_IP, IP_MINTTL,
		    &val, sizeof(val)) == -1)
			goto bad;
	}

	return (s);

 bad:
	if (s != -1)
		close(s);
	return (-1);
}

void
relay_udp_server(int fd, short sig, void *arg)
{
	struct relay *rlay = (struct relay *)arg;
	struct protocol *proto = rlay->rl_proto;
	struct session *con = NULL;
	struct ctl_natlook *cnl = NULL;
	socklen_t slen;
	struct timeval tv;
	struct sockaddr_storage ss;
	u_int8_t buf[READ_BUF_SIZE];
	u_int32_t key = 0;
	ssize_t len;

	if (relay_sessions >= RELAY_MAX_SESSIONS ||
	    rlay->rl_conf.flags & F_DISABLE)
		return;

	slen = sizeof(ss);
	if ((len = recvfrom(fd, buf, sizeof(buf), 0,
	    (struct sockaddr*)&ss, &slen)) < 1)
		return;

	/* Parse and validate the packet header */
	if (proto->validate != NULL &&
	    (*proto->validate)(rlay, &ss, buf, len, &key) != 0)
		return;

	if ((con = (struct session *)
	    calloc(1, sizeof(struct session))) == NULL)
		return;

	con->se_key = key;
	con->se_in.s = -1;
	con->se_out.s = -1;
	con->se_in.dst = &con->se_out;
	con->se_out.dst = &con->se_in;
	con->se_in.con = con;
	con->se_out.con = con;
	con->se_relay = rlay;
	con->se_id = ++relay_conid;
	con->se_outkey = rlay->rl_dstkey;
	con->se_in.tree = &proto->request_tree;
	con->se_out.tree = &proto->response_tree;
	con->se_in.dir = RELAY_DIR_REQUEST;
	con->se_out.dir = RELAY_DIR_RESPONSE;
	con->se_retry = rlay->rl_conf.dstretry;
	gettimeofday(&con->se_tv_start, NULL);
	bcopy(&con->se_tv_start, &con->se_tv_last, sizeof(con->se_tv_last));
	bcopy(&ss, &con->se_in.ss, sizeof(con->se_in.ss));
	con->se_out.port = rlay->rl_conf.dstport;
	switch (ss.ss_family) {
	case AF_INET:
		con->se_in.port = ((struct sockaddr_in *)&ss)->sin_port;
		break;
	case AF_INET6:
		con->se_in.port = ((struct sockaddr_in6 *)&ss)->sin6_port;
		break;
	}

	relay_sessions++;
	SPLAY_INSERT(session_tree, &rlay->rl_sessions, con);

	/* Increment the per-relay session counter */
	rlay->rl_stats[proc_id].last++;

	/* Pre-allocate output buffer */
	con->se_out.output = evbuffer_new();
	if (con->se_out.output == NULL) {
		relay_close(con, "failed to allocate output buffer");
		return;
	}

	/* Pre-allocate log buffer */
	con->se_log = evbuffer_new();
	if (con->se_log == NULL) {
		relay_close(con, "failed to allocate log buffer");
		return;
	}

	if (rlay->rl_conf.flags & F_NATLOOK) {
		if ((cnl = (struct ctl_natlook *)
		    calloc(1, sizeof(struct ctl_natlook))) == NULL) {
			relay_close(con, "failed to allocate natlookup");
			return;
		}
	}

	/* Save the received data */
	if (evbuffer_add(con->se_out.output, buf, len) == -1) {
		relay_close(con, "failed to store buffer");
		if (cnl != NULL)
			free(cnl);
		return;
	}

	if (rlay->rl_conf.flags & F_NATLOOK && cnl != NULL) {
		con->se_cnl = cnl;
		bzero(cnl, sizeof(*cnl));
		cnl->in = -1;
		cnl->id = con->se_id;
		cnl->proc = proc_id;
		bcopy(&con->se_in.ss, &cnl->src, sizeof(cnl->src));
		bcopy(&rlay->rl_conf.ss, &cnl->dst, sizeof(cnl->dst));
		imsg_compose(ibuf_pfe, IMSG_NATLOOK, 0, 0, -1, cnl,
		    sizeof(*cnl));

		/* Schedule timeout */
		evtimer_set(&con->se_ev, relay_natlook, con);
		bcopy(&rlay->rl_conf.timeout, &tv, sizeof(tv));
		evtimer_add(&con->se_ev, &tv);
		return;
	}

	relay_session(con);
}

void
relay_udp_timeout(int fd, short sig, void *arg)
{
	struct session		*con = (struct session *)arg;

	if (sig != EV_TIMEOUT)
		fatalx("invalid timeout event");

	relay_close(con, "udp timeout");
}

/*
 * Domain Name System support
 */

struct relay_dnshdr {
	u_int16_t	dns_id;

	u_int8_t	dns_flags0;
#define  DNS_F0_QR	0x80		/* response flag */
#define  DNS_F0_OPCODE	0x78		/* message type */
#define  DNS_F0_AA	0x04		/* authorative answer */
#define  DNS_F0_TC	0x02		/* truncated message */
#define  DNS_F0_RD	0x01		/* recursion desired */

	u_int8_t	dns_flags1;
#define  DNS_F1_RA	0x80		/* recursion available */
#define  DNS_F1_RES	0x40		/* reserved */
#define  DNS_F1_AD	0x20		/* authentic data */
#define  DNS_F1_CD	0x10		/* checking disabled */
#define  DNS_F1_RCODE	0x0f		/* response code */

	u_int16_t	dns_qdcount;
	u_int16_t	dns_ancount;
	u_int16_t	dns_nscount;
	u_int16_t	dns_arcount;
} __packed;

void
relay_dns_log(struct session *con, u_int8_t *buf)
{
	struct relay_dnshdr	*hdr = (struct relay_dnshdr *)buf;

	log_debug("relay_dns_log: session %d: %s id 0x%x "
	    "flags 0x%x:0x%x qd %u an %u ns %u ar %u",
	    con->se_id,
	    hdr->dns_flags0 & DNS_F0_QR ? "response" : "request",
	    ntohs(hdr->dns_id),
	    hdr->dns_flags0,
	    hdr->dns_flags1,
	    ntohs(hdr->dns_qdcount),
	    ntohs(hdr->dns_ancount),
	    ntohs(hdr->dns_nscount),
	    ntohs(hdr->dns_arcount));
}

int
relay_dns_validate(struct relay *rlay, struct sockaddr_storage *ss,
    u_int8_t *buf, size_t len, u_int32_t *key)
{
	struct relay_dnshdr	*hdr = (struct relay_dnshdr *)buf;
	struct session		*con, lookup;

	/* Validate the header length */
	if (len < sizeof(*hdr))
		return (-1);

	*key = ntohs(hdr->dns_id);

	/*
	 * Check if the header has the response flag set, otherwise
	 * return 0 to tell the UDP server to create a new session.
	 */
	if ((hdr->dns_flags0 & DNS_F0_QR) == 0)
		return (0);

	/*
	 * Lookup if this response is for a known session and if the
	 * remote host matches the original destination of the request.
	 */
	lookup.se_key = *key;
	if ((con = SPLAY_FIND(session_tree,
	    &rlay->rl_sessions, &lookup)) != NULL &&
	    relay_cmp_af(ss, &con->se_out.ss) == 0)
		relay_dns_response(con, buf, len);

	/*
	 * This is not a new session, ignore it in the UDP server.
	 */
	return (-1);
}

int
relay_dns_request(struct session *con)
{
	struct relay		*rlay = (struct relay *)con->se_relay;
	u_int8_t		*buf = EVBUFFER_DATA(con->se_out.output);
	size_t			 len = EVBUFFER_LENGTH(con->se_out.output);
	struct relay_dnshdr	*hdr;
	socklen_t		 slen;

	if (buf == NULL || len < 1)
		return (-1);
	if (debug)
		relay_dns_log(con, buf);

	if (gettimeofday(&con->se_tv_start, NULL))
		return (-1);

	if (rlay->rl_dsttable != NULL) {
		if (relay_from_table(con) != 0)
			return (-1);
	} else if (con->se_out.ss.ss_family == AF_UNSPEC) {
		bcopy(&rlay->rl_conf.dstss, &con->se_out.ss, sizeof(con->se_out.ss));
		con->se_out.port = rlay->rl_conf.dstport;
	}

	if (relay_socket_af(&con->se_out.ss, con->se_out.port) == -1)
		return (-1);
	slen = con->se_out.ss.ss_len;

	/*
	 * Replace the DNS request Id with a random Id.
	 */
	hdr = (struct relay_dnshdr *)buf;
	con->se_outkey = con->se_key;
	con->se_key = arc4random() & 0xffff;
	hdr->dns_id = htons(con->se_key);

 retry:
	if (sendto(rlay->rl_s, buf, len, 0,
	    (struct sockaddr *)&con->se_out.ss, slen) == -1) {
		if (con->se_retry) {
			con->se_retry--;
			log_debug("relay_dns_request: session %d: "
			    "forward failed: %s, %s",
			    con->se_id, strerror(errno),
			    con->se_retry ? "next retry" : "last retry");
			goto retry;
		}
		log_debug("relay_dns_request: session %d: forward failed: %s",
		    con->se_id, strerror(errno));
		return (-1);
	}

	event_again(&con->se_ev, con->se_out.s, EV_TIMEOUT,
	    relay_udp_timeout, &con->se_tv_start, &env->sc_timeout, con);

	return (0);
}

void
relay_dns_response(struct session *con, u_int8_t *buf, size_t len)
{
	struct relay		*rlay = (struct relay *)con->se_relay;
	struct relay_dnshdr	*hdr;
	socklen_t		 slen;

	if (debug)
		relay_dns_log(con, buf);

	/*
	 * Replace the random DNS request Id with the original Id
	 */
	hdr = (struct relay_dnshdr *)buf;
	hdr->dns_id = htons(con->se_outkey);

	slen = con->se_out.ss.ss_len;
	if (sendto(rlay->rl_s, buf, len, 0,
	    (struct sockaddr *)&con->se_in.ss, slen) == -1) {
		relay_close(con, "response failed");
		return;
	}

	relay_close(con, "session closed");
}

int
relay_dns_cmp(struct session *a, struct session *b)
{
	return (memcmp(&a->se_key, &b->se_key, sizeof(a->se_key)));
}
