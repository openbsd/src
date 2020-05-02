/*	$OpenBSD: output_json.c,v 1.1 2020/05/02 14:33:33 claudio Exp $ */

/*
 * Copyright (c) 2020 Claudio Jeker <claudio@openbsd.org>
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
#include "json.h"

static void
json_head(struct parse_result *res)
{
	json_do_start();
}

static void
json_neighbor_capabilities(struct capabilities *capa)
{
	int hascapamp;
	uint8_t i;

	for (i = 0; i < AID_MAX; i++)
		if (capa->mp[i])
			hascapamp = 1;
	if (!hascapamp && !capa->refresh && !capa->grestart.restart &&
	    !capa->as4byte)
		return;

	json_do_object("capabilities");
	json_do_bool("as4byte", capa->as4byte);
	json_do_bool("refresh", capa->refresh);

	if (hascapamp) {
		json_do_array("multiprotocol");
		for (i = 0; i < AID_MAX; i++)
			if (capa->mp[i])
				json_do_printf("mp", "%s", aid2str(i));
		json_do_end();
	}
	if (capa->grestart.restart) {
		int restarted = 0, present = 0;

		for (i = 0; i < AID_MAX; i++)
			if (capa->grestart.flags[i] & CAPA_GR_PRESENT) {
				present = 1;
				if (capa->grestart.flags[i] & CAPA_GR_RESTART)
					restarted = 1;
				break;
			}
		json_do_object("graceful_restart");
		json_do_bool("eor", 1);
		json_do_bool("restart", restarted);

		if (capa->grestart.timeout)
			json_do_uint("timeout", capa->grestart.timeout);

		if (present) {
			json_do_array("protocols");
			for (i = 0; i < AID_MAX; i++)
				if (capa->grestart.flags[i] & CAPA_GR_PRESENT) {
					json_do_object("family");
					json_do_printf("family", "%s",
					    aid2str(i));
					json_do_bool("preserved",
					    capa->grestart.flags[i] &
					    CAPA_GR_FORWARD);
					json_do_end();
				}
			json_do_end();
		}

		json_do_end();
	}

	json_do_end();
}

static void
json_neighbor_stats(struct peer *p)
{
	json_do_object("stats");
	json_do_printf("last_read", "%s", fmt_monotime(p->stats.last_read));
	json_do_printf("last_write", "%s", fmt_monotime(p->stats.last_write));

	json_do_object("prefixes");
	json_do_uint("sent", p->stats.prefix_out_cnt);
	json_do_uint("received", p->stats.prefix_cnt);
	json_do_end();

	json_do_object("message");

	json_do_object("sent");
	json_do_uint("open", p->stats.msg_sent_open);
	json_do_uint("notifications", p->stats.msg_sent_notification);
	json_do_uint("updates", p->stats.msg_sent_update);
	json_do_uint("keepalives", p->stats.msg_sent_keepalive);
	json_do_uint("route_refresh", p->stats.msg_sent_rrefresh);
	json_do_uint("total",
	    p->stats.msg_sent_open + p->stats.msg_sent_notification +
	    p->stats.msg_sent_update + p->stats.msg_sent_keepalive +
	    p->stats.msg_sent_rrefresh);
	json_do_end();

	json_do_object("received");
	json_do_uint("open", p->stats.msg_rcvd_open);
	json_do_uint("notifications", p->stats.msg_rcvd_notification);
	json_do_uint("updates", p->stats.msg_rcvd_update);
	json_do_uint("keepalives", p->stats.msg_rcvd_keepalive);
	json_do_uint("route_refresh", p->stats.msg_rcvd_rrefresh);
	json_do_uint("total",
	    p->stats.msg_rcvd_open + p->stats.msg_rcvd_notification +
	    p->stats.msg_rcvd_update + p->stats.msg_rcvd_keepalive +
	    p->stats.msg_rcvd_rrefresh);
	json_do_end();

	json_do_end();

	json_do_object("update");

	json_do_object("sent");
	json_do_uint("updates", p->stats.prefix_sent_update);
	json_do_uint("withdraws", p->stats.prefix_sent_withdraw);
	json_do_uint("eor", p->stats.prefix_sent_eor);
	json_do_end();

	json_do_object("received");
	json_do_uint("updates", p->stats.prefix_rcvd_update);
	json_do_uint("withdraws", p->stats.prefix_rcvd_withdraw);
	json_do_uint("eor", p->stats.prefix_rcvd_eor);
	json_do_end();

	json_do_end();

	json_do_end();
}

static void
json_neighbor_full(struct peer *p)
{
	const char *errstr;

	/* config */
	json_do_object("config");
	json_do_bool("template", p->conf.template);
	json_do_bool("cloned", p->template != NULL);
	json_do_bool("passive", p->conf.passive);
	json_do_bool("down", p->conf.down);
	json_do_bool("multihop", p->conf.ebgp && p->conf.distance > 1);
	if (p->conf.ebgp && p->conf.distance > 1)
		json_do_uint("multihop_distance", p->conf.distance);
	if (p->conf.max_prefix) {
		json_do_uint("max_prefix", p->conf.max_prefix);
		if (p->conf.max_prefix_restart)
			json_do_uint("max_prefix_restart",
			    p->conf.max_prefix_restart);
	}
	if (p->conf.max_out_prefix) {
		json_do_uint("max_prefix", p->conf.max_out_prefix);
		if (p->conf.max_out_prefix_restart)
			json_do_uint("max_out_prefix_restart",
			    p->conf.max_out_prefix_restart);
	}
	if (p->auth.method != AUTH_NONE)
		json_do_printf("authentication", "%s",
		    fmt_auth_method(p->auth.method));
	json_do_bool("ttl_security", p->conf.ttlsec);
	json_do_uint("holdtime", p->conf.holdtime);
	json_do_uint("min_holdtime", p->conf.min_holdtime);

	/* capabilities */
	json_do_bool("announce_capabilities", p->conf.announce_capa);
	json_neighbor_capabilities(&p->conf.capabilities);

	json_do_end();


	/* stats */
	json_neighbor_stats(p);

	/* errors */
	if (*(p->conf.shutcomm))
		json_do_printf("my_shutdown_reason", "%s",
		    log_shutcomm(p->conf.shutcomm));
	if (*(p->stats.last_shutcomm))
		json_do_printf("last_shutdown_reason", "%s",
		    log_shutcomm(p->stats.last_shutcomm));
	errstr = fmt_errstr(p->stats.last_sent_errcode,
	    p->stats.last_sent_suberr);
	if (errstr)
		json_do_printf("last_error_sent", "%s", errstr);
	errstr = fmt_errstr(p->stats.last_rcvd_errcode,
	    p->stats.last_rcvd_suberr);
	if (errstr)
		json_do_printf("last_error_received", "%s", errstr);

	/* connection info */
	if (p->state >= STATE_OPENSENT) {
		json_do_object("session");
		json_do_uint("holdtime", p->holdtime);
		json_do_uint("keepalive", p->holdtime / 3);

		json_do_object("local");
		json_do_printf("address", "%s", log_addr(&p->local));
		json_do_uint("port", p->local_port);
		json_neighbor_capabilities(&p->capa.ann);
		json_do_end();

		json_do_object("remote");
		json_do_printf("address", "%s", log_addr(&p->remote));
		json_do_uint("port", p->remote_port);
		json_neighbor_capabilities(&p->capa.peer);
		json_do_end();

		/* capabilities */
		json_neighbor_capabilities(&p->capa.neg);

		json_do_end();
	}
}

