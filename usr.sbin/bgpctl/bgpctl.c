/*	$OpenBSD: bgpctl.c,v 1.73 2005/02/02 18:52:32 henning Exp $ */

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

#include "bgpd.h"
#include "session.h"
#include "rde.h"
#include "log.h"
#include "parser.h"

enum neighbor_views {
	NV_DEFAULT,
	NV_TIMERS
};

__dead void	 usage(void);
int		 main(int, char *[]);
char		*fmt_peer(const struct peer_config *, int);
void		 show_summary_head(void);
int		 show_summary_msg(struct imsg *, int);
int		 show_neighbor_msg(struct imsg *, enum neighbor_views);
void		 print_neighbor_capa_mp_safi(u_int8_t);
void		 print_neighbor_msgstats(struct peer *);
void		 print_neighbor_timers(struct peer *);
void		 print_timer(const char *, time_t, u_int);
static char	*fmt_timeframe(time_t t);
static char	*fmt_timeframe_core(time_t t);
void		 show_fib_head(void);
void		 show_network_head(void);
int		 show_fib_msg(struct imsg *);
void		 show_nexthop_head(void);
int		 show_nexthop_msg(struct imsg *);
void		 show_interface_head(void);
const char *	 get_media_descr(int);
const char *	 get_linkstate(int, int);
void		 print_baudrate(u_long);
int		 show_interface_msg(struct imsg *);
void		 show_rib_summary_head(void);
void		 print_prefix(struct bgpd_addr *, u_int8_t, u_int8_t);
const char *	 print_origin(u_int8_t, int);
int		 show_rib_summary_msg(struct imsg *);

struct imsgbuf	*ibuf;

