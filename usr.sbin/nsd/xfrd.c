/*
 * xfrd.c - XFR (transfer) Daemon source file. Coordinates SOA updates.
 *
 * Copyright (c) 2001-2011, NLnet Labs. All rights reserved.
 *
 * See LICENSE for the license.
 *
 */

#include <config.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include "xfrd.h"
#include "xfrd-tcp.h"
#include "xfrd-disk.h"
#include "xfrd-notify.h"
#include "options.h"
#include "util.h"
#include "netio.h"
#include "region-allocator.h"
#include "nsd.h"
#include "packet.h"
#include "difffile.h"
#include "ipc.h"

#define XFRD_TRANSFER_TIMEOUT_START 10 /* empty zone timeout is between x and 2*x seconds */
#define XFRD_TRANSFER_TIMEOUT_MAX 14400 /* empty zone timeout max expbackoff */
#define XFRD_UDP_TIMEOUT 10 /* seconds, before a udp request times out */
#define XFRD_NO_IXFR_CACHE 172800 /* 48h before retrying ixfr's after notimpl */
#define XFRD_LOWERBOUND_REFRESH 1 /* seconds, smallest refresh timeout */
#define XFRD_LOWERBOUND_RETRY 1 /* seconds, smallest retry timeout */
#define XFRD_MAX_ROUNDS 3 /* max number of rounds along the masters */
#define XFRD_TSIG_MAX_UNSIGNED 103 /* max number of packets without tsig in a tcp stream. */
			/* rfc recommends 100, +3 for offbyone errors/interoperability. */

/* the daemon state */
xfrd_state_t* xfrd = 0;

/* main xfrd loop */
static void xfrd_main();
/* shut down xfrd, close sockets. */
static void xfrd_shutdown();
/* create zone rbtree at start */
static void xfrd_init_zones();
/* free up memory used by main database */
static void xfrd_free_namedb();

/* handle zone timeout, event */
static void xfrd_handle_zone(netio_type *netio,
	netio_handler_type *handler, netio_event_types_type event_types);
/* handle incoming notification message. soa can be NULL. true if transfer needed. */
static int xfrd_handle_incoming_notify(xfrd_zone_t* zone, xfrd_soa_t* soa);

/* call with buffer just after the soa dname. returns 0 on error. */
static int xfrd_parse_soa_info(buffer_type* packet, xfrd_soa_t* soa);
/* set the zone state to a new state (takes care of expiry messages) */
static void xfrd_set_zone_state(xfrd_zone_t* zone, enum xfrd_zone_state new_zone_state);
/* set timer for retry amount (depends on zone_state) */
static void xfrd_set_timer_retry(xfrd_zone_t* zone);
/* set timer for refresh timeout (depends on zone_state) */
static void xfrd_set_timer_refresh(xfrd_zone_t* zone);

/* set reload timeout */
static void xfrd_set_reload_timeout();
/* handle reload timeout */
static void xfrd_handle_reload(netio_type *netio,
	netio_handler_type *handler, netio_event_types_type event_types);

/* send expiry notifications to nsd */
static void xfrd_send_expire_notification(xfrd_zone_t* zone);
/* send ixfr request, returns fd of connection to read on */
static int xfrd_send_ixfr_request_udp(xfrd_zone_t* zone);
/* obtain udp socket slot */
static void xfrd_udp_obtain(xfrd_zone_t* zone);

/* read data via udp */
static void xfrd_udp_read(xfrd_zone_t* zone);

/* find master by notify number */
static int find_same_master_notify(xfrd_zone_t* zone, int acl_num_nfy);

void
xfrd_init(int socket, struct nsd* nsd)
{
	region_type* region;

	assert(xfrd == 0);
	/* to setup signalhandling */
	nsd->server_kind = NSD_SERVER_BOTH;

	region = region_create(xalloc, free);
	xfrd = (xfrd_state_t*)region_alloc(region, sizeof(xfrd_state_t));
	memset(xfrd, 0, sizeof(xfrd_state_t));
	xfrd->region = region;
	xfrd->xfrd_start_time = time(0);
	xfrd->netio = netio_create(xfrd->region);
	xfrd->nsd = nsd;
	xfrd->packet = buffer_create(xfrd->region, QIOBUFSZ);
	xfrd->udp_waiting_first = NULL;
	xfrd->udp_waiting_last = NULL;
	xfrd->udp_use_num = 0;
	xfrd->ipc_pass = buffer_create(xfrd->region, QIOBUFSZ);
	xfrd->parent_soa_info_pass = 0;

	/* add the handlers already, because this involves allocs */
	xfrd->reload_handler.fd = -1;
	xfrd->reload_handler.timeout = NULL;
	xfrd->reload_handler.user_data = xfrd;
	xfrd->reload_handler.event_types = NETIO_EVENT_TIMEOUT;
	xfrd->reload_handler.event_handler = xfrd_handle_reload;
	xfrd->reload_timeout.tv_sec = 0;
	xfrd->reload_cmd_last_sent = xfrd->xfrd_start_time;
	xfrd->can_send_reload = 1;

	xfrd->ipc_send_blocked = 0;
	xfrd->ipc_handler.fd = socket;
	xfrd->ipc_handler.timeout = NULL;
	xfrd->ipc_handler.user_data = xfrd;
	xfrd->ipc_handler.event_types = NETIO_EVENT_READ;
	xfrd->ipc_handler.event_handler = xfrd_handle_ipc;
	xfrd->ipc_conn = xfrd_tcp_create(xfrd->region);
	/* not reading using ipc_conn yet */
	xfrd->ipc_conn->is_reading = 0;
	xfrd->ipc_conn->fd = xfrd->ipc_handler.fd;
	xfrd->ipc_conn_write = xfrd_tcp_create(xfrd->region);
	xfrd->ipc_conn_write->fd = xfrd->ipc_handler.fd;
	xfrd->need_to_send_reload = 0;
	xfrd->sending_zone_state = 0;
	xfrd->dirty_zones = stack_create(xfrd->region,
			nsd_options_num_zones(nsd->options));

	xfrd->notify_waiting_first = NULL;
	xfrd->notify_waiting_last = NULL;
	xfrd->notify_udp_num = 0;

	xfrd->tcp_set = xfrd_tcp_set_create(xfrd->region);
	xfrd->tcp_set->tcp_timeout = nsd->tcp_timeout;
	srandom((unsigned long) getpid() * (unsigned long) time(NULL));

	DEBUG(DEBUG_XFRD,1, (LOG_INFO, "xfrd pre-startup"));
	diff_snip_garbage(nsd->db, nsd->options);
	xfrd_init_zones();
	xfrd_free_namedb();
	xfrd_read_state(xfrd);
	xfrd_send_expy_all_zones();

	/* add handlers after zone handlers so they are before them in list */
	netio_add_handler(xfrd->netio, &xfrd->reload_handler);
	netio_add_handler(xfrd->netio, &xfrd->ipc_handler);

	DEBUG(DEBUG_XFRD,1, (LOG_INFO, "xfrd startup"));
	xfrd_main();
}

static void
xfrd_main()
{
	xfrd->shutdown = 0;
	while(!xfrd->shutdown)
	{
		/* dispatch may block for a longer period, so current is gone */
		xfrd->got_time = 0;
		if(netio_dispatch(xfrd->netio, NULL, 0) == -1) {
			if (errno != EINTR) {
				log_msg(LOG_ERR,
					"xfrd netio_dispatch failed: %s",
					strerror(errno));
			}
		}
		if(xfrd->nsd->signal_hint_quit || xfrd->nsd->signal_hint_shutdown)
			xfrd->shutdown = 1;
	}
	xfrd_shutdown();
}

static void
xfrd_shutdown()
{
	xfrd_zone_t* zone;
	int i;

	DEBUG(DEBUG_XFRD,1, (LOG_INFO, "xfrd shutdown"));
	xfrd_write_state(xfrd);
	close(xfrd->ipc_handler.fd);
	/* close tcp sockets */
	for(i=0; i<XFRD_MAX_TCP; i++)
	{
		if(xfrd->tcp_set->tcp_state[i]->fd != -1) {
			close(xfrd->tcp_set->tcp_state[i]->fd);
			xfrd->tcp_set->tcp_state[i]->fd = -1;
		}
	}
	/* close udp sockets */
	RBTREE_FOR(zone, xfrd_zone_t*, xfrd->zones)
	{
		if(zone->tcp_conn==-1 && zone->zone_handler.fd != -1) {
			close(zone->zone_handler.fd);
			zone->zone_handler.fd = -1;
		}
		close_notify_fds(xfrd->notify_zones);
	}

	/* shouldn't we clean up memory used by xfrd process */
	DEBUG(DEBUG_XFRD,1, (LOG_INFO, "xfrd shutdown complete"));

	exit(0);
}

