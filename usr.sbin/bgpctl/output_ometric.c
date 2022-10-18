/*	$OpenBSD: output_ometric.c,v 1.2 2022/10/18 12:28:36 claudio Exp $ */

/*
 * Copyright (c) 2022 Claudio Jeker <claudio@openbsd.org>
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
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "bgpd.h"
#include "session.h"
#include "rde.h"
#include "version.h"

#include "bgpctl.h"
#include "parser.h"
#include "ometric.h"

struct ometric *bgpd_info, *bgpd_scrape_time;
struct ometric *peer_info, *peer_state, *peer_state_raw, *peer_last_change,
		    *peer_last_read, *peer_last_write;
struct ometric *peer_prefixes_transmit, *peer_prefixes_receive;
struct ometric *peer_message_transmit, *peer_message_recieve;
struct ometric *peer_update_transmit, *peer_update_pending,
		   *peer_update_receive;
struct ometric *peer_withdraw_transmit, *peer_withdraw_pending,
		   *peer_withdraw_receive;
struct ometric *peer_rr_req_transmit, *peer_rr_req_receive;
struct ometric *peer_rr_borr_transmit, *peer_rr_borr_receive;
struct ometric *peer_rr_eorr_transmit, *peer_rr_eorr_receive;
struct ometric *rde_mem_size, *rde_mem_count, *rde_mem_ref_count;
struct ometric *rde_set_size, *rde_set_count, *rde_table_count;

struct timeval start_time, end_time;

static void
ometric_head(struct parse_result *arg)
{
	struct olabels *ol = NULL;
	const char *keys[4] = { "nodename", "domainname", "release", NULL };
	const char *values[4];
	char hostname[HOST_NAME_MAX + 1];
	char *domainname;

	bgpd_info = ometric_new(OMT_INFO, "bgpd_info", "bgpd information");
	bgpd_scrape_time = ometric_new(OMT_GAUGE, "bgpd_scrape_seconds",
	    "bgpd scrape time in seconds");

	gettimeofday(&start_time, NULL);

	if (gethostname(hostname, sizeof(hostname)))
		err(1, "gethostname");
	if ((domainname = strchr(hostname, '.')))
		*domainname++ = '\0';

	values[0] = hostname;
	values[1] = domainname;
	values[2] = BGPD_VERSION;
	values[3] = NULL;

	ol = olabels_new(keys, values);

	ometric_set_info(bgpd_info, NULL, NULL, ol);

	olabels_free(ol);

	/*
	 * per neighbor stats: attrs will be remote_as, remote_addr,
	 * description and group
	 */
	peer_info = ometric_new(OMT_INFO, "bgpd_peer_info",
	    "peer information");
	peer_state = ometric_new_state(statenames,
	    sizeof(statenames) / sizeof(statenames[0]), "bgpd_peer_state",
	    "peer session state");
	peer_state_raw = ometric_new(OMT_GAUGE, "bgpd_peer_state_raw",
	    "peer session state raw int value");
	peer_last_change = ometric_new(OMT_GAUGE,
	    "bgpd_peer_last_change_seconds",
	    "time in seconds since peer's last up/down state change");
	peer_last_read = ometric_new(OMT_GAUGE, "bgpd_peer_last_read_seconds",
	    "peer time since last read in seconds");
	peer_last_write = ometric_new(OMT_GAUGE, "bgpd_peer_last_write_seconds",
	    "peer time since last write in seconds");

	peer_prefixes_transmit = ometric_new(OMT_GAUGE,
	    "bgpd_peer_prefixes_transmit",
	    "number of prefixes sent to peer");
	peer_prefixes_receive = ometric_new(OMT_GAUGE,
	    "bgpd_peer_prefixes_receive",
	    "number of prefixes received from peer");

	peer_message_transmit = ometric_new(OMT_COUNTER,
	    "bgpd_peer_message_transmit_total",
	    "per message type count of tranmitted messages");
	peer_message_recieve = ometric_new(OMT_COUNTER,
	    "bgpd_peer_message_receive_total",
	    "per message type count of received messages");

	peer_update_transmit = ometric_new(OMT_COUNTER,
	    "bgpd_peer_update_transmit_total",
	    "number of prefixes sent as update");
	peer_update_pending = ometric_new(OMT_COUNTER,
	    "bgpd_peer_update_pending_total",
	    "number of pending update prefixes");
	peer_update_receive = ometric_new(OMT_COUNTER,
	    "bgpd_peer_update_receive_total",
	    "number of prefixes received as update");

	peer_withdraw_transmit = ometric_new(OMT_COUNTER,
	    "bgpd_peer_withdraw_transmit_total",
	    "number of witdrawn prefixes sent to peer");
	peer_withdraw_pending = ometric_new(OMT_COUNTER,
	    "bgpd_peer_withdraw_pending_total",
	    "number of pending withdrawn prefixes");
	peer_withdraw_receive = ometric_new(OMT_COUNTER,
	    "bgpd_peer_withdraw_receive_total",
	    "number of withdrawn prefixes received from peer");

	peer_rr_req_transmit = ometric_new(OMT_COUNTER,
	    "bgpd_peer_route_refresh_req_transmit_total",
	    "number of route-refresh request transmitted to peer");
	peer_rr_req_receive = ometric_new(OMT_COUNTER,
	    "bgpd_peer_route_refresh_req_receive_total",
	    "number of route-refresh request received from peer");
	peer_rr_borr_transmit = ometric_new(OMT_COUNTER,
	    "bgpd_peer_route_refresh_borr_transmit_total",
	    "number of ext. route-refresh BORR messages transmitted to peer");
	peer_rr_borr_receive = ometric_new(OMT_COUNTER,
	    "bgpd_peer_route_refresh_borr_receive_total",
	    "number of ext. route-refresh BORR messages received from peer");
	peer_rr_eorr_transmit = ometric_new(OMT_COUNTER,
	    "bgpd_peer_route_refresh_eorr_transmit_total",
	    "number of ext. route-refresh EORR messages transmitted to peer");
	peer_rr_eorr_receive = ometric_new(OMT_COUNTER,
	    "bgpd_peer_route_refresh_eorr_receive_total",
	    "number of ext. route-refresh EORR messages received from peer");

	/* RDE memory statistics */
	rde_mem_size = ometric_new(OMT_GAUGE,
	    "bgpd_rde_memory_usage_bytes", "memory usage in bytes");
	rde_mem_count = ometric_new(OMT_GAUGE,
	    "bgpd_rde_memory_count", "number of object in use");
	rde_mem_ref_count = ometric_new(OMT_GAUGE,
	    "bgpd_rde_memory_reference_count", "number of references held");

	rde_set_size = ometric_new(OMT_GAUGE,
	    "bgpd_rde_set_usage_bytes", "memory usage of set in bytes");
	rde_set_count = ometric_new(OMT_GAUGE,
	    "bgpd_rde_set_count", "number of object in set");
	rde_table_count = ometric_new(OMT_GAUGE,
	    "bgpd_rde_table_count", "number of as_set tables");
}

