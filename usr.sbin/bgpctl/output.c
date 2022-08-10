/*	$OpenBSD: output.c,v 1.26 2022/08/10 10:21:47 claudio Exp $ */

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

#include <endian.h>
#include <err.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bgpd.h"
#include "session.h"
#include "rde.h"

#include "bgpctl.h"
#include "parser.h"

const size_t  pt_sizes[AID_MAX] = AID_PTSIZE;

static void
show_head(struct parse_result *res)
{
	switch (res->action) {
	case SHOW:
	case SHOW_SUMMARY:
		printf("%-20s %8s %10s %10s %5s %-8s %s\n", "Neighbor", "AS",
		    "MsgRcvd", "MsgSent", "OutQ", "Up/Down", "State/PrfRcvd");
		break;
	case SHOW_FIB:
		printf("flags: B = BGP, C = Connected, S = Static\n");
		printf("       N = BGP Nexthop reachable via this route\n");
		printf("       r = reject route, b = blackhole route\n\n");
		printf("%-5s %-4s %-32s %-32s\n", "flags", "prio",
		    "destination", "gateway");
		break;
	case SHOW_FIB_TABLES:
		printf("%-5s %-20s %-8s\n", "Table", "Description", "State");
		break;
	case SHOW_NEXTHOP:
		printf("Flags: * = nexthop valid\n");
		printf("\n  %-15s %-19s%-4s %-15s %-20s\n", "Nexthop", "Route",
		     "Prio", "Gateway", "Iface");
		break;
	case SHOW_INTERFACE:
		printf("%-15s%-9s%-9s%-7s%s\n", "Interface", "rdomain",
		    "Nexthop", "Flags", "Link state");
		break;
	case SHOW_RIB:
		if (res->flags & F_CTL_DETAIL)
			break;
		printf("flags: "
		    "* = Valid, > = Selected, I = via IBGP, A = Announced,\n"
		    "       S = Stale, E = Error\n");
		printf("origin validation state: "
		    "N = not-found, V = valid, ! = invalid\n");
		printf("origin: i = IGP, e = EGP, ? = Incomplete\n\n");
		printf("%-5s %3s %-20s %-15s  %5s %5s %s\n",
		    "flags", "ovs", "destination", "gateway", "lpref", "med",
		    "aspath origin");
		break;
	case SHOW_SET:
		printf("%-6s %-34s %7s %7s %6s %11s\n", "Type", "Name",
		    "#IPv4", "#IPv6", "#ASnum", "Last Change");
		break;
	case NETWORK_SHOW:
		printf("flags: S = Static\n");
		printf("%-5s %-4s %-32s %-32s\n", "flags", "prio",
		    "destination", "gateway");
		break;
	default:
		break;
	}
}

static void
show_summary(struct peer *p)
{
	char		*s;
	const char	*a;
	size_t		alen;

	s = fmt_peer(p->conf.descr, &p->conf.remote_addr,
	    p->conf.remote_masklen);

	a = log_as(p->conf.remote_as);
	alen = strlen(a);
	/* max displayed length of the peers name is 28 */
	if (alen < 28) {
		if (strlen(s) > 28 - alen)
			s[28 - alen] = '\0';
	} else
		alen = 0;

	printf("%-*s %s %10llu %10llu %5u %-8s ",
	    (28 - (int)alen), s, a,
	    p->stats.msg_rcvd_open + p->stats.msg_rcvd_notification +
	    p->stats.msg_rcvd_update + p->stats.msg_rcvd_keepalive +
	    p->stats.msg_rcvd_rrefresh,
	    p->stats.msg_sent_open + p->stats.msg_sent_notification +
	    p->stats.msg_sent_update + p->stats.msg_sent_keepalive +
	    p->stats.msg_sent_rrefresh,
	    p->wbuf.queued,
	    fmt_monotime(p->stats.last_updown));
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
}

static void
show_neighbor_capa_mp(struct capabilities *capa)
{
	int	comma;
	uint8_t	i;

	printf("    Multiprotocol extensions: ");
	for (i = AID_MIN, comma = 0; i < AID_MAX; i++)
		if (capa->mp[i]) {
			printf("%s%s", comma ? ", " : "", aid2str(i));
			comma = 1;
		}
	printf("\n");
}