static void
xfrd_init_zones()
{
	zone_type *dbzone;
	zone_options_t *zone_opt;
	xfrd_zone_t *xzone;
	const dname_type* dname;

	assert(xfrd->zones == 0);
	assert(xfrd->nsd->db != 0);

	xfrd->zones = rbtree_create(xfrd->region,
		(int (*)(const void *, const void *)) dname_compare);
	xfrd->notify_zones = rbtree_create(xfrd->region,
		(int (*)(const void *, const void *)) dname_compare);

	RBTREE_FOR(zone_opt, zone_options_t*, xfrd->nsd->options->zone_options)
	{
		DEBUG(DEBUG_XFRD,1, (LOG_INFO, "Zone %s\n", zone_opt->name));
		dname = dname_parse(xfrd->region, zone_opt->name);
		if(!dname) {
			log_msg(LOG_ERR, "xfrd: Could not parse zone name %s.", zone_opt->name);
			continue;
		}

		dbzone = domain_find_zone(domain_table_find(xfrd->nsd->db->domains, dname));
		if(dbzone && dname_compare(dname, domain_dname(dbzone->apex)) != 0)
			dbzone = 0; /* we found a parent zone */
		DEBUG(DEBUG_XFRD,1, (LOG_INFO, "xfrd: adding %s zone %s\n",
			dbzone?"filled":"empty", zone_opt->name));

		init_notify_send(xfrd->notify_zones, xfrd->netio,
			xfrd->region, dname, zone_opt, dbzone);
		if(!zone_is_slave(zone_opt)) {
			DEBUG(DEBUG_XFRD,1, (LOG_INFO, "xfrd: zone %s, master zone has no outgoing xfr requests", zone_opt->name));
			continue;
		}

		xzone = (xfrd_zone_t*)region_alloc(xfrd->region, sizeof(xfrd_zone_t));
		memset(xzone, 0, sizeof(xfrd_zone_t));
		xzone->apex = dname;
		xzone->apex_str = zone_opt->name;
		xzone->state = xfrd_zone_refreshing;
		xzone->dirty = 0;
		xzone->zone_options = zone_opt;
		/* first retry will use first master */
		xzone->master = 0;
		xzone->master_num = 0;
		xzone->next_master = 0;
		xzone->fresh_xfr_timeout = XFRD_TRANSFER_TIMEOUT_START;

		xzone->soa_nsd_acquired = 0;
		xzone->soa_disk_acquired = 0;
		xzone->soa_notified_acquired = 0;
		/* [0]=1, [1]=0; "." domain name */
		xzone->soa_nsd.prim_ns[0] = 1;
		xzone->soa_nsd.email[0] = 1;
		xzone->soa_disk.prim_ns[0]=1;
		xzone->soa_disk.email[0]=1;
		xzone->soa_notified.prim_ns[0]=1;
		xzone->soa_notified.email[0]=1;

		xzone->zone_handler.fd = -1;
		xzone->zone_handler.timeout = 0;
		xzone->zone_handler.user_data = xzone;
		xzone->zone_handler.event_types =
			NETIO_EVENT_READ|NETIO_EVENT_TIMEOUT;
		xzone->zone_handler.event_handler = xfrd_handle_zone;
		netio_add_handler(xfrd->netio, &xzone->zone_handler);
		xzone->tcp_conn = -1;
		xzone->tcp_waiting = 0;
		xzone->udp_waiting = 0;

		tsig_create_record_custom(&xzone->tsig, xfrd->region, 0, 0, 4);

		if(dbzone && dbzone->soa_rrset && dbzone->soa_rrset->rrs) {
			xzone->soa_nsd_acquired = xfrd_time();
			xzone->soa_disk_acquired = xfrd_time();
			/* we only use the first SOA in the rrset */
			xfrd_copy_soa(&xzone->soa_nsd, dbzone->soa_rrset->rrs);
			xfrd_copy_soa(&xzone->soa_disk, dbzone->soa_rrset->rrs);
		}
		/* set refreshing anyway, we have data but it may be old */
		xfrd_set_refresh_now(xzone);

		xzone->node.key = dname;
		rbtree_insert(xfrd->zones, (rbnode_t*)xzone);
	}
	DEBUG(DEBUG_XFRD,1, (LOG_INFO, "xfrd: started server %d secondary zones", (int)xfrd->zones->count));
}

void
xfrd_send_expy_all_zones()
{
	xfrd_zone_t* zone;
	RBTREE_FOR(zone, xfrd_zone_t*, xfrd->zones)
	{
		xfrd_send_expire_notification(zone);
	}
}

void
xfrd_reopen_logfile()
{
	if (xfrd->nsd->file_rotation_ok)
		log_reopen(xfrd->nsd->log_filename, 0);
}

static void
xfrd_free_namedb()
{
	namedb_close(xfrd->nsd->db);
	xfrd->nsd->db = 0;
}

static void
xfrd_set_timer_refresh(xfrd_zone_t* zone)
{
	time_t set_refresh;
	time_t set_expire;
	time_t set_min;
	time_t set;
	if(zone->soa_disk_acquired == 0 || zone->state != xfrd_zone_ok) {
		xfrd_set_timer_retry(zone);
		return;
	}
	/* refresh or expire timeout, whichever is earlier */
	set_refresh = zone->soa_disk_acquired + ntohl(zone->soa_disk.refresh);
	set_expire = zone->soa_disk_acquired + ntohl(zone->soa_disk.expire);
	if(set_refresh < set_expire)
		set = set_refresh;
	else set = set_expire;
	set_min = zone->soa_disk_acquired + XFRD_LOWERBOUND_REFRESH;
	if(set < set_min)
		set = set_min;
	xfrd_set_timer(zone, set);
}

static void
xfrd_set_timer_retry(xfrd_zone_t* zone)
{
	/* set timer for next retry or expire timeout if earlier. */
	if(zone->soa_disk_acquired == 0) {
		/* if no information, use reasonable timeout */
		xfrd_set_timer(zone, xfrd_time() + zone->fresh_xfr_timeout
			+ random()%zone->fresh_xfr_timeout);
		/* exponential backoff - some master data in zones is paid-for
		   but non-working, and will not get fixed. */
		zone->fresh_xfr_timeout *= 2;
		if(zone->fresh_xfr_timeout > XFRD_TRANSFER_TIMEOUT_MAX)
			zone->fresh_xfr_timeout = XFRD_TRANSFER_TIMEOUT_MAX;
	} else if(zone->state == xfrd_zone_expired ||
		xfrd_time() + ntohl(zone->soa_disk.retry) <
		zone->soa_disk_acquired + ntohl(zone->soa_disk.expire))
	{
		if(ntohl(zone->soa_disk.retry) < XFRD_LOWERBOUND_RETRY)
			xfrd_set_timer(zone, xfrd_time() + XFRD_LOWERBOUND_RETRY);
		else
			xfrd_set_timer(zone, xfrd_time() + ntohl(zone->soa_disk.retry));
	} else {
		if(ntohl(zone->soa_disk.expire) < XFRD_LOWERBOUND_RETRY)
			xfrd_set_timer(zone, xfrd_time() + XFRD_LOWERBOUND_RETRY);
		else
			xfrd_set_timer(zone, zone->soa_disk_acquired +
				ntohl(zone->soa_disk.expire));
	}
}

