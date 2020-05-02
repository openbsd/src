/*	$OpenBSD: bgpctl.c,v 1.262 2020/05/02 14:33:33 claudio Exp $ */

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
const struct output	*output = &show_output;
int tableid;
int nodescr;

__dead void
usage(void)
{
	extern char	*__progname;

	fprintf(stderr, "usage: %s [-jn] [-s socket] command [argument ...]\n",
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

	while ((ch = getopt(argc, argv, "jns:")) != -1) {
		switch (ch) {
		case 'n':
			if (++nodescr > 1)
				usage();
			break;
		case 'j':
			output = &json_output;
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
			output->head(res);
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

	output->head(res);

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

	output->tail();

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
		output->neighbor(p, res);
		break;
	case IMSG_CTL_SHOW_TIMER:
		t = imsg->data;
		if (t->type > 0 && t->type < Timer_Max)
			output->timer(t);
		break;
	case IMSG_CTL_SHOW_INTERFACE:
		iface = imsg->data;
		output->interface(iface);
		break;
	case IMSG_CTL_SHOW_NEXTHOP:
		nh = imsg->data;
		output->nexthop(nh);
		break;
	case IMSG_CTL_KROUTE:
	case IMSG_CTL_SHOW_NETWORK:
		if (imsg->hdr.len < IMSG_HEADER_SIZE + sizeof(*kf))
			errx(1, "wrong imsg len");
		kf = imsg->data;
		output->fib(kf);
		break;
	case IMSG_CTL_SHOW_FIB_TABLES:
		if (imsg->hdr.len < IMSG_HEADER_SIZE + sizeof(*kt))
			errx(1, "wrong imsg len");
		kt = imsg->data;
		output->fib_table(kt);
		break;
	case IMSG_CTL_SHOW_RIB:
		if (imsg->hdr.len < IMSG_HEADER_SIZE + sizeof(rib))
			errx(1, "wrong imsg len");
		memcpy(&rib, imsg->data, sizeof(rib));
		aslen = imsg->hdr.len - IMSG_HEADER_SIZE - sizeof(rib);
		asdata = imsg->data;
		asdata += sizeof(rib);
		output->rib(&rib, asdata, aslen, res);
		break;
	case IMSG_CTL_SHOW_RIB_COMMUNITIES:
		ilen = imsg->hdr.len - IMSG_HEADER_SIZE;
		if (ilen % sizeof(struct community)) {
			warnx("bad IMSG_CTL_SHOW_RIB_COMMUNITIES received");
			break;
		}
		output->communities(imsg->data, ilen, res);
		break;
	case IMSG_CTL_SHOW_RIB_ATTR:
		ilen = imsg->hdr.len - IMSG_HEADER_SIZE;
		if (ilen < 3) {
			warnx("bad IMSG_CTL_SHOW_RIB_ATTR received");
			break;
		}
		output->attr(imsg->data, ilen, res);
		break;
	case IMSG_CTL_SHOW_RIB_MEM:
		memcpy(&stats, imsg->data, sizeof(stats));
		output->rib_mem(&stats);
		break;
	case IMSG_CTL_SHOW_RIB_HASH:
		memcpy(&hash, imsg->data, sizeof(hash));
		output->rib_hash(&hash);
		break;
	case IMSG_CTL_RESULT:
		if (imsg->hdr.len != IMSG_HEADER_SIZE + sizeof(rescode)) {
			warnx("got IMSG_CTL_RESULT with wrong len");
			break;
		}
		memcpy(&rescode, imsg->data, sizeof(rescode));
		output->result(rescode);
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
fmt_auth_method(enum auth_method method)
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

#define TF_BUFS	8
#define TF_LEN	9

const char *
fmt_timeframe(time_t t)
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
fmt_monotime(time_t t)
{
	struct timespec ts;

	if (t == 0)
		return ("Never");

	if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
		err(1, "clock_gettime");
	if (t > ts.tv_sec)	/* time in the future is not possible */
		t = ts.tv_sec;
	return (fmt_timeframe(ts.tv_sec - t));
}

const char *
fmt_fib_flags(u_int16_t flags)
{
	static char buf[8];

	if (flags & F_DOWN)
		strlcpy(buf, " ", sizeof(buf));
	else
		strlcpy(buf, "*", sizeof(buf));

	if (flags & F_BGPD_INSERTED)
		strlcat(buf, "B", sizeof(buf));
	else if (flags & F_CONNECTED)
		strlcat(buf, "C", sizeof(buf));
	else if (flags & F_STATIC)
		strlcat(buf, "S", sizeof(buf));
	else if (flags & F_DYNAMIC)
		strlcat(buf, "D", sizeof(buf));
	else
		strlcat(buf, " ", sizeof(buf));

	if (flags & F_NEXTHOP)
		strlcat(buf, "N", sizeof(buf));
	else
		strlcat(buf, " ", sizeof(buf));

	if (flags & F_REJECT && flags & F_BLACKHOLE)
		strlcat(buf, "f", sizeof(buf));
	else if (flags & F_REJECT)
		strlcat(buf, "r", sizeof(buf));
	else if (flags & F_BLACKHOLE)
		strlcat(buf, "b", sizeof(buf));
	else
		strlcat(buf, " ", sizeof(buf));

	if (strlcat(buf, " ", sizeof(buf)) >= sizeof(buf))
		errx(1, "%s buffer too small", __func__);

	return buf;
}

const char *
fmt_origin(u_int8_t origin, int sum)
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

const char *
fmt_flags(u_int8_t flags, int sum)
{
	static char buf[80];
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
		snprintf(buf, sizeof(buf), "%-5s", flagstr);
	} else {
		if (flags & F_PREF_INTERNAL)
			strlcpy(buf, "internal", sizeof(buf));
		else
			strlcpy(buf, "external", sizeof(buf));

		if (flags & F_PREF_STALE)
			strlcat(buf, ", stale", sizeof(buf));
		if (flags & F_PREF_ELIGIBLE)
			strlcat(buf, ", valid", sizeof(buf));
		if (flags & F_PREF_ACTIVE)
			strlcat(buf, ", best", sizeof(buf));
		if (flags & F_PREF_ANNOUNCE)
			strlcat(buf, ", announced", sizeof(buf));
		if (strlen(buf) >= sizeof(buf) - 1)
			errx(1, "%s buffer too small", __func__);
	}

	return buf;
}

const char *
fmt_ovs(u_int8_t validation_state, int sum)
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

const char *
fmt_mem(long long num)
{
	static char	buf[16];

	if (fmt_scaled(num, buf) == -1)
		snprintf(buf, sizeof(buf), "%lldB", num);

	return (buf);
}

const char *
fmt_errstr(u_int8_t errcode, u_int8_t subcode)
{
	static char	 errbuf[256];
	const char	*errstr = NULL;
	const char	*suberr = NULL;
	int		 uk = 0;

	if (errcode == 0)	/* no error */
		return NULL;

	if (errcode < sizeof(errnames)/sizeof(char *))
		errstr = errnames[errcode];

	switch (errcode) {
	case ERR_HEADER:
		if (subcode < sizeof(suberr_header_names)/sizeof(char *))
			suberr = suberr_header_names[subcode];
		else
			uk = 1;
		break;
	case ERR_OPEN:
		if (subcode < sizeof(suberr_open_names)/sizeof(char *))
			suberr = suberr_open_names[subcode];
		else
			uk = 1;
		break;
	case ERR_UPDATE:
		if (subcode < sizeof(suberr_update_names)/sizeof(char *))
			suberr = suberr_update_names[subcode];
		else
			uk = 1;
		break;
	case ERR_HOLDTIMEREXPIRED:
		if (subcode != 0)
			uk = 1;
		break;
	case ERR_FSM:
		if (subcode < sizeof(suberr_fsm_names)/sizeof(char *))
			suberr = suberr_fsm_names[subcode];
		else
			uk = 1;
		break;
	case ERR_CEASE:
		if (subcode < sizeof(suberr_cease_names)/sizeof(char *))
			suberr = suberr_cease_names[subcode];
		else
			uk = 1;
		break;
	default:
		snprintf(errbuf, sizeof(errbuf),
		    "unknown error code %u subcode %u", errcode, subcode);
		return (errbuf);
	}

	if (uk)
		snprintf(errbuf, sizeof(errbuf),
		    "%s, unknown subcode %u", errstr, subcode);
	else if (suberr == NULL)
		return (errstr);
	else
		snprintf(errbuf, sizeof(errbuf),
		    "%s, %s", errstr, suberr);

	return (errbuf);
}

const char *
fmt_attr(u_int8_t type, int flags)
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
	if (flags != -1 && pflags) {
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

const char *
fmt_community(u_int16_t a, u_int16_t v)
{
	static char buf[12];

	if (a == COMMUNITY_WELLKNOWN)
		switch (v) {
		case COMMUNITY_GRACEFUL_SHUTDOWN:
			return "GRACEFUL_SHUTDOWN";
		case COMMUNITY_NO_EXPORT:
			return "NO_EXPORT";
		case COMMUNITY_NO_ADVERTISE:
			return "NO_ADVERTISE";
		case COMMUNITY_NO_EXPSUBCONFED:
			return "NO_EXPORT_SUBCONFED";
		case COMMUNITY_NO_PEER:
			return "NO_PEER";
		case COMMUNITY_BLACKHOLE:
			return "BLACKHOLE";
		default:
			break;
		}

	snprintf(buf, sizeof(buf), "%hu:%hu", a, v);
	return buf;
}

const char *
fmt_large_community(u_int32_t d1, u_int32_t d2, u_int32_t d3)
{
	static char buf[33];

	snprintf(buf, sizeof(buf), "%u:%u:%u", d1, d2, d3);
	return buf;
}

const char *
fmt_ext_community(u_int8_t *data)
{
	static char	buf[32];
	u_int64_t	ext;
	struct in_addr	ip;
	u_int32_t	as4, u32;
	u_int16_t	as2, u16;
	u_int8_t	type, subtype;

	type = data[0];
	subtype = data[1];

	switch (type) {
	case EXT_COMMUNITY_TRANS_TWO_AS:
		memcpy(&as2, data + 2, sizeof(as2));
		memcpy(&u32, data + 4, sizeof(u32));
		snprintf(buf, sizeof(buf), "%s %s:%u",
		    log_ext_subtype(type, subtype),
		    log_as(ntohs(as2)), ntohl(u32));
		return buf;
	case EXT_COMMUNITY_TRANS_IPV4:
		memcpy(&ip, data + 2, sizeof(ip));
		memcpy(&u16, data + 6, sizeof(u16));
		snprintf(buf, sizeof(buf), "%s %s:%hu",
		    log_ext_subtype(type, subtype),
		    inet_ntoa(ip), ntohs(u16));
		return buf;
	case EXT_COMMUNITY_TRANS_FOUR_AS:
		memcpy(&as4, data + 2, sizeof(as4));
		memcpy(&u16, data + 6, sizeof(u16));
		snprintf(buf, sizeof(buf), "%s %s:%hu",
		    log_ext_subtype(type, subtype),
		    log_as(ntohl(as4)), ntohs(u16));
		return buf;
	case EXT_COMMUNITY_TRANS_OPAQUE:
	case EXT_COMMUNITY_TRANS_EVPN:
		memcpy(&ext, data, sizeof(ext));
		ext = be64toh(ext) & 0xffffffffffffLL;
		snprintf(buf, sizeof(buf), "%s 0x%llx",
		    log_ext_subtype(type, subtype), (unsigned long long)ext);
		return buf;
	case EXT_COMMUNITY_NON_TRANS_OPAQUE:
		memcpy(&ext, data, sizeof(ext));
		ext = be64toh(ext) & 0xffffffffffffLL;
		switch (ext) {
		case EXT_COMMUNITY_OVS_VALID:
			snprintf(buf, sizeof(buf), "%s valid ",
			    log_ext_subtype(type, subtype));
			return buf;
		case EXT_COMMUNITY_OVS_NOTFOUND:
			snprintf(buf, sizeof(buf), "%s not-found ",
			    log_ext_subtype(type, subtype));
			return buf;
		case EXT_COMMUNITY_OVS_INVALID:
			snprintf(buf, sizeof(buf), "%s invalid ",
			    log_ext_subtype(type, subtype));
			return buf;
		default:
			snprintf(buf, sizeof(buf), "%s 0x%llx ",
			    log_ext_subtype(type, subtype),
			    (unsigned long long)ext);
			return buf;
		}
		break;
	default:
		memcpy(&ext, data, sizeof(ext));
		snprintf(buf, sizeof(buf), "%s 0x%llx",
		    log_ext_subtype(type, subtype),
		    (unsigned long long)be64toh(ext));
		return buf;
	}
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
	time_t				 now;
	u_int16_t			 i, j;

	memset(&res, 0, sizeof(res));
	res.flags = req->flags;
	now = time(NULL);

	for (i = 0; i < mr->nentries; i++) {
		mre = &mr->entries[i];
		bzero(&ctl, sizeof(ctl));
		ctl.prefix = mr->prefix;
		ctl.prefixlen = mr->prefixlen;
		if (mre->originated <= now)
			ctl.age = now - mre->originated;
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

		output->rib(&ctl, mre->aspath, mre->aspath_len, &res);
		if (req->flags & F_CTL_DETAIL) {
			for (j = 0; j < mre->nattrs; j++)
				output->attr(mre->attrs[j].attr,
				    mre->attrs[j].attr_len, &res);
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
	time_t				 now;
	u_int16_t			 i, j;

	now = time(NULL);
	for (i = 0; i < mr->nentries; i++) {
		mre = &mr->entries[i];
		bzero(&ctl, sizeof(ctl));
		ctl.prefix = mr->prefix;
		ctl.prefixlen = mr->prefixlen;
		if (mre->originated <= now)
			ctl.age = now - mre->originated;
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
fmt_time(struct timespec *t)
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
	printf("%s %s[%u] -> ", fmt_time(&ms->time),
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

/* XXX this function does not handle JSON output */
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

		output->attr(p, attrlen, 0);
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

	printf("%s %s[%u] -> ", fmt_time(&mm->time),
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
