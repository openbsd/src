/*	$OpenBSD: bgpctl.c,v 1.255 2019/12/31 14:09:27 claudio Exp $ */

/*
 * Copyright (c) 2003 Henning Brauer <henning@openbsd.org>
 * Copyright (c) 2004-2019 Claudio Jeker <claudio@openbsd.org>
 * Copyright (c) 2016 Job Snijders <job@instituut.net>
 * Copyright (c) 2016 Peter Hessler <phessler@openbsd.org>
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
#include <sys/stat.h>
#include <sys/un.h>

#include <endian.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <util.h>

#include "bgpd.h"
#include "session.h"
#include "rde.h"

#include "bgpctl.h"
#include "parser.h"
#include "mrtparser.h"

int		 main(int, char *[]);
int		 show(struct imsg *, struct parse_result *);
void		 show_attr(void *, u_int16_t, int);
void		 show_communities(u_char *, size_t, int);
void		 show_community(u_char *, u_int16_t);
void		 show_large_community(u_char *, u_int16_t);
void		 show_ext_community(u_char *, u_int16_t);
void		 send_filterset(struct imsgbuf *, struct filter_set_head *);
void		 show_mrt_dump_neighbors(struct mrt_rib *, struct mrt_peer *,
		    void *);
void		 show_mrt_dump(struct mrt_rib *, struct mrt_peer *, void *);
void		 network_mrt_dump(struct mrt_rib *, struct mrt_peer *, void *);
void		 show_mrt_state(struct mrt_bgp_state *, void *);
void		 show_mrt_msg(struct mrt_bgp_msg *, void *);
const char	*msg_type(u_int8_t);
void		 network_bulk(struct parse_result *);
int		 match_aspath(void *, u_int16_t, struct filter_as *);

struct imsgbuf	*ibuf;
struct mrt_parser show_mrt = { show_mrt_dump, show_mrt_state, show_mrt_msg };
struct mrt_parser net_mrt = { network_mrt_dump, NULL, NULL };
int tableid;
int nodescr;

__dead void
usage(void)
{
	extern char	*__progname;

	fprintf(stderr, "usage: %s [-n] [-s socket] command [argument ...]\n",
	    __progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	struct sockaddr_un	 sun;
	int			 fd, n, done, ch, verbose = 0;
	struct imsg		 imsg;
	struct network_config	 net;
	struct parse_result	*res;
	struct ctl_neighbor	 neighbor;
	struct ctl_show_rib_request	ribreq;
	char			*sockname;
	enum imsg_type		 type;

	if (pledge("stdio rpath wpath cpath unix inet dns", NULL) == -1)
		err(1, "pledge");

	tableid = getrtable();
	if (asprintf(&sockname, "%s.%d", SOCKET_NAME, tableid) == -1)
		err(1, "asprintf");

	while ((ch = getopt(argc, argv, "ns:")) != -1) {
		switch (ch) {
		case 'n':
			if (++nodescr > 1)
				usage();
			break;
		case 's':
			sockname = optarg;
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	if ((res = parse(argc, argv)) == NULL)
		exit(1);

	memcpy(&neighbor.addr, &res->peeraddr, sizeof(neighbor.addr));
	strlcpy(neighbor.descr, res->peerdesc, sizeof(neighbor.descr));
	neighbor.is_group = res->is_group;
	strlcpy(neighbor.shutcomm, res->shutcomm, sizeof(neighbor.shutcomm));

	switch (res->action) {
	case SHOW_MRT:
		if (pledge("stdio", NULL) == -1)
			err(1, "pledge");

		bzero(&ribreq, sizeof(ribreq));
		if (res->as.type != AS_UNDEF)
			ribreq.as = res->as;
		if (res->addr.aid) {
			ribreq.prefix = res->addr;
			ribreq.prefixlen = res->prefixlen;
		}
		/* XXX currently no communities support */
		ribreq.neighbor = neighbor;
		ribreq.aid = res->aid;
		ribreq.flags = res->flags;
		ribreq.validation_state = res->validation_state;
		show_mrt.arg = &ribreq;
		if (res->flags & F_CTL_NEIGHBORS)
			show_mrt.dump = show_mrt_dump_neighbors;
		else
			show_head(res);
		mrt_parse(res->mrtfd, &show_mrt, 1);
		exit(0);
	default:
		break;
	}

	if (pledge("stdio unix", NULL) == -1)
		err(1, "pledge");

	if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
		err(1, "control_init: socket");

	bzero(&sun, sizeof(sun));
	sun.sun_family = AF_UNIX;
	if (strlcpy(sun.sun_path, sockname, sizeof(sun.sun_path)) >=
	    sizeof(sun.sun_path))
		errx(1, "socket name too long");
	if (connect(fd, (struct sockaddr *)&sun, sizeof(sun)) == -1)
		err(1, "connect: %s", sockname);

	if (pledge("stdio", NULL) == -1)
		err(1, "pledge");

	if ((ibuf = malloc(sizeof(struct imsgbuf))) == NULL)
		err(1, NULL);
	imsg_init(ibuf, fd);
	done = 0;

	switch (res->action) {
	case NONE:
	case SHOW_MRT:
		usage();
		/* NOTREACHED */
	case SHOW:
	case SHOW_SUMMARY:
		imsg_compose(ibuf, IMSG_CTL_SHOW_NEIGHBOR, 0, 0, -1, NULL, 0);
		break;
	case SHOW_SUMMARY_TERSE:
		imsg_compose(ibuf, IMSG_CTL_SHOW_TERSE, 0, 0, -1, NULL, 0);
		break;
	case SHOW_FIB:
		if (!res->addr.aid) {
			struct ibuf	*msg;
			sa_family_t	 af;

			af = aid2af(res->aid);
			if ((msg = imsg_create(ibuf, IMSG_CTL_KROUTE,
			    res->rtableid, 0, sizeof(res->flags) +
			    sizeof(af))) == NULL)
				errx(1, "imsg_create failure");
			if (imsg_add(msg, &res->flags, sizeof(res->flags)) ==
			    -1 ||
			    imsg_add(msg, &af, sizeof(af)) == -1)
				errx(1, "imsg_add failure");
			imsg_close(ibuf, msg);
		} else
			imsg_compose(ibuf, IMSG_CTL_KROUTE_ADDR, res->rtableid,
			    0, -1, &res->addr, sizeof(res->addr));
		break;
	case SHOW_FIB_TABLES:
		imsg_compose(ibuf, IMSG_CTL_SHOW_FIB_TABLES, 0, 0, -1, NULL, 0);
		break;
	case SHOW_NEXTHOP:
		imsg_compose(ibuf, IMSG_CTL_SHOW_NEXTHOP, res->rtableid, 0, -1,
		    NULL, 0);
		break;
	case SHOW_INTERFACE:
		imsg_compose(ibuf, IMSG_CTL_SHOW_INTERFACE, 0, 0, -1, NULL, 0);
		break;
	case SHOW_NEIGHBOR:
	case SHOW_NEIGHBOR_TIMERS:
	case SHOW_NEIGHBOR_TERSE:
		neighbor.show_timers = (res->action == SHOW_NEIGHBOR_TIMERS);
		if (res->peeraddr.aid || res->peerdesc[0])
			imsg_compose(ibuf, IMSG_CTL_SHOW_NEIGHBOR, 0, 0, -1,
			    &neighbor, sizeof(neighbor));
		else
			imsg_compose(ibuf, IMSG_CTL_SHOW_NEIGHBOR, 0, 0, -1,
			    NULL, 0);
		break;
	case SHOW_RIB:
		bzero(&ribreq, sizeof(ribreq));
		type = IMSG_CTL_SHOW_RIB;
		if (res->addr.aid) {
			ribreq.prefix = res->addr;
			ribreq.prefixlen = res->prefixlen;
			type = IMSG_CTL_SHOW_RIB_PREFIX;
		}
		if (res->as.type != AS_UNDEF)
			ribreq.as = res->as;
		if (res->community.flags != 0)
			ribreq.community = res->community;
		ribreq.neighbor = neighbor;
		strlcpy(ribreq.rib, res->rib, sizeof(ribreq.rib));
		ribreq.aid = res->aid;
		ribreq.flags = res->flags;
		imsg_compose(ibuf, type, 0, 0, -1, &ribreq, sizeof(ribreq));
		break;
	case SHOW_RIB_MEM:
		imsg_compose(ibuf, IMSG_CTL_SHOW_RIB_MEM, 0, 0, -1, NULL, 0);
		break;
	case RELOAD:
		imsg_compose(ibuf, IMSG_CTL_RELOAD, 0, 0, -1, NULL, 0);
		printf("reload request sent.\n");
		break;
	case FIB:
		errx(1, "action==FIB");
		break;
	case FIB_COUPLE:
		imsg_compose(ibuf, IMSG_CTL_FIB_COUPLE, res->rtableid, 0, -1,
		    NULL, 0);
		printf("couple request sent.\n");
		done = 1;
		break;
	case FIB_DECOUPLE:
		imsg_compose(ibuf, IMSG_CTL_FIB_DECOUPLE, res->rtableid, 0, -1,
		    NULL, 0);
		printf("decouple request sent.\n");
		done = 1;
		break;
	case NEIGHBOR:
		errx(1, "action==NEIGHBOR");
		break;
	case NEIGHBOR_UP:
		imsg_compose(ibuf, IMSG_CTL_NEIGHBOR_UP, 0, 0, -1,
		    &neighbor, sizeof(neighbor));
		break;
	case NEIGHBOR_DOWN:
		imsg_compose(ibuf, IMSG_CTL_NEIGHBOR_DOWN, 0, 0, -1,
		    &neighbor, sizeof(neighbor));
		break;
	case NEIGHBOR_CLEAR:
		imsg_compose(ibuf, IMSG_CTL_NEIGHBOR_CLEAR, 0, 0, -1,
		    &neighbor, sizeof(neighbor));
		break;
	case NEIGHBOR_RREFRESH:
		imsg_compose(ibuf, IMSG_CTL_NEIGHBOR_RREFRESH, 0, 0, -1,
		    &neighbor, sizeof(neighbor));
		break;
	case NEIGHBOR_DESTROY:
		imsg_compose(ibuf, IMSG_CTL_NEIGHBOR_DESTROY, 0, 0, -1,
		    &neighbor, sizeof(neighbor));
		break;
	case NETWORK_BULK_ADD:
	case NETWORK_BULK_REMOVE:
		network_bulk(res);
		printf("requests sent.\n");
		done = 1;
		break;
	case NETWORK_ADD:
	case NETWORK_REMOVE:
		bzero(&net, sizeof(net));
		net.prefix = res->addr;
		net.prefixlen = res->prefixlen;
		net.rd = res->rd;
		/* attribute sets are not supported */
		if (res->action == NETWORK_ADD) {
			imsg_compose(ibuf, IMSG_NETWORK_ADD, 0, 0, -1,
			    &net, sizeof(net));
			send_filterset(ibuf, &res->set);
			imsg_compose(ibuf, IMSG_NETWORK_DONE, 0, 0, -1,
			    NULL, 0);
		} else
			imsg_compose(ibuf, IMSG_NETWORK_REMOVE, 0, 0, -1,
			    &net, sizeof(net));
		printf("request sent.\n");
		done = 1;
		break;
	case NETWORK_FLUSH:
		imsg_compose(ibuf, IMSG_NETWORK_FLUSH, 0, 0, -1, NULL, 0);
		printf("request sent.\n");
		done = 1;
		break;
	case NETWORK_SHOW:
		bzero(&ribreq, sizeof(ribreq));
		ribreq.aid = res->aid;
		strlcpy(ribreq.rib, res->rib, sizeof(ribreq.rib));
		imsg_compose(ibuf, IMSG_CTL_SHOW_NETWORK, 0, 0, -1,
		    &ribreq, sizeof(ribreq));
		break;
	case NETWORK_MRT:
		bzero(&ribreq, sizeof(ribreq));
		if (res->as.type != AS_UNDEF)
			ribreq.as = res->as;
		if (res->addr.aid) {
			ribreq.prefix = res->addr;
			ribreq.prefixlen = res->prefixlen;
		}
		/* XXX currently no community support */
		ribreq.neighbor = neighbor;
		ribreq.aid = res->aid;
		ribreq.flags = res->flags;
		net_mrt.arg = &ribreq;
		mrt_parse(res->mrtfd, &net_mrt, 1);
		done = 1;
		break;
	case LOG_VERBOSE:
		verbose = 1;
		/* FALLTHROUGH */
	case LOG_BRIEF:
		imsg_compose(ibuf, IMSG_CTL_LOG_VERBOSE, 0, 0, -1,
		    &verbose, sizeof(verbose));
		printf("logging request sent.\n");
		done = 1;
		break;
	}

	while (ibuf->w.queued)
		if (msgbuf_write(&ibuf->w) <= 0 && errno != EAGAIN)
			err(1, "write error");

	show_head(res);

	while (!done) {
		if ((n = imsg_read(ibuf)) == -1 && errno != EAGAIN)
			err(1, "imsg_read error");
		if (n == 0)
			errx(1, "pipe closed");

		while (!done) {
			if ((n = imsg_get(ibuf, &imsg)) == -1)
				err(1, "imsg_get error");
			if (n == 0)
				break;

			done = show(&imsg, res);
			imsg_free(&imsg);
		}
	}
	close(fd);
	free(ibuf);

	exit(0);
}