static void
ometric_neighbor_stats(struct peer *p, struct parse_result *arg)
{
	struct olabels *ol = NULL;
	const char *keys[5] = {
	    "remote_addr", "remote_as", "description", "group", NULL };
	const char *values[5];

	/* skip neighbor templates */
	if (p->conf.template)
		return;

	values[0] = log_addr(&p->conf.remote_addr);
	values[1] = log_as(p->conf.remote_as);
	values[2] = p->conf.descr;
	values[3] = p->conf.group;
	values[4] = NULL;

	ol = olabels_new(keys, values);

	ometric_set_info(peer_info, NULL, NULL, ol);
	ometric_set_state(peer_state, statenames[p->state], ol);
	ometric_set_int(peer_state_raw, p->state, ol);

	ometric_set_int(peer_last_change, get_monotime(p->stats.last_updown),
	    ol);

	if (p->state == STATE_ESTABLISHED) {
		ometric_set_int(peer_last_read,
		    get_monotime(p->stats.last_read), ol);
		ometric_set_int(peer_last_write,
		    get_monotime(p->stats.last_write), ol);
	}

	ometric_set_int(peer_prefixes_transmit, p->stats.prefix_out_cnt, ol);
	ometric_set_int(peer_prefixes_receive, p->stats.prefix_cnt, ol);

	ometric_set_int_with_label(peer_message_transmit,
	    p->stats.msg_sent_open, "message", "open", ol);
	ometric_set_int_with_label(peer_message_transmit,
	    p->stats.msg_sent_notification, "message", "notification", ol);
	ometric_set_int_with_label(peer_message_transmit,
	    p->stats.msg_sent_update, "message", "update", ol);
	ometric_set_int_with_label(peer_message_transmit,
	    p->stats.msg_sent_keepalive, "message", "keepalive", ol);
	ometric_set_int_with_label(peer_message_transmit,
	    p->stats.msg_sent_rrefresh, "message", "route_refresh", ol);

	ometric_set_int_with_label(peer_message_recieve,
	    p->stats.msg_rcvd_open, "message", "open", ol);
	ometric_set_int_with_label(peer_message_recieve,
	    p->stats.msg_rcvd_notification, "message", "notification", ol);
	ometric_set_int_with_label(peer_message_recieve,
	    p->stats.msg_rcvd_update, "message", "update", ol);
	ometric_set_int_with_label(peer_message_recieve,
	    p->stats.msg_rcvd_keepalive, "message", "keepalive", ol);
	ometric_set_int_with_label(peer_message_recieve,
	    p->stats.msg_rcvd_rrefresh, "message", "route_refresh", ol);

	ometric_set_int(peer_update_transmit, p->stats.prefix_sent_update, ol);
	ometric_set_int(peer_update_pending, p->stats.pending_update, ol);
	ometric_set_int(peer_update_receive, p->stats.prefix_rcvd_update, ol);
	ometric_set_int(peer_withdraw_transmit, p->stats.prefix_sent_withdraw,
	    ol);
	ometric_set_int(peer_withdraw_pending, p->stats.pending_withdraw, ol);
	ometric_set_int(peer_withdraw_receive, p->stats.prefix_rcvd_withdraw,
	    ol);

	ometric_set_int(peer_rr_req_transmit, p->stats.refresh_sent_req, ol);
	ometric_set_int(peer_rr_req_receive, p->stats.refresh_rcvd_req, ol);
	ometric_set_int(peer_rr_borr_transmit, p->stats.refresh_sent_borr, ol);
	ometric_set_int(peer_rr_borr_receive, p->stats.refresh_rcvd_borr, ol);
	ometric_set_int(peer_rr_eorr_transmit, p->stats.refresh_sent_eorr, ol);
	ometric_set_int(peer_rr_eorr_receive, p->stats.refresh_rcvd_eorr, ol);

	olabels_free(ol);
}

