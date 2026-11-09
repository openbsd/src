/*	$OpenBSD: ikedmode_show.c,v 1.1 2026/11/09 12:31:36 hshoexer Exp $	*/

/*
 * Copyright (c) 2026 Hans-Joerg Hoexer <hshoexer@yerbouti.franken.de>
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

/*
 * "ikectl show sa" for the ikev1 process: list isakmpd's SAs in the
 * style of iked's ikev2_info(), one text line per imsg to the control
 * process, followed by an empty imsg that ends the listing.
 *
 * This is separate from ikedmode.c because that file uses libevent,
 * whose struct event clashes with the one in isakmpd's timer.h, and
 * the remaining lifetimes come from isakmpd's timer events.
 */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/socket.h>

#include <imsg.h>
#include <netdb.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "constants.h"
#include "doi.h"
#include "ikedmode.h"
#include "ipsec.h"
#include "ipsec_num.h"
#include "log.h"
#include "sa.h"
#include "timer.h"
#include "transport.h"
#include "util.h"

#include "iked/types.h"

struct show_ctx {
	uint32_t	 peerid;	/* the ikectl connection */
	struct sa	*isakmp_sa;	/* list its phase 2 SAs, or NULL */
};

static void	 show_line(uint32_t, const char *, ...)
		    __attribute__((__format__(__printf__, 2, 3)));
static long	 time_left(struct event *);
static const char *addr_str(struct sockaddr *, char *, size_t);
static int	 mask_len(struct sockaddr *);
static const char *spi_str(u_int8_t *, size_t, char *, size_t);
static int	 ids_match(struct sa *, struct sa *);
static void	 show_phase1(struct sa *, uint32_t);
static void	 show_phase2(struct sa *, uint32_t, struct sa *);
static int	 match_p1_cb(struct sa *, void *);
static int	 show_p1_cb(struct sa *, void *);
static int	 show_p2_cb(struct sa *, void *);

/* Send one line of the listing to the control process.  */
static void
show_line(uint32_t peerid, const char *fmt, ...)
{
	va_list	 ap;
	char	*buf;
	int	 len;

	va_start(ap, fmt);
	len = vasprintf(&buf, fmt, ap);
	va_end(ap);
	if (len == -1) {
		log_error("show_line: vasprintf");
		return;
	}
	if (ikedmode_compose(PROC_CONTROL, IMSG_CTL_SHOW_SA, peerid, -1, buf,
	    len + 1) == -1)
		log_print("show_line: ikedmode_compose failed");
	free(buf);
}

/* Seconds until the timer event EV fires, -1 if there is none.  */
static long
time_left(struct event *ev)
{
	struct timespec	now;

	if (ev == NULL)
		return -1;
	clock_gettime(CLOCK_MONOTONIC, &now);
	return (long)(ev->expiration.tv_sec - now.tv_sec);
}

static const char *
addr_str(struct sockaddr *sa, char *buf, size_t size)
{
	if (sa == NULL || getnameinfo(sa, sa->sa_len, buf, size, NULL, 0,
	    NI_NUMERICHOST) != 0)
		strlcpy(buf, "<none>", size);
	return buf;
}

/* Prefix length of the netmask MASK, -1 if it is no prefix mask.  */
static int
mask_len(struct sockaddr *mask)
{
	u_int8_t	*p, b;
	int		 i, len, bits = 0;

	if (mask == NULL)
		return -1;
	p = sockaddr_addrdata(mask);
	len = sockaddr_addrlen(mask);
	for (i = 0; i < len && p[i] == 0xff; i++)
		bits += 8;
	if (i < len) {
		for (b = p[i]; b & 0x80; b <<= 1)
			bits++;
		if (b != 0)
			return -1;
		for (i++; i < len; i++)
			if (p[i] != 0)
				return -1;
	}
	return bits;
}

static const char *
spi_str(u_int8_t *spi, size_t sz, char *buf, size_t size)
{
	size_t	i, off;

	if (spi == NULL || sz == 0) {
		strlcpy(buf, "-", size);
		return buf;
	}
	off = strlcpy(buf, "0x", size);
	for (i = 0; i < sz && off + 2 < size; i++)
		off += snprintf(buf + off, size - off, "%02x", spi[i]);
	return buf;
}