int
show(struct imsg *imsg, struct parse_result *res)
{
	struct peer		*p;
	struct ctl_timer	*t;
	struct ctl_show_interface	*iface;
	struct ctl_show_nexthop	*nh;
	struct kroute_full	*kf;
	struct ktable		*kt;
	struct ctl_show_rib	 rib;
	u_char			*asdata;
	struct rde_memstats	stats;
	struct rde_hashstats	hash;
	u_int			rescode, ilen;
	size_t			aslen;

	switch (imsg->hdr.type) {
	case IMSG_CTL_SHOW_NEIGHBOR:
		p = imsg->data;
		show_neighbor(p, res);
		break;
	case IMSG_CTL_SHOW_TIMER:
		t = imsg->data;
		if (t->type > 0 && t->type < Timer_Max)
			show_timer(t);
		break;
	case IMSG_CTL_SHOW_INTERFACE:
		iface = imsg->data;
		show_interface(iface);
		break;
	case IMSG_CTL_SHOW_NEXTHOP:
		nh = imsg->data;
		show_nexthop(nh);
		break;
	case IMSG_CTL_KROUTE:
	case IMSG_CTL_SHOW_NETWORK:
		if (imsg->hdr.len < IMSG_HEADER_SIZE + sizeof(*kf))
			errx(1, "wrong imsg len");
		kf = imsg->data;
		show_fib(kf);
		break;
	case IMSG_CTL_SHOW_FIB_TABLES:
		if (imsg->hdr.len < IMSG_HEADER_SIZE + sizeof(*kt))
			errx(1, "wrong imsg len");
		kt = imsg->data;

		show_fib_table(kt);
		break;
	case IMSG_CTL_SHOW_RIB:
		if (imsg->hdr.len < IMSG_HEADER_SIZE + sizeof(rib))
			errx(1, "wrong imsg len");
		memcpy(&rib, imsg->data, sizeof(rib));
		aslen = imsg->hdr.len - IMSG_HEADER_SIZE - sizeof(rib);
		asdata = imsg->data;
		asdata += sizeof(rib);
		show_rib(&rib, asdata, aslen, res);
		break;
	case IMSG_CTL_SHOW_RIB_COMMUNITIES:
		ilen = imsg->hdr.len - IMSG_HEADER_SIZE;
		if (ilen % sizeof(struct community)) {
			warnx("bad IMSG_CTL_SHOW_RIB_COMMUNITIES received");
			break;
		}
		show_communities(imsg->data, ilen, res->flags);
		break;
	case IMSG_CTL_SHOW_RIB_ATTR:
		ilen = imsg->hdr.len - IMSG_HEADER_SIZE;
		if (ilen < 3) {
			warnx("bad IMSG_CTL_SHOW_RIB_ATTR received");
			break;
		}
		show_attr(imsg->data, ilen, res->flags);
		break;
	case IMSG_CTL_SHOW_RIB_MEM:
		memcpy(&stats, imsg->data, sizeof(stats));
		show_rib_mem(&stats);
		break;
	case IMSG_CTL_SHOW_RIB_HASH:
		memcpy(&hash, imsg->data, sizeof(hash));
		show_rib_hash(&hash);
		break;
	case IMSG_CTL_RESULT:
		if (imsg->hdr.len != IMSG_HEADER_SIZE + sizeof(rescode)) {
			warnx("got IMSG_CTL_RESULT with wrong len");
			break;
		}
		memcpy(&rescode, imsg->data, sizeof(rescode));
		show_result(rescode);
		return (1);
	case IMSG_CTL_END:
		return (1);
	default:
		warnx("unknown imsg %d received", imsg->hdr.type);
		break;
	}

	return (0);
}