__dead void
usage(void)
{
	extern char	*__progname;

	fprintf(stderr, "usage: %s [-n] <command> [arg [...]]\n", __progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	struct sockaddr_un	 sun;
	int			 fd, n, done, ch, nodescr = 0;
	struct imsg		 imsg;
	struct network_config	 net;
	struct parse_result	*res;
	struct ctl_neighbor	 neighbor;

	while ((ch = getopt(argc, argv, "n")) != -1) {
		switch (ch) {
		case 'n':
			if (++nodescr > 1)
				usage();
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

	memcpy(&neighbor.addr, &res->addr, sizeof(neighbor.addr));
	strlcpy(neighbor.descr, res->peerdesc, sizeof(neighbor.descr));

	if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
		err(1, "control_init: socket");

	bzero(&sun, sizeof(sun));
	sun.sun_family = AF_UNIX;
	strlcpy(sun.sun_path, SOCKET_NAME, sizeof(sun.sun_path));
	if (connect(fd, (struct sockaddr *)&sun, sizeof(sun)) == -1)
		err(1, "connect: %s", SOCKET_NAME);

	if ((ibuf = malloc(sizeof(struct imsgbuf))) == NULL)
		fatal(NULL);
	imsg_init(ibuf, fd);
	done = 0;

	switch (res->action) {
	case NONE:
		usage();
		/* not reached */
	case SHOW:
	case SHOW_SUMMARY:
		imsg_compose(ibuf, IMSG_CTL_SHOW_NEIGHBOR, 0, 0, -1, NULL, 0);
		show_summary_head();
		break;
	case SHOW_FIB:
		if (!res->addr.af)
			imsg_compose(ibuf, IMSG_CTL_KROUTE, 0, 0, -1,
			    &res->flags, sizeof(res->flags));
		else
			imsg_compose(ibuf, IMSG_CTL_KROUTE_ADDR, 0, 0, -1,
			    &res->addr, sizeof(res->addr));
		show_fib_head();
		break;
	case SHOW_NEXTHOP:
		imsg_compose(ibuf, IMSG_CTL_SHOW_NEXTHOP, 0, 0, -1, NULL, 0);
		show_nexthop_head();
		break;
	case SHOW_INTERFACE:
		imsg_compose(ibuf, IMSG_CTL_SHOW_INTERFACE, 0, 0, -1, NULL, 0);
		show_interface_head();
		break;
	case SHOW_NEIGHBOR:
	case SHOW_NEIGHBOR_TIMERS:
		if (res->addr.af || res->peerdesc[0])
			imsg_compose(ibuf, IMSG_CTL_SHOW_NEIGHBOR, 0, 0, -1,
			    &neighbor, sizeof(neighbor));
		else
			imsg_compose(ibuf, IMSG_CTL_SHOW_NEIGHBOR, 0, 0, -1,
			    NULL, 0);
		break;
	case SHOW_RIB:
		if (res->as.type != AS_NONE)
			imsg_compose(ibuf, IMSG_CTL_SHOW_RIB_AS, 0, 0, -1,
			    &res->as, sizeof(res->as));
		else if (res->addr.af) {
			struct ctl_show_rib_prefix	msg;

			bzero(&msg, sizeof(msg));
			memcpy(&msg.prefix, &res->addr, sizeof(res->addr));
			msg.prefixlen = res->prefixlen;
			msg.flags = res->flags;
			imsg_compose(ibuf, IMSG_CTL_SHOW_RIB_PREFIX, 0, 0, -1,
			    &msg, sizeof(msg));
		} else
			imsg_compose(ibuf, IMSG_CTL_SHOW_RIB, 0, 0, -1,
			    NULL, 0);
		show_rib_summary_head();
		break;
	case RELOAD:
		imsg_compose(ibuf, IMSG_CTL_RELOAD, 0, 0, -1, NULL, 0);
		printf("reload request sent.\n");
		done = 1;
		break;
	case FIB:
		errx(1, "action==FIB");
		break;
	case FIB_COUPLE:
		imsg_compose(ibuf, IMSG_CTL_FIB_COUPLE, 0, 0, -1, NULL, 0);
		printf("couple request sent.\n");
		done = 1;
		break;
	case FIB_DECOUPLE:
		imsg_compose(ibuf, IMSG_CTL_FIB_DECOUPLE, 0, 0, -1, NULL, 0);
		printf("decouple request sent.\n");
		done = 1;
		break;
	case NEIGHBOR:
		errx(1, "action==NEIGHBOR");
		break;
	case NEIGHBOR_UP:
		imsg_compose(ibuf, IMSG_CTL_NEIGHBOR_UP, 0, 0, -1,
		    &neighbor, sizeof(neighbor));
		printf("request sent.\n");
		done = 1;
		break;
	case NEIGHBOR_DOWN:
		imsg_compose(ibuf, IMSG_CTL_NEIGHBOR_DOWN, 0, 0, -1,
		    &neighbor, sizeof(neighbor));
		printf("request sent.\n");
		done = 1;
		break;
	case NEIGHBOR_CLEAR:
		imsg_compose(ibuf, IMSG_CTL_NEIGHBOR_CLEAR, 0, 0, -1,
		    &neighbor, sizeof(neighbor));
		printf("request sent.\n");
		done = 1;
		break;
	case NETWORK_ADD:
	case NETWORK_REMOVE:
		bzero(&net, sizeof(net));
		memcpy(&net.prefix, &res->addr, sizeof(res->addr));
		net.prefixlen = res->prefixlen;
		/* attribute sets are not supported */
		if (res->action == NETWORK_ADD)
			imsg_compose(ibuf, IMSG_NETWORK_ADD, 0, 0, -1,
			    &net, sizeof(net));
		else
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
		imsg_compose(ibuf, IMSG_CTL_SHOW_NETWORK, 0, 0, -1, NULL, 0);
		show_network_head();
		break;
	}

	while (ibuf->w.queued)
		if (msgbuf_write(&ibuf->w) < 0)
			err(1, "write error");

	while (!done) {
		if ((n = imsg_read(ibuf)) == -1)
			errx(1, "imsg_read error");
		if (n == 0)
			errx(1, "pipe closed");

		while (!done) {
			if ((n = imsg_get(ibuf, &imsg)) == -1)
				errx(1, "imsg_get error");
			if (n == 0)
				break;
			switch (res->action) {
			case SHOW:
			case SHOW_SUMMARY:
				done = show_summary_msg(&imsg, nodescr);
				break;
			case SHOW_FIB:
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
			case SHOW_RIB:
				done = show_rib_summary_msg(&imsg);
				break;
			case NETWORK_SHOW:
				done = show_fib_msg(&imsg);
				break;
			case NONE:
			case RELOAD:
			case FIB:
			case FIB_COUPLE:
			case FIB_DECOUPLE:
			case NEIGHBOR:
			case NEIGHBOR_UP:
			case NEIGHBOR_DOWN:
			case NEIGHBOR_CLEAR:
			case NETWORK_ADD:
			case NETWORK_REMOVE:
			case NETWORK_FLUSH:
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
fmt_peer(const struct peer_config *peer, int nodescr)
{
	const char	*ip;
	char		*p;

	if (peer->descr[0] && !nodescr) {
		if ((p = strdup(peer->descr)) == NULL)
			fatal(NULL);
		return (p);
	}

	ip = log_addr(&peer->remote_addr);
	if ((peer->remote_addr.af == AF_INET && peer->remote_masklen != 32) ||
	    (peer->remote_addr.af == AF_INET6 && peer->remote_masklen != 128)) {
		if (asprintf(&p, "%s/%u", ip, peer->remote_masklen) == -1)
			fatal(NULL);
	} else {
		if ((p = strdup(ip)) == NULL)
			fatal(NULL);
	}

	return (p);
}

void
show_summary_head(void)
{
	printf("%-20s %-5s %-10s %-10s %-5s %-8s %s\n", "Neighbor", "AS",
	    "MsgRcvd", "MsgSent", "OutQ", "Up/Down", "State/PrefixRcvd");
}

int
show_summary_msg(struct imsg *imsg, int nodescr)
{
	struct peer		*p;
	char			*s;

	switch (imsg->hdr.type) {
	case IMSG_CTL_SHOW_NEIGHBOR:
		p = imsg->data;
		s = fmt_peer(&p->conf, nodescr);
		if (strlen(s) >= 20)
			s[20] = 0;
		printf("%-20s %5u %10llu %10llu %5u %-8s ",
		    s, p->conf.remote_as,
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
		} else
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
show_neighbor_msg(struct imsg *imsg, enum neighbor_views nv)
{
	struct peer		*p;
	struct in_addr		 ina;
	char			 buf[NI_MAXHOST], pbuf[NI_MAXSERV], *s;

	switch (imsg->hdr.type) {
	case IMSG_CTL_SHOW_NEIGHBOR:
		p = imsg->data;
		if ((p->conf.remote_addr.af == AF_INET &&
		    p->conf.remote_masklen != 32) ||
		    (p->conf.remote_addr.af == AF_INET6 &&
		    p->conf.remote_masklen != 128)) {
			if (asprintf(&s, "%s/%u",
			    log_addr(&p->conf.remote_addr),
			    p->conf.remote_masklen) == -1)
				err(1, NULL);
		} else
			s = strdup(log_addr(&p->conf.remote_addr));

		ina.s_addr = p->remote_bgpid;
		printf("BGP neighbor is %s, ", s);
		free(s);
		if (p->conf.remote_as == 0 && p->conf.template)
			printf("remote AS: accept any");
		else
			printf("remote AS %u", p->conf.remote_as);
		if (p->conf.template)
			printf(", Template");
		if (p->conf.cloned)
			printf(", Cloned");
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
		if (p->capa.peer.mp_v4 || p->capa.peer.mp_v6 ||
		    p->capa.peer.refresh) {
			printf("  Neighbor capabilities:\n");
			if (p->capa.peer.mp_v4) {
				printf("    Multiprotocol extensions: IPv4");
				print_neighbor_capa_mp_safi(p->capa.peer.mp_v4);
			}
			if (p->capa.peer.mp_v6) {
				printf("    Multiprotocol extensions: IPv6");
				print_neighbor_capa_mp_safi(p->capa.peer.mp_v6);
			}
			if (p->capa.peer.refresh)
				printf("    Route Refresh\n");
		}
		printf("\n");
		switch (nv) {
		case NV_DEFAULT:
			print_neighbor_msgstats(p);
			break;
		case NV_TIMERS:
			print_neighbor_timers(p);
			break;
		}
		printf("\n");
		if (getnameinfo((struct sockaddr *)&p->sa_local,
		    (socklen_t)p->sa_local.ss_len,
		    buf, sizeof(buf), pbuf, sizeof(pbuf),
		    NI_NUMERICHOST | NI_NUMERICSERV)) {
			strlcpy(buf, "(unknown)", sizeof(buf));
			strlcpy(pbuf, "", sizeof(pbuf));
		}
		printf("  Local host:  %20s, Local port:  %5s\n", buf, pbuf);

		if (getnameinfo((struct sockaddr *)&p->sa_remote,
		    (socklen_t)p->sa_remote.ss_len,
		    buf, sizeof(buf), pbuf, sizeof(pbuf),
		    NI_NUMERICHOST | NI_NUMERICSERV)) {
			strlcpy(buf, "(unknown)", sizeof(buf));
			strlcpy(pbuf, "", sizeof(pbuf));
		}
		printf("  Remote host: %20s, Remote port: %5s\n", buf, pbuf);
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
print_neighbor_capa_mp_safi(u_int8_t safi)
{
	switch (safi) {
	case SAFI_UNICAST:
		printf(" Unicast");
		break;
	case SAFI_MULTICAST:
		printf(" Multicast");
		break;
	case SAFI_BOTH:
		printf(" Unicast and Multicast");
		break;
	default:
		printf(" unknown (%u)", safi);
		break;
	}
	printf("\n");
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
	printf("  %-15s %10llu %10llu\n", "Total",
	    p->stats.msg_sent_open + p->stats.msg_sent_notification +
	    p->stats.msg_sent_update + p->stats.msg_sent_keepalive +
	    p->stats.msg_sent_rrefresh,
	    p->stats.msg_rcvd_open + p->stats.msg_rcvd_notification +
	    p->stats.msg_rcvd_update + p->stats.msg_rcvd_keepalive +
	    p->stats.msg_rcvd_rrefresh);
}

void
print_neighbor_timers(struct peer *p)
{
	print_timer("IdleHoldTimer:", p->IdleHoldTimer, p->IdleHoldTime);
	print_timer("ConnectRetryTimer:", p->ConnectRetryTimer,
	    INTERVAL_CONNECTRETRY);
	print_timer("HoldTimer:", p->HoldTimer, (u_int)p->holdtime);
	print_timer("KeepaliveTimer:", p->KeepaliveTimer, (u_int)p->holdtime/3);
}

void
print_timer(const char *name, time_t val, u_int interval)
{
	int	d;

	d = val - time(NULL);
	printf("  %-20s ", name);

	if (val == 0)
		printf("%-20s", "not running");
	else if (d <= 0)
		printf("%-20s", "due");
	else
		printf("due in %-13s", fmt_timeframe_core(d));

	printf("Interval: %5us\n", interval);
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
	unsigned	 sec, min, hrs, day, week;

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
	printf("       N = BGP Nexthop reachable via this route\n\n");
	printf("flags destination          gateway\n");
}

void
show_network_head(void)
{
	printf("flags: S = Static\n");
	printf("flags destination\n");
}

int
show_fib_msg(struct imsg *imsg)
{
	struct kroute		*k;
	char			*p;

	switch (imsg->hdr.type) {
	case IMSG_CTL_KROUTE:
	case IMSG_CTL_SHOW_NETWORK:
		if (imsg->hdr.len < IMSG_HEADER_SIZE + sizeof(struct kroute))
			errx(1, "wrong imsg len");
		k = imsg->data;

		if (k->flags & F_DOWN)
			printf(" ");
		else
			printf("*");

		if (k->flags & F_BGPD_INSERTED)
			printf("B");
		else if (k->flags & F_CONNECTED)
			printf("C");
		else if (k->flags & F_KERNEL)
			printf("S");
		else
			printf(" ");

		if (k->flags & F_NEXTHOP)
			printf("N");
		else
			printf(" ");

		printf("   ");
		if (asprintf(&p, "%s/%u", inet_ntoa(k->prefix), k->prefixlen) ==
		    -1)
			err(1, NULL);
		printf("%-20s ", p);
		free(p);

		if (k->nexthop.s_addr)
			printf("%s", inet_ntoa(k->nexthop));
		else if (k->flags & F_CONNECTED)
			printf("link#%u", k->ifindex);
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
show_nexthop_head(void)
{
	printf("%-20s %s\n", "Nexthop", "State");
}

int
show_nexthop_msg(struct imsg *imsg)
{
	struct ctl_show_nexthop	*p;

	switch (imsg->hdr.type) {
	case IMSG_CTL_SHOW_NEXTHOP:
		p = imsg->data;
		printf("%-20s %s\n", inet_ntoa(p->addr.v4),
		    p->valid ? "valid" : "invalid");
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

const int	ifm_status_valid_list[] = IFM_STATUS_VALID_LIST;
const struct ifmedia_status_description
		ifm_status_descriptions[] = IFM_STATUS_DESCRIPTIONS;
const struct ifmedia_description
		ifm_type_descriptions[] = IFM_TYPE_DESCRIPTIONS;

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
	const struct ifmedia_status_description	*p;
	int					 i;

	if (link_state == LINK_STATE_UNKNOWN)
		return ("unknown");

	for (i = 0; ifm_status_valid_list[i] != 0; i++)
		for (p = ifm_status_descriptions; p->ifms_valid != 0; p++) {
			if (p->ifms_type != media_type ||
			    p->ifms_valid != ifm_status_valid_list[i])
				continue;
			return (p->ifms_string[link_state == LINK_STATE_UP]);
		}

	return ("unknown link state");
}

void
print_baudrate(u_long baudrate)
{
	if (baudrate > IF_Gbps(1))
		printf("%lu GBit/s", baudrate / IF_Gbps(1));
	else if (baudrate > IF_Mbps(1))
		printf("%lu MBit/s", baudrate / IF_Mbps(1));
	else if (baudrate > IF_Kbps(1))
		printf("%lu KBit/s", baudrate / IF_Kbps(1));
	else
		printf("%lu Bit/s", baudrate);
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
		switch (k->media_type) {
		case IFT_ETHER:
			ifms_type = IFM_ETHER;
			break;
		case IFT_FDDI:
			ifms_type = IFM_FDDI;
			break;
		case IFT_ISO88025:
			ifms_type = IFM_TOKEN;
			break;
		case IFT_CARP:
			ifms_type = IFM_CARP;
			break;
		default:
			ifms_type = 0;
			break;
		}

		if (ifms_type)
			printf("%s, %s", get_media_descr(ifms_type),
			    get_linkstate(ifms_type, k->link_state));
		else if (k->link_state == LINK_STATE_UNKNOWN)
			printf("unknown");
		else
			printf("link state %u", k->link_state);

		if (k->link_state != LINK_STATE_DOWN && k->baudrate > 0) {
			printf(", ");
			print_baudrate(k->baudrate);
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
show_rib_summary_head(void)
{
	printf(
	    "flags: * = Valid, > = Selected, I = via IBGP, A = Announced\n");
	printf("origin: i = IGP, e = EGP, ? = Incomplete\n\n");
	printf("%-4s %-20s%-15s  %5s %5s %s\n", "flags", "destination",
	    "gateway", "lpref", "med", "aspath origin");
}

void
print_prefix(struct bgpd_addr *prefix, u_int8_t prefixlen, u_int8_t flags)
{
	char			 flagstr[5];
	char			*p;

	p = flagstr;
	if (flags & F_RIB_ANNOUNCE)
		*p++ = 'A';
	if (flags & F_RIB_INTERNAL)
		*p++ = 'I';
	if (flags & F_RIB_ELIGIBLE)
		*p++ = '*';
	if (flags & F_RIB_ACTIVE)
		*p++ = '>';
	*p = '\0';

	if (asprintf(&p, "%s/%u", inet_ntoa(prefix->v4), prefixlen) == -1)
		err(1, NULL);
	printf("%-4s %-20s", flagstr, p);
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

char			*aspath = NULL;
struct ctl_show_rib	*rib = NULL;

int
show_rib_summary_msg(struct imsg *imsg)
{
	struct ctl_show_rib_prefix	*p;
	u_char				*asdata;

	switch (imsg->hdr.type) {
	case IMSG_CTL_SHOW_RIB:
		if (rib != NULL) {
			free(rib);
			rib = NULL;
		}
		if (aspath != NULL) {
			free(aspath);
			aspath = NULL;
		}

		if ((rib = malloc(imsg->hdr.len - IMSG_HEADER_SIZE)) == NULL)
			err(1, NULL);
		memcpy(rib, imsg->data, imsg->hdr.len - IMSG_HEADER_SIZE);

		print_prefix(&rib->prefix, rib->prefixlen, rib->flags);
		printf("%-15s ", inet_ntoa(rib->nexthop.v4));

		printf(" %5u %5u ", rib->local_pref, rib->med);

		asdata = imsg->data;
		asdata += sizeof(struct ctl_show_rib);
		if (aspath_asprint(&aspath, asdata, rib->aspath_len) == -1)
			err(1, NULL);
		if (strlen(aspath) > 0)
			printf("%s ", aspath);

		printf("%s\n", print_origin(rib->origin, 1));
		break;
	case IMSG_CTL_SHOW_RIB_PREFIX:
		p = imsg->data;
		if (rib == NULL)
			/* unexpected packet */
			return (0);

		print_prefix(&p->prefix, p->prefixlen, p->flags);
		printf("%-15s ", inet_ntoa(rib->nexthop.v4));

		printf(" %5u %5u ", rib->local_pref, rib->med);

		if (strlen(aspath) > 0)
			printf("%s ", aspath);

		printf("%s\n", print_origin(rib->origin, 1));
		break;
	case IMSG_CTL_END:
		if (rib != NULL) {
			free(rib);
			rib = NULL;
		}
		if (aspath != NULL) {
			free(aspath);
			aspath = NULL;
		}

		return (1);
	default:
		if (rib != NULL) {
			free(rib);
			rib = NULL;
		}
		if (aspath != NULL) {
			free(aspath);
			aspath = NULL;
		}

		break;
	}

	return (0);
}