static void
xfrd_handle_zone(netio_type* ATTR_UNUSED(netio),
	netio_handler_type *handler, netio_event_types_type event_types)
{
	xfrd_zone_t* zone = (xfrd_zone_t*)handler->user_data;

	if(zone->tcp_conn != -1) {
		/* busy in tcp transaction */
		if(xfrd_tcp_is_reading(xfrd->tcp_set, zone->tcp_conn) && event_types & NETIO_EVENT_READ) {
			DEBUG(DEBUG_XFRD,1, (LOG_INFO, "xfrd: zone %s event tcp read", zone->apex_str));
			xfrd_set_timer(zone, xfrd_time() + xfrd->tcp_set->tcp_timeout);
			xfrd_tcp_read(xfrd->tcp_set, zone);
			return;
		} else if(!xfrd_tcp_is_reading(xfrd->tcp_set, zone->tcp_conn) && event_types & NETIO_EVENT_WRITE) {
			DEBUG(DEBUG_XFRD,1, (LOG_INFO, "xfrd: zone %s event tcp write", zone->apex_str));
			xfrd_set_timer(zone, xfrd_time() + xfrd->tcp_set->tcp_timeout);
			xfrd_tcp_write(xfrd->tcp_set, zone);
			return;
		} else if(event_types & NETIO_EVENT_TIMEOUT) {
			/* tcp connection timed out. Stop it. */
			DEBUG(DEBUG_XFRD,1, (LOG_INFO, "xfrd: zone %s event tcp timeout", zone->apex_str));
			xfrd_tcp_release(xfrd->tcp_set, zone);
			/* continue to retry; as if a timeout happened */
			event_types = NETIO_EVENT_TIMEOUT;
		}
	}

	if(event_types & NETIO_EVENT_READ) {
		/* busy in udp transaction */
		DEBUG(DEBUG_XFRD,1, (LOG_INFO, "xfrd: zone %s event udp read", zone->apex_str));
		xfrd_set_refresh_now(zone);
		xfrd_udp_read(zone);
		return;
	}

	/* timeout */
	DEBUG(DEBUG_XFRD,1, (LOG_INFO, "xfrd: zone %s timeout", zone->apex_str));
	if(handler->fd != -1) {
		assert(zone->tcp_conn == -1);
		xfrd_udp_release(zone);
	}

	if(zone->tcp_waiting) {
		DEBUG(DEBUG_XFRD,1, (LOG_ERR, "xfrd: zone %s skips retry, TCP connections full",
			zone->apex_str));
		xfrd_unset_timer(zone);
		return;
	}
	if(zone->udp_waiting) {
		DEBUG(DEBUG_XFRD,1, (LOG_ERR, "xfrd: zone %s skips retry, UDP connections full",
			zone->apex_str));
		xfrd_unset_timer(zone);
		return;
	}

	if(zone->soa_disk_acquired)
	{
		if (zone->state != xfrd_zone_expired &&
			(uint32_t)xfrd_time() >= zone->soa_disk_acquired + ntohl(zone->soa_disk.expire)) {
			/* zone expired */
			log_msg(LOG_ERR, "xfrd: zone %s has expired", zone->apex_str);
			xfrd_set_zone_state(zone, xfrd_zone_expired);
		}
		else if(zone->state == xfrd_zone_ok &&
			(uint32_t)xfrd_time() >= zone->soa_disk_acquired + ntohl(zone->soa_disk.refresh)) {
			/* zone goes to refreshing state. */
			DEBUG(DEBUG_XFRD,1, (LOG_INFO, "xfrd: zone %s is refreshing", zone->apex_str));
			xfrd_set_zone_state(zone, xfrd_zone_refreshing);
		}
	}
	/* make a new request */
	xfrd_make_request(zone);
}

void
xfrd_make_request(xfrd_zone_t* zone)
{
	if(zone->next_master != -1) {
		/* we are told to use this next master */
		DEBUG(DEBUG_XFRD,1, (LOG_INFO,
			"xfrd zone %s use master %i",
			zone->apex_str, zone->next_master));
		zone->master_num = zone->next_master;
		zone->master = acl_find_num(
			zone->zone_options->request_xfr, zone->master_num);
		/* if there is no next master, fallback to use the first one */
		if(!zone->master) {
			zone->master = zone->zone_options->request_xfr;
			zone->master_num = 0;
		}
		/* fallback to cycle master */
		zone->next_master = -1;
		zone->round_num = 0; /* fresh set of retries after notify */
	} else {
		/* cycle master */

		if(zone->round_num != -1 && zone->master && zone->master->next)
		{
			/* try the next master */
			zone->master = zone->master->next;
			zone->master_num++;
		} else {
			/* start a new round */
			zone->master = zone->zone_options->request_xfr;
			zone->master_num = 0;
			zone->round_num++;
		}
		if(zone->round_num >= XFRD_MAX_ROUNDS) {
			/* tried all servers that many times, wait */
			zone->round_num = -1;
			xfrd_set_timer_retry(zone);
			DEBUG(DEBUG_XFRD,1, (LOG_INFO,
				"xfrd zone %s makereq wait_retry, rd %d mr %d nx %d",
				zone->apex_str, zone->round_num, zone->master_num, zone->next_master));
			return;
		}
	}

	/* cache ixfr_disabled only for XFRD_NO_IXFR_CACHE time */
	if (zone->master->ixfr_disabled &&
	   (zone->master->ixfr_disabled + XFRD_NO_IXFR_CACHE) <= time(NULL)) {
		DEBUG(DEBUG_XFRD,1, (LOG_INFO, "clear negative caching ixfr "
						"disabled for master %s num "
						"%d ",
			zone->master->ip_address_spec, zone->master_num));
		zone->master->ixfr_disabled = 0;
	}

	DEBUG(DEBUG_XFRD,1, (LOG_INFO, "xfrd zone %s make request round %d mr %d nx %d",
		zone->apex_str, zone->round_num, zone->master_num, zone->next_master));
	/* perform xfr request */
	if (!zone->master->use_axfr_only && zone->soa_disk_acquired > 0 &&
		!zone->master->ixfr_disabled) {

		if (zone->master->allow_udp) {
			xfrd_set_timer(zone, xfrd_time() + XFRD_UDP_TIMEOUT);
			xfrd_udp_obtain(zone);
		}
		else { /* doing 3 rounds of IXFR/TCP might not be useful */
			xfrd_set_timer(zone, xfrd_time() + xfrd->tcp_set->tcp_timeout);
			xfrd_tcp_obtain(xfrd->tcp_set, zone);
		}
	}
	else if (zone->master->use_axfr_only || zone->soa_disk_acquired <= 0) {
		xfrd_set_timer(zone, xfrd_time() + xfrd->tcp_set->tcp_timeout);
		xfrd_tcp_obtain(xfrd->tcp_set, zone);
	}
	else if (zone->master->ixfr_disabled) {
		if (zone->zone_options->allow_axfr_fallback) {
			xfrd_set_timer(zone, xfrd_time() + xfrd->tcp_set->tcp_timeout);
			xfrd_tcp_obtain(xfrd->tcp_set, zone);
		}
		else
			DEBUG(DEBUG_XFRD,1, (LOG_INFO, "xfrd zone %s axfr "
				"fallback not allowed, skipping master %s.",
				zone->apex_str, zone->master->ip_address_spec));
	}
}

static void
xfrd_udp_obtain(xfrd_zone_t* zone)
{
	assert(zone->udp_waiting == 0);
	if(zone->tcp_conn != -1) {
		/* no tcp and udp at the same time */
		xfrd_tcp_release(xfrd->tcp_set, zone);
	}
	if(xfrd->udp_use_num < XFRD_MAX_UDP) {
		xfrd->udp_use_num++;
		zone->zone_handler.fd = xfrd_send_ixfr_request_udp(zone);
		if(zone->zone_handler.fd == -1)
			xfrd->udp_use_num--;
		return;
	}
	/* queue the zone as last */
	zone->udp_waiting = 1;
	zone->udp_waiting_next = NULL;
	if(!xfrd->udp_waiting_first)
		xfrd->udp_waiting_first = zone;
	if(xfrd->udp_waiting_last)
		xfrd->udp_waiting_last->udp_waiting_next = zone;
	xfrd->udp_waiting_last = zone;
	xfrd_unset_timer(zone);
}

time_t
xfrd_time()
{
	if(!xfrd->got_time) {
		xfrd->current_time = time(0);
		xfrd->got_time = 1;
	}
	return xfrd->current_time;
}