/* Do the SAs A and B carry the same pair of phase 1 identities?  */
static int
ids_match(struct sa *a, struct sa *b)
{
	return a->id_i != NULL && a->id_r != NULL && b->id_i != NULL &&
	    b->id_r != NULL && a->id_i_len == b->id_i_len &&
	    a->id_r_len == b->id_r_len &&
	    memcmp(a->id_i, b->id_i, a->id_i_len) == 0 &&
	    memcmp(a->id_r, b->id_r, a->id_r_len) == 0;
}

static void
show_phase1(struct sa *sa, uint32_t peerid)
{
	struct sockaddr	*src = NULL, *dst = NULL;
	u_int8_t	*pid;
	size_t		 pid_len;
	char		 lbuf[NI_MAXHOST], pbuf[NI_MAXHOST];

	if (sa->transport != NULL) {
		sa->transport->vtbl->get_src(sa->transport, &src);
		sa->transport->vtbl->get_dst(sa->transport, &dst);
	}
	/* The peer's identity.  */
	pid = sa->initiator ? sa->id_r : sa->id_i;
	pid_len = sa->initiator ? sa->id_r_len : sa->id_i_len;

	show_line(peerid, "ikev1_sas: %p icookie %08x%08x rcookie %08x%08x "
	    "%s->%s<%s> %s %c%s%s%s life %llus left %lds name %s\n",
	    sa, decode_32(sa->cookies), decode_32(sa->cookies + 4),
	    decode_32(sa->cookies + 8), decode_32(sa->cookies + 12),
	    addr_str(src, lbuf, sizeof lbuf), addr_str(dst, pbuf, sizeof pbuf),
	    sa->doi->decode_ids("%s", pid, pid_len, pid, pid_len, 0),
	    sa->flags & SA_FLAG_READY ? "ESTABLISHED" : "NEGOTIATING",
	    sa->initiator ? 'i' : 'r',
	    sa->flags & SA_FLAG_DPD ? " dpd" : "",
	    sa->flags & SA_FLAG_REPLACED ? " replaced" : "",
	    sa->flags & SA_FLAG_FADING ? " fading" : "",
	    (unsigned long long)sa->seconds, time_left(sa->death),
	    sa->name ? sa->name : "<unnamed>");
}

