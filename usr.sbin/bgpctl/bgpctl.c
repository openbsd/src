/*	$OpenBSD: bgpctl.c,v 1.160 2010/05/26 13:56:07 nicm Exp $ */

/*
 * Copyright (c) 2003 Henning Brauer <henning@openbsd.org>
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
#include <sys/un.h>
#include <net/if.h>
#include <net/if_media.h>
#include <net/if_types.h>

#include <err.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <util.h>

#include "bgpd.h"
#include "session.h"
#include "rde.h"
#include "log.h"
#include "parser.h"
#include "irrfilter.h"

enum neighbor_views {
	NV_DEFAULT,
	NV_TIMERS
};

int		 main(int, char *[]);
char		*fmt_peer(const char *, const struct bgpd_addr *, int, int);
void		 show_summary_head(void);
int		 show_summary_msg(struct imsg *, int);
int		 show_summary_terse_msg(struct imsg *, int);
int		 show_neighbor_terse(struct imsg *);
int		 show_neighbor_msg(struct imsg *, enum neighbor_views);
void		 print_neighbor_capa_mp(struct peer *);
void		 print_neighbor_msgstats(struct peer *);
void		 print_timer(const char *, time_t);
static char	*fmt_timeframe(time_t t);
static char	*fmt_timeframe_core(time_t t);
void		 show_fib_head(void);
void		 show_fib_tables_head(void);
void		 show_network_head(void);
void		 show_fib_flags(u_int16_t);
int		 show_fib_msg(struct imsg *);
void		 show_nexthop_head(void);
int		 show_nexthop_msg(struct imsg *);
void		 show_interface_head(void);
int		 ift2ifm(int);
const char *	 get_media_descr(int);
const char *	 get_linkstate(int, int);
const char *	 get_baudrate(u_int64_t, char *);
int		 show_interface_msg(struct imsg *);
void		 show_rib_summary_head(void);
void		 print_prefix(struct bgpd_addr *, u_int8_t, u_int8_t);
const char *	 print_origin(u_int8_t, int);
void		 print_flags(u_int8_t, int);
int		 show_rib_summary_msg(struct imsg *);
int		 show_rib_detail_msg(struct imsg *, int);
void		 show_community(u_char *, u_int16_t);
void		 show_ext_community(u_char *, u_int16_t);
char		*fmt_mem(int64_t);
int		 show_rib_memory_msg(struct imsg *);
void		 send_filterset(struct imsgbuf *, struct filter_set_head *);
static const char	*get_errstr(u_int8_t, u_int8_t);
int		 show_result(struct imsg *);

struct imsgbuf	*ibuf;

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
	int			 fd, n, done, ch, nodescr = 0, verbose = 0;
	struct imsg		 imsg;
	struct network_config	 net;
	struct parse_result	*res;
	struct ctl_neighbor	 neighbor;
	struct ctl_show_rib_request	ribreq;
	char			*sockname;
	enum imsg_type		 type;

	sockname = SOCKET_NAME;
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

	if (res->action == IRRFILTER) {
		if (!(res->flags & (F_IPV4|F_IPV6)))
			res->flags |= (F_IPV4|F_IPV6);
		irr_main(res->as.as, res->flags, res->irr_outdir);
	}

	memcpy(&neighbor.addr, &res->peeraddr, sizeof(neighbor.addr));
	strlcpy(neighbor.descr, res->peerdesc, sizeof(neighbor.descr));

	if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
		err(1, "control_init: socket");

	bzero(&sun, sizeof(sun));
	sun.sun_family = AF_UNIX;
	if (strlcpy(sun.sun_path, sockname, sizeof(sun.sun_path)) >=
	    sizeof(sun.sun_path))
		errx(1, "socket name too long");
	if (connect(fd, (struct sockaddr *)&sun, sizeof(sun)) == -1)
		err(1, "connect: %s", sockname);

	if ((ibuf = malloc(sizeof(struct imsgbuf))) == NULL)
		err(1, NULL);
	imsg_init(ibuf, fd);
	done = 0;

	switch (res->action) {
	case NONE:
	case IRRFILTER:
		usage();
		/* not reached */
	case SHOW:
	case SHOW_SUMMARY:
		imsg_compose(ibuf, IMSG_CTL_SHOW_NEIGHBOR, 0, 0, -1, NULL, 0);
		show_summary_head();
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
		show_fib_head();
		break;
	case SHOW_FIB_TABLES:
		imsg_compose(ibuf, IMSG_CTL_SHOW_FIB_TABLES, 0, 0, -1, NULL, 0);
		show_fib_tables_head();
		break;
	case SHOW_NEXTHOP:
		imsg_compose(ibuf, IMSG_CTL_SHOW_NEXTHOP, res->rtableid, 0, -1,
		    NULL, 0);
		show_nexthop_head();
		break;
	case SHOW_INTERFACE:
		imsg_compose(ibuf, IMSG_CTL_SHOW_INTERFACE, 0, 0, -1, NULL, 0);
		show_interface_head();
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
		if (res->as.type != AS_NONE) {
			memcpy(&ribreq.as, &res->as, sizeof(res->as));
			type = IMSG_CTL_SHOW_RIB_AS;
		}
		if (res->addr.aid) {
			memcpy(&ribreq.prefix, &res->addr, sizeof(res->addr));
			ribreq.prefixlen = res->prefixlen;
			type = IMSG_CTL_SHOW_RIB_PREFIX;
		}
		if (res->community.as != COMMUNITY_UNSET &&
		    res->community.type != COMMUNITY_UNSET) {
			memcpy(&ribreq.community, &res->community,
			    sizeof(res->community));
			type = IMSG_CTL_SHOW_RIB_COMMUNITY;
		}
		memcpy(&ribreq.neighbor, &neighbor,
		    sizeof(ribreq.neighbor));
		strlcpy(ribreq.rib, res->rib, sizeof(ribreq.rib));
		ribreq.aid = res->aid;
		ribreq.flags = res->flags;
		imsg_compose(ibuf, type, 0, 0, -1, &ribreq, sizeof(ribreq));
		if (!(res->flags & F_CTL_DETAIL))
			show_rib_summary_head();
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
	case NETWORK_ADD:
	case NETWORK_REMOVE:
		bzero(&net, sizeof(net));
		memcpy(&net.prefix, &res->addr, sizeof(res->addr));
		net.prefixlen = res->prefixlen;
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
		show_network_head();
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
		if (msgbuf_write(&ibuf->w) < 0)
			err(1, "write error");

	while (!done) {
		if ((n = imsg_read(ibuf)) == -1)
			err(1, "imsg_read error");
		if (n == 0)
			errx(1, "pipe closed");

		while (!done) {
			if ((n = imsg_get(ibuf, &imsg)) == -1)
				err(1, "imsg_get error");
			if (n == 0)
				break;

			if (imsg.hdr.type == IMSG_CTL_RESULT) {
				done = show_result(&imsg);
				imsg_free(&imsg);
				continue;
			}

			switch (res->action) {
			case SHOW:
			case SHOW_SUMMARY:
				done = show_summary_msg(&imsg, nodescr);
				break;
			case SHOW_SUMMARY_TERSE:
				done = show_summary_terse_msg(&imsg, nodescr);
				break;
			case SHOW_FIB:
			case SHOW_FIB_TABLES:
			case NETWORK_SHOW:
				done = show_fib_msg(&imsg);
				break;
			case SHOW_NEXTHOP:
				done = show_nexthop_msg(&imsg);
				break;
			case SHOW_INTERFACE:
				done = show_interface_msg(&imsg);
				break;
			case SHOW_NEIGHBOR:
				done = show_neighbor_msg(&imsg, NV_DEFAULT);
				break;
			case SHOW_NEIGHBOR_TIMERS:
				done = show_neighbor_msg(&imsg, NV_TIMERS);
				break;
			case SHOW_NEIGHBOR_TERSE:
				done = show_neighbor_terse(&imsg);
				break;
			case SHOW_RIB:
				if (res->flags & F_CTL_DETAIL)
					done = show_rib_detail_msg(&imsg,
					    nodescr);
				else
					done = show_rib_summary_msg(&imsg);
				break;
			case SHOW_RIB_MEM:
				done = show_rib_memory_msg(&imsg);
				break;
			case NEIGHBOR:
			case NEIGHBOR_UP:
			case NEIGHBOR_DOWN:
			case NEIGHBOR_CLEAR:
			case NEIGHBOR_RREFRESH:
			case NONE:
			case RELOAD:
			case FIB:
			case FIB_COUPLE:
			case FIB_DECOUPLE:
			case NETWORK_ADD:
			case NETWORK_REMOVE:
			case NETWORK_FLUSH:
			case IRRFILTER:
			case LOG_VERBOSE:
			case LOG_BRIEF:
				break;
			}
			imsg_free(&imsg);
		}
	}
	close(fd);
	free(ibuf);

	exit(0);
}