char *
fmt_peer(const char *descr, const struct bgpd_addr *remote_addr,
    int masklen)
{
	const char	*ip;
	char		*p;

	if (descr && descr[0] && !nodescr) {
		if ((p = strdup(descr)) == NULL)
			err(1, NULL);
		return (p);
	}

	ip = log_addr(remote_addr);
	if (masklen != -1 && ((remote_addr->aid == AID_INET && masklen != 32) ||
	    (remote_addr->aid == AID_INET6 && masklen != 128))) {
		if (asprintf(&p, "%s/%u", ip, masklen) == -1)
			err(1, NULL);
	} else {
		if ((p = strdup(ip)) == NULL)
			err(1, NULL);
	}

	return (p);
}

const char *
print_auth_method(enum auth_method method)
{
	switch (method) {
	case AUTH_MD5SIG:
		return ", using md5sig";
	case AUTH_IPSEC_MANUAL_ESP:
		return ", using ipsec manual esp";
	case AUTH_IPSEC_MANUAL_AH:
		return ", using ipsec manual ah";
	case AUTH_IPSEC_IKE_ESP:
		return ", using ipsec ike esp";
	case AUTH_IPSEC_IKE_AH:
		return ", using ipsec ike ah";
	case AUTH_NONE:	/* FALLTHROUGH */
	default:
		return "";
	}
}

void
print_neighbor_capa_mp(struct peer *p)
{
	int		comma;
	u_int8_t	i;

	for (i = 0, comma = 0; i < AID_MAX; i++)
		if (p->capa.peer.mp[i]) {
			printf("%s%s", comma ? ", " : "", aid2str(i));
			comma = 1;
		}
}

void
print_neighbor_capa_restart(struct peer *p)
{
	int		comma;
	u_int8_t	i;

	if (p->capa.peer.grestart.timeout)
		printf(": Timeout: %d, ", p->capa.peer.grestart.timeout);
	for (i = 0, comma = 0; i < AID_MAX; i++)
		if (p->capa.peer.grestart.flags[i] & CAPA_GR_PRESENT) {
			if (!comma &&
			    p->capa.peer.grestart.flags[i] & CAPA_GR_RESTART)
				printf("restarted, ");
			if (comma)
				printf(", ");
			printf("%s", aid2str(i));
			if (p->capa.peer.grestart.flags[i] & CAPA_GR_FORWARD)
				printf(" (preserved)");
			comma = 1;
		}
}

void
print_neighbor_msgstats(struct peer *p)
{
	printf("  Message statistics:\n");
	printf("  %-15s %-10s %-10s\n", "", "Sent", "Received");
	printf("  %-15s %10llu %10llu\n", "Opens",
	    p->stats.msg_sent_open, p->stats.msg_rcvd_open);
	printf("  %-15s %10llu %10llu\n", "Notifications",
	    p->stats.msg_sent_notification, p->stats.msg_rcvd_notification);
	printf("  %-15s %10llu %10llu\n", "Updates",
	    p->stats.msg_sent_update, p->stats.msg_rcvd_update);
	printf("  %-15s %10llu %10llu\n", "Keepalives",
	    p->stats.msg_sent_keepalive, p->stats.msg_rcvd_keepalive);
	printf("  %-15s %10llu %10llu\n", "Route Refresh",
	    p->stats.msg_sent_rrefresh, p->stats.msg_rcvd_rrefresh);
	printf("  %-15s %10llu %10llu\n\n", "Total",
	    p->stats.msg_sent_open + p->stats.msg_sent_notification +
	    p->stats.msg_sent_update + p->stats.msg_sent_keepalive +
	    p->stats.msg_sent_rrefresh,
	    p->stats.msg_rcvd_open + p->stats.msg_rcvd_notification +
	    p->stats.msg_rcvd_update + p->stats.msg_rcvd_keepalive +
	    p->stats.msg_rcvd_rrefresh);
	printf("  Update statistics:\n");
	printf("  %-15s %-10s %-10s\n", "", "Sent", "Received");
	printf("  %-15s %10llu %10llu\n", "Updates",
	    p->stats.prefix_sent_update, p->stats.prefix_rcvd_update);
	printf("  %-15s %10llu %10llu\n", "Withdraws",
	    p->stats.prefix_sent_withdraw, p->stats.prefix_rcvd_withdraw);
	printf("  %-15s %10llu %10llu\n", "End-of-Rib",
	    p->stats.prefix_sent_eor, p->stats.prefix_rcvd_eor);
}

#define TF_BUFS	8
#define TF_LEN	9

static const char *
fmt_timeframe_core(time_t t)
{
	char		*buf;
	static char	 tfbuf[TF_BUFS][TF_LEN];	/* ring buffer */
	static int	 idx = 0;
	unsigned int	 sec, min, hrs, day;
	unsigned long long	week;

	buf = tfbuf[idx++];
	if (idx == TF_BUFS)
		idx = 0;

	week = t;

	sec = week % 60;
	week /= 60;
	min = week % 60;
	week /= 60;
	hrs = week % 24;
	week /= 24;
	day = week % 7;
	week /= 7;

	if (week > 0)
		snprintf(buf, TF_LEN, "%02lluw%01ud%02uh", week, day, hrs);
	else if (day > 0)
		snprintf(buf, TF_LEN, "%01ud%02uh%02um", day, hrs, min);
	else
		snprintf(buf, TF_LEN, "%02u:%02u:%02u", hrs, min, sec);

	return (buf);
}

const char *
fmt_timeframe(time_t t)
{
	time_t now;

	if (t == 0)
		return ("Never");

	now = time(NULL);
	if (t > now)	/* time in the future is not possible */
		t = now;
	return (fmt_timeframe_core(now - t));
}

void
show_fib_flags(u_int16_t flags)
{
	if (flags & F_DOWN)
		printf(" ");
	else
		printf("*");

	if (flags & F_BGPD_INSERTED)
		printf("B");
	else if (flags & F_CONNECTED)
		printf("C");
	else if (flags & F_STATIC)
		printf("S");
	else if (flags & F_DYNAMIC)
		printf("D");
	else
		printf(" ");

	if (flags & F_NEXTHOP)
		printf("N");
	else
		printf(" ");

	if (flags & F_REJECT && flags & F_BLACKHOLE)
		printf("f");
	else if (flags & F_REJECT)
		printf("r");
	else if (flags & F_BLACKHOLE)
		printf("b");
	else
		printf(" ");

	printf(" ");
}

void
print_prefix(struct bgpd_addr *prefix, u_int8_t prefixlen, u_int8_t flags,
    u_int8_t ovs)
{
	char			*p;

	print_flags(flags, 1);
	printf("%3s ", print_ovs(ovs, 1));
	if (asprintf(&p, "%s/%u", log_addr(prefix), prefixlen) == -1)
		err(1, NULL);
	printf("%-20s", p);
	free(p);
}

const char *
print_origin(u_int8_t origin, int sum)
{
	switch (origin) {
	case ORIGIN_IGP:
		return (sum ? "i" : "IGP");
	case ORIGIN_EGP:
		return (sum ? "e" : "EGP");
	case ORIGIN_INCOMPLETE:
		return (sum ? "?" : "incomplete");
	default:
		return (sum ? "X" : "bad origin");
	}
}

void
print_flags(u_int8_t flags, int sum)
{
	char	 flagstr[5];
	char	*p = flagstr;

	if (sum) {
		if (flags & F_PREF_INVALID)
			*p++ = 'E';
		if (flags & F_PREF_ANNOUNCE)
			*p++ = 'A';
		if (flags & F_PREF_INTERNAL)
			*p++ = 'I';
		if (flags & F_PREF_STALE)
			*p++ = 'S';
		if (flags & F_PREF_ELIGIBLE)
			*p++ = '*';
		if (flags & F_PREF_ACTIVE)
			*p++ = '>';
		*p = '\0';
		printf("%-5s ", flagstr);
	} else {
		if (flags & F_PREF_INTERNAL)
			printf("internal");
		else
			printf("external");
		if (flags & F_PREF_STALE)
			printf(", stale");
		if (flags & F_PREF_ELIGIBLE)
			printf(", valid");
		if (flags & F_PREF_ACTIVE)
			printf(", best");
		if (flags & F_PREF_ANNOUNCE)
			printf(", announced");
	}
}