void
xfrd_copy_soa(xfrd_soa_t* soa, rr_type* rr)
{
	const uint8_t* rr_ns_wire = dname_name(domain_dname(rdata_atom_domain(rr->rdatas[0])));
	uint8_t rr_ns_len = domain_dname(rdata_atom_domain(rr->rdatas[0]))->name_size;
	const uint8_t* rr_em_wire = dname_name(domain_dname(rdata_atom_domain(rr->rdatas[1])));
	uint8_t rr_em_len = domain_dname(rdata_atom_domain(rr->rdatas[1]))->name_size;

	if(rr->type != TYPE_SOA || rr->rdata_count != 7) {
		log_msg(LOG_ERR, "xfrd: copy_soa called with bad rr, type %d rrs %u.",
			rr->type, rr->rdata_count);
		return;
	}
	DEBUG(DEBUG_XFRD,1, (LOG_INFO, "xfrd: copy_soa rr, type %d rrs %u, ttl %u.",
			rr->type, rr->rdata_count, rr->ttl));
	soa->type = htons(rr->type);
	soa->klass = htons(rr->klass);
	soa->ttl = htonl(rr->ttl);
	soa->rdata_count = htons(rr->rdata_count);

	/* copy dnames */
	soa->prim_ns[0] = rr_ns_len;
	memcpy(soa->prim_ns+1, rr_ns_wire, rr_ns_len);
	soa->email[0] = rr_em_len;
	memcpy(soa->email+1, rr_em_wire, rr_em_len);

	/* already in network format */
	memcpy(&soa->serial, rdata_atom_data(rr->rdatas[2]), sizeof(uint32_t));
	memcpy(&soa->refresh, rdata_atom_data(rr->rdatas[3]), sizeof(uint32_t));
	memcpy(&soa->retry, rdata_atom_data(rr->rdatas[4]), sizeof(uint32_t));
	memcpy(&soa->expire, rdata_atom_data(rr->rdatas[5]), sizeof(uint32_t));
	memcpy(&soa->minimum, rdata_atom_data(rr->rdatas[6]), sizeof(uint32_t));
	DEBUG(DEBUG_XFRD,1, (LOG_INFO,
		"xfrd: copy_soa rr, serial %u refresh %u retry %u expire %u",
		ntohl(soa->serial), ntohl(soa->refresh), ntohl(soa->retry),
		ntohl(soa->expire)));
}

static void
xfrd_set_zone_state(xfrd_zone_t* zone, enum xfrd_zone_state s)
{
	if(s != zone->state) {
		enum xfrd_zone_state old = zone->state;
		zone->state = s;
		if(s == xfrd_zone_expired || old == xfrd_zone_expired) {
			xfrd_send_expire_notification(zone);
		}
	}
}

void
xfrd_set_refresh_now(xfrd_zone_t* zone)
{
	xfrd_set_timer(zone, xfrd_time());
	DEBUG(DEBUG_XFRD,1, (LOG_INFO, "xfrd zone %s sets timeout right now, state %d",
		zone->apex_str, zone->state));
}

void
xfrd_unset_timer(xfrd_zone_t* zone)
{
	zone->zone_handler.timeout = NULL;
}

void
xfrd_set_timer(xfrd_zone_t* zone, time_t t)
{
	/* randomize the time, within 90%-100% of original */
	/* not later so zones cannot expire too late */
	/* only for times far in the future */
	if(t > xfrd_time() + 10) {
		time_t extra = t - xfrd_time();
		time_t base = extra*9/10;
		t = xfrd_time() + base + random()%(extra-base);
	}

	zone->zone_handler.timeout = &zone->timeout;
	zone->timeout.tv_sec = t;
	zone->timeout.tv_nsec = 0;
}

void
xfrd_handle_incoming_soa(xfrd_zone_t* zone,
	xfrd_soa_t* soa, time_t acquired)
{
	if(soa == NULL) {
		/* nsd no longer has a zone in memory */
		zone->soa_nsd_acquired = 0;
		xfrd_set_zone_state(zone, xfrd_zone_refreshing);
		xfrd_set_refresh_now(zone);
		return;
	}
	if(zone->soa_nsd_acquired && soa->serial == zone->soa_nsd.serial)
		return;

	if(zone->soa_disk_acquired && soa->serial == zone->soa_disk.serial)
	{
		/* soa in disk has been loaded in memory */
		log_msg(LOG_INFO, "Zone %s serial %u is updated to %u.",
			zone->apex_str, ntohl(zone->soa_nsd.serial),
			ntohl(soa->serial));
		zone->soa_nsd = zone->soa_disk;
		zone->soa_nsd_acquired = zone->soa_disk_acquired;
		if((uint32_t)xfrd_time() - zone->soa_disk_acquired
			< ntohl(zone->soa_disk.refresh))
		{
			/* zone ok, wait for refresh time */
			xfrd_set_zone_state(zone, xfrd_zone_ok);
			zone->round_num = -1;
			xfrd_set_timer_refresh(zone);
		} else if((uint32_t)xfrd_time() - zone->soa_disk_acquired
			< ntohl(zone->soa_disk.expire))
		{
			/* zone refreshing */
			xfrd_set_zone_state(zone, xfrd_zone_refreshing);
			xfrd_set_refresh_now(zone);
		}
		if((uint32_t)xfrd_time() - zone->soa_disk_acquired
			>= ntohl(zone->soa_disk.expire)) {
			/* zone expired */
			xfrd_set_zone_state(zone, xfrd_zone_expired);
			xfrd_set_refresh_now(zone);
		}

		if(zone->soa_notified_acquired != 0 &&
			(zone->soa_notified.serial == 0 ||
		   	compare_serial(ntohl(zone->soa_disk.serial),
				ntohl(zone->soa_notified.serial)) >= 0))
		{	/* read was in response to this notification */
			zone->soa_notified_acquired = 0;
		}
		if(zone->soa_notified_acquired && zone->state == xfrd_zone_ok)
		{
			/* refresh because of notification */
			xfrd_set_zone_state(zone, xfrd_zone_refreshing);
			xfrd_set_refresh_now(zone);
		}
		xfrd_send_notify(xfrd->notify_zones, zone->apex, &zone->soa_nsd);
		return;
	}

	/* user must have manually provided zone data */
	DEBUG(DEBUG_XFRD,1, (LOG_INFO,
		"xfrd: zone %s serial %u from unknown source. refreshing",
		zone->apex_str, ntohl(soa->serial)));
	zone->soa_nsd = *soa;
	zone->soa_disk = *soa;
	zone->soa_nsd_acquired = acquired;
	zone->soa_disk_acquired = acquired;
	if(zone->soa_notified_acquired != 0 &&
		(zone->soa_notified.serial == 0 ||
	   	compare_serial(ntohl(zone->soa_disk.serial),
			ntohl(zone->soa_notified.serial)) >= 0))
	{	/* user provided in response to this notification */
		zone->soa_notified_acquired = 0;
	}
	xfrd_set_zone_state(zone, xfrd_zone_refreshing);
	xfrd_set_refresh_now(zone);
	xfrd_send_notify(xfrd->notify_zones, zone->apex, &zone->soa_nsd);
}

static void
xfrd_send_expire_notification(xfrd_zone_t* zone)
{
	if(zone->dirty)
		return; /* already queued */
	/* enqueue */
	assert(xfrd->dirty_zones->num < xfrd->dirty_zones->capacity);
	zone->dirty = 1;
	stack_push(xfrd->dirty_zones, (void*)zone);
	xfrd->ipc_handler.event_types |= NETIO_EVENT_WRITE;
}

int
xfrd_udp_read_packet(buffer_type* packet, int fd)
{
	ssize_t received;

	/* read the data */
	buffer_clear(packet);
	received = recvfrom(fd, buffer_begin(packet), buffer_remaining(packet),
		0, NULL, NULL);
	if(received == -1) {
		log_msg(LOG_ERR, "xfrd: recvfrom failed: %s",
			strerror(errno));
		return 0;
	}
	buffer_set_limit(packet, received);
	return 1;
}

void
xfrd_udp_release(xfrd_zone_t* zone)
{
	assert(zone->udp_waiting == 0);
	if(zone->zone_handler.fd != -1)
		close(zone->zone_handler.fd);
	zone->zone_handler.fd = -1;
	/* see if there are waiting zones */
	if(xfrd->udp_use_num == XFRD_MAX_UDP)
	{
		while(xfrd->udp_waiting_first) {
			/* snip off waiting list */
			xfrd_zone_t* wz = xfrd->udp_waiting_first;
			assert(wz->udp_waiting);
			wz->udp_waiting = 0;
			xfrd->udp_waiting_first = wz->udp_waiting_next;
			if(xfrd->udp_waiting_last == wz)
				xfrd->udp_waiting_last = NULL;
			/* see if this zone needs udp connection */
			if(wz->tcp_conn == -1) {
				wz->zone_handler.fd =
					xfrd_send_ixfr_request_udp(wz);
				if(wz->zone_handler.fd != -1)
					return;
			}
		}
	}
	/* no waiting zones */
	if(xfrd->udp_use_num > 0)
		xfrd->udp_use_num--;
}