static void
json_neighbor(struct peer *p, struct parse_result *res)
{
	json_do_array("neighbors");

	json_do_object("neighbor");

	json_do_printf("remote_as", "%s", log_as(p->conf.remote_as));
	if (p->conf.descr[0])
		json_do_printf("description", "%s", p->conf.descr);
	if (!p->conf.template)
		json_do_printf("remote_addr", "%s",
		    log_addr(&p->conf.remote_addr));
	else
		json_do_printf("remote_addr", "%s/%u",
		    log_addr(&p->conf.remote_addr), p->conf.remote_masklen);
	if (p->state == STATE_ESTABLISHED) {
		struct in_addr ina;
		ina.s_addr = p->remote_bgpid;
		json_do_printf("bgpid", "%s", inet_ntoa(ina));
	}
	json_do_printf("state", "%s", statenames[p->state]);
	json_do_printf("last_updown", "%s", fmt_monotime(p->stats.last_updown));

	switch (res->action) {
	case SHOW:
	case SHOW_SUMMARY:
	case SHOW_SUMMARY_TERSE:
		/* only show basic data */
		break;
	case SHOW_NEIGHBOR:
	case SHOW_NEIGHBOR_TIMERS:
	case SHOW_NEIGHBOR_TERSE:
		json_neighbor_full(p);
		break;
	default:
		break;
	}

	/* keep the object open in case there are timers */
}