const char *
print_ovs(u_int8_t validation_state, int sum)
{
	switch (validation_state) {
	case ROA_INVALID:
		return (sum ? "!" : "invalid");
	case ROA_VALID:
		return (sum ? "V" : "valid");
	default:
		return (sum ? "N" : "not-found");
	}
}

static const char *
print_attr(u_int8_t type, u_int8_t flags)
{
#define CHECK_FLAGS(s, t, m)	\
	if (((s) & ~(ATTR_DEFMASK | (m))) != (t)) pflags = 1

	static char cstr[48];
	int pflags = 0;

	switch (type) {
	case ATTR_ORIGIN:
		CHECK_FLAGS(flags, ATTR_WELL_KNOWN, 0);
		strlcpy(cstr, "Origin", sizeof(cstr));
		break;
	case ATTR_ASPATH:
		CHECK_FLAGS(flags, ATTR_WELL_KNOWN, 0);
		strlcpy(cstr, "AS-Path", sizeof(cstr));
		break;
	case ATTR_AS4_PATH:
		CHECK_FLAGS(flags, ATTR_WELL_KNOWN, 0);
		strlcpy(cstr, "AS4-Path", sizeof(cstr));
		break;
	case ATTR_NEXTHOP:
		CHECK_FLAGS(flags, ATTR_WELL_KNOWN, 0);
		strlcpy(cstr, "Nexthop", sizeof(cstr));
		break;
	case ATTR_MED:
		CHECK_FLAGS(flags, ATTR_OPTIONAL, 0);
		strlcpy(cstr, "Med", sizeof(cstr));
		break;
	case ATTR_LOCALPREF:
		CHECK_FLAGS(flags, ATTR_WELL_KNOWN, 0);
		strlcpy(cstr, "Localpref", sizeof(cstr));
		break;
	case ATTR_ATOMIC_AGGREGATE:
		CHECK_FLAGS(flags, ATTR_WELL_KNOWN, 0);
		strlcpy(cstr, "Atomic Aggregate", sizeof(cstr));
		break;
	case ATTR_AGGREGATOR:
		CHECK_FLAGS(flags, ATTR_OPTIONAL|ATTR_TRANSITIVE, ATTR_PARTIAL);
		strlcpy(cstr, "Aggregator", sizeof(cstr));
		break;
	case ATTR_AS4_AGGREGATOR:
		CHECK_FLAGS(flags, ATTR_OPTIONAL|ATTR_TRANSITIVE, ATTR_PARTIAL);
		strlcpy(cstr, "AS4-Aggregator", sizeof(cstr));
		break;
	case ATTR_COMMUNITIES:
		CHECK_FLAGS(flags, ATTR_OPTIONAL|ATTR_TRANSITIVE, ATTR_PARTIAL);
		strlcpy(cstr, "Communities", sizeof(cstr));
		break;
	case ATTR_ORIGINATOR_ID:
		CHECK_FLAGS(flags, ATTR_OPTIONAL, 0);
		strlcpy(cstr, "Originator Id", sizeof(cstr));
		break;
	case ATTR_CLUSTER_LIST:
		CHECK_FLAGS(flags, ATTR_OPTIONAL, 0);
		strlcpy(cstr, "Cluster Id List", sizeof(cstr));
		break;
	case ATTR_MP_REACH_NLRI:
		CHECK_FLAGS(flags, ATTR_OPTIONAL, 0);
		strlcpy(cstr, "MP Reach NLRI", sizeof(cstr));
		break;
	case ATTR_MP_UNREACH_NLRI:
		CHECK_FLAGS(flags, ATTR_OPTIONAL, 0);
		strlcpy(cstr, "MP Unreach NLRI", sizeof(cstr));
		break;
	case ATTR_EXT_COMMUNITIES:
		CHECK_FLAGS(flags, ATTR_OPTIONAL|ATTR_TRANSITIVE, ATTR_PARTIAL);
		strlcpy(cstr, "Ext. Communities", sizeof(cstr));
		break;
	case ATTR_LARGE_COMMUNITIES:
		CHECK_FLAGS(flags, ATTR_OPTIONAL|ATTR_TRANSITIVE, ATTR_PARTIAL);
		strlcpy(cstr, "Large Communities", sizeof(cstr));
		break;
	default:
		/* ignore unknown attributes */
		snprintf(cstr, sizeof(cstr), "Unknown Attribute #%u", type);
		pflags = 1;
		break;
	}
	if (pflags) {
		strlcat(cstr, " flags [", sizeof(cstr));
		if (flags & ATTR_OPTIONAL)
			strlcat(cstr, "O", sizeof(cstr));
		if (flags & ATTR_TRANSITIVE)
			strlcat(cstr, "T", sizeof(cstr));
		if (flags & ATTR_PARTIAL)
			strlcat(cstr, "P", sizeof(cstr));
		strlcat(cstr, "]", sizeof(cstr));
	}
	return (cstr);

#undef CHECK_FLAGS
}

void
show_attr(void *b, u_int16_t len, int flag0)
{
	u_char		*data = b, *path;
	struct in_addr	 id;
	struct bgpd_addr prefix;
	char		*aspath;
	u_int32_t	 as;
	u_int16_t	 alen, ioff, short_as, afi;
	u_int8_t	 flags, type, safi, aid, prefixlen;
	int		 i, pos, e2, e4;

	if (len < 3)
		errx(1, "show_attr: too short bgp attr");

	flags = data[0];
	type = data[1];

	/* get the attribute length */
	if (flags & ATTR_EXTLEN) {
		if (len < 4)
			errx(1, "show_attr: too short bgp attr");
		memcpy(&alen, data+2, sizeof(u_int16_t));
		alen = ntohs(alen);
		data += 4;
		len -= 4;
	} else {
		alen = data[2];
		data += 3;
		len -= 3;
	}

	/* bad imsg len how can that happen!? */
	if (alen > len)
		errx(1, "show_attr: bad length");

	printf("    %s: ", print_attr(type, flags));

	switch (type) {
	case ATTR_ORIGIN:
		if (alen == 1)
			printf("%u", *data);
		else
			printf("bad length");
		break;
	case ATTR_ASPATH:
	case ATTR_AS4_PATH:
		/* prefer 4-byte AS here */
		e4 = aspath_verify(data, alen, 1);
		e2 = aspath_verify(data, alen, 0);
		if (e4 == 0 || e4 == AS_ERR_SOFT) {
			path = data;
		} else if (e2 == 0 || e2 == AS_ERR_SOFT) {
			path = aspath_inflate(data, alen, &alen);
			if (path == NULL)
				errx(1, "aspath_inflate failed");
		} else {
			printf("bad AS-Path");
			break;
		}
		if (aspath_asprint(&aspath, path, alen) == -1)
			err(1, NULL);
		printf("%s", aspath);
		free(aspath);
		if (path != data)
			free(path);
		break;
	case ATTR_NEXTHOP:
		if (alen == 4) {
			memcpy(&id, data, sizeof(id));
			printf("%s", inet_ntoa(id));
		} else
			printf("bad length");
		break;
	case ATTR_MED:
	case ATTR_LOCALPREF:
		if (alen == 4) {
			u_int32_t val;
			memcpy(&val, data, sizeof(val));
			val = ntohl(val);
			printf("%u", val);
		} else
			printf("bad length");
		break;
	case ATTR_AGGREGATOR:
	case ATTR_AS4_AGGREGATOR:
		if (alen == 8) {
			memcpy(&as, data, sizeof(as));
			memcpy(&id, data + sizeof(as), sizeof(id));
			as = ntohl(as);
		} else if (alen == 6) {
			memcpy(&short_as, data, sizeof(short_as));
			memcpy(&id, data + sizeof(short_as), sizeof(id));
			as = ntohs(short_as);
		} else {
			printf("bad length");
			break;
		}
		printf("%s [%s]", log_as(as), inet_ntoa(id));
		break;
	case ATTR_COMMUNITIES:
		show_community(data, alen);
		break;
	case ATTR_ORIGINATOR_ID:
		memcpy(&id, data, sizeof(id));
		printf("%s", inet_ntoa(id));
		break;
	case ATTR_CLUSTER_LIST:
		for (ioff = 0; ioff + sizeof(id) <= alen;
		    ioff += sizeof(id)) {
			memcpy(&id, data + ioff, sizeof(id));
			printf(" %s", inet_ntoa(id));
		}
		break;
	case ATTR_MP_REACH_NLRI:
	case ATTR_MP_UNREACH_NLRI:
		if (alen < 3) {
 bad_len:
			printf("bad length");
			break;
		}
		memcpy(&afi, data, 2);
		data += 2;
		alen -= 2;
		afi = ntohs(afi);
		safi = *data++;
		alen--;

		if (afi2aid(afi, safi, &aid) == -1) {
			printf("bad AFI/SAFI pair");
			break;
		}
		printf(" %s", aid2str(aid));

		if (type == ATTR_MP_REACH_NLRI) {
			struct bgpd_addr nexthop;
			u_int8_t nhlen;
			if (len == 0)
				goto bad_len;
			nhlen = *data++;
			alen--;
			if (nhlen > len)
				goto bad_len;
			bzero(&nexthop, sizeof(nexthop));
			switch (aid) {
			case AID_INET6:
				nexthop.aid = aid;
				if (nhlen != 16 && nhlen != 32)
					goto bad_len;
				memcpy(&nexthop.v6.s6_addr, data, 16);
				break;
			case AID_VPN_IPv4:
				if (nhlen != 12)
					goto bad_len;
				nexthop.aid = AID_INET;
				memcpy(&nexthop.v4, data + sizeof(u_int64_t),
				    sizeof(nexthop.v4));
				break;
			default:
				printf("unhandled AID #%u", aid);
				goto done;
			}
			/* ignore reserved (old SNPA) field as per RFC4760 */
			data += nhlen + 1;
			alen -= nhlen + 1;

			printf(" nexthop: %s", log_addr(&nexthop));
		}

		while (alen > 0) {
			switch (aid) {
			case AID_INET6:
				pos = nlri_get_prefix6(data, alen, &prefix,
				    &prefixlen);
				break;
			case AID_VPN_IPv4:
				pos = nlri_get_vpn4(data, alen, &prefix,
				    &prefixlen, 1);
				break;
			default:
				printf("unhandled AID #%u", aid);
				goto done;
			}
			if (pos == -1) {
				printf("bad %s prefix", aid2str(aid));
				break;
			}
			printf(" %s/%u", log_addr(&prefix), prefixlen);
			data += pos;
			alen -= pos;
		}
		break;
	case ATTR_EXT_COMMUNITIES:
		show_ext_community(data, alen);
		break;
	case ATTR_LARGE_COMMUNITIES:
		show_large_community(data, alen);
		break;
	case ATTR_ATOMIC_AGGREGATE:
	default:
		printf(" len %u", alen);
		if (alen) {
			printf(":");
			for (i=0; i < alen; i++)
				printf(" %02x", *(data+i));
		}
		break;
	}
 done:
	printf("%c", EOL0(flag0));
}