static void
ometric_rib_mem_element(const char *v, uint64_t count, uint64_t size,
    uint64_t refs)
{
	if (count != UINT64_MAX)
		ometric_set_int_with_label(rde_mem_count, count, "type", v,
		    NULL);
	if (size != UINT64_MAX)
		ometric_set_int_with_label(rde_mem_size, size, "type", v, NULL);
	if (refs != UINT64_MAX)
		ometric_set_int_with_label(rde_mem_ref_count, refs, "type", v,
		    NULL);
}

static void
ometric_rib_mem(struct rde_memstats *stats)
{
	size_t pts = 0;
	int i;

	for (i = 0; i < AID_MAX; i++) {
		if (stats->pt_cnt[i] == 0)
			continue;
		pts += stats->pt_cnt[i] * pt_sizes[i];
		ometric_rib_mem_element(aid_vals[i].name, stats->pt_cnt[i],
		    stats->pt_cnt[i] * pt_sizes[i], UINT64_MAX);
	}
	ometric_rib_mem_element("rib", stats->rib_cnt,
	    stats->rib_cnt * sizeof(struct rib_entry), UINT64_MAX);
	ometric_rib_mem_element("prefix", stats->prefix_cnt,
	    stats->prefix_cnt * sizeof(struct prefix), UINT64_MAX);
	ometric_rib_mem_element("rde_aspath", stats->path_cnt,
	    stats->path_cnt * sizeof(struct rde_aspath),
	    stats->path_refs);
	ometric_rib_mem_element("aspath", stats->aspath_cnt,
	    stats->aspath_size, UINT64_MAX);
	ometric_rib_mem_element("community_entries", stats->comm_cnt,
	    stats->comm_cnt * sizeof(struct rde_community), UINT64_MAX);
	ometric_rib_mem_element("community", stats->comm_nmemb,
	    stats->comm_size * sizeof(struct community), stats->comm_refs);
	ometric_rib_mem_element("attributes_entries", stats->attr_cnt,
	    stats->attr_cnt * sizeof(struct attr), stats->attr_refs);
	ometric_rib_mem_element("attributes", stats->attr_dcnt,
	    stats->attr_data, UINT64_MAX);

	ometric_rib_mem_element("total", UINT64_MAX, 
	    pts + stats->prefix_cnt * sizeof(struct prefix) +
	    stats->rib_cnt * sizeof(struct rib_entry) +
	    stats->path_cnt * sizeof(struct rde_aspath) +
	    stats->aspath_size + stats->attr_cnt * sizeof(struct attr) +
	    stats->attr_data, UINT64_MAX);

	ometric_set_int(rde_table_count, stats->aset_cnt, NULL);
	ometric_set_int_with_label(rde_set_size, stats->aset_size,
	   "type", "as_set", NULL);
	ometric_set_int_with_label(rde_set_count, stats->aset_nmemb,
	   "type", "as_set", NULL);
	ometric_set_int_with_label(rde_set_size, stats->pset_size,
	   "type", "prefix_set", NULL);
	ometric_set_int_with_label(rde_set_count, stats->pset_cnt,
	   "type", "prefix_set", NULL);
	ometric_rib_mem_element("set_total", UINT64_MAX, 
	    stats->aset_size + stats->pset_size, UINT64_MAX);
}

static void
ometric_tail(void)
{
	struct timeval elapsed_time;
	double scrape;

	gettimeofday(&end_time, NULL);
	timersub(&end_time, &start_time, &elapsed_time);

	scrape = (double)elapsed_time.tv_sec +
	    (double)elapsed_time.tv_usec / 1000000;

	ometric_set_float(bgpd_scrape_time, scrape, NULL);
	ometric_output_all();

	ometric_free_all();
}

const struct output ometric_output = {
	.head = ometric_head,
	.neighbor = ometric_neighbor_stats,
	.rib_mem = ometric_rib_mem,
	.tail = ometric_tail,
};