char *
fmt_peer(const char *descr, const struct bgpd_addr *remote_addr,
    int masklen, int nodescr)
{
	const char	*ip;
	char		*p;

	if (descr[0] && !nodescr) {
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

void
show_summary_head(void)
{
	printf("%-20s %8s %10s %10s %5s %-8s %s\n", "Neighbor", "AS",
	    "MsgRcvd", "MsgSent", "OutQ", "Up/Down", "State/PrfRcvd");
}

int
show_summary_msg(struct imsg *imsg, int nodescr)
{
	struct peer		*p;
	char			*s;

	switch (imsg->hdr.type) {
	case IMSG_CTL_SHOW_NEIGHBOR:
		p = imsg->data;
		s = fmt_peer(p->conf.descr, &p->conf.remote_addr,
		    p->conf.remote_masklen, nodescr);
		if (strlen(s) >= 20)
			s[20] = 0;
		printf("%-20s %8s %10llu %10llu %5u %-8s ",
		    s, log_as(p->conf.remote_as),
		    p->stats.msg_rcvd_open + p->stats.msg_rcvd_notification +
		    p->stats.msg_rcvd_update + p->stats.msg_rcvd_keepalive +
		    p->stats.msg_rcvd_rrefresh,
		    p->stats.msg_sent_open + p->stats.msg_sent_notification +
		    p->stats.msg_sent_update + p->stats.msg_sent_keepalive +
		    p->stats.msg_sent_rrefresh,
		    p->wbuf.queued,
		    fmt_timeframe(p->stats.last_updown));
		if (p->state == STATE_ESTABLISHED) {
			printf("%6u", p->stats.prefix_cnt);
			if (p->conf.max_prefix != 0)
				printf("/%u", p->conf.max_prefix);
		} else if (p->conf.template)
			printf("Template");
		else
			printf("%s", statenames[p->state]);
		printf("\n");
		free(s);
		break;
	case IMSG_CTL_END:
		return (1);
	default:
		break;
	}

	return (0);
}

int
show_summary_terse_msg(struct imsg *imsg, int nodescr)
{
	struct peer		*p;
	char			*s;

	switch (imsg->hdr.type) {
	case IMSG_CTL_SHOW_NEIGHBOR:
		p = imsg->data;
		s = fmt_peer(p->conf.descr, &p->conf.remote_addr,
		    p->conf.remote_masklen, nodescr);
		printf("%s %s %s\n", s, log_as(p->conf.remote_as),
		    p->conf.template ? "Template" : statenames[p->state]);
		free(s);
		break;
	case IMSG_CTL_END:
		return (1);
	default:
		break;
	}

	return (0);
}

int
show_neighbor_terse(struct imsg *imsg)
{
	struct peer		*p;

	switch (imsg->hdr.type) {
	case IMSG_CTL_SHOW_NEIGHBOR:
		p = imsg->data;
		printf("%llu %llu %llu %llu %llu %llu %llu "
		    "%llu %llu %llu %u %u %llu %llu %llu %llu\n",
		    p->stats.msg_sent_open, p->stats.msg_rcvd_open,
		    p->stats.msg_sent_notification,
		    p->stats.msg_rcvd_notification,
		    p->stats.msg_sent_update, p->stats.msg_rcvd_update,
		    p->stats.msg_sent_keepalive, p->stats.msg_rcvd_keepalive,
		    p->stats.msg_sent_rrefresh, p->stats.msg_rcvd_rrefresh,
		    p->stats.prefix_cnt, p->conf.max_prefix,
		    p->stats.prefix_sent_update, p->stats.prefix_rcvd_update,
		    p->stats.prefix_sent_withdraw,
		    p->stats.prefix_rcvd_withdraw);
		break;
	case IMSG_CTL_END:
		return (1);
	default:
		break;
	}

	return (0);
}

int
show_neighbor_msg(struct imsg *imsg, enum neighbor_views nv)
{
	struct peer		*p;
	struct ctl_timer	*t;
	struct in_addr		 ina;
	char			 buf[NI_MAXHOST], pbuf[NI_MAXSERV], *s;
	int			 hascapamp = 0;
	u_int8_t		 i;

	switch (imsg->hdr.type) {
	case IMSG_CTL_SHOW_NEIGHBOR:
		p = imsg->data;
		if ((p->conf.remote_addr.aid == AID_INET &&
		    p->conf.remote_masklen != 32) ||
		    (p->conf.remote_addr.aid == AID_INET6 &&
		    p->conf.remote_masklen != 128)) {
			if (asprintf(&s, "%s/%u",
			    log_addr(&p->conf.remote_addr),
			    p->conf.remote_masklen) == -1)
				err(1, NULL);
		} else
			if ((s = strdup(log_addr(&p->conf.remote_addr))) ==
			    NULL)
				err(1, "strdup");

		ina.s_addr = p->remote_bgpid;
		printf("BGP neighbor is %s, ", s);
		free(s);
		if (p->conf.remote_as == 0 && p->conf.template)
			printf("remote AS: accept any");
		else
			printf("remote AS %s", log_as(p->conf.remote_as));
		if (p->conf.template)
			printf(", Template");
		if (p->conf.cloned)
			printf(", Cloned");
		if (p->conf.passive)
			printf(", Passive");
		if (p->conf.ebgp && p->conf.distance > 1)
			printf(", Multihop (%u)", (int)p->conf.distance);
		printf("\n");
		if (p->conf.descr[0])
			printf(" Description: %s\n", p->conf.descr);
		printf("  BGP version 4, remote router-id %s\n",
		    inet_ntoa(ina));
		printf("  BGP state = %s", statenames[p->state]);
		if (p->stats.last_updown != 0)
			printf(", %s for %s",
			    p->state == STATE_ESTABLISHED ? "up" : "down",
			    fmt_timeframe(p->stats.last_updown));
		printf("\n");
		printf("  Last read %s, holdtime %us, keepalive interval %us\n",
		    fmt_timeframe(p->stats.last_read),
		    p->holdtime, p->holdtime/3);
		for (i = 0; i < AID_MAX; i++)
			if (p->capa.peer.mp[i])
				hascapamp = 1;
		if (hascapamp || p->capa.peer.refresh ||
		    p->capa.peer.restart || p->capa.peer.as4byte) {
			printf("  Neighbor capabilities:\n");
			if (hascapamp) {
				printf("    Multiprotocol extensions: ");
				print_neighbor_capa_mp(p);
				printf("\n");
			}
			if (p->capa.peer.refresh)
				printf("    Route Refresh\n");
			if (p->capa.peer.restart)
				printf("    Graceful Restart\n");
			if (p->capa.peer.as4byte)
				printf("    4-byte AS numbers\n");
		}
		printf("\n");
		if (nv == NV_TIMERS)
			break;
		print_neighbor_msgstats(p);
		printf("\n");
		if (p->state == STATE_IDLE) {
			static const char	*errstr;

			errstr = get_errstr(p->stats.last_sent_errcode,
			    p->stats.last_sent_suberr);
			if (errstr)
				printf("  Last error: %s\n\n", errstr);
		} else {
			if (getnameinfo((struct sockaddr *)&p->sa_local,
			    (socklen_t)p->sa_local.ss_len,
			    buf, sizeof(buf), pbuf, sizeof(pbuf),
			    NI_NUMERICHOST | NI_NUMERICSERV)) {
				strlcpy(buf, "(unknown)", sizeof(buf));
				strlcpy(pbuf, "", sizeof(pbuf));
			}
			printf("  Local host:  %20s, Local port:  %5s\n", buf,
			    pbuf);

			if (getnameinfo((struct sockaddr *)&p->sa_remote,
			    (socklen_t)p->sa_remote.ss_len,
			    buf, sizeof(buf), pbuf, sizeof(pbuf),
			    NI_NUMERICHOST | NI_NUMERICSERV)) {
				strlcpy(buf, "(unknown)", sizeof(buf));
				strlcpy(pbuf, "", sizeof(pbuf));
			}
			printf("  Remote host: %20s, Remote port: %5s\n", buf,
			    pbuf);
			printf("\n");
		}
		break;
	case IMSG_CTL_SHOW_TIMER:
		t = imsg->data;
		if (t->type > 0 && t->type < Timer_Max)
			print_timer(timernames[t->type], t->val);
		break;
	case IMSG_CTL_END:
		return (1);
		break;
	default:
		break;
	}

	return (0);
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
}

void
print_timer(const char *name, timer_t d)
{
	printf("  %-20s ", name);

	if (d <= 0)
		printf("%-20s\n", "due");
	else
		printf("due in %-13s\n", fmt_timeframe_core(d));
}

#define TF_BUFS	8
#define TF_LEN	9

static char *
fmt_timeframe(time_t t)
{
	if (t == 0)
		return ("Never");
	else
		return (fmt_timeframe_core(time(NULL) - t));
}

static char *
fmt_timeframe_core(time_t t)
{
	char		*buf;
	static char	 tfbuf[TF_BUFS][TF_LEN];	/* ring buffer */
	static int	 idx = 0;
	unsigned int	 sec, min, hrs, day, week;

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
		snprintf(buf, TF_LEN, "%02uw%01ud%02uh", week, day, hrs);
	else if (day > 0)
		snprintf(buf, TF_LEN, "%01ud%02uh%02um", day, hrs, min);
	else
		snprintf(buf, TF_LEN, "%02u:%02u:%02u", hrs, min, sec);

	return (buf);
}

void
show_fib_head(void)
{
	printf("flags: * = valid, B = BGP, C = Connected, S = Static\n");
	printf("       N = BGP Nexthop reachable via this route\n");
	printf("       r = reject route, b = blackhole route\n\n");
	printf("flags prio destination          gateway\n");
}

void
show_fib_tables_head(void)
{
	printf("%-5s %-20s %-8s\n", "Table", "Description", "State");
}

void
show_network_head(void)
{
	printf("flags: S = Static\n");
	printf("flags destination\n");
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

	printf("  ");
}

int
show_fib_msg(struct imsg *imsg)
{
	struct kroute_full	*kf;
	struct ktable		*kt;
	char			*p;

	switch (imsg->hdr.type) {
	case IMSG_CTL_KROUTE:
	case IMSG_CTL_SHOW_NETWORK:
		if (imsg->hdr.len < IMSG_HEADER_SIZE + sizeof(*kf))
			errx(1, "wrong imsg len");
		kf = imsg->data;

		show_fib_flags(kf->flags);

		if (asprintf(&p, "%s/%u", log_addr(&kf->prefix),
		    kf->prefixlen) == -1)
			err(1, NULL);
		printf("%4i %-20s ", kf->priority, p);
		free(p);

		if (kf->flags & F_CONNECTED)
			printf("link#%u", kf->ifindex);
		else
			printf("%s", log_addr(&kf->nexthop));
		printf("\n");

		break;
	case IMSG_CTL_SHOW_FIB_TABLES:
		if (imsg->hdr.len < IMSG_HEADER_SIZE + sizeof(*kt))
			errx(1, "wrong imsg len");
		kt = imsg->data;

		printf("%5i %-20s %-8s%s\n", kt->rtableid, kt->descr,
		    kt->fib_sync ? "coupled" : "decoupled",
		    kt->fib_sync != kt->fib_conf ? "*" : "");

		break;
	case IMSG_CTL_END:
		return (1);
	default:
		break;
	}

	return (0);
}

void
show_nexthop_head(void)
{
	printf("Flags: * = nexthop valid\n");
	printf("\n  %-15s %-19s%-4s %-15s %-20s\n", "Nexthop", "Route",
	     "Prio", "Gateway", "Iface");
}

int
show_nexthop_msg(struct imsg *imsg)
{
	struct ctl_show_nexthop	*p;
	struct kroute		*k;
	struct kroute6		*k6;
	char			*s;

	switch (imsg->hdr.type) {
	case IMSG_CTL_SHOW_NEXTHOP:
		p = imsg->data;
		printf("%s %-15s ", p->valid ? "*" : " ", log_addr(&p->addr));
		if (!p->krvalid) {
			printf("\n");
			return (0);
		}
		switch (p->addr.aid) {
		case AID_INET:
			k = &p->kr.kr4;
			if (asprintf(&s, "%s/%u", inet_ntoa(k->prefix),
			    k->prefixlen) == -1)
				err(1, NULL);
			printf("%-20s", s);
			free(s);
			printf("%3i %-15s ", k->priority,
			    k->flags & F_CONNECTED ? "connected" :
			    inet_ntoa(k->nexthop));
			break;
		case AID_INET6:
			k6 = &p->kr.kr6;
			if (asprintf(&s, "%s/%u", log_in6addr(&k6->prefix),
			    k6->prefixlen) == -1)
				err(1, NULL);
			printf("%-20s", s);
			free(s);
			printf("%3i %-15s ", k6->priority,
			    k6->flags & F_CONNECTED ? "connected" :
			    log_in6addr(&k6->nexthop));
			break;
		default:
			printf("unknown address family\n");
			return (0);
		}
		if (p->kif.ifname[0]) {
			char *s1;
			if (p->kif.baudrate) {
				if (asprintf(&s1, ", %s",
				    get_baudrate(p->kif.baudrate,
				    "bps")) == -1)
					err(1, NULL);
			} else if (asprintf(&s1, ", %s", get_linkstate(
			    p->kif.media_type, p->kif.link_state)) == -1)
					err(1, NULL);
			if (asprintf(&s, "%s (%s%s)", p->kif.ifname, 
			    p->kif.flags & IFF_UP ? "UP" : "DOWN", s1) == -1)
				err(1, NULL);
			printf("%-15s", s);
			free(s1);
			free(s);
		}
		printf("\n");
		break;
	case IMSG_CTL_END:
		return (1);
		break;
	default:
		break;
	}

	return (0);
}


void
show_interface_head(void)
{
	printf("%-15s%-15s%-15s%s\n", "Interface", "Nexthop state", "Flags",
	    "Link state");
}

const struct if_status_description
		if_status_descriptions[] = LINK_STATE_DESCRIPTIONS;
const struct ifmedia_description
		ifm_type_descriptions[] = IFM_TYPE_DESCRIPTIONS;

int
ift2ifm(int media_type)
{
	switch (media_type) {
	case IFT_ETHER:
		return (IFM_ETHER);
	case IFT_FDDI:
		return (IFM_FDDI);
	case IFT_CARP:
		return (IFM_CARP);
	case IFT_IEEE80211:
		return (IFM_IEEE80211);
	default:
		return (0);
	}
}

const char *
get_media_descr(int media_type)
{
	const struct ifmedia_description	*p;

	for (p = ifm_type_descriptions; p->ifmt_string != NULL; p++)
		if (media_type == p->ifmt_word)
			return (p->ifmt_string);

	return ("unknown media");
}

const char *
get_linkstate(int media_type, int link_state)
{
	const struct if_status_description *p;
	static char buf[8];

	for (p = if_status_descriptions; p->ifs_string != NULL; p++) {
		if (LINK_STATE_DESC_MATCH(p, media_type, link_state))
			return (p->ifs_string);
	}
	snprintf(buf, sizeof(buf), "[#%d]", link_state);
	return (buf);
}

const char *
get_baudrate(u_int64_t baudrate, char *unit)
{
	static char bbuf[16];

	if (baudrate > IF_Gbps(1))
		snprintf(bbuf, sizeof(bbuf), "%llu G%s",
		    baudrate / IF_Gbps(1), unit);
	else if (baudrate > IF_Mbps(1))
		snprintf(bbuf, sizeof(bbuf), "%llu M%s",
		    baudrate / IF_Mbps(1), unit);
	else if (baudrate > IF_Kbps(1))
		snprintf(bbuf, sizeof(bbuf), "%llu K%s",
		    baudrate / IF_Kbps(1), unit);
	else
		snprintf(bbuf, sizeof(bbuf), "%llu %s",
		    baudrate, unit);

	return (bbuf);
}

int
show_interface_msg(struct imsg *imsg)
{
	struct kif	*k;
	int		 ifms_type;

	switch (imsg->hdr.type) {
	case IMSG_CTL_SHOW_INTERFACE:
		k = imsg->data;
		printf("%-15s", k->ifname);
		printf("%-15s", k->nh_reachable ? "ok" : "invalid");
		printf("%-15s", k->flags & IFF_UP ? "UP" : "");

		if ((ifms_type = ift2ifm(k->media_type)) != 0)
			printf("%s, ", get_media_descr(ifms_type));

		printf("%s", get_linkstate(k->media_type, k->link_state));

		if (k->link_state != LINK_STATE_DOWN && k->baudrate > 0)
			printf(", %s", get_baudrate(k->baudrate, "Bit/s"));
		printf("\n");
		break;
	case IMSG_CTL_END:
		return (1);
		break;
	default:
		break;
	}

	return (0);
}

void
show_rib_summary_head(void)
{
	printf(
	    "flags: * = Valid, > = Selected, I = via IBGP, A = Announced\n");
	printf("origin: i = IGP, e = EGP, ? = Incomplete\n\n");
	printf("%-5s %-20s %-15s  %5s %5s %s\n", "flags", "destination",
	    "gateway", "lpref", "med", "aspath origin");
}

void
print_prefix(struct bgpd_addr *prefix, u_int8_t prefixlen, u_int8_t flags)
{
	char			*p;

	print_flags(flags, 1);
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
		if (flags & F_PREF_ANNOUNCE)
			*p++ = 'A';
		if (flags & F_PREF_INTERNAL)
			*p++ = 'I';
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
		if (flags & F_PREF_ELIGIBLE)
			printf(", valid");
		if (flags & F_PREF_ACTIVE)
			printf(", best");
		if (flags & F_PREF_ANNOUNCE)
			printf(", announced");
	}
}

int
show_rib_summary_msg(struct imsg *imsg)
{
	struct ctl_show_rib	 rib;
	char			*aspath;
	u_char			*asdata;

	switch (imsg->hdr.type) {
	case IMSG_CTL_SHOW_RIB:
		memcpy(&rib, imsg->data, sizeof(rib));

		print_prefix(&rib.prefix, rib.prefixlen, rib.flags);
		printf(" %-15s ", log_addr(&rib.exit_nexthop));

		printf(" %5u %5u ", rib.local_pref, rib.med);

		asdata = imsg->data;
		asdata += sizeof(struct ctl_show_rib);
		if (aspath_asprint(&aspath, asdata, rib.aspath_len) == -1)
			err(1, NULL);
		if (strlen(aspath) > 0)
			printf("%s ", aspath);
		free(aspath);

		printf("%s\n", print_origin(rib.origin, 1));
		break;
	case IMSG_CTL_END:
		return (1);
	default:
		break;
	}

	return (0);
}

int
show_rib_detail_msg(struct imsg *imsg, int nodescr)
{
	struct ctl_show_rib	 rib;
	struct in_addr		 id;
	char			*aspath, *s;
	u_char			*data;
	u_int32_t		 as;
	u_int16_t		 ilen, alen, ioff;
	u_int8_t		 flags, type;
	time_t			 now;

	switch (imsg->hdr.type) {
	case IMSG_CTL_SHOW_RIB:
		memcpy(&rib, imsg->data, sizeof(rib));

		printf("\nBGP routing table entry for %s/%u\n",
		    log_addr(&rib.prefix), rib.prefixlen);

		data = imsg->data;
		data += sizeof(struct ctl_show_rib);
		if (aspath_asprint(&aspath, data, rib.aspath_len) == -1)
			err(1, NULL);
		if (strlen(aspath) > 0)
			printf("    %s\n", aspath);
		free(aspath);

		s = fmt_peer(rib.descr, &rib.remote_addr, -1, nodescr);
		printf("    Nexthop %s ", log_addr(&rib.exit_nexthop));
		printf("(via %s) from %s (", log_addr(&rib.true_nexthop), s);
		free(s);
		id.s_addr = htonl(rib.remote_id);
		printf("%s)\n", inet_ntoa(id));

		printf("    Origin %s, metric %u, localpref %u, ",
		    print_origin(rib.origin, 0), rib.med, rib.local_pref);
		print_flags(rib.flags, 0);

		now = time(NULL);
		if (now > rib.lastchange)
			now -= rib.lastchange;
		else
			now = 0;

		printf("\n    Last update: %s ago\n",
		    fmt_timeframe_core(now));
		break;
	case IMSG_CTL_SHOW_RIB_ATTR:
		ilen = imsg->hdr.len - IMSG_HEADER_SIZE;
		if (ilen < 3)
			errx(1, "bad IMSG_CTL_SHOW_RIB_ATTR received");
		data = imsg->data;
		flags = data[0];
		type = data[1];

		/* get the attribute length */
		if (flags & ATTR_EXTLEN) {
			if (ilen < 4)
				errx(1, "bad IMSG_CTL_SHOW_RIB_ATTR received");
			memcpy(&alen, data+2, sizeof(u_int16_t));
			alen = ntohs(alen);
			data += 4;
			ilen -= 4;
		} else {
			alen = data[2];
			data += 3;
			ilen -= 3;
		}
		/* bad imsg len how can that happen!? */
		if (alen != ilen)
			errx(1, "bad IMSG_CTL_SHOW_RIB_ATTR received");

		switch (type) {
		case ATTR_COMMUNITIES:
			printf("    Communities: ");
			show_community(data, alen);
			printf("\n");
			break;
		case ATTR_AGGREGATOR:
			memcpy(&as, data, sizeof(as));
			memcpy(&id, data + sizeof(as), sizeof(id));
			printf("    Aggregator: %s [%s]\n", 
			    log_as(ntohl(as)), inet_ntoa(id));
			break;
		case ATTR_ORIGINATOR_ID:
			memcpy(&id, data, sizeof(id));
			printf("    Originator Id: %s\n", inet_ntoa(id));
			break;
		case ATTR_CLUSTER_LIST:
			printf("    Cluster ID List:");
			for (ioff = 0; ioff + sizeof(id) <= ilen;
			    ioff += sizeof(id)) {
				memcpy(&id, data + ioff, sizeof(id));
				printf(" %s", inet_ntoa(id));
			}
			printf("\n");
			break;
		case ATTR_EXT_COMMUNITIES:
			printf("    Ext. communities: ");
			show_ext_community(data, alen);
			printf("\n");
			break;
		default:
			/* ignore unknown attributes */
			break;
		}
		break;
	case IMSG_CTL_END:
		printf("\n");
		return (1);
	default:
		break;
	}

	return (0);
}

char *
fmt_mem(int64_t num)
{
	static char	buf[16];

	if (fmt_scaled(num, buf) == -1)
		snprintf(buf, sizeof(buf), "%lldB", (long long)num);

	return (buf);
}

size_t  pt_sizes[AID_MAX] = AID_PTSIZE;

int
show_rib_memory_msg(struct imsg *imsg)
{
	struct rde_memstats	stats;
	size_t			pts = 0;
	int			i;

	switch (imsg->hdr.type) {
	case IMSG_CTL_SHOW_RIB_MEM:
		memcpy(&stats, imsg->data, sizeof(stats));
		printf("RDE memory statistics\n");
		for (i = 0; i < AID_MAX; i++) {
			if (stats.pt_cnt[i] == 0)
				continue;
			pts += stats.pt_cnt[i] * pt_sizes[i];
			printf("%10lld %s network entries using %s of memory\n",
			    (long long)stats.pt_cnt[i], aid_vals[i].name,
			    fmt_mem(stats.pt_cnt[i] * pt_sizes[i]));
		}
		printf("%10lld rib entries using %s of memory\n",
		    (long long)stats.rib_cnt, fmt_mem(stats.rib_cnt *
		    sizeof(struct rib_entry)));
		printf("%10lld prefix entries using %s of memory\n",
		    (long long)stats.prefix_cnt, fmt_mem(stats.prefix_cnt *
		    sizeof(struct prefix)));
		printf("%10lld BGP path attribute entries using %s of memory\n",
		    (long long)stats.path_cnt, fmt_mem(stats.path_cnt *
		    sizeof(struct rde_aspath)));
		printf("%10lld BGP AS-PATH attribute entries using "
		    "%s of memory,\n\t   and holding %lld references\n",
		    (long long)stats.aspath_cnt, fmt_mem(stats.aspath_size),
		    (long long)stats.aspath_refs);
		printf("%10lld BGP attributes entries using %s of memory\n",
		    (long long)stats.attr_cnt, fmt_mem(stats.attr_cnt *
		    sizeof(struct attr)));
		printf("\t   and holding %lld references\n",
		    (long long)stats.attr_refs);
		printf("%10lld BGP attributes using %s of memory\n",
		    (long long)stats.attr_dcnt, fmt_mem(stats.attr_data));
		printf("RIB using %s of memory\n", fmt_mem(pts +
		    stats.prefix_cnt * sizeof(struct prefix) +
		    stats.rib_cnt * sizeof(struct rib_entry) +
		    stats.path_cnt * sizeof(struct rde_aspath) +
		    stats.aspath_size + stats.attr_cnt * sizeof(struct attr) +
		    stats.attr_data));
		break;
	default:
		break;
	}

	return (1);
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
		if (a == COMMUNITY_WELLKNOWN)
			switch (v) {
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
			default:
				printf("WELLKNOWN:%hu", v);
				break;
			}
		else
			printf("%hu:%hu", a, v);

		if (i + 4 < len)
			printf(" ");
	}
}