static void
xfrd_udp_read(xfrd_zone_t* zone)
{
	DEBUG(DEBUG_XFRD,1, (LOG_INFO, "xfrd: zone %s read udp data", zone->apex_str));
	if(!xfrd_udp_read_packet(xfrd->packet, zone->zone_handler.fd)) {
		xfrd_udp_release(zone);
		return;
	}
	switch(xfrd_handle_received_xfr_packet(zone, xfrd->packet)) {
		case xfrd_packet_tcp:
			xfrd_set_timer(zone, xfrd_time() + xfrd->tcp_set->tcp_timeout);
			xfrd_udp_release(zone);
			xfrd_tcp_obtain(xfrd->tcp_set, zone);
			break;
		case xfrd_packet_transfer:
		case xfrd_packet_newlease:
			/* nothing more to do */
			assert(zone->round_num == -1);
			xfrd_udp_release(zone);
			break;
		case xfrd_packet_notimpl:
			zone->master->ixfr_disabled = time(NULL);
			/* drop packet */
			xfrd_udp_release(zone);
			/* query next server */
			xfrd_make_request(zone);
			break;
		case xfrd_packet_more:
		case xfrd_packet_bad:
		default:
			/* drop packet */
			xfrd_udp_release(zone);
			/* query next server */
			xfrd_make_request(zone);
			break;
	}
}

int
xfrd_send_udp(acl_options_t* acl, buffer_type* packet, acl_options_t* ifc)
{
#ifdef INET6
	struct sockaddr_storage to;
#else
	struct sockaddr_in to;
#endif /* INET6 */
	int fd, family;

	/* this will set the remote port to acl->port or TCP_PORT */
	socklen_t to_len = xfrd_acl_sockaddr_to(acl, &to);

	/* get the address family of the remote host */
	if(acl->is_ipv6) {
#ifdef INET6
		family = PF_INET6;
#else
		return -1;
#endif /* INET6 */
	} else {
		family = PF_INET;
	}

	fd = socket(family, SOCK_DGRAM, IPPROTO_UDP);
	if(fd == -1) {
		log_msg(LOG_ERR, "xfrd: cannot create udp socket to %s: %s",
			acl->ip_address_spec, strerror(errno));
		return -1;
	}

	/* bind it */
	if (!xfrd_bind_local_interface(fd, ifc, acl, 0)) {
		log_msg(LOG_ERR, "xfrd: cannot bind outgoing interface '%s' to "
				 "udp socket: No matching ip addresses found",
			ifc->ip_address_spec);
		return -1;
	}

	/* send it (udp) */
	if(sendto(fd,
		buffer_current(packet),
		buffer_remaining(packet), 0,
		(struct sockaddr*)&to, to_len) == -1)
	{
		log_msg(LOG_ERR, "xfrd: sendto %s failed %s",
			acl->ip_address_spec, strerror(errno));
		return -1;
	}
	return fd;
}

int
xfrd_bind_local_interface(int sockd, acl_options_t* ifc, acl_options_t* acl,
	int tcp)
{
#ifdef SO_LINGER
	struct linger linger = {1, 0};
#endif
	socklen_t frm_len;
#ifdef INET6
	struct sockaddr_storage frm;
#else
	struct sockaddr_in frm;
#endif /* INET6 */

	if (!ifc) /* no outgoing interface set */
		return 1;

	while (ifc) {
		if (ifc->is_ipv6 != acl->is_ipv6) {
			/* check if we have a matching address family */
			ifc = ifc->next;
			continue;
		}

		DEBUG(DEBUG_XFRD,1, (LOG_INFO, "xfrd: bind() %s to %s socket",
			ifc->ip_address_spec, tcp? "tcp":"udp"));

		frm_len = xfrd_acl_sockaddr_frm(ifc, &frm);

		if (tcp) {
#ifdef SO_REUSEADDR
			if (setsockopt(sockd, SOL_SOCKET, SO_REUSEADDR, &frm,
				frm_len) < 0) {
				VERBOSITY(2, (LOG_WARNING, "xfrd: setsockopt "
			     "SO_REUSEADDR failed: %s", strerror(errno)));
			}
#else
			VERBOSITY(2, (LOG_WARNING, "xfrd: setsockopt SO_REUSEADDR "
			     "failed: SO_REUSEADDR not defined"));
#endif /* SO_REUSEADDR */

			if (ifc->port != 0) {
#ifdef SO_LINGER
				if (setsockopt(sockd, SOL_SOCKET, SO_LINGER,
					&linger, sizeof(linger)) < 0) {
					VERBOSITY(2, (LOG_WARNING, "xfrd: setsockopt "
				     "SO_LINGER failed: %s", strerror(errno)));
				}
#else
				VERBOSITY(2, (LOG_WARNING, "xfrd: setsockopt SO_LINGER "
					"failed: SO_LINGER not defined"));
#endif /* SO_LINGER */
			}
		}

		/* found one */
		if(bind(sockd, (struct sockaddr*)&frm, frm_len) >= 0) {
			DEBUG(DEBUG_XFRD,2, (LOG_INFO, "xfrd: bind() %s to %s "
						       "socket was successful",
			ifc->ip_address_spec, tcp? "tcp":"udp"));
			return 1;
		}

		DEBUG(DEBUG_XFRD,2, (LOG_INFO, "xfrd: bind() %s to %s socket"
					       "failed: %s",
			ifc->ip_address_spec, tcp? "tcp":"udp",
			strerror(errno)));
		/* try another */
		ifc = ifc->next;
	}

	log_msg(LOG_WARNING, "xfrd: could not bind source address:port to "
			     "socket: %s", strerror(errno));
	return 0;
}

void
xfrd_tsig_sign_request(buffer_type* packet, tsig_record_type* tsig,
	acl_options_t* acl)
{
	tsig_algorithm_type* algo;
	assert(acl->key_options && acl->key_options->tsig_key);
	algo = tsig_get_algorithm_by_name(acl->key_options->algorithm);
	if(!algo) {
		log_msg(LOG_ERR, "tsig unknown algorithm %s",
			acl->key_options->algorithm);
		return;
	}
	assert(algo);
	tsig_init_record(tsig, algo, acl->key_options->tsig_key);
	tsig_init_query(tsig, ID(packet));
	tsig_prepare(tsig);
	tsig_update(tsig, packet, buffer_position(packet));
	tsig_sign(tsig);
	tsig_append_rr(tsig, packet);
	ARCOUNT_SET(packet, ARCOUNT(packet) + 1);
	DEBUG(DEBUG_XFRD,1, (LOG_INFO, "appending tsig to packet"));
	/* prepare for validating tsigs */
	tsig_prepare(tsig);
}

static int
xfrd_send_ixfr_request_udp(xfrd_zone_t* zone)
{
	int fd;

	/* make sure we have a master to query the ixfr request to */
	assert(zone->master);

	if(zone->tcp_conn != -1) {
		/* tcp is using the zone_handler.fd */
		log_msg(LOG_ERR, "xfrd: %s tried to send udp whilst tcp engaged",
			zone->apex_str);
		return -1;
	}
	xfrd_setup_packet(xfrd->packet, TYPE_IXFR, CLASS_IN, zone->apex);
	zone->query_id = ID(xfrd->packet);
	zone->msg_seq_nr = 0;
	zone->msg_rr_count = 0;
	DEBUG(DEBUG_XFRD,1, (LOG_INFO, "sent query with ID %d", zone->query_id));
        NSCOUNT_SET(xfrd->packet, 1);
	xfrd_write_soa_buffer(xfrd->packet, zone->apex, &zone->soa_disk);
	/* if we have tsig keys, sign the ixfr query */
	if(zone->master->key_options && zone->master->key_options->tsig_key) {
		xfrd_tsig_sign_request(xfrd->packet, &zone->tsig, zone->master);
	}
	buffer_flip(xfrd->packet);
	xfrd_set_timer(zone, xfrd_time() + XFRD_UDP_TIMEOUT);

	if((fd = xfrd_send_udp(zone->master, xfrd->packet,
		zone->zone_options->outgoing_interface)) == -1)
		return -1;

	DEBUG(DEBUG_XFRD,1, (LOG_INFO,
		"xfrd sent udp request for ixfr=%u for zone %s to %s",
		ntohl(zone->soa_disk.serial),
		zone->apex_str, zone->master->ip_address_spec));
	return fd;
}