static void
print_community(u_int16_t a, u_int16_t v)
{
	if (a == COMMUNITY_WELLKNOWN)
		switch (v) {
		case COMMUNITY_GRACEFUL_SHUTDOWN:
			printf("GRACEFUL_SHUTDOWN");
			break;
		case COMMUNITY_NO_EXPORT:
			printf("NO_EXPORT");
			break;
		case COMMUNITY_NO_ADVERTISE:
			printf("NO_ADVERTISE");
			break;
		case COMMUNITY_NO_EXPSUBCONFED:
			printf("NO_EXPORT_SUBCONFED");
			break;
		case COMMUNITY_NO_PEER:
			printf("NO_PEER");
			break;
		case COMMUNITY_BLACKHOLE:
			printf("BLACKHOLE");
			break;
		default:
			printf("%hu:%hu", a, v);
			break;
		}
	else
		printf("%hu:%hu", a, v);
}

static void
print_ext_community(u_int8_t *data)
{
	u_int64_t	ext;
	struct in_addr	ip;
	u_int32_t	as4, u32;
	u_int16_t	as2, u16;
	u_int8_t	type, subtype;

	type = data[0];
	subtype = data[1];

	printf("%s ", log_ext_subtype(type, subtype));

	switch (type) {
	case EXT_COMMUNITY_TRANS_TWO_AS:
		memcpy(&as2, data + 2, sizeof(as2));
		memcpy(&u32, data + 4, sizeof(u32));
		printf("%s:%u", log_as(ntohs(as2)), ntohl(u32));
		break;
	case EXT_COMMUNITY_TRANS_IPV4:
		memcpy(&ip, data + 2, sizeof(ip));
		memcpy(&u16, data + 6, sizeof(u16));
		printf("%s:%hu", inet_ntoa(ip), ntohs(u16));
		break;
	case EXT_COMMUNITY_TRANS_FOUR_AS:
		memcpy(&as4, data + 2, sizeof(as4));
		memcpy(&u16, data + 6, sizeof(u16));
		printf("%s:%hu", log_as(ntohl(as4)), ntohs(u16));
		break;
	case EXT_COMMUNITY_TRANS_OPAQUE:
	case EXT_COMMUNITY_TRANS_EVPN:
		memcpy(&ext, data, sizeof(ext));
		ext = be64toh(ext) & 0xffffffffffffLL;
		printf("0x%llx", (unsigned long long)ext);
		break;
	case EXT_COMMUNITY_NON_TRANS_OPAQUE:
		memcpy(&ext, data, sizeof(ext));
		ext = be64toh(ext) & 0xffffffffffffLL;
		switch (ext) {
		case EXT_COMMUNITY_OVS_VALID:
			printf("valid ");
			break;
		case EXT_COMMUNITY_OVS_NOTFOUND:
			printf("not-found ");
			break;
		case EXT_COMMUNITY_OVS_INVALID:
			printf("invalid ");
			break;
		default:
			printf("0x%llx ", (unsigned long long)ext);
			break;
		}
		break;
	default:
		memcpy(&ext, data, sizeof(ext));
		printf("0x%llx", (unsigned long long)be64toh(ext));
	}
}

void
show_communities(u_char *data, size_t len, int flag0)
{
	struct community c;
	size_t	i;
	u_int64_t ext;
	u_int8_t type = 0, nt;

	if (len % sizeof(c))
		return;

	for (i = 0; i < len; i += sizeof(c)) {
		memcpy(&c, data + i, sizeof(c));

		nt = c.flags;
		if (type != nt) {
			if (type != 0)
				printf("%c", EOL0(flag0));
			printf("    %s:", print_attr(nt,
			    ATTR_OPTIONAL | ATTR_TRANSITIVE));
			type = nt;
		}
		printf(" ");

		switch (nt) {
		case COMMUNITY_TYPE_BASIC:
			print_community(c.data1, c.data2);
			break;
		case COMMUNITY_TYPE_LARGE:
			printf("%u:%u:%u", c.data1, c.data2, c.data3);
			break;
		case COMMUNITY_TYPE_EXT:
			ext = (u_int64_t)c.data3 << 48;
			switch (c.data3 >> 8) {
			case EXT_COMMUNITY_TRANS_TWO_AS:
			case EXT_COMMUNITY_TRANS_OPAQUE:
			case EXT_COMMUNITY_TRANS_EVPN:
			case EXT_COMMUNITY_NON_TRANS_OPAQUE:
				ext |= ((u_int64_t)c.data1 & 0xffff) << 32;
				ext |= (u_int64_t)c.data2;
				break;
			case EXT_COMMUNITY_TRANS_FOUR_AS:
			case EXT_COMMUNITY_TRANS_IPV4:
				ext |= (u_int64_t)c.data1 << 16;
				ext |= (u_int64_t)c.data2 & 0xffff;
				break;
			}
			ext = htobe64(ext);

			print_ext_community((void *)&ext);
			break;
		}
	}

	printf("%c", EOL0(flag0));
}

void
show_community(u_char *data, u_int16_t len)
{
	u_int16_t	a, v;
	u_int16_t	i;

	if (len & 0x3)
		return;

	for (i = 0; i < len; i += 4) {
		memcpy(&a, data + i, sizeof(a));
		memcpy(&v, data + i + 2, sizeof(v));
		a = ntohs(a);
		v = ntohs(v);
		print_community(a, v);
		if (i + 4 < len)
			printf(" ");
	}
}

void
show_large_community(u_char *data, u_int16_t len)
{
	u_int32_t	a, l1, l2;
	u_int16_t	i;

	if (len % 12)
		return;

	for (i = 0; i < len; i += 12) {
		memcpy(&a, data + i, sizeof(a));
		memcpy(&l1, data + i + 4, sizeof(l1));
		memcpy(&l2, data + i + 8, sizeof(l2));
		a = ntohl(a);
		l1 = ntohl(l1);
		l2 = ntohl(l2);
		printf("%u:%u:%u", a, l1, l2);

		if (i + 12 < len)
			printf(" ");
	}
}