void
show_ext_community(u_char *data, u_int16_t len)
{
	u_int64_t	ext;
	struct in_addr	ip;
	u_int32_t	as4, u32;
	u_int16_t	i, as2, u16;
	u_int8_t	type, subtype;

	if (len & 0x7)
		return;

	for (i = 0; i < len; i += 8) {
		type = data[i];
		subtype = data[i + 1];

		switch (type & EXT_COMMUNITY_VALUE) {
		case EXT_COMMUNITY_TWO_AS:
			memcpy(&as2, data + i + 2, sizeof(as2));
			memcpy(&u32, data + i + 4, sizeof(u32));
			printf("%s %s:%u", log_ext_subtype(subtype),
			    log_as(ntohs(as2)), ntohl(u32));
			break;
		case EXT_COMMUNITY_IPV4:
			memcpy(&ip, data + i + 2, sizeof(ip));
			memcpy(&u16, data + i + 6, sizeof(u16));
			printf("%s %s:%hu", log_ext_subtype(subtype),
			    inet_ntoa(ip), ntohs(u16));
			break;
		case EXT_COMMUNITY_FOUR_AS:
			memcpy(&as4, data + i + 2, sizeof(as4));
			memcpy(&u16, data + i + 6, sizeof(u16));
			printf("%s %s:%hu", log_ext_subtype(subtype),
			    log_as(ntohl(as4)), ntohs(u16));
			break;
		case EXT_COMMUNITY_OPAQUE:
			memcpy(&ext, data + i, sizeof(ext));
			ext = betoh64(ext) & 0xffffffffffffLL;
			printf("%s 0x%llx", log_ext_subtype(subtype), ext); 
			break;
		default:
			memcpy(&ext, data + i, sizeof(ext));
			printf("0x%llx", betoh64(ext)); 
		}
		if (i + 8 < len)
			printf(", ");
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

static const char *
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

int
show_result(struct imsg *imsg)
{
	u_int	rescode;

	if (imsg->hdr.len != IMSG_HEADER_SIZE + sizeof(rescode))
		errx(1, "got IMSG_CTL_RESULT with wrong len");
	memcpy(&rescode, imsg->data, sizeof(rescode));

	if (rescode == 0)
		printf("request processed\n");
	else {
		if (rescode >
		    sizeof(ctl_res_strerror)/sizeof(ctl_res_strerror[0]))
			errx(1, "illegal error code %u", rescode);
		printf("%s\n", ctl_res_strerror[rescode]);
	}

	return (1);
}

/* following functions are necessary for imsg framework */
void
log_warnx(const char *emsg, ...)
{
	va_list	 ap;

	va_start(ap, emsg);
	vwarnx(emsg, ap);
	va_end(ap);
}

void
log_warn(const char *emsg, ...)
{
	va_list	 ap;

	va_start(ap, emsg);
	vwarn(emsg, ap);
	va_end(ap);
}

void
fatal(const char *emsg)
{
	err(1, emsg);
}