static void
json_timer(struct ctl_timer *t)
{
	json_do_array("timers");

	json_do_object("timer");
	json_do_printf("name", "%s", timernames[t->type]);
	json_do_int("due", t->val);
	json_do_end();
}

static void
json_fib(struct kroute_full *kf)
{
	const char *origin;

	json_do_array("fib");

	json_do_object("fib_entry");

	json_do_printf("prefix", "%s/%u", log_addr(&kf->prefix), kf->prefixlen);
	json_do_uint("priority", kf->priority);
	json_do_bool("up", !(kf->flags & F_DOWN));
	if (kf->flags & F_BGPD_INSERTED)
		origin = "bgp";
	else if (kf->flags & F_CONNECTED)
		origin = "connected";
	else if (kf->flags & F_STATIC)
		origin = "static";
	else if (kf->flags & F_DYNAMIC)
		origin = "dynamic";
	else
		origin = "unknown";
	json_do_printf("origin", "%s", origin);
	json_do_bool("used_by_nexthop", kf->flags & F_NEXTHOP);
	json_do_bool("blackhole", kf->flags & F_BLACKHOLE);
	json_do_bool("reject", kf->flags & F_REJECT);

	if (kf->flags & F_CONNECTED)
		json_do_printf("nexthop", "link#%u", kf->ifindex);
	else
		json_do_printf("nexthop", "%s", log_addr(&kf->nexthop));

	json_do_end();
}

static void
json_fib_table(struct ktable *kt)
{
	json_do_array("fibtables");

	json_do_object("fibtable");
	json_do_uint("rtableid", kt->rtableid);
	json_do_printf("description", "%s", kt->descr);
	json_do_bool("coupled", kt->fib_sync);
	json_do_bool("admin_change", kt->fib_sync != kt->fib_conf);
	json_do_end();
}

static void
json_do_interface(struct ctl_show_interface *iface)
{
	json_do_object("interface");

	json_do_printf("name", "%s", iface->ifname);
	json_do_uint("rdomain", iface->rdomain);
	json_do_bool("is_up", iface->is_up);
	json_do_bool("nh_reachable", iface->nh_reachable);

	if (iface->media[0])
		json_do_printf("media", "%s", iface->media);

	json_do_printf("linkstate", "%s", iface->linkstate);
	if (iface->baudrate > 0)
		json_do_uint("baudrate", iface->baudrate);

	json_do_end();
}

static void
json_nexthop(struct ctl_show_nexthop *nh)
{
	struct kroute *k;
	struct kroute6 *k6;

	json_do_array("nexthops");

	json_do_object("nexthop");

	json_do_printf("address", "%s", log_addr(&nh->addr));
	json_do_bool("valid", nh->valid);

	if (!nh->krvalid)
		goto done;

	switch (nh->addr.aid) {
	case AID_INET:
		k = &nh->kr.kr4;
		json_do_printf("prefix", "%s/%u", inet_ntoa(k->prefix),
		    k->prefixlen);
		json_do_uint("priority", k->priority);
		json_do_bool("connected", k->flags & F_CONNECTED);
		json_do_printf("nexthop", "%s", inet_ntoa(k->nexthop));
		break;
	case AID_INET6:
		k6 = &nh->kr.kr6;
		json_do_printf("prefix", "%s/%u", log_in6addr(&k6->prefix),
		    k6->prefixlen);
		json_do_uint("priority", k6->priority);
		json_do_bool("connected", k6->flags & F_CONNECTED);
		json_do_printf("nexthop", "%s", log_in6addr(&k6->nexthop));
		break;
	default:
		warnx("nexthop: unknown address family");
		goto done;
	}
	if (nh->iface.ifname[0])
		json_do_interface(&nh->iface);
done:
	json_do_end();
	/* keep array open */
}

static void
json_interface(struct ctl_show_interface *iface)
{
	json_do_array("interfaces");
	json_do_interface(iface);
}