static void
show_neighbor_capa_add_path(struct capabilities *capa)
{
	const char	*mode;
	int		comma;
	uint8_t		i;

	printf("    Add-path: ");
	for (i = AID_MIN, comma = 0; i < AID_MAX; i++) {
		switch (capa->add_path[i]) {
		case 0:
		default:
			continue;
		case CAPA_AP_RECV:
			mode = "recv";
			break;
		case CAPA_AP_SEND:
			mode = "send";
			break;
		case CAPA_AP_BIDIR:
			mode = "bidir";
		}
		printf("%s%s %s", comma ? ", " : "", aid2str(i), mode);
		comma = 1;
	}
	printf("\n");
}

static void
show_neighbor_capa_restart(struct capabilities *capa)
{
	int	comma;
	uint8_t	i;

	printf("    Graceful Restart");
	if (capa->grestart.timeout)
		printf(": Timeout: %d, ", capa->grestart.timeout);
	for (i = AID_MIN, comma = 0; i < AID_MAX; i++)
		if (capa->grestart.flags[i] & CAPA_GR_PRESENT) {
			if (!comma &&
			    capa->grestart.flags[i] & CAPA_GR_RESTART)
				printf("restarted, ");
			if (comma)
				printf(", ");
			printf("%s", aid2str(i));
			if (capa->grestart.flags[i] & CAPA_GR_FORWARD)
				printf(" (preserved)");
			comma = 1;
		}
	printf("\n");
}

static void
show_neighbor_msgstats(struct peer *p)
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
	printf("  %-15s %10u %10u\n", "Prefixes",
	    p->stats.prefix_out_cnt, p->stats.prefix_cnt);
	printf("  %-15s %10llu %10llu\n", "Updates",
	    p->stats.prefix_sent_update, p->stats.prefix_rcvd_update);
	printf("  %-15s %10llu %10llu\n", "Withdraws",
	    p->stats.prefix_sent_withdraw, p->stats.prefix_rcvd_withdraw);
	printf("  %-15s %10llu %10llu\n", "End-of-Rib",
	    p->stats.prefix_sent_eor, p->stats.prefix_rcvd_eor);
	printf("  Route Refresh statistics:\n");
	printf("  %-15s %10llu %10llu\n", "Request",
	    p->stats.refresh_sent_req, p->stats.refresh_rcvd_req);
	printf("  %-15s %10llu %10llu\n", "Begin-of-RR",
	    p->stats.refresh_sent_borr, p->stats.refresh_rcvd_borr);
	printf("  %-15s %10llu %10llu\n", "End-of-RR",
	    p->stats.refresh_sent_eorr, p->stats.refresh_rcvd_eorr);
}