void
show_ext_community(u_char *data, u_int16_t len)
{
	u_int16_t	i;

	if (len & 0x7)
		return;

	for (i = 0; i < len; i += 8) {
		print_ext_community(data + i);

		if (i + 8 < len)
			printf(" ");
	}
}

const char *
fmt_mem(long long num)
{
	static char	buf[16];

	if (fmt_scaled(num, buf) == -1)
		snprintf(buf, sizeof(buf), "%lldB", num);

	return (buf);
}

void
send_filterset(struct imsgbuf *i, struct filter_set_head *set)
{
	struct filter_set	*s;

	while ((s = TAILQ_FIRST(set)) != NULL) {
		imsg_compose(i, IMSG_FILTER_SET, 0, 0, -1, s,
		    sizeof(struct filter_set));
		TAILQ_REMOVE(set, s, entry);
		free(s);
	}
}

const char *
get_errstr(u_int8_t errcode, u_int8_t subcode)
{
	static const char	*errstr = NULL;

	if (errcode && errcode < sizeof(errnames)/sizeof(char *))
		errstr = errnames[errcode];

	switch (errcode) {
	case ERR_HEADER:
		if (subcode &&
		    subcode < sizeof(suberr_header_names)/sizeof(char *))
			errstr = suberr_header_names[subcode];
		break;
	case ERR_OPEN:
		if (subcode &&
		    subcode < sizeof(suberr_open_names)/sizeof(char *))
			errstr = suberr_open_names[subcode];
		break;
	case ERR_UPDATE:
		if (subcode &&
		    subcode < sizeof(suberr_update_names)/sizeof(char *))
			errstr = suberr_update_names[subcode];
		break;
	case ERR_HOLDTIMEREXPIRED:
	case ERR_FSM:
	case ERR_CEASE:
		break;
	default:
		return ("unknown error code");
	}

	return (errstr);
}

void
network_bulk(struct parse_result *res)
{
	struct network_config net;
	struct filter_set *s = NULL;
	struct bgpd_addr h;
	char *line = NULL;
	size_t linesize = 0;
	ssize_t linelen;
	u_int8_t len;
	FILE *f;

	if ((f = fdopen(STDIN_FILENO, "r")) == NULL)
		err(1, "Failed to open stdin\n");

	while ((linelen = getline(&line, &linesize, f)) != -1) {
		char *b, *buf = line;
		while ((b = strsep(&buf, " \t\n")) != NULL) {
			if (*b == '\0')	/* skip empty tokens */
				continue;
			/* Stop processing after a comment */
			if (*b == '#')
				break;
			bzero(&net, sizeof(net));
			if (parse_prefix(b, strlen(b), &h, &len) != 1)
				errx(1, "bad prefix: %s", b);
			net.prefix = h;
			net.prefixlen = len;
			net.rd = res->rd;

			if (res->action == NETWORK_BULK_ADD) {
				imsg_compose(ibuf, IMSG_NETWORK_ADD,
				    0, 0, -1, &net, sizeof(net));
				/*
				 * can't use send_filterset since that
				 * would free the set.
				 */
				TAILQ_FOREACH(s, &res->set, entry) {
					imsg_compose(ibuf,
					    IMSG_FILTER_SET,
					    0, 0, -1, s, sizeof(*s));
				}
				imsg_compose(ibuf, IMSG_NETWORK_DONE,
				    0, 0, -1, NULL, 0);
			} else
				imsg_compose(ibuf, IMSG_NETWORK_REMOVE,
				     0, 0, -1, &net, sizeof(net));
		}
	}
	free(line);
	if (ferror(f))
		err(1, "getline");
	fclose(f);
}

void
show_mrt_dump_neighbors(struct mrt_rib *mr, struct mrt_peer *mp, void *arg)
{
	struct mrt_peer_entry *p;
	struct in_addr ina;
	u_int16_t i;

	ina.s_addr = htonl(mp->bgp_id);
	printf("view: %s BGP ID: %s Number of peers: %u\n\n",
	    mp->view, inet_ntoa(ina), mp->npeers);
	printf("%-30s %8s %15s\n", "Neighbor", "AS", "BGP ID");
	for (i = 0; i < mp->npeers; i++) {
		p = &mp->peers[i];
		ina.s_addr = htonl(p->bgp_id);
		printf("%-30s %8u %15s\n", log_addr(&p->addr), p->asnum,
		    inet_ntoa(ina));
	}
	/* we only print the first message */
	exit(0);
}

void
show_mrt_dump(struct mrt_rib *mr, struct mrt_peer *mp, void *arg)
{
	struct ctl_show_rib		 ctl;
	struct parse_result		 res;
	struct ctl_show_rib_request	*req = arg;
	struct mrt_rib_entry		*mre;
	u_int16_t			 i, j;

	memset(&res, 0, sizeof(res));
	res.flags = req->flags;

	for (i = 0; i < mr->nentries; i++) {
		mre = &mr->entries[i];
		bzero(&ctl, sizeof(ctl));
		ctl.prefix = mr->prefix;
		ctl.prefixlen = mr->prefixlen;
		ctl.lastchange = mre->originated;
		ctl.true_nexthop = mre->nexthop;
		ctl.exit_nexthop = mre->nexthop;
		ctl.origin = mre->origin;
		ctl.local_pref = mre->local_pref;
		ctl.med = mre->med;
		/* weight is not part of the mrt dump so it can't be set */

		if (mre->peer_idx < mp->npeers) {
			ctl.remote_addr = mp->peers[mre->peer_idx].addr;
			ctl.remote_id = mp->peers[mre->peer_idx].bgp_id;
		}

		/* filter by neighbor */
		if (req->neighbor.addr.aid != AID_UNSPEC &&
		    memcmp(&req->neighbor.addr, &ctl.remote_addr,
		    sizeof(ctl.remote_addr)) != 0)
			continue;
		/* filter by AF */
		if (req->aid && req->aid != ctl.prefix.aid)
			return;
		/* filter by prefix */
		if (req->prefix.aid != AID_UNSPEC) {
			if (req->flags & F_LONGER) {
				if (req->prefixlen > ctl.prefixlen)
					return;
				if (prefix_compare(&req->prefix, &ctl.prefix,
				    req->prefixlen))
					return;
			} else if (req->flags & F_SHORTER) {
				if (req->prefixlen < ctl.prefixlen)
					return;
				if (prefix_compare(&req->prefix, &ctl.prefix,
				    ctl.prefixlen))
					return;
			} else {
				if (req->prefixlen != ctl.prefixlen)
					return;
				if (prefix_compare(&req->prefix, &ctl.prefix,
				    req->prefixlen))
					return;
			}
		}
		/* filter by AS */
		if (req->as.type != AS_UNDEF &&
		    !match_aspath(mre->aspath, mre->aspath_len, &req->as))
			continue;

		show_rib(&ctl, mre->aspath, mre->aspath_len, &res);
		if (req->flags & F_CTL_DETAIL) {
			for (j = 0; j < mre->nattrs; j++)
				show_attr(mre->attrs[j].attr,
				    mre->attrs[j].attr_len,
				    req->flags);
		}
	}
}