static void
json_communities(u_char *data, size_t len, struct parse_result *res)
{
	struct community c;
	size_t  i;
	uint64_t ext;

	if (len % sizeof(c)) {
		warnx("communities: bad size");
		return;
	}

	for (i = 0; i < len; i += sizeof(c)) {
		memcpy(&c, data + i, sizeof(c));

		switch (c.flags) {
		case COMMUNITY_TYPE_BASIC:
			json_do_array("communities");
			json_do_printf("community", "%s",
			    fmt_community(c.data1, c.data2));
			break;
		case COMMUNITY_TYPE_LARGE:
			json_do_array("large_communities");
			json_do_printf("community", "%s",
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

			json_do_array("extended_communities");
			json_do_printf("community", "%s",
			    fmt_ext_community((void *)&ext));
			break;
		}
	}
}

static void
json_do_community(u_char *data, uint16_t len)
{
	uint16_t a, v, i;

	if (len & 0x3) {
		json_do_printf("error", "bad length");
		return;
	}

	json_do_array("communities");

	for (i = 0; i < len; i += 4) {
		memcpy(&a, data + i, sizeof(a));
		memcpy(&v, data + i + 2, sizeof(v));
		a = ntohs(a);
		v = ntohs(v);
		json_do_printf("community", "%s", fmt_community(a, v));
	}

	json_do_end();
}

static void
json_do_large_community(u_char *data, uint16_t len)
{
	uint32_t a, l1, l2;
	u_int16_t i;

	if (len % 12) {
		json_do_printf("error", "bad length");
		return;
	}

	json_do_array("large_communities");

	for (i = 0; i < len; i += 12) {
		memcpy(&a, data + i, sizeof(a));
		memcpy(&l1, data + i + 4, sizeof(l1));
		memcpy(&l2, data + i + 8, sizeof(l2));
		a = ntohl(a);
		l1 = ntohl(l1);
		l2 = ntohl(l2);

		json_do_printf("community", "%s",
		    fmt_large_community(a, l1, l2));
	}

	json_do_end();
}

static void
json_do_ext_community(u_char *data, uint16_t len)
{
	uint16_t i;

	if (len & 0x7) {
		json_do_printf("error", "bad length");
		return;
	}

	json_do_array("extended_communities");

	for (i = 0; i < len; i += 8)
		json_do_printf("community", "%s", fmt_ext_community(data + i));

	json_do_end();
}

static void
json_attr(u_char *data, size_t len, struct parse_result *res)
{
	struct bgpd_addr prefix;
	struct in_addr id;
	char *aspath;
	u_char *path;
	uint32_t as;
	uint16_t alen, afi, off, short_as;
	uint8_t flags, type, safi, aid, prefixlen;
	int e4, e2, pos;

	if (len < 3) {
		warnx("Too short BGP attrbute");
		return;
	}

	flags = data[0];
	type = data[1];
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

	json_do_array("attributes");

	json_do_object("attribute");
	json_do_printf("type", "%s", fmt_attr(type, -1));
	json_do_uint("length", alen);
	json_do_object("flags");
	json_do_bool("partial", flags & ATTR_PARTIAL);
	json_do_bool("transitive", flags & ATTR_TRANSITIVE);
	json_do_bool("optional", flags & ATTR_OPTIONAL);
	json_do_end();

	switch (type) {
	case ATTR_ORIGIN:
		if (alen == 1)
			json_do_printf("origin", "%s", fmt_origin(*data, 0));
		else
			json_do_printf("error", "bad length");
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
			json_do_printf("error", "bad AS-Path");
			break;
		}
		if (aspath_asprint(&aspath, path, alen) == -1)
			err(1, NULL);
		json_do_printf("aspath", "%s", aspath);
		free(aspath);
		if (path != data)
			free(path);
		break;
	case ATTR_NEXTHOP:
		if (alen == 4) {
			memcpy(&id, data, sizeof(id));
			json_do_printf("nexthop", "%s", inet_ntoa(id));
		} else
			json_do_printf("error", "bad length");
		break;
	case ATTR_MED:
	case ATTR_LOCALPREF:
		if (alen == 4) {
			uint32_t val;
			memcpy(&val, data, sizeof(val));
			json_do_uint("metric", ntohl(val));
		} else
			json_do_printf("error", "bad length");
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
			json_do_printf("error", "bad AS-Path");
			break;
		}
		json_do_uint("AS", as);
		json_do_printf("router_id", "%s", inet_ntoa(id));
		break;
	case ATTR_COMMUNITIES:
		json_do_community(data, alen);
		break;
	case ATTR_ORIGINATOR_ID:
		if (alen == 4) {
			memcpy(&id, data, sizeof(id));
			json_do_printf("originator", "%s", inet_ntoa(id));
		} else
			json_do_printf("error", "bad length");
		break;
	case ATTR_CLUSTER_LIST:
		json_do_array("cluster_list");
		for (off = 0; off + sizeof(id) <= alen;
		    off += sizeof(id)) {
			memcpy(&id, data + off, sizeof(id));
			json_do_printf("cluster_id", "%s", inet_ntoa(id));
		}
		json_do_end();
		break;
	case ATTR_MP_REACH_NLRI:
	case ATTR_MP_UNREACH_NLRI:
		if (alen < 3) {
bad_len:
			json_do_printf("error", "bad length");
			break;
		}
		memcpy(&afi, data, 2);
		data += 2;
		alen -= 2;
		afi = ntohs(afi);
		safi = *data++;
		alen--;

		if (afi2aid(afi, safi, &aid) == -1) {
			json_do_printf("error", "bad AFI/SAFI pair: %d/%d",
			    afi, safi);
			break;
		}
		json_do_printf("family", "%s", aid2str(aid));

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
				json_do_printf("error", "unhandled AID: %d",
				    aid);
				return;
			}
			/* ignore reserved (old SNPA) field as per RFC4760 */
			data += nhlen + 1;
			alen -= nhlen + 1;

			json_do_printf("nexthop", "%s", log_addr(&nexthop));
		}

		json_do_array("NLRI");
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
			case AID_VPN_IPv6:
				 pos = nlri_get_vpn6(data, alen, &prefix,
				     &prefixlen, 1);
				 break;
			default:
				json_do_printf("error", "unhandled AID: %d",
				    aid);
				return;
			}
			if (pos == -1) {
				json_do_printf("error", "bad %s prefix",
				    aid2str(aid));
				break;
			}
			json_do_printf("prefix", "%s/%u", log_addr(&prefix),
			    prefixlen);
			data += pos;
			alen -= pos;
		}
		break;
	case ATTR_EXT_COMMUNITIES:
		json_do_ext_community(data, alen);
		break;
	case ATTR_LARGE_COMMUNITIES:
		json_do_large_community(data, alen);
		break;
	case ATTR_ATOMIC_AGGREGATE:
	default:
		if (alen)
			json_do_hexdump("data", data, alen);
		break;
	}
}

