/*	$OpenBSD: output.c,v 1.5 2020/01/24 05:46:00 claudio Exp $ */

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

void
show_head(struct parse_result *res)
{
	switch (res->action) {
	case SHOW:
	case SHOW_SUMMARY:
		printf("%-20s %8s %10s %10s %5s %-8s %s\n", "Neighbor", "AS",
		    "MsgRcvd", "MsgSent", "OutQ", "Up/Down", "State/PrfRcvd");
		break;
	case SHOW_FIB:
		printf("flags: * = valid, B = BGP, C = Connected, "
		    "S = Static, D = Dynamic\n");
		printf("       "
		    "N = BGP Nexthop reachable via this route\n");
		printf("       r = reject route, b = blackhole route\n\n");
		printf("flags prio destination          gateway\n");
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
	case NETWORK_SHOW:
		printf("flags: S = Static\n");
		printf("flags prio destination          gateway\n");
		break;
	default:
		break;
	}
}

static void
show_summary(struct peer *p)
{
	char			*s;
	const char		*a;
	size_t			alen;

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
show_neighbor_full(struct peer *p, struct parse_result *res)
{
	struct in_addr		 ina;
	char			*s;
	int			 hascapamp = 0;
	u_int8_t		 i;

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

	ina.s_addr = p->remote_bgpid;
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

	printf("  BGP version 4, remote router-id %s",
	    inet_ntoa(ina));
	printf("%s\n", print_auth_method(p->auth.method));
	printf("  BGP state = %s", statenames[p->state]);
	if (p->conf.down) {
		printf(", marked down");
		if (*(p->conf.shutcomm)) {
			printf(" with shutdown reason \"%s\"",
			    log_shutcomm(p->conf.shutcomm));
		}
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
	for (i = 0; i < AID_MAX; i++)
		if (p->capa.peer.mp[i])
			hascapamp = 1;
	if (hascapamp || p->capa.peer.refresh ||
	    p->capa.peer.grestart.restart || p->capa.peer.as4byte) {
		printf("  Neighbor capabilities:\n");
		if (hascapamp) {
			printf("    Multiprotocol extensions: ");
			print_neighbor_capa_mp(p);
			printf("\n");
		}
		if (p->capa.peer.refresh)
			printf("    Route Refresh\n");
		if (p->capa.peer.grestart.restart) {
			printf("    Graceful Restart");
			print_neighbor_capa_restart(p);
			printf("\n");
		}
		if (p->capa.peer.as4byte)
			printf("    4-byte AS numbers\n");
	}
	printf("\n");

	if (res->action == SHOW_NEIGHBOR_TIMERS)
		return;

	print_neighbor_msgstats(p);
	printf("\n");
	if (*(p->stats.last_shutcomm)) {
		printf("  Last received shutdown reason: \"%s\"\n",
		    log_shutcomm(p->stats.last_shutcomm));
	}
	if (p->state == STATE_IDLE) {
		const char *errstr;

		errstr = get_errstr(p->stats.last_sent_errcode,
		    p->stats.last_sent_suberr);
		if (errstr)
			printf("  Last error sent: %s\n\n", errstr);
		errstr = get_errstr(p->stats.last_rcvd_errcode,
		    p->stats.last_rcvd_suberr);
		if (errstr)
			printf("  Last error received: %s\n\n", errstr);
	} else {
		printf("  Local host:  %20s, Local port:  %5u\n",
		    log_addr(&p->local), p->local_port);

		printf("  Remote host: %20s, Remote port: %5u\n",
		    log_addr(&p->remote), p->remote_port);
		printf("\n");
	}
}

void
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

void
show_timer(struct ctl_timer *t)
{
	printf("  %-20s ", timernames[t->type]);

	if (t->val <= 0)
		printf("%-20s\n", "due");
	else
		printf("due in %-13s\n", fmt_timeframe(t->val));
}

void
show_fib(struct kroute_full *kf)
{
	char			*p;

	show_fib_flags(kf->flags);

	if (asprintf(&p, "%s/%u", log_addr(&kf->prefix), kf->prefixlen) == -1)
		err(1, NULL);
	printf("%4i %-20s ", kf->priority, p);
	free(p);

	if (kf->flags & F_CONNECTED)
		printf("link#%u", kf->ifindex);
	else
		printf("%s", log_addr(&kf->nexthop));
	printf("\n");
}

void
show_fib_table(struct ktable *kt)
{
	printf("%5i %-20s %-8s%s\n", kt->rtableid, kt->descr,
	    kt->fib_sync ? "coupled" : "decoupled",
	    kt->fib_sync != kt->fib_conf ? "*" : "");
}

void
show_nexthop(struct ctl_show_nexthop *nh)
{
	struct kroute		*k;
	struct kroute6		*k6;
	char			*s;

	printf("%s %-15s ", nh->valid ? "*" : " ", log_addr(&nh->addr));
	if (!nh->krvalid) {
		printf("\n");
		return;
	}
	switch (nh->addr.aid) {
	case AID_INET:
		k = &nh->kr.kr4;
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
		k6 = &nh->kr.kr6;
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
		return;
	}
	if (nh->iface.ifname[0]) {
		printf("%s (%s, %s)", nh->iface.ifname,
		    nh->iface.is_up ? "UP" : "DOWN",
		    nh->iface.baudrate ?
		    get_baudrate(nh->iface.baudrate, "bps") :
		    nh->iface.linkstate);
	}
	printf("\n");
}

void
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
show_rib_brief(struct ctl_show_rib *r, u_char *asdata, size_t aslen)
{
	char			*aspath;

	print_prefix(&r->prefix, r->prefixlen, r->flags, r->validation_state);
	printf(" %-15s ", log_addr(&r->exit_nexthop));
	printf(" %5u %5u ", r->local_pref, r->med);

	if (aspath_asprint(&aspath, asdata, aslen) == -1)
		err(1, NULL);
	if (strlen(aspath) > 0)
		printf("%s ", aspath);
	free(aspath);

	printf("%s\n", print_origin(r->origin, 1));
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
	printf("    Nexthop %s ", log_addr(&r->exit_nexthop));
	printf("(via %s) Neighbor %s (", log_addr(&r->true_nexthop), s);
	free(s);
	id.s_addr = htonl(r->remote_id);
	printf("%s)%c", inet_ntoa(id), EOL0(flag0));

	printf("    Origin %s, metric %u, localpref %u, weight %u, ovs %s, ",
	    print_origin(r->origin, 0), r->med, r->local_pref, r->weight,
	    print_ovs(r->validation_state, 0));
	print_flags(r->flags, 0);

	printf("%c    Last update: %s ago%c", EOL0(flag0),
	    fmt_timeframe(r->age), EOL0(flag0));
}

void
show_rib(struct ctl_show_rib *r, u_char *asdata, size_t aslen,
    struct parse_result *res)
{
	if (res->flags & F_CTL_DETAIL)
		show_rib_detail(r, asdata, aslen, res->flags);
	else
		show_rib_brief(r, asdata, aslen);
}

void
show_rib_mem(struct rde_memstats *stats)
{
	static size_t  pt_sizes[AID_MAX] = AID_PTSIZE;
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

void
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

void
show_result(u_int rescode)
{
	if (rescode == 0)
		printf("request processed\n");
	else {
		if (rescode >
		    sizeof(ctl_res_strerror)/sizeof(ctl_res_strerror[0]))
			printf("unknown result error code %u\n", rescode);
		else
			printf("%s\n", ctl_res_strerror[rescode]);
	}
}