static void
show_neighbor_full(struct peer *p, struct parse_result *res)
{
	const char	*errstr;
	struct in_addr	 ina;
	char		*s;
	int		 hascapamp, hascapaap;
	uint8_t		 i;

	if ((p->conf.remote_addr.aid == AID_INET &&
	    p->conf.remote_masklen != 32) ||
	    (p->conf.remote_addr.aid == AID_INET6 &&
	    p->conf.remote_masklen != 128)) {
		if (asprintf(&s, "%s/%u",
		    log_addr(&p->conf.remote_addr),
		    p->conf.remote_masklen) == -1)
			err(1, NULL);
	} else if ((s = strdup(log_addr(&p->conf.remote_addr))) == NULL)
			err(1, "strdup");

	printf("BGP neighbor is %s, ", s);
	free(s);
	if (p->conf.remote_as == 0 && p->conf.template)
		printf("remote AS: accept any");
	else
		printf("remote AS %s", log_as(p->conf.remote_as));
	if (p->conf.template)
		printf(", Template");
	if (p->template)
		printf(", Cloned");
	if (p->conf.passive)
		printf(", Passive");
	if (p->conf.ebgp && p->conf.distance > 1)
		printf(", Multihop (%u)", (int)p->conf.distance);
	printf("\n");
	if (p->conf.descr[0])
		printf(" Description: %s\n", p->conf.descr);
	if (p->conf.max_prefix) {
		printf(" Max-prefix: %u", p->conf.max_prefix);
		if (p->conf.max_prefix_restart)
			printf(" (restart %u)",
			    p->conf.max_prefix_restart);
	}
	if (p->conf.max_out_prefix) {
		printf(" Max-prefix out: %u", p->conf.max_out_prefix);
		if (p->conf.max_out_prefix_restart)
			printf(" (restart %u)",
			    p->conf.max_out_prefix_restart);
	}
	if (p->conf.max_prefix || p->conf.max_out_prefix)
		printf("\n");

	if (p->state == STATE_ESTABLISHED) {
		ina.s_addr = p->remote_bgpid;
		printf("  BGP version 4, remote router-id %s",
		    inet_ntoa(ina));
		printf("%s\n", fmt_auth_method(p->auth.method));
	}
	printf("  BGP state = %s", statenames[p->state]);
	if (p->conf.down) {
		printf(", marked down");
	}
	if (p->conf.reason[0]) {
		printf(" with shutdown reason \"%s\"",
		    log_reason(p->conf.reason));
	}
	if (p->stats.last_updown != 0)
		printf(", %s for %s",
		    p->state == STATE_ESTABLISHED ? "up" : "down",
		    fmt_monotime(p->stats.last_updown));
	printf("\n");
	printf("  Last read %s, holdtime %us, keepalive interval %us\n",
	    fmt_monotime(p->stats.last_read),
	    p->holdtime, p->holdtime/3);
	printf("  Last write %s\n", fmt_monotime(p->stats.last_write));

	hascapamp = 0;
	hascapaap = 0;
	for (i = AID_MIN; i < AID_MAX; i++) {
		if (p->capa.peer.mp[i])
			hascapamp = 1;
		if (p->capa.peer.add_path[i])
			hascapaap = 1;
	}
	if (hascapamp || hascapaap || p->capa.peer.grestart.restart ||
	    p->capa.peer.refresh || p->capa.peer.enhanced_rr ||
	    p->capa.peer.as4byte || p->capa.peer.role_ena) {
		printf("  Neighbor capabilities:\n");
		if (hascapamp)
			show_neighbor_capa_mp(&p->capa.peer);
		if (p->capa.peer.as4byte)
			printf("    4-byte AS numbers\n");
		if (p->capa.peer.refresh)
			printf("    Route Refresh\n");
		if (p->capa.peer.enhanced_rr)
			printf("    Enhanced Route Refresh\n");
		if (p->capa.peer.grestart.restart)
			show_neighbor_capa_restart(&p->capa.peer);
		if (hascapaap)
			show_neighbor_capa_add_path(&p->capa.peer);
		if (p->capa.peer.role_ena)
			printf("    Open Policy role %s (local %s)\n",
			    log_policy(p->capa.peer.role),
			    log_policy(p->capa.ann.role));
	}

	hascapamp = 0;
	hascapaap = 0;
	for (i = AID_MIN; i < AID_MAX; i++) {
		if (p->capa.neg.mp[i])
			hascapamp = 1;
		if (p->capa.neg.add_path[i])
			hascapaap = 1;
	}
	if (hascapamp || hascapaap || p->capa.neg.grestart.restart ||
	    p->capa.neg.refresh || p->capa.neg.enhanced_rr ||
	    p->capa.neg.as4byte || p->capa.neg.role_ena) {
		printf("  Negotiated capabilities:\n");
		if (hascapamp)
			show_neighbor_capa_mp(&p->capa.neg);
		if (p->capa.neg.as4byte)
			printf("    4-byte AS numbers\n");
		if (p->capa.neg.refresh)
			printf("    Route Refresh\n");
		if (p->capa.neg.enhanced_rr)
			printf("    Enhanced Route Refresh\n");
		if (p->capa.neg.grestart.restart)
			show_neighbor_capa_restart(&p->capa.neg);
		if (hascapaap)
			show_neighbor_capa_add_path(&p->capa.neg);
		if (p->capa.neg.role_ena)
			printf("    Open Policy role %s (local %s)\n",
			    log_policy(p->capa.neg.role),
			    log_policy(p->capa.ann.role));
	}
	printf("\n");

	if (res->action == SHOW_NEIGHBOR_TIMERS)
		return;

	show_neighbor_msgstats(p);
	printf("\n");
	if (p->stats.last_reason[0]) {
		printf("  Last received shutdown reason: \"%s\"\n",
		    log_reason(p->stats.last_reason));
	}

	errstr = fmt_errstr(p->stats.last_sent_errcode,
	    p->stats.last_sent_suberr);
	if (errstr)
		printf("  Last error sent: %s\n", errstr);
	errstr = fmt_errstr(p->stats.last_rcvd_errcode,
	    p->stats.last_rcvd_suberr);
	if (errstr)
		printf("  Last error received: %s\n", errstr);

	if (p->state >= STATE_OPENSENT) {
		printf("  Local host:  %20s, Local port:  %5u\n",
		    log_addr(&p->local), p->local_port);

		printf("  Remote host: %20s, Remote port: %5u\n",
		    log_addr(&p->remote), p->remote_port);
		printf("\n");
	}
}