static int xfrd_parse_soa_info(buffer_type* packet, xfrd_soa_t* soa)
{
	if(!buffer_available(packet, 10))
		return 0;
	soa->type = htons(buffer_read_u16(packet));
	soa->klass = htons(buffer_read_u16(packet));
	soa->ttl = htonl(buffer_read_u32(packet));
	if(ntohs(soa->type) != TYPE_SOA || ntohs(soa->klass) != CLASS_IN)
	{
		return 0;
	}

	if(!buffer_available(packet, buffer_read_u16(packet)) /* rdata length */ ||
		!(soa->prim_ns[0] = dname_make_wire_from_packet(soa->prim_ns+1, packet, 1)) ||
		!(soa->email[0] = dname_make_wire_from_packet(soa->email+1, packet, 1)))
	{
		return 0;
	}
	soa->serial = htonl(buffer_read_u32(packet));
	soa->refresh = htonl(buffer_read_u32(packet));
	soa->retry = htonl(buffer_read_u32(packet));
	soa->expire = htonl(buffer_read_u32(packet));
	soa->minimum = htonl(buffer_read_u32(packet));

	return 1;
}


/*
 * Check the RRs in an IXFR/AXFR reply.
 * returns 0 on error, 1 on correct parseable packet.
 * done = 1 if the last SOA in an IXFR/AXFR has been seen.
 * soa then contains that soa info.
 * (soa contents is modified by the routine)
 */
static int
xfrd_xfr_check_rrs(xfrd_zone_t* zone, buffer_type* packet, size_t count,
	int *done, xfrd_soa_t* soa)
{
	/* first RR has already been checked */
	uint16_t type, klass, rrlen;
	uint32_t ttl;
	size_t i, soapos;
	for(i=0; i<count; ++i,++zone->msg_rr_count)
	{
		if(!packet_skip_dname(packet))
			return 0;
		if(!buffer_available(packet, 10))
			return 0;
		soapos = buffer_position(packet);
		type = buffer_read_u16(packet);
		klass = buffer_read_u16(packet);
		ttl = buffer_read_u32(packet);
		rrlen = buffer_read_u16(packet);
		if(!buffer_available(packet, rrlen))
			return 0;
		if(type == TYPE_SOA) {
			/* check the SOAs */
			size_t mempos = buffer_position(packet);
			buffer_set_position(packet, soapos);
			if(!xfrd_parse_soa_info(packet, soa))
				return 0;
			if(zone->msg_rr_count == 1 &&
				ntohl(soa->serial) != zone->msg_new_serial) {
				/* 2nd RR is SOA with lower serial, this is an IXFR */
				zone->msg_is_ixfr = 1;
				if(!zone->soa_disk_acquired)
					return 0; /* got IXFR but need AXFR */
				if(ntohl(soa->serial) != ntohl(zone->soa_disk.serial))
					return 0; /* bad start serial in IXFR */
				zone->msg_old_serial = ntohl(soa->serial);
			}
			else if(ntohl(soa->serial) == zone->msg_new_serial) {
				/* saw another SOA of new serial. */
				if(zone->msg_is_ixfr == 1) {
					zone->msg_is_ixfr = 2; /* seen middle SOA in ixfr */
				} else {
					/* 2nd SOA for AXFR or 3rd newSOA for IXFR */
					*done = 1;
				}
			}
			buffer_set_position(packet, mempos);
		}
		buffer_skip(packet, rrlen);
	}
	/* packet seems to have a valid DNS RR structure */
	return 1;
}

static int
xfrd_xfr_process_tsig(xfrd_zone_t* zone, buffer_type* packet)
{
	int have_tsig = 0;
	assert(zone && zone->master && zone->master->key_options
		&& zone->master->key_options->tsig_key && packet);
	if(!tsig_find_rr(&zone->tsig, packet)) {
		log_msg(LOG_ERR, "xfrd: zone %s, from %s: malformed tsig RR",
			zone->apex_str, zone->master->ip_address_spec);
		return 0;
	}
	if(zone->tsig.status == TSIG_OK) {
		have_tsig = 1;
	}
	if(have_tsig) {
		/* strip the TSIG resource record off... */
		buffer_set_limit(packet, zone->tsig.position);
		ARCOUNT_SET(packet, ARCOUNT(packet) - 1);
	}

	/* keep running the TSIG hash */
	tsig_update(&zone->tsig, packet, buffer_limit(packet));
	if(have_tsig) {
		if (!tsig_verify(&zone->tsig)) {
			log_msg(LOG_ERR, "xfrd: zone %s, from %s: bad tsig signature",
				zone->apex_str, zone->master->ip_address_spec);
			return 0;
		}
		DEBUG(DEBUG_XFRD,1, (LOG_INFO, "xfrd: zone %s, from %s: good tsig signature",
			zone->apex_str, zone->master->ip_address_spec));
		/* prepare for next tsigs */
		tsig_prepare(&zone->tsig);
	}
	else if(zone->tsig.updates_since_last_prepare > XFRD_TSIG_MAX_UNSIGNED) {
		/* we allow a number of non-tsig signed packets */
		log_msg(LOG_INFO, "xfrd: zone %s, from %s: too many consecutive "
			"packets without TSIG", zone->apex_str,
			zone->master->ip_address_spec);
		return 0;
	}

	if(!have_tsig && zone->msg_seq_nr == 0) {
		log_msg(LOG_ERR, "xfrd: zone %s, from %s: no tsig in first packet of reply",
			zone->apex_str, zone->master->ip_address_spec);
		return 0;
	}
	return 1;
}