static void
json_rib(struct ctl_show_rib *r, u_char *asdata, size_t aslen,
    struct parse_result *res)
{
	struct in_addr id;
	char *aspath;

	json_do_array("rib");

	json_do_object("rib_entry");

	json_do_printf("prefix", "%s/%u", log_addr(&r->prefix), r->prefixlen);

	if (aspath_asprint(&aspath, asdata, aslen) == -1)
		err(1, NULL);
	json_do_printf("aspath", "%s", aspath);
	free(aspath);

	json_do_printf("exit_nexthop", "%s", log_addr(&r->exit_nexthop));
	json_do_printf("true_nexthop", "%s", log_addr(&r->true_nexthop));

	json_do_object("neighbor");
	if (r->descr[0])
		json_do_printf("description", "%s", r->descr);
	json_do_printf("remote_addr", "%s", log_addr(&r->remote_addr));
	id.s_addr = htonl(r->remote_id);
	json_do_printf("bgp_id", "%s", inet_ntoa(id));
	json_do_end();

	/* flags */
	json_do_bool("valid", r->flags & F_PREF_ELIGIBLE);
	if (r->flags & F_PREF_ACTIVE)
		json_do_bool("best", 1);
	if (r->flags & F_PREF_INTERNAL)
		json_do_printf("source", "%s", "internal");
	else
		json_do_printf("source", "%s", "external");
	if (r->flags & F_PREF_STALE)
		json_do_bool("stale", 1);
	if (r->flags & F_PREF_ANNOUNCE)
		json_do_bool("announced", 1);

	/* various attribibutes */
	json_do_printf("ovs", "%s", fmt_ovs(r->validation_state, 0));
	json_do_printf("origin", "%s", fmt_origin(r->origin, 0));
	json_do_uint("metric", r->med);
	json_do_uint("localpref", r->local_pref);
	json_do_uint("weight", r->weight);
	json_do_printf("last_update", "%s", fmt_timeframe(r->age));

	/* keep the object open for communities and attribuites */
}

static void
json_rib_mem_element(const char *name, uint64_t count, uint64_t size,
    uint64_t refs)
{
	json_do_object(name);
	if (count != UINT64_MAX)
		json_do_uint("count", count);
	if (size != UINT64_MAX)
		json_do_uint("size", size);
	if (refs != UINT64_MAX)
		json_do_uint("references", refs);
	json_do_end();
}