static void
show_neighbor(struct peer *p, struct parse_result *res)
{
	char *s;

	switch (res->action) {
	case SHOW:
	case SHOW_SUMMARY:
		show_summary(p);
		break;
	case SHOW_SUMMARY_TERSE:
		s = fmt_peer(p->conf.descr, &p->conf.remote_addr,
		    p->conf.remote_masklen);
		printf("%s %s %s\n", s, log_as(p->conf.remote_as),
		    p->conf.template ? "Template" : statenames[p->state]);
		free(s);
		break;
	case SHOW_NEIGHBOR:
	case SHOW_NEIGHBOR_TIMERS:
		show_neighbor_full(p, res);
		break;
	case SHOW_NEIGHBOR_TERSE:
		s = fmt_peer(NULL, &p->conf.remote_addr,
		    p->conf.remote_masklen);
		printf("%llu %llu %llu %llu %llu %llu %llu %llu %llu "
		    "%llu %u %u %llu %llu %llu %llu %s %s \"%s\"\n",
		    p->stats.msg_sent_open, p->stats.msg_rcvd_open,
		    p->stats.msg_sent_notification,
		    p->stats.msg_rcvd_notification,
		    p->stats.msg_sent_update, p->stats.msg_rcvd_update,
		    p->stats.msg_sent_keepalive, p->stats.msg_rcvd_keepalive,
		    p->stats.msg_sent_rrefresh, p->stats.msg_rcvd_rrefresh,
		    p->stats.prefix_cnt, p->conf.max_prefix,
		    p->stats.prefix_sent_update, p->stats.prefix_rcvd_update,
		    p->stats.prefix_sent_withdraw,
		    p->stats.prefix_rcvd_withdraw, s,
		    log_as(p->conf.remote_as), p->conf.descr);
		free(s);
		break;
	default:
		break;
	}
}

static void
show_timer(struct ctl_timer *t)
{
	printf("  %-20s ", timernames[t->type]);

	if (t->val <= 0)
		printf("%-20s\n", "due");
	else
		printf("due in %-13s\n", fmt_timeframe(t->val));
}

static void
show_fib(struct kroute_full *kf)
{
	char	*p;

	if (asprintf(&p, "%s/%u", log_addr(&kf->prefix), kf->prefixlen) == -1)
		err(1, NULL);
	printf("%-5s %4i %-32s ", fmt_fib_flags(kf->flags), kf->priority, p);
	free(p);

	if (kf->flags & F_CONNECTED)
		printf("link#%u", kf->ifindex);
	else
		printf("%s", log_addr(&kf->nexthop));
	printf("\n");
}

static void
show_fib_table(struct ktable *kt)
{
	printf("%5i %-20s %-8s%s\n", kt->rtableid, kt->descr,
	    kt->fib_sync ? "coupled" : "decoupled",
	    kt->fib_sync != kt->fib_conf ? "*" : "");
}