/* parse the received packet. returns xfrd packet result code. */
static enum xfrd_packet_result
xfrd_parse_received_xfr_packet(xfrd_zone_t* zone, buffer_type* packet,
	xfrd_soa_t* soa)
{
	size_t rr_count;
	size_t qdcount = QDCOUNT(packet);
	size_t ancount = ANCOUNT(packet), ancount_todo;
	int done = 0;

	/* has to be axfr / ixfr reply */
	if(!buffer_available(packet, QHEADERSZ)) {
		log_msg(LOG_INFO, "packet too small");
		return xfrd_packet_bad;
	}

	/* only check ID in first response message. Could also check that
	 * AA bit and QR bit are set, but not needed.
	 */
	DEBUG(DEBUG_XFRD,2, (LOG_INFO,
		"got query with ID %d and %d needed", ID(packet), zone->query_id));
	if(ID(packet) != zone->query_id) {
		log_msg(LOG_ERR, "xfrd: zone %s received bad query id from %s, "
				 "dropped",
			zone->apex_str, zone->master->ip_address_spec);
		return xfrd_packet_bad;
	}
	/* check RCODE in all response messages */
	if(RCODE(packet) != RCODE_OK) {
		log_msg(LOG_ERR, "xfrd: zone %s received error code %s from "
				 "%s",
			zone->apex_str, rcode2str(RCODE(packet)),
			zone->master->ip_address_spec);
		if (RCODE(packet) == RCODE_IMPL ||
			RCODE(packet) == RCODE_FORMAT) {
			return xfrd_packet_notimpl;
		}
		return xfrd_packet_bad;
	}
	/* check TSIG */
	if(zone->master->key_options) {
		if(!xfrd_xfr_process_tsig(zone, packet)) {
			DEBUG(DEBUG_XFRD,1, (LOG_ERR, "dropping xfr reply due "
				"to bad TSIG"));
			return xfrd_packet_bad;
		}
	}
	buffer_skip(packet, QHEADERSZ);

	/* skip question section */
	for(rr_count = 0; rr_count < qdcount; ++rr_count) {
		if (!packet_skip_rr(packet, 1)) {
			log_msg(LOG_ERR, "xfrd: zone %s, from %s: bad RR in "
					 		 "question section",
				zone->apex_str, zone->master->ip_address_spec);
			return xfrd_packet_bad;
		}
	}
	if(zone->msg_rr_count == 0 && ancount == 0) {
		if(zone->tcp_conn == -1 && TC(packet)) {
			DEBUG(DEBUG_XFRD,1, (LOG_INFO, "xfrd: TC flagged"));
			return xfrd_packet_tcp;
		}
		DEBUG(DEBUG_XFRD,1, (LOG_INFO, "xfrd: too short xfr packet: no "
					       			   "answer"));
		return xfrd_packet_bad;
	}
	ancount_todo = ancount;

	if(zone->msg_rr_count == 0) {
		/* parse the first RR, see if it is a SOA */
		if(!packet_skip_dname(packet) ||
			!xfrd_parse_soa_info(packet, soa))
		{
			DEBUG(DEBUG_XFRD,1, (LOG_ERR, "xfrd: zone %s, from %s: "
						      "no SOA begins answer"
						      " section",
				zone->apex_str, zone->master->ip_address_spec));
			return xfrd_packet_bad;
		}
		if(zone->soa_disk_acquired != 0 &&
			zone->state != xfrd_zone_expired /* if expired - accept anything */ &&
			compare_serial(ntohl(soa->serial), ntohl(zone->soa_disk.serial)) < 0) {
			DEBUG(DEBUG_XFRD,1, (LOG_INFO,
				"xfrd: zone %s ignoring old serial from %s",
				zone->apex_str, zone->master->ip_address_spec));
			VERBOSITY(1, (LOG_INFO,
				"xfrd: zone %s ignoring old serial from %s",
				zone->apex_str, zone->master->ip_address_spec));
			return xfrd_packet_bad;
		}
		if(zone->soa_disk_acquired != 0 && zone->soa_disk.serial == soa->serial) {
			DEBUG(DEBUG_XFRD,1, (LOG_INFO, "xfrd: zone %s got "
						       "update indicating "
						       "current serial",
				zone->apex_str));
			/* (even if notified) the lease on the current soa is renewed */
			zone->soa_disk_acquired = xfrd_time();
			if(zone->soa_nsd.serial == soa->serial)
				zone->soa_nsd_acquired = xfrd_time();
			xfrd_set_zone_state(zone, xfrd_zone_ok);
 			DEBUG(DEBUG_XFRD,1, (LOG_INFO, "xfrd: zone %s is ok",
				zone->apex_str));
			if(zone->soa_notified_acquired == 0) {
				/* not notified or anything, so stop asking around */
				zone->round_num = -1; /* next try start a new round */
				xfrd_set_timer_refresh(zone);
				return xfrd_packet_newlease;
			}
			/* try next master */
			return xfrd_packet_bad;
		}
		DEBUG(DEBUG_XFRD,1, (LOG_INFO, "IXFR reply has ok serial (have \
%u, reply %u).", ntohl(zone->soa_disk.serial), ntohl(soa->serial)));
		/* serial is newer than soa_disk */
		if(ancount == 1) {
			/* single record means it is like a notify */
			(void)xfrd_handle_incoming_notify(zone, soa);
		}
		else if(zone->soa_notified_acquired && zone->soa_notified.serial &&
			compare_serial(ntohl(zone->soa_notified.serial), ntohl(soa->serial)) < 0) {
			/* this AXFR/IXFR notifies me that an even newer serial exists */
			zone->soa_notified.serial = soa->serial;
		}
		zone->msg_new_serial = ntohl(soa->serial);
		zone->msg_rr_count = 1;
		zone->msg_is_ixfr = 0;
		if(zone->soa_disk_acquired)
			zone->msg_old_serial = ntohl(zone->soa_disk.serial);
		else zone->msg_old_serial = 0;
		ancount_todo = ancount - 1;
	}

	if(zone->tcp_conn == -1 && TC(packet)) {
		DEBUG(DEBUG_XFRD,1, (LOG_INFO,
			"xfrd: zone %s received TC from %s. retry tcp.",
			zone->apex_str, zone->master->ip_address_spec));
		return xfrd_packet_tcp;
	}

	if(zone->tcp_conn == -1 && ancount < 2) {
		/* too short to be a real ixfr/axfr data transfer: need at */
		/* least two RRs in the answer section. */
		/* The serial is newer, so try tcp to this master. */
		DEBUG(DEBUG_XFRD,1, (LOG_INFO, "xfrd: udp reply is short. Try "
					       			   "tcp anyway."));
		return xfrd_packet_tcp;
	}

	if(!xfrd_xfr_check_rrs(zone, packet, ancount_todo, &done, soa))
	{
		DEBUG(DEBUG_XFRD,1, (LOG_INFO, "xfrd: zone %s sent bad xfr "
					       			   "reply.", zone->apex_str));
		return xfrd_packet_bad;
	}
	if(zone->tcp_conn == -1 && done == 0) {
		DEBUG(DEBUG_XFRD,1, (LOG_INFO, "xfrd: udp reply incomplete"));
		return xfrd_packet_bad;
	}
	if(done == 0)
		return xfrd_packet_more;
	if(zone->master->key_options) {
		if(zone->tsig.updates_since_last_prepare != 0) {
			log_msg(LOG_INFO, "xfrd: last packet of reply has no "
					 		  "TSIG");
			return xfrd_packet_bad;
		}
	}
	return xfrd_packet_transfer;
}

enum xfrd_packet_result
xfrd_handle_received_xfr_packet(xfrd_zone_t* zone, buffer_type* packet)
{
	xfrd_soa_t soa;
	enum xfrd_packet_result res;

	/* parse and check the packet - see if it ends the xfr */
	switch((res=xfrd_parse_received_xfr_packet(zone, packet, &soa)))
	{
		case xfrd_packet_more:
		case xfrd_packet_transfer:
			/* continue with commit */
			break;
		case xfrd_packet_newlease:
			return xfrd_packet_newlease;
		case xfrd_packet_tcp:
			return xfrd_packet_tcp;
		case xfrd_packet_notimpl:
		case xfrd_packet_bad:
		default:
		{
			/* rollback */
			if(zone->msg_seq_nr > 0) {
				/* do not process xfr - if only one part simply ignore it. */
				/* rollback previous parts of commit */
				buffer_clear(packet);
				buffer_printf(packet, "xfrd: zone %s xfr "
						      "rollback serial %u at "
						      "time %u from %s of %u "
						      "parts",
					zone->apex_str,
					(int)zone->msg_new_serial,
					(int)xfrd_time(),
					zone->master->ip_address_spec,
					zone->msg_seq_nr);

				buffer_flip(packet);
				diff_write_commit(zone->apex_str,
					zone->msg_old_serial,
					zone->msg_new_serial,
					zone->query_id, zone->msg_seq_nr, 0,
					(char*)buffer_begin(packet),
					xfrd->nsd->options);
				DEBUG(DEBUG_XFRD,1, (LOG_INFO, "xfrd: zone %s "
							       "xfr reverted "
							       "\"%s\"",
					zone->apex_str,
					(char*)buffer_begin(packet)));
			}
			if (res == xfrd_packet_notimpl)
				return res;
			else
				return xfrd_packet_bad;
		}
	}

	/* dump reply on disk to diff file */
	diff_write_packet(zone->apex_str, zone->msg_new_serial, zone->query_id,
		zone->msg_seq_nr, buffer_begin(packet), buffer_limit(packet),
		xfrd->nsd->options);
	VERBOSITY(1, (LOG_INFO,
		"xfrd: zone %s written received XFR from %s with serial %u to "
		"disk", zone->apex_str, zone->master->ip_address_spec,
		(int)zone->msg_new_serial));
	zone->msg_seq_nr++;
	if(res == xfrd_packet_more) {
		/* wait for more */
		return xfrd_packet_more;
	}

	/* done. we are completely sure of this */
	buffer_clear(packet);
	buffer_printf(packet, "xfrd: zone %s received update to serial %u at "
			      "time %u from %s in %u parts",
		zone->apex_str, (int)zone->msg_new_serial, (int)xfrd_time(),
		zone->master->ip_address_spec, zone->msg_seq_nr);
	if(zone->master->key_options) {
		buffer_printf(packet, " TSIG verified with key %s",
			zone->master->key_options->name);
	}
	buffer_flip(packet);
	diff_write_commit(zone->apex_str, zone->msg_old_serial,
		zone->msg_new_serial, zone->query_id, zone->msg_seq_nr, 1,
		(char*)buffer_begin(packet), xfrd->nsd->options);
	VERBOSITY(1, (LOG_INFO, "xfrd: zone %s committed \"%s\"",
		zone->apex_str, (char*)buffer_begin(packet)));
	/* update the disk serial no. */
	zone->soa_disk_acquired = xfrd_time();
	zone->soa_disk = soa;
	if(zone->soa_notified_acquired && (
		zone->soa_notified.serial == 0 ||
		compare_serial(htonl(zone->soa_disk.serial),
		htonl(zone->soa_notified.serial)) >= 0))
	{
		zone->soa_notified_acquired = 0;
	}
	if(!zone->soa_notified_acquired) {
		/* do not set expired zone to ok:
		 * it would cause nsd to start answering
		 * bad data, since the zone is not loaded yet.
		 * if nsd does not reload < retry time, more
		 * queries (for even newer versions) are made.
		 * For expired zone after reload it is set ok (SOAINFO ipc). */
		if(zone->state != xfrd_zone_expired)
			xfrd_set_zone_state(zone, xfrd_zone_ok);
		DEBUG(DEBUG_XFRD,1, (LOG_INFO,
			"xfrd: zone %s is waiting for reload",
			zone->apex_str));
		zone->round_num = -1; /* next try start anew */
		xfrd_set_timer_refresh(zone);
		xfrd_set_reload_timeout();
		return xfrd_packet_transfer;
	} else {
		/* try to get an even newer serial */
		/* pretend it was bad to continue queries */
		xfrd_set_reload_timeout();
		return xfrd_packet_bad;
	}
}