void
network_mrt_dump(struct mrt_rib *mr, struct mrt_peer *mp, void *arg)
{
	struct ctl_show_rib		 ctl;
	struct network_config		 net;
	struct ctl_show_rib_request	*req = arg;
	struct mrt_rib_entry		*mre;
	struct ibuf			*msg;
	u_int16_t			 i, j;

	for (i = 0; i < mr->nentries; i++) {
		mre = &mr->entries[i];
		bzero(&ctl, sizeof(ctl));
		ctl.prefix = mr->prefix;
		ctl.prefixlen = mr->prefixlen;
		ctl.lastchange = mre->originated;
		ctl.true_nexthop = mre->nexthop;
		ctl.exit_nexthop = mre->nexthop;
		ctl.origin = mre->origin;
		ctl.local_pref = mre->local_pref;
		ctl.med = mre->med;

		if (mre->peer_idx < mp->npeers) {
			ctl.remote_addr = mp->peers[mre->peer_idx].addr;
			ctl.remote_id = mp->peers[mre->peer_idx].bgp_id;
		}

		/* filter by neighbor */
		if (req->neighbor.addr.aid != AID_UNSPEC &&
		    memcmp(&req->neighbor.addr, &ctl.remote_addr,
		    sizeof(ctl.remote_addr)) != 0)
			continue;
		/* filter by AF */
		if (req->aid && req->aid != ctl.prefix.aid)
			return;
		/* filter by prefix */
		if (req->prefix.aid != AID_UNSPEC) {
			if (!prefix_compare(&req->prefix, &ctl.prefix,
			    req->prefixlen)) {
				if (req->flags & F_LONGER) {
					if (req->prefixlen > ctl.prefixlen)
						return;
				} else if (req->prefixlen != ctl.prefixlen)
					return;
			} else
				return;
		}
		/* filter by AS */
		if (req->as.type != AS_UNDEF &&
		    !match_aspath(mre->aspath, mre->aspath_len, &req->as))
			continue;

		bzero(&net, sizeof(net));
		net.prefix = ctl.prefix;
		net.prefixlen = ctl.prefixlen;
		net.type = NETWORK_MRTCLONE;
		/* XXX rd can't be set and will be 0 */

		imsg_compose(ibuf, IMSG_NETWORK_ADD, 0, 0, -1,
		    &net, sizeof(net));
		if ((msg = imsg_create(ibuf, IMSG_NETWORK_ASPATH,
		    0, 0, sizeof(ctl) + mre->aspath_len)) == NULL)
			errx(1, "imsg_create failure");
		if (imsg_add(msg, &ctl, sizeof(ctl)) == -1 ||
		    imsg_add(msg, mre->aspath, mre->aspath_len) == -1)
			errx(1, "imsg_add failure");
		imsg_close(ibuf, msg);
		for (j = 0; j < mre->nattrs; j++)
			imsg_compose(ibuf, IMSG_NETWORK_ATTR, 0, 0, -1,
			    mre->attrs[j].attr, mre->attrs[j].attr_len);
		imsg_compose(ibuf, IMSG_NETWORK_DONE, 0, 0, -1, NULL, 0);

		while (ibuf->w.queued) {
			if (msgbuf_write(&ibuf->w) <= 0 && errno != EAGAIN)
				err(1, "write error");
		}
	}
}

static const char *
print_time(struct timespec *t)
{
	static char timebuf[32];
	static struct timespec prevtime;
	struct timespec temp;

	timespecsub(t, &prevtime, &temp);
	snprintf(timebuf, sizeof(timebuf), "%lld.%06ld",
	    (long long)temp.tv_sec, temp.tv_nsec / 1000);
	prevtime = *t;
	return (timebuf);
}

void
show_mrt_state(struct mrt_bgp_state *ms, void *arg)
{
	printf("%s %s[%u] -> ", print_time(&ms->time),
	    log_addr(&ms->src), ms->src_as);
	printf("%s[%u]: %s -> %s\n", log_addr(&ms->dst), ms->dst_as,
	    statenames[ms->old_state], statenames[ms->new_state]);
}

static void
print_afi(u_char *p, u_int8_t len)
{
	u_int16_t afi;
	u_int8_t safi, aid;

	if (len != 4) {
		printf("bad length");
		return;
	}

	/* afi, 2 byte */
	memcpy(&afi, p, sizeof(afi));
	afi = ntohs(afi);
	p += 2;
	/* reserved, 1 byte */
	p += 1;
	/* safi, 1 byte */
	memcpy(&safi, p, sizeof(safi));
	if (afi2aid(afi, safi, &aid) == -1)
		printf("unkown afi %u safi %u", afi, safi);
	else
		printf("%s", aid2str(aid));
}

static void
print_capability(u_int8_t capa_code, u_char *p, u_int8_t len)
{
	switch (capa_code) {
	case CAPA_MP:
		printf("multiprotocol capability: ");
		print_afi(p, len);
		break;
	case CAPA_REFRESH:
		printf("route refresh capability");
		break;
	case CAPA_RESTART:
		printf("graceful restart capability");
		/* XXX there is more needed here */
		break;
	case CAPA_AS4BYTE:
		printf("4-byte AS num capability: ");
		if (len == 4) {
			u_int32_t as;
			memcpy(&as, p, sizeof(as));
			as = ntohl(as);
			printf("AS %u", as);
		} else
			printf("bad length");
		break;
	default:
		printf("unknown capability %u length %u", capa_code, len);
		break;
	}
}

static void
print_notification(u_int8_t errcode, u_int8_t subcode)
{
	const char *suberrname = NULL;
	int uk = 0;

	switch (errcode) {
	case ERR_HEADER:
		if (subcode >= sizeof(suberr_header_names)/sizeof(char *))
			uk = 1;
		else
			suberrname = suberr_header_names[subcode];
		break;
	case ERR_OPEN:
		if (subcode >= sizeof(suberr_open_names)/sizeof(char *))
			uk = 1;
		else
			suberrname = suberr_open_names[subcode];
		break;
	case ERR_UPDATE:
		if (subcode >= sizeof(suberr_update_names)/sizeof(char *))
			uk = 1;
		else
			suberrname = suberr_update_names[subcode];
		break;
	case ERR_CEASE:
		if (subcode >= sizeof(suberr_cease_names)/sizeof(char *))
			uk = 1;
		else
			suberrname = suberr_cease_names[subcode];
		break;
	case ERR_HOLDTIMEREXPIRED:
		if (subcode != 0)
			uk = 1;
		break;
	case ERR_FSM:
		if (subcode >= sizeof(suberr_fsm_names)/sizeof(char *))
			uk = 1;
		else
			suberrname = suberr_fsm_names[subcode];
		break;
	default:
		printf("unknown errcode %u, subcode %u",
		    errcode, subcode);
		return;
	}

	if (uk)
		printf("%s, unknown subcode %u", errnames[errcode], subcode);
	else {
		if (suberrname == NULL)
			printf("%s", errnames[errcode]);
		else
			printf("%s, %s", errnames[errcode], suberrname);
	}
}

static int
show_mrt_capabilities(u_char *p, u_int16_t len)
{
	u_int16_t totlen = len;
	u_int8_t capa_code, capa_len;

	while (len > 2) {
		memcpy(&capa_code, p, sizeof(capa_code));
		p += sizeof(capa_code);
		len -= sizeof(capa_code);
		memcpy(&capa_len, p, sizeof(capa_len));
		p += sizeof(capa_len);
		len -= sizeof(capa_len);
		if (len < capa_len) {
			printf("capa_len %u exceeds remaining length",
			    capa_len);
			return (-1);
		}
		printf("\n        ");
		print_capability(capa_code, p, capa_len);
		p += capa_len;
		len -= capa_len;
	}
	if (len != 0) {
		printf("length missmatch while capability parsing");
		return (-1);
	}
	return (totlen);
}

static void
show_mrt_open(u_char *p, u_int16_t len)
{
	u_int8_t version, optparamlen;
	u_int16_t short_as, holdtime;
	struct in_addr bgpid;

	/* length check up to optparamlen already happened */
	memcpy(&version, p, sizeof(version));
	p += sizeof(version);
	len -= sizeof(version);
	memcpy(&short_as, p, sizeof(short_as));
	p += sizeof(short_as);
	len -= sizeof(short_as);
	short_as = ntohs(short_as);
	memcpy(&holdtime, p, sizeof(holdtime));
	holdtime = ntohs(holdtime);
	p += sizeof(holdtime);
	len -= sizeof(holdtime);
	memcpy(&bgpid, p, sizeof(bgpid));
	p += sizeof(bgpid);
	len -= sizeof(bgpid);
	memcpy(&optparamlen, p, sizeof(optparamlen));
	p += sizeof(optparamlen);
	len -= sizeof(optparamlen);

	printf("\n    ");
	printf("Version: %d AS: %u Holdtime: %u BGP Id: %s Paramlen: %u",
	    version, short_as, holdtime, inet_ntoa(bgpid), optparamlen);
	if (optparamlen != len) {
		printf("optional parameter length mismatch");
		return;
	}
	while (len > 2) {
		u_int8_t op_type, op_len;
		int r;

		memcpy(&op_type, p, sizeof(op_type));
		p += sizeof(op_type);
		len -= sizeof(op_type);
		memcpy(&op_len, p, sizeof(op_len));
		p += sizeof(op_len);
		len -= sizeof(op_len);

		printf("\n    ");
		switch (op_type) {
		case OPT_PARAM_CAPABILITIES:
			printf("Capabilities: size %u", op_len);
			r = show_mrt_capabilities(p, op_len);
			if (r == -1)
				return;
			p += r;
			len -= r;
			break;
		case OPT_PARAM_AUTH:
		default:
			printf("unsupported optional parameter: type %u",
			    op_type);
			return;
		}
	}
	if (len != 0) {
		printf("optional parameter encoding error");
		return;
	}
}