static void
show_nexthop(struct ctl_show_nexthop *nh)
{
	char		*s;

	printf("%s %-15s ", nh->valid ? "*" : " ", log_addr(&nh->addr));
	if (!nh->krvalid) {
		printf("\n");
		return;
	}
	if (asprintf(&s, "%s/%u", log_addr(&nh->kr.prefix),
	    nh->kr.prefixlen) == -1)
		err(1, NULL);
	printf("%-20s", s);
	free(s);
	printf("%3i %-15s ", nh->kr.priority,
	    nh->kr.flags & F_CONNECTED ? "connected" :
	    log_addr(&nh->kr.nexthop));

	if (nh->iface.ifname[0]) {
		printf("%s (%s, %s)", nh->iface.ifname,
		    nh->iface.is_up ? "UP" : "DOWN",
		    nh->iface.baudrate ?
		    get_baudrate(nh->iface.baudrate, "bps") :
		    nh->iface.linkstate);
	}
	printf("\n");
}

static void
show_interface(struct ctl_show_interface *iface)
{
	printf("%-15s", iface->ifname);
	printf("%-9u", iface->rdomain);
	printf("%-9s", iface->nh_reachable ? "ok" : "invalid");
	printf("%-7s", iface->is_up ? "UP" : "");

	if (iface->media[0])
		printf("%s, ", iface->media);
	printf("%s", iface->linkstate);

	if (iface->baudrate > 0)
		printf(", %s", get_baudrate(iface->baudrate, "Bit/s"));
	printf("\n");
}

static void
show_communities(u_char *data, size_t len, struct parse_result *res)
{
	struct community c;
	size_t	i;
	uint64_t ext;
	uint8_t type = 0;

	if (len % sizeof(c))
		return;

	for (i = 0; i < len; i += sizeof(c)) {
		memcpy(&c, data + i, sizeof(c));

		if (type != c.flags) {
			if (type != 0)
				printf("%c", EOL0(res->flags));
			printf("    %s:", fmt_attr(c.flags,
			    ATTR_OPTIONAL | ATTR_TRANSITIVE));
			type = c.flags;
		}

		switch (c.flags) {
		case COMMUNITY_TYPE_BASIC:
			printf(" %s", fmt_community(c.data1, c.data2));
			break;
		case COMMUNITY_TYPE_LARGE:
			printf(" %s",
			    fmt_large_community(c.data1, c.data2, c.data3));
			break;
		case COMMUNITY_TYPE_EXT:
			ext = (uint64_t)c.data3 << 48;
			switch (c.data3 >> 8) {
			case EXT_COMMUNITY_TRANS_TWO_AS:
			case EXT_COMMUNITY_TRANS_OPAQUE:
			case EXT_COMMUNITY_TRANS_EVPN:
			case EXT_COMMUNITY_NON_TRANS_OPAQUE:
				ext |= ((uint64_t)c.data1 & 0xffff) << 32;
				ext |= (uint64_t)c.data2;
				break;
			case EXT_COMMUNITY_TRANS_FOUR_AS:
			case EXT_COMMUNITY_TRANS_IPV4:
				ext |= (uint64_t)c.data1 << 16;
				ext |= (uint64_t)c.data2 & 0xffff;
				break;
			}
			ext = htobe64(ext);

			printf(" %s", fmt_ext_community((void *)&ext));
			break;
		}
	}

	printf("%c", EOL0(res->flags));
}

static void
show_community(u_char *data, uint16_t len)
{
	uint16_t	a, v;
	uint16_t	i;

	if (len & 0x3) {
		printf("bad length");
		return;
	}

	for (i = 0; i < len; i += 4) {
		memcpy(&a, data + i, sizeof(a));
		memcpy(&v, data + i + 2, sizeof(v));
		a = ntohs(a);
		v = ntohs(v);
		printf("%s", fmt_community(a, v));

		if (i + 4 < len)
			printf(" ");
	}
}

static void
show_large_community(u_char *data, uint16_t len)
{
	uint32_t	a, l1, l2;
	uint16_t	i;

	if (len % 12) {
		printf("bad length");
		return;
	}

	for (i = 0; i < len; i += 12) {
		memcpy(&a, data + i, sizeof(a));
		memcpy(&l1, data + i + 4, sizeof(l1));
		memcpy(&l2, data + i + 8, sizeof(l2));
		a = ntohl(a);
		l1 = ntohl(l1);
		l2 = ntohl(l2);
		printf("%s", fmt_large_community(a, l1, l2));

		if (i + 12 < len)
			printf(" ");
	}
}