static void
json_rib_mem(struct rde_memstats *stats)
{
	size_t pts = 0;
	int i;

	json_do_object("memory");
	for (i = 0; i < AID_MAX; i++) {
		if (stats->pt_cnt[i] == 0)
			continue;
		pts += stats->pt_cnt[i] * pt_sizes[i];
		json_rib_mem_element(aid_vals[i].name, stats->pt_cnt[i],
		    stats->pt_cnt[i] * pt_sizes[i], UINT64_MAX);
	}
	json_rib_mem_element("rib", stats->rib_cnt,
	    stats->rib_cnt * sizeof(struct rib_entry), UINT64_MAX);
	json_rib_mem_element("prefix", stats->prefix_cnt,
	    stats->prefix_cnt * sizeof(struct prefix), UINT64_MAX);
	json_rib_mem_element("rde_aspath", stats->path_cnt,
	    stats->path_cnt * sizeof(struct rde_aspath),
	    stats->path_refs);
	json_rib_mem_element("aspath", stats->aspath_cnt,
	    stats->aspath_size, stats->aspath_refs);
	json_rib_mem_element("community_entries", stats->comm_cnt,
	    stats->comm_cnt * sizeof(struct rde_community), UINT64_MAX);
	json_rib_mem_element("community", stats->comm_nmemb,
	    stats->comm_size * sizeof(struct community), stats->comm_refs);
	json_rib_mem_element("attributes_entries", stats->attr_cnt,
	    stats->attr_cnt * sizeof(struct attr), stats->attr_refs);
	json_rib_mem_element("attributes", stats->attr_dcnt,
	    stats->attr_data, UINT64_MAX);
	json_rib_mem_element("total", UINT64_MAX, 
	    pts + stats->prefix_cnt * sizeof(struct prefix) +
	    stats->rib_cnt * sizeof(struct rib_entry) +
	    stats->path_cnt * sizeof(struct rde_aspath) +
	    stats->aspath_size + stats->attr_cnt * sizeof(struct attr) +
	    stats->attr_data, UINT64_MAX);
	json_do_end();

	json_do_object("sets");
	json_rib_mem_element("as_set", stats->aset_nmemb,
	    stats->aset_size, UINT64_MAX);
	json_rib_mem_element("as_set_tables", stats->aset_cnt, UINT64_MAX,
	    UINT64_MAX);
	json_rib_mem_element("prefix_set", stats->pset_cnt, stats->pset_size,
	    UINT64_MAX);
	json_rib_mem_element("total", UINT64_MAX, 
	    stats->aset_size + stats->pset_size, UINT64_MAX);
	json_do_end();
}

static void
json_rib_hash(struct rde_hashstats *hash)
{
	double avg, dev;

	json_do_array("hashtables");

	avg = (double)hash->sum / (double)hash->num;
	dev = sqrt(fmax(0, hash->sumq / hash->num - avg * avg));

	json_do_object("hashtable");

	json_do_printf("name", "%s", hash->name);
	json_do_uint("size", hash->num);
	json_do_uint("entries", hash->sum);
	json_do_uint("min", hash->min);
	json_do_uint("max", hash->max);
	json_do_double("avg", avg);
	json_do_double("std_dev", dev);
	json_do_end();
}

static void
json_result(u_int rescode)
{
	if (rescode == 0)
		json_do_printf("status", "OK");
	else if (rescode >
	    sizeof(ctl_res_strerror)/sizeof(ctl_res_strerror[0])) {
		json_do_printf("status", "FAILED");
		json_do_printf("error", "unknown error %d", rescode);
	} else {
		json_do_printf("status", "FAILED");
		json_do_printf("error", "%s", ctl_res_strerror[rescode]);
	}
}

static void
json_tail(void)
{
	json_do_finish();
}

const struct output json_output = {
	.head = json_head,
	.neighbor = json_neighbor,
	.timer = json_timer,
	.fib = json_fib,
	.fib_table = json_fib_table,
	.nexthop = json_nexthop,
	.interface = json_interface,
	.communities = json_communities,
	.attr = json_attr,
	.rib = json_rib,
	.rib_mem = json_rib_mem,
	.rib_hash = json_rib_hash,
	.result = json_result,
	.tail = json_tail
};