static void
show_mrt_notification(u_char *p, u_int16_t len)
{
	u_int16_t i;
	u_int8_t errcode, subcode;
	size_t shutcomm_len;
	char shutcomm[SHUT_COMM_LEN];

	memcpy(&errcode, p, sizeof(errcode));
	p += sizeof(errcode);
	len -= sizeof(errcode);

	memcpy(&subcode, p, sizeof(subcode));
	p += sizeof(subcode);
	len -= sizeof(subcode);

	printf("\n    ");
	print_notification(errcode, subcode);

	if (errcode == ERR_CEASE && (subcode == ERR_CEASE_ADMIN_DOWN ||
	    subcode == ERR_CEASE_ADMIN_RESET)) {
		if (len > 1) {
			shutcomm_len = *p++;
			len--;
			if (len < shutcomm_len) {
				printf("truncated shutdown reason");
				return;
			}
			if (shutcomm_len > SHUT_COMM_LEN - 1) {
				printf("overly long shutdown reason");
				return;
			}
			memcpy(shutcomm, p, shutcomm_len);
			shutcomm[shutcomm_len] = '\0';
			printf("shutdown reason: \"%s\"",
			    log_shutcomm(shutcomm));
			p += shutcomm_len;
			len -= shutcomm_len;
		}
	}
	if (errcode == ERR_OPEN && subcode == ERR_OPEN_CAPA) {
		int r;

		r = show_mrt_capabilities(p, len);
		if (r == -1)
			return;
		p += r;
		len -= r;
	}

	if (len > 0) {
		printf("\n    additional data %u bytes", len);
		for (i = 0; i < len; i++) {
			if (i % 16 == 0)
				printf("\n    ");
			if (i % 8 == 0)
				printf("   ");
			printf(" %02X", *p++);
		}
	}
}

static void
show_mrt_update(u_char *p, u_int16_t len)
{
	struct bgpd_addr prefix;
	int pos;
	u_int16_t wlen, alen;
	u_int8_t prefixlen;

	if (len < sizeof(wlen)) {
		printf("bad length");
		return;
	}
	memcpy(&wlen, p, sizeof(wlen));
	wlen = ntohs(wlen);
	p += sizeof(wlen);
	len -= sizeof(wlen);

	if (len < wlen) {
		printf("bad withdraw length");
		return;
	}
	if (wlen > 0) {
		printf("\n     Withdrawn prefixes:");
		while (wlen > 0) {
			if ((pos = nlri_get_prefix(p, wlen, &prefix,
			    &prefixlen)) == -1) {
				printf("bad withdraw prefix");
				return;
			}
			printf(" %s/%u", log_addr(&prefix), prefixlen);
			p += pos;
			len -= pos;
			wlen -= pos;
		}
	}

	if (len < sizeof(alen)) {
		printf("bad length");
		return;
	}
	memcpy(&alen, p, sizeof(alen));
	alen = ntohs(alen);
	p += sizeof(alen);
	len -= sizeof(alen);

	if (len < alen) {
		printf("bad attribute length");
		return;
	}
	printf("\n");
	/* alen attributes here */
	while (alen > 3) {
		u_int8_t flags;
		u_int16_t attrlen;

		flags = p[0];
		/* type = p[1]; */

		/* get the attribute length */
		if (flags & ATTR_EXTLEN) {
			if (len < sizeof(attrlen) + 2)
				printf("bad attribute length");
			memcpy(&attrlen, &p[2], sizeof(attrlen));
			attrlen = ntohs(attrlen);
			attrlen += sizeof(attrlen) + 2;
		} else {
			attrlen = p[2];
			attrlen += 1 + 2;
		}

		show_attr(p, attrlen, 0);
		p += attrlen;
		alen -= attrlen;
		len -= attrlen;
	}

	if (len > 0) {
		printf("    NLRI prefixes:");
		while (len > 0) {
			if ((pos = nlri_get_prefix(p, len, &prefix,
			    &prefixlen)) == -1) {
				printf("bad withdraw prefix");
				return;
			}
			printf(" %s/%u", log_addr(&prefix), prefixlen);
			p += pos;
			len -= pos;
		}
	}
}

void
show_mrt_msg(struct mrt_bgp_msg *mm, void *arg)
{
	static const u_int8_t marker[MSGSIZE_HEADER_MARKER] = {
	    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
	u_char *p;
	u_int16_t len;
	u_int8_t type;

	printf("%s %s[%u] -> ", print_time(&mm->time),
	    log_addr(&mm->src), mm->src_as);
	printf("%s[%u]: size %u ", log_addr(&mm->dst), mm->dst_as, mm->msg_len);
	p = mm->msg;
	len = mm->msg_len;

	if (len < MSGSIZE_HEADER) {
		printf("illegal header length: %u byte\n", len);
		return;
	}

	/* parse BGP message header */
	if (memcmp(p, marker, sizeof(marker))) {
		printf("incorrect marker in BGP message\n");
		return;
	}
	p += MSGSIZE_HEADER_MARKER;

	memcpy(&len, p, 2);
	len = ntohs(len);
	p += 2;
	memcpy(&type, p, 1);
	p += 1;

	if (len < MSGSIZE_HEADER || len > MAX_PKTSIZE) {
		printf("illegal header length: %u byte\n", len);
		return;
	}

	switch (type) {
	case OPEN:
		printf("%s ", msgtypenames[type]);
		if (len < MSGSIZE_OPEN_MIN) {
			printf("illegal length: %u byte\n", len);
			return;
		}
		show_mrt_open(p, len - MSGSIZE_HEADER);
		break;
	case NOTIFICATION:
		printf("%s ", msgtypenames[type]);
		if (len < MSGSIZE_NOTIFICATION_MIN) {
			printf("illegal length: %u byte\n", len);
			return;
		}
		show_mrt_notification(p, len - MSGSIZE_HEADER);
		break;
	case UPDATE:
		printf("%s ", msgtypenames[type]);
		if (len < MSGSIZE_UPDATE_MIN) {
			printf("illegal length: %u byte\n", len);
			return;
		}
		show_mrt_update(p, len - MSGSIZE_HEADER);
		break;
	case KEEPALIVE:
		printf("%s ", msgtypenames[type]);
		if (len != MSGSIZE_KEEPALIVE) {
			printf("illegal length: %u byte\n", len);
			return;
		}
		/* nothing */
		break;
	case RREFRESH:
		printf("%s ", msgtypenames[type]);
		if (len != MSGSIZE_RREFRESH) {
			printf("illegal length: %u byte\n", len);
			return;
		}
		print_afi(p, len);
		break;
	default:
		printf("unknown type %u\n", type);
		return;
	}
	printf("\n");
}

const char *
msg_type(u_int8_t type)
{
	if (type >= sizeof(msgtypenames)/sizeof(msgtypenames[0]))
		return "BAD";
	return (msgtypenames[type]);
}

int
match_aspath(void *data, u_int16_t len, struct filter_as *f)
{
	u_int8_t	*seg;
	int		 final;
	u_int16_t	 seg_size;
	u_int8_t	 i, seg_len;
	u_int32_t	 as = 0;

	if (f->type == AS_EMPTY) {
		if (len == 0)
			return (1);
		else
			return (0);
	}

	seg = data;

	/* just check the leftmost AS */
	if (f->type == AS_PEER && len >= 6) {
		as = aspath_extract(seg, 0);
		if (f->as_min == as)
			return (1);
		else
			return (0);
	}

	for (; len >= 6; len -= seg_size, seg += seg_size) {
		seg_len = seg[1];
		seg_size = 2 + sizeof(u_int32_t) * seg_len;

		final = (len == seg_size);

		if (f->type == AS_SOURCE) {
			/*
			 * Just extract the rightmost AS
			 * but if that segment is an AS_SET then the rightmost
			 * AS of a previous AS_SEQUENCE segment should be used.
			 * Because of that just look at AS_SEQUENCE segments.
			 */
			if (seg[0] == AS_SEQUENCE)
				as = aspath_extract(seg, seg_len - 1);
			/* not yet in the final segment */
			if (!final)
				continue;
			if (f->as_min == as)
				return (1);
			else
				return (0);
		}
		/* AS_TRANSIT or AS_ALL */
		for (i = 0; i < seg_len; i++) {
			/*
			 * the source (rightmost) AS is excluded from
			 * AS_TRANSIT matches.
			 */
			if (final && i == seg_len - 1 && f->type == AS_TRANSIT)
				return (0);
			as = aspath_extract(seg, i);
			if (f->as_min == as)
				return (1);
		}
	}
	return (0);
}