static void
show_ext_community(u_char *data, uint16_t len)
{
	uint16_t	i;

	if (len & 0x7) {
		printf("bad length");
		return;
	}

	for (i = 0; i < len; i += 8) {
		printf("%s", fmt_ext_community(data + i));

		if (i + 8 < len)
			printf(" ");
	}
}

static void
show_attr(u_char *data, size_t len, int reqflags, int addpath)
{
	u_char		*path;
	struct in_addr	 id;
	struct bgpd_addr prefix;
	char		*aspath;
	uint32_t	 as, pathid;
	uint16_t	 alen, ioff, short_as, afi;
	uint8_t		 flags, type, safi, aid, prefixlen;
	int		 i, pos, e2, e4;

	if (len < 3) {
		warnx("Too short BGP attrbute");
		return;
	}

	flags = data[0];
	type = data[1];

	/* get the attribute length */
	if (flags & ATTR_EXTLEN) {
		if (len < 4) {
			warnx("Too short BGP attrbute");
			return;
		}
		memcpy(&alen, data+2, sizeof(uint16_t));
		alen = ntohs(alen);
		data += 4;
		len -= 4;
	} else {
		alen = data[2];
		data += 3;
		len -= 3;
	}

	/* bad imsg len how can that happen!? */
	if (alen > len) {
		warnx("Bad BGP attrbute length");
		return;
	}

	printf("    %s: ", fmt_attr(type, flags));

	switch (type) {
	case ATTR_ORIGIN:
		if (alen == 1)
			printf("%s", fmt_origin(*data, 0));
		else
			printf("bad length");
		break;
	case ATTR_ASPATH:
	case ATTR_AS4_PATH:
		/* prefer 4-byte AS here */
		e4 = aspath_verify(data, alen, 1, 0);
		e2 = aspath_verify(data, alen, 0, 0);
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
			uint32_t val;
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
		if (alen == 4) {
			memcpy(&id, data, sizeof(id));
			printf("%s", inet_ntoa(id));
		} else
			printf("bad length");
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
			uint8_t nhlen;
			if (len == 0)
				goto bad_len;
			nhlen = *data++;
			alen--;
			if (nhlen > len)
				goto bad_len;
			memset(&nexthop, 0, sizeof(nexthop));
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
				memcpy(&nexthop.v4, data + sizeof(uint64_t),
				    sizeof(nexthop.v4));
				break;
			case AID_VPN_IPv6:
				if (nhlen != 24)
					goto bad_len;
				nexthop.aid = AID_INET6;
				memcpy(&nexthop.v6, data + sizeof(uint64_t),
				    sizeof(nexthop.v6));
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
			if (addpath) {
				if (alen <= sizeof(pathid)) {
					printf("bad nlri prefix");
					return;
				}
				memcpy(&pathid, data, sizeof(pathid));
				pathid = ntohl(pathid);
				data += sizeof(pathid);
				alen -= sizeof(pathid);
			}
			switch (aid) {
			case AID_INET6:
				pos = nlri_get_prefix6(data, alen, &prefix,
				    &prefixlen);
				break;
			case AID_VPN_IPv4:
				pos = nlri_get_vpn4(data, alen, &prefix,
				    &prefixlen, 1);
				break;
			case AID_VPN_IPv6:
				pos = nlri_get_vpn6(data, alen, &prefix,
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
			if (addpath)
				printf(" path-id %u", pathid);
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
	case ATTR_OTC:
		if (alen == 4) {
			memcpy(&as, data, sizeof(as));
			as = ntohl(as);
			printf("%s", log_as(as));
		} else {
			printf("bad length");
		}
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
	printf("%c", EOL0(reqflags));
}

static void
show_rib_brief(struct ctl_show_rib *r, u_char *asdata, size_t aslen)
{
	char *p, *aspath;

	if (asprintf(&p, "%s/%u", log_addr(&r->prefix), r->prefixlen) == -1)
		err(1, NULL);
	printf("%s %3s %-20s %-15s %5u %5u ",
	    fmt_flags(r->flags, 1), fmt_ovs(r->validation_state, 1), p,
	    log_addr(&r->exit_nexthop), r->local_pref, r->med);
	free(p);

	if (aspath_asprint(&aspath, asdata, aslen) == -1)
		err(1, NULL);
	if (strlen(aspath) > 0)
		printf("%s ", aspath);
	free(aspath);

	printf("%s\n", fmt_origin(r->origin, 1));
}

static void
show_rib_detail(struct ctl_show_rib *r, u_char *asdata, size_t aslen,
    int flag0)
{
	struct in_addr		 id;
	char			*aspath, *s;

	printf("\nBGP routing table entry for %s/%u%c",
	    log_addr(&r->prefix), r->prefixlen,
	    EOL0(flag0));

	if (aspath_asprint(&aspath, asdata, aslen) == -1)
		err(1, NULL);
	if (strlen(aspath) > 0)
		printf("    %s%c", aspath, EOL0(flag0));
	free(aspath);

	s = fmt_peer(r->descr, &r->remote_addr, -1);
	id.s_addr = htonl(r->remote_id);
	printf("    Nexthop %s ", log_addr(&r->exit_nexthop));
	printf("(via %s) Neighbor %s (%s)", log_addr(&r->true_nexthop), s,
	    inet_ntoa(id));
	if (r->flags & F_PREF_PATH_ID)
		printf(" Path-Id: %u", r->path_id);
	printf("%c", EOL0(flag0));
	free(s);

	printf("    Origin %s, metric %u, localpref %u, weight %u, ovs %s, ",
	    fmt_origin(r->origin, 0), r->med, r->local_pref, r->weight,
	    fmt_ovs(r->validation_state, 0));
	printf("%s", fmt_flags(r->flags, 0));

	printf("%c    Last update: %s ago%c", EOL0(flag0),
	    fmt_timeframe(r->age), EOL0(flag0));
}

static void
show_rib(struct ctl_show_rib *r, u_char *asdata, size_t aslen,
    struct parse_result *res)
{
	if (res->flags & F_CTL_DETAIL)
		show_rib_detail(r, asdata, aslen, res->flags);
	else
		show_rib_brief(r, asdata, aslen);
}

static void
show_rib_mem(struct rde_memstats *stats)
{
	size_t			pts = 0;
	int			i;

	printf("RDE memory statistics\n");
	for (i = 0; i < AID_MAX; i++) {
		if (stats->pt_cnt[i] == 0)
			continue;
		pts += stats->pt_cnt[i] * pt_sizes[i];
		printf("%10lld %s network entries using %s of memory\n",
		    stats->pt_cnt[i], aid_vals[i].name,
		    fmt_mem(stats->pt_cnt[i] * pt_sizes[i]));
	}
	printf("%10lld rib entries using %s of memory\n",
	    stats->rib_cnt, fmt_mem(stats->rib_cnt *
	    sizeof(struct rib_entry)));
	printf("%10lld prefix entries using %s of memory\n",
	    stats->prefix_cnt, fmt_mem(stats->prefix_cnt *
	    sizeof(struct prefix)));
	printf("%10lld BGP path attribute entries using %s of memory\n",
	    stats->path_cnt, fmt_mem(stats->path_cnt *
	    sizeof(struct rde_aspath)));
	printf("\t   and holding %lld references\n",
	    stats->path_refs);
	printf("%10lld BGP AS-PATH attribute entries using "
	    "%s of memory\n\t   and holding %lld references\n",
	    stats->aspath_cnt, fmt_mem(stats->aspath_size),
	    stats->aspath_refs);
	printf("%10lld entries for %lld BGP communities "
	    "using %s of memory\n", stats->comm_cnt, stats->comm_nmemb,
	    fmt_mem(stats->comm_cnt * sizeof(struct rde_community) +
	    stats->comm_size * sizeof(struct community)));
	printf("\t   and holding %lld references\n",
	    stats->comm_refs);
	printf("%10lld BGP attributes entries using %s of memory\n",
	    stats->attr_cnt, fmt_mem(stats->attr_cnt *
	    sizeof(struct attr)));
	printf("\t   and holding %lld references\n",
	    stats->attr_refs);
	printf("%10lld BGP attributes using %s of memory\n",
	    stats->attr_dcnt, fmt_mem(stats->attr_data));
	printf("%10lld as-set elements in %lld tables using "
	    "%s of memory\n", stats->aset_nmemb, stats->aset_cnt,
	    fmt_mem(stats->aset_size));
	printf("%10lld prefix-set elements using %s of memory\n",
	    stats->pset_cnt, fmt_mem(stats->pset_size));
	printf("RIB using %s of memory\n", fmt_mem(pts +
	    stats->prefix_cnt * sizeof(struct prefix) +
	    stats->rib_cnt * sizeof(struct rib_entry) +
	    stats->path_cnt * sizeof(struct rde_aspath) +
	    stats->aspath_size + stats->attr_cnt * sizeof(struct attr) +
	    stats->attr_data));
	printf("Sets using %s of memory\n", fmt_mem(stats->aset_size +
	    stats->pset_size));
	printf("\nRDE hash statistics\n");
}

static void
show_rib_hash(struct rde_hashstats *hash)
{
	double avg, dev;

	printf("\t%s: size %lld, %lld entries\n", hash->name, hash->num,
	    hash->sum);
	avg = (double)hash->sum / (double)hash->num;
	dev = sqrt(fmax(0, hash->sumq / hash->num - avg * avg));
	printf("\t    min %lld max %lld avg/std-dev = %.3f/%.3f\n",
	    hash->min, hash->max, avg, dev);
}

static void
show_rib_set(struct ctl_show_set *set)
{
	char buf[64];

	if (set->type == ASNUM_SET)
		snprintf(buf, sizeof(buf), "%7s %7s %6zu",
		    "-", "-", set->as_cnt);
	else
		snprintf(buf, sizeof(buf), "%7zu %7zu %6s",
		    set->v4_cnt, set->v6_cnt, "-");

	printf("%-6s %-34s %s %11s\n", fmt_set_type(set), set->name,
	    buf, fmt_monotime(set->lastchange));
}

static void
show_rtr(struct ctl_show_rtr *rtr)
{
	static int not_first;

	if (not_first)
		printf("\n");
	not_first = 1;

	printf("RTR neighbor is %s, port %u\n",
	    log_addr(&rtr->remote_addr), rtr->remote_port);
	if (rtr->descr[0])
		printf(" Description: %s\n", rtr->descr);
	if (rtr->local_addr.aid != AID_UNSPEC)
		printf(" Local Address: %s\n", log_addr(&rtr->local_addr));
	if (rtr->session_id != -1)
		printf (" Session ID: %d Serial #: %u\n",
		    rtr->session_id, rtr->serial);
	printf(" Refresh: %u, Retry: %u, Expire: %u\n",
	    rtr->refresh, rtr->retry, rtr->expire);

	if (rtr->last_sent_error != NO_ERROR) {
		printf(" Last sent error: %s\n",
		  log_rtr_error(rtr->last_sent_error));
		if (rtr->last_sent_msg[0])
			printf(" with reason \"%s\"",
			    log_reason(rtr->last_sent_msg));
	}
	if (rtr->last_recv_error != NO_ERROR) {
		printf("Last received error: %s\n",
		  log_rtr_error(rtr->last_recv_error));
		if (rtr->last_recv_msg[0])
			printf(" with reason \"%s\"",
			    log_reason(rtr->last_recv_msg));
	}

	printf("\n");
}

static void
show_result(u_int rescode)
{
	if (rescode == 0)
		printf("request processed\n");
	else if (rescode >=
	    sizeof(ctl_res_strerror)/sizeof(ctl_res_strerror[0]))
		printf("unknown result error code %u\n", rescode);
	else
		printf("%s\n", ctl_res_strerror[rescode]);
}

static void
show_tail(void)
{
	/* nothing */
}

const struct output show_output = {
	.head = show_head,
	.neighbor = show_neighbor,
	.timer = show_timer,
	.fib = show_fib,
	.fib_table = show_fib_table,
	.nexthop = show_nexthop,
	.interface = show_interface,
	.communities = show_communities,
	.attr = show_attr,
	.rib = show_rib,
	.rib_mem = show_rib_mem,
	.rib_hash = show_rib_hash,
	.set = show_rib_set,
	.rtr = show_rtr,
	.result = show_result,
	.tail = show_tail
};