static void
show_phase2(struct sa *sa, uint32_t peerid, struct sa *isakmp_sa)
{
	struct ipsec_sa		*isa = sa->data;
	struct ipsec_proto	*iproto;
	struct proto		*proto;
	struct sockaddr		*src = NULL, *dst = NULL;
	const char		*pname, *enc, *auth, *state;
	char			 lbuf[NI_MAXHOST], pbuf[NI_MAXHOST];
	char			 nbuf[NI_MAXHOST], mbuf[NI_MAXHOST];
	char			 sbuf[2 + 2 * 16 + 1];
	long			 left = time_left(sa->death);

	if (sa->transport != NULL) {
		sa->transport->vtbl->get_src(sa->transport, &src);
		sa->transport->vtbl->get_dst(sa->transport, &dst);
	}
	addr_str(src, lbuf, sizeof lbuf);
	addr_str(dst, pbuf, sizeof pbuf);
	state = sa->flags & SA_FLAG_READY ? "" : " negotiating";

	/* One line per direction and protocol, spi[0] out, spi[1] in.  */
	for (proto = TAILQ_FIRST(&sa->protos); proto;
	    proto = TAILQ_NEXT(proto, link)) {
		iproto = proto->data;
		pname = constant_name(ipsec_proto_cst, proto->proto);
		switch (proto->proto) {
		case IPSEC_PROTO_IPSEC_ESP:
			enc = constant_name(ipsec_esp_cst, proto->id);
			auth = iproto != NULL && iproto->auth != 0 ?
			    constant_name(ipsec_auth_cst, iproto->auth) :
			    "none";
			break;
		case IPSEC_PROTO_IPSEC_AH:
			enc = "none";
			auth = constant_name(ipsec_ah_cst, proto->id);
			break;
		default:
			enc = auth = "-";
			break;
		}
		show_line(peerid, "  sa_childsas: %p %s %s out %s -> %s "
		    "(%s/%s)%s life %llus left %lds @%p\n", sa, pname,
		    spi_str(proto->spi[0], proto->spi_sz[0], sbuf,
		    sizeof sbuf), lbuf, pbuf, enc, auth, state,
		    (unsigned long long)sa->seconds, left, isakmp_sa);
		show_line(peerid, "  sa_childsas: %p %s %s in %s -> %s "
		    "(%s/%s)%s life %llus left %lds @%p\n", sa, pname,
		    spi_str(proto->spi[1], proto->spi_sz[1], sbuf,
		    sizeof sbuf), pbuf, lbuf, enc, auth, state,
		    (unsigned long long)sa->seconds, left, isakmp_sa);
	}

	/* The flows, as pf_key_v2_enable_sa() installs them.  */
	if (isa == NULL || isa->src_net == NULL || isa->dst_net == NULL)
		return;
	proto = TAILQ_FIRST(&sa->protos);
	pname = proto != NULL ?
	    constant_name(ipsec_proto_cst, proto->proto) : "-";
	show_line(peerid, "  sa_flows: %p %s out %s/%d -> %s/%d [%u] %u:%u "
	    "@%p\n", sa, pname,
	    addr_str(isa->src_net, nbuf, sizeof nbuf), mask_len(isa->src_mask),
	    addr_str(isa->dst_net, mbuf, sizeof mbuf), mask_len(isa->dst_mask),
	    isa->tproto, ntohs(isa->sport), ntohs(isa->dport), isakmp_sa);
	show_line(peerid, "  sa_flows: %p %s in %s/%d -> %s/%d [%u] %u:%u "
	    "@%p\n", sa, pname,
	    addr_str(isa->dst_net, mbuf, sizeof mbuf), mask_len(isa->dst_mask),
	    addr_str(isa->src_net, nbuf, sizeof nbuf), mask_len(isa->src_mask),
	    isa->tproto, ntohs(isa->dport), ntohs(isa->sport), isakmp_sa);
}

/* sa_find() callback: is SA a phase 1 SA with the identities of ARG?  */
static int
match_p1_cb(struct sa *sa, void *arg)
{
	return sa->phase == 1 && ids_match(sa, arg);
}

/*
 * sa_find() callback: list the phase 2 SAs of the context's ISAKMP SA,
 * or, without one, those that have no ISAKMP SA any more.
 */
static int
show_p2_cb(struct sa *sa, void *arg)
{
	struct show_ctx	*ctx = arg;

	if (sa->phase != 2)
		return 0;
	if (ctx->isakmp_sa != NULL) {
		if (ids_match(sa, ctx->isakmp_sa))
			show_phase2(sa, ctx->peerid, ctx->isakmp_sa);
	} else if (sa_find(match_p1_cb, sa) == NULL)
		show_phase2(sa, ctx->peerid, NULL);
	return 0;
}

/* sa_find() callback: list a phase 1 SA followed by its phase 2 SAs.  */
static int
show_p1_cb(struct sa *sa, void *arg)
{
	struct show_ctx	*ctx = arg, sub;

	if (sa->phase != 1)
		return 0;
	show_phase1(sa, ctx->peerid);
	sub.peerid = ctx->peerid;
	sub.isakmp_sa = sa;
	sa_find(show_p2_cb, &sub);
	return 0;
}

/* Answer IMSG_CTL_SHOW_SA from the control process.  */
void
ikedmode_show_sa(uint32_t peerid)
{
	struct show_ctx	ctx;

	ctx.peerid = peerid;
	ctx.isakmp_sa = NULL;
	sa_find(show_p1_cb, &ctx);
	/* Phase 2 SAs whose ISAKMP SA is gone.  */
	sa_find(show_p2_cb, &ctx);

	/* The empty message ends the listing in ikectl.  */
	if (ikedmode_compose(PROC_CONTROL, IMSG_CTL_SHOW_SA, peerid, -1, NULL,
	    0) == -1)
		log_print("ikedmode_show_sa: ikedmode_compose failed");
}