static void
xfrd_set_reload_timeout()
{
	if(xfrd->nsd->options->xfrd_reload_timeout == -1)
		return; /* automatic reload disabled. */
	if(xfrd->reload_timeout.tv_sec == 0 ||
		xfrd_time() >= xfrd->reload_timeout.tv_sec ) {
		/* no reload wait period (or it passed), do it right away */
		xfrd->need_to_send_reload = 1;
		xfrd->ipc_handler.event_types |= NETIO_EVENT_WRITE;
		/* start reload wait period */
		xfrd->reload_timeout.tv_sec = xfrd_time() +
			xfrd->nsd->options->xfrd_reload_timeout;
		xfrd->reload_timeout.tv_nsec = 0;
		return;
	}
	/* cannot reload now, set that after the timeout a reload has to happen */
	xfrd->reload_handler.timeout = &xfrd->reload_timeout;
}

static void
xfrd_handle_reload(netio_type *ATTR_UNUSED(netio),
	netio_handler_type *handler, netio_event_types_type event_types)
{
	/* reload timeout */
	assert(event_types & NETIO_EVENT_TIMEOUT);
	/* timeout wait period after this request is sent */
	handler->timeout = NULL;
	xfrd->reload_timeout.tv_sec = xfrd_time() +
		xfrd->nsd->options->xfrd_reload_timeout;
	xfrd->need_to_send_reload = 1;
	xfrd->ipc_handler.event_types |= NETIO_EVENT_WRITE;
}

void
xfrd_handle_passed_packet(buffer_type* packet, int acl_num)
{
	uint8_t qnamebuf[MAXDOMAINLEN];
	uint16_t qtype, qclass;
	const dname_type* dname;
	region_type* tempregion = region_create(xalloc, free);
	xfrd_zone_t* zone;

	buffer_skip(packet, QHEADERSZ);
	if(!packet_read_query_section(packet, qnamebuf, &qtype, &qclass)) {
		region_destroy(tempregion);
		return; /* drop bad packet */
	}

	dname = dname_make(tempregion, qnamebuf, 1);
	DEBUG(DEBUG_XFRD,1, (LOG_INFO, "xfrd: got passed packet for %s, acl "
					   "%d", dname_to_string(dname,0), acl_num));

	/* find the zone */
	zone = (xfrd_zone_t*)rbtree_search(xfrd->zones, dname);
	if(!zone) {
		log_msg(LOG_INFO, "xfrd: incoming packet for unknown zone %s",
			dname_to_string(dname,0));
		region_destroy(tempregion);
		return; /* drop packet for unknown zone */
	}
	region_destroy(tempregion);

	/* handle */
	if(OPCODE(packet) == OPCODE_NOTIFY) {
		xfrd_soa_t soa;
		int have_soa = 0;
		int next;
		/* get serial from a SOA */
		if(ANCOUNT(packet) == 1 && packet_skip_dname(packet) &&
			xfrd_parse_soa_info(packet, &soa)) {
				have_soa = 1;
		}
		if(xfrd_handle_incoming_notify(zone, have_soa?&soa:NULL)) {
			if(zone->zone_handler.fd == -1
				&& zone->tcp_conn == -1 &&
				!zone->tcp_waiting && !zone->udp_waiting) {
					xfrd_set_refresh_now(zone);
			}
		}
		next = find_same_master_notify(zone, acl_num);
		if(next != -1) {
			zone->next_master = next;
			DEBUG(DEBUG_XFRD,1, (LOG_INFO,
				"xfrd: notify set next master to query %d",
				next));
		}
	}
	else {
		/* TODO handle incoming IXFR udp reply via port 53 */
	}
}

static int
xfrd_handle_incoming_notify(xfrd_zone_t* zone, xfrd_soa_t* soa)
{
	if(soa && zone->soa_disk_acquired && zone->state != xfrd_zone_expired &&
	   compare_serial(ntohl(soa->serial),ntohl(zone->soa_disk.serial)) <= 0)
	{
		DEBUG(DEBUG_XFRD,1, (LOG_INFO,
			"xfrd: ignored notify %s %u old serial, zone valid "
			"(soa disk serial %u)", zone->apex_str,
			ntohl(soa->serial),
			ntohl(zone->soa_disk.serial)));
		return 0; /* ignore notify with old serial, we have a valid zone */
	}
	if(soa == 0) {
		zone->soa_notified.serial = 0;
	}
	else if (zone->soa_notified_acquired == 0 ||
		 zone->soa_notified.serial == 0 ||
		 compare_serial(ntohl(soa->serial),
			ntohl(zone->soa_notified.serial)) > 0)
	{
		zone->soa_notified = *soa;
	}
	zone->soa_notified_acquired = xfrd_time();
	if(zone->state == xfrd_zone_ok) {
		xfrd_set_zone_state(zone, xfrd_zone_refreshing);
	}
	/* transfer right away */
	VERBOSITY(1, (LOG_INFO, "Handle incoming notify for zone %s",
		zone->apex_str));
	return 1;
}

static int
find_same_master_notify(xfrd_zone_t* zone, int acl_num_nfy)
{
	acl_options_t* nfy_acl = acl_find_num(
		zone->zone_options->allow_notify, acl_num_nfy);
	int num = 0;
	acl_options_t* master = zone->zone_options->request_xfr;
	if(!nfy_acl)
		return -1;
	while(master)
	{
		if(acl_same_host(nfy_acl, master))
			return num;
		master = master->next;
		num++;
	}
	return -1;
}

void
xfrd_check_failed_updates()
{
	/* see if updates have not come through */
	xfrd_zone_t* zone;
	RBTREE_FOR(zone, xfrd_zone_t*, xfrd->zones)
	{
		/* zone has a disk soa, and no nsd soa or a different nsd soa */
		if(zone->soa_disk_acquired != 0 &&
			(zone->soa_nsd_acquired == 0 ||
			zone->soa_disk.serial != zone->soa_nsd.serial))
		{
			if(zone->soa_disk_acquired <
				xfrd->reload_cmd_last_sent)
			{
				/* this zone should have been loaded, since its disk
				   soa time is before the time of the reload cmd. */
				xfrd_soa_t dumped_soa = zone->soa_disk;
				log_msg(LOG_ERR, "xfrd: zone %s: soa serial %u "
						 		 "update failed, restarting "
						 		 "transfer (notified zone)",
					zone->apex_str, ntohl(zone->soa_disk.serial));
				/* revert the soa; it has not been acquired properly */
				zone->soa_disk_acquired = zone->soa_nsd_acquired;
				zone->soa_disk = zone->soa_nsd;
				/* pretend we are notified with disk soa.
				   This will cause a refetch of the data, and reload. */
				xfrd_handle_incoming_notify(zone, &dumped_soa);
				xfrd_set_timer_refresh(zone);
			} else if(zone->soa_disk_acquired >= xfrd->reload_cmd_last_sent) {
				/* this zone still has to be loaded,
				   make sure reload is set to be sent. */
				if(xfrd->need_to_send_reload == 0 &&
					xfrd->reload_handler.timeout == NULL) {
					log_msg(LOG_ERR, "xfrd: zone %s: needs "
									 "to be loaded. reload lost? "
									 "try again", zone->apex_str);
					xfrd_set_reload_timeout();
				}
			}
		}
	}
}

void
xfrd_prepare_zones_for_reload()
{
	xfrd_zone_t* zone;
	RBTREE_FOR(zone, xfrd_zone_t*, xfrd->zones)
	{
		/* zone has a disk soa, and no nsd soa or a different nsd soa */
		if(zone->soa_disk_acquired != 0 &&
			(zone->soa_nsd_acquired == 0 ||
			zone->soa_disk.serial != zone->soa_nsd.serial))
		{
			if(zone->soa_disk_acquired == xfrd_time()) {
				/* antedate by one second.
				 * this makes sure that the zone time is before
				 * reload, so that check_failed_zones() is
				 * certain of the result.
				 */
				zone->soa_disk_acquired--;
			}
		}
	}
}

struct buffer*
xfrd_get_temp_buffer()
{
	return xfrd->packet;
}
